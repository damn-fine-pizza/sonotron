// LIVE-BUG repro (owner report, Repeat Zone): "auto-song ON + transport
// PLAYING: (1) the playhead bar does NOT scroll, (2) the scenes do NOT
// advance -- scene 1 repeats forever -- even though audio plays". The
// existing functional coverage of the render-side decision
// (test_grid_panel_auto_song.cpp) drives render_grid_panel by injecting
// SYNTHETIC kBeat events straight into an AppState -- it proves the
// advance/render LOGIC given a bar that is already climbing, but never
// proves bars actually climb through the REAL pipeline. This test drives the
// REAL production backend instead: a real InProcessBrainSession (the same
// class main.cpp uses without `--control`), its real engine thread
// (in_process_brain_session.cpp's run_engine()), the real OutEvent ring, and
// the real brain_event_from_outevent() decode -- then reduces every decoded
// event through a REAL AppState::apply(), exactly the reduction main.cpp's
// own frame loop performs (main.cpp:696-697), and watches AppState::bar()
// advance.
//
// No synthetic event ever touches AppState here -- every kBeat this test
// observes travelled the full path: Engine::advance_ticks -> Engine::
// fire_clock_pulse -> Engine::emit_beat -> the Shell's OutEvent sink ->
// out_event_ring -> InProcessBrainSession::poll() -> brain_event_from_
// outevent() -> AppState::apply(). If AppState::bar() fails to advance here,
// the defect is somewhere on that real chain, not in the render/auto-song
// decision logic (which apps/gui-sonotron/tests/test_grid_panel_auto_song.cpp
// and apps/gui-sonotron/tests/test_grid_model.cpp already cover in isolation
// and prove correct GIVEN an advancing bar).
//
// Roadmap 14110 (clock-injection seam): this test used to wait 6 REAL
// wall-clock seconds against the engine thread's genuine steady_clock and
// could only assert "the bar moved at all" (max seen > first seen), because
// tightening that to an exact bar count against real time would have made
// the test flaky under CI scheduler jitter. InProcessBrainSession now
// exposes set_clock_hooks_for_test() (in_process_brain_session.hpp): this
// test installs a virtual clock instead, feeding run_engine()'s tick math
// EXACTLY the virtual microseconds needed for kTargetBars of playback at the
// style's own tempo (TickAccumulator's integer math, common/time.hpp, is
// drift-free -- see FakeClock's own comment below for why this makes the
// final bar an EXACT, not approximate, prediction) and a near-instant wakeup
// hook instead of the loop's real 500us pacing sleep. The engine thread
// still runs for real, still decodes real OutEvents through the real ring --
// only the passage of TIME inside it is now test-controlled, so this test
// now finishes in a small fraction of a real second instead of 6.

#include "src/in_process_brain_session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "common/time.hpp"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

using arrangrr::kDefaultBpm;
using arrangrr::kPpqn;
using arrangrr::kTickDenominator;
using arrangrr::kTicksPerBar;

// How many full bars this test drives the engine through. Position::bar
// (runtime/transport.hpp) is 1-based and a fresh Transport starts at bar 1,
// so the exact final bar this test expects is 1 + kTargetBars.
constexpr int kTargetBars = 2;
constexpr int kExpectedFinalBar = 1 + kTargetBars;

constexpr std::uint64_t kTargetTicks = static_cast<std::uint64_t>(kTargetBars) * kTicksPerBar;

// Exact-division guard: TickAccumulator::advance_us (common/time.hpp) is
// drift-free integer math -- m_acc carries the exact remainder forward
// between calls, so feeding it a TOTAL of kTargetVirtualUs of elapsed
// virtual time, split across any number of calls in any sizes, always
// yields EXACTLY kTargetTicks with zero remainder left over, as long as
// kTargetVirtualUs itself divides evenly. This static_assert makes that
// division exact a COMPILE-time property instead of a runtime hope -- if a
// future constant change ever makes kTargetBars stop dividing evenly, this
// fails to build rather than silently producing a flaky off-by-one bar.
static_assert(kTargetTicks * kTickDenominator % (static_cast<std::uint64_t>(kDefaultBpm) * kPpqn) ==
                  0,
              "kTargetBars must convert to a whole virtual-microsecond budget at kDefaultBpm");
constexpr std::uint64_t kTargetVirtualUs =
    kTargetTicks * kTickDenominator / (static_cast<std::uint64_t>(kDefaultBpm) * kPpqn);

// Deterministic virtual clock (roadmap 14110): the FIRST call establishes
// run_engine()'s own `last_us` baseline at virtual time 0 (it is read once,
// before the tick loop starts). Every call after that advances by
// kStepUs, clamped at kTargetVirtualUs -- so the total virtual elapsed time
// ever handed to TickAccumulator across the whole run is EXACTLY
// kTargetVirtualUs, delivered in ~200 small steps rather than one giant
// jump. That spread gives drain_command_ring() several of the engine loop's
// own early iterations of margin to apply "style load"/"transport start"
// (queued via send() immediately after start()) before any of the tick-
// generating time budget is spent, rather than risking the whole budget
// landing in a single iteration that races ahead of those two commands.
// Once saturated, every further call returns the same clamped value
// forever (delta 0), so the engine can never advance a single tick past
// kExpectedFinalBar -- there is no way for this test to observe an
// overshoot.
//
// Single-threaded by construction: constructed on the test thread, then
// moved into the InProcessBrainSession's std::function hook and, from that
// point on, called ONLY by the engine thread (see set_clock_hooks_for_test's
// own doc comment) -- never touched concurrently, so no atomics are needed.
class FakeClock {
 public:
  FakeClock(std::uint64_t step_us, std::uint64_t target_us)
      : m_step_us(step_us), m_target_us(target_us) {}

  std::uint64_t operator()() {
    if (!m_started) {
      m_started = true;
      return 0;
    }
    m_value_us = std::min(m_value_us + m_step_us, m_target_us);
    return m_value_us;
  }

 private:
  bool m_started = false;
  std::uint64_t m_value_us = 0;
  std::uint64_t m_step_us;
  std::uint64_t m_target_us;
};

constexpr std::uint64_t kFakeClockSteps = 200;
constexpr std::uint64_t kFakeClockStepUs = kTargetVirtualUs / kFakeClockSteps;

// THE PINNED BUG (or its acquittal): a style is loaded (content, exactly
// main.cpp's own default-boot behavior) and the transport is genuinely
// started against the REAL production backend, then a virtual clock (not
// real wall time) is driven forward by EXACTLY kTargetVirtualUs -- the
// microseconds kTargetBars of playback take at the style's own tempo
// (kDefaultBpm, common/time.hpp). If AppState::bar() reaches EXACTLY
// kExpectedFinalBar, ticks are flowing correctly end to end; anything less
// reproduces the live bug (the playhead bar failing to advance) with no
// synthetic event in the loop.
void test_bar_advances_by_exact_virtual_bars_through_real_backend() {
  InProcessBrainSession session;

  InProcessBrainSession::ClockHooks hooks;
  hooks.now_us = FakeClock(kFakeClockStepUs, kTargetVirtualUs);
  hooks.wait = [] { std::this_thread::yield(); };  // no real pacing sleep needed
  session.set_clock_hooks_for_test(std::move(hooks));

  CHECK(session.start());

  session.send("style load basic");
  session.send("transport start");

  AppState app_state;
  std::vector<int> bars_seen;
  std::vector<BrainEvent> batch;
  // Safety bound on REAL wall time only (never on iteration/tick count),
  // purely to fail fast instead of hanging if a regression breaks tick
  // generation outright -- in the passing case this loop finishes in low
  // milliseconds, since it is the VIRTUAL clock, not real time, that the
  // engine's tick math advances against.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    batch.clear();
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      app_state.apply(ev);
      if (ev.kind == BrainEvent::Kind::kBeat) {
        bars_seen.push_back(app_state.bar());
      }
    }
    if (app_state.bar() == kExpectedFinalBar) {
      break;
    }
    std::this_thread::yield();
  }

  session.stop();

  CHECK(!bars_seen.empty());  // kBeat events must arrive at all while playing
  // THE ASSERTION UNDER TEST: not "did it move at all" -- the EXACT bar
  // kTargetBars of virtual playback must land on, no more and no less.
  CHECK(app_state.bar() == kExpectedFinalBar);
}

}  // namespace

int main() {
  test_bar_advances_by_exact_virtual_bars_through_real_backend();
  return sonotron::test::failures();
}

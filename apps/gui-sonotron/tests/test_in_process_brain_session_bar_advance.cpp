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
// (in_process_brain_session.cpp's run_engine(), ticking off a genuine
// steady_clock, ~0.5 ms cadence), the real OutEvent ring, and the real
// brain_event_from_outevent() decode -- then reduces every decoded event
// through a REAL AppState::apply(), exactly the reduction main.cpp's own
// frame loop performs (main.cpp:696-697), and watches AppState::bar() over
// several REAL wall-clock seconds.
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

#include "src/in_process_brain_session.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

// Drains poll() in short bursts across up to `max_wait` of REAL elapsed wall-
// clock time (the engine thread's own clock is genuinely asynchronous with
// respect to this thread, same discipline as test_in_process_brain_session.
// cpp's own poll_until), reducing every decoded event through a REAL
// AppState::apply() and recording app_state.bar() after every kBeat. Bounded
// on total WALL TIME, not iteration/event count, per the task's own
// robustness guardrail (assert "did it advance at all", never an exact
// count, to avoid flakiness under CI scheduler jitter).
std::vector<int> collect_bar_sequence_over(InProcessBrainSession& session, AppState& app_state,
                                           std::chrono::milliseconds max_wait) {
  std::vector<int> bars_seen;
  const auto deadline = std::chrono::steady_clock::now() + max_wait;
  std::vector<BrainEvent> batch;
  while (std::chrono::steady_clock::now() < deadline) {
    batch.clear();
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      app_state.apply(ev);
      if (ev.kind == BrainEvent::Kind::kBeat) {
        bars_seen.push_back(app_state.bar());
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return bars_seen;
}

// THE PINNED BUG (or its acquittal): a style is loaded (content, exactly
// main.cpp's own default-boot behavior) and the transport is genuinely
// started against the REAL production backend, then the test waits 6 REAL
// wall-clock seconds -- ample headroom, since the default tempo is 120 BPM
// (kDefaultBpm, common/time.hpp) and a 4/4 bar is 2 real seconds at that
// tempo, so 6 seconds should cross at least two full bar boundaries if the
// clock is advancing at all. If AppState::bar() never climbs past the first
// value this test observes, that is the live bug, reproduced end-to-end with
// no synthetic event in the loop.
void test_bar_advances_over_real_time_through_real_backend() {
  InProcessBrainSession session;
  CHECK(session.start());

  session.send("style load basic");
  session.send("transport start");

  AppState app_state;
  const std::vector<int> bars =
      collect_bar_sequence_over(session, app_state, std::chrono::milliseconds(6000));

  session.stop();

  CHECK(!bars.empty());  // kBeat events must arrive at all while playing
  const int first_bar = bars.front();
  const int max_bar = *std::max_element(bars.begin(), bars.end());
  // THE ASSERTION UNDER TEST: the bar must have STRICTLY ADVANCED past the
  // very first value observed -- not stayed pinned at a sentinel/constant
  // value for the whole 6 real seconds. A bar stuck at a constant value here
  // is exactly the owner-reported symptom ("scene 1 repeats forever", "the
  // playhead bar does NOT scroll").
  CHECK(max_bar > first_bar);
}

}  // namespace

int main() {
  test_bar_advances_over_real_time_through_real_backend();
  return sonotron::test::failures();
}

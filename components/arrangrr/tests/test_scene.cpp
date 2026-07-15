// Smoke tests for node 8100 (Scenes/song mode -- docs/reflections/
// phase7-scope-6000-8100-clip-timeline-seam.md, Fork B RESOLVED = own
// transport): SceneChain itself is a thin, header-only pool (no dedicated
// unit test file -- its whole surface is exercised end to end here, through
// the real ABI, exactly like test_clip.cpp/test_loop.cpp drive their own
// primitive). This is the MINIMAL happy-path pass (compile + smoke + the
// three critical gates the owner named); thorough functional/regression QA,
// including the non-4/4 meter-path coverage, is Torquato's follow-up.

#include "arrangrr/scene/scene_chain.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 1024>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = op;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // a = performance_slot; b = n_bars (low byte) | (beats_per_bar << 8) (high
  // byte, 0 = default 4/4); c = SceneTransitionKind -- mirrors abi.hpp's own
  // kSceneAdd packing comment.
  void add_scene(std::uint16_t performance_slot, std::uint8_t n_bars,
                 std::uint8_t beats_per_bar = 0,
                 SceneTransitionKind transition = SceneTransitionKind::kCut) {
    cmd(Param::kSceneAdd, performance_slot,
        static_cast<std::int32_t>(n_bars) | (static_cast<std::int32_t>(beats_per_bar) << 8),
        static_cast<std::int32_t>(transition));
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
};

// A Performance with every field inside perf::validate()'s accepted range --
// style_id/chord_sequence_id/controller_map_id/routing_profile_id keep their
// own "keep current"/"none" 0xFFFF sentinel defaults, so only `tempo_x100`
// (the field this file uses to tell scenes apart) needs setting. Mirrors
// test_performance.cpp's own valid_performance() baseline.
Performance perf_with_tempo(std::uint16_t tempo_x100) {
  Performance p;
  p.tempo_x100 = tempo_x100;
  return p;
}

// (a) A 2-scene chain advances at the right bar, applying the destination
// Performance at EVERY transition -- including the synchronous step-0 fire
// at kScenePlay time (mirrors LoopBuffer/ChordSequencer's own immediate-
// launch-fires-synchronously precedent).
void test_scene_two_scene_chain_advances_and_applies_performances() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1);  // scene 0: performance slot 0, holds 1 bar @ default 4/4
  b.add_scene(1, 1);  // scene 1: performance slot 1, holds 1 bar @ default 4/4
  CHECK(b.e.scenes().count() == 2);
  CHECK(b.warns() == 0);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kScenePlay);
  CHECK(b.warns() == 0);
  CHECK(b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 0);
  CHECK(b.e.transport().bpm() == 9000);  // step 0's Performance applied synchronously

  b.advance(kTicksPerBar);  // scene 0's own n_bars(1) elapses -> transition
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().bpm() == 15000);  // step 1's Performance applied at the transition
  CHECK(b.e.scenes().playing());          // still playing: holding the last scene

  b.advance(kTicksPerBar);  // scene 1's own n_bars(1) elapses: the chain (linear, no
                            // implicit loop) simply stops advancing
  CHECK(!b.e.scenes().playing());
  CHECK(b.e.transport().bpm() == 15000);  // holds the last scene's state
}

// (b) A scene whose step carries a non-4/4 TimeSig actually changes
// Transport::ticks_per_bar() at its transition, AND -- the regression guard
// for the Transport::at_bar_boundary re-anchoring fix -- the NEXT bar
// boundary lands at the METER-CORRECT tick (transition_tick +
// new_ticks_per_bar), never at the tick the old `tick % ticks_per_bar == 0`
// absolute modulo would have misfired at (an EARLIER, wrongly-phased
// multiple of the new meter counted from tick 0).
void test_scene_non_4_4_meter_realigns_next_boundary() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 2);     // scene 0: 2 bars @ default 4/4
  b.add_scene(1, 1, 3);  // scene 1: 1 bar @ 3/4 -- the mid-song meter change

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);  // still 4/4 at step 0

  b.advance(2 * kTicksPerBar);  // scene 0's own 2 bars elapse -> transition to scene 1
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().time_sig().beats_per_bar == 3);
  const Tick new_ticks_per_bar = 3 * kTicksPerBeat;
  CHECK(b.e.transport().ticks_per_bar() == new_ticks_per_bar);  // (b)'s first half
  CHECK(b.e.scenes().playing());  // holding the last scene, chain not ended yet

  // (b)'s second half, the re-anchoring regression guard: the CORRECT next
  // boundary re-anchors at the transition tick (2*kTicksPerBar +
  // new_ticks_per_bar); a pre-fix absolute modulo (`tick % new_ticks_per_bar
  // == 0`) would instead misfire at the smallest multiple of
  // new_ticks_per_bar that is merely > 2*kTicksPerBar, which lands EARLIER.
  const Tick correct_boundary = 2 * kTicksPerBar + new_ticks_per_bar;
  const Tick buggy_early_boundary =
      ((2 * kTicksPerBar) / new_ticks_per_bar + 1) * new_ticks_per_bar;
  CHECK(buggy_early_boundary < correct_boundary);  // sanity: the two genuinely differ here

  // Advance to a tick strictly between the buggy-early and the correct
  // boundary: a pre-fix build would already have ended the chain (the last
  // scene's own 1-bar hold would have closed at the buggy tick); the fixed
  // gate must still be holding it.
  const Tick midpoint = buggy_early_boundary + new_ticks_per_bar / 2;
  b.advance(midpoint - 2 * kTicksPerBar);
  CHECK(b.e.scenes().playing());

  // Advance the rest of the way to the meter-correct boundary: the chain
  // (scene 1 is the last step) ends exactly there.
  b.advance(correct_boundary - midpoint);
  CHECK(!b.e.scenes().playing());
}

// (c) An inert/never-played chain changes nothing: registering scenes alone
// must not apply any Performance or move the transport's meter, and a
// running transport with a never-played chain must behave exactly as if no
// scene had ever been registered.
void test_scene_inert_chain_changes_nothing() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  b.add_scene(0, 1);
  CHECK(b.e.scenes().count() == 1);
  CHECK(!b.e.scenes().playing());

  const auto bpm_before = b.e.transport().bpm();
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(8 * kTicksPerBar);                 // plenty of bars; the chain was never kScenePlay'd
  CHECK(b.e.transport().bpm() == bpm_before);  // no Performance was ever applied
  CHECK(!b.e.scenes().playing());
  for (const OutEvent& o : b.ev) {
    CHECK(o.kind != OutEvent::Kind::kTimeSig);  // no time-sig announce from an unplayed chain
  }
}

// Byte-identity guard: an Engine that never touches any kScene* verb at all
// must behave EXACTLY as before (mirrors test_loop_default_inert.cpp's own
// precedent).
void test_scene_default_inert() {
  Band b;
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(4 * kTicksPerBar);
  CHECK(b.warns() == 0);
  CHECK(b.e.scenes().count() == 0);
  CHECK(!b.e.scenes().playing());
}

// Cheap dispatch-wiring sanity: the two "reject" edges of the ABI surface.
void test_scene_play_empty_chain_warns() {
  Band b;
  b.cmd(Param::kScenePlay);
  CHECK(b.warns() == 1);
  CHECK(!b.e.scenes().playing());
}

void test_scene_add_bad_performance_slot_warns() {
  Band b;
  b.add_scene(static_cast<std::uint16_t>(kMaxPerformances), 1);  // one past the last valid slot
  CHECK(b.warns() == 1);
  CHECK(b.e.scenes().count() == 0);
}

}  // namespace

int main() {
  test_scene_two_scene_chain_advances_and_applies_performances();
  test_scene_non_4_4_meter_realigns_next_boundary();
  test_scene_inert_chain_changes_nothing();
  test_scene_default_inert();
  test_scene_play_empty_chain_warns();
  test_scene_add_bad_performance_slot_warns();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scene: all OK\n");
  }
  return arrangrr::test::failures();
}

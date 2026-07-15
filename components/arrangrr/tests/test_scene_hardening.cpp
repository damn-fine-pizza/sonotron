// Torquato QA (Phase 7, node 8100 hardening pass): goes BEYOND test_scene.cpp's
// own MINIMAL happy-path smoke (6 cases: a 2-scene chain, ONE meter change and
// its re-anchoring regression guard, inert/default-path byte-identity, and the
// two cheap ABI reject edges). This file closes the remaining coverage the
// owner named:
//   1. a chain that changes meter TWICE mid-song (4/4 -> 3/4 -> 4/4), proving
//      the re-anchored gate survives a SECOND transition without accumulating
//      any drift from the first;
//   2. chord commit_bar staying coherent with the FIXED Transport gate while
//      a non-4/4 scene is active (the GREEN baseline -- contrast this file's
//      sibling, test_scene_meter_gate_regression.cpp, where the OTHER three
//      bar-gated consumers named by the same brief -- Performance recall, pad
//      fires, clip launch -- do NOT stay coherent under the identical trigger);
//   3. the remaining SceneChain semantics test_scene.cpp's smoke pass does not
//      reach: play() restarting from step 0 after a stop, the chain-full warn,
//      and kSceneClear's reset;
//   4. an ABI round trip through the raw WIRE verb ids (65..68) themselves,
//      not just the named Param enum every other scene test already uses.
//
// Subject is still Engine's own cross-producer wiring (cmd_scene/fire_scene/
// apply_scene_transition, plus the chord/ABI seams it touches) -> functional,
// same precedent as test_scene.cpp itself.

#include "arrangrr/scene/scene_chain.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 4096>;

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
  // Mirrors test_scene.cpp's own add_scene helper exactly (same packing).
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
  // Ticks at which every kChordFollowed OutEvent landed, in emission order.
  StaticVector<Tick, 16> chord_followed_ticks() const {
    StaticVector<Tick, 16> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChordFollowed) {
        CHECK(out.push_back(o.tick));
      }
    }
    return out;
  }
};

Performance perf_with_tempo(std::uint16_t tempo_x100) {
  Performance p;
  p.tempo_x100 = tempo_x100;
  return p;
}

// ---- 1. Bar-grid re-anchoring survives a SECOND mid-song meter change -----

// The load-bearing regression guard test_scene.cpp's own test(b) proves for
// ONE transition (4/4 -> 3/4): here the chain changes meter TWICE
// (4/4 -> 3/4 -> 4/4). Every scene must hold for EXACTLY its own n_bars of
// its OWN (possibly new) meter, and the chain must not accumulate any
// residual phase error from the first transition into the second -- a bug
// that re-derived the grid from a STALE absolute origin (rather than
// re-anchoring at each transition's own tick, Transport::advance_bar_tick's
// whole reason to exist) would drift further with every additional change.
void test_scene_two_meter_changes_stay_meter_correct_through_the_whole_chain() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  CHECK(b.e.performances().store(2, perf_with_tempo(21000)));
  b.add_scene(0, 2, 0);  // scene0: 2 bars @ 4/4 (explicit default)
  b.add_scene(1, 3, 3);  // scene1: 3 bars @ 3/4 -- first meter change
  b.add_scene(2, 2, 4);  // scene2: 2 bars @ 4/4 -- second meter change, back to 4/4

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  CHECK(b.e.transport().bpm() == 9000);
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);
  CHECK(b.e.scenes().current_index() == 0);

  // scene0 holds its own 2 bars @ 4/4, then transitions.
  b.advance(2 * kTicksPerBar);
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().bpm() == 15000);
  const Tick bar_3_4 = 3 * kTicksPerBeat;
  CHECK(b.e.transport().ticks_per_bar() == bar_3_4);
  CHECK(b.e.scenes().playing());

  // scene1 must hold for EXACTLY 3 bars of the NEW (3/4) length -- not one
  // tick less (still scene1) nor one tick more (already scene2).
  b.advance(3 * bar_3_4 - 1);
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().ticks_per_bar() == bar_3_4);
  b.advance(1);  // the exact 3rd 3/4-bar boundary
  CHECK(b.e.scenes().current_index() == 2);
  CHECK(b.e.transport().bpm() == 21000);
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);  // back to 4/4, byte-identical
  CHECK(b.e.scenes().playing());                           // scene2 (2 bars) has not finished yet

  // scene2 (the last step) must hold for EXACTLY 2 bars of the meter IT
  // declares -- the second transition re-anchored cleanly, with no residual
  // phase inherited from scene1's own 3/4 window.
  b.advance(2 * kTicksPerBar - 1);
  CHECK(b.e.scenes().playing());
  CHECK(b.e.transport().bpm() == 21000);
  b.advance(1);
  CHECK(!b.e.scenes().playing());  // chain ended: holds scene2 (linear, no implicit loop)
  CHECK(b.e.transport().bpm() == 21000);
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);

  // Holding indefinitely: many more bars change nothing further.
  b.advance(10 * kTicksPerBar);
  CHECK(!b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 2);
  CHECK(b.e.transport().bpm() == 21000);
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);
}

// ---- 2. Chord commit_bar stays coherent inside a non-4/4 scene (GREEN) ----

// A SHIFT-staged (quantized) chord promotes on the very NEXT bar boundary
// commit_bar() is called from (Engine::on_tick's own already-fixed
// Transport::at_bar_boundary() gate, no separate window/latch of its own) --
// this stays correct even while a scene-driven non-4/4 meter is live, because
// commit_bar has nothing of its own to re-derive: it simply reacts to
// whichever tick the (now re-anchored) gate says is a boundary. Contrast
// this file's sibling regression file, where the OTHER three consumers named
// by the same brief do NOT share this immunity.
void test_chord_commit_bar_lands_on_the_correct_bar_in_a_non_4_4_scene() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1, 0);  // scene0: 1 bar @ 4/4
  b.add_scene(1, 6, 3);  // scene1: 6 bars @ 3/4, last step -> holds

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.advance(kTicksPerBar);  // land exactly at the transition tick (3840)
  CHECK(b.e.scenes().current_index() == 1);
  const Tick bar_3_4 = 3 * kTicksPerBeat;
  CHECK(b.e.transport().ticks_per_bar() == bar_3_4);

  b.advance(50);  // move a bit further into the (now stable) 3/4 scene
  b.ev.clear();
  // Stage a chord (quantize=true whenever Command::boundary != kImmediate --
  // chord_play's own convention, mirrors a SHIFT-held keypress). chord_play
  // itself emits TWO kChordFollowed events: an immediate one right here
  // (announcing the staged "next", chord_play's own unconditional echo) and
  // the real promotion at the bar boundary commit_bar() actually lands on.
  b.cmd(Param::kChordPlay, 60, 0, 100, 0, Op::kDo, Boundary::kNextBar);
  b.advance(2 * bar_3_4);
  const auto ticks = b.chord_followed_ticks();
  CHECK(ticks.size() == 2);
  CHECK(ticks[0] == kTicksPerBar + 50);  // the immediate "staged" echo
  // The commit itself: the very NEXT 3/4 bar boundary after the stage tick
  // (3890) -- 3840 + 2880 = 6720, not any tick a stale absolute-modulo grid
  // (counted from tick 0 under the OLD 4/4 meter) would have produced.
  CHECK(ticks[1] == kTicksPerBar + bar_3_4);
}

// ---- 3. SceneChain semantics test_scene.cpp's smoke pass does not reach ---

// play() always restarts from step 0, even mid-chain: a stop() followed by a
// fresh kScenePlay re-applies step 0's own Performance/TimeSig synchronously,
// exactly like the very first play() did (mirrors ChordSequencer's own
// play()-rebases-to-tick-0 discipline, scene_chain.hpp's own header comment).
void test_scene_replay_restarts_from_step_zero_after_a_stop() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1, 0);
  b.add_scene(1, 1, 3);

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.advance(kTicksPerBar);  // now on step 1 (3/4, bpm 15000)
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().bpm() == 15000);

  b.cmd(Param::kSceneStop);
  CHECK(!b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 1);  // stop() does not reset position

  b.cmd(Param::kScenePlay);
  CHECK(b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 0);  // play() always rebases to step 0
  CHECK(b.e.transport().bpm() == 9000);      // step 0's Performance re-applied synchronously
  CHECK(b.e.transport().ticks_per_bar() == kTicksPerBar);  // step 0's own 4/4 too
}

// kSceneTableFull: the chain's own bounded pool (kMaxScenes) warns cleanly
// once full, without corrupting the count of what is already registered.
void test_scene_add_warns_when_the_table_is_full() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  for (std::size_t i = 0; i < kMaxScenes; ++i) {
    b.add_scene(0, 1, 0);
  }
  CHECK(b.warns() == 0);
  CHECK(b.e.scenes().count() == kMaxScenes);

  b.add_scene(0, 1, 0);  // one past capacity
  CHECK(b.warns() == 1);
  CHECK(b.e.scenes().count() == kMaxScenes);  // unchanged, no partial write
}

// kSceneClear: resets the WHOLE chain (content, play state, and position),
// even mid-playback -- a subsequent add_scene starts a genuinely fresh chain,
// not a stale tail of the cleared one.
void test_scene_clear_resets_everything_even_mid_playback() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1, 0);
  b.add_scene(1, 4, 3);
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.advance(kTicksPerBar);  // now mid-chain, on step 1
  CHECK(b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 1);

  b.cmd(Param::kSceneClear);
  CHECK(b.e.scenes().count() == 0);
  CHECK(!b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 0);

  // A fresh chain built after clear() behaves like a brand-new one -- no
  // leftover state from the cleared chain (e.g. a stale bar counter) leaks
  // into it.
  CHECK(b.e.performances().store(2, perf_with_tempo(21000)));
  b.add_scene(2, 1, 0);
  CHECK(b.e.scenes().count() == 1);
  CHECK(b.warns() == 0);
  b.cmd(Param::kScenePlay);
  CHECK(b.e.scenes().playing());
  CHECK(b.e.scenes().current_index() == 0);
  CHECK(b.e.transport().bpm() == 21000);
}

// ---- 4. ABI round trip through the RAW wire verb ids (65..68) -------------

// test_abi_frozen.cpp already static_asserts the numeric value of each Param
// enumerator; every other scene test drives the command path through the
// named enum. This closes the remaining gap: build the exact wire Command
// using the raw integer ids the frozen ABI promises (kSceneAdd=65,
// kSceneClear=66, kScenePlay=67, kSceneStop=68) and confirm push_command
// still dispatches each one correctly -- a future accidental reordering of
// the Param enum would break dispatch for these raw values even if the
// enumerator NAME kept compiling against the old (now wrong) number.
void test_scene_abi_verbs_round_trip_via_raw_wire_ids() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));

  // Raw kSceneAdd (65): a = performance_slot, b = n_bars | (beats_per_bar<<8).
  Command add_cmd;
  add_cmd.op = Op::kDo;
  add_cmd.param = static_cast<Param>(65);
  add_cmd.a = 0;
  add_cmd.b = 1;  // n_bars=1, beats_per_bar wire 0 -> default 4/4
  add_cmd.c = 0;
  b.e.push_command(add_cmd, [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  CHECK(b.warns() == 0);
  CHECK(b.e.scenes().count() == 1);

  b.cmd(Param::kTransportStart);

  // Raw kScenePlay (67).
  Command play_cmd;
  play_cmd.op = Op::kDo;
  play_cmd.param = static_cast<Param>(67);
  b.e.push_command(play_cmd, [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  CHECK(b.e.scenes().playing());
  CHECK(b.e.transport().bpm() == 9000);

  // Raw kSceneStop (68).
  Command stop_cmd;
  stop_cmd.op = Op::kDo;
  stop_cmd.param = static_cast<Param>(68);
  b.e.push_command(stop_cmd, [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  CHECK(!b.e.scenes().playing());

  // Raw kSceneClear (66).
  Command clear_cmd;
  clear_cmd.op = Op::kDo;
  clear_cmd.param = static_cast<Param>(66);
  b.e.push_command(clear_cmd, [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  CHECK(b.e.scenes().count() == 0);
}

}  // namespace

int main() {
  test_scene_two_meter_changes_stay_meter_correct_through_the_whole_chain();
  test_chord_commit_bar_lands_on_the_correct_bar_in_a_non_4_4_scene();
  test_scene_replay_restarts_from_step_zero_after_a_stop();
  test_scene_add_warns_when_the_table_is_full();
  test_scene_clear_resets_everything_even_mid_playback();
  test_scene_abi_verbs_round_trip_via_raw_wire_ids();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scene_hardening: all OK\n");
  }
  return arrangrr::test::failures();
}

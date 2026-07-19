// Torquato QA (Phase 7, node 8100 song-mode hardening): pins an owner-
// reported RED bug -- soloing a track LIVE while a Repeat-Zone SceneChain
// (song mode) is playing does not stay isolated: the moment the chain
// crosses into its next scene step, the transition silently re-applies the
// STALE mute/solo bitmask that was captured into that step's Performance at
// `song build` time (before the user ever touched solo), clobbering the
// live gesture. Root cause (diagnosed, not fixed here): Engine::
// apply_performance (engine.cpp, ~lines 1795-1801) unconditionally re-
// applies `perf.track_mute_mask`/`perf.track_solo_mask` on EVERY call,
// including calls from Engine::apply_scene_transition (a live SceneChain
// step, `scene_hold_bars != 0`) -- not just from a genuine Performance/pad
// recall (`scene_hold_bars == 0`), which is the only case the live mute/
// solo rig should ever be clobbered by a recalled mask.
//
// Fixed by Giotto: Engine::apply_performance now gates the
// set_mute()/set_solo() restore behind `scene_hold_bars == 0`, so a genuine
// Performance/pad recall still restores the captured mask byte-exact
// (test_perf_capture_recall_round_trip_restores_everything, test_
// performance.cpp), while a live SceneChain transition leaves the live
// mute/solo rig alone -- mirroring the `chord_sequence_id == 0xFFFF`
// sentinel apply_song_build already forces for the same class of reason.
// Was RED before the fix (see this file's own commit history for the exact
// failing CHECK lines); now GREEN. Kept in its own file/CTest entry so a
// future re-break of this specific gate is caught in isolation, without
// masking test_scene.cpp/test_scene_hardening.cpp's surrounding coverage.

#include "arrangrr/scene/scene_chain.hpp"

#include "arrangrr/common/static_vector.hpp"
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
  void add_scene(std::uint16_t performance_slot, std::uint8_t n_bars,
                 std::uint8_t beats_per_bar = 0) {
    cmd(Param::kSceneAdd, performance_slot,
        static_cast<std::int32_t>(n_bars) | (static_cast<std::int32_t>(beats_per_bar) << 8), 0);
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, /*idx=*/0, Op::kSet);
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0,
        /*idx=*/0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0,
        /*idx=*/0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        /*idx=*/0, Op::kSet);
  }
  int note_on_count(std::uint8_t channel) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == channel) {
        ++n;
      }
    }
    return n;
  }
};

// Sanity: proves the SAME solo primitive isolates a track correctly when NO
// SceneChain is involved at all (mirrors test_arranger.cpp's own
// test_part_mute_solo -- kept here too so a failure of the regression test
// below can never be confused with a broken solo primitive).
void test_solo_isolation_sanity() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // C: tonal roles have a chord to sound
  b.cmd(Param::kPartSolo, static_cast<std::int32_t>(TrackRole::kDrums), 1, 0, /*idx=*/0, Op::kSet);
  CHECK(b.e.arranger().soloed(TrackRole::kDrums));

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(kTicksPerBar - 1);
  CHECK(b.note_on_count(9) > 0);   // drums soloed -> audible
  CHECK(b.note_on_count(1) == 0);  // bass silenced by the solo
  CHECK(b.note_on_count(2) == 0);  // chord1 silenced by the solo
}

// THE REGRESSION (RED): a live solo, applied AFTER a SceneChain has already
// started (mid-song, exactly like the owner's repro), is silently cleared
// the instant the chain crosses into its next step -- because
// apply_scene_transition -> apply_performance unconditionally re-applies
// that step's Performance-captured mute/solo mask, which was captured
// BEFORE the live solo ever happened and therefore carries no solo at all.
void test_live_solo_survives_scene_chain_transition() {
  Band b;
  b.setup_basic();
  // The scene steps' base Performance: captures the CURRENT live rig, with
  // nothing muted/soloed yet (track_solo_mask == 0 in the captured record).
  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);

  // Two steps, BOTH pointing at the same (unsoloed) base Performance.
  b.add_scene(/*performance_slot=*/0, /*n_bars=*/1, /*beats_per_bar=*/0);
  b.add_scene(/*performance_slot=*/0, /*n_bars=*/6, /*beats_per_bar=*/0);

  b.cmd(Param::kChordPlay, 60, -1, 100);  // tonal roles have a chord for the whole test
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);  // fires step 0 synchronously: base, unsoloed Performance lands

  // The LIVE user gesture, mid-song, exactly like the owner's repro.
  b.cmd(Param::kPartSolo, static_cast<std::int32_t>(TrackRole::kDrums), 1, 0, /*idx=*/0, Op::kSet);
  CHECK(b.e.arranger().soloed(TrackRole::kDrums));

  b.ev.clear();
  b.advance(kTicksPerBar - 1);     // still inside step 0's own 1-bar hold
  CHECK(b.note_on_count(9) > 0);   // drums soloed -> audible
  CHECK(b.note_on_count(1) == 0);  // bass silenced by the solo
  CHECK(b.note_on_count(2) == 0);  // chord1 silenced by the solo

  // Cross the bar boundary into step 1: SceneChain::on_bar fires the
  // transition, calling apply_scene_transition -> apply_performance with
  // scene_hold_bars=6 -- the call site that (pre-fix) unconditionally
  // re-applies step 1's captured mask (== 0), silently clearing the live
  // solo.
  b.ev.clear();
  b.advance(2 * kTicksPerBar);               // safely past the transition tick
  CHECK(b.e.scenes().current_index() == 1);  // confirm the transition actually happened

  CHECK(b.e.arranger().soloed(TrackRole::kDrums));  // FINDING: fails pre-fix, live solo cleared
  CHECK(b.note_on_count(9) > 0);
  // FINDING: fails pre-fix, bass becomes audible again (solo silently cleared).
  CHECK(b.note_on_count(1) == 0);
  // FINDING: fails pre-fix, chord1 becomes audible again.
  CHECK(b.note_on_count(2) == 0);
}

}  // namespace

int main() {
  test_solo_isolation_sanity();
  test_live_solo_survives_scene_chain_transition();
  return arrangrr::test::failures();
}

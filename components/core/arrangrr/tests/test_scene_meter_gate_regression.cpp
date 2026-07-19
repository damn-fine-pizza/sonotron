// Torquato QA (Phase 7, node 8100 hardening pass): three RED findings, all
// downstream of the SAME root cause -- Engine::on_tick's bar-boundary gate
// itself (Transport::at_bar_boundary()/advance_bar_tick()) IS correctly
// re-anchored (67fdd13's own fix, pinned by test_scene.cpp and
// test_scene_hardening.cpp's own multi-transition test), but three of the
// FOUR consumers the fix's own brief names as "gated on it" do NOT actually
// re-derive their own due tick from that fixed gate -- each keeps a private
// window/phase of its own, computed once (at arm() time, or at the first
// on_bar() check since arm()) as an absolute-tick modulo, and never
// re-anchored again. The moment that window is armed AFTER a mid-song meter
// change has already moved the bar grid off the origin (tick 0), the
// window's own remainder-from-zero no longer coincides with ANY tick the
// re-anchored gate will ever produce again -- so due() (or on_bar()'s
// equivalent) can PERMANENTLY miss its own promotion. This is NOT a
// contrived multi-change corner case: it reproduces on the simplest possible
// trigger (ONE meter change, a plain "next bar" quantize, n_bars=1).
//
//   - Performance recall (Engine::m_perf_recall, a BoundaryLatch) -- F1 below.
//   - A pad fire (Engine::m_pad_latch, the SAME BoundaryLatch primitive,
//     one instance per pad slot) -- F2 below.
//   - A clip launch (ClipMatrix::on_bar's own frozen-window fix, commit
//     28864bc) -- F3 below.
//
// Chord commit_bar is the one exception (see test_scene_hardening.cpp's
// GREEN contrast test): it has no window of its own at all, it simply reacts
// to whichever tick the FIXED gate calls it on -- there is nothing for it to
// re-derive, so it never drifts.
//
// A fourth, structurally different finding closes this file: Arranger::
// on_tick's OWN internal bar/phase gate (arranger.hpp, `pos % ticks_per_bar
// == 0` relative to m_section_start) is independent of Engine's fixed gate
// and reproduces the EXACT SAME class of bug at the Arranger level -- once a
// mid-song meter change lands on a bar Arranger's own gate is watching, its
// internal phase permanently drifts off Transport's real grid (F4).
//
// Handoff to Giotto: NONE of this is fixed here (accuse, don't repair).
// All four ticks below were confirmed empirically (a scratch Engine-level
// probe, not hand arithmetic alone) before being pinned as CHECKs.

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
  // Mirrors test_pad.cpp's own exact bit packing (abi.hpp/engine.cpp's
  // pad_assign comment).
  void assign_pad(std::uint16_t id, PadType type, PadMode mode, Boundary sync,
                  std::uint16_t source_idx, std::uint8_t n_bars = 1,
                  PadPitch pitch = PadPitch::kFixed, std::uint8_t dest_port = 0,
                  std::uint8_t dest_channel = 0, std::uint8_t source_aux = 0) {
    const std::int32_t a =
        static_cast<std::int32_t>(type) | (static_cast<std::int32_t>(mode) << 8) |
        (static_cast<std::int32_t>(sync) << 16) | (static_cast<std::int32_t>(pitch) << 24);
    const std::int32_t bb =
        dest_port | (dest_channel << 8) | (static_cast<std::int32_t>(n_bars) << 16);
    const std::int32_t c = source_idx | (static_cast<std::int32_t>(source_aux) << 24);
    cmd(Param::kPadAssign, a, bb, c, id);
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
  int clip_playing_events() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kClip &&
          o.msg.status == static_cast<std::uint8_t>(LaunchState::kPlaying)) {
        ++n;
      }
    }
    return n;
  }
};

Performance perf_with_tempo(std::uint16_t tempo_x100) {
  Performance p;
  p.tempo_x100 = tempo_x100;
  return p;
}

// Shared fixture for F1/F2/F3: a 2-scene chain, scene0 holds 1 bar @ 4/4
// (tick 0..kTicksPerBar), scene1 holds 6 bars @ 3/4 (the last step -- holds
// indefinitely once its own 6 bars elapse, and the TRANSPORT itself keeps
// ticking at the same 3/4 cadence forever after that, independent of
// SceneChain::playing()). Every arm below happens WELL AFTER the transition
// (tick kTicksPerBar + 50), fully inside an already-stable 3/4 meter -- the
// simplest possible trigger, deliberately NOT a contrived multi-change edge
// case.
void play_one_meter_change_fixture(Band& b) {
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1, 0);  // scene0: 1 bar @ 4/4
  b.add_scene(1, 6, 3);  // scene1: 6 bars @ 3/4, last -> holds
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.advance(kTicksPerBar);  // land exactly at the transition tick
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().ticks_per_bar() == 3 * kTicksPerBeat);
  b.advance(50);  // move a bit further into the now-stable 3/4 scene
}

// F1 (RED): a Performance recall armed for the simplest "next bar"
// (n_bars=1, Engine::m_perf_recall's BoundaryLatch) after a mid-song meter
// change to 3/4 NEVER lands -- not late, not early, never. BoundaryLatch::
// arm() freezes `window = n_bars * ticks_per_bar` at arm time (2880 here);
// due(t) checks `t % window == 0`. The transport's own re-anchored boundary
// sequence past the transition (tick 3840) is 3840 + 2880*k for k=1,2,3,...
// -- every one of those ticks is EXACTLY 960 (mod 2880), never 0, because
// 3840 itself is not a multiple of 2880 (3840 % 2880 == 960). The latch is
// waiting for a remainder the sequence can never produce again.
void test_pending_performance_recall_never_lands_after_a_single_meter_change() {
  Band b;
  play_one_meter_change_fixture(b);
  CHECK(b.e.performances().store(2, perf_with_tempo(5000)));  // the recall TARGET, distinct tempo

  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/2, Op::kDo, Boundary::kNextBar);
  CHECK(b.e.transport().bpm() == 15000);  // not yet: waits for the next bar

  // Advance through 20 more 3/4 bars -- FAR more than the "1 bar" the recall
  // was actually quantized to. A correct implementation would have applied
  // it at the very first one of these.
  b.advance(20 * (3 * kTicksPerBeat));

  // FINDING: this fails. bpm stays 15000 (scene1's own Performance) forever
  // -- the recall to slot 2 (bpm 5000) never lands, on ANY of the 20 bars
  // advanced, even though it was armed for exactly "the next bar".
  CHECK(b.e.transport().bpm() == 5000);
}

// F2 (RED): the SAME primitive, a different call site -- a pad armed for
// "the next bar" (Engine::m_pad_latch[id], also a BoundaryLatch) shares the
// exact same fate: it never fires.
void test_pad_fire_never_lands_after_a_single_meter_change() {
  Band b;
  play_one_meter_change_fixture(b);

  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kNextBar,
               /*source_idx=*/40, /*n_bars=*/1, PadPitch::kFixed, /*dest_port=*/0,
               /*dest_channel=*/3, /*source_aux=*/100);
  b.cmd(Param::kPadTrigger, 0, 0, 0, /*idx=*/0);
  b.ev.clear();
  b.advance(20 * (3 * kTicksPerBeat));

  // FINDING: this fails. Zero NoteOn events on channel 3 across 20 bars --
  // the pad stays armed forever, never firing the note it was triggered for.
  CHECK(b.note_on_count(3) > 0);
}

// F3 (RED): ClipMatrix::on_bar's own frozen-window fix (commit 28864bc,
// "freeze ClipMatrix quantize window against live meter changes") resolves
// its window on the FIRST on_bar() check since arm() -- but that first check
// itself lands on a tick from the SAME re-anchored, non-zero-origin sequence
// (e.g. 6720 here), and `transport_tick % window` (6720 % 2880 == 960) fails
// for the identical reason as F1/F2. The 28864bc fix stops a clip from being
// RETUNED mid-countdown by a LATER meter change (test_clip_matrix_live_
// meter_change_regression.cpp's own scenario) -- it does not make the
// window's own arithmetic correct once the grid itself is already offset
// from absolute tick 0, which is exactly what happens the instant a clip is
// armed on ANY scene reached via a prior meter change.
void test_clip_launch_never_lands_after_a_single_meter_change() {
  Band b;
  play_one_meter_change_fixture(b);

  b.cmd(Param::kClipAdd, static_cast<std::int32_t>(TrackRole::kDrums), 0,
        static_cast<std::int32_t>(ContentKind::kStepTrack));
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0, Op::kDo, Boundary::kNextBar);
  b.advance(20 * (3 * kTicksPerBeat));

  // FINDING: this fails. The clip never promotes past kArmed; zero kClip
  // "now playing" echoes across 20 bars.
  CHECK(b.clip_playing_events() > 0);
}

// F4 (RED): Arranger::on_tick's OWN internal bar/phase gate (arranger.hpp,
// independent of Engine's now-fixed Transport gate) permanently drifts off
// the transport's real grid the moment a mid-song meter change lands while
// it is watching. Concretely (this exact scenario, values confirmed by an
// Engine-level probe): style "basic" (1-bar VarA sections) loaded, a chain
// transitions 4/4 -> 3/4 at tick 7680, then holds 3/4 for a long time. A
// style/section switch requested (kStyleSection, auto-quantized to "the next
// bar" while the transport plays) after the chain has fully settled into its
// (by then already permanently mis-phased) local cadence lands on tick
// 38400 -- NOT on any tick of the transport's own true re-anchored sequence
// (7680 + 2880*k for integer k; 38400 is 960 ticks off that grid, the exact
// same phase error class as F1-F3, just computed independently inside
// Arranger's own m_section_start/pos arithmetic rather than a BoundaryLatch/
// ClipMatrix window).
void test_arranger_internal_gate_drifts_off_transport_s_grid_after_a_meter_change() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);  // "basic": 1-bar VarA/VarB sections
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 2, 0);   // scene0: 2 bars @ 4/4
  b.add_scene(1, 20, 3);  // scene1: 20 bars @ 3/4, last -> holds

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.advance(2 * kTicksPerBar);  // land exactly at the transition tick (7680)
  CHECK(b.e.transport().ticks_per_bar() == 3 * kTicksPerBeat);

  // Let the (by-construction meter-stable) chain run for a while so
  // Arranger's own internal bar detection has had every chance to settle.
  b.advance(10 * (3 * kTicksPerBeat));  // now at tick 36480

  b.ev.clear();
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.advance(6 * (3 * kTicksPerBeat));

  Tick landed_tick = 0;
  int section_events = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kSection) {
      landed_tick = o.tick;
      ++section_events;
    }
  }
  CHECK(section_events == 1);

  // The transport's own TRUE re-anchored boundary sequence from the
  // transition (7680) onward is 7680 + 2880*k. FINDING: the switch lands at
  // tick 38400, which this loop proves is NOT a member of that sequence --
  // Arranger's own local gate detected a "boundary" at a tick the transport
  // itself never recognized as one.
  bool on_transport_s_true_grid = false;
  for (Tick k = 0; k <= 12; ++k) {
    if (landed_tick == 2 * kTicksPerBar + k * 3 * kTicksPerBeat) {
      on_transport_s_true_grid = true;
      break;
    }
  }
  CHECK(on_transport_s_true_grid);  // FAILS: landed_tick == 38400, off-grid
}

}  // namespace

int main() {
  test_pending_performance_recall_never_lands_after_a_single_meter_change();
  test_pad_fire_never_lands_after_a_single_meter_change();
  test_clip_launch_never_lands_after_a_single_meter_change();
  test_arranger_internal_gate_drifts_off_transport_s_grid_after_a_meter_change();
  return arrangrr::test::failures();
}

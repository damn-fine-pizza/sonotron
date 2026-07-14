// Functional tests for the pad-bank dispatch primitive (Phase-5 Item #9,
// docs/phase5-design-reviews.md "Pad/Scene live -> Performance"): PadEngine
// itself is pure POD bookkeeping (test_pad_bank.cpp), but the musically-
// observable behavior only exists through Engine's cmd_pad/fire_pad wiring
// (clip_request/clip_scene_launch/Arranger::request/perf_recall) -- exactly
// like test_clip.cpp -> functional.

#include "arrangrr/pad/pad_bank.hpp"

#include "arrangrr/clip/clip_matrix.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = Op::kDo;
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
  void setup_basic() {
    e.push_command(Command{.op = Op::kSet, .param = Param::kKeySet, .a = 0, .b = 0},
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8));
  }
  // a = TrackRole part_role, b = scene_index, c = ContentKind | (content_index << 8).
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8));
  }
  // Mirrors kPadAssign's exact bit packing documented in abi.hpp/engine.cpp's
  // pad_assign: a = type|(mode<<8)|(sync<<16)|(pitch<<24); b = dest_port|
  // (dest_channel<<8)|(n_bars<<16); c = source_idx|(source_aux<<24).
  void assign_pad(std::uint16_t id, PadType type, PadMode mode, Boundary sync,
                  std::uint16_t source_idx, std::uint8_t n_bars = 1,
                  PadPitch pitch = PadPitch::kFixed, std::uint8_t dest_port = 0,
                  std::uint8_t dest_channel = 0, std::uint8_t source_aux = 0) {
    const std::int32_t a =
        static_cast<std::int32_t>(type) | (static_cast<std::int32_t>(mode) << 8) |
        (static_cast<std::int32_t>(sync) << 16) | (static_cast<std::int32_t>(pitch) << 24);
    const std::int32_t b =
        dest_port | (dest_channel << 8) | (static_cast<std::int32_t>(n_bars) << 16);
    const std::int32_t c = source_idx | (static_cast<std::int32_t>(source_aux) << 24);
    cmd(Param::kPadAssign, a, b, c, id);
  }
  void trigger(std::uint16_t id) { cmd(Param::kPadTrigger, 0, 0, 0, id); }
  void release(std::uint16_t id) { cmd(Param::kPadRelease, 0, 0, 0, id); }

  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  int clip_event_count() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kClip) {
        ++n;
      }
    }
    return n;
  }
  SectionType current_section() const { return e.arranger().current(); }
};

// ---- item 3: fire_pad dispatch per type ------------------------------------

void test_pad_phrase_wraps_clip_launch() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));  // clip id 0
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kImmediate, /*source_idx=*/0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

// kChord shares the EXACT SAME clip_request dispatch as kPhrase (only the
// PadType tag differs) -- proven here with a step-track clip (mute/unmute)
// AND kHold's own launch/release pair, folding in item 4's kHold coverage.
void test_pad_chord_wraps_clip_launch_step_track_and_hold_release() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTrackNew, static_cast<std::int32_t>(TrackRole::kLead), 0 | (3 << 8));
  const std::size_t track_idx = b.e.timeline().track_count() - 1;
  b.add_clip(TrackRole::kLead, 0, ContentKind::kStepTrack, static_cast<std::uint16_t>(track_idx));
  b.assign_pad(0, PadType::kChord, PadMode::kHold, Boundary::kImmediate, /*source_idx=*/0);
  b.cmd(Param::kTransportStart);
  CHECK(!b.e.timeline().track(track_idx)->mute);  // fresh track starts un-muted
  b.ev.clear();
  b.trigger(0);  // kHold: trigger launches (un-mutes)
  CHECK(!b.e.timeline().track(track_idx)->mute);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  b.ev.clear();
  b.release(0);  // kHold: release stops (re-mutes)
  CHECK(b.e.timeline().track(track_idx)->mute);
  CHECK(b.e.clips().get(0)->state == LaunchState::kStopped);
}

void test_pad_scene_column_fans_out() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 2, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));  // clip 0, scene 2
  b.add_clip(TrackRole::kChord1, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarC));  // clip 1, DIFFERENT scene
  b.assign_pad(0, PadType::kSceneColumn, PadMode::kOneShot, Boundary::kImmediate,
               /*source_idx=scene*/ 2);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  CHECK(b.e.clips().get(1)->state == LaunchState::kStopped);  // untouched, different scene
}

void test_pad_variation_requests_section() {
  Band b;
  b.setup_basic();
  b.assign_pad(0, PadType::kVariation, PadMode::kOneShot, Boundary::kImmediate,
               static_cast<std::uint16_t>(SectionType::kVarC));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarC);
}

void test_pad_fill_requests_section() {
  Band b;
  b.setup_basic();
  b.assign_pad(0, PadType::kFill, PadMode::kOneShot, Boundary::kImmediate,
               static_cast<std::uint16_t>(SectionType::kFillA));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kFillA);
}

void test_pad_performance_recalls() {
  Band b;
  b.setup_basic();  // style 0 ("basic"), section defaults to varA
  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);                         // captures varA
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarC));  // mutate live
  CHECK(b.current_section() == SectionType::kVarC);
  b.assign_pad(0, PadType::kPerformance, PadMode::kOneShot, Boundary::kImmediate,
               /*source_idx=slot*/ 0);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarA);  // restored by the recall
}

// KNOWN v1 RESERVED (documented, not a bug): a kPerformance pad has no
// "reverse action" -- toggling OFF is a no-op (fire_pad only dispatches
// kPerformance on target == kPlaying). Confirm the recalled rig is NOT
// reverted, i.e. the no-op is genuinely inert, not silently broken.
void test_pad_performance_toggle_off_is_inert_no_revert() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kPerformanceStore, 0, 0, 0, 0);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarC));
  b.assign_pad(0, PadType::kPerformance, PadMode::kToggle, Boundary::kImmediate, 0);
  b.ev.clear();
  b.trigger(0);  // toggle on: recalls -> section back to varA
  CHECK(b.current_section() == SectionType::kVarA);
  b.ev.clear();
  b.trigger(0);  // toggle off: fire_pad no-ops for kPerformance on kStopped
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarA);  // unchanged, no revert
}

// ---- item 4: pad modes -----------------------------------------------------

void test_pad_oneshot_release_is_noop() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kImmediate, 0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  b.ev.clear();
  b.release(0);
  CHECK(b.warns() == 0);
  CHECK(b.clip_event_count() == 0);                           // no clip echo at all: true no-op
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);  // unaffected
}

void test_pad_loop_release_is_noop() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kLoop, Boundary::kImmediate, 0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  b.ev.clear();
  b.release(0);
  CHECK(b.warns() == 0);
  CHECK(b.clip_event_count() == 0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

void test_pad_toggle_alternates_launch_stop() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kToggle, Boundary::kImmediate, 0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kStopped);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

// ---- item 5: pad quantize ---------------------------------------------------

void test_pad_sync_next_bar_arms_then_fires() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kNextBar, 0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);
  CHECK(b.current_section() == SectionType::kVarA);  // not yet
  b.advance(kTicksPerBar);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  CHECK(b.current_section() == SectionType::kVarB);
}

void test_pad_sync_next_n_bars() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kNextNBars, 0, /*n_bars=*/2);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);
  CHECK(b.e.clips().get(0)->n_bars == 2);
  b.advance(kTicksPerBar);
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);  // 1 of 2 bars: not yet
  b.advance(kTicksPerBar);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);  // 2nd bar: due
}

void test_pad_sync_immediate_fires_now() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kImmediate, 0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);  // no arm phase at all
}

// The pad's OWN sync/n_bars fields, not the trigger Command's, decide the
// quantize window -- and pad_assign clamps n_bars < 1 to 1, exactly like
// BoundaryLatch::arm and ClipMatrix::arm's own clamps.
void test_pad_assign_n_bars_zero_clamps_to_one() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(0, PadType::kPhrase, PadMode::kOneShot, Boundary::kNextNBars, 0, /*n_bars=*/0);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.e.clips().get(0)->n_bars == 1);  // clamped
  b.advance(kTicksPerBar);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

void test_pad_variation_next_bar_quantize() {
  Band b;
  b.setup_basic();
  b.assign_pad(0, PadType::kVariation, PadMode::kOneShot, Boundary::kNextBar,
               static_cast<std::uint16_t>(SectionType::kVarC));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.current_section() == SectionType::kVarA);  // deferred, not immediate
  b.advance(kTicksPerBar);
  CHECK(b.current_section() == SectionType::kVarC);
}

// KNOWN v1 RESERVED (documented, not a bug): Arranger has no N-bar quantize
// primitive, so a Variation/Fill pad's kNextNBars degrades to a plain
// next-bar. Confirm the degrade is EXACTLY what lands (n_bars silently
// ignored), not that 3 bars are honored.
void test_pad_variation_next_n_bars_degrades_to_next_bar() {
  Band b;
  b.setup_basic();
  b.assign_pad(0, PadType::kVariation, PadMode::kOneShot, Boundary::kNextNBars,
               static_cast<std::uint16_t>(SectionType::kVarC), /*n_bars=*/3);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  b.advance(kTicksPerBar);                           // ONE bar, not three
  CHECK(b.current_section() == SectionType::kVarC);  // already landed: degraded to next-bar
}

// ---- validation edge cases --------------------------------------------------

void test_pad_assign_rejects_bad_fields() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.cmd(Param::kPadAssign, 7, 0, 0, 0);  // type out of range (7 > kPerformance == 6)
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.cmd(Param::kPadAssign, static_cast<std::int32_t>(PadType::kPhrase) | (4 << 8), 0, 0, 0);
  CHECK(b.warns() == 1);  // mode out of range (4 > kToggle == 3)
  b.ev.clear();
  b.cmd(Param::kPadAssign, static_cast<std::int32_t>(PadType::kPhrase) | (3 << 16), 0, 0, 0);
  CHECK(b.warns() == 1);  // sync out of range (3 > kNextNBars == 2)
  b.ev.clear();
  b.cmd(Param::kPadAssign, static_cast<std::int32_t>(PadType::kPhrase), 16 << 8, 0, 0);
  CHECK(b.warns() == 1);  // dest_channel out of range (16 > 15)
  b.ev.clear();
  b.cmd(Param::kPadAssign, static_cast<std::int32_t>(PadType::kPhrase), 0, 0,
        /*idx=*/static_cast<std::uint16_t>(kMaxPads));
  CHECK(b.warns() == 1);  // flat pad id out of range
}

void test_pad_trigger_release_bad_id_warns() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.trigger(static_cast<std::uint16_t>(kMaxPads));  // out of range: never a valid slot
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.release(static_cast<std::uint16_t>(kMaxPads));
  CHECK(b.warns() == 1);
}

void test_pad_trigger_unassigned_pad_is_silently_inert() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(5);  // valid flat id, never assign()'d -> PadType::kNone, no-op
  CHECK(b.warns() == 0);
  CHECK(b.ev.size() == 0);
}

// ---- Phase-6 Theme 3 Item #4: kPadBankSelect ------------------------------

void test_pad_bank_select_accepts_every_bank() {
  Band b;
  CHECK(b.e.pad_bank() == 0);
  for (std::uint16_t bank = 0; bank < kMaxPadBanks; ++bank) {
    b.ev.clear();
    b.cmd(Param::kPadBankSelect, static_cast<std::int32_t>(bank));
    CHECK(b.warns() == 0);
    CHECK(b.e.pad_bank() == bank);
  }
}

void test_pad_bank_select_rejects_out_of_range() {
  Band b;
  b.cmd(Param::kPadBankSelect, 5);  // valid, non-default: proves reject leaves it UNCHANGED
  CHECK(b.e.pad_bank() == 5);
  b.ev.clear();
  b.cmd(Param::kPadBankSelect, static_cast<std::int32_t>(kMaxPadBanks));  // one past the top
  CHECK(b.warns() == 1);
  CHECK(b.e.pad_bank() == 5);  // unchanged on reject
  b.ev.clear();
  b.cmd(Param::kPadBankSelect, 1000);  // large, still out of range
  CHECK(b.warns() == 1);
  CHECK(b.e.pad_bank() == 5);
  b.ev.clear();
  b.cmd(Param::kPadBankSelect, -1);  // negative
  CHECK(b.warns() == 1);
  CHECK(b.e.pad_bank() == 5);
}

// The active bank is a persisted VIEW CURSOR only: flat pad addressing
// (assign/trigger/release, 0..kMaxPads-1) is unaffected by it.
void test_pad_bank_select_does_not_change_flat_pad_addressing() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.assign_pad(3, PadType::kPhrase, PadMode::kOneShot, Boundary::kImmediate, /*source_idx=*/0);
  b.cmd(Param::kPadBankSelect, 6);
  CHECK(b.e.pad_bank() == 6);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(3);  // same flat id 3, unaffected by the active bank being 6
  CHECK(b.warns() == 0);
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

}  // namespace

int main() {
  test_pad_phrase_wraps_clip_launch();
  test_pad_chord_wraps_clip_launch_step_track_and_hold_release();
  test_pad_scene_column_fans_out();
  test_pad_variation_requests_section();
  test_pad_fill_requests_section();
  test_pad_performance_recalls();
  test_pad_performance_toggle_off_is_inert_no_revert();
  test_pad_oneshot_release_is_noop();
  test_pad_loop_release_is_noop();
  test_pad_toggle_alternates_launch_stop();
  test_pad_sync_next_bar_arms_then_fires();
  test_pad_sync_next_n_bars();
  test_pad_sync_immediate_fires_now();
  test_pad_assign_n_bars_zero_clamps_to_one();
  test_pad_variation_next_bar_quantize();
  test_pad_variation_next_n_bars_degrades_to_next_bar();
  test_pad_assign_rejects_bad_fields();
  test_pad_trigger_release_bad_id_warns();
  test_pad_trigger_unassigned_pad_is_silently_inert();
  test_pad_bank_select_accepts_every_bank();
  test_pad_bank_select_rejects_out_of_range();
  test_pad_bank_select_does_not_change_flat_pad_addressing();
  return arrangrr::test::failures();
}

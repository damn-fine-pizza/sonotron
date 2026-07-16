// Functional tests for the pad-bank dispatch primitive (Phase-5 Item #9,
// docs/phase5-design-reviews.md "Pad/Scene live -> Performance"): PadEngine
// itself is pure POD bookkeeping (test_pad_bank.cpp), but the musically-
// observable behavior only exists through Engine's cmd_pad/fire_pad wiring
// (clip_request/clip_scene_launch/Arranger::request/perf_recall) -- exactly
// like test_clip.cpp -> functional.

#include "arrangrr/pad/pad_bank.hpp"

#include <optional>

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
  // idx = kNoExplicitClipId: the legacy sequential-append form (Repeat-Zone
  // binding contract Shape A, abi.hpp's kClipAdd comment) -- explicit here
  // since Command::idx now means "explicit clip id" for kClipAdd, and this
  // helper's own callers rely on the ORIGINAL sequential-id assignment.
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8),
        kNoExplicitClipId);
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
  // type out of range: kPadTypeCount == 9 (kNone..kCC), 9 is one past the top.
  b.cmd(Param::kPadAssign, static_cast<std::int32_t>(kPadTypeCount), 0, 0, 0);
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

// ---- Phase-6 Theme 3 Item #2: pad types kDrum/kCC (direct emission) -------
// docs/reflections/phase6-theme3-pad-drum-cc-scope.md. Unlike every other
// PadType above, fire_pad builds the MidiMessage itself here, so these tests
// assert directly against the OutEvent::Kind::kMidi wire shape rather than a
// wrapped subsystem's state.

bool has_midi(const Events& ev, std::uint8_t status_type, std::uint8_t port, std::uint8_t channel,
              std::uint8_t d1, std::optional<std::uint8_t> d2 = std::nullopt) {
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == status_type && o.port == port &&
        o.msg.channel() == channel && o.msg.d1 == d1 && (!d2.has_value() || o.msg.d2 == *d2)) {
      return true;
    }
  }
  return false;
}

void test_pad_drum_oneshot_emits_note_on_then_scheduled_off() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, /*source_idx=note*/ 36,
               1, PadPitch::kFixed, /*dest_port=*/0, /*dest_channel=*/9, /*source_aux=vel*/ 100);
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 0);
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 36, 100));  // fires immediately (fire_pad flushes)
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 36));     // gate not due yet
  b.ev.clear();
  b.advance(kPadDrumOneShotGateTicks - 1);
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 36));  // still one tick short
  b.ev.clear();
  b.advance(1);  // exactly the fixed gate: the scheduled note-off is now due
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 36));
}

void test_pad_drum_default_velocity_when_source_aux_zero() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, 38, 1, PadPitch::kFixed,
               0, 0, /*source_aux=*/0);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 0, 38, kPadDrumDefaultVelocity));
}

void test_pad_drum_hold_sustains_until_release() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kHold, Boundary::kImmediate, 40, 1, PadPitch::kFixed, 1,
               2, 90);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kNoteOn, 1, 2, 40, 90));
  b.ev.clear();
  b.advance(kPadDrumOneShotGateTicks * 4);  // well past any one-shot gate -- kHold has none
  CHECK(!has_midi(b.ev, midi::kNoteOff, 1, 2, 40));
  b.ev.clear();
  b.release(0);
  CHECK(has_midi(b.ev, midi::kNoteOff, 1, 2, 40));
}

void test_pad_drum_toggle_flips_note_on_off() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kToggle, Boundary::kImmediate, 42, 1, PadPitch::kFixed,
               0, 9, 80);
  b.ev.clear();
  b.trigger(0);  // toggle on
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 42, 80));
  b.ev.clear();
  b.trigger(0);  // toggle off
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 42));
}

void test_pad_drum_out_of_range_note_or_velocity_warns() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, 200, 1, PadPitch::kFixed,
               0, 0, 100);  // note > 127
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 1);
  b.assign_pad(1, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, 60, 1, PadPitch::kFixed,
               0, 0, 200);  // velocity > 127
  b.ev.clear();
  b.trigger(1);
  CHECK(b.warns() == 1);
}

void test_pad_cc_oneshot_emits_cc_once_release_is_noop() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kOneShot, Boundary::kImmediate, /*source_idx=cc#*/ 74, 1,
               PadPitch::kFixed, 0, 3, /*source_aux=value*/ 100);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 3, 74, 100));
  b.ev.clear();
  b.release(0);  // kOneShot: release is a no-op (Decision 4 -- no reverse action)
  CHECK(b.ev.size() == 0);
}

void test_pad_cc_hold_sends_zero_on_release() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kHold, Boundary::kImmediate, 1, 1, PadPitch::kFixed, 0, 0,
               127);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 1, 127));
  b.ev.clear();
  b.release(0);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 1, 0));  // hardcoded off-value (Decision 4)
}

void test_pad_cc_toggle_flips_value_and_zero() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kToggle, Boundary::kImmediate, 64, 1, PadPitch::kFixed, 0,
               0, 127);
  b.ev.clear();
  b.trigger(0);  // toggle on
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 64, 127));
  b.ev.clear();
  b.trigger(0);  // toggle off
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 64, 0));
}

void test_pad_cc_out_of_range_controller_or_value_warns() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kOneShot, Boundary::kImmediate, 200, 1, PadPitch::kFixed,
               0, 0, 50);  // controller > 127
  b.ev.clear();
  b.trigger(0);
  CHECK(b.warns() == 1);
  b.assign_pad(1, PadType::kCC, PadMode::kOneShot, Boundary::kImmediate, 20, 1, PadPitch::kFixed, 0,
               0, 200);  // value > 127
  b.ev.clear();
  b.trigger(1);
  CHECK(b.warns() == 1);
}

// pad_assign's widened bound check accepts both new types; flat addressing
// (0..kMaxPads-1) is unaffected -- two different pad ids fire independently.
void test_pad_assign_accepts_drum_and_cc_addressing_unaffected() {
  Band b;
  b.ev.clear();
  b.assign_pad(3, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, 36, 1, PadPitch::kFixed,
               0, 9, 100);
  CHECK(b.warns() == 0);
  b.ev.clear();
  b.assign_pad(7, PadType::kCC, PadMode::kOneShot, Boundary::kImmediate, 74, 1, PadPitch::kFixed, 0,
               0, 100);
  CHECK(b.warns() == 0);
  b.ev.clear();
  b.trigger(3);
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 36, 100));
  b.ev.clear();
  b.trigger(7);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 74, 100));
}

// Panic (NoteTracker) silences a sounding Drum/CC pad for free -- fire_pad's
// immediate flush() feeds NoteTracker::observe exactly like every other
// emission path (Decision 5: no bespoke tracking needed).
void test_pad_panic_silences_sounding_drum_pad() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kHold, Boundary::kImmediate, 36, 1, PadPitch::kFixed, 0,
               9, 100);
  b.trigger(0);  // note sounding, held (kHold has no auto-off)
  b.ev.clear();
  b.cmd(Param::kPanic);
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 36));
}

void test_pad_panic_silences_sounding_cc_pad() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kHold, Boundary::kImmediate, 64, 1, PadPitch::kFixed, 0, 0,
               127);
  b.trigger(0);  // CC on-value sent, held
  b.ev.clear();
  b.cmd(Param::kPanic);
  // NoteTracker::panic only tracks note on/off + CC64 sustain (note_tracker.hpp);
  // it does not revert an arbitrary CC value -- confirm panic is genuinely
  // inert for a CC pad rather than silently (and wrongly) zeroing it itself.
  CHECK(!has_midi(b.ev, midi::kControlChange, 0, 0, 64, 0));
}

// ---- Torquato hardening: kLoop coverage (mandate: "kLoop shares the
// 120-tick gate -- verify its actual behavior matches intent... pin whatever
// it actually does"). fire_pad_drum folds kLoop into the exact same branch
// as kOneShot (Decision 3); these tests PIN that fold so a regression that
// splits the two behaviors apart again is caught.

void test_pad_drum_loop_behaves_exactly_like_oneshot() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kLoop, Boundary::kImmediate, 37, 1, PadPitch::kFixed, 0,
               9, 100);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 37, 100));
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 37));  // gate not due yet -- no auto-repeat either
  b.ev.clear();
  b.advance(kPadDrumOneShotGateTicks - 1);
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 37));
  b.ev.clear();
  b.advance(1);
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 37));  // exactly one gated off, no re-trigger
  b.ev.clear();
  b.release(0);  // pad_release is a no-op for kLoop, mirroring kOneShot
  CHECK(b.ev.size() == 0);
}

void test_pad_cc_loop_fires_once_release_is_noop() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kLoop, Boundary::kImmediate, 20, 1, PadPitch::kFixed, 0, 5,
               90);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 5, 20, 90));
  b.ev.clear();
  b.release(0);  // kLoop folded to kOneShot: no reverse action
  CHECK(b.ev.size() == 0);
}

// ---- Torquato hardening: documented actual behavior at two interaction
// edges the mandate flagged as needing verification. Neither is a stuck-note
// or crash risk (confirmed below), so these are pinned GREEN as documentation
// of the real contract, not filed as defects -- see the report for the
// distinction from the three RED pins further down.

// kHold's trigger dispatch (engine.cpp's pad_trigger) does NOT check
// PadRuntime::on before firing -- unlike kToggle, a second trigger while
// already held re-sends note_on (a hardware "restrike"), not a suppressed
// no-op. NoteTracker's bitmask tracking (note_tracker.hpp) makes this safe:
// a repeated NoteOn just re-sets the same bit, and the single matching
// release still clears it -- no stuck note, no double note-off.
void test_pad_drum_hold_second_trigger_without_release_restrikes_not_stuck() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kHold, Boundary::kImmediate, 40, 1, PadPitch::kFixed, 0,
               9, 90);
  b.ev.clear();
  b.trigger(0);
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 40, 90));
  b.ev.clear();
  b.trigger(0);  // second trigger, no release in between: re-strikes
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 40, 90));
  b.ev.clear();
  b.release(0);  // a SINGLE release still fully clears it -- no leaked stuck note
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 40));
  b.ev.clear();
  b.cmd(Param::kPanic);  // NoteTracker::panic always emits its 3 housekeeping CCs for any
                         // channel that ever saw traffic (note_tracker.hpp), regardless of
                         // whether anything is currently sounding -- assert on note
                         // messages specifically, not the raw event count.
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 40));  // nothing WAS sounding: no leaked note-off
  CHECK(!has_midi(b.ev, midi::kNoteOn, 0, 9, 40));
}

// pad_release for kHold does not check whether the pad is actually sounding
// before firing: releasing a never-triggered kHold pad still emits a
// note-off / CC-0 on the wire. Confirmed SAFE (no warn, no crash, NoteTracker
// silently no-ops clearing an already-clear bit) -- just not a true
// zero-event no-op. Flagged in the report as a minor polish item, not a
// defect.
void test_pad_drum_release_with_nothing_sounding_is_safe() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kHold, Boundary::kImmediate, 44, 1, PadPitch::kFixed, 0,
               9, 80);
  b.ev.clear();
  b.release(0);  // never triggered
  CHECK(b.warns() == 0);
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 44));  // NOT a true no-op -- documented, not a bug
}

void test_pad_cc_release_with_nothing_sounding_is_safe() {
  Band b;
  b.assign_pad(0, PadType::kCC, PadMode::kHold, Boundary::kImmediate, 10, 1, PadPitch::kFixed, 0, 0,
               80);
  b.ev.clear();
  b.release(0);  // never triggered
  CHECK(b.warns() == 0);
  CHECK(has_midi(b.ev, midi::kControlChange, 0, 0, 10, 0));  // hardcoded-0 off, not a true no-op
}

// ============================================================================
// Torquato findings (Phase-6 Theme 3 Item #2 hand-off), fixed by Nazzareno.
// Each asserts the behavior consistent with the rest of the pad system / with
// musical correctness; each used to FAIL against the pre-fix fire_pad_drum/
// fire_pad_cc implementation, for the precise reason stated in its own
// comment -- kept registered here as permanent regression tests so none of
// the three defects can silently return.
// ============================================================================

// FINDING 1 -- pad.sync/pad.n_bars used to be silently ignored by kDrum/kCC.
// pad_assign structurally validates sync (kImmediate/kNextBar/kNextNBars) and
// n_bars identically for EVERY PadType, kDrum/kCC included (engine.cpp:928-
// 932), and EVERY OTHER PadType honors sync in some form: kPhrase/kChord get
// a true N-bar quantize via ClipMatrix; kVariation/kFill honor kNextBar and
// (documented, test_pad_variation_next_n_bars_degrades_to_next_bar above)
// degrade kNextNBars to next-bar. fire_pad_drum/fire_pad_cc used to consult
// ONLY `target` (kPlaying/kStopped) -- pad.sync and pad.n_bars were never
// read at all. Fixed: fire_pad's kDrum/kCC case now arms the SAME shared
// BoundaryLatch primitive m_perf_recall already uses (one instance per pad
// slot), promoted by apply_pending_pad_fires at the next bar boundary,
// mirroring kVariation/kFill's own kNextBar/kNextNBars-degrades-to-next-bar
// contract exactly.
void test_pad_drum_next_bar_sync_defers_to_boundary() {
  Band b;
  b.setup_basic();
  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kNextBar, 36, 1, PadPitch::kFixed, 0,
               9, 100);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.trigger(0);
  // A kNextBar-synced pad must NOT sound before the boundary arrives.
  CHECK(!has_midi(b.ev, midi::kNoteOn, 0, 9, 36, 100));
  b.advance(kTicksPerBar);
  // ... and DOES sound once the armed boundary lands (arm + promote both
  // proven, not just the absence of an immediate fire).
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 36, 100));
}

// FINDING 2 -- a kOneShot/kLoop Drum pad retriggered before its own fixed
// gate elapses used to be cut short by the FIRST trigger's now-stale
// scheduled note-off, not its own. Trigger at t=0 schedules note_off at
// t=120; a retrigger at t=50 (a legitimate "drum roll", well within one gate
// window) schedules a SECOND, independent note_off at t=170 -- but the stale
// t=120 event used to fire first, silencing the retriggered hit 50 ticks
// early. Fixed: fire_pad_drum now calls the scheduler's own dedicated
// retrigger primitive (OutScheduler::cancel_note_off, §9.B) before
// scheduling a kOneShot/kLoop note-on, tombstoning any pending off for the
// same (port, channel, note) due at or after now and re-anchoring it to fire
// right now instead of at its stale tick -- exactly the mechanism
// fire_timeline's own schedule_pattern already uses for the same
// same-note-overlap case.
void test_pad_drum_oneshot_retrigger_not_cut_short_by_stale_gate() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kOneShot, Boundary::kImmediate, 50, 1, PadPitch::kFixed,
               0, 9, 100);
  b.ev.clear();
  b.trigger(0);   // t=0: note_on, note_off scheduled for t=120
  b.advance(50);  // t=50
  b.ev.clear();
  b.trigger(0);  // t=50: RETRIGGER -- note_on again, note_off scheduled for t=170
  b.ev.clear();
  b.advance(70);  // t=120: the FIRST trigger's now-stale gate is due
  // EXPECTED: the retriggered hit (started at t=50) is still within its OWN
  // 120-tick gate (due at t=170) and must still be sounding.
  CHECK(!has_midi(b.ev, midi::kNoteOff, 0, 9, 50));
  b.ev.clear();
  b.advance(50);  // t=170: the retriggered hit's OWN gate is now due
  int off_count = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOff && o.port == 0 &&
        o.msg.channel() == 9 && o.msg.d1 == 50) {
      ++off_count;
    }
  }
  // Exactly one note-off lands here -- no stuck note, no redundant duplicate
  // left over from the cancelled stale gate.
  CHECK(off_count == 1);
}

// FINDING 3 -- Panic used to desync a kToggle Drum/CC pad's own on/off
// runtime state. `cmd_routing`'s kPanic case (engine.cpp:155-162) drives
// NoteTracker::panic + m_chorddet.clear() + m_arp.panic() -- it never touched
// m_pads/PadRuntime. A toggled-ON Drum/CC pad was correctly silenced at the
// wire by Panic, but `PadRuntime::on` for that pad stayed TRUE, so the pad's
// next trigger flipped rt->on FALSE (interpreting it as "turn off an
// already-on pad") and sent ANOTHER note-off/CC-0 instead of sounding the
// pad again -- the user needed to trigger the pad TWICE after a Panic to
// hear it again. Fixed: kPanic now resets every pad's PadRuntime::on to
// false, for every PadType uniformly (kHold included -- same class of stale
// on-state, even though kHold's own trigger dispatch happens not to consult
// it today).
void test_pad_drum_toggle_panic_resyncs_runtime_state() {
  Band b;
  b.assign_pad(0, PadType::kDrum, PadMode::kToggle, Boundary::kImmediate, 42, 1, PadPitch::kFixed,
               0, 9, 80);
  b.trigger(0);  // toggle ON: note_on 42, PadRuntime::on == true
  b.ev.clear();
  b.cmd(Param::kPanic);
  CHECK(has_midi(b.ev, midi::kNoteOff, 0, 9, 42));  // sanity: Panic DID silence the wire
  b.ev.clear();
  b.trigger(0);  // the pad is now silent -- this trigger should sound it again
  // EXPECTED: re-triggering a Panic-silenced toggle pad sounds it again.
  CHECK(has_midi(b.ev, midi::kNoteOn, 0, 9, 42, 80));
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
  test_pad_drum_oneshot_emits_note_on_then_scheduled_off();
  test_pad_drum_default_velocity_when_source_aux_zero();
  test_pad_drum_hold_sustains_until_release();
  test_pad_drum_toggle_flips_note_on_off();
  test_pad_drum_out_of_range_note_or_velocity_warns();
  test_pad_cc_oneshot_emits_cc_once_release_is_noop();
  test_pad_cc_hold_sends_zero_on_release();
  test_pad_cc_toggle_flips_value_and_zero();
  test_pad_cc_out_of_range_controller_or_value_warns();
  test_pad_assign_accepts_drum_and_cc_addressing_unaffected();
  test_pad_panic_silences_sounding_drum_pad();
  test_pad_panic_silences_sounding_cc_pad();
  test_pad_drum_loop_behaves_exactly_like_oneshot();
  test_pad_cc_loop_fires_once_release_is_noop();
  test_pad_drum_hold_second_trigger_without_release_restrikes_not_stuck();
  test_pad_drum_release_with_nothing_sounding_is_safe();
  test_pad_cc_release_with_nothing_sounding_is_safe();
  // Torquato findings, fixed by Nazzareno -- kept registered as permanent
  // regression tests (see the comment above each one).
  test_pad_drum_next_bar_sync_defers_to_boundary();
  test_pad_drum_oneshot_retrigger_not_cut_short_by_stale_gate();
  test_pad_drum_toggle_panic_resyncs_runtime_state();
  return arrangrr::test::failures();
}

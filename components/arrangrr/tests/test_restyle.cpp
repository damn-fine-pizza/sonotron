#include "test.hpp"

#include <vector>

#include "arrangrr/arranger/style.hpp"  // styles::kBuiltins
#include "arrangrr/restyle/restyle_stage.hpp"

using namespace arrangrr;
using arrangrr::restyle::Classification;
using arrangrr::restyle::ToneKind;

namespace {

constexpr Key kCMajor{.root_pc = 0, .mode = Mode::kMajor};
constexpr ChordState kCMaj{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};

}  // namespace

// --- restyle::classify -------------------------------------------------------

// A pitch matching one of the chord's own offsets is a chord tone, index
// order root/3rd/5th (theory::shape_of(kMaj) = {0,4,7}).
static void test_classify_chord_tones() {
  const Classification root = restyle::classify(kCMajor, kCMaj, 60);  // C
  CHECK(root.kind == ToneKind::kChordTone);
  CHECK(root.index == 0);
  const Classification third = restyle::classify(kCMajor, kCMaj, 64);  // E
  CHECK(third.kind == ToneKind::kChordTone);
  CHECK(third.index == 1);
  const Classification fifth = restyle::classify(kCMajor, kCMaj, 67);  // G
  CHECK(fifth.kind == ToneKind::kChordTone);
  CHECK(fifth.index == 2);
  // Octave-independent: any octave of the same pitch class classifies the same.
  const Classification root_up = restyle::classify(kCMajor, kCMaj, 72);  // C an octave up
  CHECK(root_up.kind == ToneKind::kChordTone);
  CHECK(root_up.index == 0);
}

// A pitch in-key but not a chord tone (the ii over a I chord) is a scale
// degree, not a chord tone and not reharmonized.
static void test_classify_scale_degree() {
  const Classification d = restyle::classify(kCMajor, kCMaj, 62);  // D: degree 1 (ii)
  CHECK(d.kind == ToneKind::kScaleDegree);
  CHECK(d.index == 1);
}

// A chromatic pitch (out of both the chord and the key) classifies as
// kNonChordTone -- the first-slice scope gate's pass-through case.
static void test_classify_chromatic_passes_through() {
  const Classification cs = restyle::classify(kCMajor, kCMaj, 61);  // C#: chromatic in C major
  CHECK(cs.kind == ToneKind::kNonChordTone);
  CHECK(cs.index == -1);
}

// With no valid chord (silence before the first chord sounds), only the key
// is consulted -- a diatonic pitch still classifies as a scale degree.
static void test_classify_falls_back_to_key_when_chord_invalid() {
  const ChordState invalid{};
  const Classification d = restyle::classify(kCMajor, invalid, 62);  // D
  CHECK(d.kind == ToneKind::kScaleDegree);
  CHECK(d.index == 1);
}

// --- restyle::snap_to_grid ---------------------------------------------------

static void test_snap_to_grid_rounds_to_nearest_16th() {
  CHECK(restyle::snap_to_grid(0) == 0);
  CHECK(restyle::snap_to_grid(1) == 0);                // rounds down
  CHECK(restyle::snap_to_grid(119) == 0);              // just under half
  CHECK(restyle::snap_to_grid(120) == kTicksPerStep);  // exact half rounds up
  CHECK(restyle::snap_to_grid(121) == kTicksPerStep);
  CHECK(restyle::snap_to_grid(239) == kTicksPerStep);
  CHECK(restyle::snap_to_grid(240) == kTicksPerStep);  // already on the grid
  CHECK(restyle::snap_to_grid(2 * kTicksPerStep + 200) == 3 * kTicksPerStep);
}

// --- restyle::nearest_octave_to / role_anchor -------------------------------

static void test_nearest_octave_to_moves_toward_anchor() {
  CHECK(restyle::nearest_octave_to(60, 72) == 72);  // C4 -> C5 (kLead's anchor)
  CHECK(restyle::nearest_octave_to(36, 72) == 72);  // low C -> same, several octaves up
  CHECK(restyle::nearest_octave_to(72, 72) == 72);  // already there
  CHECK(restyle::role_anchor(TrackRole::kLead) == 72);
  CHECK(restyle::role_anchor(TrackRole::kBass) == 36);
}

// --- RestyleStage -------------------------------------------------------------

namespace {

constexpr std::size_t kPorts = 4;

// RestyleStage (like Engine) hardcodes OutScheduler<kSchedulerCapacity> (it
// lives in the arrangrr package and schedules through the SAME concrete
// scheduler type Engine does, restyle_stage.hpp's header comment) -- unlike
// midisrc::MidiSourceStage, it is not templated on the scheduler capacity.
struct Harness {
  FollowedContext followed;
  ChorddetStage<kPorts> chorddet{followed};
  OutScheduler<kSchedulerCapacity> scheduler;
  Transport transport;
  RestyleStage<kPorts> stage{scheduler,
                             transport,
                             chorddet,
                             followed,
                             static_cast<std::uint8_t>(2),
                             static_cast<std::uint8_t>(0)};

  void feed(std::uint8_t port, std::initializer_list<std::uint8_t> bytes) {
    const std::vector<std::uint8_t> v(bytes);
    stage.push_midi_in(port, v.data(), v.size(), [](Producer) {});
  }
  void tick(Tick now) {
    const runtime::StageContext ctx{.now = now};
    stage.on_tick(ctx, [](const ScheduledEvent&) {});
  }
  std::vector<ScheduledEvent> drain(Tick now) {
    std::vector<ScheduledEvent> out;
    scheduler.pop_due(now, [&](const ScheduledEvent& ev) { out.push_back(ev); });
    return out;
  }
};

}  // namespace

// Inert by default: with no style loaded, inbound bytes never schedule
// anything (byte-identical when unused, the same convention every other
// stage in this pipeline establishes).
static void test_restyle_stage_inert_until_loaded() {
  Harness h;
  h.followed.establish_default(kCMajor);
  h.tick(1);
  h.feed(0, {0x90, 60, 100});
  h.feed(0, {0x80, 60, 0});
  CHECK(h.drain(10000).empty());
}

// Once loaded, a chord-tone input note is scheduled on the configured
// port/channel, moved into the kLead anchor register (72), and its matching
// note-off arrives with the ORIGINAL gate preserved.
static void test_restyle_stage_schedules_anchored_chord_tone() {
  Harness h;
  CHECK(h.stage.load_style(
      styles::kBuiltins[0]));             // "basic": no authored kLead pattern -> kAsWritten
  h.followed.establish_default(kCMajor);  // C major triad

  h.tick(1);
  h.feed(0, {0x90, 60, 100});  // C4, a chord tone (root)
  h.tick(2);
  h.tick(3);
  h.feed(0, {0x80, 60, 0});  // note-off two ticks later (a tiny, deliberately short gate)

  const std::vector<ScheduledEvent> events = h.drain(100000);
  CHECK(events.size() == 2);
  bool saw_on = false;
  bool saw_off = false;
  for (const ScheduledEvent& ev : events) {
    CHECK(ev.port == 2);
    CHECK(ev.msg.channel() == 0);
    if (ev.msg.type() == midi::kNoteOn) {
      saw_on = true;
      CHECK(ev.msg.d1 == 72);  // anchored to kLead's register, not the input's own octave
    } else if (ev.msg.type() == midi::kNoteOff) {
      saw_off = true;
      CHECK(ev.msg.d1 == 72);
    }
  }
  CHECK(saw_on);
  CHECK(saw_off);
}

// A non-chord-tone (in-key but not a chord tone) passes through at its
// ORIGINAL pitch -- no octave move, no reharmonization.
static void test_restyle_stage_passes_scale_degree_through_unchanged_pitch() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0]));
  h.followed.establish_default(kCMajor);

  h.tick(1);
  h.feed(0, {0x90, 62, 100});  // D4, scale degree (ii), not a chord tone
  h.tick(2);
  h.feed(0, {0x80, 62, 0});

  const std::vector<ScheduledEvent> events = h.drain(100000);
  bool saw_on = false;
  for (const ScheduledEvent& ev : events) {
    if (ev.msg.type() == midi::kNoteOn) {
      saw_on = true;
      CHECK(ev.msg.d1 == 62);  // unchanged: the scope gate defers reharmonization
    }
  }
  CHECK(saw_on);
}

// The core correctness property (the task's own gate): re-classifying the
// OUTPUT note against the SAME chord/key must yield the SAME tone kind/index
// the INPUT note classified as -- harmony is preserved even though the pitch
// (register) and the tick moved.
static void test_restyle_stage_preserves_harmony_of_every_note() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0]));
  h.followed.establish_default(kCMajor);

  const std::vector<std::uint8_t> inputs = {60, 64, 67, 62,
                                            65};  // C E G (chord tones), D, F (scale degrees)
  Tick t = 1;
  h.tick(t);
  for (std::uint8_t note : inputs) {
    h.feed(0, {0x90, note, 100});
    ++t;
    h.tick(t);
    h.feed(0, {0x80, note, 0});
    ++t;
    h.tick(t);
  }

  const std::vector<ScheduledEvent> events = h.drain(200000);
  CHECK(events.size() == inputs.size() * 2);  // one on + one off per input note

  std::size_t checked = 0;
  for (std::size_t i = 0; i < events.size() && checked < inputs.size(); ++i) {
    if (events[i].msg.type() != midi::kNoteOn) {
      continue;
    }
    const std::uint8_t input_note = inputs[checked];
    const Classification before = restyle::classify(kCMajor, kCMaj, input_note);
    const Classification after = restyle::classify(kCMajor, kCMaj, events[i].msg.d1);
    CHECK(before.kind == after.kind);
    CHECK(before.index == after.index);
    ++checked;
  }
  CHECK(checked == inputs.size());
}

// --- Channel filter (roadmap 9320, second slice) ----------------------------

// Default mask excludes the GM drum channel (9, 0-based): a chord-tone note
// arriving on that channel is forwarded VERBATIM (original pitch, original
// channel, no octave anchor) instead of being musically transformed -- the
// real-bug fix (restyle-musical-scope.md/-placement.md's first slice was
// channel-blind).
static void test_restyle_stage_default_mask_passes_drum_channel_through_unchanged() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0]));
  h.followed.establish_default(kCMajor);
  CHECK(h.stage.channel_mask() == restyle::kDefaultChannelMask);

  h.tick(1);
  h.feed(0, {0x99, 60, 100});  // C4 on channel 9 (0-based): a chord tone, but drum channel
  h.tick(2);
  h.feed(0, {0x89, 60, 0});

  const std::vector<ScheduledEvent> events = h.drain(100000);
  CHECK(events.size() == 2);
  for (const ScheduledEvent& ev : events) {
    CHECK(ev.msg.channel() == 9);  // untouched: original channel, not the fixed output one
    CHECK(ev.msg.d1 == 60);        // untouched: original pitch, no anchor move
  }
}

// A channel INSIDE the mask keeps the ORIGINAL transformed behavior (same
// anchored register the first-slice test above already pins) -- the filter
// only carves out excluded channels, it does not change in-mask behavior.
static void test_restyle_stage_in_mask_channel_still_transforms() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0]));
  h.followed.establish_default(kCMajor);

  h.tick(1);
  h.feed(0, {0x90, 60, 100});  // C4 on channel 0: inside the default mask
  h.tick(2);
  h.feed(0, {0x80, 60, 0});

  const std::vector<ScheduledEvent> events = h.drain(100000);
  bool saw_on = false;
  for (const ScheduledEvent& ev : events) {
    if (ev.msg.type() == midi::kNoteOn) {
      saw_on = true;
      CHECK(ev.msg.channel() == 0);  // the stage's fixed OUTPUT channel
      CHECK(ev.msg.d1 == 72);        // anchored to kLead's register
    }
  }
  CHECK(saw_on);
}

// The mask is overridable: widening it to include the drum channel makes
// that channel's chord tones transform exactly like any other.
static void test_restyle_stage_channel_mask_is_overridable() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0]));
  h.followed.establish_default(kCMajor);
  h.stage.set_channel_mask(restyle::kAllChannels);
  CHECK(h.stage.channel_mask() == restyle::kAllChannels);

  h.tick(1);
  h.feed(0, {0x99, 60, 100});  // C4 on channel 9, now inside the widened mask
  h.tick(2);
  h.feed(0, {0x89, 60, 0});

  const std::vector<ScheduledEvent> events = h.drain(100000);
  bool saw_on = false;
  for (const ScheduledEvent& ev : events) {
    if (ev.msg.type() == midi::kNoteOn) {
      saw_on = true;
      CHECK(ev.msg.channel() == 0);  // the stage's fixed OUTPUT channel, not channel 9 anymore
      CHECK(ev.msg.d1 == 72);        // anchored, same as any other in-mask chord tone
    }
  }
  CHECK(saw_on);
}

// --- Target role argument (roadmap 9320, second slice) ----------------------

// `load_style`'s optional role parameter changes the register anchor / policy
// read (role_anchor/target_voicing_policy) away from the ORIGINAL fixed
// kLead default; omitting it (test_restyle_stage_schedules_anchored_chord_tone
// above) keeps the exact prior behavior.
static void test_restyle_stage_target_role_overrides_anchor() {
  Harness h;
  CHECK(h.stage.load_style(styles::kBuiltins[0], TrackRole::kBass));
  CHECK(h.stage.target_role() == TrackRole::kBass);
  h.followed.establish_default(kCMajor);

  h.tick(1);
  h.feed(0, {0x90, 60, 100});  // C4, a chord tone (root)
  h.tick(2);
  h.feed(0, {0x80, 60, 0});

  const std::vector<ScheduledEvent> events = h.drain(100000);
  bool saw_on = false;
  for (const ScheduledEvent& ev : events) {
    if (ev.msg.type() == midi::kNoteOn) {
      saw_on = true;
      CHECK(ev.msg.d1 == restyle::role_anchor(TrackRole::kBass));  // 36, not kLead's 72
    }
  }
  CHECK(saw_on);
}

int main() {
  test_classify_chord_tones();
  test_classify_scale_degree();
  test_classify_chromatic_passes_through();
  test_classify_falls_back_to_key_when_chord_invalid();
  test_snap_to_grid_rounds_to_nearest_16th();
  test_nearest_octave_to_moves_toward_anchor();
  test_restyle_stage_inert_until_loaded();
  test_restyle_stage_schedules_anchored_chord_tone();
  test_restyle_stage_passes_scale_degree_through_unchanged_pitch();
  test_restyle_stage_preserves_harmony_of_every_note();
  test_restyle_stage_default_mask_passes_drum_channel_through_unchanged();
  test_restyle_stage_in_mask_channel_still_transforms();
  test_restyle_stage_channel_mask_is_overridable();
  test_restyle_stage_target_role_overrides_anchor();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_restyle: all OK\n");
  }
  return arrangrr::test::failures();
}

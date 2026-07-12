// Dxx functional tests: two-zone harmony input + scale-aware single-finger.
//
// The feature has two halves, both exercised here through the real binary ABI
// (push_command / push_midi_in) and asserted on OBSERVABLE output:
//
//   1. Per-input-port InputZone. A kHarmony port's note messages are
//      OUTPUT-SUPPRESSED (they never reach the router -> no note OutEvents) yet
//      still OBSERVED by the detector when it is the detect port, so the band
//      re-harmonizes SILENTLY. A kMelody port routes normally and only steers if
//      it is also the detect port. Non-note traffic (CC/program) passes on
//      kHarmony. Default kMelody keeps every port byte-identical to legacy.
//
//   2. Scale-aware single-finger. One key -> the diatonic maj/min TRIAD of that
//      root in the current key: dim degree snaps to MINOR, aug to MAJOR, an
//      out-of-key root defaults to MAJOR (never rejected). This is DISTINCT from
//      diatonic mode, which is D20-strict (rejects out-of-key roots) and D19-rich
//      (adds sevenths).
//
// Subject under test is the Engine's own cross-producer coordination
// (route-vs-suppress on the shared input path, detector steering the followed
// chord) plus the ChordEngine distinctness invariant -> `functional`.

#include "arrangrr/chord/chord_engine.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

// A thru route packs in/out port + channel exactly like test_engine's helper.
Command route_add(std::uint8_t in_port, std::int8_t in_ch, std::uint8_t out_port,
                  std::int8_t out_ch, std::uint8_t pass) {
  Command c;
  c.op = Op::kDo;
  c.param = Param::kRouteAdd;
  c.a = in_port | ((in_ch & 0xFF) << 8);
  c.b = out_port | ((out_ch & 0xFF) << 8);
  c.c = pass;
  return c;
}

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void push(const Command& command) {
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds one NoteOn (vel>0) or NoteOff (vel==0) on an arbitrary input port.
  void key(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds three simultaneous NoteOns (a triad) on one input port.
  void triad(std::uint8_t port, std::uint8_t a, std::uint8_t b, std::uint8_t c) {
    const std::uint8_t bytes[9] = {0x90, a, 100, 0x90, b, 100, 0x90, c, 100};
    e.push_midi_in(port, Span<const std::uint8_t>(bytes, 9),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }

  const ChordState& followed() const { return e.chords().state(); }

  // Note messages (on/off) emitted on a given OUTPUT port.
  int note_events_on_port(std::uint8_t port) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port &&
          (o.msg.type() == midi::kNoteOn || o.msg.type() == midi::kNoteOff)) {
        ++n;
      }
    }
    return n;
  }
  // Any CC message emitted on a given OUTPUT port.
  bool saw_cc_on_port(std::uint8_t port) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port &&
          o.msg.type() == midi::kControlChange) {
        return true;
      }
    }
    return false;
  }
};

constexpr std::uint8_t kHarmonyPort = 1;  // silent chord zone + detect source
constexpr std::uint8_t kMelodyPort = 2;   // routes/sounds, not the detect port
constexpr std::uint8_t kOutPort = 3;      // both thru routes land here

// --- 1. Silent re-harmonize: kHarmony observes but never routes -------------
// A kHarmony detect port steers the followed chord while emitting ZERO note
// output; a kMelody port that is NOT the detect port routes its notes out and
// does NOT steer. One Band proves both directions of the split.
void test_harmony_silent_steers_melody_routes() {
  Band b;
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
  b.cmd(Param::kInputZone, kHarmonyPort, static_cast<std::int32_t>(InputZone::kHarmony), 0,
        Op::kSet);
  b.cmd(Param::kChordDetect, 1, kHarmonyPort, 0, Op::kSet);  // detect on, source = harmony port
  b.push(route_add(kHarmonyPort, -1, kOutPort, -1, route_pass::kAll));  // thru harmony -> out
  b.push(route_add(kMelodyPort, -1, kOutPort, -1, route_pass::kAll));   // thru melody  -> out

  // C major triad on the HARMONY port: suppressed from output, observed for
  // detection -> silent re-harmonization.
  b.triad(kHarmonyPort, 60, 64, 67);
  CHECK(b.note_events_on_port(kOutPort) == 0);  // silent: never reaches the router
  CHECK(b.followed().valid);                    // but it steered the band
  CHECK(b.followed().root_pc == 0);
  CHECK(b.followed().quality == ChordQuality::kMaj);

  // Same gesture on the MELODY port (kMelody, and NOT the detect port): the
  // notes DO route out, and the followed chord is UNTOUCHED (no steer).
  b.ev.clear();
  b.triad(kMelodyPort, 62, 65, 69);             // D minor triad
  CHECK(b.note_events_on_port(kOutPort) == 3);  // three note-ons routed through
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 0);  // still C, the melody port did not steer
  CHECK(b.followed().quality == ChordQuality::kMaj);
}

// A kHarmony port suppresses only NOTES; CC (and other non-note traffic) still
// passes through the router.
void test_harmony_passes_non_note_traffic() {
  Band b;
  b.cmd(Param::kInputZone, kHarmonyPort, static_cast<std::int32_t>(InputZone::kHarmony), 0,
        Op::kSet);
  b.push(route_add(kHarmonyPort, -1, kOutPort, -1, route_pass::kAll));

  const std::uint8_t note_and_cc[6] = {0x90, 60, 100, 0xB0, 7, 99};  // NoteOn + CC7
  b.e.push_midi_in(kHarmonyPort, Span<const std::uint8_t>(note_and_cc, 6),
                   [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  CHECK(b.note_events_on_port(kOutPort) == 0);  // the note is silenced
  CHECK(b.saw_cc_on_port(kOutPort));            // the CC still passes
}

// Default zone is kMelody: an untouched port stays byte-identical to legacy —
// its notes route out normally.
void test_default_zone_is_melody() {
  Band b;
  CHECK(b.e.input_zone(kMelodyPort) == InputZone::kMelody);
  b.push(route_add(kMelodyPort, -1, kOutPort, -1, route_pass::kAll));
  b.triad(kMelodyPort, 60, 64, 67);
  CHECK(b.note_events_on_port(kOutPort) == 3);  // legacy pass-through intact
}

// --- 2. Scale-aware single-finger: one key -> diatonic maj/min triad --------
// Driven through the live detector one-key path: chord mode `single` drops the
// detector minimum to one held note and flips it into scale-aware resolution.

// Presses ONE key on the detect port (0), captures the followed chord, releases.
ChordState single_press(Band& b, std::uint8_t note) {
  b.key(0, note, 100);
  const ChordState s = b.followed();
  b.key(0, note, 0);  // release so the next press starts clean (memory holds s)
  return s;
}

void setup_single_finger_C_major(Band& b) {
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
  b.cmd(Param::kChordMode, static_cast<std::int32_t>(ChordMode::kSingle), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detect on, source = port 0
}

void test_single_finger_scale_aware_c_major() {
  Band b;
  setup_single_finger_C_major(b);
  struct Case {
    std::uint8_t note;
    std::uint8_t root_pc;
    ChordQuality quality;
  };
  // C major degrees (octave 4 roots): I C, ii D, iii E, IV F, V G, vi A, vii B.
  // The vii is a diminished degree that single-finger snaps to MINOR.
  const Case cases[7] = {
      {60, 0, ChordQuality::kMaj},   // C  -> C  major
      {62, 2, ChordQuality::kMin},   // D  -> D  minor
      {64, 4, ChordQuality::kMin},   // E  -> E  minor
      {65, 5, ChordQuality::kMaj},   // F  -> F  major
      {67, 7, ChordQuality::kMaj},   // G  -> G  major
      {69, 9, ChordQuality::kMin},   // A  -> A  minor
      {71, 11, ChordQuality::kMin},  // B  -> B  minor  (vii dim snapped to minor)
  };
  for (const Case& tc : cases) {
    const ChordState s = single_press(b, tc.note);
    CHECK(s.valid);
    CHECK(s.root_pc == tc.root_pc);
    CHECK(s.quality == tc.quality);
  }
}

void test_single_finger_out_of_key_is_major_never_silent() {
  Band b;
  setup_single_finger_C_major(b);
  // Chromatic roots in C major: F# and Eb. Single-finger must resolve MAJOR,
  // never stall (the shortcut must not go silent mid-phrase).
  const ChordState fsharp = single_press(b, 66);  // F#
  CHECK(fsharp.valid);
  CHECK(fsharp.root_pc == 6);
  CHECK(fsharp.quality == ChordQuality::kMaj);
  const ChordState eflat = single_press(b, 63);  // Eb
  CHECK(eflat.valid);
  CHECK(eflat.root_pc == 3);
  CHECK(eflat.quality == ChordQuality::kMaj);
}

void test_single_finger_minor_key_aug_degree_is_major() {
  Band b;
  // A minor: the III degree (C, pc 0) sits over an augmented triad in the
  // harmonic-minor practice smart_quality uses; single-finger keeps the major
  // third and restores the fifth -> MAJOR. The i degree (A) stays MINOR.
  b.cmd(Param::kKeySet, 9, static_cast<std::int32_t>(Mode::kMinor), 0, Op::kSet);  // A minor
  b.cmd(Param::kChordMode, static_cast<std::int32_t>(ChordMode::kSingle), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  const ChordState three = single_press(b, 60);  // C = III of A minor
  CHECK(three.valid && three.root_pc == 0 && three.quality == ChordQuality::kMaj);
  const ChordState one = single_press(b, 69);  // A = i
  CHECK(one.valid && one.root_pc == 9 && one.quality == ChordQuality::kMin);
  const ChordState two = single_press(b, 71);  // B = ii dim -> minor
  CHECK(two.valid && two.root_pc == 11 && two.quality == ChordQuality::kMin);
}

// --- 3. Distinctness from diatonic mode -------------------------------------
// Three invariants, at the ChordEngine seam (a no-op schedule sink isolates the
// resolved ChordResult):
//   (a) single-finger ACCEPTS an out-of-key root (-> major triad); diatonic
//       REJECTS it (degree -1, nothing sounds);
//   (b) single-finger yields a plain TRIAD (3 tones) where diatonic adds D19
//       richness (a 7th, 4 tones);
//   (c) single-finger snaps a dim degree to a minor TRIAD where diatonic shows
//       the true half-diminished 7th.
void test_single_finger_distinct_from_diatonic() {
  const auto noop = [](std::uint8_t, const MidiMessage&) {};

  ChordEngine ce;
  ce.set_key(Key{.root_pc = 0, .mode = Mode::kMajor});  // C major

  // (a) chromatic root F# (66): single-finger accepts and majors it; diatonic
  //     rejects it (degree -1, no sound).
  const ChordResult sf_fsharp = ce.play_single(66, -1, 100, noop, false);
  CHECK(sf_fsharp.degree == static_cast<std::int8_t>(kNoDegree));
  CHECK(sf_fsharp.quality == ChordQuality::kMaj);
  CHECK(sf_fsharp.shape.count == 3);  // plain triad
  const ChordResult dia_fsharp = ce.play(66, -1, 100, noop, false);
  CHECK(dia_fsharp.degree == -1);  // D20-strict: chromatic root rejected

  // (b) ii degree D (62): single-finger -> plain Dm triad (3 tones); diatonic
  //     -> Dm7 richness (4 tones).
  const ChordResult sf_d = ce.play_single(62, -1, 100, noop, false);
  CHECK(sf_d.quality == ChordQuality::kMin);
  CHECK(sf_d.shape.count == 3);
  const ChordResult dia_d = ce.play(62, -1, 100, noop, false);
  CHECK(dia_d.quality == ChordQuality::kMin7);
  CHECK(dia_d.shape.count == 4);

  // (c) vii degree B (71): single-finger snaps the dim degree to a plain minor
  //     triad; diatonic shows the true half-diminished 7th.
  const ChordResult sf_b = ce.play_single(71, -1, 100, noop, false);
  CHECK(sf_b.quality == ChordQuality::kMin);
  CHECK(sf_b.shape.count == 3);
  const ChordResult dia_b = ce.play(71, -1, 100, noop, false);
  CHECK(dia_b.quality == ChordQuality::kHalfDim7);
  CHECK(dia_b.shape.count == 4);
}

}  // namespace

int main() {
  test_harmony_silent_steers_melody_routes();
  test_harmony_passes_non_note_traffic();
  test_default_zone_is_melody();
  test_single_finger_scale_aware_c_major();
  test_single_finger_out_of_key_is_major_never_silent();
  test_single_finger_minor_key_aug_degree_is_major();
  test_single_finger_distinct_from_diatonic();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_input_zone: all OK\n");
  }
  return arrangrr::test::failures();
}

// QA diagnosis (Torquato): two user-reported chord-context bugs while a style
// plays, reproduced at the Engine ABI level with full observability of the
// chord timeline.
//
//   (A) `current key` (Engine::chords().state(), the styles-panel "current
//       key:" readout) is reported to drift on its OWN while a style plays,
//       with zero key input. This suite drives 16+ idle bars under BOTH
//       ChordFollow::kAuto (the enum default) and ChordFollow::kDetect (the
//       actual live default set by ~/.arrangrr.init: `chord follow detect`)
//       and asserts the followed chord stays pinned at the song tonic. The
//       full per-bar timeline (root_pc/quality/pending/section) is printed
//       either way, so a break shows exactly where it moves.
//
//   (B) changing SECTION is reported to reset a chord the user just steered
//       (e.g. FMaj7) back to a section default. This suite steers FMaj7
//       through the live-detect quantized (D53) path, lets it commit at the
//       bar boundary, then separately exercises kStyleSection, kStyleSwitch,
//       and kStyleLoad, printing `current` before/after each so the printed
//       trace pinpoints exactly which switch path (if any) clears it.
//
// While isolating (B) an EMPIRICAL finding surfaced that is a strong
// candidate root cause for BOTH user reports: releasing an already-committed
// chord note-by-note (not simultaneously) re-triggers the live detector mid-
// release, stages a PHANTOM intermediate chord, and that phantom commits at
// the NEXT bar boundary with no new key ever pressed. See
// test_releasing_a_held_chord_note_by_note_stages_a_phantom_chord below.
//
// This file is a DIAGNOSIS pass: it does not fix anything. Any RED case here
// is a pinned defect handed to Nazzareno; do not silence it by loosening the
// assertion.

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 8192>;

constexpr std::uint8_t kBasicStyleIndex = 0;  // styles::kBuiltins[0] == "basic"
constexpr std::uint8_t kPopStyleIndex = 1;    // a second builtin, for kStyleSwitch

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           Boundary boundary = Boundary::kImmediate) {
    Command command{.op = op, .boundary = boundary, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds a NoteOn (vel>0) or NoteOff (vel==0) on the detect port (0).
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  const ChordState& followed() const { return e.chords().state(); }
  const ChordState& next() const { return e.chords().pending(); }
};

const char* quality_name(ChordQuality q) {
  switch (q) {
    case ChordQuality::kMaj:
      return "maj";
    case ChordQuality::kMin:
      return "min";
    case ChordQuality::kDim:
      return "dim";
    case ChordQuality::kAug:
      return "aug";
    case ChordQuality::kMaj7:
      return "maj7";
    case ChordQuality::kMin7:
      return "min7";
    case ChordQuality::kDom7:
      return "dom7";
    case ChordQuality::kHalfDim7:
      return "halfdim7";
    case ChordQuality::kDim7:
      return "dim7";
    default:
      return "?";
  }
}

void print_state(const char* label, const ChordState& s) {
  if (s.valid) {
    std::printf("%-40s root_pc=%2u quality=%-8s\n", label, s.root_pc, quality_name(s.quality));
  } else {
    std::printf("%-40s <invalid>\n", label);
  }
}

// Presses an F major 7 (F4 A4 C5 E5) on the detect port, root_pc=5.
void hold_F_maj7(Band& b) {
  b.key(65, 100);  // F4
  b.key(69, 100);  // A4
  b.key(72, 100);  // C5
  b.key(76, 100);  // E5
}

// ============================================================================
// (A) Idle drift: 16+ bars, ZERO chord input, style "basic" playing.
// ============================================================================

// Runs the idle-playback probe under a given ChordFollow value and asserts the
// followed chord never moves off the song tonic (C major). Prints the full
// per-bar timeline unconditionally (observability first).
void run_idle_drift_probe(const char* label, ChordFollow follow) {
  std::printf("\n==== (A) idle-drift probe: %s ====\n", label);

  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major tonic
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(follow), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detection ON, port 0 (matches the live default)
  b.cmd(Param::kTransportStart);

  constexpr std::uint8_t kTonicRootPc = 0;  // C
  constexpr ChordQuality kTonicQuality = ChordQuality::kMaj;

  constexpr int kBars = 20;
  bool drifted = false;
  {
    char label0[64];
    std::snprintf(label0, sizeof(label0), "bar %2d (start)", 0);
    print_state(label0, b.followed());
  }
  for (int bar = 1; bar <= kBars; ++bar) {
    b.advance(kTicksPerBar);
    char bar_label[96];
    std::snprintf(bar_label, sizeof(bar_label), "bar %2d  pending=%s  section=%d", bar,
                  b.next().valid ? "STAGED" : "-", static_cast<int>(b.e.arranger().current()));
    print_state(bar_label, b.followed());
    if (!b.followed().valid || b.followed().root_pc != kTonicRootPc ||
        b.followed().quality != kTonicQuality) {
      drifted = true;
    }
  }
  std::printf("---- verdict (%s): %s ----\n", label, drifted ? "DRIFTED (RED)" : "held (green)");

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kTonicRootPc);
  CHECK(b.followed().quality == kTonicQuality);
  CHECK(!drifted);  // must never have moved at ANY of the sampled bars
}

void test_idle_drift_under_follow_auto() {
  run_idle_drift_probe("follow=auto (ChordFollow enum default)", ChordFollow::kAuto);
}

void test_idle_drift_under_follow_detect() {
  run_idle_drift_probe("follow=detect (arrangrr.init.example live default)", ChordFollow::kDetect);
}

// ============================================================================
// (B) Section/style switch: does it reset a chord the user just steered?
// ============================================================================

// Steers FMaj7 through the live-detect QUANTIZED (D53) path and lets it commit
// at the next bar boundary; returns with `current == FMaj7` (asserted), keys
// STILL HELD (the switch-path probes below must not be confounded by a note
// release re-triggering the detector — see
// test_releasing_a_held_chord_note_by_note_stages_a_phantom_chord for that,
// deliberately isolated, effect).
void steer_to_F_maj7_and_commit(Band& b) {
  hold_F_maj7(b);
  CHECK(!b.next().valid);   // immediate model: nothing staged, it commits at once
  b.advance(kTicksPerBar);  // cross a bar boundary; the committed chord persists
  print_state("current after FMaj7 steer", b.followed());
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_style_section_change_persists_the_steered_chord() {
  std::printf("\n==== (B) kStyleSection: does a section change reset the chord? ====\n");
  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);

  steer_to_F_maj7_and_commit(b);
  print_state("current BEFORE kStyleSection", b.followed());

  // Quantized section switch (transport is playing -> lands on the next bar).
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.advance(kTicksPerBar);  // let the section switch land
  print_state("current AFTER kStyleSection -> varB", b.followed());

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_style_switch_persists_the_steered_chord() {
  std::printf(
      "\n==== (B) kStyleSwitch: does the interactive style/section chooser reset the "
      "chord? ====\n");
  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);

  steer_to_F_maj7_and_commit(b);
  print_state("current BEFORE kStyleSwitch", b.followed());

  // The interactive styles-panel chooser (shell_chooser.cpp) ALWAYS issues
  // kStyleSwitch, even for a pure section change (same style, new section).
  // boundary kNextBar: quantized, lands on the next bar like kStyleSection.
  b.cmd(Param::kStyleSwitch, kBasicStyleIndex, static_cast<std::int32_t>(SectionType::kVarC), 0,
        Op::kDo, Boundary::kNextBar);
  b.advance(kTicksPerBar);
  print_state("current AFTER kStyleSwitch (same style, varC)", b.followed());

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_style_load_resets_the_steered_chord_to_the_new_tonic() {
  std::printf("\n==== (B) kStyleLoad: does reloading a style reset the chord? ====\n");
  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);

  steer_to_F_maj7_and_commit(b);
  print_state("current BEFORE kStyleLoad", b.followed());

  b.cmd(Param::kStyleLoad, kPopStyleIndex);  // reload a (different) builtin style
  print_state("current AFTER kStyleLoad (pop)", b.followed());

  // D53 (engine.cpp, kStyleLoad case): a freshly loaded style calls
  // init_context_to_key() + reset_pending(), unconditionally re-seeding the
  // followed context to the NEW style's song tonic (still C major here, but
  // the chord the user had steered — FMaj7 — is gone). This is EXPECTED to be
  // RED against a "the chord should persist through it" assertion: it is the
  // by-design behavior that the user's report is colliding with.
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);  // FMaj7 root — WILL fail: reset to tonic (C, pc 0)
  CHECK(b.followed().quality ==
        ChordQuality::kMaj7);  // WILL fail: reset to the tonic triad quality
}

// ============================================================================
// EMPIRICAL FINDING: releasing an already-committed chord one note at a time
// re-triggers the detector mid-release, stages a PHANTOM intermediate chord,
// and that phantom commits at the NEXT bar boundary -- with the user never
// having pressed anything new. This surfaced while probing (B) above: the
// original steer_to_F_maj7_and_commit() released all four notes of FMaj7
// back-to-back right after commit, and by the time the switch-path probes
// looked at `current` a bar later, it had already become A minor (root_pc=9,
// quality=min) -- NOT the tonic, NOT FMaj7, and with NO section/style command
// involved yet at the point the phantom was staged.
//
// Root cause (engine.hpp, Engine::observe_chord_input): every NoteOn AND
// NoteOff on the detect port calls ChordDetector::recognize() unconditionally
// and, if it succeeds, ChordEngine::stage_context() unconditionally (subject
// only to the D47 follow gate). ChordDetector::recognize() only refuses to
// name a chord when held_count() < min_notes() (3, fingered default); it does
// NOT distinguish "the user is lifting fingers off a chord already handed
// off" from "the user is fingering a brand new chord". Releasing FMaj7
// (F A C E, 4 notes) one finger at a time passes through a 3-note plateau
// (A C E) that is >= min_notes(3), so it is happily recognized as a NEW valid
// chord (A minor) and staged -- even though the user's intent was only to let
// go of the SAME chord they already committed.
void test_releasing_a_held_chord_note_by_note_stages_a_phantom_chord() {
  std::printf(
      "\n==== EMPIRICAL: does releasing FMaj7 note-by-note stage a phantom chord? "
      "====\n");
  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);

  steer_to_F_maj7_and_commit(b);  // current := FMaj7 (root_pc=5), keys still held

  // Release the four notes ONE AT A TIME, exactly as a human lifting fingers
  // off a keyboard would -- no simultaneous release, no new key pressed.
  b.key(65, 0);  // release F: A C E remain held (3 notes, still >= min_notes)
  std::printf("held_count after releasing F = %u\n", b.e.chord_held_count());
  print_state("current right after releasing F (should be untouched)", b.followed());
  print_state("pending right after releasing F", b.next());

  b.key(69, 0);  // release A: C E remain (2 notes, below min_notes -> no recognition)
  b.key(72, 0);  // release C: E remains (1 note)
  b.key(76, 0);  // release E: 0 notes held

  print_state("current after releasing all four notes (no NEW chord pressed)", b.followed());
  print_state("pending after releasing all four notes", b.next());

  // The user pressed NOTHING new. `current` must still read FMaj7 -- the
  // chord they last committed -- while the notes are lifted.
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);                    // FMaj7 root: EXPECTED to still hold
  CHECK(b.followed().quality == ChordQuality::kMaj7);  // EXPECTED to still hold

  // Now advance to the next bar boundary with STILL no new key pressed.
  b.advance(kTicksPerBar);
  print_state("current one bar after the silent release (still no new input)", b.followed());

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);                    // FMaj7 root: WILL fail -- see printed trace
  CHECK(b.followed().quality == ChordQuality::kMaj7);  // WILL fail -- committed phantom instead
}

}  // namespace

int main() {
  test_idle_drift_under_follow_auto();
  test_idle_drift_under_follow_detect();
  test_style_section_change_persists_the_steered_chord();
  test_style_switch_persists_the_steered_chord();
  test_style_load_resets_the_steered_chord_to_the_new_tonic();
  test_releasing_a_held_chord_note_by_note_stages_a_phantom_chord();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_context_diagnosis: all OK\n");
  } else {
    std::printf("test_chord_context_diagnosis: %d FAILURE(S) -- see printed timelines above\n",
                arrangrr::test::failures());
  }
  return arrangrr::test::failures();
}

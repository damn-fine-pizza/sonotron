#include "style_lower.hpp"

#include <string>

#include "midisrc/diagnostics.hpp"
#include "model.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

bool any_diag_contains(const Diagnostics& diag, const std::string& needle) {
  for (const Diagnostic& d : diag.items()) {
    if (contains(d.message, needle)) {
      return true;
    }
  }
  return false;
}

PhraseEvent ev(std::uint32_t tick, std::uint8_t note, std::uint8_t vel = 100,
               std::uint32_t gate = 100) {
  return PhraseEvent{.tick = tick, .note = note, .velocity = vel, .gate_ticks = gate};
}

// A synthetic, hand-built "bossa"-flavored StyleModel (no corpus content —
// DESIGN.md §11: only hand-authored fixtures ever enter the repo). Exercises,
// in one section, every mechanical/STOP case the lowering pass documents.
StyleModel make_model() {
  StyleModel model;
  model.name = "synthetic bossa fixture";
  model.source_format = SourceFormat::kYamahaSff;
  model.source_ppqn = 960;  // matches the device's fixed PPQN: no off-grid noise here
  model.tempo_milli_bpm = 130000;
  model.time_sig_num = 4;
  model.time_sig_den = 4;

  // --- Section 1: main/a -> VarA (clean mapping). ---
  StyleSection main_a;
  main_a.kind = SectionKind::kMain;
  main_a.variation = SectionVariation::kA;
  main_a.bars = 1;

  // Drums: kFixed, literal MIDI notes copied straight through.
  PhraseLane drums;
  drums.role = Role::kDrums;
  drums.source_channel = 9;
  drums.transposition = TranspositionPolicy::kFixed;
  drums.events = {ev(0, 36, 110, 240), ev(480, 42, 90, 120)};
  main_a.lanes.push_back(drums);

  // Bass: kChordTone over a reference C maj7 (root_pc=0), anchor=36.
  // root(36+0)=36 -> tone0/oct0; fifth one octave up (36+7+12=55) -> tone2/oct1;
  // root one octave down (36-12=24) -> tone0/oct-1.
  PhraseLane bass;
  bass.role = Role::kBass;
  bass.source_channel = 10;
  bass.transposition = TranspositionPolicy::kChordTone;
  bass.source_root_pc = 0;
  bass.source_quality = ChordQuality::kMaj7;
  bass.events = {ev(0, 36), ev(240, 55), ev(480, 24, 100, 480)};
  main_a.lanes.push_back(bass);

  // Chord1: kChordTone over the same reference chord, anchor=60; includes one
  // event (note 62, a D/9th) that does NOT land on root/3rd/5th/7th of C maj7
  // (pcs {0,4,7,11}) — must be dropped, not guessed.
  PhraseLane chord1;
  chord1.role = Role::kChord1;
  chord1.source_channel = 1;
  chord1.transposition = TranspositionPolicy::kChordTone;
  chord1.source_root_pc = 0;
  chord1.source_quality = ChordQuality::kMaj7;
  chord1.events = {ev(0, 60), ev(240, 62), ev(480, 71)};
  main_a.lanes.push_back(chord1);

  // Pad: kChordTone but the source chord quality was never decoded (SFF2
  // Ctb2-partial style gap) — the whole lane cannot be reduced mechanically.
  PhraseLane pad;
  pad.role = Role::kPad;
  pad.source_channel = 14;
  pad.transposition = TranspositionPolicy::kChordTone;
  pad.source_root_pc = 0;
  pad.source_quality = ChordQuality::kUnknown;
  pad.events = {ev(0, 60)};
  main_a.lanes.push_back(pad);

  // Unassigned role: no device TrackRole destination at all.
  PhraseLane unassigned;
  unassigned.role = Role::kUnassigned;
  unassigned.source_channel = 7;
  unassigned.transposition = TranspositionPolicy::kFixed;
  unassigned.events = {ev(0, 50)};
  main_a.lanes.push_back(unassigned);

  model.sections.push_back(main_a);

  // --- Section 2: intro/c -> no device slot (only Intro1/Intro2 exist). ---
  StyleSection intro_c;
  intro_c.kind = SectionKind::kIntro;
  intro_c.variation = SectionVariation::kC;
  intro_c.bars = 1;
  PhraseLane intro_lane;
  intro_lane.role = Role::kDrums;
  intro_lane.transposition = TranspositionPolicy::kFixed;
  intro_lane.events = {ev(0, 36)};
  intro_c.lanes.push_back(intro_lane);
  model.sections.push_back(intro_c);

  // --- Section 3: a SECOND main/a -> the slot is already filled by Section 1. ---
  StyleSection main_a_dup;
  main_a_dup.kind = SectionKind::kMain;
  main_a_dup.variation = SectionVariation::kA;
  main_a_dup.bars = 1;
  PhraseLane dup_lane;
  dup_lane.role = Role::kDrums;
  dup_lane.transposition = TranspositionPolicy::kFixed;
  dup_lane.events = {ev(0, 38)};
  main_a_dup.lanes.push_back(dup_lane);
  model.sections.push_back(main_a_dup);

  return model;
}

void test_happy_path_and_stop_cases() {
  const StyleModel model = make_model();
  StyleLowerOptions opts;
  opts.style_name = "bossa_test";
  Diagnostics diag;
  std::string text;
  CHECK(lower_style(model, opts, text, diag));

  // Structural shape.
  CHECK(contains(text, "namespace bossa_test {"));
  CHECK(contains(text, "SectionType::kVarA"));
  CHECK(!contains(text, "SectionType::kIntro1"));  // never populated
  CHECK(!contains(text, "SectionType::kIntro2"));

  // Drums: fixed, literal notes, unchanged.
  CHECK(contains(text, "TrackRole::kDrums"));
  CHECK(contains(text, "RolePolicy::kFixed"));
  CHECK(contains(text, "{.step=0, .tone=36, .octave=0, .vel=110, .gate=240}"));
  CHECK(contains(text, "{.step=2, .tone=42, .octave=0, .vel=90, .gate=120}"));

  // Bass: chord-tone reduction against C maj7 (anchor 36).
  CHECK(contains(text, "TrackRole::kBass"));
  CHECK(contains(text, "RolePolicy::kChordTone"));
  CHECK(contains(text, "{.step=0, .tone=0, .octave=0"));   // root, register 0
  CHECK(contains(text, "{.step=1, .tone=2, .octave=1"));   // fifth, one octave up
  CHECK(contains(text, "{.step=2, .tone=0, .octave=-1"));  // root, one octave down

  // Chord1: the 9th (note 62) never appears as a lowered event; only 2 of its
  // 3 source events survive (root and the maj7 seventh).
  CHECK(contains(text, "TrackRole::kChord1"));
  CHECK(contains(text, "{.step=0, .tone=0, .octave=0"));
  CHECK(contains(text, "{.step=2, .tone=3, .octave=0"));  // seventh (index 3 of maj7 shape)

  // Dropped-field / STOP diagnostics, each reported rather than guessed.
  CHECK(any_diag_contains(diag, "no device SectionType slot"));           // intro/c
  CHECK(any_diag_contains(diag, "already populated from an earlier"));    // duplicate main/a
  CHECK(any_diag_contains(diag, "unassigned' has no device TrackRole"));  // unassigned role
  CHECK(any_diag_contains(diag, "no decoded reference chord"));           // pad, unknown quality
  CHECK(any_diag_contains(diag, "did not land on a root/3rd/5th/7th chord tone"));  // the 9th
  CHECK(any_diag_contains(diag, "gm_program/voicing"));  // always-unmapped fields
}

void test_deterministic() {
  const StyleModel model = make_model();
  StyleLowerOptions opts;
  opts.style_name = "bossa_test";
  Diagnostics d1;
  Diagnostics d2;
  std::string t1;
  std::string t2;
  CHECK(lower_style(model, opts, t1, d1));
  CHECK(lower_style(model, opts, t2, d2));
  CHECK(t1 == t2);
}

void test_off_grid_is_snapped_and_reported() {
  StyleModel model;
  model.name = "off-grid fixture";
  model.source_ppqn = 480;  // half the device's 960 PPQN
  model.tempo_milli_bpm = 120000;
  StyleSection sec;
  sec.kind = SectionKind::kMain;
  sec.variation = SectionVariation::kA;
  sec.bars = 1;
  PhraseLane lane;
  lane.role = Role::kDrums;
  lane.transposition = TranspositionPolicy::kFixed;
  lane.events = {ev(1, 36)};  // tick=1 at ppqn=480 never lands on a device step
  sec.lanes.push_back(lane);
  model.sections.push_back(sec);

  StyleLowerOptions opts;
  opts.style_name = "off_grid_test";
  Diagnostics diag;
  std::string text;
  CHECK(lower_style(model, opts, text, diag));
  CHECK(any_diag_contains(diag, "were not on the device's fixed 16th-grid"));
}

void test_invalid_style_name_is_rejected() {
  const StyleModel model = make_model();
  StyleLowerOptions opts;
  opts.style_name = "Not A Valid Identifier";
  Diagnostics diag;
  std::string text;
  CHECK(!lower_style(model, opts, text, diag));
  CHECK(diag.has_errors());
  CHECK(text.empty());
}

void test_nothing_lowerable_fails() {
  StyleModel model;
  model.name = "empty";
  StyleLowerOptions opts;
  opts.style_name = "empty_test";
  Diagnostics diag;
  std::string text;
  CHECK(!lower_style(model, opts, text, diag));
  CHECK(diag.has_errors());
  CHECK(any_diag_contains(diag, "no section could be lowered"));
}

}  // namespace

int main() {
  test_happy_path_and_stop_cases();
  test_deterministic();
  test_off_grid_is_snapped_and_reported();
  test_invalid_style_name_is_rejected();
  test_nothing_lowerable_fails();
  return arrstyle::test::failures();
}

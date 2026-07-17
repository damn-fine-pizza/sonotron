// Oracle-backed acceptance test for preview::preview_for()'s three confirmed
// fidelity gaps vs the real engine (owner task #4). Originally authored
// RED-before-green against the OLD one-bar/repeat=0/placeholder-harmony
// preview_for(); Nazzareno's SLICE 1 (multi-bar window + per-bar real
// default-progression harmony + repeat=bar motif approximation) has since
// landed in apps/gui-sonotron/src/preview.cpp, so this file is now the
// GREEN acceptance gate for that fix, kept as a standing regression test
// (not deleted -- an oracle diff like this is exactly the kind of coverage
// that should stay in the tree to catch a future re-regression).
//
// Ground truth comes from preview_oracle.hpp/.cpp (see its own header
// comment for the full two-halves design): the PURE half calls the SAME
// arrangrr:: kernel functions preview.cpp itself calls
// (Style::find/Arranger::resolve/motif::apply_repeat), so a match here is
// never a coincidence of a re-implementation agreeing with itself.
#include <set>

#include "preview_oracle.hpp"
#include "src/preview.hpp"
#include "test.hpp"

namespace {

using sonotron::preview::preview_for;
using sonotron::preview::Section;

// ---------------------------------------------------------------------------
// Bug #1 (multi-bar truncation, literal/non-motif pattern): basic (style 0),
// kVarA (bars=2), kBass (role 2, RolePolicy::kChordTone, no motif -- kVarABass
// in basic.hpp) -- isolates the window-widening fix from the harmony fix and
// the motif-repeat fix. Oracle ground truth (real_bar_onset_steps) reads the
// RAW authored StyleEvent span restricted to bar 1 (absolute steps [16,32)),
// independent of preview_for()'s own window; before SLICE 1 that content was
// unreachable (preview_for() only ever built a 16-step-wide PreviewPattern).
void test_multibar_literal_pattern_reaches_bar_two() {
  const int style = 0;  // basic
  const int var_a = static_cast<int>(Section::kVarA);
  const std::size_t bass = 2;

  CHECK(sonotron::preview::section_bars(style, Section::kVarA) == 2);

  const std::set<int> real_bar1_onsets =
      sonotron::preview_oracle::real_bar_onset_steps(style, var_a, bass, /*bar_index=*/1);
  CHECK(real_bar1_onsets.size() == 4);  // {0,4,8,12} relative to bar 1

  const sonotron::preview::PreviewPattern p = preview_for(style, Section::kVarA, bass);
  CHECK(p.bars == 2);
  for (int relative_step = 0; relative_step < sonotron::preview::kSteps; ++relative_step) {
    const bool oracle_has_onset = real_bar1_onsets.count(relative_step) > 0;
    const int absolute_step = sonotron::preview::kSteps + relative_step;
    const bool preview_has_onset =
        p.pitch[static_cast<std::size_t>(absolute_step)][0] >= 0;
    CHECK(preview_has_onset == oracle_has_onset);
  }
}

// ---------------------------------------------------------------------------
// Bug #3 (placeholder-harmony divergence): rock (style 2), kVarA, kBass,
// step 0. rock's own default progression (default_style_progressions.hpp
// entry 2) opens on an EXPLICIT-quality I chord (G major, root_pc=7) -- not
// "smart" quality -- so real_first_progression_chord_pitch() can resolve it
// directly. Before SLICE 1, preview_for() resolved step 0 against a hardcoded
// C-major placeholder (pitch 36, still what real_bar_pitch()'s OWN
// placeholder-based ground truth reports below); SLICE 1's chord_for_bar()
// now threads the style's REAL progression through instead, so preview_for()
// must now agree with real_first_progression_chord_pitch(), not with the old
// placeholder value.
void test_harmony_matches_real_default_progression() {
  const int rock = 2;
  const int var_a = static_cast<int>(Section::kVarA);
  const std::size_t bass = 2;

  const int old_placeholder_pitch =
      sonotron::preview_oracle::real_bar_pitch(rock, var_a, bass, /*bar_index=*/0, /*step_in_bar=*/0);
  const int real_progression_pitch =
      sonotron::preview_oracle::real_first_progression_chord_pitch(rock, var_a, bass, /*step=*/0);
  CHECK(old_placeholder_pitch == 36);       // the bug's OLD baseline (C-major placeholder)
  CHECK(real_progression_pitch == 43);      // G-major root: anchor(36) + root_pc(7)

  const sonotron::preview::PreviewPattern p = preview_for(rock, Section::kVarA, bass);
  CHECK(p.pitch[0][0] == real_progression_pitch);  // now matches the REAL chord...
  CHECK(p.pitch[0][0] != old_placeholder_pitch);   // ...not the old placeholder.
}

// ---------------------------------------------------------------------------
// Bug #2 (motif-repeat approximation), part A -- the underlying motif kernel
// itself: basic (style 0), kVarD (bars=1), kChord1 (role 3, kPeakChordMotif,
// MotifTransform::kRetrograde). Confirms arrangrr::motif::apply_repeat()
// genuinely produces a DIFFERENT onset set at repeat=1 than repeat=0 -- the
// ground truth SLICE 1's `repeat=bar` approximation depends on. kVarD is
// bars=1, so preview_for() itself never reaches repeat=1 for this specific
// section (there is no bar 2 to walk) -- that reachability is covered by the
// next test instead, using a style/section where a motif pattern DOES span
// two bars.
void test_motif_engine_repeat_transform_is_real() {
  const int style = 0;  // basic
  const int var_d = static_cast<int>(Section::kVarD);
  const std::size_t chord1 = 3;

  const std::set<int> repeat0 =
      sonotron::preview_oracle::real_motif_repeat_onset_steps(style, var_d, chord1, /*repeat=*/0);
  const std::set<int> repeat1 =
      sonotron::preview_oracle::real_motif_repeat_onset_steps(style, var_d, chord1, /*repeat=*/1);
  CHECK(repeat0.size() == 4);
  CHECK(repeat1.size() == 4);
  CHECK(repeat0 != repeat1);
  for (int step : repeat0) {
    CHECK(repeat1.count(step) == 0);  // fully disjoint under this retrograde transform
  }

  // preview_for() itself never shows repeat1's content here: kVarD is bars=1,
  // so its own bar-loop (resolve_motif_pattern) only ever calls
  // apply_repeat(seed, spec, /*bar=*/0). Documents the boundary, not a defect.
  const sonotron::preview::PreviewPattern p = preview_for(style, Section::kVarD, chord1);
  CHECK(p.bars == 1);
  for (int step : repeat1) {
    if (repeat0.count(step) == 0) {
      CHECK(p.pitch[static_cast<std::size_t>(step)][0] == -1);
    }
  }
}

// ---------------------------------------------------------------------------
// Bug #2, part B -- preview_for()'s OWN `repeat=bar` approximation IS
// reachable for a genuinely two-bar motif-driven pattern: pop (style 1),
// kVarA (bars=2), kChord2 (role 4, kSharedChord2Motif,
// MotifTransform::kDisplacement -- kChord2On in pop.hpp, authored onsets
// {0,8}). Before SLICE 1, preview_for() only ever called
// apply_repeat(seed, spec, /*repeat=*/0), so bar 2 (the transform) was never
// visible in the preview at all. SLICE 1 threads `bar` as the repeat index,
// so bar 2 now shows the displaced echo of the SAME seed.
//
// HONEST CAVEAT (not asserted here, just documented): pop.hpp's own header
// comment on kVarAPatterns records that the REAL Arranger stays SILENT on
// this role in bar 2 (a motif::from_span/generate/apply_repeat event can
// never carry step>=16 by construction, so at real playback time this
// motif-driven role simply never re-fires past bar 1) -- an engine-level
// constraint, not a data or preview bug. PreviewPattern::approx's own header
// comment documents this exact tradeoff as a DELIBERATE, owner-approved
// approximation ("make the answer-bar's transform visible within one static
// preview window, not a literal reproduction of one single real playback
// pass"). This test only proves the approximation is genuinely exercised
// (non-trivial, reachable, and distinct from bar 1) -- it does NOT claim bar
// 2's preview content matches what a real loop of this exact section would
// play, because for this specific role it provably does not.
void test_motif_repeat_approximation_reaches_bar_two() {
  const int pop = 1;
  const std::size_t chord2 = 4;

  const sonotron::preview::PreviewPattern p = preview_for(pop, Section::kVarA, chord2);
  CHECK(p.bars == 2);
  CHECK(p.approx == true);

  bool bar_two_has_onset = false;
  for (int step = sonotron::preview::kSteps; step < sonotron::preview::kMaxSteps; ++step) {
    if (p.pitch[static_cast<std::size_t>(step)][0] >= 0) {
      bar_two_has_onset = true;
    }
  }
  CHECK(bar_two_has_onset);  // SLICE 1 fix: bar 2 is no longer a silent void

  // Bar 2's onset positions (relative to its own bar start) must differ from
  // bar 1's -- a genuinely transformed echo, not a verbatim repeat.
  std::set<int> bar1_relative_onsets;
  std::set<int> bar2_relative_onsets;
  for (int relative_step = 0; relative_step < sonotron::preview::kSteps; ++relative_step) {
    if (p.pitch[static_cast<std::size_t>(relative_step)][0] >= 0) {
      bar1_relative_onsets.insert(relative_step);
    }
    const int absolute_step = sonotron::preview::kSteps + relative_step;
    if (p.pitch[static_cast<std::size_t>(absolute_step)][0] >= 0) {
      bar2_relative_onsets.insert(relative_step);
    }
  }
  CHECK(bar1_relative_onsets != bar2_relative_onsets);
}

}  // namespace

int main() {
  test_multibar_literal_pattern_reaches_bar_two();
  test_harmony_matches_real_default_progression();
  test_motif_engine_repeat_transform_is_real();
  test_motif_repeat_approximation_reaches_bar_two();
  return sonotron::test::failures();
}

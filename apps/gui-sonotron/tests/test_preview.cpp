// Unit tests for gui_sonotron_preview::preview_for (repeat-zone-real-
// contract.md, "cell preview made real" pass, plus the multi-bar/motif-
// answer/real-progression widening pass): pins that the preview draws REAL
// resolved note data (the arranger's own NTT kernel against this style's own
// real default harmonic progression), across every bar a section holds, not
// a label-hash, and that it is a pure/deterministic function of its
// arguments.

#include "src/preview.hpp"

#include "test.hpp"

#include <array>

using sonotron::preview::preview_for;
using sonotron::preview::PreviewPattern;
using sonotron::preview::Section;

namespace {

bool same_pattern(const PreviewPattern& a, const PreviewPattern& b) {
  if (a.approx != b.approx || a.bars != b.bars) {
    return false;
  }
  for (int i = 0; i < sonotron::preview::kMaxSteps; ++i) {
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      if (a.pitch[static_cast<std::size_t>(i)][static_cast<std::size_t>(v)] !=
          b.pitch[static_cast<std::size_t>(i)][static_cast<std::size_t>(v)]) {
        return false;
      }
    }
  }
  return true;
}

// Style 0 ("basic", components/core/arrangrr/include/arrangrr/arranger/
// styles/basic.hpp), section kVarA: repeated calls with the same arguments
// must yield byte-identical results (no seed, no randomness).
void test_determinism() {
  const PreviewPattern a = preview_for(0, Section::kVarA, /*role_index=*/2);
  const PreviewPattern b = preview_for(0, Section::kVarA, /*role_index=*/2);
  CHECK(same_pattern(a, b));
}

// kDrums (role_index 0) in basic/kVarA is RolePolicy::kFixed: kVarADrums
// mixes a kick/snare backbeat (steps 0,4,8,12; tones 36,38,36,38) with a
// closed-hat bed (steps 0,2,4,6,8,10,12,14; tone 42, GM closed hi-hat). The
// kick/snare events are authored FIRST in the array, so on the four steps
// where both collide (0,4,8,12) the kick/snare lands in voice slot 0 and the
// hat lands in slot 1 -- owner bug #13's exact repro: before the fix, a
// single `pitch[step]` int could only remember the LAST StyleEvent resolved
// per step, so the kick/snare backbeat silently vanished under the hat on
// every one of those four steps. This is the REAL NTT output (literal tones,
// kFixed never resolves against harmony), pinned by exact per-voice value.
//
// basic.hpp is bars=2 for kVarA (Wave-1 style-depth). kVarADrums' own bar-2
// content (basic.hpp lines 102-105) repeats the backbeat verbatim one bar
// later (steps 16,20,24,28) but the hat bed OPENS on the last 8th instead of
// closing (tone 46, GM open hi-hat, at step 30 -- a turnaround, no collision
// there since the backbeat has already finished by then).
void test_known_value_fixed_role_drums() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/0);
  CHECK(p.approx == false);  // kFixed: literal, never approximate
  CHECK(p.bars == 2);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
    int expected_slot0 = -1;
    int expected_slot1 = -1;
    if (step == 0 || step == 8) {
      expected_slot0 = 36;  // kick
      expected_slot1 = 42;  // hat, collides -> its own voice slot
    } else if (step == 4 || step == 12) {
      expected_slot0 = 38;  // snare
      expected_slot1 = 42;  // hat, collides -> its own voice slot
    } else if (step == 2 || step == 6 || step == 10 || step == 14) {
      expected_slot0 = 42;  // hat alone, no collision
    }
    CHECK(p.pitch[static_cast<std::size_t>(step)][0] == expected_slot0);
    CHECK(p.pitch[static_cast<std::size_t>(step)][1] == expected_slot1);
    // No built-in style reaches a third simultaneous voice on this pattern.
    CHECK(p.pitch[static_cast<std::size_t>(step)][2] == -1);
    CHECK(p.pitch[static_cast<std::size_t>(step)][3] == -1);
  }
  // Bar 2 (absolute steps 16..31): backbeat repeats verbatim; the hat bed
  // opens (tone 46) at step 30 instead of closing, and nothing sounds at
  // step 31 (basic.hpp's kVarADrums has no event past step 30).
  for (int step = sonotron::preview::kSteps; step < sonotron::preview::kMaxSteps; ++step) {
    int expected_slot0 = -1;
    int expected_slot1 = -1;
    if (step == 16 || step == 24) {
      expected_slot0 = 36;  // kick
      expected_slot1 = 42;  // hat, collides -> its own voice slot
    } else if (step == 20 || step == 28) {
      expected_slot0 = 38;  // snare
      expected_slot1 = 42;  // hat, collides -> its own voice slot
    } else if (step == 18 || step == 22 || step == 26) {
      expected_slot0 = 42;  // hat alone, no collision
    } else if (step == 30) {
      expected_slot0 = 46;  // open hat (turnaround), no collision here
    }
    CHECK(p.pitch[static_cast<std::size_t>(step)][0] == expected_slot0);
    CHECK(p.pitch[static_cast<std::size_t>(step)][1] == expected_slot1);
    CHECK(p.pitch[static_cast<std::size_t>(step)][2] == -1);
    CHECK(p.pitch[static_cast<std::size_t>(step)][3] == -1);
  }
}

// kBass (role_index 2) in basic/kVarA is RolePolicy::kChordTone: kVarABass's
// bar 1 (basic.hpp lines 108-111) is {step0=root, step4=fifth, step8=root,
// step12=fifth}. Resolved against bar 1's chord: style 0's own default
// progression (apps/gui-sonotron/src/default_style_progressions.hpp entry 0,
// "basic": I-IV-V-I, each 1 bar) opens on the I chord (C major) -- the exact
// same chord the OLD static placeholder used, so bar 1's numbers happen to
// stay identical to before this rewrite (a coincidence of style 0's own
// progression, not evidence the rewrite is a no-op -- see bar 2 below).
// anchor(kBass)=36 + chord_root_pc(0) + triad offset (kMaj shape {0,4,7}):
// root -> 36+0+0 = 36; fifth -> 36+0+7 = 43.
void test_known_value_resolved_role_bass() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/2);
  CHECK(p.approx == true);  // kChordTone: resolved against the style's progression
  CHECK(p.bars == 2);
  CHECK(p.pitch[0][0] == 36);
  CHECK(p.pitch[4][0] == 43);
  CHECK(p.pitch[8][0] == 36);
  CHECK(p.pitch[12][0] == 43);
  // Bar 2 (absolute steps 16/20/24/28, basic.hpp kVarABass bar-2 content,
  // lines 114-117): bar 2 (bar index 1) of the 4-step, 1-bar-per-step
  // progression falls on its SECOND step -- IV, F major, root_pc=5.
  // resolve()'s kChordTone formula: anchor(36) + chord_root_pc(5) + triad
  // offset (kMaj shape {0,4,7}, index = tone % 3) + 12*(octave + tone / 3):
  //   step16 tone=0 octave=0 (root)       -> 36+5+offsets[0]+12*(0+0)   = 41
  //   step20 tone=2 octave=0 (fifth)      -> 36+5+offsets[2]+12*(0+0)   = 48
  //   step24 tone=0 octave=1 (root, +8ve) -> 36+5+offsets[0]+12*(1+0)   = 53
  //   step28 tone=3 octave=0              -> 36+5+offsets[0]+12*(0+1)   = 53
  //     (tone=3 wraps: 3 % 3 == 0 -> root offset, 3 / 3 == 1 -> +1 octave;
  //     the authored intent was a "leading 7th", but a plain triad shape has
  //     no 4th chord tone to hold one, so it wraps back to the root an
  //     octave up -- the REAL resolve() output, not the intended label.)
  CHECK(p.pitch[16][0] == 41);
  CHECK(p.pitch[20][0] == 48);
  CHECK(p.pitch[24][0] == 53);
  CHECK(p.pitch[28][0] == 53);
  for (int step = 0; step < sonotron::preview::kMaxSteps; ++step) {
    // kVarABass never has two voices on the same step -- every other slot
    // (including slot 0 on a rest step) stays a rest.
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      const bool is_onset = (step == 0 || step == 4 || step == 8 || step == 12 || step == 16 ||
                             step == 20 || step == 24 || step == 28) &&
                            v == 0;
      if (is_onset) {
        continue;
      }
      CHECK(p.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] == -1);
    }
  }
}

// Owner tasks #2/#3, the whole point of the multi-bar window: bar 2 resolves
// against a genuinely DIFFERENT chord than bar 1 -- something a one-bar-only
// preview window could never even express. Bar 1's root pitch class is 0
// (C, from step 0's note 36 % 12); bar 2's is 5 (F, from step 16's note 41 %
// 12) -- a real harmonic change, not merely a different tone/octave choice
// against the same chord.
void test_bar_two_resolves_against_a_different_chord() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/2);
  CHECK(p.pitch[0][0] % 12 != p.pitch[16][0] % 12);
}

// A role with no StylePattern at all in this section (kPhrase, role_index
// 7 -- basic/kVarA only has drums/bass/chord1/pad/chord2) is an honestly
// EMPTY preview, not an error: every step rests, approx stays the default
// (false) -- but `bars` still reflects the section's own real length (2),
// since that width is set BEFORE the "no content for this role" check.
void test_role_absent_from_section_is_empty() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/7);
  CHECK(p.approx == false);
  CHECK(p.bars == 2);
  for (int step = 0; step < sonotron::preview::kMaxSteps; ++step) {
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      CHECK(p.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] == -1);
    }
  }
}

// SLICE 4a (docs/proposals/repeat-zone-real-contract.md): grid_panel.cpp now
// calls preview_for() with a scene COLUMN's own SectionType instead of a
// hardcoded kVarA, so two columns carrying different sections must actually
// preview differently. Style 0 ("basic")'s kBass (role_index 2) pattern
// differs between kVarA and kVarB by construction (components/core/arrangrr/
// include/arrangrr/arranger/styles/basic.hpp: kVarABass is a plain 4-note
// root/fifth pattern; kVarBBass is a busier 6-note pattern with different
// steps/tones) -- a real, content-level difference, not merely a different
// `approx` flag.
void test_scene_section_changes_the_preview() {
  const PreviewPattern var_a = preview_for(0, Section::kVarA, /*role_index=*/2);
  const PreviewPattern var_b = preview_for(0, Section::kVarB, /*role_index=*/2);
  CHECK(!same_pattern(var_a, var_b));
}

// Out-of-range arguments (a negative/too-large style index, or a role index
// past the core's own TrackRole vocabulary) return an honestly empty
// preview rather than asserting or reading out of bounds. `bars` stays the
// default (1) -- these return before `out.bars` is ever assigned.
void test_out_of_range_arguments_are_empty() {
  const PreviewPattern negative_style = preview_for(-1, Section::kVarA, 0);
  const PreviewPattern huge_style = preview_for(9999, Section::kVarA, 0);
  const PreviewPattern huge_role = preview_for(0, Section::kVarA, 9999);
  for (int step = 0; step < sonotron::preview::kMaxSteps; ++step) {
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      CHECK(negative_style.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] ==
            -1);
      CHECK(huge_style.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] == -1);
      CHECK(huge_role.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] == -1);
    }
  }
  CHECK(negative_style.approx == false);
  CHECK(huge_style.approx == false);
  CHECK(huge_role.approx == false);
  CHECK(negative_style.bars == 1);
  CHECK(huge_style.bars == 1);
  CHECK(huge_role.bars == 1);
}

// SLICE 4b (docs/proposals/repeat-zone-real-contract.md): section_bars reads
// arrangrr::StyleSection::bars for the auto-song advance decision. Style 0
// ("basic")'s kVarA is bars=2 since Wave-2 C gave Intro1/VarA a genuine
// two-bar build (components/core/arrangrr/include/arrangrr/arranger/styles/
// basic.hpp); kVarB stays bars=1 -- both known values, not merely "some
// positive number".
using sonotron::preview::section_bars;

void test_section_bars_known_value_basic_var_a() {
  CHECK(section_bars(0, Section::kVarA) == 2);
  CHECK(section_bars(0, Section::kVarB) == 1);
}

// Out-of-range style_index falls back to the honest "1 bar" default, never a
// crash and never a 0-or-negative value (which would make an "elapsed >=
// length" caller check trivially/permanently true).
void test_section_bars_out_of_range_style_falls_back_to_one() {
  CHECK(section_bars(-1, Section::kVarA) == 1);
  CHECK(section_bars(9999, Section::kVarA) == 1);
}

}  // namespace

int main() {
  test_determinism();
  test_known_value_fixed_role_drums();
  test_known_value_resolved_role_bass();
  test_bar_two_resolves_against_a_different_chord();
  test_role_absent_from_section_is_empty();
  test_scene_section_changes_the_preview();
  test_out_of_range_arguments_are_empty();
  test_section_bars_known_value_basic_var_a();
  test_section_bars_out_of_range_style_falls_back_to_one();
  return sonotron::test::failures();
}

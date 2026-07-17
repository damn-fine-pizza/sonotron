// Unit tests for gui_sonotron_preview::preview_for (repeat-zone-real-
// contract.md, "cell preview made real" pass): pins that the preview draws
// REAL resolved note data (the arranger's own NTT kernel against a
// placeholder harmony), not a label-hash, and that it is a pure/
// deterministic function of its arguments.

#include "src/preview.hpp"

#include "test.hpp"

#include <array>

using sonotron::preview::preview_for;
using sonotron::preview::PreviewPattern;
using sonotron::preview::Section;

namespace {

bool same_pattern(const PreviewPattern& a, const PreviewPattern& b) {
  if (a.approx != b.approx) {
    return false;
  }
  for (int i = 0; i < sonotron::preview::kSteps; ++i) {
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
void test_known_value_fixed_role_drums() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/0);
  CHECK(p.approx == false);  // kFixed: literal, never approximate
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
}

// kBass (role_index 2) in basic/kVarA is RolePolicy::kChordTone: kVarABass
// is {step0=root, step4=fifth, step8=root, step12=fifth}. Resolved against
// the canonical placeholder harmony (C major, tonic triad, anchor 36 for
// kBass): root -> 36 + 0 (chord root pc) + 0 (triad offset 0) = 36; fifth ->
// 36 + 0 + 7 (triad offset 2, kMaj shape {0,4,7}) = 43. These are the exact
// numbers arrangrr::Arranger::resolve() itself would produce -- the real
// NTT kernel, not a re-implementation and not a hash.
void test_known_value_resolved_role_bass() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/2);
  CHECK(p.approx == true);  // kChordTone: resolved against a placeholder chord
  CHECK(p.pitch[0][0] == 36);
  CHECK(p.pitch[4][0] == 43);
  CHECK(p.pitch[8][0] == 36);
  CHECK(p.pitch[12][0] == 43);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
    // kVarABass never has two voices on the same step -- every other slot
    // (including slot 0 on a rest step) stays a rest.
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      if ((step == 0 || step == 4 || step == 8 || step == 12) && v == 0) {
        continue;
      }
      CHECK(p.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] == -1);
    }
  }
}

// A role with no StylePattern at all in this section (kPhrase, role_index
// 7 -- basic/kVarA only has drums/bass/chord1/pad/chord2) is an honestly
// EMPTY preview, not an error: every step rests, approx stays the default
// (false).
void test_role_absent_from_section_is_empty() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/7);
  CHECK(p.approx == false);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
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
// preview rather than asserting or reading out of bounds.
void test_out_of_range_arguments_are_empty() {
  const PreviewPattern negative_style = preview_for(-1, Section::kVarA, 0);
  const PreviewPattern huge_style = preview_for(9999, Section::kVarA, 0);
  const PreviewPattern huge_role = preview_for(0, Section::kVarA, 9999);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
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
  test_role_absent_from_section_is_empty();
  test_scene_section_changes_the_preview();
  test_out_of_range_arguments_are_empty();
  test_section_bars_known_value_basic_var_a();
  test_section_bars_out_of_range_style_falls_back_to_one();
  return sonotron::test::failures();
}

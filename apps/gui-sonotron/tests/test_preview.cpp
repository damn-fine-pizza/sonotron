// Unit tests for gui_sonotron_preview::preview_for (repeat-zone-real-
// contract.md, "cell preview made real" pass): pins that the preview draws
// REAL resolved note data (the arranger's own NTT kernel against a
// placeholder harmony), not a label-hash, and that it is a pure/
// deterministic function of its arguments.

#include "src/preview.hpp"

#include "test.hpp"

using sonotron::preview::preview_for;
using sonotron::preview::PreviewPattern;
using sonotron::preview::Section;

namespace {

bool same_pattern(const PreviewPattern& a, const PreviewPattern& b) {
  if (a.approx != b.approx) {
    return false;
  }
  for (int i = 0; i < sonotron::preview::kSteps; ++i) {
    if (a.pitch[static_cast<std::size_t>(i)] != b.pitch[static_cast<std::size_t>(i)]) {
      return false;
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
// mixes a kick/snare backbeat with a closed-hat bed, both at steps
// {0,2,4,6,8,10,12,14}; the hat events are authored LAST in the array, so
// they are the ones that end up resolved at those shared steps (a real,
// literal MIDI note -- 42, GM closed hi-hat -- never a hash). This is the
// REAL NTT output, pinned by exact value, not merely "some pitch".
void test_known_value_fixed_role_drums() {
  const PreviewPattern p = preview_for(0, Section::kVarA, /*role_index=*/0);
  CHECK(p.approx == false);  // kFixed: literal, never approximate
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
    const int expected = (step % 2 == 0 && step <= 14) ? 42 : -1;
    CHECK(p.pitch[static_cast<std::size_t>(step)] == expected);
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
  CHECK(p.pitch[0] == 36);
  CHECK(p.pitch[4] == 43);
  CHECK(p.pitch[8] == 36);
  CHECK(p.pitch[12] == 43);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
    if (step == 0 || step == 4 || step == 8 || step == 12) {
      continue;
    }
    CHECK(p.pitch[static_cast<std::size_t>(step)] == -1);
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
    CHECK(p.pitch[static_cast<std::size_t>(step)] == -1);
  }
}

// Out-of-range arguments (a negative/too-large style index, or a role index
// past the core's own TrackRole vocabulary) return an honestly empty
// preview rather than asserting or reading out of bounds.
void test_out_of_range_arguments_are_empty() {
  const PreviewPattern negative_style = preview_for(-1, Section::kVarA, 0);
  const PreviewPattern huge_style = preview_for(9999, Section::kVarA, 0);
  const PreviewPattern huge_role = preview_for(0, Section::kVarA, 9999);
  for (int step = 0; step < sonotron::preview::kSteps; ++step) {
    CHECK(negative_style.pitch[static_cast<std::size_t>(step)] == -1);
    CHECK(huge_style.pitch[static_cast<std::size_t>(step)] == -1);
    CHECK(huge_role.pitch[static_cast<std::size_t>(step)] == -1);
  }
  CHECK(negative_style.approx == false);
  CHECK(huge_style.approx == false);
  CHECK(huge_role.approx == false);
}

}  // namespace

int main() {
  test_determinism();
  test_known_value_fixed_role_drums();
  test_known_value_resolved_role_bass();
  test_role_absent_from_section_is_empty();
  test_out_of_range_arguments_are_empty();
  return sonotron::test::failures();
}

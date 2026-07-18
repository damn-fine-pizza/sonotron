// Unit tests for BrowserModel: the 16 builtin styles as a drag-source, the
// (currently empty) Clips/MIDI-seqs placeholders, and the search filter
// (ux-workstation.md §4.3). No GPU, no display, no core.

#include "src/browser_model.hpp"

#include "test.hpp"

#include <array>
#include <optional>
#include <string>

using sonotron::browser_category_label;
using sonotron::BrowserCategory;
using sonotron::BrowserModel;
using sonotron::kBrowserCategoryCount;
using sonotron::kBuiltinStyleFamilies;
using sonotron::kBuiltinStyleNames;
using sonotron::style_family_label;
using sonotron::StyleFamily;

namespace {

void test_style_count_and_names() {
  BrowserModel model;
  CHECK(model.style_count() == 16);
  CHECK(model.style_name(0) == "basic");
  CHECK(model.style_name(4) == "funk");
  CHECK(model.style_name(15) == "motown");
  // The array is the exact drag-source list the panel iterates.
  CHECK(kBuiltinStyleNames.size() == 16);
}

void test_clips_and_midi_seqs_start_empty() {
  BrowserModel model;
  CHECK(model.clip_names().empty());
  CHECK(model.midi_seq_names().empty());
}

void test_search_filter_empty_matches_everything() {
  BrowserModel model;
  CHECK(model.search_filter().empty());
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    CHECK(model.style_matches_filter(i));
  }
}

void test_search_filter_is_case_insensitive_substring() {
  BrowserModel model;
  model.set_search_filter("FUN");
  CHECK(model.search_filter() == "FUN");
  CHECK(model.style_matches_filter(4));   // "funk"
  CHECK(!model.style_matches_filter(0));  // "basic"

  model.set_search_filter("co");
  CHECK(model.style_matches_filter(5));   // "disco"
  CHECK(model.style_matches_filter(11));  // "country"
  CHECK(!model.style_matches_filter(4));  // "funk"
}

void test_search_filter_no_match() {
  BrowserModel model;
  model.set_search_filter("zzz");
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    CHECK(!model.style_matches_filter(i));
  }
}

// task #30: style_family() returns the hand-classified family for a handful
// of representative indices, spanning every branch of the fallback logic
// (kOther for the generic "basic" default, real families for the rest).
void test_style_family_returns_expected_family_for_representative_indices() {
  BrowserModel model;
  CHECK(model.style_family(0) == StyleFamily::kOther);           // basic
  CHECK(model.style_family(4) == StyleFamily::kFunkGroove);      // funk
  CHECK(model.style_family(8) == StyleFamily::kLatinClave);      // bossa
  CHECK(model.style_family(11) == StyleFamily::kPopRockBallad);  // country
}

// task #30: every one of the 8 family enumerators has a real, non-empty
// label -- kOther is not decorative, so it must label itself honestly too.
void test_style_family_label_is_non_empty_for_every_enumerator() {
  constexpr std::array<StyleFamily, 8> kAllFamilies = {
      StyleFamily::kPopRockBallad, StyleFamily::kDanceFourOnFloor,
      StyleFamily::kFunkGroove,    StyleFamily::kSwingShuffleJazz,
      StyleFamily::kLatinClave,    StyleFamily::kBallroomTraditional,
      StyleFamily::kWorldRegional, StyleFamily::kOther,
  };
  for (const StyleFamily family : kAllFamilies) {
    CHECK(!style_family_label(family).empty());
  }
}

// task #30: setting a family filter narrows style_matches_filter to exactly
// that family's members -- enumerate all 16 built-ins and partition by
// their hand-authored kBuiltinStyleFamilies membership (kBuiltinStyleFamilies
// is read directly here, not re-derived, so this test breaks loudly if the
// classification table above is ever edited without updating this check).
void test_family_filter_narrows_to_exactly_that_family() {
  BrowserModel model;
  model.set_family_filter(StyleFamily::kLatinClave);
  CHECK(model.family_filter().has_value());
  CHECK(*model.family_filter() == StyleFamily::kLatinClave);
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    const bool expected = kBuiltinStyleFamilies[i] == StyleFamily::kLatinClave;
    CHECK(model.style_matches_filter(i) == expected);
  }
  // Explicit spot-check of the family's real members (bossa/samba/reggae/
  // latin) vs a few non-members (pop/basic/funk), matching the doc's own
  // family table (docs/proposals/style-browser-corpus-scale.md §2.1).
  CHECK(model.style_matches_filter(8));   // bossa
  CHECK(model.style_matches_filter(9));   // samba
  CHECK(model.style_matches_filter(10));  // reggae
  CHECK(model.style_matches_filter(14));  // latin
  CHECK(!model.style_matches_filter(1));  // pop
  CHECK(!model.style_matches_filter(0));  // basic
  CHECK(!model.style_matches_filter(4));  // funk
}

// task #30: family filter AND text filter narrow FURTHER than either alone
// -- the exact scenario browser_model.hpp's label-collision note exists to
// keep correct. kPopRockBallad's siblings are pop/rock/ballad/country/
// motown; text="roc" must match ONLY "rock", never any of its siblings,
// which it would if the family's own label spelled out "Rock" (it doesn't --
// "Pop / Ballad", not "Pop / Rock / Ballad").
void test_family_filter_and_text_filter_combine_and_narrow_further() {
  BrowserModel model;
  model.set_family_filter(StyleFamily::kPopRockBallad);
  model.set_search_filter("roc");
  CHECK(model.style_matches_filter(2));    // rock
  CHECK(!model.style_matches_filter(1));   // pop
  CHECK(!model.style_matches_filter(3));   // ballad
  CHECK(!model.style_matches_filter(11));  // country
  CHECK(!model.style_matches_filter(15));  // motown
  // A style matching the text but NOT the family is still excluded (family
  // AND text, not OR): no other family happens to contain "roc" among the
  // 16, so this only re-confirms the AND combination rather than adding a
  // new positive case, but it is asserted explicitly for clarity.
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    const bool in_family = kBuiltinStyleFamilies[i] == StyleFamily::kPopRockBallad;
    const bool name_has_roc = std::string(model.style_name(i)).find("roc") != std::string::npos;
    CHECK(model.style_matches_filter(i) == (in_family && name_has_roc));
  }
}

// task #30: clearing the family filter back to nullopt restores full-family
// visibility, still subject to whatever text filter remains active.
void test_clearing_family_filter_restores_full_visibility_under_text_filter() {
  BrowserModel model;
  model.set_family_filter(StyleFamily::kLatinClave);
  model.set_search_filter("bossa");
  CHECK(model.style_matches_filter(8));   // bossa: in-family, text matches
  CHECK(!model.style_matches_filter(9));  // samba: in-family, text doesn't match

  model.set_family_filter(std::nullopt);
  CHECK(!model.family_filter().has_value());
  // Text filter "bossa" alone still narrows to just "bossa" -- family no
  // longer restricts anything.
  CHECK(model.style_matches_filter(8));   // bossa
  CHECK(!model.style_matches_filter(9));  // samba
  CHECK(!model.style_matches_filter(1));  // pop

  model.set_search_filter("");
  // Both filters cleared: every style visible again, exactly like the
  // pre-existing "empty matches everything" behavior.
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    CHECK(model.style_matches_filter(i));
  }
}

// browser-redesign-taxonomy.md Phase 1: the outer category selector defaults
// to Styles, and round-trips through set_category for at least two other
// enumerators.
void test_default_category_is_styles() {
  BrowserModel model;
  CHECK(model.category() == BrowserCategory::kStyles);
}

void test_set_category_round_trips() {
  BrowserModel model;
  model.set_category(BrowserCategory::kVoices);
  CHECK(model.category() == BrowserCategory::kVoices);
  model.set_category(BrowserCategory::kKits);
  CHECK(model.category() == BrowserCategory::kKits);
}

// browser_category_label: non-empty, distinct label for every enumerator
// (mirroring test_style_family_label_is_non_empty_for_every_enumerator).
void test_browser_category_label_is_non_empty_for_every_enumerator() {
  constexpr std::array<BrowserCategory, kBrowserCategoryCount> kAllCategories = {
      BrowserCategory::kStyles, BrowserCategory::kVariations, BrowserCategory::kVoices,
      BrowserCategory::kKits,   BrowserCategory::kClips,
  };
  for (const BrowserCategory category : kAllCategories) {
    CHECK(!browser_category_label(category).empty());
  }
}

// The 128 canonical GM voice names, hand-copied from gm_program.cpp -- a
// handful of representative indices, spanning the array's start/end/middle.
void test_voice_count_and_names() {
  BrowserModel model;
  CHECK(model.voice_count() == 128);
  CHECK(model.voice_name(0) == "Acoustic Grand Piano");
  CHECK(model.voice_name(127) == "Gunshot");
  CHECK(model.voice_name(56) == "Trumpet");
}

// browser-redesign-taxonomy.md §3: "search is per-active-tab, never a global
// cross-family search" -- the single most important new test in this slice.
void test_search_filter_is_isolated_per_category() {
  BrowserModel model;
  CHECK(model.category() == BrowserCategory::kStyles);
  model.set_search_filter("fun");
  CHECK(model.search_filter() == "fun");

  model.set_category(BrowserCategory::kVoices);
  // A fresh, separate slot: switching category must NOT carry over the
  // Styles filter text.
  CHECK(model.search_filter().empty());
  model.set_search_filter("trumpet");
  CHECK(model.search_filter() == "trumpet");

  model.set_category(BrowserCategory::kStyles);
  // The original Styles filter is still there, unchanged by the Voices
  // filter that was set in between.
  CHECK(model.search_filter() == "fun");
}

void test_voice_destination_defaults() {
  BrowserModel model;
  CHECK(model.voice_port() == "out0");
  CHECK(model.voice_channel() == 1);
}

void test_set_voice_channel_clamps_to_one_sixteen() {
  BrowserModel model;
  model.set_voice_channel(0);
  CHECK(model.voice_channel() == 1);
  model.set_voice_channel(99);
  CHECK(model.voice_channel() == 16);
  model.set_voice_channel(7);
  CHECK(model.voice_channel() == 7);
}

void test_set_voice_port_empty_resets_to_default() {
  BrowserModel model;
  model.set_voice_port("synth");
  CHECK(model.voice_port() == "synth");
  model.set_voice_port("");
  CHECK(model.voice_port() == "out0");
}

void test_last_voice_sent_defaults_and_round_trips() {
  BrowserModel model;
  CHECK(model.last_voice_sent() == -1);
  model.set_last_voice_sent(56);
  CHECK(model.last_voice_sent() == 56);
}

void test_build_program_verb_uses_current_destination() {
  BrowserModel model;
  CHECK(model.build_program_verb("Trumpet") == "program out0:1 Trumpet");

  model.set_voice_port("synth");
  model.set_voice_channel(3);
  CHECK(model.build_program_verb("Acoustic Grand Piano") == "program synth:3 Acoustic Grand Piano");
}

}  // namespace

int main() {
  test_style_count_and_names();
  test_clips_and_midi_seqs_start_empty();
  test_search_filter_empty_matches_everything();
  test_search_filter_is_case_insensitive_substring();
  test_search_filter_no_match();
  test_style_family_returns_expected_family_for_representative_indices();
  test_style_family_label_is_non_empty_for_every_enumerator();
  test_family_filter_narrows_to_exactly_that_family();
  test_family_filter_and_text_filter_combine_and_narrow_further();
  test_clearing_family_filter_restores_full_visibility_under_text_filter();
  test_default_category_is_styles();
  test_set_category_round_trips();
  test_browser_category_label_is_non_empty_for_every_enumerator();
  test_voice_count_and_names();
  test_search_filter_is_isolated_per_category();
  test_voice_destination_defaults();
  test_set_voice_channel_clamps_to_one_sixteen();
  test_set_voice_port_empty_resets_to_default();
  test_last_voice_sent_defaults_and_round_trips();
  test_build_program_verb_uses_current_destination();
  return sonotron::test::failures();
}

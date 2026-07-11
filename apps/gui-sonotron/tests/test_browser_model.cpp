// Unit tests for BrowserModel: the 16 builtin styles as a drag-source, the
// (currently empty) Clips/MIDI-seqs placeholders, and the search filter
// (ux-workstation.md §4.3). No GPU, no display, no core.

#include "src/browser_model.hpp"

#include "test.hpp"

using sonotron::BrowserModel;
using sonotron::kBuiltinStyleNames;

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

}  // namespace

int main() {
  test_style_count_and_names();
  test_clips_and_midi_seqs_start_empty();
  test_search_filter_empty_matches_everything();
  test_search_filter_is_case_insensitive_substring();
  test_search_filter_no_match();
  return sonotron::test::failures();
}

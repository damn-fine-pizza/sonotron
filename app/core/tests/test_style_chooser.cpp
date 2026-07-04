#include "style_chooser.hpp"

#include <string>
#include <vector>

#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

std::vector<StyleInfo> make_styles() {
  // Indices chosen to exercise substring matching: "13" hits 13, 113, 130, 213;
  // "130" hits only 130.
  return {
      {.index = 13, .name = "alpha", .sections = {SectionType::kVarA, SectionType::kVarB}},
      {.index = 113, .name = "bravo", .sections = {SectionType::kVarA}},
      {.index = 130, .name = "charlie", .sections = {SectionType::kVarA, SectionType::kFillA}},
      {.index = 213, .name = "delta", .sections = {SectionType::kIntro1, SectionType::kVarA}},
  };
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

void test_filter_substring_matching() {
  StyleChooser c{make_styles()};
  CHECK(c.filtered().size() == 4);  // empty filter = all

  c.feed_digit('1');
  c.feed_digit('3');  // "13" is a substring of 13, 113, 130 AND 213
  CHECK(c.filter() == "13");
  const auto f = c.filtered();
  CHECK(f.size() == 4);
  CHECK(f[0].index == 13 && f[1].index == 113 && f[2].index == 130 && f[3].index == 213);

  c.feed_digit('0');  // "130" matches only 130
  const auto g = c.filtered();
  CHECK(g.size() == 1 && g[0].index == 130);

  c.backspace();  // back to "13"
  CHECK(c.filter() == "13" && c.filtered().size() == 4);
}

void test_non_digit_ignored() {
  StyleChooser c{make_styles()};
  c.feed_digit('x');
  c.feed_digit('-');
  CHECK(c.filter().empty());
}

void test_selected_null_when_no_match() {
  StyleChooser c{make_styles()};
  c.feed_digit('9');  // matches nothing
  CHECK(c.filtered().empty());
  CHECK(c.selected_style() == nullptr);
  CHECK(c.selected_section() == SectionType::kVarA);  // safe default
}

void test_filter_resets_style_highlight() {
  StyleChooser c{make_styles()};
  c.nav_style(2);  // move down into the full list
  CHECK(c.selected_style()->index == 130);
  c.feed_digit('2');  // "2" -> only 213; highlight resets to first match
  CHECK(c.selected_style()->index == 213);
}

void test_nav_style_clamps_and_resets_section() {
  StyleChooser c{make_styles()};
  // Start on index 13 (2 sections), move section to the second one.
  c.nav_section(1);
  CHECK(c.selected_section() == SectionType::kVarB);

  // Moving style must reset the section highlight to the new style's first.
  c.nav_style(1);
  CHECK(c.selected_style()->index == 113);
  CHECK(c.selected_section() == SectionType::kVarA);

  // Clamp at the top: repeated up-moves rest on the first style.
  c.nav_style(-10);
  CHECK(c.selected_style()->index == 13);
  // Clamp at the bottom: repeated down-moves rest on the last style.
  c.nav_style(100);
  CHECK(c.selected_style()->index == 213);
}

void test_nav_section_clamps() {
  StyleChooser c{make_styles()};  // index 13 has {VarA, VarB}
  c.nav_section(-5);
  CHECK(c.selected_section() == SectionType::kVarA);  // clamp low
  c.nav_section(9);
  CHECK(c.selected_section() == SectionType::kVarB);  // clamp high (2 sections)
}

void test_render_contents() {
  StyleChooser c{make_styles()};
  c.feed_digit('1');
  c.feed_digit('3');
  const auto lines = c.render(NoteNaming::kCde);
  CHECK(lines.size() == 3);
  // Style line carries the filter and the selected marker.
  CHECK(contains(lines[0], "13"));
  CHECK(contains(lines[0], ">"));
  CHECK(contains(lines[0], "alpha"));
  // Section line names the selected style's sections with a marker.
  CHECK(contains(lines[1], "VarA") && contains(lines[1], ">"));
  // Hint line.
  CHECK(contains(lines[2], "ENTER next-bar"));
  CHECK(contains(lines[2], "CTRL+\\ now"));
  CHECK(contains(lines[2], "left/right section"));
}

void test_render_no_match() {
  StyleChooser c{make_styles()};
  c.feed_digit('9');
  const auto lines = c.render(NoteNaming::kCde);
  CHECK(lines.size() == 3);
  CHECK(contains(lines[0], "(no match)"));
  CHECK(contains(lines[1], "(none)"));
}

}  // namespace

int main() {
  test_filter_substring_matching();
  test_non_digit_ignored();
  test_selected_null_when_no_match();
  test_filter_resets_style_highlight();
  test_nav_style_clamps_and_resets_section();
  test_nav_section_clamps();
  test_render_contents();
  test_render_no_match();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_style_chooser: all OK\n");
  }
  return arrangrr::test::failures();
}

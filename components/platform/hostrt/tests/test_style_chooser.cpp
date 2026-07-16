#include "style_chooser.hpp"

#include <string>
#include <vector>

#include "test.hpp"

namespace {

using namespace arrangrr::host;

std::vector<StyleInfo> make_styles() {
  // Indices chosen to exercise substring matching: "13" hits 13, 113, 130, 213;
  // "130" hits only 130.
  return {
      {.index = 13, .name = "alpha", .sections = {SectionKind::kVarA, SectionKind::kVarB}},
      {.index = 113, .name = "bravo", .sections = {SectionKind::kVarA}},
      {.index = 130, .name = "charlie", .sections = {SectionKind::kVarA, SectionKind::kFillA}},
      {.index = 213, .name = "delta", .sections = {SectionKind::kIntro1, SectionKind::kVarA}},
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
  CHECK(c.selected_section() == SectionKind::kVarA);  // safe default
}

void test_filter_resets_style_highlight() {
  StyleChooser c{make_styles()};
  c.nav_style(2);  // move down into the full list
  CHECK(c.selected_style()->index == 130);
  c.feed_digit('2');  // "2" -> only 213; highlight resets to first match
  CHECK(c.selected_style()->index == 213);
}

void test_nav_style_preserves_section_by_type() {
  StyleChooser c{make_styles()};  // 13{VarA,VarB} 113{VarA} 130{VarA,FillA} 213{Intro1,VarA}
  // On 13, VarA sits at position 0.
  CHECK(c.selected_section() == SectionKind::kVarA);
  // Jump to 213, where VarA sits at position 1: the section TYPE is preserved
  // (not the numeric index) — the same variation stays highlighted.
  c.nav_style(3);
  CHECK(c.selected_style()->index == 213);
  CHECK(c.selected_section() == SectionKind::kVarA);

  // Select VarB on 13, then move to 113 which lacks VarB -> clamps into its list.
  StyleChooser d{make_styles()};
  d.nav_section(1);
  CHECK(d.selected_section() == SectionKind::kVarB);
  d.nav_style(1);  // 113 has only VarA
  CHECK(d.selected_style()->index == 113);
  CHECK(d.selected_section() == SectionKind::kVarA);

  // Clamp at the ends: repeated up/down rest on the first/last style.
  d.nav_style(-10);
  CHECK(d.selected_style()->index == 13);
  d.nav_style(100);
  CHECK(d.selected_style()->index == 213);
}

void test_select_absolute() {
  StyleChooser c{make_styles()};
  // Absolute placement by (style index, section type).
  c.select(130, SectionKind::kFillA);
  CHECK(c.selected_style()->index == 130);
  CHECK(c.selected_section() == SectionKind::kFillA);
  // A section absent from the target style clamps to that style's first.
  c.select(113, SectionKind::kFillA);
  CHECK(c.selected_style()->index == 113);
  CHECK(c.selected_section() == SectionKind::kVarA);
}

void test_nav_section_clamps() {
  StyleChooser c{make_styles()};  // index 13 has {VarA, VarB}
  c.nav_section(-5);
  CHECK(c.selected_section() == SectionKind::kVarA);  // clamp low
  c.nav_section(9);
  CHECK(c.selected_section() == SectionKind::kVarB);  // clamp high (2 sections)
}

void test_render_contents() {
  StyleChooser c{make_styles()};
  c.feed_digit('1');
  c.feed_digit('3');
  const auto lines = c.render(NoteNaming::kCde, UiStyle{});
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
  const auto lines = c.render(NoteNaming::kCde, UiStyle{});
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
  test_nav_style_preserves_section_by_type();
  test_select_absolute();
  test_nav_section_clamps();
  test_render_contents();
  test_render_no_match();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_style_chooser: all OK\n");
  }
  return arrangrr::test::failures();
}

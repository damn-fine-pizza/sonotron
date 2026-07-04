// Host-only ~/.arrangrr.rc parser tests: layout, ordered panel list, declared
// heights, full-row (*), empty spacer, unknown-name warnings, missing file.

#include <sstream>
#include <string>

#include "rc_config.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

RcConfig parse(const std::string& text) {
  std::istringstream in(text);
  return parse_rc(in);
}

void test_full_config() {
  const RcConfig rc = parse(
      "# my layout\n"
      "layout = 2\n"
      "[panels]\n"
      "piano\n"
      "console = 5\n"
      "styles\n"
      "*events\n"
      "empty\n");

  CHECK(rc.has_layout);
  CHECK(rc.per_row == 2);
  CHECK(rc.warnings.empty());
  CHECK(rc.order.size() == 5);

  // Bottom-to-top, as written.
  CHECK(rc.order[0].id == PanelId::kPiano);
  CHECK(rc.order[1].id == PanelId::kConsole);
  CHECK(rc.order[1].height == 5);  // declared height
  CHECK(rc.order[2].id == PanelId::kStyles);
  CHECK(rc.order[3].id == PanelId::kEvents);
  CHECK(rc.order[3].full_row);  // '*' marks a full-row panel
  CHECK(rc.order[4].id == PanelId::kEmpty);
  CHECK(rc.order[0].height == 0);  // no declared height -> default
  CHECK(!rc.order[0].full_row);
}

void test_unknown_name_warns_and_skips() {
  const RcConfig rc = parse(
      "[panels]\n"
      "piano\n"
      "wobble\n"
      "styles\n");
  CHECK(rc.order.size() == 2);  // wobble dropped
  CHECK(rc.order[0].id == PanelId::kPiano);
  CHECK(rc.order[1].id == PanelId::kStyles);
  CHECK(rc.warnings.size() == 1);
  CHECK(rc.warnings[0].find("wobble") != std::string::npos);
}

void test_help_alias_and_menu() {
  const RcConfig rc = parse(
      "[panels]\n"
      "help\n"
      "menu\n");
  CHECK(rc.order.size() == 2);
  CHECK(rc.order[0].id == PanelId::kHelp);  // "help" is a menu alias
  CHECK(rc.order[1].id == PanelId::kHelp);
}

void test_bad_values_warn() {
  const RcConfig rc = parse(
      "layout = 9\n"
      "[panels]\n"
      "console = wat\n");
  CHECK(!rc.has_layout);  // 9 is neither 1 nor 2
  CHECK(rc.order.size() == 1);
  CHECK(rc.order[0].id == PanelId::kConsole);
  CHECK(rc.order[0].height == 0);  // bad height ignored -> default
  bool warned_layout = false;
  bool warned_height = false;
  for (const std::string& w : rc.warnings) {
    warned_layout = warned_layout || w.find("layout") != std::string::npos;
    warned_height = warned_height || w.find("height") != std::string::npos;
  }
  CHECK(warned_layout);
  CHECK(warned_height);
}

void test_missing_file_defaults() {
  const RcConfig rc = load_rc("/nonexistent/path/.arrangrr.rc.absent");
  CHECK(!rc.has_layout);
  CHECK(rc.per_row == 1);
  CHECK(rc.order.empty());
  CHECK(rc.warnings.empty());
}

void test_empty_and_comments_only() {
  const RcConfig rc = parse("\n# just a comment\n   \n");
  CHECK(!rc.has_layout);
  CHECK(rc.order.empty());
  CHECK(rc.warnings.empty());
}

}  // namespace

int main() {
  test_full_config();
  test_unknown_name_warns_and_skips();
  test_help_alias_and_menu();
  test_bad_values_warn();
  test_missing_file_defaults();
  test_empty_and_comments_only();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_rc_config: all OK\n");
  }
  return arrangrr::test::failures();
}

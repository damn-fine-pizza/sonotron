// Unit tests for the layout JSON reader/writer (text <-> Layout). No file
// I/O here — that is exercised by test_layout_roundtrip.cpp.

#include "src/layout_json.hpp"

#include "test.hpp"

namespace {

void test_write_then_parse_round_trips_default_layout() {
  const sonotron::Layout original = sonotron::default_layout();
  const std::string text = sonotron::write_layout(original);

  sonotron::Layout reparsed;
  std::string error;
  const bool ok = sonotron::parse_layout(text, reparsed, error);
  CHECK(ok);
  CHECK(error.empty());
  CHECK(original == reparsed);
}

void test_parse_applies_defaults_for_omitted_optional_fields() {
  const std::string text = R"({
    "window": "sonotron",
    "zones": [
      { "id": "solo", "title": "Solo Zone", "row": 0 }
    ]
  })";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(ok);
  CHECK(layout.zones.size() == 1);
  CHECK(layout.zones[0].id == "solo");
  CHECK(layout.zones[0].col == 0);
  CHECK(!layout.zones[0].full_span);
  CHECK(!layout.zones[0].width_weight.has_value());
  CHECK(!layout.zones[0].height_weight.has_value());
}

void test_parse_recognizes_full_span() {
  const std::string text = R"({
    "window": "sonotron",
    "zones": [
      { "id": "top", "title": "Top", "row": 0, "span": "full", "h": 0.1 }
    ]
  })";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(ok);
  CHECK(layout.zones[0].full_span);
  CHECK(layout.zones[0].height_weight.has_value());
  CHECK(layout.zones[0].height_weight.value() > 0.09F &&
        layout.zones[0].height_weight.value() < 0.11F);
}

void test_parse_reads_custom_font_size() {
  const std::string text = R"({
    "window": "sonotron",
    "font_size": 18,
    "zones": []
  })";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(ok);
  CHECK(layout.font_size_px > 17.9F && layout.font_size_px < 18.1F);
}

void test_parse_defaults_font_size_when_absent() {
  const std::string text = R"({
    "window": "sonotron",
    "zones": []
  })";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(ok);
  CHECK(layout.font_size_px > sonotron::kDefaultFontSizePx - 0.01F &&
        layout.font_size_px < sonotron::kDefaultFontSizePx + 0.01F);
}

void test_parse_falls_back_to_default_font_size_when_out_of_range() {
  const std::string text = R"({
    "window": "sonotron",
    "font_size": 0,
    "zones": []
  })";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(ok);  // an out-of-range value degrades gracefully, it does not fail the whole file
  CHECK(layout.font_size_px > sonotron::kDefaultFontSizePx - 0.01F &&
        layout.font_size_px < sonotron::kDefaultFontSizePx + 0.01F);
}

void test_write_then_parse_round_trips_custom_font_size() {
  sonotron::Layout original = sonotron::default_layout();
  original.font_size_px = 20.0F;
  const std::string text = sonotron::write_layout(original);

  sonotron::Layout reparsed;
  std::string error;
  const bool ok = sonotron::parse_layout(text, reparsed, error);
  CHECK(ok);
  CHECK(original == reparsed);
  CHECK(reparsed.font_size_px > 19.9F && reparsed.font_size_px < 20.1F);
}

void test_parse_rejects_malformed_json() {
  const std::string text = R"({ "window": "sonotron", "zones": [ )";  // truncated, no closing

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(!ok);
  CHECK(!error.empty());
}

void test_parse_rejects_missing_top_level_brace() {
  const std::string text = R"("not an object")";

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::parse_layout(text, layout, error);
  CHECK(!ok);
  CHECK(!error.empty());
}

}  // namespace

int main() {
  test_write_then_parse_round_trips_default_layout();
  test_parse_applies_defaults_for_omitted_optional_fields();
  test_parse_recognizes_full_span();
  test_parse_reads_custom_font_size();
  test_parse_defaults_font_size_when_absent();
  test_parse_falls_back_to_default_font_size_when_out_of_range();
  test_write_then_parse_round_trips_custom_font_size();
  test_parse_rejects_malformed_json();
  test_parse_rejects_missing_top_level_brace();
  return sonotron::test::failures();
}

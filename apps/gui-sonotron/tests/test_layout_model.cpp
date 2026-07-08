// Unit tests for the pure-data layout model: default_layout() and
// compute_rows(). No JSON, no ImGui, no I/O — pure computation.

#include "src/layout_model.hpp"

#include <cmath>
#include <limits>

#include "test.hpp"

namespace {

bool approx(float a, float b, float epsilon = 1e-4F) { return std::fabs(a - b) < epsilon; }

void test_default_layout_shape() {
  const sonotron::Layout layout = sonotron::default_layout();
  CHECK(layout.window_title == "sonotron");
  CHECK(approx(layout.font_size_px, sonotron::kDefaultFontSizePx));
  CHECK(layout.zones.size() == 5);

  CHECK(layout.zones[0].id == "transport");
  CHECK(layout.zones[0].row == 0);
  CHECK(layout.zones[0].full_span);

  CHECK(layout.zones[1].id == "arrangement");
  CHECK(layout.zones[1].row == 1);
  CHECK(layout.zones[1].col == 0);

  CHECK(layout.zones[2].id == "intention");
  CHECK(layout.zones[2].row == 1);
  CHECK(layout.zones[2].col == 1);

  CHECK(layout.zones[3].id == "harmony");
  CHECK(layout.zones[3].row == 2);
  CHECK(layout.zones[3].col == 0);

  CHECK(layout.zones[4].id == "structure");
  CHECK(layout.zones[4].row == 2);
  CHECK(layout.zones[4].col == 1);
}

void test_layout_equality() {
  const sonotron::Layout a = sonotron::default_layout();
  const sonotron::Layout b = sonotron::default_layout();
  CHECK(a == b);

  sonotron::Layout c = sonotron::default_layout();
  c.zones[0].title = "changed";
  CHECK(!(a == c));

  sonotron::Layout d = sonotron::default_layout();
  d.font_size_px = 20.0F;
  CHECK(!(a == d));
}

void test_is_valid_font_size_px_bounds() {
  CHECK(sonotron::is_valid_font_size_px(sonotron::kDefaultFontSizePx));
  CHECK(sonotron::is_valid_font_size_px(sonotron::kMinFontSizePx));
  CHECK(sonotron::is_valid_font_size_px(sonotron::kMaxFontSizePx));
  CHECK(!sonotron::is_valid_font_size_px(0.0F));
  CHECK(!sonotron::is_valid_font_size_px(-5.0F));
  CHECK(!sonotron::is_valid_font_size_px(sonotron::kMinFontSizePx - 0.1F));
  CHECK(!sonotron::is_valid_font_size_px(sonotron::kMaxFontSizePx + 0.1F));
  CHECK(!sonotron::is_valid_font_size_px(std::numeric_limits<float>::quiet_NaN()));
  CHECK(!sonotron::is_valid_font_size_px(std::numeric_limits<float>::infinity()));
}

void test_compute_rows_default_layout() {
  const sonotron::Layout layout = sonotron::default_layout();
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);

  CHECK(rows.size() == 3);

  // Row 0 (transport): a single, full-span zone.
  CHECK(rows[0].cells.size() == 1);
  CHECK(approx(rows[0].cells[0].width_fraction, 1.0F));
  CHECK(layout.zones[rows[0].cells[0].zone_index].id == "transport");

  // Row 1 (arrangement | intention): the wide central surface beside the
  // narrower HUD rail — 0.72 / 0.28.
  CHECK(rows[1].cells.size() == 2);
  CHECK(approx(rows[1].cells[0].width_fraction, 0.72F));
  CHECK(approx(rows[1].cells[1].width_fraction, 0.28F));
  CHECK(layout.zones[rows[1].cells[0].zone_index].id == "arrangement");

  // Row 2 (harmony | structure): two equal-weight columns.
  CHECK(rows[2].cells.size() == 2);
  CHECK(approx(rows[2].cells[0].width_fraction, 0.5F));
  CHECK(approx(rows[2].cells[1].width_fraction, 0.5F));

  // Height fractions sum to 1, and the middle (unweighted-default) row
  // dominates over the two explicitly-thin strips.
  const float height_sum =
      rows[0].height_fraction + rows[1].height_fraction + rows[2].height_fraction;
  CHECK(approx(height_sum, 1.0F, 1e-3F));
  CHECK(rows[1].height_fraction > rows[0].height_fraction);
  CHECK(rows[1].height_fraction > rows[2].height_fraction);
}

void test_compute_rows_defaults_unweighted_columns_to_equal_split() {
  sonotron::Layout layout;
  layout.zones = {
      sonotron::Zone{.id = "a",
                     .title = "A",
                     .row = 0,
                     .col = 0,
                     .full_span = false,
                     .width_weight = std::nullopt,
                     .height_weight = std::nullopt},
      sonotron::Zone{.id = "b",
                     .title = "B",
                     .row = 0,
                     .col = 1,
                     .full_span = false,
                     .width_weight = std::nullopt,
                     .height_weight = std::nullopt},
      sonotron::Zone{.id = "c",
                     .title = "C",
                     .row = 0,
                     .col = 2,
                     .full_span = false,
                     .width_weight = std::nullopt,
                     .height_weight = std::nullopt},
  };
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  CHECK(rows.size() == 1);
  CHECK(rows[0].cells.size() == 3);
  for (const sonotron::ZoneGeometry& cell : rows[0].cells) {
    CHECK(approx(cell.width_fraction, 1.0F / 3.0F));
  }
}

void test_compute_rows_orders_columns_by_col_not_declaration_order() {
  sonotron::Layout layout;
  layout.zones = {
      sonotron::Zone{.id = "second",
                     .title = "Second",
                     .row = 0,
                     .col = 1,
                     .full_span = false,
                     .width_weight = std::nullopt,
                     .height_weight = std::nullopt},
      sonotron::Zone{.id = "first",
                     .title = "First",
                     .row = 0,
                     .col = 0,
                     .full_span = false,
                     .width_weight = std::nullopt,
                     .height_weight = std::nullopt},
  };
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  CHECK(rows.size() == 1);
  CHECK(rows[0].cells.size() == 2);
  CHECK(layout.zones[rows[0].cells[0].zone_index].id == "first");
  CHECK(layout.zones[rows[0].cells[1].zone_index].id == "second");
}

}  // namespace

int main() {
  test_default_layout_shape();
  test_layout_equality();
  test_is_valid_font_size_px_bounds();
  test_compute_rows_default_layout();
  test_compute_rows_defaults_unweighted_columns_to_equal_split();
  test_compute_rows_orders_columns_by_col_not_declaration_order();
  return sonotron::test::failures();
}

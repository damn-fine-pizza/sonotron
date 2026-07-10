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
  CHECK(layout.zones.size() == 6);

  CHECK(layout.zones[0].id == "transport");
  CHECK(layout.zones[0].row == 0);
  CHECK(layout.zones[0].full_span);

  CHECK(layout.zones[1].id == "browser");
  CHECK(layout.zones[1].row == 1);
  CHECK(layout.zones[1].col == 0);

  CHECK(layout.zones[2].id == "grid");
  CHECK(layout.zones[2].row == 1);
  CHECK(layout.zones[2].col == 1);

  // The right rail: Intention over Parts, both in (row 1, col 2) — this is
  // the nested vertical stack.
  CHECK(layout.zones[3].id == "intention");
  CHECK(layout.zones[3].row == 1);
  CHECK(layout.zones[3].col == 2);

  CHECK(layout.zones[4].id == "parts");
  CHECK(layout.zones[4].row == 1);
  CHECK(layout.zones[4].col == 2);

  CHECK(layout.zones[5].id == "seqedit");
  CHECK(layout.zones[5].row == 2);
  CHECK(layout.zones[5].full_span);

  // Every zone starts shown; the View-menu toggles flip this.
  for (const sonotron::Zone& zone : layout.zones) {
    CHECK(zone.visible);
  }
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

  // Row 0 (transport): a single, full-span zone — one cell, one-zone stack.
  CHECK(rows[0].cells.size() == 1);
  CHECK(approx(rows[0].cells[0].width_fraction, 1.0F));
  CHECK(rows[0].cells[0].stack.size() == 1);
  CHECK(layout.zones[rows[0].cells[0].stack[0].zone_index].id == "transport");

  // Row 1 (Browser | Repeat Zone | right rail): 0.22 / 0.54 / 0.24.
  CHECK(rows[1].cells.size() == 3);
  CHECK(approx(rows[1].cells[0].width_fraction, 0.22F));
  CHECK(approx(rows[1].cells[1].width_fraction, 0.54F));
  CHECK(approx(rows[1].cells[2].width_fraction, 0.24F));
  CHECK(layout.zones[rows[1].cells[0].stack[0].zone_index].id == "browser");
  CHECK(layout.zones[rows[1].cells[1].stack[0].zone_index].id == "grid");

  // The third cell of row 1 is the NESTED STACK: Intention over Parts, split
  // 0.38 / 0.62 of the cell's height.
  const sonotron::ZoneGeometry& rail = rows[1].cells[2];
  CHECK(rail.stack.size() == 2);
  CHECK(layout.zones[rail.stack[0].zone_index].id == "intention");
  CHECK(layout.zones[rail.stack[1].zone_index].id == "parts");
  CHECK(approx(rail.stack[0].height_fraction, 0.38F));
  CHECK(approx(rail.stack[1].height_fraction, 0.62F));
  CHECK(approx(rail.stack[0].height_fraction + rail.stack[1].height_fraction, 1.0F));

  // Row 2 (seqedit): a single, full-span zone.
  CHECK(rows[2].cells.size() == 1);
  CHECK(approx(rows[2].cells[0].width_fraction, 1.0F));
  CHECK(layout.zones[rows[2].cells[0].stack[0].zone_index].id == "seqedit");

  // Height fractions sum to 1, and the tall middle row dominates the two
  // thin strips above and below.
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
  CHECK(layout.zones[rows[0].cells[0].stack[0].zone_index].id == "first");
  CHECK(layout.zones[rows[0].cells[1].stack[0].zone_index].id == "second");
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

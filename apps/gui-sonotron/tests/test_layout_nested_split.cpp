// Unit tests for the layout engine's nested vertical split and `visible`
// toggle (ux-workstation.md §4.6) — the capability the Fase 2 restart added
// so the right rail can stack Intention over Parts inside one column. Pure
// computation over compute_rows(); no JSON, no ImGui, no I/O.

#include "src/layout_model.hpp"

#include <cmath>

#include "test.hpp"

namespace {

bool approx(float a, float b, float epsilon = 1e-4F) { return std::fabs(a - b) < epsilon; }

// Builds a Zone spelling out every field — this codebase compiles with
// -Werror=missing-field-initializers, so partial designated initializers are
// rejected. Wrapping the construction keeps the tests below readable.
sonotron::Zone zone(std::string id, int row, int col, bool visible,
                    std::optional<float> width_weight, std::optional<float> height_weight) {
  return sonotron::Zone{.id = std::move(id),
                        .title = "Z",
                        .row = row,
                        .col = col,
                        .full_span = false,
                        .visible = visible,
                        .width_weight = width_weight,
                        .height_weight = height_weight};
}

// A minimal one-row layout: two full-width columns, the second of which is a
// vertical stack of two zones sharing (row, col).
sonotron::Layout two_col_with_stacked_second() {
  sonotron::Layout layout;
  layout.zones = {
      zone("left", 0, 0, true, 0.5F, std::nullopt),
      zone("top", 0, 1, true, 0.5F, 0.3F),
      zone("bottom", 0, 1, true, 0.5F, 0.7F),
  };
  return layout;
}

void test_shared_row_col_stacks_vertically_in_one_cell() {
  const sonotron::Layout layout = two_col_with_stacked_second();
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);

  CHECK(rows.size() == 1);
  // Two column cells, not three zones-as-cells: the shared (row, col) collapses
  // to a single cell holding a stack.
  CHECK(rows[0].cells.size() == 2);

  const sonotron::ZoneGeometry& left = rows[0].cells[0];
  CHECK(left.stack.size() == 1);
  CHECK(layout.zones[left.stack[0].zone_index].id == "left");
  CHECK(approx(left.width_fraction, 0.5F));

  const sonotron::ZoneGeometry& stacked = rows[0].cells[1];
  CHECK(approx(stacked.width_fraction, 0.5F));
  CHECK(stacked.stack.size() == 2);
}

void test_stack_order_is_declaration_order_and_heights_normalize() {
  const sonotron::Layout layout = two_col_with_stacked_second();
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  const sonotron::ZoneGeometry& stacked = rows[0].cells[1];

  // "top" declared before "bottom" → top is first (rendered at the top).
  CHECK(layout.zones[stacked.stack[0].zone_index].id == "top");
  CHECK(layout.zones[stacked.stack[1].zone_index].id == "bottom");

  // 0.3 / 0.7 already sum to 1, so they normalize to themselves.
  CHECK(approx(stacked.stack[0].height_fraction, 0.3F));
  CHECK(approx(stacked.stack[1].height_fraction, 0.7F));
  CHECK(approx(stacked.stack[0].height_fraction + stacked.stack[1].height_fraction, 1.0F));
}

void test_unweighted_stack_splits_evenly() {
  sonotron::Layout layout;
  layout.zones = {
      zone("only", 0, 0, true, std::nullopt, std::nullopt),
      zone("a", 1, 0, true, std::nullopt, std::nullopt),
      zone("b", 1, 0, true, std::nullopt, std::nullopt),
      zone("c", 1, 0, true, std::nullopt, std::nullopt),
  };
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  CHECK(rows.size() == 2);
  CHECK(rows[1].cells.size() == 1);
  const sonotron::ZoneGeometry& cell = rows[1].cells[0];
  CHECK(cell.stack.size() == 3);
  for (const sonotron::StackedZone& item : cell.stack) {
    CHECK(approx(item.height_fraction, 1.0F / 3.0F));
  }
}

void test_hidden_zone_is_removed_and_siblings_redistribute() {
  sonotron::Layout layout;
  layout.zones = {
      zone("a", 0, 0, true, 1.0F, std::nullopt),
      zone("b", 0, 1, true, 1.0F, std::nullopt),
      zone("c", 0, 2, false, 1.0F, std::nullopt),
  };
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  CHECK(rows.size() == 1);
  // The hidden zone takes no cell; the two shown ones split the full width.
  CHECK(rows[0].cells.size() == 2);
  CHECK(approx(rows[0].cells[0].width_fraction, 0.5F));
  CHECK(approx(rows[0].cells[1].width_fraction, 0.5F));
}

void test_hiding_one_stack_member_collapses_to_single() {
  sonotron::Layout layout = two_col_with_stacked_second();
  layout.zones[1].visible = false;  // hide "top"; only "bottom" remains in the stack
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);

  CHECK(rows.size() == 1);
  CHECK(rows[0].cells.size() == 2);
  const sonotron::ZoneGeometry& stacked = rows[0].cells[1];
  CHECK(stacked.stack.size() == 1);
  CHECK(layout.zones[stacked.stack[0].zone_index].id == "bottom");
  CHECK(approx(stacked.stack[0].height_fraction, 1.0F));
}

void test_all_zones_in_row_hidden_drops_the_row() {
  sonotron::Layout layout;
  layout.zones = {
      zone("shown", 0, 0, true, std::nullopt, std::nullopt),
      zone("gone1", 1, 0, false, std::nullopt, std::nullopt),
      zone("gone2", 1, 1, false, std::nullopt, std::nullopt),
  };
  const std::vector<sonotron::RowGeometry> rows = sonotron::compute_rows(layout);
  // Row 1 vanished entirely; only the shown row remains, full height.
  CHECK(rows.size() == 1);
  CHECK(rows[0].cells.size() == 1);
  CHECK(layout.zones[rows[0].cells[0].stack[0].zone_index].id == "shown");
  CHECK(approx(rows[0].height_fraction, 1.0F));
}

}  // namespace

int main() {
  test_shared_row_col_stacks_vertically_in_one_cell();
  test_stack_order_is_declaration_order_and_heights_normalize();
  test_unweighted_stack_splits_evenly();
  test_hidden_zone_is_removed_and_siblings_redistribute();
  test_hiding_one_stack_member_collapses_to_single();
  test_all_zones_in_row_hidden_drops_the_row();
  return sonotron::test::failures();
}

#include "layout_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace sonotron {

namespace {

constexpr float kWeightEpsilon = 1e-4F;

// The renderable zone-id inventory backing is_renderable_zone_id() below —
// see that function's declaration in layout_model.hpp for why this is the
// one place this list is written down. Keep in sync with
// layout_renderer.cpp's render_zone_content() if-else dispatch.
constexpr std::array<std::string_view, 6> kRenderableZoneIds = {
    "transport", "browser", "grid", "seqedit", "parts", "intention",
};

bool nearly_equal(float lhs, float rhs) { return std::fabs(lhs - rhs) < kWeightEpsilon; }

bool nearly_equal(const std::optional<float>& lhs, const std::optional<float>& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }
  if (!lhs.has_value()) {
    return true;
  }
  return nearly_equal(*lhs, *rhs);
}

}  // namespace

bool is_valid_font_size_px(float value) {
  return std::isfinite(value) && value >= kMinFontSizePx && value <= kMaxFontSizePx;
}

bool operator==(const Zone& lhs, const Zone& rhs) {
  return lhs.id == rhs.id && lhs.title == rhs.title && lhs.row == rhs.row && lhs.col == rhs.col &&
         lhs.full_span == rhs.full_span && lhs.visible == rhs.visible &&
         nearly_equal(lhs.width_weight, rhs.width_weight) &&
         nearly_equal(lhs.height_weight, rhs.height_weight);
}

bool operator==(const Layout& lhs, const Layout& rhs) {
  return lhs.schema_version == rhs.schema_version && lhs.window_title == rhs.window_title &&
         nearly_equal(lhs.font_size_px, rhs.font_size_px) && lhs.zones == rhs.zones;
}

bool is_renderable_zone_id(std::string_view id) {
  return std::find(kRenderableZoneIds.begin(), kRenderableZoneIds.end(), id) !=
         kRenderableZoneIds.end();
}

Layout default_layout() {
  Layout layout;
  layout.window_title = "sonotron";
  // The workstation screen (ux-workstation.md §3): a thin Transport bar on
  // top; a tall middle row of Browser | Repeat-Zone hero | right rail; and
  // the Sequence-Edit surface across the bottom. The right rail (col 2 of the
  // middle row) is a NESTED VERTICAL STACK — Intention over Parts — expressed
  // by two zones sharing (row 1, col 2); their height_weights (0.38 / 0.62)
  // split that one column. Browser/Grid carry height_weight 1.0 so the middle
  // row stays the tall one (row height = max weight in the row), independent
  // of the rail's internal split.
  layout.zones = {
      Zone{.id = "transport",
           .title = "Transport",
           .row = 0,
           .col = 0,
           .full_span = true,
           .visible = true,
           .width_weight = std::nullopt,
           .height_weight = 0.06F},
      Zone{.id = "browser",
           .title = "Browser",
           .row = 1,
           .col = 0,
           .full_span = false,
           .visible = true,
           .width_weight = 0.22F,
           .height_weight = 1.0F},
      Zone{.id = "grid",
           .title = "Repeat Zone",
           .row = 1,
           .col = 1,
           .full_span = false,
           .visible = true,
           .width_weight = 0.54F,
           .height_weight = 1.0F},
      Zone{.id = "intention",
           .title = "Intention",
           .row = 1,
           .col = 2,
           .full_span = false,
           .visible = true,
           .width_weight = 0.24F,
           .height_weight = 0.38F},
      Zone{.id = "parts",
           .title = "Parts / Mixer",
           .row = 1,
           .col = 2,
           .full_span = false,
           .visible = true,
           .width_weight = 0.24F,
           .height_weight = 0.62F},
      Zone{.id = "seqedit",
           .title = "Sequence Edit",
           .row = 2,
           .col = 0,
           .full_span = true,
           .visible = true,
           .width_weight = std::nullopt,
           .height_weight = 0.34F},
  };
  return layout;
}

namespace {

// Builds one cell's vertical stack from the zones sharing a (row, col),
// given in declaration order. A single zone fills the cell (height 1.0);
// several split the cell by their height_weight (value_or 1.0), normalized
// within the stack. The cell's own width weight is the largest width_weight
// among its stacked zones (value_or 1.0) — they share a column, so one
// representative width governs the whole stack.
ZoneGeometry build_cell(const Layout& layout, const std::vector<std::size_t>& stack_indices) {
  ZoneGeometry cell;
  float weight_sum = 0.0F;
  float max_width_weight = 0.0F;
  for (const std::size_t idx : stack_indices) {
    weight_sum += layout.zones[idx].height_weight.value_or(1.0F);
    max_width_weight = std::max(max_width_weight, layout.zones[idx].width_weight.value_or(1.0F));
  }
  if (weight_sum <= 0.0F) {
    weight_sum = static_cast<float>(stack_indices.size());
  }
  cell.width_fraction = max_width_weight;  // normalized against sibling cells later
  for (const std::size_t idx : stack_indices) {
    const float weight = layout.zones[idx].height_weight.value_or(1.0F);
    const float fraction = (stack_indices.size() == 1) ? 1.0F : (weight / weight_sum);
    cell.stack.push_back(StackedZone{.zone_index = idx, .height_fraction = fraction});
  }
  return cell;
}

// The largest height_weight present among a row's zones (unset means 1.0);
// this becomes the row's height weight, so a stacked column's internal split
// never shrinks the row it lives in.
float row_height_weight(const Layout& layout, const std::vector<std::size_t>& indices) {
  float max_height_weight = 0.0F;
  bool has_height_weight = false;
  for (const std::size_t idx : indices) {
    const std::optional<float>& weight = layout.zones[idx].height_weight;
    if (weight.has_value()) {
      has_height_weight = true;
      max_height_weight = std::max(max_height_weight, *weight);
    }
  }
  return has_height_weight ? max_height_weight : 1.0F;
}

// Groups a row's zones into column cells by ascending col (zones sharing a
// col stack vertically inside one cell), then normalizes the cells' widths to
// sum to 1. A row with a single column cell is inherently full width.
std::vector<ZoneGeometry> build_row_cells(const Layout& layout,
                                          const std::vector<std::size_t>& indices) {
  std::map<int, std::vector<std::size_t>> zones_by_col;
  for (const std::size_t idx : indices) {
    zones_by_col[layout.zones[idx].col].push_back(idx);
  }

  std::vector<ZoneGeometry> cells;
  float width_sum = 0.0F;
  for (const auto& [col_index, stack_indices] : zones_by_col) {
    ZoneGeometry cell = build_cell(layout, stack_indices);
    width_sum += cell.width_fraction;
    cells.push_back(std::move(cell));
  }

  if (cells.size() == 1) {
    cells.front().width_fraction = 1.0F;
    return cells;
  }
  if (width_sum <= 0.0F) {
    width_sum = static_cast<float>(cells.size());
  }
  for (ZoneGeometry& cell : cells) {
    cell.width_fraction /= width_sum;
  }
  return cells;
}

}  // namespace

std::vector<RowGeometry> compute_rows(const Layout& layout) {
  // Group VISIBLE zone indices by row, preserving ascending row order; a
  // hidden zone (visible == false) takes no space and its siblings
  // redistribute. Indices are pushed in declaration order, so each group
  // stays in that order — the stack order within a column cell.
  std::map<int, std::vector<std::size_t>> zones_by_row;
  for (std::size_t i = 0; i < layout.zones.size(); ++i) {
    if (layout.zones[i].visible) {
      zones_by_row[layout.zones[i].row].push_back(i);
    }
  }

  // First pass: resolve each row's raw height weight and its cells (ordered
  // by ascending col, each cell a vertical stack), so the height
  // normalization below can see every row's weight before computing
  // fractions.
  struct RawRow {
    float height_weight = 1.0F;
    std::vector<ZoneGeometry> cells;
  };
  std::vector<RawRow> raw_rows;
  raw_rows.reserve(zones_by_row.size());

  for (const auto& [row_index, indices] : zones_by_row) {
    raw_rows.push_back(RawRow{.height_weight = row_height_weight(layout, indices),
                              .cells = build_row_cells(layout, indices)});
  }

  float height_sum = 0.0F;
  for (const RawRow& raw : raw_rows) {
    height_sum += raw.height_weight;
  }
  if (height_sum <= 0.0F) {
    height_sum = static_cast<float>(raw_rows.size());
  }

  std::vector<RowGeometry> rows;
  rows.reserve(raw_rows.size());
  for (RawRow& raw : raw_rows) {
    const float fraction = (height_sum <= 0.0F) ? (1.0F / static_cast<float>(raw_rows.size()))
                                                : (raw.height_weight / height_sum);
    rows.push_back(RowGeometry{.height_fraction = fraction, .cells = std::move(raw.cells)});
  }
  return rows;
}

}  // namespace sonotron

#include "layout_model.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace sonotron {

namespace {

constexpr float kWeightEpsilon = 1e-4F;

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
         lhs.full_span == rhs.full_span && nearly_equal(lhs.width_weight, rhs.width_weight) &&
         nearly_equal(lhs.height_weight, rhs.height_weight);
}

bool operator==(const Layout& lhs, const Layout& rhs) {
  return lhs.window_title == rhs.window_title && nearly_equal(lhs.font_size_px, rhs.font_size_px) &&
         lhs.zones == rhs.zones;
}

Layout default_layout() {
  Layout layout;
  layout.window_title = "sonotron";
  layout.zones = {
      Zone{.id = "transport",
           .title = "Transport · Seed",
           .row = 0,
           .col = 0,
           .full_span = true,
           .width_weight = std::nullopt,
           .height_weight = 0.10F},
      Zone{.id = "intention",
           .title = "Intention",
           .row = 1,
           .col = 0,
           .full_span = false,
           .width_weight = 0.5F,
           .height_weight = std::nullopt},
      Zone{.id = "band",
           .title = "Band",
           .row = 1,
           .col = 1,
           .full_span = false,
           .width_weight = 0.5F,
           .height_weight = std::nullopt},
      Zone{.id = "harmony",
           .title = "Harmony",
           .row = 2,
           .col = 0,
           .full_span = false,
           .width_weight = 0.5F,
           .height_weight = 0.15F},
      Zone{.id = "structure",
           .title = "Structure",
           .row = 2,
           .col = 1,
           .full_span = false,
           .width_weight = 0.5F,
           .height_weight = 0.15F},
  };
  return layout;
}

std::vector<RowGeometry> compute_rows(const Layout& layout) {
  // Group zone indices by row, preserving ascending row order.
  std::map<int, std::vector<std::size_t>> zones_by_row;
  for (std::size_t i = 0; i < layout.zones.size(); ++i) {
    zones_by_row[layout.zones[i].row].push_back(i);
  }

  // First pass: resolve each row's raw height weight and each row's cells
  // (already ordered by ascending col), so the height normalization below
  // can see every row's weight before computing fractions.
  struct RawRow {
    float height_weight = 1.0F;
    std::vector<ZoneGeometry> cells;
  };
  std::vector<RawRow> raw_rows;
  raw_rows.reserve(zones_by_row.size());

  for (auto& [row_index, indices] : zones_by_row) {
    std::sort(indices.begin(), indices.end(), [&layout](std::size_t a, std::size_t b) {
      return layout.zones[a].col < layout.zones[b].col;
    });

    RawRow raw;
    float max_height_weight = 0.0F;
    bool has_height_weight = false;
    for (const std::size_t idx : indices) {
      const std::optional<float>& weight = layout.zones[idx].height_weight;
      if (weight.has_value()) {
        has_height_weight = true;
        max_height_weight = std::max(max_height_weight, *weight);
      }
    }
    raw.height_weight = has_height_weight ? max_height_weight : 1.0F;

    // A row with a single zone is inherently full width, regardless of a
    // stored `width_weight`/`full_span` value.
    if (indices.size() == 1) {
      raw.cells.push_back(ZoneGeometry{.zone_index = indices.front(), .width_fraction = 1.0F});
    } else {
      float width_sum = 0.0F;
      for (const std::size_t idx : indices) {
        width_sum += layout.zones[idx].width_weight.value_or(1.0F);
      }
      if (width_sum <= 0.0F) {
        width_sum = static_cast<float>(indices.size());
      }
      for (const std::size_t idx : indices) {
        const float weight = layout.zones[idx].width_weight.value_or(1.0F);
        const float fraction = (width_sum <= 0.0F) ? (1.0F / static_cast<float>(indices.size()))
                                                   : (weight / width_sum);
        raw.cells.push_back(ZoneGeometry{.zone_index = idx, .width_fraction = fraction});
      }
    }
    raw_rows.push_back(std::move(raw));
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
  for (const RawRow& raw : raw_rows) {
    const float fraction = (height_sum <= 0.0F) ? (1.0F / static_cast<float>(raw_rows.size()))
                                                : (raw.height_weight / height_sum);
    rows.push_back(RowGeometry{.height_fraction = fraction, .cells = raw.cells});
  }
  return rows;
}

}  // namespace sonotron

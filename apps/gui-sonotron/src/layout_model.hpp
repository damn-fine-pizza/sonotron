#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Pure-data UI layout model for gui-sonotron's general screen layout.
//
// This header is intentionally free of any rendering or I/O dependency: a
// `Layout` is just data, so a future layout-editing tool or a designer
// hand-editing the JSON file never needs to touch ImGui or the parser.
// `layout_json.hpp` converts JSON text <-> Layout; `layout_renderer.hpp`
// draws a Layout with ImGui. Neither of those headers is included here, and
// this header includes neither of theirs — the three stay cleanly split.

namespace sonotron {

// Base UI font size, in *logical* (1x DPI) pixels — i.e. before the
// runtime GLFW content-scale multiply applied in main.cpp. `Layout` carries
// this as a configurable knob (the "font_size" JSON key) so the owner can
// tune legibility without a rebuild. kMinFontSizePx/kMaxFontSizePx bound a
// sane range; is_valid_font_size_px() is what both the JSON reader and
// main.cpp use to reject a hand-edited value that is absent, non-finite, or
// silly (e.g. 0 or 500), falling back to kDefaultFontSizePx instead.
inline constexpr float kDefaultFontSizePx = 14.0F;
inline constexpr float kMinFontSizePx = 6.0F;
inline constexpr float kMaxFontSizePx = 64.0F;

bool is_valid_font_size_px(float value);

// One titled zone of the dashboard. `row`/`col` place it on the grid: `col`
// is only meaningful relative to sibling zones sharing the same `row` (their
// left-to-right order is by ascending `col`). `full_span` makes the zone
// occupy the whole row width, ignoring `col` and any sibling in that row.
//
// `width_weight`/`height_weight` are RELATIVE weights, not fractions — a
// zone that leaves one unset behaves as weight 1.0. Weights are normalized
// on demand by `compute_rows`, so they need not sum to 1 in the JSON file;
// that keeps hand-edits forgiving (add a zone without rebalancing every
// sibling).
struct Zone {
  std::string id;
  std::string title;
  int row = 0;
  int col = 0;
  bool full_span = false;
  std::optional<float> width_weight;
  std::optional<float> height_weight;
};

bool operator==(const Zone& lhs, const Zone& rhs);

// The whole dashboard: a window title and the flat zone list. Row/column
// grouping is derived on demand by `compute_rows`, not stored redundantly
// here — the model is exactly what a hand-edited JSON file expresses.
struct Layout {
  std::string window_title = "sonotron";
  // See kDefaultFontSizePx above for the meaning/units and the fallback
  // rule; the in-class default here is what a freshly-constructed Layout
  // (or one whose JSON omitted "font_size") gets.
  float font_size_px = kDefaultFontSizePx;
  std::vector<Zone> zones;
};

bool operator==(const Layout& lhs, const Layout& rhs);

// The built-in fallback: the 5-zone primary-screen layout from
// docs/design/ux-concept.md (Transport · Seed / Intention | Band /
// Harmony | Structure), written to disk the first time the app runs.
Layout default_layout();

// Resolved, render-ready geometry: one row per distinct `Zone::row` value
// present in `layout.zones` (ascending), each row's zones ordered by
// ascending `col`, with weights normalized into fractions that sum to 1
// (within a row for width; across all rows for height). Pure computation —
// no ImGui, no I/O — so it is unit-testable on its own and reusable by any
// future renderer or tool.
struct ZoneGeometry {
  std::size_t zone_index = 0;  // index into the source Layout::zones
  float width_fraction = 1.0F;
};

struct RowGeometry {
  float height_fraction = 1.0F;
  std::vector<ZoneGeometry> cells;  // left-to-right
};

std::vector<RowGeometry> compute_rows(const Layout& layout);

}  // namespace sonotron

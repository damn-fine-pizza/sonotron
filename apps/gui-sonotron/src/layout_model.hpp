#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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
inline constexpr float kDefaultFontSizePx = 13.0F;
inline constexpr float kMinFontSizePx = 6.0F;
inline constexpr float kMaxFontSizePx = 64.0F;

bool is_valid_font_size_px(float value);

// The on-disk JSON schema version (the "schema_version" key, written first
// by write_layout() for readability). Bump this whenever the persisted
// shape changes in a way an OLDER binary's renderer could not safely draw
// (e.g. a zone id inventory change) — Layout::schema_version and the
// load-time check in layout_json.cpp's load_or_create_default() are what
// turn a stale file (from an older app version) into a self-upgraded
// default instead of a silently broken UI (host-side analog of
// docs/DESIGN.md §2 architectural principle #8, "one single versioned
// format").
inline constexpr int kLayoutSchemaVersion = 1;

// One titled zone of the dashboard. `row`/`col` place it on the grid: `col`
// is only meaningful relative to sibling zones sharing the same `row` (their
// left-to-right order is by ascending `col`). `full_span` makes the zone
// occupy the whole row width, ignoring `col` and any sibling in that row.
//
// NESTED VERTICAL SPLIT: two or more zones that share the SAME (row, col)
// stack vertically inside that one column cell, top-to-bottom in declaration
// order, their relative heights taken from `height_weight`. This is how the
// workstation right rail puts Intention above Parts in a single column
// (ux-workstation.md §4.6) without a second layout mechanism — the flat zone
// list still expresses the whole screen.
//
// `visible` false removes the zone from the computed geometry entirely (it
// takes no space and its siblings redistribute); this backs the View-menu
// toggles (e.g. hide the Intention rail) additively, defaulting to shown.
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
  bool visible = true;
  std::optional<float> width_weight;
  std::optional<float> height_weight;
};

bool operator==(const Zone& lhs, const Zone& rhs);

// The whole dashboard: a window title and the flat zone list. Row/column
// grouping is derived on demand by `compute_rows`, not stored redundantly
// here — the model is exactly what a hand-edited JSON file expresses.
struct Layout {
  // See kLayoutSchemaVersion above. A freshly-constructed Layout (or one
  // built by default_layout()) is always current; parse_layout() treats an
  // ABSENT "schema_version" key as legacy and sets this to 0 before
  // dispatching the rest of the file's fields, so load_or_create_default()
  // (layout_json.cpp) can tell a pre-versioning file apart from a current
  // one.
  int schema_version = kLayoutSchemaVersion;
  std::string window_title = "sonotron";
  // See kDefaultFontSizePx above for the meaning/units and the fallback
  // rule; the in-class default here is what a freshly-constructed Layout
  // (or one whose JSON omitted "font_size") gets.
  float font_size_px = kDefaultFontSizePx;
  std::vector<Zone> zones;
};

bool operator==(const Layout& lhs, const Layout& rhs);

// The single authority on which zone ids layout_renderer.cpp's
// render_zone_content() can actually dispatch to a live panel (transport,
// browser, grid, seqedit, parts, intention — G3, docs/design/
// gui-fase2-mechanical-plan.md). layout_json.cpp's load_or_create_default()
// consults the SAME predicate to decide whether a persisted zone id is
// still renderable, so the loader's notion of "valid id" can never drift
// from what the renderer can actually draw — that drift is exactly what let
// a stale layout.json silently render empty panels.
bool is_renderable_zone_id(std::string_view id);

// The built-in fallback: the workstation screen from
// docs/design/ux-workstation.md §3 — Transport across the top; Browser,
// the Repeat-Zone hero, and the Intention-over-Parts right rail across the
// middle; the Sequence-Edit surface across the bottom. Written to disk the
// first time the app runs.
Layout default_layout();

// Resolved, render-ready geometry: one row per distinct `Zone::row` value
// present among the VISIBLE zones (ascending), each row split left-to-right
// into cells by ascending `col`, and each cell a top-to-bottom stack of the
// zones that share its (row, col). Weights are normalized into fractions
// that sum to 1 (within a row for width; within a cell for its stack; across
// all rows for height). Pure computation — no ImGui, no I/O — so it is
// unit-testable on its own and reusable by any future renderer or tool.

// One zone inside a cell's vertical stack. `height_fraction` is its share of
// the cell's height; a cell holding a single zone has one entry at 1.0.
struct StackedZone {
  std::size_t zone_index = 0;  // index into the source Layout::zones
  float height_fraction = 1.0F;
};

// One horizontal cell of a row: a column of `width_fraction` width holding a
// vertical stack (usually one zone; more than one is the nested split).
struct ZoneGeometry {
  float width_fraction = 1.0F;
  std::vector<StackedZone> stack;  // top-to-bottom
};

struct RowGeometry {
  float height_fraction = 1.0F;
  std::vector<ZoneGeometry> cells;  // left-to-right
};

std::vector<RowGeometry> compute_rows(const Layout& layout);

}  // namespace sonotron

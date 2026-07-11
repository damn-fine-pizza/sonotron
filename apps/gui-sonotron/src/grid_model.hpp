#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "track_roles.hpp"

// Pure-data matrix/scene shape for the Repeat Zone — the Live-Loops launch
// grid (ux-workstation.md §4.4/§5). No ImGui, no I/O; grid_panel.cpp is the
// only file that renders this with ImGui or wires drag-drop.

namespace sonotron {

// Whether a launched cell/scene actually fires anything on the core. FALSE
// until the core clip/scene primitive ships (`launch/stop clip <id>
// quantize <n>`, `launch scene <n> quantize <q>` — ux-workstation.md
// §11.3, brought-forward front-of-line core work, still pending). Kept as
// one named constant grid_panel.cpp checks to grey out the launch
// affordance honestly rather than silently doing nothing on click; flip it
// to true (no GridModel/grid_panel rewrite needed) once a real send() of
// the launch verb lands.
inline constexpr bool kGridLaunchWired = false;

enum class GridCellKind : std::uint8_t { kEmpty, kStyleSection, kChordSequence, kStepTrack };

// One cell of the matrix: a part row x scene column, holding one of the
// three material kinds the browser offers (§5), or empty. `label` is
// display text only (e.g. a style name) — the cell does not yet reference
// a real core object, since none exists until the clip primitive lands.
struct GridCell {
  GridCellKind kind = GridCellKind::kEmpty;
  std::string label;
};

// Rows are the 9 TrackRole parts (track_roles.hpp); columns are scenes.
// Real cell CONTENT is real today (dragging a style from the browser sets
// a cell, §4.3/§5); real LAUNCH stays an honest placeholder
// (kGridLaunchWired) until the core clip primitive exists. The shape is
// modeled now so the panel lights up later with no rewrite.
class GridModel {
 public:
  static constexpr std::size_t kPartCount = kTrackRoleCount;
  static constexpr std::size_t kDefaultSceneCount = 3;
  static constexpr std::size_t kMaxSceneCount = 8;

  explicit GridModel(std::size_t scene_count = kDefaultSceneCount);

  std::size_t part_count() const { return kPartCount; }
  std::size_t scene_count() const { return m_scene_count; }
  std::string_view part_label(std::size_t part_index) const;

  const GridCell& cell(std::size_t part_index, std::size_t scene_index) const;
  void set_cell(std::size_t part_index, std::size_t scene_index, GridCellKind kind,
                std::string label);
  void clear_cell(std::size_t part_index, std::size_t scene_index);

  // Adds one more scene column (the "+" affordance in the §3 wireframe's
  // scene header row), preserving every existing cell's content. A no-op
  // once kMaxSceneCount is reached.
  void add_scene();

 private:
  std::size_t index_of(std::size_t part_index, std::size_t scene_index) const;

  std::size_t m_scene_count;
  std::vector<GridCell> m_cells;  // row-major: part_index * m_scene_count + scene_index
};

}  // namespace sonotron

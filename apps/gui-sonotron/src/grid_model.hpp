#pragma once

#include <array>
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

// Whether a launched cell/scene actually fires anything on the core. TRUE:
// the core clip/scene primitive shipped (Phase-5 Item #2, docs/design/
// clip-primitive-design.md) -- `launch clip <id> quantize <n>` / `launch
// scene <n> quantize <q>` are real L1 verbs (components/platform/hostrt/
// shell_clip_commands.cpp), the core emits a real `clip` event, and
// grid_panel.cpp's cell/scene-header buttons send() them for real. Kept as
// one named constant (rather than deleting it outright) for symmetry with
// the equally-real content-registration path below.
inline constexpr bool kGridLaunchWired = true;

enum class GridCellKind : std::uint8_t { kEmpty, kStyleSection, kChordSequence, kStepTrack };

// One cell of the matrix: a part row x scene column, holding one of the
// three material kinds the browser offers (§5), or empty. `label` is display
// text only (e.g. a style name) -- the GUI's own display copy, kept
// independent of whatever the core's ClipMatrix stores for the same cell
// (repeat-zone-real-contract.md §3: a browser-dropped style DOES now
// register a real ClipMatrix clip at this cell's stable id, grid_panel.cpp's
// drag-drop handler; the "+" empty-cell placeholder click still only sets
// this local display cell, unregistered -- there is no authored-content path
// into ClipMatrix from the GUI yet, only drag-a-style, per owner decision 2).
struct GridCell {
  GridCellKind kind = GridCellKind::kEmpty;
  std::string label;
};

// Rows are the 9 TrackRole parts (track_roles.hpp); columns are scenes. Real
// cell CONTENT is real today (dragging a style from the browser sets a cell,
// §4.3/§5) AND now registers with the core's ClipMatrix at the cell's own
// stable id (repeat-zone-real-contract.md §3 Shape A) so a launch actually
// addresses THIS cell's material, not an empty pool slot; real LAUNCH is
// wired too (kGridLaunchWired) through the core clip primitive (Phase-5 Item
// #2).
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

  // Host-only scene display name (repeat-zone-real-contract.md §4/§8b
  // decision 3: OWNER LOCKED to host-only storage here, NOT core-resident --
  // no kSceneName verb). Storage is sized to kMaxSceneCount (not
  // m_scene_count), so a name set on a not-yet-added column survives a later
  // add_scene() unchanged, and every index up to kMaxSceneCount is always
  // valid to query even before that many columns exist. Defaults to the bare
  // 1-based column number, matching the pre-rename display exactly.
  // Bounds-checked: an out-of-range `scene_index` (>= kMaxSceneCount) is a
  // no-op for the setter and returns an empty view from the getter, rather
  // than asserting or indexing out of bounds.
  std::string_view scene_name(std::size_t scene_index) const;
  void set_scene_name(std::size_t scene_index, std::string name);

  // Host-side per-scene SECTION (repeat-zone-real-contract.md SLICE 4a,
  // owner-locked model decision: a scene/grid COLUMN carries a SectionType,
  // applied through the EXISTING `style section` verb -- NOT a ClipMatrix
  // change, NOT a new core mechanism). Stored as the raw underlying byte of
  // arrangrr::SectionType (components/core/arrangrr/include/arrangrr/
  // arranger/style_model.hpp) to keep GridModel exactly as core-free as its
  // GridCellKind/scene-name fields already are (D38) -- callers on both
  // sides of the boundary (grid_panel.cpp/preview.hpp/in_process_brain_
  // session.cpp) already share this same "numerically identical, hand-copied
  // literal" discipline (see preview.hpp's own Section enum). Every scene
  // defaults to kDefaultSectionType (SectionType::kVarA == 2 -- the
  // arranger's own default/most-common section, and the value every launch
  // cell preview already hardcoded before this slice), so a fresh grid with
  // no drag-drop yet behaves identically to before. Bounds-checked exactly
  // like scene_name/set_scene_name above: an out-of-range `scene_index` is a
  // no-op for the setter and returns kDefaultSectionType from the getter.
  static constexpr std::uint8_t kDefaultSectionType = 2;
  std::uint8_t scene_section(std::size_t scene_index) const;
  void set_scene_section(std::size_t scene_index, std::uint8_t section);

 private:
  std::size_t index_of(std::size_t part_index, std::size_t scene_index) const;

  std::size_t m_scene_count;
  std::vector<GridCell> m_cells;  // row-major: part_index * m_scene_count + scene_index
  std::array<std::string, kMaxSceneCount> m_scene_names;
  std::array<std::uint8_t, kMaxSceneCount> m_scene_sections;
};

// Section-type wire-name table, numerically/spelling-IDENTICAL to
// in_process_brain_session.cpp's own `parse_section_name` (and components/
// platform/hostrt/shell_parse.cpp's `parse_section()`/event_labels.cpp's
// `section_name()`) -- the exact spellings the `style section <name>` L1
// verb accepts, duplicated deliberately (D38: GridModel/grid_panel.cpp never
// reach into hostrt's own parsing helpers, same discipline every other
// hand-copied literal in this file already uses). Returns an empty view for
// an out-of-range `section` byte (there is no wire verb to send in that
// case).
std::string_view section_wire_name(std::uint8_t section);

}  // namespace sonotron

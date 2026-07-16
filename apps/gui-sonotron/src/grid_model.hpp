#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

// kLoopBuffer (Phase 7, node 6000, the Looper -- docs/proposals/looper-in-
// gui-contract.md §7 item 7) mirrors the core's own arrangrr::ContentKind
// (clip_matrix.hpp:31-41), which has this fifth value in the SAME position;
// a fifth GridCellKind value gives a recorded loop somewhere to live in
// GridModel's own type for the first time.
enum class GridCellKind : std::uint8_t {
  kEmpty,
  kStyleSection,
  kChordSequence,
  kStepTrack,
  kLoopBuffer,
};

// One cell of the matrix: a part row x scene column, holding one of the
// four material kinds the browser offers (§5), or empty. `label` is display
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
  // Meaningful only when `kind == GridCellKind::kLoopBuffer`: the LoopBuffer
  // slot id (arrangrr/loop/loop_buffer.hpp) this cell's recorded loop lives
  // in, core-side -- the note-level peer of what `label` already is for a
  // style-section cell. -1 = none, mirroring LoopBuffer's own "-1 = no
  // shadow" sentinel convention (loop_buffer.hpp's m_shadow_slot) for a
  // consistent "no value" idiom across this boundary.
  int loop_slot_id = -1;
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
  // `loop_slot_id` defaults to -1 (none); a caller registering a
  // GridCellKind::kLoopBuffer cell passes the LoopBuffer slot id explicitly.
  // Passing it for any other `kind` is harmless (it is simply ignored by
  // every reader that checks `kind` first) but not meaningful.
  void set_cell(std::size_t part_index, std::size_t scene_index, GridCellKind kind,
                std::string label, int loop_slot_id = -1);
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

  // Host-side per-scene LENGTH, in bars (auto-song fix: the advance decision
  // and the launch-cell playhead sweep must both use the SAME per-scene
  // length, not the style's own section length, which is what the pre-fix
  // code used and why the sprint was locked to 1 bar per scene for every
  // built-in style). Every scene defaults to kDefaultSceneBars, so a fresh
  // grid loops each column for a musically reasonable stretch before
  // auto-song ever advances it. Bounds-checked exactly like scene_section
  // above: an out-of-range `scene_index` is a no-op for the setter and
  // returns kDefaultSceneBars from the getter. The setter also clamps
  // `bars` to >= 1 -- a zero-or-negative scene length would make the
  // auto-song "elapsed >= length" check trivially and permanently true
  // (the same reasoning preview::section_bars' own header comment gives for
  // why IT never returns <= 0 either).
  //
  // Reserved for a future extension (memory: auto-song-playhead-and-
  // repeats): a per-scene REPEAT COUNT (play K times, or infinite, before
  // advancing) will hook in here, alongside this length, once that decision
  // is made -- not implemented yet.
  static constexpr int kDefaultSceneBars = 8;
  int scene_bars(std::size_t scene_index) const;
  void set_scene_bars(std::size_t scene_index, int bars);

 private:
  std::size_t index_of(std::size_t part_index, std::size_t scene_index) const;

  std::size_t m_scene_count;
  std::vector<GridCell> m_cells;  // row-major: part_index * m_scene_count + scene_index
  std::array<std::string, kMaxSceneCount> m_scene_names;
  std::array<std::uint8_t, kMaxSceneCount> m_scene_sections;
  std::array<int, kMaxSceneCount> m_scene_bars;
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

// Repeat-Zone auto-song advance decision (SLICE 4b, docs/proposals/
// repeat-zone-real-contract.md's TIMING DECISION: GUI-DRIVEN, host-only, NO
// new engine mechanism -- the GUI already tracks the live bar off the
// existing "beat" heartbeat and the `style section` verb is itself
// bar-quantized by the arranger, so the GUI only needs to decide WHICH
// section to request, never WHEN with frame-perfect precision). A pure,
// side-effect-free function of the current auto-song/transport/scene state,
// so the "does the active scene column need to advance now" question
// unit-tests without the GUI event loop.
//
// `auto_song` OFF or `playing` false means the active scene column just
// loops in place (current, pre-auto-song behavior) -- nullopt (stay). ON +
// playing, once `bars_elapsed_in_scene` reaches or passes
// `active_scene_section_bars`, the next scene is
// `(active_scene + 1) % scene_count` -- the song WRAPS around the scene
// sequence rather than stopping at the last column. `active_scene` is
// normalized modulo `scene_count` before advancing, so an out-of-range input
// never indexes past the wrap. `scene_count <= 0` has no scene to wrap into,
// so it is treated the same as "stay" (nullopt).
std::optional<int> next_scene_to_launch(bool auto_song, bool playing, int active_scene,
                                        int scene_count, int bars_elapsed_in_scene,
                                        int active_scene_section_bars);

// Once-per-crossing guard for the auto-song advance check above: ImGui
// re-evaluates every rendered frame, but the live bar (app_state.bar(),
// reduced from the "beat" heartbeat) only changes once per beat-heartbeat
// poll, so re-running next_scene_to_launch() on every frame while the bar
// number is unchanged would otherwise fire the SAME crossing repeatedly.
// Returns true (and updates `last_checked_bar` in place) exactly once per
// distinct `current_bar` value; false on every other call until the bar
// actually changes again.
bool bar_just_advanced(int current_bar, int& last_checked_bar);

// Beat-synchronized per-section PLAYHEAD phase (owner-locked: the launch-
// cell sweep bar must fill 0->100% over the ACTIVE SCENE's own section
// length, driven by the authoritative beat/bar/pulse -- NOT wall-clock time,
// which is what the former neon::sweep_bar(..., fx.time, ...) call drove it
// with, a fixed ~1.7s period with no relation to tempo or the section
// boundary). Pure, side-effect-free, unit-testable -- mirrors next_scene_to_
// launch()'s own "no ImGui, no I/O" discipline above.
//
// `current_bar`/`beat_num`/`pulse` mirror AppState::bar()/beat_num()/pulse()
// exactly (`beat_num` 1-based, `pulse` 0..23 at 24 PPQN); `active_scene_
// start_bar` mirrors V02State::active_scene_start_bar (the live bar the
// active scene became active, grid_panel.cpp's update_auto_song); `beats_
// per_bar` mirrors AppState::beats_per_bar(); `section_bars` mirrors
// preview::section_bars(active_style, active_section).
//
// Returns a value in [0,1] once started, or the sentinel -1.0F ("no
// playhead") when: not started (`current_bar <= 0`), `section_bars <= 0`,
// `beats_per_bar <= 0`, or a bar REWIND (`current_bar < active_scene_start_
// bar` -- a stop/restart cycle mid-scene, mirroring update_auto_song's own
// bar-rewind guard: the anchor is stale until the caller re-arms/re-anchors
// it, and a deeply negative phase would only read as a nonsensical playhead
// jump, not an honest "no position yet").
float section_playhead_phase(int current_bar, int active_scene_start_bar, int beat_num, int pulse,
                             int beats_per_bar, int section_bars);

}  // namespace sonotron

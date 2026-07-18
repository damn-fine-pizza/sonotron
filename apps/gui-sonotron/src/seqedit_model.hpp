#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "step_pattern_model.hpp"
#include "track_roles.hpp"

// Pure-data toolbar/selection state for the Sequence Edit zone
// (ux-workstation.md §4.5/§6). No ImGui, no I/O; seqedit_panel.cpp is the
// only file that renders this with ImGui.
//
// Task #11 Phase 1 (roadmap node 11600/11610, "entrambi a fasi" owner
// decision): the note canvas is now modeled for real, as a STEP SEQUENCER --
// the per-step `Track` buffer edited by `track step <i> <note> <vel> <gate>
// ...` (§7 A7), mirrored host-side by StepPatternStore below. A future
// Phase-2 piano-roll is a richer VIEW over this SAME data (step_pattern_
// model.hpp's own header comment), not a second content kind or a second
// model.

namespace sonotron {

class BrainSession;

enum class SeqEditView : std::uint8_t { kPianoRoll, kStep };

class SeqEditModel {
 public:
  SeqEditModel() { m_role_visible.fill(true); }

  std::size_t part_index() const { return m_part_index; }
  // Clamped into [0, kTrackRoleCount).
  void set_part_index(std::size_t index);
  std::string_view part_label() const { return kTrackRoleLabels[m_part_index]; }

  const std::string& clip_label() const { return m_clip_label; }
  void set_clip_label(std::string label) { m_clip_label = std::move(label); }

  bool record_armed() const { return m_record_armed; }
  void set_record_armed(bool armed) { m_record_armed = armed; }

  // Grid resolution as a "1/N" denominator (e.g. 16 == 1/16), matching the
  // §3 wireframe's "grid 1/16" readout.
  int grid_division() const { return m_grid_division; }
  void set_grid_division(int division) { m_grid_division = division; }

  SeqEditView view() const { return m_view; }
  void set_view(SeqEditView view) { m_view = view; }

  // Per-role BARS-VISIBLE toggle for the Sequence-Edit piano-roll overlay
  // (owner task #1: list every instrument, each individually toggleable).
  // All 9 roles default VISIBLE. An out-of-range `role_index` reads back as
  // visible (the safe default) and a set on an out-of-range index is a
  // silent no-op (never touches a DIFFERENT role than the caller intended)
  // -- same clamp-or-ignore-not-crash discipline set_part_index() already
  // uses above, except a no-op is the right call here (unlike set_part_
  // index's clamp: silently retargeting a DIFFERENT role's visibility would
  // be a worse bug than simply ignoring an out-of-range request).
  //
  // Semantics correction (owner, 2026-07-18, replacing 026e3de's wrong
  // per-lane "x" HIDE-THE-WHOLE-LANE button): this flag NO LONGER decides
  // whether a Sequence-Edit lane EXISTS -- seqedit_panel.cpp's
  // compute_piano_roll_lanes now always lists every one of the 6 launch_
  // rows.hpp kRows roles, so a lane's name label is unconditional. This flag
  // now gates ONLY whether draw_piano_roll_lanes paints that lane's note
  // BARS -- toggled via a checkbox drawn directly under the lane's own name,
  // fully reversible in place. This is a GUI-only host view flag: it never
  // reaches the core, ClipMatrix, playback, or any wire command.
  bool role_visible(std::size_t role_index) const {
    return role_index >= kTrackRoleCount || m_role_visible[role_index];
  }
  void set_role_visible(std::size_t role_index, bool visible) {
    if (role_index < kTrackRoleCount) {
      m_role_visible[role_index] = visible;
    }
  }

  // "all tracks / last track" bulk visibility action (docs/proposals/
  // seqedit-column-view-and-zoom.md Feature B items 3/4): NOT a persistent
  // view mode -- pressing the toggle, or opening a NEW cell, recomputes
  // every role's visibility ONCE (all on, or only `part_index()` on).
  // Individual checkboxes (role_visible/set_role_visible above) still
  // freely override the result afterward; this only tracks which BULK
  // state the toggle button should currently offer next, so its own label
  // reads sensibly -- it does not re-derive "is everything currently on"
  // from the 9 individual flags every frame; that is a deliberate
  // simplification: this is a one-shot trigger, not a mirrored live readout
  // of the fine-grained per-role state.
  bool all_tracks_shown() const { return m_all_tracks_shown; }

  // Sets every role's visibility in one shot: `true` shows all 9; `false`
  // solos the CURRENT part_index() (every other role hidden). Used both by
  // the cell-open default (item 3: solo the clicked role) and the sidebar's
  // own "all tracks / last track" toggle button (item 4).
  void set_all_tracks_visible(bool all);

  // Task #11 Phase 1: the editable StepPatternModel pool the "step" view
  // reads/writes, and grid_panel.cpp's step-track creation gesture appends
  // to. Owned HERE (not GridModel/UiState) because layout_renderer.cpp (the
  // sole threader of WorkstationState into render_grid_panel/render_seqedit_
  // panel) is out of scope for this task, and SeqEditModel is already the
  // one model both entry points receive by non-const reference.
  StepPatternStore& step_tracks() { return m_step_tracks; }
  const StepPatternStore& step_tracks() const { return m_step_tracks; }

  // Which StepPatternStore track (if any) the CURRENTLY OPEN cell (grid_
  // panel.cpp's fx.open_cell) owns -- the SeqEditModel-side counterpart of
  // UiState::open_wav (ui_state.hpp is out of scope for this task, so this
  // lives here instead). -1 = the open cell is not a step-track cell (a
  // style-section cell, or nothing open at all); render_seqedit_panel's own
  // "step" tab is hidden and the kStep canvas branch is inactive whenever
  // this reads < 0.
  int open_step_track() const { return m_open_step_track; }
  void set_open_step_track(int step_track_index) { m_open_step_track = step_track_index; }

  // Task #11 Phase 1: the ONE BrainSession* this model holds, uniquely among
  // the zone models -- render_seqedit_panel(SeqEditModel&, const UiState&)
  // has no BrainSession parameter of its own (layout_renderer.cpp's call
  // site is frozen/out of scope for this task), so the interactive "step"
  // canvas has no other way to send the `track step ...` wire command a
  // click must produce (the anti-no-op requirement: a step toggle has to
  // reach the core, not just this model's own local echo). Wired once from
  // main.cpp (NOT layout_renderer.cpp) right after both objects exist.
  // Nullable and defensively checked by every caller: a SeqEditModel
  // constructed bare (e.g. a unit test) still works for local-only state,
  // it simply sends nothing.
  BrainSession* brain_session() const { return m_brain_session; }
  void set_brain_session(BrainSession* session) { m_brain_session = session; }

 private:
  std::size_t m_part_index = 0;
  std::string m_clip_label = "-";
  bool m_record_armed = false;
  int m_grid_division = 16;
  SeqEditView m_view = SeqEditView::kPianoRoll;
  std::array<bool, kTrackRoleCount> m_role_visible;
  bool m_all_tracks_shown = true;
  StepPatternStore m_step_tracks;
  int m_open_step_track = -1;
  BrainSession* m_brain_session = nullptr;
};

}  // namespace sonotron

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "track_roles.hpp"

// Pure-data toolbar/selection state for the Sequence Edit zone
// (ux-workstation.md §4.5/§6). No ImGui, no I/O; seqedit_panel.cpp is the
// only file that renders this with ImGui.
//
// The actual note canvas (the per-step `Track` buffer edited by `track step
// <i> <note> <vel> <gate> ...`, §7 A7) is NOT modeled here — that is a
// bigger, later slice (a real piano-roll/step grid over live Track data).
// This slice gives the toolbar (part/clip selection, record-arm, grid
// resolution, view mode) a real, testable home so seqedit_panel.cpp has
// state to render and toggle, and an honest placeholder canvas underneath.

namespace sonotron {

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

  // Per-role SHOW/HIDE toggle for the Sequence-Edit overlay (owner task #1:
  // list every instrument on the left, each individually toggleable). All 9
  // roles default VISIBLE. An out-of-range `role_index` reads back as visible
  // (the safe default) and a set on an out-of-range index is a silent no-op
  // (never touches a DIFFERENT role than the caller intended) -- same
  // clamp-or-ignore-not-crash discipline set_part_index() already uses above,
  // except a no-op is the right call here (unlike set_part_index's clamp:
  // silently retargeting a DIFFERENT role's visibility would be a worse bug
  // than simply ignoring an out-of-range request).
  bool role_visible(std::size_t role_index) const {
    return role_index >= kTrackRoleCount || m_role_visible[role_index];
  }
  void set_role_visible(std::size_t role_index, bool visible) {
    if (role_index < kTrackRoleCount) {
      m_role_visible[role_index] = visible;
    }
  }

 private:
  std::size_t m_part_index = 0;
  std::string m_clip_label = "-";
  bool m_record_armed = false;
  int m_grid_division = 16;
  SeqEditView m_view = SeqEditView::kPianoRoll;
  std::array<bool, kTrackRoleCount> m_role_visible;
};

}  // namespace sonotron

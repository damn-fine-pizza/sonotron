#pragma once

#include "grid_model.hpp"
#include "seqedit_model.hpp"
#include "ui_state.hpp"

// Renders the SEQUENCE EDIT zone (v02-workstation-spec.md §3). Declared
// here as pure data/interface (SeqEditModel + UiState + GridModel, no ImGui)
// -- see the *_panel/*_model split invariant.

namespace sonotron {

// Draws the header (part/clip readout + grid + piano-roll/step tabs, all real
// SeqEditModel state, plus a "+ track" popup) and the canvas: bar guides
// plus, when a clip is open (UiState::open_cell), one piano-roll LANE per
// visible launch-grid row (launch_rows.hpp's kRows, filtered by
// SeqEditModel::role_visible) with a green playhead sweeping while it plays.
// `grid` is the SAME GridModel the Repeat Zone (grid_panel.cpp) renders --
// every lane's content is resolved through launch_rows.hpp's shared
// resolve_track_cell_preview() against `grid`'s cell at (role, UiState::
// open_scene), so a given cell's real content (blank/live-step/style-section)
// reads identically in both panels by construction (Aretino review,
// 2026-07-18: this closes a track-set AND content divergence the two panels
// used to have).
void render_seqedit_panel(SeqEditModel& model, const UiState& fx, const GridModel& grid);

}  // namespace sonotron

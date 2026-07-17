#pragma once

#include "seqedit_model.hpp"
#include "ui_state.hpp"

// Renders the SEQUENCE EDIT zone (v02-workstation-spec.md §3). Declared
// here as pure data/interface (SeqEditModel + UiState, no ImGui) -- see the
// *_panel/*_model split invariant.

namespace sonotron {

// Draws the header (part/clip readout + grid + piano-roll/step tabs, all real
// SeqEditModel state) and the canvas: bar guides plus, when a clip is open
// (UiState::open_cell), a 16x8 track-colored piano-roll deterministic from
// the clip label with a green playhead sweeping while it plays. The note
// blocks are procedural -- there is no live `Track` step data on the wire
// (the documented seqedit gap) -- so they are local-only, not a fake readback.
void render_seqedit_panel(SeqEditModel& model, const UiState& fx);

}  // namespace sonotron

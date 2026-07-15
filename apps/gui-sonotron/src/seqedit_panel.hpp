#pragma once

#include "seqedit_model.hpp"
#include "v02_state.hpp"

// Renders the v02 SEQUENCE EDIT zone (v02-workstation-spec.md §3). Declared
// here as pure data/interface (SeqEditModel + V02State, no ImGui) -- see the
// *_panel/*_model split invariant.

namespace sonotron {

// Draws the header (part/clip readout + grid + piano-roll/step tabs, all real
// SeqEditModel state) and the canvas: bar guides plus, when a clip is open
// (V02State::open_cell), a 16x8 track-colored piano-roll deterministic from
// the clip label with a green playhead sweeping while it plays. The note
// blocks are procedural -- there is no live `Track` step data on the wire
// (the documented seqedit gap) -- so they are local-only, not a fake readback.
void render_seqedit_panel(SeqEditModel& model, const V02State& fx);

}  // namespace sonotron

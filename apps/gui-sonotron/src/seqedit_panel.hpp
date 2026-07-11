#pragma once

#include "seqedit_model.hpp"

// Renders the Sequence Edit zone (ux-workstation.md §4.5/§6). Declared here
// as pure data/interface (SeqEditModel only, no ImGui) — see the
// *_panel/*_model split invariant.

namespace sonotron {

// Renders the toolbar (part/clip selector, record-arm, grid-division
// stepper, piano-roll/step view toggle — all REAL, testable SeqEditModel
// state) plus an HONEST PLACEHOLDER note canvas: the piano-roll/step grid
// over live `Track` step data (§7 A7's `track step ...` verb) is a later
// slice, not modeled or rendered here yet.
void render_seqedit_panel(SeqEditModel& model);

}  // namespace sonotron

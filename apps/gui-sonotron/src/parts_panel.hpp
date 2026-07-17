#pragma once

#include "ui_state.hpp"

// Renders the PARTS section of the right rail (v02-workstation-spec.md
// §2c). Declared here as pure data/interface (UiState only, no ImGui) -- see
// the *_panel/*_model split invariant.

namespace sonotron {

// Draws the PARTS divider + the DRUMS/BASS/CHORD "amount" rotary knobs. The
// amount knobs write UiState's local-only intent (there is NO per-part
// amount/volume verb on the wire). Per §2c the rail is amount-only: mute/solo
// is a PER-TRACK verb and lives on the Repeat Zone track rows (§2b), not here.
void render_parts_panel(UiState& fx);

}  // namespace sonotron

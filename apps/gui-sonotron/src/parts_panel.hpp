#pragma once

#include "v02_state.hpp"

// Renders the v02 PARTS section of the right rail (v02-workstation-spec.md
// §2c). Declared here as pure data/interface (V02State only, no ImGui) -- see
// the *_panel/*_model split invariant.

namespace sonotron {

// Draws the PARTS divider + the DRUMS/BASS/CHORD "amount" rotary knobs. The
// amount knobs write V02State's local-only intent (there is NO per-part
// amount/volume verb on the wire). Per §2c the rail is amount-only: mute/solo
// is a PER-TRACK verb and lives on the Repeat Zone track rows (§2b), not here.
void render_parts_panel(V02State& fx);

}  // namespace sonotron

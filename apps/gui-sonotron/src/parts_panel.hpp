#pragma once

#include "brain_session.hpp"
#include "parts_model.hpp"
#include "v02_state.hpp"

// Renders the v02 PARTS section of the right rail (v02-workstation-spec.md
// §2c). Declared here as pure data/interface (PartsModel + BrainSession +
// V02State, no ImGui) -- see the *_panel/*_model split invariant.

namespace sonotron {

// Draws the PARTS divider + the DRUMS/BASS/CHORD "amount" rotary knobs, each
// with compact M/S latches beneath. The amount knobs write V02State's
// local-only intent (there is NO per-part amount/volume verb on the wire); the
// M/S latches send the REAL `part <role> mute|solo on|off` L1 verb (§7 B8),
// preserving that wiring in the v02 surface.
void render_parts_panel(PartsModel& model, BrainSession& brain_session, V02State& fx);

}  // namespace sonotron

#pragma once

#include "app_state.hpp"
#include "v02_state.hpp"

// Renders the v02 INTENTION section of the right rail (v02-workstation-spec.md
// §2c). Declared here as pure data/interface (AppState + V02State, no ImGui) --
// see the *_panel/*_model split invariant.

namespace sonotron {

// Draws, inside the CURRENT ImGui child: the INTENTION title + live tag
// (driven by real AppState::harmony_active()), the FOLLOWS/NEXT chord cards
// (real AppState::chord_followed_current()/chord_followed_next()), the XY pad
// (valence x / energy y), and the ENERGY/TENSION/VALENCE rotary knobs. The XY
// pad and the knobs write V02State's local intent surface -- there is NO
// engine verb for energy/tension/valence, so those are honest local-only
// controls (docs/v02-feature-list.md). The XY pad and the ENERGY/VALENCE knobs
// share the same backing values so they track each other.
void render_intention_panel(const AppState& app_state, V02State& fx);

}  // namespace sonotron

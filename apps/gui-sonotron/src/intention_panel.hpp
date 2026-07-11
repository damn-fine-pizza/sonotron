#pragma once

#include "app_state.hpp"

// Renders the Intention zone (ux-workstation.md §4.7). Declared here as
// pure data/interface (AppState only, no ImGui) — see the *_panel/*_model
// split invariant. No intention_model.* pairs with this panel: it reads
// AppState directly and holds no state of its own (deliberately minimal,
// read-only — the Director, node 10000, is not built).

namespace sonotron {

// Renders the Intention rail inside the CURRENT ImGui window/child: a
// minimal, READ-ONLY view. The live green/amber harmonic visualizer and the
// energy/tension/valence bars await the Director (node 10000) and
// `kChordFollowed` (gap P0-1, ux-workstation.md §11) — neither is built, so
// this renders an honest placeholder (bars pinned at zero) rather than
// inferring fake values. The one REAL signal available today is
// `AppState::harmony_active()` (transport playing, or a chord was just
// steered) — used only to brighten the header text, never to fake
// pitch-class data.
void render_intention_panel(const AppState& app_state);

}  // namespace sonotron

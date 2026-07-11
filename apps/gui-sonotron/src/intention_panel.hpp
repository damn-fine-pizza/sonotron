#pragma once

#include "app_state.hpp"

// Renders the Intention zone (ux-workstation.md §4.7). Declared here as
// pure data/interface (AppState only, no ImGui) — see the *_panel/*_model
// split invariant. No intention_model.* pairs with this panel: it reads
// AppState directly and holds no state of its own (deliberately minimal,
// read-only — the Director, node 10000, is not built).

namespace sonotron {

// Renders the Intention rail inside the CURRENT ImGui window/child: a
// minimal, READ-ONLY view. The live green/amber harmonic visualizer is now
// REAL, fed by the additive `kChordFollowed` event (gap P0-1,
// pipeline-p0-mechanical-plan.md): GREEN = the chord followed this bar
// (`AppState::chord_followed_current()`, lit only while `harmony_active()`),
// AMBER = the shift-staged chord pending for next bar
// (`chord_followed_next()`), colour semantics per ux-workstation.md §10. The
// energy/tension/valence bars still await the Director (node 10000) — no
// signal exists for those yet, so they stay an honest placeholder (pinned at
// zero) rather than inferring fake values.
void render_intention_panel(const AppState& app_state);

}  // namespace sonotron

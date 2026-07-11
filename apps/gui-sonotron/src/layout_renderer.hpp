#pragma once

#include "layout_model.hpp"
#include "workstation_state.hpp"

// ImGui rendering for a `Layout`, kept separate from both the pure-data
// model (layout_model.hpp) and the JSON reader/writer (layout_json.hpp).
// This is the only file besides the individual `*_panel.cpp` files allowed
// to include ImGui headers.

namespace sonotron {

// Draws `layout`'s zones inside the CURRENT ImGui window (the caller is
// responsible for the surrounding ImGui::Begin/End). Zones are positioned
// and sized from `compute_rows()`'s normalized fractions against the
// available content region, so the grid rescales with the window on every
// frame.
//
// Each zone is dispatched by `Zone::id` to its live panel (transport_panel,
// browser_panel, grid_panel, seqedit_panel, parts_panel, intention_panel —
// G3, see docs/design/gui-fase2-mechanical-plan.md): this is the ONE
// dispatch point, so panels never need to know about the layout grid
// itself. `state` bundles the app/brain state and the per-zone models the
// panels read/mutate (workstation_state.hpp). A zone id with no matching
// panel (a future/unknown zone) still renders as an empty titled frame —
// the G1 fallback — so a stale or hand-edited layout.json never crashes.
void render_layout(const Layout& layout, WorkstationState& state);

}  // namespace sonotron

#pragma once

#include "grid_model.hpp"

// Renders the Repeat Zone / Live-Loops launch grid (ux-workstation.md
// §4.4/§5). Declared here as pure data/interface (GridModel only, no
// ImGui) — see the *_panel/*_model split invariant.

namespace sonotron {

// Renders the matrix inside the CURRENT ImGui window/child: one row per
// part (GridModel::part_label), one column per scene, plus a "+" to add a
// scene. Each cell is a real ImGui drop target for a browser style drag
// (browser_panel.hpp's kStyleDragPayloadId) — dropping sets the cell's
// content for real (GridModel::set_cell). LAUNCH stays an honest
// placeholder: the per-cell/per-scene launch button is disabled
// (kGridLaunchWired == false, grid_model.hpp) with a tooltip naming the gap,
// rather than a click that silently does nothing.
void render_grid_panel(GridModel& model);

}  // namespace sonotron

#pragma once

#include "arrangement.hpp"

// ImGui rendering for the Arrangement zone — a streamgraph of the ensemble
// flowing in musical time. Kept separate from the pure-data model
// (arrangement.hpp), like the other panels. Only this file, intention_panel
// and layout_renderer include ImGui headers.

namespace sonotron {

// Draws the arrangement streamgraph inside the CURRENT ImGui window/child
// (the caller owns the surrounding frame): a section strip on top, the
// stacked flowing lanes with NOW pinned ~1/3 from the left and the future
// region ghosted, and a horizon (zoom) slider along the bottom. A dumb view:
// all data comes from `arr`; the only state it owns is the ephemeral horizon
// zoom (a view setting, not model data).
void render_arrangement_panel(const Arrangement& arr);

}  // namespace sonotron

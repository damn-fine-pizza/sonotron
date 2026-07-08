#pragma once

#include "layout_model.hpp"

// ImGui rendering for a `Layout`, kept separate from both the pure-data
// model (layout_model.hpp) and the JSON reader/writer (layout_json.hpp).
// This is the only one of the three files allowed to include ImGui headers.

namespace sonotron {

// Draws `layout`'s zones as empty, titled frames inside the CURRENT ImGui
// window (the caller is responsible for the surrounding ImGui::Begin/End).
// Zones are positioned and sized from `compute_rows()`'s normalized
// fractions against the available content region, so the grid rescales
// with the window on every frame. No live content is drawn — title, a
// separator, and the empty frame only.
void render_layout(const Layout& layout);

}  // namespace sonotron

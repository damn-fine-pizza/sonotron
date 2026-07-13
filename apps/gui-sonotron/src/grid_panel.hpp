#pragma once

#include "brain_session.hpp"
#include "grid_model.hpp"

// Renders the Repeat Zone / Live-Loops launch grid (ux-workstation.md
// §4.4/§5). Declared here as pure data/interface (GridModel only, no
// ImGui) — see the *_panel/*_model split invariant.

namespace sonotron {

// Renders the matrix inside the CURRENT ImGui window/child: one row per
// part (GridModel::part_label), one column per scene, plus a "+" to add a
// scene. Each cell is a real ImGui drop target for a browser style drag
// (browser_panel.hpp's kStyleDragPayloadId) — dropping sets the cell's
// content for real (GridModel::set_cell). LAUNCH is wired to the real core
// clip primitive (Phase-5 Item #2, docs/design/clip-primitive-design.md):
// clicking a cell sends `launch clip <id> quantize <n>` (id = part_index *
// scene_count() + scene_index, mirroring the cell's own ImGui PushID);
// clicking a scene header's "▶" fans out `launch scene <n> quantize <q>`.
// `brain_session` mirrors render_styles_branch's own BrainSession& param.
void render_grid_panel(GridModel& model, BrainSession& brain_session);

}  // namespace sonotron

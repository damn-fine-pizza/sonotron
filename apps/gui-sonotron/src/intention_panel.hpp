#pragma once

#include "intention.hpp"

// ImGui rendering for the Intention zone, kept separate from the pure-data
// model (intention.hpp) exactly like layout_renderer.hpp is split from
// layout_model.hpp. Only this file and layout_renderer.cpp are allowed to
// include ImGui headers.

namespace sonotron {

// Draws the Intention "podium" — the energy/tension/valence axes, each as a
// current->target bar — inside the CURRENT ImGui window/child (the caller
// owns the surrounding frame). A dumb view: all data comes from `state`.
void render_intention_panel(const IntentionState& state);

}  // namespace sonotron

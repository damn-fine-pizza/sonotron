#pragma once

#include "brain_session.hpp"
#include "browser_model.hpp"
#include "v02_state.hpp"

// Renders the v02 BROWSER zone (v02-workstation-spec.md §2a). Declared here as
// pure data/interface (BrowserModel + BrainSession + V02State, no ImGui) --
// see the *_panel/*_model split invariant.

namespace sonotron {

// Draws the searchable tree (styles / variations / kits · GM / clips) inside
// the CURRENT ImGui child, with the search field pinned at the bottom. Every
// style row is a real drag source (kStyleDragPayloadId) AND a real load
// (`style load <name>`, a shipped L1 verb) -- clicking one also marks it the
// active row (V02State::active_style, a local highlight; there is no
// selection readback on the wire). Variations / kits are local-only lists (no
// section/kit-load verb is wired from here); clips is an honest empty branch.
void render_browser_panel(BrowserModel& model, BrainSession& brain_session, V02State& fx);

}  // namespace sonotron

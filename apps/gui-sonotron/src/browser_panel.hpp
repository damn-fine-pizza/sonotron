#pragma once

#include "app_state.hpp"
#include "brain_session.hpp"
#include "browser_model.hpp"
#include "ui_state.hpp"

// Renders the BROWSER zone (v02-workstation-spec.md §2a). Declared here as
// pure data/interface (BrowserModel + BrainSession + UiState + AppState, no
// ImGui) -- see the *_panel/*_model split invariant.

namespace sonotron {

// Draws the searchable tree for every category currently toggled visible
// (styles / variation / sounds / kits-GM / clips), gated by a wrapping
// toggle-label bar (BrowserModel::category_visible, browser-redesign-
// taxonomy.md Phase 1) rather than a single-select combo -- MULTIPLE
// categories' trees can be visible simultaneously, stacked inside the CURRENT
// ImGui child (only styles is visible by default), with the search field
// pinned at the bottom and scoped to whichever category was most recently
// toggled (BrowserModel::search_filter/filter_for keep each category's own
// filter isolated). Every style row is a real
// drag source (kStyleDragPayloadId) AND a real load (`style load <name>`, a
// shipped L1 verb) -- clicking one also marks it the active row (UiState::
// active_style, a local highlight; there is no selection readback on the
// wire). Every variations row now also sends `style section <wire_name>` on
// click (a standalone "switch section now", independent of grid_panel.cpp's
// own scene-header mechanism) while remaining a drag source for that scene
// header. Voices sends `program <port>[:ch] <voice>` to a small destination
// picker (port + 1-based channel) held on the model. Kits stays a local-only
// list (no kit-load verb is wired from here); clips is an honest empty
// branch.
//
// `app_state` (owner task #2): while playing, a click sends `style switch`
// instead of `style load` -- it now also passes the engine's own current
// section (app_state.section(), the authoritative last-committed kSection
// echo) as an explicit `section <name>` suffix, so switching styles PRESERVES
// the active section instead of always reverting to varA (see in_process_
// brain_session.cpp's command_line_to_command for the wire-level rationale).
void render_browser_panel(BrowserModel& model, BrainSession& brain_session,
                          const AppState& app_state, UiState& fx);

}  // namespace sonotron

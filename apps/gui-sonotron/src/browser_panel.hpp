#pragma once

#include "brain_session.hpp"
#include "browser_model.hpp"

// Renders the Browser zone (ux-workstation.md §4.3). Declared here as pure
// data/interface (BrowserModel + BrainSession only, no ImGui) — see the
// *_panel/*_model split invariant.

namespace sonotron {

// Renders the searchable Styles / Clips / MIDI seqs tree inside the CURRENT
// ImGui window/child. Every style entry is BOTH a real ImGui drag source
// (payload id kStyleDragPayloadId, browser_model.hpp — the drop target is
// grid_panel.cpp's matrix cells) AND a real load: clicking a style sends
// `style load <name>` (§7 A1, already-shipped L1 verb) — not a placeholder.
// Clips / MIDI seqs render as an honest "(none authored yet)" line
// (BrowserModel does not invent entries — no clip primitive, no recorder UI
// exists yet).
void render_browser_panel(BrowserModel& model, BrainSession& brain_session);

}  // namespace sonotron

#pragma once

#include "brain_session.hpp"
#include "parts_model.hpp"

// Renders the Parts / Mixer zone (ux-workstation.md §4.6). Declared here as
// pure data/interface (PartsModel + BrainSession only, no ImGui) — see the
// *_panel/*_model split invariant.

namespace sonotron {

// Renders one row per part inside the CURRENT ImGui window/child: name,
// real Mute/Solo toggles (sends the shipped `part <role> mute|solo on|off`
// L1 verb, §7 B8 — not a placeholder) and a GM-program readout that stays
// "--" (honest: no per-part program readback exists yet, §11.4).
void render_parts_panel(PartsModel& model, BrainSession& brain_session);

}  // namespace sonotron

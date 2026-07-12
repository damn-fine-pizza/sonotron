#pragma once

#include "app_state.hpp"
#include "brain_session.hpp"

// Renders the Transport zone (ux-workstation.md §4.2). Declared here as pure
// data/interface (AppState + BrainSession only, no ImGui) so nothing but
// transport_panel.cpp itself needs to include ImGui — see the *_panel/
// *_model split invariant (docs/design/gui-fase2-mechanical-plan.md).

namespace sonotron {

// Renders the transport strip inside the CURRENT ImGui window/child (the
// caller — layout_renderer.cpp — is responsible for the surrounding
// BeginChild/EndChild): connection status (replaces the G2 stopgap
// wholesale, now owning the whole row), Play/Stop/Panic (real L1 verbs:
// `transport start|stop`, `panic`), the current section readout, and an
// live bar·beat readout — ux-workstation.md §11 P0-2's `kBeat` heartbeat
// is now on the wire (brain_event.hpp decodes BrainEvent::Kind::kBeat and
// app_state reduces it into bar/beat/pulse), so this shows the real moving
// position while the transport runs, parked at rest instead of a fake one.
void render_transport_panel(AppState& app_state, BrainSession& brain_session);

}  // namespace sonotron

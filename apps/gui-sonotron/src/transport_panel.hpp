#pragma once

#include "app_state.hpp"
#include "brain_session.hpp"
#include "v02_state.hpp"

// Renders the v02 TRANSPORT RACK (v02-workstation-spec.md §1). Declared here
// as pure data/interface (AppState + BrainSession + V02State, no ImGui) so
// nothing but transport_panel.cpp itself includes ImGui -- the *_panel/*_model
// split invariant (docs/design/gui-fase2-mechanical-plan.md).

namespace sonotron {

// Draws the single-row transport rack inside the CURRENT ImGui child: the
// cyan wordmark, the Play/Stop/Panic neon pad buttons (real L1 verbs
// `transport start|stop`, `panic`), the tempo/key/transpose inset (BPM +
// transpose nudge send `bpm <n>` / `transpose <n>`; 4/4 and key are
// local-only display, no readback on the wire), the REAL bar:beat:pulse
// readout (from the `kBeat` heartbeat reduced into AppState), the status dot,
// and the ⚙ button that toggles the global glow flag (V02State::glow).
void render_transport_panel(AppState& app_state, BrainSession& brain_session, V02State& fx);

}  // namespace sonotron

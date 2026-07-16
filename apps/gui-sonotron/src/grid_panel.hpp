#pragma once

#include "app_state.hpp"
#include "brain_session.hpp"
#include "grid_model.hpp"
#include "parts_model.hpp"
#include "seqedit_model.hpp"
#include "v02_state.hpp"

// Renders the v02 REPEAT ZONE launch grid (v02-workstation-spec.md §2b).
// Declared here as pure data/interface (models + BrainSession + V02State, no
// ImGui) -- see the *_panel/*_model split invariant.

namespace sonotron {

// Draws the 6-track x 5-scene launch grid inside the CURRENT ImGui child:
// track labels, scene headers (click = `launch scene <n> quantize 1`), and
// launch cells with procedural mini clip previews + an L->R sweep on the
// playing cell. Clicking a FILLED cell sends `launch clip <id> quantize 1`
// (real verb) AND opens the clip into Sequence Edit (`seqedit`); clicking an
// EMPTY cell fills it with a local demo clip (no launch, no verb, no
// ClipMatrix registration -- only a browser style drop registers for real,
// repeat-zone-real-contract.md §3/§8b decision 2). Per-cell PLAYING/ARMED/
// QUEUED-STOP state is a REAL readback (repeat-zone-real-contract.md §3):
// read from `app_state`'s per-clip map, reduced from the core's own "clip"
// OutEvent -- not a local click-time guess. Each track row also carries M/S
// latches in its label column, wired to the REAL `part <role> mute|solo on/off`
// L1 verb (§7 B8) through `parts` -- they share PartsModel state with the rail
// mute/solo, and drive the standard solo-implies-others-muted dim in the grid.
void render_grid_panel(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, const AppState& app_state, V02State& fx);

}  // namespace sonotron

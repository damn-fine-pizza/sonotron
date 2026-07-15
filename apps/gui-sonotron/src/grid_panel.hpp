#pragma once

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
// EMPTY cell fills it with a local demo clip (no launch, no verb -- there is
// no clip primitive content binding on the wire yet, grid_model.hpp's gap).
// The per-row single-playing echo (V02State::row_playing) is local: there is
// no per-cell playing readback on the wire. Each track row also carries M/S
// latches in its label column, wired to the REAL `part <role> mute|solo on|off`
// L1 verb (§7 B8) through `parts` -- they share PartsModel state with the rail
// mute/solo, and drive the standard solo-implies-others-muted dim in the grid.
void render_grid_panel(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, V02State& fx);

}  // namespace sonotron

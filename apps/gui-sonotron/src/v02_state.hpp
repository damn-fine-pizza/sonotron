#pragma once

#include <array>
#include <cstddef>

// Client-side, engine-free UI state introduced by the v02 workstation
// redesign (v02-workstation-spec.md). Everything here is HONEST LOCAL STATE:
// a display preference (glow), a per-frame clock/gate the renderer sets, and
// the design's "intent surface" values (energy/tension/valence, per-part
// amount) that have NO BrainSession verb on the wire yet -- so they are
// deliberately local-only, never a faked round-trip (see docs/v02-feature-
// list.md for the per-element [wired]/[local-only]/[gap] tags). Pure data, no
// ImGui / model dependency, so it threads through WorkstationState next to the
// other pure-data models without widening who includes ImGui.

namespace sonotron {

struct V02State {
  // Global glow flag toggled by the transport ⚙ button (default ON). Every
  // neon glow honors it. Local display preference only.
  bool glow = true;

  // Set once per frame by the renderer from ImGui's frame clock and the real
  // transport state, so the panels drive animations off one shared source
  // without each calling ImGui::GetTime() / reaching into AppState.
  float time = 0.0F;
  bool playing = false;

  // Launch-cell edge length in px (v02 zoom -/+, 34..88).
  float cell_zoom = 52.0F;

  // Browser active-style echo: index into kBuiltinStyleNames of the last style
  // this client sent `style load` for (-1 = none). Purely a local highlight --
  // there is no style-selection readback on the wire.
  int active_style = -1;

  // Intention intent surface (0..1). No verb exists -> local-only. The XY pad
  // and the ENERGY/VALENCE knobs share `energy`/`valence` so they track each
  // other; TENSION is knob-only.
  float energy = 0.62F;
  float tension = 0.34F;
  float valence = 0.55F;

  // Parts "amount" knobs DRUMS/BASS/CHORD (0..1). No per-part amount verb on
  // the wire (`part <role>` takes only mute/solo) -> local-only.
  std::array<float, 3> part_amount = {0.72F, 0.60F, 0.50F};

  // Repeat-Zone selection/launch bookkeeping. `open_cell` is the clip currently
  // opened into Sequence Edit (-1 = none). `row_playing[r]` is the scene column
  // playing on track row r (-1 = none) -- ONE playing clip per row, a local
  // echo of the last launch this client sent (there is no per-cell playing
  // readback on the wire, grid_model.hpp's documented gap).
  int open_cell = -1;
  static constexpr std::size_t kGridRows = 6;
  std::array<int, kGridRows> row_playing = {-1, -1, -1, -1, -1, -1};

  // The label of the opened clip, mirrored for the Sequence Edit header/canvas.
  // Empty means "nothing open".
  int open_row = -1;

  // Seeded-once guard: the renderer fills a demo clip pattern into the grid on
  // the first frame so the procedural previews have something to show (the same
  // local content path a browser drag uses; launching still sends real verbs).
  bool seeded = false;
};

}  // namespace sonotron

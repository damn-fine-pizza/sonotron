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

  // Repeat-Zone selection bookkeeping. `open_cell` is the clip currently
  // opened into Sequence Edit (-1 = none). Per-cell PLAYING state itself is no
  // longer tracked here (was `row_playing`, a local click-time echo) -- the
  // "clip" OutEvent is real per-clip readback (repeat-zone-real-contract.md
  // §3), so grid_panel.cpp now reads it straight off AppState instead of
  // mirroring a local guess.
  int open_cell = -1;

  // The label of the opened clip, mirrored for the Sequence Edit header/canvas.
  // Empty means "nothing open".
  int open_row = -1;

  // True when the currently opened clip lives on the pad row (grid_panel.cpp's
  // kRows[...].audio) -- the only audio row. Lets Sequence Edit render a
  // waveform for that clip instead of a MIDI piano-roll (all other rows are
  // MIDI). Set alongside open_row/open_cell at every cell-open site.
  bool open_audio = false;

  // Seeded-once guard: the renderer fills a demo clip pattern into the grid on
  // the first frame so the procedural previews have something to show (the same
  // local content path a browser drag uses; launching still sends real verbs).
  bool seeded = false;

  // Inline scene-header rename (repeat-zone-real-contract.md §4/§8b decision
  // 3): -1 when no scene header is being edited, else the scene index whose
  // header currently shows an ImGui InputText instead of its stored name.
  // `rename_buffer` holds the in-progress edit; `rename_focus_pending` tells
  // grid_panel.cpp to call ImGui::SetKeyboardFocusHere() exactly once, the
  // frame the InputText widget first appears (double-click sets both
  // `renaming_scene` and this flag together).
  int renaming_scene = -1;
  std::array<char, 32> rename_buffer{};
  bool rename_focus_pending = false;
};

}  // namespace sonotron

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Client-side, engine-free UI state introduced by the neon workstation
// redesign (v02-workstation-spec.md). Everything here is HONEST LOCAL STATE:
// a display preference (glow), a per-frame clock/gate the renderer sets, and
// the design's "intent surface" values (energy/tension/valence, per-part
// amount) that have NO BrainSession verb on the wire yet -- so they are
// deliberately local-only, never a faked round-trip (see docs/feature-list.md
// for the per-element [wired]/[local-only]/[gap] tags). Pure data, no
// ImGui / model dependency, so it threads through WorkstationState next to the
// other pure-data models without widening who includes ImGui.

namespace sonotron {

struct UiState {
  // Global glow flag toggled by the transport ⚙ button (default ON). Every
  // neon glow honors it. Local display preference only.
  bool glow = true;

  // Set once per frame by the renderer from ImGui's frame clock and the real
  // transport state, so the panels drive animations off one shared source
  // without each calling ImGui::GetTime() / reaching into AppState.
  float time = 0.0F;
  bool playing = false;

  // Launch-cell edge length in px (zoom -/+, 34..88).
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

  // The opened cell's OWN scene column's SectionType byte (repeat-zone-
  // real-contract.md SLICE 4a) -- set alongside open_cell/open_row at every
  // cell-open site in grid_panel.cpp (GridModel::scene_section(scene)), and
  // read by seqedit_panel.cpp's own preview_for() call so the Sequence Edit
  // canvas previews the SAME column/section the launch-cell mini-preview
  // does. Meaningless while open_cell < 0 (nothing open); defaults to
  // GridModel::kDefaultSectionType's numeric value so an unopened read is
  // still a sane section rather than garbage.
  std::uint8_t open_section = 2;

  // True when the currently opened clip lives on the pad row (grid_panel.cpp's
  // kRows[...].audio) -- the only audio row. Set alongside open_row/open_cell
  // at every cell-open site. Repeat-zone-real-contract.md "cell preview made
  // real" pass: no longer selects a different Sequence Edit render path --
  // pad has no real audio content yet, so it renders its real MIDI note
  // pattern like every other row (seqedit_panel.cpp). Kept for genuine future
  // audio content (a real captured LoopBuffer waveform).
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

  // Auto-song (repeat-zone-real-contract.md SLICE 4b, GUI-DRIVEN, host-only
  // -- NO new engine mechanism, NO new ABI verb). `auto_song` is the header
  // toggle's own state. `active_scene` is the Repeat-Zone COLUMN currently
  // considered "the song's current scene" -- updated both by auto-song's own
  // advance and by any manual scene-header launch (grid_panel.cpp), so
  // auto-song always measures from the most recent real launch, automatic or
  // not. `active_scene_start_bar` is the live bar (AppState::bar()) at which
  // `active_scene` became active; `bars_elapsed_in_scene` for the pure
  // next_scene_to_launch() decision is always `AppState::bar() -
  // active_scene_start_bar`, computed fresh at the call site rather than
  // stored here. `auto_song_last_bar` is the once-per-crossing guard's own
  // bookkeeping (grid_model.hpp's bar_just_advanced) -- defaults to -1 so the
  // very first live bar (0) still counts as "not yet evaluated".
  //
  // DEFAULT ON (owner decision 2026-07-17): auto_song starts armed. The
  // still-live "I always have to click to advance" bug was that NOTHING in
  // production armed auto_song -- only the near-invisible header toggle did,
  // and pressing master Play never touched it, so the active scene looped
  // forever. Every auto-song test set this true by hand, so they stayed green
  // while the real Play path advanced nothing. Starting armed means Play makes
  // the song advance through its scenes on its own; the header toggle now only
  // turns it OFF for a deliberate fixed single-scene loop.
  bool auto_song = true;
  int active_scene = 0;
  int active_scene_start_bar = 0;
  int auto_song_last_bar = -1;

  // MASTER PLAY launch guard (owner issue a, grid_panel.cpp's
  // handle_master_play_launch): true once the active scene column has been
  // launched for the transport run currently in progress, so a Play press
  // launches it exactly once rather than re-firing on every subsequent
  // frame. Cleared the instant the transport is observed NOT playing
  // (AppState::transport() != kPlaying), re-arming for the next Play.
  bool master_play_launched = false;

  // ENDING button suppression latch (roadmap task #37, transport_panel.cpp's
  // dedicated ENDING pad): set the instant the button sends `style section
  // ending1` while playing. grid_panel.cpp's update_auto_song reads this to
  // suppress its own `style section <next>` advance for as long as the cued
  // Ending is still playing out -- otherwise auto-song would override the
  // manual cue at the next section boundary and the song would never
  // actually stop (Arranger::on_tick's section_is_ending one-shot stop only
  // fires if nothing else re-arms the section first). Cleared the instant the
  // transport is observed NOT playing (mirrors master_play_launched's own
  // reset shape), re-arming cleanly for the next Play/song run.
  bool ending_cued = false;

  // ENDING-vs-in-flight-clip-arm race guard (roadmap task #37, Torquato QA
  // finding): true once grid_panel.cpp's update_auto_song has already
  // cancelled the active scene column's own clip arms for the CURRENT ending
  // cue (see cancel_active_scene_clip_arms's own header comment for the full
  // race this closes). A once-per-cue latch, not a per-frame resend: sending
  // `stop clip <id>` again on every subsequent frame while ending_cued stays
  // true would be harmless but wasteful. Cleared alongside ending_cued the
  // instant the transport is observed NOT playing, so the next Ending cue
  // re-arms this guard cleanly.
  bool ending_clip_arms_cancelled = false;
};

}  // namespace sonotron

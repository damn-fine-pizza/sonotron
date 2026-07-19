# Repeat Zone zoom + Sequence Edit column view

Status: PROPOSED (2026-07-18). Two GUI features requested by the owner,
QUEUED behind the song-mode Phase 1 work ([[song-mode-scenechain-adoption]])
because both touch `grid_panel.cpp` + `ui_state.hpp`, the same files that
workstream is editing. Scoping verified (Explore pass 2026-07-18).

## Feature A — Repeat Zone zoom: +3 max notches + mild font scaling

Current: `UiState::cell_zoom` (ui_state.hpp:30) is a continuous px cell edge,
default 52, clamped `[34, 88]` in steps of `9.0F` by the `-`/`+` buttons
(grid_panel.cpp:429-435). Cell size is computed straight from `cell_zoom`
(grid_panel.cpp:1232 → draw_cell → InvisibleButton `cz × cz`); there is NO
level array. The `88.0F` max is a bare literal duplicated in both clamp calls.

To do:
- **+3 notches** = raise the max by `3 × 9 = 27` → `88 → 115`. Change BOTH
  clamp calls. Introduce named constants (`kCellZoomMin=34`, `kCellZoomMax=115`,
  `kCellZoomStep=9`) to kill the duplicated-literal smell. Update the stale
  "34..88" comments (ui_state.hpp:30, grid_panel.cpp:117, 1064).
- **Fonts scale mildly with zoom** (owner: "anche i font si ingrandiscono
  leggermente ad ogni livello di zoom, sii equilibrato"). The cell text (mini-
  preview labels, `▷ wlk` etc., role labels, stepper digits) grows with zoom
  but SUB-LINEARLY — milder than the cell. Suggested factor: font scale =
  `clamp(sqrt(cell_zoom / 52), ~0.85, ~1.3)` or an equivalent gentle ramp, so
  at default 52px scale≈1.0 and at max 115px the text is noticeably-but-not-
  proportionally larger. Applied via a scaled push at the cell-text draw sites.

## Feature B — click a Repeat Zone cell → its whole COLUMN in Sequence Edit

Owner refined spec (2026-07-18, with screenshot):

1. **Click a cell** → the whole Repeat Zone **column highlights**, and Sequence
   Edit loads that column's WHOLE sequence with all its tracks OVERLAID.
2. **Sequence Edit left sidebar lists ALL tracks** (today only ~3 are visible —
   Drums/Perc/Bass), each with a **visibility checkbox**.
3. **On cell-click, only the clicked cell's track checkbox is ON**; every other
   track is deselected (its bars hidden).
4. **Add a toggle button "all tracks / last track"** — flips between showing
   all tracks and showing only the last/clicked one.
5. **Checking a track's checkbox reveals that track's bars** in Sequence Edit.
6. The checkboxes govern **VISIBILITY of tracks within Sequence Edit** — NOT a
   real audio mute (owner-locked decision). (Real mute/solo stays the grid's
   `M`/`S` squares via `PartsModel` + the `part <role> mute` verb, unchanged.)
7. **WAV branch**: if the clicked cell holds a WAV/audio clip (not a MIDI/
   sequence) → Sequence Edit shows the **WAV/sound preview**
   (`neon::clip_preview_waveform`, currently defined-but-uncalled) instead of
   the piano-roll overlay.
8. **FUTURE direction (keep the architecture open, do NOT build yet):** Sequence
   Edit becomes an EDITOR — add / remove / move / edit bars. Today it is a
   READ-ONLY piano-roll/step visualizer fed by `preview::preview_for(...)`.
   Real edit support is TBD; design the column-view so an editable model can
   slot in later.

### What already exists (reuse)
- Sequence Edit (`seqedit_panel.cpp`) ALREADY overlays all 9 core roles for the
  opened cell's section and ALREADY renders one visibility checkbox per role
  (`render_role_toggle_sidebar`, `SeqEditModel::role_visible/set_role_visible`,
  all default `true`). Multi-bar already handled. The shared real-content
  resolver `preview::preview_for(style, section, role)` is the single source of
  truth (also feeds the launch-cell mini-preview).
- Grid cell click already opens ONE role into Sequence Edit
  (`render_track_cell`, grid_panel.cpp:1141-1162) — it just discards the column
  index `s` and sets only `open_cell`/`open_row`/`open_section`.

### What's missing (to build)
- A stored **selected-column index** in `UiState` (today `s` is discarded).
- A **whole-column highlight** in the Repeat Zone grid (today only the scene-
  header text tint tracks `active_scene`).
- Sidebar must list **all tracks** (today the panel shows only ~3 — verify
  whether that is a height/scroll limit or a rendered subset) — likely needs a
  taller/scrollable sidebar.
- **Default-visibility on open = only the clicked role ON, all others OFF**
  (today they all default ON) — set from the click handler.
- The **"all tracks / last track" toggle** button + state.
- The **WAV branch**: nothing creates `kLoopBuffer`/audio cells today and
  `clip_preview_waveform` has no caller; wire `cell.kind`/`fx.open_audio` →
  waveform. (May stay dormant until real audio cells exist.)

## Sequencing
Both land AFTER song-mode Phase 1. A and B are independent of each other but
both serialize behind Phase 1 on `grid_panel.cpp`/`ui_state.hpp`. A is small
(zoom clamp + font ramp). B is larger and mostly UI-plumbing over existing
seqedit machinery; its editable-Sequence-Edit future is explicitly out of scope
for the first cut.

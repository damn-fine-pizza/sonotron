# Sonotron v02 Workstation — feature list

Status: living checklist for the v02 redesign of `apps/gui-sonotron`. Every
element the v02 reference (`Sonotron v02 Workstation.dc.html`, digested into
`design-ref-v02/v02-workstation-spec.md`) shows is enumerated below with an
honesty tag:

- `[wired]`   — backed by a real BrainSession L1 verb; the click/drag actually
                reaches the engine.
- `[local-only]` — honest client-side UI state / animation with no engine verb
                yet; it looks live but sends nothing (flagged so nobody mistakes
                it for a round-trip).
- `[new-widget]` — needs a new ImGui draw-list widget built for this redesign.
- `[gap]`    — needs engine/model support that does not exist on the wire today.

The wired L1 verb surface (from `in_process_brain_session.cpp`'s
`command_line_to_command`, mirrored by hostrt's Shell) is exactly:
`transport start|stop|continue`, `panic`, `style load <name>`,
`transpose <-12..12>`, `bpm <20..400>`, `pad bank <n>`,
`part <role> mute|solo on|off`, `launch clip <id> quantize <n>`,
`launch scene <n> quantize <q>`, `stop clip <id> [quantize <n>]`. There is NO
verb for energy/tension/valence, per-part "amount", groove/arp amount, or a
live BPM/dB/level readback — those are `[local-only]` or `[gap]` below.

## Global frame
- 3 fixed vertical bands (transport 58px / working row flex / sequence edit
  172px), no horizontal scroll, each inner body scrolls only itself. `[wired]`
  reflow (done in `layout_renderer.cpp`).
- Neon background: two radial glows (top-right cyan, bottom-left teal) + a 40px
  grid overlay behind everything. `[new-widget]` (`neon::background`). Done.
- JetBrains Mono NL 13px base (already vendored). Done.
- Global `glow` flag toggled by the ⚙ button (default ON); every glow honors it.
  `[local-only]` (a client display preference, `V02State::glow`). Done.
- `density` comfortable/compact — NOT implemented (nice-to-have per spec). `[gap]`

## Palette (v02 neon)
- cyan `#22e0e6`, pink `#ff3ea5`, green `#3dffa0`, amber `#ffc24d`,
  blue `#2ea8ff`, lead purple `#c678dd`, text `#cfe0e6`, backgrounds/borders
  per spec. Done in `theme.hpp`/`theme.cpp`. `[wired]` (pure style data).

## 1. Transport rack (58px)
- Wordmark `sonotron` cyan-glow + pink `_`. `[local-only]` text. Done.
- Play `▶` → `transport start`. `[wired]`. Done.
- Stop `■` → `transport stop`. `[wired]`. Done.
- Panic `◉` → `panic`. `[wired]`. Done.
- Rounded 38×34 neon pad buttons w/ hover brighten + glow. `[new-widget]`. Done.
- Inset cluster: big BPM number + `BPM`; nudged by wheel/Up-Down over the
  readout → `bpm <n>`. `[wired]` (send) but the DISPLAYED value is local intent,
  not a readback. `[local-only]` display / `[wired]` send. Done.
- `4/4` time-sig readout. `[local-only]` (no time-sig on the wire). Done.
- `key Cm` (Cm green). `[local-only]` (no key readback; `BrainSnapshot.key`
  unwired). Done.
- `transp ±N` → `transpose <n>` on nudge. `[wired]` send / `[local-only]`
  display. Done.
- Bar:beat:pulse readout (`— : — : ··` when stopped). Driven by the REAL
  `kBeat` heartbeat reduced into `AppState::bar()/beat_num()/pulse()`.
  `[wired]`. Done.
- Status dot + label (green "playing" blinking / grey "stopped"). Driven by real
  transport state; blink animation `[local-only]`. Done.
- ⚙ options button toggles `glow`. `[local-only]`. Done.

## 2. Working row
### 2a. Browser (210px)
- `BROWSER` pink title. Done.
- styles section: 16 style names, click → `style load <name>`, active row =
  cyan text + left border + wash. `[wired]` (send + active echo local). Done.
- variations (intro/verse A/…/outro). `[local-only]` list (no section-select
  verb wired from here). Done.
- kits · GM (acoustic kit/808/909/…). `[local-only]` list (no kit-load verb).
  Done.
- clips (empty). Honest empty. Done.
- Search field at the bottom, filters all sections live, "(no match)" when
  empty. `[wired]` (pure client filter). Done.

### 2b. Repeat Zone — launch grid (flex hero)
- `REPEAT ZONE` cyan header + hint + zoom −/+ (cell 34..88px). `[local-only]`
  zoom. Done.
- 64px track labels + 5 scene columns; row 0 = scene headers `n ▶`, click
  launches the whole column → `launch scene <n> quantize <1>`. `[wired]`. Done.
- 6 track rows (drums/bass/chord/pad/arp/lead) with color dot + name. Done.
- Launch cell: empty → `+` placeholder (click adds a local clip);
  filled → track-tinted, click = `launch clip <id> quantize <1>` AND opens the
  clip in Sequence Edit. `[wired]` launch / `[local-only]` per-row single-play
  bookkeeping (there is no per-cell playing readback on the wire —
  `grid_model.hpp`'s documented gap), `[local-only]` cell content (no clip
  primitive content binding). Done.
- Mini clip preview: pad row = audio waveform envelope; other rows = 16×5 dot
  piano-roll; both deterministic from the clip label hash. `[new-widget]`
  (`neon::clip_preview_*`). Done.
- L→R sweep bar on a playing cell while running (~1.7s loop). `[new-widget]` +
  `[local-only]` animation (gated on playing). Done.
- Playing cell = stronger fill + track-color border + glow; opened cell = inset
  ring. `[local-only]`. Done.

### 2c. Rail (288px, scrolls)
- `INTENTION` green + live tag (green "● live" / grey "at rest") — driven by
  real `AppState::harmony_active()`. `[wired]`. Done.
- Chord cards FOLLOWS (green) / NEXT (amber, `NEXT →n` countdown): FOLLOWS =
  real `AppState::chord_followed_current()` (lit while harmony_active), NEXT =
  real `chord_followed_next()`; "—" when absent. Countdown derived from the
  real beat position. `[wired]`. Done.
- XY pad: valence horiz, energy vert, draggable blue dot + radial glow + grid.
  `[new-widget]`. Drag updates `V02State::energy/valence` — NO engine verb
  exists → `[local-only]`. Done.
- Intention knobs ENERGY/TENSION/VALENCE. `[new-widget]` (rotary). Values are
  `V02State` intent, NO verb → `[local-only]`. Done. (XY and the energy/valence
  knobs share the same backing values so they track each other.)
- `PARTS` divider + "amount" hint. Done.
- Parts knobs DRUMS/BASS/CHORD "amount" 0..1. `[new-widget]` (rotary). NO
  per-part amount/volume verb on the wire (`part <role>` only takes mute/solo)
  → `[local-only]`. Done. (Mute/solo latches from the old parts panel are NOT
  in the v02 rail layout — the rail shows amount knobs only, per spec; the real
  mute/solo verbs remain available but are not surfaced here. `[gap]` in v02
  surface.)
- Master VU: `MASTER` + dB readout (`-6.2 dB` playing / `—` stopped) +
  10-segment animated EQ meter (green→amber→pink). `[new-widget]`. The dB value
  and the per-segment levels are a `[local-only]` animation — there is no level
  metering on the wire (`[gap]` for a real meter). Done.

## 3. Sequence Edit (172px)
- Header: `SEQUENCE EDIT` cyan + `part <name>` (track color) + `clip <label>` +
  `grid 1/16` + right-aligned `piano-roll | step` tabs (active = cyan bg).
  Tabs/selection are real `SeqEditModel` state. `[wired]` (model) /
  `[local-only]` (no engine echo). Done.
- Canvas: vertical bar guides; if a clip is open → 16×8 track-colored piano-roll
  blocks (deterministic from label) with glow + a green playhead sweeping while
  the opened clip plays; else centered muted hint. `[new-widget]` +
  `[local-only]` (the note blocks are procedural from the label hash — there is
  no live `Track` step data on the wire, the documented `seqedit` gap; playhead
  gated on playing). Done.

## Animations (ImGui frame clock, gated on playing + glow)
- `sn-sweepx` cell sweep, `sn-blink` status dot, `sn-eq` master VU, sequence-edit
  playhead. All `[local-only]`, all gated on `V02State::playing`. Done.
- Transport clock (24 pulses/beat) for bar:beat:pulse is REAL (from `kBeat`).
  `[wired]`.

## New reusable widgets built (`neon_widgets.hpp/.cpp`)
1. `neon::knob` — conic arc + pointer, vertical-drag. Used 6× (3 intention + 3
   parts). Done.
2. `neon::xy_pad` — draggable dot + radial glow + grid. Done.
3. `neon::master_vu` — animated segmented EQ meter. Done.
4. `neon::clip_preview_waveform` / `neon::clip_preview_pianoroll` +
   `neon::sweep_bar` — procedural launch-cell previews. Done.
5. `neon::glow_rect` / `neon::glow_circle` — soft outer glow honoring the global
   flag. Done.
6. `neon::background` — radial glows + 40px grid overlay. Done.
7. `neon::pad_button` — rounded neon transport pad. Done (bonus).

## Deferred / not in this pass
- `density` compact mode. `[gap]`
- Real engine wiring for energy/tension/valence, parts amount, master metering,
  live BPM/key/time-sig readback. All `[gap]` — need new L1 verbs / snapshot
  events (`Op::kGet` unwired, no Director node 10000).
- Drag-to-reorder / real clip content binding to the core ClipMatrix. `[gap]`

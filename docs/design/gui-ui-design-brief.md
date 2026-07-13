# Design brief — sonotron: pixel-perfect UI for a live arranger workstation

> Prompt for an external design pass (e.g. Claude Design). Product-accurate as of
> 2026-07-13, grounded in `docs/design/gui-ux-flow-inventory.md` (the file:line source
> of truth for what is live vs placeholder). Design the aspirational complete
> workstation, but ground it in the real zone skeleton and mark every element
> LIVE-TODAY vs TO-BE-WIRED so the design doubles as an implementation target.

## Role
You are a senior product/UI designer for professional audio & live-performance
software (Ableton Live, NI Maschine, Korg/Yamaha arrangers, Elektron). Produce a
PIXEL-PERFECT visual design + design system for the desktop app "sonotron". Design
the ASPIRATIONAL complete workstation (the target), but ground it in the app's REAL
zone skeleton below, and clearly mark each element as LIVE-TODAY vs TO-BE-WIRED so
the design doubles as an implementation target.

## What sonotron is
A real-time **arranger workstation**: feed it a style (a genre band) and harmony, and
it generates a full accompaniment live, following your chords in real time. It's a
MIDI brain (drives synths; audio is a companion). Used 50/50 live-on-stage and in the
studio: glanceable and unambiguous under stage lighting, dense enough for studio
control. Currently Dear ImGui (plain); design the confident, musical, pixel-perfect
target that stays implementable as an immediate-mode dark UI (state visible AT REST,
never hover-only).

## Real layout skeleton (design exactly this structure; it's JSON-driven & reflows by weight)
A menu bar + three horizontal bands:
1. **Transport bar** (thin, top, persistent). LIVE TODAY: a numeric position readout
   `bar N · beat M · PP` (moving while playing, parked at rest); Play/Stop/Continue;
   Panic; connection status. TO-BE-WIRED (design as target): tempo/BPM, meter, key,
   a clickable current-style chip, a graphical bar-progress bar.
2. **Working row** (three columns):
   - **Browser** (left): a style library you browse and pick from; `style load <name>`
     on select. Drag a style onto a grid cell (that drag-drop IS real today).
   - **Repeat-Zone grid — THE HERO** (center, widest, ~0.54): a matrix of launchable
     clip cells in scenes (columns) across part rows. Cell states: empty, has-content
     (a style dropped in), armed (amber, pending next-bar), playing (green), stopped.
     TODAY launching is INERT (design the full armed→playing→stopped lifecycle as the
     target; the drop-to-fill is already real).
   - **Right rail** (a single column, vertical stack of two):
     - **Intention panel** (top). LIVE TODAY: the harmonic visualizer — the chord the
       arranger is following now (root+quality) in green (`follows`) with the pending
       next chord in amber (`next`), plus key. TO-BE-WIRED: energy/tension/valence
       intention sliders (placeholders today).
     - **Parts panel** (bottom): one row per band part (role, mute, solo, activity).
       LIVE-ish: mute/solo send real commands (state is optimistic-local — the engine
       echoes state but the GUI doesn't yet read it back, so design a clear "commanded"
       look). TO-BE-WIRED: per-part GM instrument + volume.
3. **Sequence Edit** (bottom band): a piano-roll / step canvas — honest placeholder
   today; design the target editing surface + a playhead synced to the transport.

## Live data bindings (design these to feel truly alive)
- `kChordFollowed` → Intention harmonic visualizer (committed=green, pending=amber,
  at-rest=neutral).
- `kBeat` → Transport numeric playhead + (target) the bar-progress bar + Sequence-Edit
  playhead.
- (Target, once wired) engine state echo → Parts mute/solo readback, style chip, groove/
  arp values.

## Zones to design as the TARGET (exist in the product's sibling TUI, not yet in the GUI)
Design these as first-class GUI panels for the complete vision, clearly labeled "to be
brought into the GUI":
- **Groove panel**: feel controls — swing, humanize, accent, timing — as knobs/sliders
  with live values.
- **Arp panel**: arpeggiator controls.
- **Style/section chooser**: variation A/B/C/D, intro/fill/ending section switches.
- **MIDI monitor**: a live scrolling view of outgoing MIDI events.
- **Accompany affordance**: load a MIDI melody file → band plays under auto-detected
  chords (an import entry point + the resulting live harmony).
- **Restyle affordance**: transform an imported melody into a target genre's idiom.
- **Export**: dump the session to a .mid file.

## Flows to support (design entry points + states for each)
Connect to engine (connected/disconnected) · browse & load a style · drag a style into a
grid cell · Start/Stop/Continue/Panic; playhead scrolls · harmonic visualizer follows
chords (green/amber/neutral) · mute/solo parts · launch/stop a clip cell; launch a scene;
cells cycle armed→playing→stopped quantized to the bar · (target) adjust groove/arp ·
(target) switch style variation/section · (target) Accompany import · (target) Restyle ·
(target) export .mid · (target) read the MIDI monitor.

## Deliverables (pixel-perfect)
1. **Design system / tokens**: semantic color palette (base/surface/elevated, text tiers,
   live-green, pending-amber, warn-red, per-part-role hues), type scale (a mono for live
   musical/numeric data + a humanist sans for labels), spacing, radii, borders/elevation,
   focus states.
2. **Component library**: buttons; mute/solo toggles; knobs & sliders with value readouts;
   the clip/launch pad in ALL cell states; the part row; the harmonic-visualizer widget in
   all states; transport controls + moving playhead + bar-progress; meters; browser list
   item; style chip; MIDI-monitor row; docked/tabbed panels; connection indicator.
3. **The main workstation screen** in full fidelity, in two states: AT-REST (stopped, no
   clip playing) and LIVE (playing; a chord followed=green; a clip armed=amber; a clip
   playing=green; a part soloed; playhead mid-bar).
4. **Each zone/panel** in its meaningful states (empty / active / disabled / warning /
   placeholder-vs-wired).
5. **Annotations** mapping every element to its data source (name the engine event or the
   L1 command) and marking LIVE-TODAY vs TO-BE-WIRED.
6. A short **rationale** tying the visual language to live-performance legibility.

## Constraints
- Immediate-mode-friendly: state visible at rest (not hover-gated), fixed predictable hit
  targets, works without animation but specify motion where it aids performance (playhead,
  cell launch, pending pulse).
- Stage-legible: high contrast, large live readouts, unambiguous status color.
- Deliver as a high-fidelity self-contained artifact (annotated mockups / an interactive
  HTML+CSS mockup of the main screen + a component sheet) plus the token/spec list in text.

Order: design system → main screen (both states) → component sheet → per-panel states.

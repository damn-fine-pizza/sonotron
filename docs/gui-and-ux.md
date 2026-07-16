# GUI and UX — the sonotron workstation

Status: **authoritative for the GUI screen (node 11600).** This document governs the desktop
workstation screen, its two working flows, the GUI↔core wire contract, the as-built reality of the
shipped binary, the pixel-perfect design target, the product-level flow set, and the sibling TUI
piano/MIDI-monitor spec. It stays **bound** by:

- `components/core/arrangrr/include/arrangrr/abi.hpp` — the frozen v1 command/event vocabulary.
- The shipped UDS-JSONL adapter (`components/platform/hostrt/uds_server.*`, `jsonl.cpp`, `shell.cpp`).
- The naming/architecture locks in `docs/product-vision.md` (sonotron = host workstation, arrangrr =
  core brain, melodd = audio peer).

> The JSON-envelope protocol in `DESIGN.md §24` (`{"op":"do","path":...,"id":5}` with `id`/`re`
> correlation and a `hello` handshake) does **not** describe the shipped wire — the real contract is
> the UDS-JSONL adapter named above. Build to the real code and to this document, not §24.

---

## 1. Product posture & vision

sonotron is a real-time **arranger workstation**: feed it a style (a genre band) and harmony, and it
generates a full accompaniment live, following your chords in real time. It is a MIDI brain (drives
synths; audio is a companion peer). It is used 50/50 live-on-stage and in the studio: glanceable and
unambiguous under stage lighting, dense enough for studio control.

- **Immediacy of GarageBand for iPad, with better flows.** One screen, no modes to hunt through; you
  are making sound within seconds. The launch grid is the hero.
- **Depth of clip management closer to Bitwig.** The grid is a real clip/scene matrix with a
  piano-roll/step editor one gesture away — not a toy.
- **MIDI-only symbolic.** No linear audio timeline (that gravity is what the whole design refuses).
  Audio realisation is a separate peer engine (melodd), out of scope here.
- **Two operations the screen must serve, first-class and equal:**
  1. **Track preparation** — build the set: pick a style/key/tempo, author clips, assign voices,
     route, set groove — before you perform.
  2. **Performance** — play the prepared set live: launch scenes/cells, steer harmony by hand,
     trigger fills, mute/solo, tweak feel.

**Kill the word "DAW."** Even as internal shorthand it imports a gravity `docs/product-vision.md` spends
its whole design resisting: a linear audio timeline, audio tracks, plugin hosting, mixing. Name it
what it is — a *generative MIDI live-instrument / arranger workstation*. The moment it is measured
against Ableton it has lost the ground where it actually wins (Genos + Band-in-a-Box + a modular
generative rig). Any literal-DAW drift (audio tracks, audio timeline, plugin host, bundled mixing) is
thrown away by construction.

**The three laws hold:** everything **observable**, everything **overridable and you win**,
everything **hand-operable**. TUI and GUI mirror the *same* truth. The machine's help is optional and
on top, never a gate.

---

## 2. Personas & use cases

Personas, ranked by who the FIRST GUI is for.

**A — The live arranger-keyboard player, performing solo *(the product's soul / WOW target)*.**
Wedding/piano-bar/resident-gig keyboardist. Today plays a Yamaha Genos or Korg Pa (€4–6k). Lives on
single-finger chords, fills, and variations; *is* the band's chord track, live, with the left hand.
- **Job:** *"Let me be a whole band with two hands, in front of real people, without ever staring at
  a screen while I play."*
- **Success in the hands:** plays a C one-finger → the band lands on it *within the bar* and
  **holds** it; a right-hand solo has no wrong notes; a fill before the chorus attacks quantized to
  the bar. Success is kinaesthetic — he *feels* it, doesn't watch it.
- **Why a desktop GUI:** live, he looks at nothing — so the GUI serves him **before and around** the
  gig (prepare the set, glance once to confirm the section, a music-stand-sized harmony readout
  legible at 2 metres, which the dense text TUI cannot give). The GUI beats the TUI **only** on
  glanceable legibility and 2-D structure, **never** on live command speed. It beats hardware on
  determinism, reproducibility, and weight (a laptop, free of a €6k board).

**B — The bedroom producer sketching a song over an auto-band *(PRIMARY for the first GUI / depth &
retention)*.** Makes beats/pop/lo-fi in a DAW; one-finger chords, hunting the idea. Today uses
Scaler 2 or ChordPulse. **Extended to the experimental / improvising performer** who uses the GUI as
an *active surface while playing* (watches and interacts, unlike Persona A who never looks at the
screen).
- **Job:** *"Give me a progression that sounds like a real band in thirty seconds, so I can hear if
  the idea holds before I open the DAW."*
- **Success in the hands:** loads `funk`, one-fingers `Am F C G`, hears a credible groove, mutes the
  pad, raises the swing, has a mood in two minutes — then **locks the seed** and finds it
  byte-identical next week (arrangrr's edge over Scaler's static chord-grid).
- **Why a desktop GUI:** for him it IS the primary surface (mouse + computer-keyboard, no MIDI
  hardware required). Layout consequence: parts-mixer / groove / reseed are first-class citizens; the
  harmony surface stays prominent, re-justified as B's close-up sketching aid (≈40 cm) rather than
  A's 2-metre stand; the no-central-chord-pad ban stands.

**C — The performer building/rehearsing a set *(REAL but NOT v1-serviceable)*.**
Cover band / solo looper / worship leader; must prep 15 songs with different styles/sections/tempos/
keys and recall them without panic. The set is intrinsically 2-D — the GUI is the right home for it.
- **Honest limit:** song-mode/recall (node 8000) is **behind the freeze line, not built.** In v1
  this persona is served only by manual `style load` / `bpm` / `key` changes. A future retention
  persona, not a launch persona.

**D — The beginner / non-player *(plausible market, "no wrong notes" claim)*.** Wants to improvise
and never hit a wrong note. Without song-mode / guided progressions / didactic feedback he gets only
"a backing that follows your finger" — a market, not a served job yet. But NTT-constrained live input
(the whole keyboard mapped to chord/scale tones over the running band) is a genuine accessibility
feature almost nobody ships well, and the persona no competitor serves honestly. Do not build the WOW
on him, but the differentiator is real.

**Adjacent personas the architecture also shapes for:** the generative/ambient musician (harmonic
field + expressive targets, deterministic enough to recall, alive enough to surprise); the
live-looping performer (loops that *follow the chords* and can be re-harmonized on the fly — the
reversible looper); the sound designer / rig integrator (deterministic MIDI brain of a hardware rig:
multi-port routing, per-role voices, reproducible streams to test synths against a golden); the style
author (wants to *see* the relative pattern model — root, walk to the fifth — the relative-pattern
editor is the GUI's real prize).

**Explicitly NOT a persona:** the orchestral/linear-arrangement composer. That is DAW gravity,
rejected by construction. Promising it would be a lie.

### Use cases

1. **The solo piano-bar (A).** Saturday night. Loads `bossa`, 120 BPM, key F. Steers chords with the
   left hand, improvises with the right, raises a fill into the chorus (`varB`). Glances at the GUI
   **once** to confirm the section. The GUI is the stand, not the instrument.
2. **The 90-second sketch (B).** Midnight idea. `funk`, one-fingers `Dm7 G7 Cmaj7`, mutes `pad` and
   `phrase`, swing to 40, feels the groove breathe. Locks the seed; reopens it identical.
3. **The set rehearsal (C, v1-honest).** Before the gig, preps three songs by hand-changing `style
   load` / `bpm` / `key`, noting the settings. Clumsy without recall — but it works. One-touch recall
   arrives with node 8000.
4. **The group jam (A/C).** The keyboardist is band-in-a-box for a drummer + singer in rehearsal:
   steers chords live, the MIDI band fills bass + comping. `kLivePriority` matters — while a live
   chord is HELD, his hand beats the sequencer; on release the sequencer resumes.
5. **The case we KILL.** "Mixing/automating audio tracks in the GUI." Does not exist: no audio in the
   core, no mixer, no linear timeline. Whoever asks wants Ableton.

---

## 3. The screen at a glance

Principle: **one screen, the repeat grid is the hero.** The menu bar is chrome; everything below it
is the JSON-driven zone grid the layout engine renders. The app is a single ImGui window titled
`sonotron`, 1280×800 logical at launch, HiDPI-scaled by the monitor's GLFW content scale, monospace
(vendored JetBrains Mono NL, base **13 px**). A `File / Edit / View / Transport / Help` main-menu-bar
sits above a borderless full-viewport window hosting a **6-zone grid**.

```
┌ File  Edit  View  Transport  Help ─────────────────────────────── menu bar (chrome) ┐
├──────────────────────────────────────────────────────────────────────────────────────┤
│ ▶ ■  ♩=120 4/4  Key Cm  Style: Funk ▸ [Var A]  ● Connected     bar 3·beat 2  ▓▓░░    │ transport
├───────────────┬──────────────────────────────────────────────┬─────────────────────────┤
│ BROWSER       │ REPEAT ZONE  (Live-Loops launch grid)        │ INTENTION (optional)    │
│ ▾ Styles      │        Scene1  Scene2  Scene3  +             │  energy  ▓▓▓▓▓░░░       │
│   Funk        │ Drums  [▶A ]  [▶B ]  [ · ]                   │  tension ▓▓░░░░░░       │
│ ▾ Clips       │ Bass   [▶wlk] [ · ]  [ · ]                   │  valence ◑ neutral      │
│   comp-8ths   │ Chord1 [▶cmp] [▶stab][ · ]                   │─────────────────────────│
│ ▾ MIDI seqs   │ Pad    [ · ]  [▶swl] [ · ]                   │ PARTS / MIXER           │
│   verse-prog  │ Arp    [ · ]  [ · ]  [▶up]                   │ Drums  M S  vol▓▓▓ gm   │
│   chorus-prog │  (drag from browser into any cell)           │ Bass   M S  vol▓▓░ 33   │
│ [search…]     │  ▸ launch quantize [1 bar ▾]   Scene ▶ all   │ Chord1 M S  vol▓▓▓ 4    │
├───────────────┴──────────────────────────────────────────────┴─────────────────────────┤
│ SEQUENCE EDIT  Part: Bass ▾  Clip: wlk ▾  ●rec ▾grid 1/16  [piano-roll|step]  [▏playhead]│
│  A ──────████──────────████──────────                     ← your notes are solid          │
│  now │  bars 1 · 2 · 3 · 4                                       [playhead ▏]             │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

### Zones and layout

Each zone is a `Zone{id,title,row,col,weight,visible}` in `default_layout()`
(`src/layout_model.cpp`), dispatched by `id` in `render_zone_content()` (`src/layout_renderer.cpp`).
The menu bar is **not** a zone. Geometry is relative *weights* normalized per frame by `compute_rows`
against `GetContentRegionAvail`, so zones **reflow** proportionally as the window resizes.

| Zone `id` | Title | Row / col | Width wt | Height wt | Notes |
|---|---|---|---|---|---|
| — | menu bar | chrome | — | — | `ImGui::BeginMainMenuBar()`, outside the grid |
| `transport` | Transport | 0, full-span | — | 0.06 | thin, single line |
| `browser` | Browser | 1, col 0 | 0.22 | 1.0 | searchable material tree |
| `grid` | Repeat Zone | 1, col 1 | **0.54** | 1.0 | **HERO** — widest by design |
| `intention` | Intention | 1, col 2 (top) | 0.24 | 0.38 | demoted, optional, read-only |
| `parts` | Parts / Mixer | 1, col 2 (bottom) | 0.24 | 0.62 | stacked under Intention |
| `seqedit` | Sequence Edit | 2, full-span | — | 0.34 | piano-roll / step editor |

Three horizontal bands top-to-bottom: Row 0 Transport (full width, thin, the only zone drawn without
a title+separator); Row 1 the working row (browser / hero grid / right rail); Row 2 Sequence Edit.
The right rail (`intention` over `parts` in col 2) is a **vertical stack inside one column** —
`compute_rows` groups same-(row,col) zones into one cell's vertical stack, rendered by the
stacked-column path in `render_layout`. This nested vertical split is the layout-engine investment
the restart added (§10 decision 3).

Font atlas is rasterized once at startup at `content_scale × base_font_size`; style metrics scaled
with `ScaleAllSizes` (**not** live per-monitor-move rescale). No layout persistence of *user*
resizing — zones are static frames, no drag splitters, no docking; `layout.json`
(`$XDG_CONFIG_HOME/sonotron/`) round-trips only hand-edits, and an unknown zone `id` falls through to
a titled empty frame.

---

## 4. Zones — intent and as-built state

State legend: **LIVE** = driven by real engine events; **REAL (local)** = a real control whose L1
verb is shipped and sent, but with no readback; **PLACEHOLDER** = built and visible but inert;
**DISABLED** = greyed / non-interactive stub.

### 4.1 Menu bar (chrome) — `render_menu_bar`

| Menu | Items | State | L1 verb |
|---|---|---|---|
| **File** | New set / Open set… / Save set | **DISABLED** (GUI-local, not built) | — |
| | Quit | **REAL** — closes the *window only*, never a socket `quit` | — |
| **Edit** | Undo / Redo / Preferences… | **DISABLED** | — |
| **View** | Intention (checkbox) | **LIVE toggle** — bound to `intention` zone `visible` | host-only |
| | Parts (checkbox) | **LIVE toggle** — bound to `parts` zone `visible` | host-only |
| | Layout density… | **DISABLED** | — |
| **Transport** | Start / Stop / Continue / Panic | **REAL** — each sends its L1 verb | `transport start\|stop\|continue`, `panic` |
| **Help** | About / Key bindings… / Contract–version | **DISABLED** | — |

Never wire window-close to a bare `quit`/`exit` on the socket — that kills the brain for every client
(`BrainSession::send()` blacklists it, §9). View toggles flip `Zone::visible`; a hidden zone takes no
space and its siblings redistribute. **Keybindings from the TUI** (surfaced under Help ▸ key bindings
when built): transport play/stop is **CTRL+P**; the style/section chooser is the **backtick** `` ` ``
key (CTRL+SPACE is the piano momentary↔toggle switch, not a chooser). Today Ctrl+P is shown as the
Start accelerator *label only* — it is **not** an actual global keybind; the backtick chooser is not
ported.

### 4.2 Transport zone — `render_transport_panel`

Connection status + transport control + position readout, all on one line. Contents left→right:
connection dot + `Connected`/`Disconnected` label (**LIVE**, from `BrainSession::status()` each
frame); **Play** → `transport start` + optimistic `note_transport_sent(true)` (**REAL**); **Stop** →
`transport stop` (**REAL**); **Panic** → `panic` (**REAL**); transport-state + `Section: <name>`
label (**LIVE**, from `kTransport`/`kBeat`/`kSection`); **bar·beat playhead** — `bar N . beat M .PP`
when playing, else disabled `bar -- . beat --` (**LIVE (P0-2, `kBeat`)**; `PP` is the 0–23 sub-beat
pulse). The `bar·beat` readout is a true playhead driven by the beat/position event, not inferred.

**TO-BE-WIRED vs the §3 wireframe:** tempo (`♩=120` / `bpm <n>`), meter (`4/4`), key (`Key Cm` /
`key <root> <mode>`), the clickable `Style: Funk ▸ [Var A]` chip, and the graphical bar-progress bar
`▓▓░░` are **not rendered** — the panel has no tempo/meter/key/style widgets today.

### 4.3 Browser zone — `render_browser_panel` + `browser_model`

A searchable tree of the three draggable material kinds:
- **Styles** — the 16 builtins (`basic, pop, rock, ballad, funk, disco, house, swing, bossa, samba,
  reggae, country, blues, shuffle, latin, motown`), each an ImGui drag source (payload
  `SONOTRON_STYLE_INDEX`). Click → `style load <name>`. **REAL / LIVE-capable.** A full-width search
  field applies a case-insensitive substring filter over style names. Drag a style → a grid cell; a
  style *section* → a grid cell.
- **Clips** — chord-sequence and step-track material the user authored/recorded. **PLACEHOLDER** —
  always `(none authored yet)`: there is no recorder/authoring UI to populate this browser list. (Not
  to be confused with the core's `ClipMatrix` launch primitive, §4.4, which the grid now binds to for
  real — the gap here is specifically *user-authored* content, not the launch mechanism.)
- **MIDI seqs** — recorded chord progressions (`ChordSequence`, degrees stored relative to key so
  they transpose/re-harmonise, D28). **PLACEHOLDER** — always empty.

### 4.4 Repeat Zone / grid — the HERO — `render_grid_panel` + `grid_model`

A **Live-Loops launch grid**: rows are the 9 `TrackRole` parts (`Drums, Perc, Bass, Chord1, Chord2,
Pad, Arp, Phrase, Lead`; `kCc` deliberately excluded), columns are **scenes** (default 3, max 8; a
`+ Scene` button adds a column preserving contents). Structure: an `ImGui::BeginTable` with a fixed
70 px "Part" column plus one column per scene.

**Cell model.** `GridCell{GridCellKind kind, std::string label}`, kind ∈ `{kEmpty, kStyleSection,
kChordSequence, kStepTrack}`:
- a **style section** (varA..D / fillA..D / intro / break / ending),
- a **chord sequence** (`seq` — the one producer that emits a `chord` event today),
- a **step track** (`track` — a hand-authored pattern for that part).

**Drag from browser** drops material into a cell and records which primitive it launches — a cell
**accepts** a `SONOTRON_STYLE_INDEX` drag, calls `set_cell(kStyleSection, name)` for the GUI-local
display copy, **and** registers a real `ClipMatrix` clip at that cell's own stable id (`clip add
<role> <scene> style <section> id <id>`, `grid_panel.cpp`'s drop handler). **REAL** — cell *content*
is real, and (see below) so is *launch*; only `kEmpty`/`kStyleSection` are reachable from the UI (the
`kChordSequence`/`kStepTrack` kinds have no drag source yet).

**Launching — shipped.** Launching a column ("Scene ▶") applies the column's `SectionType` via the
existing `style section` verb and sends `launch scene <n> quantize <q>`; launching a single filled
cell sends `launch clip <id> quantize <q>` (`kDefaultLaunchQuantizeBars = 1`). **Launch quantize** is
honoured by the **core** — launch timing is a musical decision that belongs where the clock lives, not
smeared across the socket. `inline constexpr bool kGridLaunchWired = true`
(`apps/gui-sonotron/src/grid_model.hpp`) — the `>` scene button and the cells are no longer
`BeginDisabled`; clicking a filled cell sends the real `launch clip` verb and opens it in Sequence
Edit. Launched state is **read back for real**, not locally echoed: `AppState::clip_state(id)` decodes
the core's `clip` `OutEvent` into `ClipLaunchState{kStopped, kArmed, kPlaying, kQueuedStop}`
(`app_state.hpp`/`app_state.cpp`), and armed/playing/queued-stop all render as "lit" in the cell.

**Built on a real core clip primitive — shipped (Phase-5 Item #2).** The core's `ClipMatrix`
(`components/core/arrangrr/include/arrangrr/clip/clip_matrix.hpp`) is a first-class, Engine-owned
bounded POD pool of clips (`{part_role, scene_index, kind, content_index, state, n_bars,
due_bar_index}`); the ABI carries `kClipAdd`/`kClipLaunch`/`kClipStop` (`abi.hpp`). The Repeat Zone
binds to this real primitive — the GUI does **not** ship a throwaway launcher that fakes clips by
scripting `style section`/`seq*`/`track*`. Landed across `3398f04` (readback into `AppState` +
Shape-A `ClipMatrix::add_at` explicit-id binding), `f531d8f` (renamable scenes + `scenes.json`
persistence), `9ce480d` (real-content cell preview via `preview::preview_for`), `f1fa7d7` (per-scene
`SectionType` + variations drag palette), and `9382c07` (GUI-driven auto-song scene advance,
`next_scene_to_launch`). `grid_model.hpp` holds the display matrix/scene structure; the
launch/stop/quantize/armed-playing state machine lives in the core behind the `clip` verbs, exactly
as decided (§11 decision 1). The standalone Looper (node 6000) and full recall/song-mode persistence
(node 8000) remain separately unbuilt — see item 6 below and §13.

### 4.5 Intention zone — `render_intention_panel`

A minimal, **read-only** rail: not the old conductor podium, not the center, not authoritative — the
Director that would drive it (node 10000) is not built. Toggled by **View ▸ Intention** (an additive
`bool visible` on `Zone`, default true; `render_layout` skips `!visible` zones). This is the home of
the live harmonic visualizer. Contents top→bottom:
1. **Live/at-rest header** — green `Intention (live)` when `harmony_active()`, else disabled
   `Intention (at rest)`. **LIVE gate.**
2. **`follows <chord>`** — the followed chord *this bar*. **GREEN**, rendered only when the activity
   gate is open **and** the current chord is valid, else `follows --`. **LIVE (P0-1,
   `kChordFollowed`).**
3. **`next <chord>`** — the shift-staged pending chord. **AMBER**, independent of the gate (staging
   can happen at rest), else `next --`. **LIVE (P0-1).**
4. **energy / tension / valence** — three disabled rows and `valence o unknown`. **PLACEHOLDER**,
   pinned at zero because the Director (node 10000) does not exist.

**Ghosts — deferred with the Director.** When the Director is real, its *next move* shows as a
**ghost** — the machine's intent rendered *before it lands* — in both the band/part lanes ("Pad enters
→ 4") and the Sequence Edit piano-roll (a dashed proposed note). Distinct from the pull-only copilot's
proposals (§4.7): a ghost is the Director's imminent automatic move shown ahead of the bar boundary,
not something you asked for.

### 4.6 Parts / Mixer zone — `render_parts_panel` + `parts_model`

Per-part mute/solo/volume/GM-voice, shaped like `Arranger::PartInfo {routed, port, channel,
gm_program, muted, soloed, present}`, one row per the same 9 `TrackRole` parts. Sits **under Intention
in the right column** (col 2), matching the wireframe literally (the nested vertical split, §10
decision 3). Each row: **`M`** → toggles local `muted` and sends `part <role> mute on|off`; **`S`** →
`part <role> solo on|off` (both **REAL (local, optimistic)**); the part label; **`gm --`** disabled
GM-program readout (**PLACEHOLDER** — `gm_program` stays −1, no per-part program readback).

**TO-BE-WIRED vs §3 wireframe:** the **volume bar** (`vol ▓▓▓`) and the numeric GM program are not
rendered as live controls. **State honesty:** mute/solo are *optimistic local hints*; another client's
change is not reflected because `Op::kGet`/snapshot is unwired (§10). `kParamState` echoes exist on the
wire but the GUI drops them (§6). Solo does not visibly dim other rows.

Full command surface for parts/routing: `part mute <role> <0|1>`, `part solo <role> <0|1>`,
`program <gm> <port:ch>`, `style route <role> <port:ch>`.

### 4.7 Sequence Edit zone — `render_seqedit_panel` + `seqedit_model`

A piano-roll (and a step view for drum-style parts) over a single `Track` (per-step note/vel/gate/tie/
probability/ratchet/micro — `timeline/timeline.hpp`). Editing a note writes `track step <i> <note>
<vel> <gate> …` (the param-locked step form in `abi.hpp`); Record arms the part and captures incoming
notes. Toolbar: **Part combo** (the 9 roles, **REAL local**); **Clip label** (static, defaults `-`,
**PLACEHOLDER**); **`rec` checkbox** (**REAL local** but drives no capture path yet); **`grid 1/N`**
(read-only, default 1/16); **piano-roll / step radio** (**REAL local**, both render the same
placeholder). **Note canvas** — disabled `(note canvas awaits Track step editing)` (**PLACEHOLDER** —
no `Track` step data modeled or drawn; **no playhead here yet** either).

**The pull-only copilot (deferred for v1 — spec kept).** v1 is a plain, honest editor: you always
hold the pen. The copilot, when it lands, is **pull-only** — silent until asked:
- **Solid = your notes; dashed/dim = the copilot's proposal** (ghosts), scoped to your selection.
- **Ask ▸** exposes three verbs — **Propose / Adjust / Add**; **⟳** cycles a new seeded proposal;
  **⏎** accepts, **⌫** rejects. Nothing lands until you accept.

---

## 5. The GUI↔core contract (the wire)

The GUI is a **separate process, pure client.** It never links or `#include`s the core; it speaks to
the headless core **only** over the Unix-domain-socket control adapter (or, in the default integrated
mode, over in-process rings — §9). The v1 ABI that is frozen and pinned by tests is the **binary**
`Op`/`Param`/`Command`/`OutEvent` vocabulary — but that binary struct **never crosses the socket.**
What crosses:

- **GUI → core (requests):** plain **text L1 command lines**, the exact REPL grammar (`chord play C`,
  `style load 3`, `groove swing 40`, `transport start`, `bpm 120`), one per `\n`-terminated line.
  `Shell::exec_line` tokenizes and dispatches. There is **no** wire-level `Op`/`Param`, no request id,
  no JSON envelope inbound.
- **core → GUI (events):** canonical **JSONL** lines, one `OutEvent` each, rendered by `to_jsonl()`
  and **broadcast to every connected client** (best-effort; a slow client drops events rather than
  stalling MIDI).

### Transport facts
- `AF_UNIX` / `SOCK_STREAM`, newline-delimited framing, `kMaxLineLength = 4096` per line.
- Path is **explicit**: launch the core with `cli-arrangrr --control /path/to.sock` (no default; no
  `--control` ⇒ no socket). Headless/non-tty launch skips TUI/panels — the mode the GUI wants.
- Multi-client, no handshake, no per-client subscription. Connect = raw `connect()`.
- **Trap:** sending `quit`/`exit` over the socket terminates the whole host process for every client.
  Never wire window-close to a bare `quit`.
- Best client template to imitate:
  `components/platform/hostrt/tests/test_host.cpp::test_uds_server_end_to_end()`.

### Pure-client boundary rules
- **Single binary, single thread, poll-in-frame.** The ImGui render loop (~60 fps, ~16 ms budget)
  drains the non-blocking socket fd (`recv`/`poll`, `MSG_DONTBLOCK`, to `EAGAIN`) once per frame. **No
  background reader thread** — max added latency = one frame; zero risk of the GUI entering the timing
  path.
- **Rule 1:** the GUI target links **neither `arrangrr` nor `hostrt`** and `#include`s **zero** core
  headers (`jsonl.cpp` is coupled to the core and lives in `hostrt`). Model its `CMakeLists.txt` on
  `apps/tools/arrstyle-converter`, NOT on `hostrt`.
- **Rule 2:** the GUI carries its **own** wire layer — a `LineBuffer`-style newline reassembly reader
  + a minimal JSON-line parser + its **own** name tables (section↔string, chord-quality↔suffix,
  warn↔string) as plain strings. **Never** `static_cast` a core enum across the boundary.
  `note_names.{hpp,cpp}` is already a clean seam (integers only) — reusable.

### Toolkit: decided & vendored
**Dear ImGui vendored** (upstream `ocornut/imgui`, never a distro package) driven by the
`imgui_impl_glfw` + `imgui_impl_opengl3` upstream backend pair, on **GLFW3** (window + GL context +
input) rendering through **system OpenGL**. GLFW was chosen over SDL2/SDL3 and hello_imgui for the
narrowest footprint and native Wayland support on the Fedora dev platform. These are pure
window/render libs, isolated from the boundary rules above.

### 5.1 Commands the GUI SENDS (text line → resolved `Param`)

Exact verb spellings live in `components/platform/hostrt/shell_music_commands.cpp` / `shell_io_commands.cpp`.

**Transport / clock**

| Text line | Param | Notes |
|---|---|---|
| `transport start` | `kTransportStart` | rewind to 0, play |
| `transport stop` | `kTransportStop` | |
| `transport continue` | `kTransportContinue` | resume at tick (CTRL+P uses stop/continue) |
| `bpm <20..400>` | `kTransportTempo` | clamped 20–400 BPM |
| `panic` | `kPanic` | all-notes-off |
| `clock out <mask>` | `kClockOutMask` | which ports get F8/FA/FB/FC |

**Style / arranger / section**

| Text line | Param | Notes |
|---|---|---|
| `style load <0..15>` | `kStyleLoad` | the 16 builtins listed in §4.3 |
| `style section <type>` | `kStyleSection` | intro1/2, varA..D, fillA..D, break, ending1/2 (quantized to next bar while playing) |
| `style switch <style> <section> <immediate>` | `kStyleSwitch` | combined; chooser ENTER = next-bar / CTRL+\ = immediate |
| `style route <role> <port:ch>` | `kStyleRoute` | |

**Chord / harmony (the heart)**

| Text line | Param | Notes |
|---|---|---|
| `chord play <note[s]>` | `kChordPlay` | steer the band; single note in single-finger mode; SHIFT variant stages to next bar |
| `chord stop` | `kChordStop` | release voicing |
| `chord detect on\|off [port]` | `kChordDetect` | live piano→chord detection on an input port |
| `chord follow <auto\|detect\|sequencer\|manual\|live>` | `kChordFollow` | engine default = `kLivePriority` |
| `chord mode <diatonic\|single\|shell>` | `kChordMode` | input interpretation |
| `chord hold on\|off` | `kChordHold` | |
| `chord out <port:ch>` | `kChordOut` | |
| `key <root> <mode>` / `scale …` | `kKeySet` | major/minor/dorian/…/locrian |
| (input zone) | `kInputZone` | port → melody(sounds) / harmony(silent, feeds detector) |

**Parts mixer** — `part mute <role> <0|1>` → `kPartMute` (roles: drums,perc,bass,chord1,chord2,pad,
arp,phrase,lead,cc — cc = automation lane, not a musical part); `part solo <role> <0|1>` → `kPartSolo`
(any-solo ⇒ only soloed parts play); `program <gm> <port:ch>` → `kProgram`.

**Groove / feel** — `groove <field> <value>` → `kGroove`. Fields: `swing, humanize_timing,
humanize_velocity, accent` (0..100%), `swing_grid` (8|16), `quantize` (0..100%), `seed`.

**Arpeggiator** — `arp <field> <value>` → `kArp`; `arp out <port:ch>` → `kArpOut`. Fields: `enabled,
rate (1/4..1/32), direction (up/down/updown/downup/asplayed/random), octaves (1..4), gate (0..100),
latch, seed`.

**Recorded chord sequences** — `seq new/use/rec/add/loop/play/stop/transpose/del/clear` →
`kSeqNew..kSeqClear`. Secondary for the GUI MVP (song-mode territory).

**Timeline step tracks** — `track new/step/length/mute/solo` → `kTrackNew..kTrackSolo`. Advanced; not
MVP.

**Reserved — DO NOT SEND** — MIDI-FX / insert chain (`kFxSet/kFxParam/kFxEnable/kFxClear`) is
shape-reserved but has **no live ids** in v1; sending it yields `unsupported`. Reserve UI space, wire
nothing.

### 5.2 Events the GUI RECEIVES (JSONL)

Exactly five `OutEvent::Kind`s cross the wire today; the additive P0 kinds (§10) extend this set.

| JSONL shape | Kind | Fires when |
|---|---|---|
| `{"ev":"midi-out","port":N,"msg":"noteon\|noteoff\|cc\|program\|pitchbend\|raw\|<realtime>",…,"@":tick}` | `kMidi` | **every** sounded byte — arranger, tracks, chord, arp all sound through raw MIDI; there is no separate "note" event |
| `{"ev":"chord","in":"<note>","out":"<PCquality>","deg":"<roman\|->","@":tick}` | `kChord` | **only** from the recorded-sequencer path — **NOT** from `chord play` nor live detection (see §10) |
| `{"ev":"section","name":"<varA\|fillB\|…>","@":tick}` | `kSection` | arranger section **change** only |
| `{"ev":"transport","state":"playing\|paused\|stopped","@":tick}` | `kTransport` | emitted from the arranger-ending stop path; start/explicit-stop may not emit it (see §10) |
| `{"ev":"warn","code":"<name>","@":tick}` | `kWarn` | failure/nack only: scheduler_full, route_table_full, unknown_command, bad_argument, track_table_full, not_in_key, seq_table_full, seq_empty, unsupported |

Error to the offending client only (not an event): `{"error":"<msg>","cmd":"<line>"}`. **There is no
positive/success ack.** Success is inferred from resulting events or silence.

### 5.3 What the GUI mirrors vs does not duplicate
- The GUI is **a mirror that reflects and a keyboard that commands — never a truth and never a note in
  the timing path.** The live musical gesture (the chord the band follows) enters the core via
  MIDI/computer-keyboard, not through the GUI.
- **GUI = 2-D structure, inspection, and a big readable harmony surface.** Live keyboard/MIDI steering
  stays first-class but is **not re-invented** here (the TUI already does it well: CTRL+P, backtick
  chooser, parts mixer, MIDI monitor).
- **No central clickable chord-pad.** Put the harmony *readout* at the centre, never a chord-input
  widget.
- **Colour semantics, preserved exactly:** bold **GREEN** = chord followed *this bar* (lit only when
  transport plays or a chord is explicitly steered), **AMBER** = pending chord staged for next bar, off
  otherwise; green wins over amber on a shared pitch class.

---

## 6. Live data bindings (engine event → panel, as built)

Events are decoded by `parse_brain_event` (JSONL, UDS backend) or `brain_event_from_outevent`
(in-process backend) — both produce the same `BrainEvent` POD, drained once per frame, reduced by
`AppState::apply`.

| Engine event | `BrainEvent::Kind` | AppState reduction | Panel surface & animation |
|---|---|---|---|
| `chord-followed` | `kChordFollowed` | sets `chord_followed_current/next(_valid)`, `_source`; opens the activity gate if current valid | **Intention**: green `follows` + amber `next` rows — the single live **harmonic visualizer** |
| `beat` | `kBeat` | sets `m_bar/m_beat/m_pulse`; **forces** `kPlaying` | **Transport**: `bar N . beat M .PP` readout — the live **playhead** (numeric; no graphical scroll bar yet) |
| `transport` | `kTransport` | maps `playing/paused/stopped` → `m_transport`; on stop clears steer + parks playhead to 0 | **Transport**: state label; parks the bar·beat readout |
| `section` | `kSection` | `m_section = name` | **Transport**: `Section: <name>` |
| `chord` (recorded-sequencer only) | `kChord` | `m_chord_in/out/deg` | **Log line only** — no panel renders `chord_in/out/deg` (accessors exist but unused). Gap |
| `midi-out` | `kMidiOut` | log only | **stdout log only** — there is **no MIDI-monitor zone** in the GUI |
| `warn` | `kWarn` | log only | stdout log only |
| `error` (`{"error","cmd"}`) | `kError` | log only | stdout log only (untranslated commands / `midi-source` failures) |
| `param-state` echo | *(dropped)* | — | **NOT decoded** → no live mirror of groove/arp/parts/style. Gap (§10) |
| `clip` state | `kClip` | `m_clip_states[clip_id]` (a `ClipLaunchState`) | **Repeat Zone**: armed/playing/queued-stop cells render "lit"; a real per-cell readback (Phase-5 Item #2, §4.4), not a local echo |

**Connection status** is not an event: each frame reads `BrainSession::status()` → the green/red dot.
**Activity gate** (`harmony_active()`): true when transport is playing OR a chord was steered this
run; governs whether green reads as "live". Opened by an optimistic `note_manual_steer()` hint, a
valid `kChordFollowed`, or `kBeat`/`kTransport playing`; closed on stop.

---

## 7. User flows

The product-level flow set is the experience spine; each flow names the L1 verb(s) sent, the JSONL
event(s) reacted to, and the as-built status on today's GUI. The screen that realizes flows #1, #2,
#4, #5, #10 is this document's §3–§4; where the vision reaches past the built screen it is marked.

**Reality check — the L1 shapes the GUI sends today:** `transport start|stop|continue`, `panic`,
`style load <name>`, `part <role> mute|solo on|off`, plus (Phase-5 Item #2, the Repeat Zone, §4.4)
`style section <type>`, `launch scene <n> quantize <q>`, `launch clip <id> quantize <q>`, `clip add
<role> <scene> style <section> id <id>` — plus `midi-source load <path>` (wired in the session but
with **no UI trigger**). Flows the vision lists beyond these are not yet reachable from the GUI; each
is marked. *(The v02 redesign, outside this reconciliation's scope, has also added at least `bpm <n>`
and `transpose <n>` sends from the transport panel and a `style switch` send from the browser panel —
this bullet is not a complete inventory of every send in the current tree; treat it as the
Repeat-Zone-relevant subset, and see "Aperto per il proprietario" for the broader gap.)*

### Flow #1 — Start / set the base (from zero to a living band)
Pick style + key + BPM + meter (a few one-line choices, not menus-deep); press play and a full,
coherent band plays *immediately* — a sensible default section (Intro or A) and parts, at mid energy /
low tension / neutral valence (the style/section default; the true Director ground point is host-side,
not on the naked core). This is the ground state everything else steers from, seeded from the first
note (flow #9). **The WOW:** three choices and a whole band is alive — no timeline, no clips.
- **Sends:** `style load <0..15>`, `key <root> <mode>`, `bpm <n>`, `transport start`. **Reacts to:**
  `section`, `midi-out`, `transport`+`beat`. **As-built:** style load via click/drag (**REAL**); key
  and BPM controls **not yet in the GUI** (transport panel has no tempo/key widgets, §4.2); Start/Stop
  live with the P0-2 playhead. *Live BPM/key change lands on boundaries and must not desync audio
  color (flow #6) — suspended while a clip loops, or melodd resyncs at the loop point (owner-call).*

### Flow #2 — Steer harmony live (one finger conducts the band)
You play chords — even one finger — the band **follows and HOLDS**; the hand beats the machine
(`kLivePriority`); no wrong notes for the *machine* (NTT — every arranger note is a chord tone by
construction) and *optionally* for your hand too. Queue the next chord: it shows **amber** (queued),
lands **green** on the bar boundary, with a countdown ("→2"). Solo over it with an **opt-in** safety
net whose strength is declared — *hard-snap* (every note forced onto the chord) · *nearest-chord-tone*
(nudged) · *off* (default, for players who can play). It's the MIDI-FX insert of band 5000, not an
always-on tutor. **The WOW:** one finger conducts a whole band live — and, if you ask, you can't play
a wrong note either, without losing the freedom to bend a blue note on purpose.
- **Sends:** `chord play <note>` (SHIFT stages to next bar), `chord detect on <port>`, `chord follow
  live`, `input zone <port> harmony`. **Reacts to:** `chord-followed {current, pending, valid,
  source}`. **As-built:** the Intention visualizer (green `follows` / amber `next`) is **LIVE** off
  `kChordFollowed`. The **GUI has no chord-input widget** (by design) — the gesture enters the core via
  MIDI/keyboard. *In-process caveat:* the default integrated engine has **no MIDI-in** wired (Phase 2b
  scope), so live manual/detected chords need an external engine over `--control`, or the sequencer
  path. "Instant" is literal for single-finger; a fingered chord carries a small hold/hysteresis
  window (exact debounce still to detail).

### Flow #3 — Conduct intention (the distinctive spine)
See the current expressive state; express a **target** (where you want it to GO and how fast — "toward
more energy + tension, rising over 8 bars") by moving the controls to where you want to ARRIVE, or a
gesture; the system walks toward it on musical boundaries (arp switches on, pad re-enters, moves to
Var C, a fill fires before the peak, register lifts). Tension builds then RESOLVES — at the peak a
roll/fill, on the next downbeat the drop (build→drop is the style-default, not a universal law:
held-and-unresolved tension is legitimate). At any moment you hand-override and YOU win; the Director
resumes proposing from where you now are. Lock the seed → the same intention tomorrow produces the
same build byte-exact. **The WOW:** you said *"build to a peak"* and a whole band composed and
performed the build, musically, and you could put your hands on it any instant.
- *Honest status:* the **motion/rate axis is deferred** — the axis set is `energy · tension · valence`
  only; "over 8 bars"/"rising" is the intended experience, not a shipped trajectory control. On
  today's screen this is **not** the center: a demoted, read-only intention rail (§4.5) until the
  **Director** (node 10000, planned CAPSTONE) exists. It remains the product-vision spine.

### Flow #4 — Build structure (song-form without a timeline)
Sections/variations/fills are **launchable states on musical time**; the path you take is captured as
a **Song**. Move through sections (Intro→A→B→Break→Ending), each landing on a boundary
(amber→green like harmony); trigger fills/transitions that fire *before* the change (the scheduler
inserts the fill until the next bar boundary, then switches); A/B/C/D variation **per track**; capture
the path as a recallable Song. **The WOW:** you build a song's shape by launching sections live, and
the shape you played becomes a thing you can recall — no timeline you paint on.
- **Sends:** `style section <type>`, `launch scene <n> quantize <q>`, `launch clip <id> quantize <q>`,
  `stop clip <id>`. **Reacts to:** `clip`, `section`, `chord`, `midi-out`. **As-built:** drop-to-fill
  in a grid cell is **REAL**; launch is **REAL and shipped** (`kGridLaunchWired==true`, §4.4 — Phase-5
  Item #2, the core `ClipMatrix`/`clip` event). *"You win over the Director" on sections is not yet
  built — user-wins is shipped for chords (`kLivePriority`); generalizing arbitration to sections is a
  real ABI commitment (`docs/architecture.md`, NEEDS-DECISION).*

### Flow #5 — Shape the feel (you hold the feel; the Director never overwrites your hand)
The instant you set a value by hand, that lane is marked "yours"; the Director proposes around it,
never through it, until you release. Mute/solo parts (boundary or immediate); groove/swing/humanize
(global, genre-conditioned, node 9100); arp on a **harmonic-role part** (chord/pad/lead, not
drums/bass); per-part density/register/octave/dynamics. **The WOW:** you dial the feel of a live band
and it *stays* the way you left it. *That persistent human-over-machine hold is the thing a Genos/Korg
can't do.*
- **Sends:** `groove <field> <v>`, `arp <field> <v>`, `part mute/solo <role> <0|1>`. **As-built:**
  mute/solo are **REAL (optimistic local)** with no readback; the GUI has **no groove or arp panel**
  and sends neither verb (they exist in the TUI). *Per-part register is only an authored default
  (`kRoleAnchor`/3230), not a runtime lever — the `Param` enum has no live register command.*

### Flow #6 — Deploy audio color (arrangeable, not a track) · HOST-ONLY, deferred
arrangrr **DECIDES** audio symbolically (opaque clip references — "deploy clip N at bar X, loop to
clock"); **melodd/sampler REALIZE** it. No waveform editing, no linear timeline. Bring in a clip
(recorded/imported, an opaque content-addressed asset); the Arranger deploys it by **musical role, on
the clock** (riser before the drop, vocal-chop on the offbeat, loop under section B); it follows the
arrangement; the seed *points at* the asset but never regenerates its samples. **Absence is VISIBLE:**
with no audio engine the section that expected a riser **shows the gap**, not a silent hole. **The
WOW:** audio as an arrangeable color, never a track to edit.
- **Regime:** HOST-ONLY, and melodd **does not exist yet** — **NEEDS-DECISION, deferred**
  (`docs/backlog-future.md`).

### Flow #7 — Route sound (the same live band, on many voices at once)
arrangrr emits **MIDI symbols**; sonotron routes each part to a sound source (host, never core).
**MIDI → hardware is shipped** (assign parts to external synths over multi-port DIN/USB — Zone /
RoutingProfile, node 1260). **VST/CLAP/LV2 + melodd sources are host, future, not built.** Assign each
part to a source independently; MIDI-FX in the chain. **The WOW:** the same living arrangement sounds
at once on drums-VST + bass-melodd + a hardware synth, still one arrangement.

### Flow #8 — Observe & override (nothing hidden, nothing locked)
The three laws: everything observable (via hooks), overridable-and-you-win, hand-operable; TUI and GUI
mirror the *same* truth. See the true state of every part; see the machine's next move as **ghosts**
("Pad enters →4") before it lands (closest precedent is *Into the Breach*'s telegraph-then-override —
no live-music tool does this today, which is exactly the opportunity); override anything and win,
marked "yours". **Honest status — target, not today:** observability is currently **bifurcated** (a
poor cross-process channel of a few `OutEvent::Kind` + a rich TUI-only one via direct accessors), and
"user-wins everywhere" is decided, not built (shipped only for chords). The hook layer (ABI-additive)
is where it gets made real uniformly across TUI *and* GUI.

### Flow #9 — Reproduce / recall (the good take is a fact, not luck)
Determinism — same intention/seed → same result next week; the seed is a **musical object**. Lock the
seed (one control) → the whole performance becomes reproducible; reopen byte-exact (audio clips are
*referenced*, not regenerated); audition N seeded variations, lock the one you like, recall it later;
recall a Song (the path, flow #4), a Scene (an instant config snapshot), or a set (future); your
hand-moves fold into the seed/log so "yours" replays too. **The WOW:** the good take isn't a lucky
one-off — it's a fact you can lock, reopen, and trust identical, forever. *The unclaimed combination:
byte-exact reproducibility on a live, chord-reactive arrangement, with your hand-moves folded in.*
- Set-level recall (the Scene/song-mode level, node 8000) is **not built.**

### Flow #10 — Author a part by hand, assisted on demand (you always hold the pen)
Manual note entry stays first-class; the sequencer is a **pull-based copilot** — silent until asked,
nothing lands until you accept. You write notes (step or piano-roll); you ask for help scoped to a
selection — **Propose** (a bassline / a fill / a counter-melody, arrives as ghost notes you accept,
cycle, or dismiss), **Adjust** (tighten to the groove / fix voice-leading / snap into key, shown as a
ghost diff), **Add** (a second voice / passing tones / octave doubling, additive ghosts); every
proposal is **seeded** so "give me another" walks seeds and accepted notes fold into the seed/log.
**The WOW:** it's your part, in your hand — a musical assistant one ask away, and it never touches a
note you didn't approve.
- **Sends:** `track step <i> <note> <vel> <gate> …`. **As-built:** only the plain piano-roll/step
  editor is spec'd today (the Sequence Edit canvas is a **PLACEHOLDER**, §4.7); the pull-only copilot
  is **deferred for v1**.

### Flows plumbed or spec'd but NOT reachable from the GUI today
- **Accompany (`midi-source load <path>`)** — load a MIDI melody → band plays under auto-detected
  chords. The session layer is plumbed (routes a path over a dedicated ring to
  `Shell::load_midi_source`, failures surface as `kError`), but **no panel, menu item, or file picker
  calls it.**
- **Restyle (`restyle <style>`)** — transform an imported melody into a genre idiom. The shell verb
  exists but no GUI control sends it and the in-process translator does not translate it (untranslated
  lines yield an `kError` note).
- **Export SMF (`export-smf`)** — the shell verb exists; **no GUI affordance** (File ▸ Save disabled),
  not translated in-process.
- **MIDI monitor** — the TUI has a rich monitor; the GUI has **no MIDI-monitor zone** (`midi-out` →
  stdout only).
- **Groove / arp panels, style/section chooser** — exist only in the TUI (`groove_view.*`,
  `arp_view.*`, `style_chooser.*`); no GUI equivalent.

---

## 8. Interaction model, states & edge cases

**Mouse-first.** Buttons, checkboxes, tree nodes, combos, radios, drag-and-drop (browser style → grid
cell). No custom canvas interactions (grid launch and seqedit canvas are inert). **Keyboard:** only
ImGui defaults; the advertised Ctrl+P is a *label only*, the TUI backtick chooser is not ported; no
focus/tab-order model beyond ImGui's implicit widget order. **Modes:** none — a single screen,
everything visible at once (the GarageBand-immediacy posture); the only "mode-like" toggles are View ▸
Intention/Parts and the seqedit piano-roll/step radio. **`PanelManager`** lives in the **TUI**, not in
`gui-sonotron` — the GUI has no PanelManager and no tab-order/focus seam. **Quit safety:** File ▸ Quit
and window-close close the GUI only; `quit`/`exit` are blacklisted at the session boundary.

States per panel:
- **Transport:** *disconnected* → red dot, buttons still clickable but sends dropped/queued;
  *connected/stopped* → green dot, `stopped`, `bar -- . beat --`; *playing* → scrolling bar·beat;
  *paused* → playhead frozen at last position (only `stopped` parks it to 0). Mid-session connect while
  already playing: a stray `kBeat` forces the label to `playing`.
- **Browser:** empty search → all 16 styles; filtered → case-insensitive substring subset;
  Clips/MIDI-seqs always `(none authored yet)`.
- **Grid:** at rest → all cells `.`; content dropped → cell shows the style name AND registers a real
  `ClipMatrix` clip; a filled cell click sends a real `launch clip` and reads back armed/playing/
  queued-stop as "lit" from the core's `clip` event (§4.4/§6); scene count clamped 3–8 (`+ Scene`
  hidden at 8).
- **Intention:** at rest / no chord → disabled `follows --`, `next --`, header `(at rest)`; pending
  staged at rest → amber `next` shows even while grey elsewhere; active + current → green `follows`;
  energy/tension/valence always disabled zeros.
- **Parts:** default → all M/S off, all `gm --`; another client's mute is **not** reflected (no
  readback); solo does not visibly dim other rows.
- **Sequence Edit:** always the placeholder canvas regardless of part/clip/view/rec toggles.
- **Whole screen (no style loaded):** nothing blocks — every zone renders at rest; there is no "load a
  style first" gate.
- **View toggles:** hiding Intention and/or Parts removes them and the right rail redistributes; if
  both hidden, col 2 vanishes and browser/grid widen.
- **Stale/hand-edited `layout.json`:** an unknown zone `id` falls through to a titled empty frame; a
  bad `font_size` falls back to 13.

---

## 9. The brain interface — `BrainSession`

A small GUI-side interface hiding **whether the brain is in-process (default) or across the UDS
socket**. The physical transport is a swappable detail behind an abstract command/event/snapshot
boundary — not baked into the panels.

```cpp
namespace sonotron {

struct BrainEvent {                    // one decoded inbound event (JSONL parsed here)
  enum class Kind { MidiOut, Chord, Section, Transport, Warn,
                    ChordFollowed /*additive, gap P0-1*/, Beat /*additive, gap P0-2*/,
                    Clip /*additive, Phase-5 Item #2, shipped*/ };
  Kind kind;
  // small POD payload: port, tick, name/code, note strings … panels never see raw text
};

struct BrainSnapshot { /* style/section/key/bpm, per-part mute/solo/program, groove/arp,
                          follow mode — every field optional/"unknown" until the snapshot gap lands */ };

class BrainSession {
 public:
  virtual ~BrainSession() = default;
  virtual void send(std::string_view command_line) = 0;      // one L1 line; refuses quit/exit
  virtual void poll(std::vector<BrainEvent>& out) = 0;       // non-blocking drain, once per frame
  virtual const BrainSnapshot& snapshot() const = 0;         // best-effort; may be mostly-unknown
  enum class Status { Disconnected, Connecting, Connected, Error };
  virtual Status status() const = 0;
};

}  // namespace sonotron
```

- **Abstraction boundary = text-command in / decoded-event out** — mirrors the *shipped* contract.
  Command ack/correlation (§10) is not modelled yet; addable without breaking callers.
- **`UdsBrainSession`** — `AF_UNIX`/`SOCK_STREAM`, newline framing, `kMaxLineLength = 4096`, connects
  to the path passed as `cli-arrangrr --control /path.sock`. Non-blocking drain in `poll()`; a slow
  GUI drops events rather than stalling. `send()` blacklists `quit`/`exit`.
- **`InProcessBrainSession` — the shipped default backend.** With no `--control`, an
  `InProcessBrainSession` runs the real arrangrr `Shell` + `AlsaMidi` + tick clock on a **dedicated
  engine thread inside the GUI process**, auto-wiring `port open in in0`, `port open out out0`,
  `thru in0 out0`, exchanging `Command`/`OutEvent` over SPSC rings (`src/spsc_ring.hpp`) and bridging
  `OutEvent`→`BrainEvent` in-process (`brain_event_from_outevent.cpp`). Panels are unaffected — they
  only ever see the abstract `BrainSession`, exactly the swap this abstraction was built to allow. Both
  backends surface the same 8 event kinds and accept the same six command shapes; the in-process one
  additionally routes `midi-source load <path>` over a path ring but nothing calls it. Per-OS MIDI
  (ALSA/CoreMIDI/WinMM) lives in this host layer, **never** in a pure ImGui client over the socket.
- The JSONL decoder lives GUI-side, scoped to the shipped shapes plus the additive ones — same "small
  local parser for a trusted schema" precedent as `layout_json`.

---

## 10. Required additive brain work / wire gaps

The restart's key decision: **build the foundation at the root, not the GUI shortcut.** The Repeat
Zone, the harmony visualiser, and the playhead all depend on core work the GUI must NOT fake. All
additive to the frozen v1 ABI: none touches an existing id, none breaks the freeze. **P0 = blocks the
central live surface.**

**Front-of-line batch (built before/with the GUI zones that depend on them; all three are now
shipped):**
1. **[P0, shipped] Followed-chord event.** `chord play` and live detection change harmony *silently*;
   the only `chord` event is the recorded-sequencer path. `kChordFollowed {current, pending, valid,
   source}`, fired on every commit (manual / detect / sequencer / bar-promote). Lights the Intention
   green/amber visualizer.
2. **[P0, shipped] Beat/position heartbeat.** `Transport::position()` (bar/beat/tick) was in-process
   only. `kBeat`/`kPosition` (and `kTransport` firing on start/stop/continue) light the playhead and
   bar-progress readout.
3. **[P0, shipped] Clip/scene primitive + launch-quantize (nodes 6000/8000, brought forward).** A
   first-class clip object (`ClipMatrix`, `abi.hpp`'s `kClipAdd`/`kClipLaunch`/`kClipStop`),
   `launch/stop clip <id> quantize <n>` and `launch scene <n> quantize <q>` verbs, and a `clip` state
   event, launch-quantize honoured by the core clock. The Repeat Zone binds to this real primitive —
   the GUI does **not** ship the throwaway launcher once feared (Phase-5 Item #2, §4.4; landed across
   `3398f04`/`f531d8f`/`9ce480d`/`f1fa7d7`/`9382c07`).

**Follow-on (after the front batch):**
4. **State-on-connect snapshot.** All Params are write-only, `Op::kGet` is unwired; a GUI attaching
   mid-session cannot learn current style/key/mutes/groove/follow-mode. Add a snapshot-on-connect or
   per-domain readback. Makes reconnection honest. *This is why the GUI drops `param-state` echoes
   today (§6) — no live mirror of mutes/style/groove/arp.*
5. **Positive ack / request correlation.** No success ack today; `Command` has no correlation field.
   Add an ack event. Polish.
6. **Persistence/recall (node 8000).** Saving the grid/Scene/Song beyond the shipped `scenes.json`
   host-side rename/section storage (§4.4); full Song/Scene recall still folds into this follow-on
   item. Until it lands, the GUI stores its grid/scene names in its own config, not the core.

One further additive proposal flagged but lower priority: a **held-notes / detector state event** ("3
of 3 fingers held" is in-process introspection only). Order: **1, 2, 3 up front** (all three shipped),
then 4/5/6.

**Roadmap orphan.** The `ScaleDegree`/`RelativeInterval` style-data extension (the relative-pattern
editor's prerequisite) has no node number and needs one (likely under **~3300**), independent of when
the GUI builds its editor.

### 10.1 Corelli implementation seam for `kChordFollowed`
`FollowedContext` is a pure, dependency-free value type and MUST stay so — the emit does NOT go inside
it. It belongs in `engine.hpp` (where `EventSink sink` is already in scope) at **four** call sites —
`chord_play` (`kManual`), `fire_chord_seq` (`kSequencer`), `observe_chord_input` (`kDetect` — its
signature must grow a `sink` param), and the `commit_bar` bar-promote — funnelled through a single
private `Engine::emit_chord_followed(Producer, EventSink)` helper so the four sites cannot diverge.
The bar-promote also needs a small additive field `Producer m_pending_source` in `FollowedContext`
(still no-heap) so `source` is honest on promote. `OutEvent`'s 16 bytes hold current+pending+valid+
source comfortably (~20 bits of 40 available), packed like the existing `kChord`. Pinned in
`test_abi_frozen` + a golden, rendered in `jsonl.cpp`.

---

## 11. Decisions (locked)

Resolved 2026-07-10 in review of the workstation screen:
1. **Clip primitive.** Wait for the real clip in the core, but **bring it forward** — build the
   first-class clip/scene primitive (§10 item 3) as front-of-line core work; do **not** ship the throwaway
   GUI-side launcher. The Repeat Zone binds to the real primitive. (Shipped, Phase-5 Item #2 — §4.4.)
2. **Two P0 wire gaps.** **Build them, do not degrade.** `kChordFollowed` and `kBeat`/position are
   front-of-line; the harmony visualiser and playhead are real from day one, not inferred. (Both
   shipped.)
3. **Layout engine.** **Invest in the nested vertical split now** — the right rail stacks Intention
   over Parts in one column; no full-width-rows fallback.
4. **First transport impl.** `UdsBrainSession` matches the shipped contract; the `InProcessBrainSession`
   proved the swap and is now the default backend.
5. **No ack / no snapshot.** Follow-on additive work (§10.4–5); event-reduction covers the interim.

Owner decisions locked 2026-07-07 (the pre-restart antecedents this inherits):
1. **Primary persona for the first GUI = B (the producer), extended to the improvising performer.**
   Persona A (live finger-steering) remains the product's soul, not the audience of the music-stand.
   Parts-mixer / groove / reseed are first-class; the harmony surface is re-justified as B's close-up
   sketching aid; the no-central-chord-pad ban stands.
2. **ABI: the additive `kChordFollowed` event is approved** — the core announces the followed chord on
   every change, from any source, so GREEN/AMBER is always truthful including single-finger. A small
   dependency-free CORE change, additive-only, chosen over the zero-ABI optimistic-only slice.
   Transport heartbeat was P1 at the time; it has since shipped as well.
3. **Toolkit = Dear ImGui, settled by D38.** No fresh multi-toolkit evaluation; confirm ImGui + the
   concrete GLFW + OpenGL3 backend set.

---

## 12. Preferences

Reachable via **Edit ▸ Preferences** (unbuilt today; menu item disabled). v1 contents:
- **Font size** (the existing `font_size` knob, live).
- **Control socket path** (what `UdsBrainSession` connects to; matches `--control` on the brain).
- **Default port/channel** for new parts/routes.
- **Default launch quantize** (1 bar / 2 bars / instant).
- **Theme** (dark default; light later).

---

## 13. Deferred / out of scope for v1

- The Director and the intention rail beyond a read-only view (node 10000).
- Audio realisation / colour (melodd).
- The pull-only sequencer copilot (propose/adjust/add).
- Song/Scene persistence and recall (node 8000).

---

## 14. Design brief for a pixel-perfect pass

Prompt for an external design pass. Design the **aspirational complete workstation** (the target), but
ground it in the real zone skeleton above, and mark each element **LIVE-TODAY vs TO-BE-WIRED** so the
design doubles as an implementation target.

**Role.** Senior product/UI designer for professional audio & live-performance software (Ableton Live,
NI Maschine, Korg/Yamaha arrangers, Elektron). Produce a pixel-perfect visual design + design system
for a confident, musical, dark, immediate-mode UI where **state is visible AT REST** (never
hover-only).

**Layout to design (exactly the §3 skeleton).** Transport bar — LIVE: numeric `bar N · beat M · PP`,
Play/Stop/Continue, Panic, connection status; TO-BE-WIRED: tempo/BPM, meter, key, clickable style
chip, graphical bar-progress. Working row — Browser (style library; `style load` on select; drag onto
a grid cell is real); Repeat-Zone grid THE HERO (launchable clip cells in scenes across part rows;
launching is real and shipped — `launch clip`/`launch scene`, real armed/playing/queued-stop readback
off the core's `clip` event (§4.4) — but rendered today as a binary lit/unlit cell, not the distinct
armed(amber)/playing(green)/queued-stop states; design that full lifecycle visually distinct;
drop-to-fill is real); right rail — Intention (LIVE harmonic
visualizer: `follows` green + `next` amber + key; TO-BE-WIRED energy/tension/valence sliders) over
Parts (role/mute/solo/activity; mute/solo real optimistic-local — design a clear "commanded" look;
TO-BE-WIRED per-part GM + volume). Sequence Edit — piano-roll/step canvas (placeholder today; design
the editing surface + a playhead synced to transport).

**Live bindings to make feel alive.** `kChordFollowed` → Intention (committed=green, pending=amber,
at-rest=neutral); `kBeat` → transport playhead + (target) bar-progress + seqedit playhead; (target,
once wired) engine state echo → mute/solo readback, style chip, groove/arp values.

**Zones to design as the TARGET** (exist in the sibling TUI, not yet in the GUI; label "to be brought
into the GUI"): Groove panel (swing/humanize/accent/timing knobs with live values); Arp panel;
Style/section chooser (variation A/B/C/D, intro/fill/ending); MIDI monitor (live scrolling outgoing
MIDI); Accompany affordance (load a MIDI melody → band under auto-detected chords); Restyle affordance;
Export (dump session to `.mid`).

**Deliverables.** (1) Design system / tokens: semantic palette (base/surface/elevated, text tiers,
live-green, pending-amber, warn-red, per-part-role hues), type scale (a mono for live musical/numeric
data + a humanist sans for labels), spacing, radii, borders/elevation, focus states. (2) Component
library: buttons; mute/solo toggles; knobs & sliders with readouts; the clip/launch pad in ALL cell
states; the part row; the harmonic-visualizer widget in all states; transport + moving playhead +
bar-progress; meters; browser list item; style chip; MIDI-monitor row; docked/tabbed panels;
connection indicator. (3) The main screen in full fidelity in two states — AT-REST and LIVE (playing;
a chord followed=green; a clip armed=amber; a clip playing=green; a part soloed; playhead mid-bar).
(4) Each zone in its meaningful states (empty / active / disabled / warning / placeholder-vs-wired).
(5) Annotations mapping every element to its data source (engine event or L1 command) and marking
LIVE-TODAY vs TO-BE-WIRED. (6) A short rationale tying the visual language to live-performance
legibility.

**Constraints.** Immediate-mode-friendly (state at rest, fixed hit targets, works without animation
but specify motion where it aids performance — playhead, cell launch, pending pulse); stage-legible
(high contrast, large live readouts, unambiguous status color); deliver as a high-fidelity
self-contained artifact plus the token/spec list. Order: design system → main screen (both states) →
component sheet → per-panel states.

---

## 15. Product roadmap — how not to be clunky, dream features, prioritized additions

The direction — a cross-platform desktop **generative arranger / live instrument** *around* arrangrr
whose signature gesture is *the machine playing itself* (a deterministic Director, D37, not AI) — is
the intended cash-out of the architecture, not a new demand on it. Two disciplines decide whether it
lands: **kill the word "DAW"** (§1), and **obey the D37 sequencing law for the GUI too** — a co-pilot
surface has nothing to steer until the core's tunable parameters (scale-degree melody, voice-leading,
step density, scenes) exist. The GUI's structural editors and adapter can start now; the autoplay apex
waits for its knobs.

### 15.1 UX principles that keep it from becoming clunky
Derived from the documented failure modes of Zrythm, Yamaha Genos / Korg Pa, Band-in-a-Box, Ardour /
LMMS:
1. **Never a silent mode.** Every editing/inspection surface keeps the music audible — no "enter
   step-record and lose the sound" (the Pa4X sin). The core is always-running and authoritative; the
   GUI must never gate playback.
2. **Live gestures are always one action away.** Play/stop, section change, chord detect, part
   mute/solo, co-pilot targets — top-level, not menu-buried. Split the labour: the TUI (or a compact
   GUI transport bar) for live keyboard-first control; the GUI's rich 2-D views for
   structure/editing/inspection — do not duplicate.
3. **Progressive disclosure (D9).** Default view = transport, current chord, current section, co-pilot
   state, parts. Everything else (relative-pattern editor, routing matrix, MIDI-learn) is a panel you
   open, not clutter you wade through.
4. **Immediate-mode is an anti-bug asset.** ImGui redrawing every frame from the mirrored event stream
   structurally prevents the GUI/state desync class; the core wins every frame. Do not add GUI-side
   authoritative state.
5. **Platform honesty (the Zrythm lesson).** The GUI touches no audio/MIDI subsystem — it speaks the
   socket; the daemon owns MIDI per-OS. That single boundary makes "unusable on Wayland/Pipewire"
   impossible for the GUI.
6. **No feature-grafting.** The bounded/no-heap core is a cultural firewall against the Band-in-a-Box
   "forest of hidden commands": a feature that cannot be bounded does not enter the core, and the GUI
   grows panels, not core scope.

Cross-platform packaging shape: **two processes, one boundary** — a **host daemon** (links `arrangrr`,
owns per-OS MIDI + an optional soft-synth sink, exposes the adapter) and the **ImGui client** (pure UI
over the socket, links no core). Per-OS MIDI HAL in the daemon (ALSA/CoreMIDI/WinMM, or RtMidi/PortMidi
behind it). Audio-out as an optional bundled soft-synth sink, never on the timing-authoritative path.
Adapter transport cross-platform: UDS on Linux/macOS, a named pipe or localhost TCP on Windows carrying
the *same* JSONL. Packaging: AppImage/Flatpak (Linux), signed `.app` (macOS), portable exe/MSI
(Windows) — daemon and GUI package together but stay separable so killing the GUI never touches
playback.

### 15.2 Dream features nobody has done right, and why arrangrr can
1. **Reproducible generativity — "seed as a musical object."** Audition variations, recall the exact
   one byte-for-byte next week. Enabled by D16 seeded PRNG + total-order determinism (D29): every
   variation is an addressable, recallable fact.
2. **Non-destructive harmonic what-if, computed ahead of the playhead.** Hear what a loop would do
   under a different progression *before* committing. Enabled by the `advance_ticks`-pure headless core
   + virtual clock: a second host-side instance renders the hypothetical and diffs it against live.
3. **The reversible looper (capture → re-harmonizable degrees).** Loop a live phrase, later
   re-harmonize/transpose it as if written relative to the chords. Enabled by capturing against the
   live `ChordState` into the D28 degree-relative form — invertible by construction.
4. **"No wrong notes" extended to the human.** Hand a non-player a keyboard over a running band and let
   them solo with *musical* results. Enabled by pointing the NTT resolver at *live input*, not just
   style patterns — the whole keyboard chord-aware in real time.
5. **Style morphing / interpolation.** Blend two styles (30% funk drums, 70% bossa feel) instead of
   hard-switching. Enabled by styles being POD role tables — a role-wise parametric crossfade over a
   shared groove space makes "between two styles" an addressable point.
6. **Polyphonic arrangement (per-role section state).** Bass on Var A while drums take a Fill and the
   pad holds a Break — arrangement as independent role-lanes. Enabled because roles are already
   independent, routed, mutable.
7. **Version-control for performance.** Record a live set as a reproducible command log, replay it
   exactly, branch from any bar, audition an alternate take deterministically. The replayable L0
   protocol that feeds the golden harness *is* a performance log — "git for a set" falls out of
   D16/D17.

### 15.3 Twenty prioritized additions (non-obvious, arrangrr-specific)
Ordered by musical + strategic leverage; each tagged `[core]` / `[host]` / `[both]`. None are trivial
DAW absences — every one exploits NTT / role-arranger / generativity / freestanding core / piano→chord
/ modularity.
1. **Scale-degree / RelativeInterval / ChordGesture pattern model** — extend `StyleEvent` beyond
   chord-tone-only (thread `Key` into `resolve()`) so bass can walk and leads can be scalar. Highest —
   unblocks melodic parts, the Director's complexity axis, the whole co-pilot's range. `[core]`
2. **Voice-leading + harmonic spillover resolver** — common-tone retention and minimal-motion voicing,
   `ChordGesture`-driven, replacing root-position re-stack. The single largest jump in perceived
   quality. `[core]`
3. **Generative Director as a live co-pilot surface (D37)** — the deterministic trajectory engine in
   the core; an XY/target surface + target-timeline in the GUI. Gated on items 1/2/5/6 giving it knobs.
   `[both]`
4. **Reversible looper (capture → degrees relative to chord-at-capture)** — captures store both
   absolute and degree-relative forms. Must follow item 1. `[core]`
5. **NTT-constrained live input ("no wrong notes" for the human)** — map live keyboard input through
   the NTT/scale resolver over the running chord, whole-keyboard or per-zone. Reuses item 1's resolver.
   `[core]` (zone config `[host]`)
6. **Reproducible variation browser (seed as first-class object)** — audition N deterministic
   variations of groove/arp/fills, lock + recall the chosen seed. `[both]`
7. **Advanced step params (probability / ratchet / tie / rest / conditional-trig / micro-timing,
   seeded)** — the Elektron-style parameter-locks absent from `Step`; the raw material the Director's
   density axes steer. `[core]`
8. **Scenes / song mode (snapshot + chain of sections/mutes/routing/targets)** — recallable performance
   snapshots chained over time; prerequisite for "co-pilot arranges a whole song." `[core]` (editor
   `[host]`)
9. **Per-port latency compensation (adaptive bus scheduler)** — per-port integer-tick offset so a chord
   on DIN+USB doesn't flam. `[core]`
10. **Chord-context- and section-aware arpeggiator** — feed the ArpeggiatorEngine from the live
    `ChordState` (not only held keys) and hook rate/octaves to section/Director. `[core]`
11. **Live chord confidence + interpretation suggestions** — `ChordDetector` returns a confidence +
    alternative readings ("you played X — meant Xm7 or X6?"). `[both]`
12. **Style morphing / role-wise interpolation** — parametric crossfade between two styles per role +
    shared groove space. `[core]` (control `[host]`)
13. **Polyphonic arrangement (per-role section state)** — decouple section state per role. `[core]`
    (surface `[host]`)
14. **Deep MIDI-learn / parameter-mapping matrix** — expose the stable param-ID space (D17c) as a
    learnable matrix so any controller drives any knob live. `[both]`
15. **Retroactive capture ("grab the last N bars")** — an always-on bounded ring; pairs with item 4.
    `[core]`
16. **Non-destructive re-harmonization preview (offline against a virtual clock)** — a host-side second
    core instance renders "this loop under that progression" ahead of the playhead. `[both]`
17. **Slash-chord / on-bass resolver** — the bass-note policy no code implements yet. `[core]`
18. **Deterministic song-form co-pilot (Director targets over a chord sequence)** — auto-shape
    intro/verse/chorus/bridge dynamics by sequencing Director targets; downstream of items 3 + 8.
    `[core]` (authoring `[host]`)
19. **Cross-role register/collision auto-spacing** — use `kRoleAnchor` (D36) to spread register clashes
    between chord1/chord2/pad; partly subsumed by item 2. `[core]`
20. **MPE-aware expressive input into the harmonizer** — absorb MPE on ingress without touching the
    8-byte `Event`. Narrow audience. `[core]`

---

## 16. TUI companion spec — piano / MIDI monitor

The sibling host TUI (`components/platform/hostrt/console.*`, `shell.*`, and new files under
`components/platform/hostrt/`) carries the keyboard-first live surface the GUI deliberately does not
re-invent. Feature classification: **host-live + host-tool**. Bound by the same architecture locks as
the GUI wire, restated here because a piano widget and a MIDI log feel like they belong "close to the
engine" and they do not.

### 16.1 Architecture constraints (non-negotiable)
1. **The embedded core stays UI-free.** No terminal rendering, ANSI codes, REPL strings, panel/window
   management, piano-key drawing, log buffers, filter predicates, monitor views, or color/unicode
   logic in `core/`. If a symbol needed here would have to live in `core/`, the feature is designed
   wrong.
2. **All TUI code lives in `components/platform/hostrt/`.** Panel manager, piano renderer, MIDI monitor
   formatter/filter pipeline, key-dispatch, resize handling, color/theme/unicode layers.
3. **The simulated piano is an input device, not a shortcut.** A computer key produces MIDI bytes
   exactly as external hardware would, injected via `Shell::feed_midi(port, bytes)` on the same input
   port — through the same `feed_midi` → `push_midi_in` → router → engine path. It never calls the
   router/engine/note-tracker directly and never writes to ALSA on its own, so it can never become a
   second divergent "note came in" path.
4. **Output stability is sacred.** Enabling the piano/monitor/filters/colors must never change a byte
   of `--format jsonl`/golden output nor of `--script` output. `--clock virtual` and non-TTY behavior
   are unchanged — this whole feature only exists on a real terminal with a live human.
5. **Every host buffer is bounded** — active-note tracker, visual event ring, log ring, per-panel line
   store: fixed capacity, named constant, explicit "when full" policy (reject+warn, ring overwrite, or
   truncate — never silently grow, never crash).

### 16.2 Phasing
**Phase 1 — Foundation (in progress).** Note-name utilities (CDE and DoReMi, `C4 = 60`, `prefer_flats`
toggle) extracted from `jsonl.cpp` into shared host code; a small **multi-panel manager** (fixed panel
ids `help`, `piano`, `filter`; visibility + focus; stacked vertically; per-panel line cap; defined
oversized-content behavior); the canonical **`panel …`** command family (the only place panel lifecycle
lives); a real **help panel** (`help <topic>` atomically sets content and opens; `help open`/`help
close` are removed); a **static pure piano renderer** (a pure function of terminal width + note-name
mode → lines, no input/MIDI/highlight); deterministic tests; an STM32 impact report even though
Phase 1 touches no core code. Explicitly OUT of Phase 1: live key dispatch, MIDI generation, any
monitor buffers, active-note tracking, filters beyond a placeholder id, colors/themes/unicode,
side-by-side layout, benchmarks, any `ScheduledEvent` or `core/` change.

**Phase 2 — Piano/monitor MVP.** The piano becomes playable and the monitor real. Piano commands (§16.3):
`piano octave <N>|up|down` clamped -1..9; `piano channel <1..16>` (1-based UI, stored 0-based);
`piano velocity <1..127>`; `piano keymap`; `piano panic` (piano-originated notes only); `piano view
keyboard|active-notes|event-log`. Note-name mode: `notes names cde|doremi|toggle` — one setting, one
source of truth across piano and monitor. A **live key-dispatch layer inserted before the LineEditor**,
active **only** when the piano panel has focus (raw presses are piano-first when focused; keys behave
as today when focus is elsewhere). Piano shortcuts (piano focus only):

| Key | Action |
|---|---|
| `TAB` | Focus next panel |
| `P` | Close piano panel / return focus to REPL |
| `N` | Toggle note-name mode (CDE ↔ DoReMi) |
| `V` | Cycle piano view (keyboard → active-notes → event-log → keyboard) |
| `C` | Clear buffers (active-notes + event-log, not the panel) |
| `[` / `]` | Octave down / up |

`Z`, `D`, `F` are optional Phase-3 extension shortcuts, implemented only if trivial.

**MIDI generation** — pressing a mapped key calls `Shell::feed_midi` on the currently-selected input
port. Note-off policy is **toggle**, not press/release, because terminals deliver no key-release: first
press = Note On, same key again = Note Off. A stuck note is recovered with `piano panic`; re-pressing a
different key for the same pitch at a different octave is a distinct, independently tracked note.

**Momentary key mode (H3, kitty keyboard protocol).** The toggle is the *fallback*, not the ceiling. A
raw TTY delivers a byte on press only; honest press/release polyphony needs the **kitty keyboard
protocol** (CSI u progressive enhancement). Two modes in a shell-side `PianoKeyMode { kMomentary,
kToggle }`: **`kMomentary` (default)** — a note sounds while its key is physically held and stops on
release, driven by `Shell::piano_key_event(char, bool pressed)` fed from parsed kitty events
(`\x1b[<code>;<mods>:<event>u`, event 1=press 2=repeat 3=release); autorepeat ignored, polyphonic and
correct. **`kToggle`** — the Phase 2 behaviour. **SPACE** in piano focus toggles the two modes and logs
the new mode (a mode switch only, never a musical key). Flags pushed: `0x1|0x2|0x8 = 11`, enabled with
`CSI > 11 u`, popped with `CSI < 1 u`; parser in `components/platform/hostrt/kitty_keys.{hpp,cpp}`. Enabling is
**scoped to piano focus** and gated behind `isatty` (pushing globally would reroute every REPL
keystroke). **Graceful degradation is mandatory** — on a terminal without the protocol the plain-byte
toggle path still runs; both coexist across terminal types.

**Range enforcement** — a keymap+octave combination producing a note outside 0..127 is **rejected**
with a clear message, never silently clamped or wrapped.

**Monitor models.** `ActiveNoteTracker` (host-only, distinct from the core Note/Voice Tracker):
bounded **128** concurrently-tracked notes; when full a new note is **rejected with a warning**, never
evicting (eviction would make the active-notes view lie). `PianoVisualEventBuffer`: a bounded ring of
**32**, at most **5** rows rendered at once; no animation/frame-timer — repainted on state change only.
`MidiLogEvent { tick, same_tick_index, port, msg }` — `same_tick_index` disambiguates same-tick events
(consistent with D29 total-order tie-breaking). The monitor pipeline is strictly staged: **`MidiLogEvent`
→ filter → formatter → renderer**, each stage replaceable and testable; the renderer draws exactly what
the formatter hands it. Guiding rule: **honestly report what is observed** — no inferred state, no
"probably a chord" guesses, no smoothing over dropped/out-of-order bytes.

**Duration formatting** at the canonical PPQN of 960 (D27): 120→1/32, 240→1/16, 480→1/8, 960→1/4
(exact); anything else → nearest value prefixed `~`. The formatter never fabricates false precision.

**Minimal filters** (data, not code): by channel, by port, by event kind (`note-on`|`note-off`),
`clear`. **View options** (also data): `show note-names|note-numbers|velocity|channel|port on|off`,
`show-octaves boundary|all|none`, `clear`. Filters and view options are always plain data consumed by
the formatter/renderer — never hardcoded `if` branches — which makes Phase 3's richer filters additive.

**Phase 3 — Deferred** (out of scope until Phase 2 has shipped and been used). Colors (`--colors` /
`colors on|off|toggle`); themes (`--theme`, `theme list|set|current`; styles as semantic `UiRole`
values resolved by the active theme; min set `default`, `mono`, `high-contrast`); unicode
(`--unicode`, runtime toggle; ASCII fallback always available and the default when capability is
unknown); side-by-side layout (`Z` toggles stacked↔side-by-side, auto-falling-back to vertical when too
narrow); GM drum-name toggle (channel 10); richer views (`channels` per-channel summary); richer filters
(note lists, ranges, drums-vs-melodic split, velocity thresholds); bar-position display as
`@tick#sameTickIndex` (never a fake decimal bar:beat); benchmarks/stress tools (host-tool); human
MIDI-log enrichment (note names + GM drum names).

### 16.3 Command grammar (canonical, no aliases)
All commands are **L2 surface** syntax (verb-first, space-separated, no dotted paths). One name, one
meaning.

```
panel list | open <panel> | close <panel> | toggle <panel> | close all
      | focus <panel> | focus repl | focus next | status | help
piano octave <N>|up|down | channel <1..16> | velocity <1..127>
      | keymap | panic | view keyboard|active-notes|event-log
notes names cde|doremi|toggle
view  show note-names|note-numbers|velocity|channel|port on|off
      | show-octaves boundary|all|none | clear
filter channel <1..16>|all | port <name>|all | event note-on|note-off|all | clear
help                              # opens help panel with the default/overview topic
help <topic>                     # updates help content to <topic> AND opens it
```

`<panel>` is one of the registered ids `help`, `piano`, `filter` (Phase 2 adds content to `piano`, no
new ids). A panel never invents its own open/close verb; `piano` commands never implicitly open the
panel (that is `panel open piano`). `help` is a deliberate, narrow exception that mixes content +
open; it has no `help close` (use `panel close help`) and no `help open <topic>`.

**Forbidden duplicates** (a handler for any of these is a bug; use the canonical form): `piano on` →
`panel open piano`; `piano off` → `panel close piano`; `piano show`/`piano hide` → `panel open/close
piano`; `help open` → `help <topic>`; `help close` → `panel close help`; `help open <topic>` → `help
<topic>`; any bespoke `<panel> toggle` → `panel toggle <panel>`.

**`chord detect` — live piano→chord harmonizer surface (D34).** The one command here whose *engine* is
**not** host-only: the recognition is a core-portable `ChordDetector` + `ChordEngine::set_context`, so
it is exempt from §16.1's "core impact: none" rule (that governs the piano/monitor TUI, not this
feature). Only the **host-live command and panel surface** lives here.

```
chord detect on                  # notes held on the input port re-harmonize the running band
chord detect off                 # disable live detection
```

With `chord detect on` and the arranger playing, holding a chord on the simulated piano (or any
external keyboard on the input port) makes the whole band follow it — bass and comping re-harmonize in
real time. The held keys still sound through normal routing; detection only *steers*, so there is no
doubled voicing. A chord is recognized once **≥3 notes** are held (lowest = root). **Chord-memory
(hold-last):** releasing the keys does **not** stop the band or clear the chord — dropping below 3
held notes leaves the **last** recognized chord in place until you hold a new one. The `kChords` panel
renders the live-recognized chord name together with the detect on/off state, `(no chord)` before the
first recognition — a pure readout, it does not itself recognize. **Scope (MVP):** the whole keyboard
acts as chord input while ON; keyboard split/zones (low = chords, high = melody) is a documented future
refinement (core-portable, D34).

### 16.4 Piano rendering spec (two-row keys)
Rendered as **two rows of key cells** — a computer-keyboard binding centered directly above the
musical note label it triggers. This two-row shape is constant across all width tiers; only cell width,
spacing, and black-key placement scale.

- **Note-label spacing:** white-key labels are SPACED (`C 4`, `Do 4`) purely for column alignment;
  black-key labels are NOT spaced (`C#4`, `Do#4`); **event/log note names are always unspaced** (`C4`,
  `Do#4`) — one note-name utility, two presentation rules.
- **Octave display:** every visible key shows its octave by default (`show-octaves = all`). For the
  piano renderer, `boundary` is treated as an alias for `all` (keys are too dense for "only at C");
  `none` hides per-key octaves but the panel header still states the base octave.
- **Keymap (fixed, Phase 2 — no runtime remapping):** two rows as a single continuous 18-semitone span
  from the current base octave. White row `A S D F G H J K L ; '` → offsets 0,2,4,5,7,9,11,12,14,16,17.
  Black row `W E T Y U O` → offsets 1,3,6,8,10,13. **`P` is never a musical key** in any keymap — a
  hard exclusion, permanently reserved to close the piano / return to the REPL (losing the
  panic-adjacent "get me out" key to a remap would be a live hazard).
- **Width tiers** (re-selected on every resize): **Wide** (`kWideMinColumns = 76`+) draws black keys
  horizontally between their white neighbors, matching physical geometry; **Compact**
  (`kCompactMinColumns = 56` up to 75) the same two-row idea, tighter; **Minimal** (below 56) abandons
  geometry for grouped plain text, one line per row (`black: W/C#4 …` / `white: A/C 4 …`); **Below
  minimal** shows a single honest fallback line ("piano: widen terminal to view keyboard") — never a
  broken, wrapped, or clipped rendering.
- **Implementation shape:** a small pipeline — **binding → label → row composer → lines**: the fixed
  keymap maps a key to a semitone offset; the shared note-name utility + `format_keyboard_note_label()`
  produce a spaced/unspaced label; a row composer lays out cells at the tier's width/gap constants;
  plain lines go to the panel manager. All tier thresholds and geometry constants live in a
  `piano_layout::` namespace as named `constexpr` — no bare numeric literal for a threshold, cell
  width, or gap in row-composer code.

### 16.5 Resize robustness contract
**State is the source of truth; rendered lines are regenerated on every geometry change, never
patched.** Resize flow: (1) resize detected (SIGWINCH or polling, host-only); (2) `TerminalGeometry`
refreshed; (3) `ConsoleLayout` recomputed (named rows: log-top, log-bottom, panel-top, panel-bottom,
status, input); (4) every visible panel re-rendered **from its retained state**, not from previous
lines; (5) output truncated per-panel to the new area; (6) scroll region updated; (7) full repaint.
Invariants across any resize/maximize/restore: the renderer tier is **re-selected** every time; resize
**never resets** piano/panel/filter state and **never clears** the active-note tracker or event buffer
(a resize is not a `piano panic` or a `view clear`); resize has **no effect** in script/non-TTY mode
(the path must be unreachable there); the status bar and input line are **always visible** (the last
thing sacrificed, never the first); too-small terminals degrade to the "below minimal" line rather than
garbled art.

### 16.6 Style rules (enforced for all new code)
No magic numbers (every meaningful literal is a named `constexpr` in a purpose-named namespace —
`piano_layout::`, `ansi::`, `midi::`, `layout::`); no raw ANSI literals in implementation (confined to
named helpers, and from Phase 3 resolved through the `UiRole` → theme indirection); protocol fragments
(L0/L1 wire, MIDI byte sequences) live only in dedicated named helpers; non-trivial bodies belong in
`.cpp`, not headers; blank lines separate logical phases within a function; comments explain policy and
intent, never decode a literal; members use the `m_` prefix (no trailing underscore), braces always
present. The existing `console.cpp` predates these rules (raw ANSI inline) and is **not** retrofitted
under this feature — a separately filed task.

### 16.7 STM32 impact discipline & measured baseline
Every change ships with an impact table covering: files touched in `core/` (should read "none" for this
whole feature), new core RAM/flash, new host-only RAM, ARM build status (must stay green per D3), JSONL
output (byte-for-byte unchanged for existing goldens), scheduler behavior (unaffected), MIDI throughput
(unaffected — the piano rides the same input path as hardware, so it cannot change throughput by
construction).

Measured baseline captured at HEAD `4f46bbe` (before Phase 2), as a fixed diff point:

| Quantity | Value | | Quantity | Value |
|---|---|---|---|---|
| `firmware_stub.elf` `.text` | 45,731 B | | `sizeof(Engine)` | 97,496 B |
| `firmware_stub.elf` `.data` | 97,352 B | | `OutScheduler<4096>` | 65,552 B |
| `firmware_stub.elf` `.bss` | 440 B | | `sizeof(ScheduledEvent)` | 16 B |
| `ChordSequencer` | 24,864 B | | `Timeline` | 4,200 B |
| `Track` | 262 B | | `NoteTracker` | 2,560 B |
| `Router` | 168 B | | `Command` | 20 B |
| `OutEvent` | 16 B | | `kMaxTracks` | 16 |
| `kSchedulerCapacity` | 4096 | | | |

Two open core findings (report-only; fixes are separate core tasks): (1) the engine-gate static
`Engine` lands in `.data` (≈97 KB copied from flash at boot) because its members have non-zero
defaults — the fix is zero defaults + explicit runtime init so its pools live in `.bss`; (2)
`sizeof(ScheduledEvent) == 16 B` vs the 12 B D33 budget (64 KB vs 48 KB at capacity 4096) — resolve by
updating the budget to 16 B or shrinking the sequence-number field to `u16`; packing the struct is
**not** acceptable without a dedicated Cortex-M7 alignment/access-cost discussion.

**Saturation ranking** (reach for these limits in order — each saturates before the next binds): (1)
per-port DIN-5 bandwidth, 3125 B/s — a 4-note chord is ~3.8 ms on one DIN port, the first ceiling with
real hardware; (2) scheduler capacity — every gated note occupies two slots (note-on + note-off), so
dense chords burn capacity twice as fast as a naive count suggests; (3) `cancel_note_off` cost —
currently O(N) worst case against live entries; benchmark before optimizing.

**Linux roles** (D2/D7 — Linux is devenv-first, never the primary target): (1) **deterministic
simulator** — goldens, `--script`, `--clock virtual`; this feature has **zero footprint** here and
none may leak in; (2) **host live runtime** — real product usage (ALSA MIDI, the router/arranger being
played); **this is where the TUI and this feature belong** — the piano is a live input device standing
in for hardware, the monitor a live diagnostic view; (3) **authoring/tooling** — style compiler,
pattern editors, benchmarks, never ported to STM32.

---

## 17. Glossary

The path is **captured as a Song; an instant config snapshot is a Scene — different** (DESIGN.md §7):

- **Scene** — an *instant configuration snapshot*: one column of the Repeat Zone launch grid, the set
  of clips that fire together at one moment (a still frame; the Roland/Ableton sense). "Scene ▶ all"
  launches a column.
- **Song** — the *path* through the set captured over time: the ordered, reproducible sequence of
  sections/chords/launches (the film, not a frame). Flow #4 captures a Song; recall/song-mode (node
  8000) persists the whole path.

Do not conflate them.

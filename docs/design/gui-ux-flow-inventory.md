# GUI UX Flow & Screen Inventory — `gui-sonotron` (node 11600)

Status: **survey of as-built reality, 2026-07-13.** An exhaustive, code-grounded map of every zone,
panel, control, state, live binding and user flow in the shipped desktop workstation, to feed a
pixel-perfect UI design brief. Grounded in `docs/design/ux-workstation.md` (product spec),
`docs/design/gui-fase2-mechanical-plan.md` (the G0–G3 panel build), `docs/design/pipeline-p0-mechanical-plan.md`
(the two live P0 events), and the source under `apps/gui-sonotron/`. Every claim cites file:line.

> Read this next to `ux-workstation.md`: that doc is the *intent*; this doc is *what the binary
> actually renders today*. Where they diverge it is called out (§7, Gap list).

---

## 0. One-paragraph orientation

The app is a single ImGui window titled `sonotron`, 1280×800 logical at launch
(`apps/gui-sonotron/main.cpp:154`), HiDPI-scaled by the monitor's GLFW content scale
(`main.cpp:380-387`), monospace (vendored JetBrains Mono NL, base 13 px —
`src/layout_model.hpp:26`). A `File / Edit / View / Transport / Help` main-menu-bar sits above a
borderless full-viewport window (`main.cpp:306-313`) that hosts a **JSON-driven 6-zone grid**. Zones
are pure data (`src/layout_model.cpp::default_layout`, `main.cpp` loads/saves
`$XDG_CONFIG_HOME/sonotron/layout.json`, `main.cpp:78-89`) and are dispatched to panels by `id`
(`src/layout_renderer.cpp:24-38`). The GUI is a **pure client**: it holds no authoritative state, only
a view (`AppState`) reduced from a decoded event stream (`src/app_state.hpp:9-25`), and it talks to
the engine through the abstract `BrainSession` interface (`src/brain_session.hpp`) — either an
**integrated in-process engine thread** (default) or an **external UDS socket** (`--control`),
selected in `main.cpp:403-421`.

---

## 1. Global layout

### 1.1 Window structure

- **Chrome (outside the grid):** the main menu bar, drawn by `render_menu_bar`
  (`main.cpp:236-296`) via `ImGui::BeginMainMenuBar()`. It is *not* a zone
  (`ux-workstation.md:120`, `:170`).
- **The grid:** one borderless `ImGui::Begin("sonotron", … NoDecoration|NoMove|NoBringToFrontOnFocus)`
  covering the viewport work-area (`main.cpp:306-313`), inside which `render_layout`
  (`src/layout_renderer.cpp:69-109`) walks the computed row/cell geometry and draws each zone as a
  titled, bordered `BeginChild` (`render_zone_frame`, `src/layout_renderer.cpp:55-65`).

### 1.2 The named zones (from `ux-workstation.md` §4, as built in `default_layout()`)

`src/layout_model.cpp:54-103` declares exactly six zones. Geometry is relative *weights* normalized
on demand by `compute_rows` (`src/layout_model.cpp:184-228`), not fixed fractions:

| Zone `id` | Title | Row | Col | Span | Width wt | Height wt | Panel dispatch |
|---|---|---|---|---|---|---|---|
| `transport` | Transport | 0 | 0 | full | — | 0.06 | `render_transport_panel` |
| `browser` | Browser | 1 | 0 | no | 0.22 | 1.0 | `render_browser_panel` |
| `grid` | Repeat Zone | 1 | 1 | no | **0.54** | 1.0 | `render_grid_panel` **(HERO)** |
| `intention` | Intention | 1 | 2 | no | 0.24 | 0.38 | `render_intention_panel` |
| `parts` | Parts / Mixer | 1 | 2 | no | 0.24 | 0.62 | `render_parts_panel` |
| `seqedit` | Sequence Edit | 2 | 0 | full | — | 0.34 | `render_seqedit_panel` |

Dispatch by id: `src/layout_renderer.cpp:24-38`.

### 1.3 Visual hierarchy / arrangement

Three horizontal bands top-to-bottom (`ux-workstation.md:119-142`, `layout_model.cpp:48-53`):

1. **Row 0 — Transport bar** (full width, thin: height weight 0.06 → a single line). It is the *only*
   zone drawn without a title+separator; `transport_panel` owns the whole line
   (`src/layout_renderer.cpp:55-63`, comment `:47-54`).
2. **Row 1 — the working row** (tall: browser/grid carry height weight 1.0 so this row dominates,
   `layout_model.cpp:70,78`). Three columns left→right:
   - col 0 **Browser** (0.22 width),
   - col 1 **Repeat Zone / grid — the HERO** (0.54 width; widest by design,
     `ux-workstation.md:162,201-204`),
   - col 2 **right rail**, a **nested vertical stack**: `intention` (top, 0.38 of the column) over
     `parts` (bottom, 0.62). Both zones share `(row 1, col 2)`; `compute_rows` groups same-(row,col)
     zones into one cell's vertical stack (`layout_model.cpp:107-133`, `:154-180`), rendered by the
     stacked-column path in `render_layout` (`src/layout_renderer.cpp:83-99`). This nested split is
     the G1 layout-engine investment (`gui-fase2-mechanical-plan.md:61-72`, decision 3
     `ux-workstation.md:500-501`).
3. **Row 2 — Sequence Edit** (full width, height weight 0.34).

The **hero** is the Repeat Zone / launch grid (`ux-workstation.md:116,162,201`). The Intention rail
is explicitly demoted and optional (`ux-workstation.md:220-225`).

### 1.4 Responsiveness / DPI

- Layout is weight-relative and re-normalized every frame against `GetContentRegionAvail`
  (`src/layout_renderer.cpp:70-81`), so zones **reflow** proportionally as the window resizes; there
  are no fixed pixel panels except the grid's 70 px "Part" name column
  (`src/grid_panel.cpp:67`) and combo widths in the seqedit toolbar (`src/seqedit_panel.cpp:15`).
- Font atlas is rasterized once at startup at `content_scale × base_font_size`
  (`main.cpp:199-213`, `:380-387`); style metrics scaled with `ScaleAllSizes` (`main.cpp:383`).
  **Not** live per-monitor-move rescale (`main.cpp:377-379`).
- No layout persistence of *user* resizing (zones are static frames, no drag splitters,
  `main.cpp:487-490`); `layout.json` round-trips only hand-edits.

---

## 2. Every panel / zone — purpose, contents, controls, data, current state

State legend: **LIVE** = driven by real engine events; **REAL (local)** = a real control whose L1
verb is shipped and sent, but with no readback; **PLACEHOLDER** = built and visible but inert;
**DISABLED** = greyed / non-interactive stub.

### 2.0 Menu bar (chrome) — `render_menu_bar`, `main.cpp:236-296`

| Menu | Items | State | L1 verb |
|---|---|---|---|
| **File** | New set / Open set… / Save set | **DISABLED** (`MenuItem(…,false,false)`, `main.cpp:243-245`) | — (GUI-local, not built) |
| | Quit | **REAL** — sets `quit_requested` (`main.cpp:247-249`); closes the *window only*, never a socket `quit` (`main.cpp:227-235`) | — |
| **Edit** | Undo / Redo / Preferences… | **DISABLED** (`main.cpp:254-257`) | — |
| **View** | Intention (checkbox) | **LIVE toggle** — bound to `intention` zone `visible` (`main.cpp:262-264`) | host-only |
| | Parts (checkbox) | **LIVE toggle** — bound to `parts` zone `visible` (`main.cpp:265-267`) | host-only |
| | Layout density… | **DISABLED** (`main.cpp:268`) | — |
| **Transport** | Start / Stop / Continue / Panic | **REAL** — each sends its L1 verb (`main.cpp:272-285`) | `transport start\|stop\|continue`, `panic` |
| **Help** | About / Key bindings… / Contract–version | **DISABLED** (`main.cpp:288-292`) | — |

The View toggles find their zone via `find_zone` (`main.cpp:218-225`) and flip `Zone::visible`; a
hidden zone takes no space and its siblings redistribute (`compute_rows`, `layout_model.cpp:189-194`).
Ctrl+P is shown as the Start accelerator label only — it is **not** an actual global keybind
(`main.cpp:273`; no key handler exists; contrast `ux-workstation.md:179-183`).

### 2.1 Transport zone — `render_transport_panel`, `src/transport_panel.cpp`

- **Purpose:** connection status + transport control + position readout, all on one line
  (`ux-workstation.md:185-189`).
- **Contents / controls (left→right):**
  1. **Connection dot + label** — green `* Connected` / red `* Disconnected`
     (`transport_panel.cpp:8-12`). **LIVE**, from `AppState::connected()` set each frame from
     `BrainSession::status()` (`main.cpp:472-474`).
  2. **Play** button → `transport start` + optimistic `note_transport_sent(true)`
     (`transport_panel.cpp:15-18`). **REAL.**
  3. **Stop** button → `transport stop` + `note_transport_sent(false)` (`transport_panel.cpp:20-23`).
     **REAL.**
  4. **Panic** button → `panic` (`transport_panel.cpp:25-27`). **REAL.**
  5. **Transport-state + Section label** — `| <stopped|playing|paused> | Section: <name>`
     (`transport_panel.cpp:30-36`). **LIVE** (state from `kTransport`/`kBeat`; section from
     `kSection`).
  6. **Bar·beat playhead** — `| bar N . beat M .PP` when playing, else disabled `| bar -- . beat --`
     (`transport_panel.cpp:39-49`). **LIVE (P0-2, `kBeat`)**; `PP` is the 0–23 sub-beat pulse.
- **NOT present vs the §3 wireframe:** tempo (`♩=120`), meter (`4/4`), key (`Key Cm`), the
  clickable `Style: Funk ▸ [Var A]` chip, and the bar-progress bar `▓▓░░` are **not rendered** —
  the panel has no tempo/meter/key/style widgets at all (compare `ux-workstation.md:122`). Gap §7.

### 2.2 Browser zone — `render_browser_panel`, `src/browser_panel.cpp` + `src/browser_model.hpp`

- **Purpose:** a searchable tree of the three draggable material kinds
  (`ux-workstation.md:191-199`).
- **Contents:**
  1. **Search field** — `InputTextWithHint("##browser_search","search...")`, full-width, 64-byte
     buffer (`browser_panel.cpp:14-24`). Case-insensitive substring filter over style names
     (`browser_model.hpp:50-53`). **REAL (local).**
  2. **Styles branch** (default-open tree) — the **16 builtin styles** as a hand-copied literal list
     `basic,pop,rock,ballad,funk,disco,house,swing,bossa,samba,reggae,country,blues,shuffle,latin,motown`
     (`browser_model.hpp:24-27`). Each is an `ImGui::Selectable` **drag source**
     (payload `SONOTRON_STYLE_INDEX`, `browser_panel.cpp:35-46`). Click → `style load <name>`
     (`browser_panel.cpp:39-41`). **REAL / LIVE-capable** (a shipped L1 verb).
  3. **Clips branch** — `render_placeholder_branch("Clips", …)`; the model list is always empty →
     renders `(none authored yet)` (`browser_panel.cpp:51-63,70`; `browser_model.hpp:44-46`).
     **PLACEHOLDER** (no recorder/authoring UI, no clip primitive).
  4. **MIDI seqs branch** — same placeholder path, always empty (`browser_panel.cpp:71`). **PLACEHOLDER.**
- **Data:** styles are static; clips/midi-seqs are empty `std::vector<std::string>` fields
  (`browser_model.hpp:55-58`).

### 2.3 Repeat Zone / grid — the HERO — `render_grid_panel`, `src/grid_panel.cpp` + `src/grid_model.hpp`

- **Purpose:** the Live-Loops launch matrix — rows are parts, columns are scenes
  (`ux-workstation.md:201-204,235-262`).
- **Structure:** an `ImGui::BeginTable("grid")` with a fixed 70 px "Part" column plus one column per
  scene (`grid_panel.cpp:61-71`).
  - **Rows:** the 9 `TrackRole` parts `Drums, Perc, Bass, Chord1, Chord2, Pad, Arp, Phrase, Lead`
    (`src/track_roles.hpp:24-33`; `kCc` deliberately excluded, `track_roles.hpp:14-17`).
  - **Columns / scenes:** default 3, max 8 (`grid_model.hpp:46-47`). A `+ Scene` small-button adds
    a column preserving contents (`grid_panel.cpp:90-92`, `grid_model.hpp:60-63`). **REAL (local).**
- **Controls / widgets:**
  1. **Scene header + `>` launch button** — per column, labelled `SceneN` with a `>` SmallButton
    that is **wrapped in `BeginDisabled(!kGridLaunchWired)`** and greyed
    (`grid_panel.cpp:18-33`). Hover tooltip: *"Scene launch awaits the core clip primitive
    (ux-workstation.md S11.3)"*. **PLACEHOLDER (DISABLED).**
  2. **Cells** — a full-width `ImGui::Button` per (part × scene) showing `.` when empty or the
    content label (`grid_panel.cpp:14-16,35-44`). The button is *clickable but inert*; hover shows
    *"Launch awaits the core clip primitive (ux-workstation.md S11.3)"* (`grid_panel.cpp:41-43`).
    **PLACEHOLDER launch.**
  3. **Drop target** — each cell **accepts** a `SONOTRON_STYLE_INDEX` drag from the browser and
    calls `GridModel::set_cell(…, kStyleSection, styleName)` (`grid_panel.cpp:47-54`). **REAL** —
    cell *content* is real today; only *launch* is placeholder.
- **The launch gap flag:** `inline constexpr bool kGridLaunchWired = false;`
  (`grid_model.hpp:19-25`). Flipping it to true (once the core clip primitive + `launch/stop clip`
  / `launch scene` verbs ship) lights the affordance with no panel rewrite.
- **Cell data model:** `GridCell{GridCellKind kind, std::string label}` where kind ∈
  `{kEmpty, kStyleSection, kChordSequence, kStepTrack}` (`grid_model.hpp:27-36`); only `kEmpty` and
  `kStyleSection` are reachable from the UI today (drag sets style sections).

### 2.4 Intention zone — `render_intention_panel`, `src/intention_panel.cpp`

- **Purpose:** the demoted, **read-only** harmonic/intention rail
  (`ux-workstation.md:220-225`) — and the home of the live harmonic visualizer.
- **Contents (top→bottom):**
  1. **Live/at-rest header** — green `Intention (live)` when `harmony_active()`, else disabled
     `Intention (at rest)` (`intention_panel.cpp:8-13`). **LIVE gate.**
  2. **`follows <chord>`** — the followed chord *this bar*. **GREEN**, rendered only when the
     activity gate is open **and** the current chord is valid (`intention_panel.cpp:19-24`), else
     disabled `follows --`. **LIVE (P0-1, `kChordFollowed`).**
  3. **`next <chord>`** — the shift-staged pending chord. **AMBER**, independent of the gate (staging
     can happen at rest) (`intention_panel.cpp:25-30`), else disabled `next --`. **LIVE (P0-1).**
  4. **energy / tension / valence** — three disabled rows `[----------] --` and `valence o unknown`
     (`intention_panel.cpp:34-36`). **PLACEHOLDER**, pinned at zero because the Director (node 10000)
     does not exist.
- **Colour semantics** match `ux-workstation.md:385-388` exactly: bold green = followed this bar
  (lit only while active); amber = pending next bar; green over amber on a shared pitch class (the
  green/amber are separate rows here so no pixel conflict).

### 2.5 Parts / Mixer zone — `render_parts_panel`, `src/parts_panel.cpp` + `src/parts_model.hpp`

- **Purpose:** per-part mute/solo/volume/GM voice (`ux-workstation.md:211-218`).
- **Contents:** one row per the same 9 `TrackRole` parts (`parts_model.hpp:34`,
  `track_roles.hpp`). Each row (`parts_panel.cpp:10-33`):
  1. **`M` checkbox** → toggles local `muted` and sends `part <role> mute on|off`
     (`parts_panel.cpp:16-19`). **REAL (local, optimistic).**
  2. **`S` checkbox** → toggles local `soloed` and sends `part <role> solo on|off`
     (`parts_panel.cpp:21-25`). **REAL (local, optimistic).**
  3. **Part label** — e.g. `Bass` (`parts_panel.cpp:27`).
  4. **`gm --`** — disabled GM-program readout (`parts_panel.cpp:29-31`). **PLACEHOLDER** (no
     per-part program readback; `gm_program` stays −1, `parts_model.hpp:26-30`).
- **NOT present vs §3 wireframe:** the **volume bar** (`vol ▓▓▓`) and the numeric GM program are not
  rendered as live controls — mute/solo/label/`gm --` only (compare `ux-workstation.md:131-134`).
- **State honesty:** mute/solo are *optimistic local hints*; another client's change is not reflected
  because `Op::kGet`/snapshot is unwired (`parts_model.hpp:18-30`, gap §11.4). `kParamState` echoes
  exist on the wire but the GUI drops them (see §3.7). Gap §7.

### 2.6 Sequence Edit zone — `render_seqedit_panel`, `src/seqedit_panel.cpp` + `src/seqedit_model.hpp`

- **Purpose:** a piano-roll / step editor over a `Track` (`ux-workstation.md:206-209,265-280`).
- **Contents:**
  1. **Toolbar** (`seqedit_panel.cpp:12-50`):
     - **Part combo** — the 9 track roles, selectable (`seqedit_panel.cpp:14-26`,
       clamps in `seqedit_model.hpp:28-29`). **REAL (local).**
     - **Clip label** — static text, defaults `-` (`seqedit_panel.cpp:28-30`,
       `seqedit_model.hpp:32-33`). **PLACEHOLDER** (no clip source).
     - **`rec` checkbox** — record-arm, local bool (`seqedit_panel.cpp:32-36`). **REAL (local)** but
       drives nothing (no capture path).
     - **`grid 1/N`** — read-only text, default 1/16 (`seqedit_panel.cpp:38-39`,
       `seqedit_model.hpp:40-41`). Display only; no setter surfaced in the panel.
     - **piano-roll / step radio** — view-mode toggle, real local state (`seqedit_panel.cpp:41-49`).
       **REAL (local)** but both render the same placeholder.
  2. **Note canvas** — disabled `(note canvas awaits Track step editing)`
     (`seqedit_panel.cpp:57-59`). **PLACEHOLDER** — no `Track` step data is modeled or drawn; the
     `track step …` write path (`ux-workstation.md:267`) is a later slice
     (`seqedit_model.hpp:14-19`). **No playhead here either** (contrast `ux-workstation.md:136-141`).

---

## 3. Every live data binding (engine event → panel)

Events are decoded by `parse_brain_event` (`src/brain_event.cpp`, JSONL) for the UDS backend, or by
`brain_event_from_outevent` (`src/brain_event_from_outevent.cpp`) for the in-process backend — both
produce the same `BrainEvent` POD (`src/brain_event.hpp:81-140`), drained once per frame
(`main.cpp:463-471`), reduced by `AppState::apply` (`src/app_state.cpp:39-103`).

| Engine event | `BrainEvent::Kind` | AppState reduction | Panel surface & animation |
|---|---|---|---|
| `chord-followed` | `kChordFollowed` | sets `chord_followed_current/next(_valid)`, `_source`; opens the activity gate (`m_manual_steer=true`) if current valid — `app_state.cpp:72-83` | **Intention**: green `follows` + amber `next` rows (`intention_panel.cpp:19-30`). The single live **harmonic visualizer**. |
| `beat` | `kBeat` | sets `m_bar/m_beat/m_pulse`; **forces** `Transport::kPlaying` (`app_state.cpp:84-96`) | **Transport**: `bar N . beat M .PP` readout increments per pulse (`transport_panel.cpp:39-49`). The live **playhead** (numeric; no graphical scroll bar yet). |
| `transport` | `kTransport` | maps `playing/paused/stopped` → `m_transport`; on stop, clears steer + parks playhead to 0 (`app_state.cpp:57-71`) | **Transport**: state label `stopped/playing/paused` (`transport_panel.cpp:30-36`); parks the bar·beat readout. |
| `section` | `kSection` | `m_section = name` (`app_state.cpp:54-56`) | **Transport**: `Section: <name>` (`transport_panel.cpp:36`). |
| `chord` (recorded-sequencer path only) | `kChord` | `m_chord_in/out/deg` (`app_state.cpp:49-53`) | **Log line only** (`app_state.cpp:15-16`). **No panel renders `chord_in/out/deg`** — the accessors exist (`app_state.hpp:63-65`) but are unused by any zone. Gap. |
| `midi-out` | `kMidiOut` | none but the log (`app_state.cpp:97-101`) | **stdout log only** — there is **no MIDI-monitor zone** in the GUI (`main.cpp:466-471`; `ux-workstation.md` never spec'd one). |
| `warn` | `kWarn` | log only (`app_state.cpp:98`) | stdout log only. |
| `error` (per-client `{"error","cmd"}`) | `kError` | log only (`app_state.cpp:23-24`) | stdout log only. Emitted in-process for untranslated commands / `midi-source` failures (`in_process_brain_session.cpp:369-386,407-415`). |
| `param-state` echo | *(dropped)* | — | **NOT decoded**: `brain_event_from_outevent` maps `kParamState`→`kUnknown,invalid` (`brain_event_from_outevent.cpp:103-113`); the JSONL decoder doesn't model it either. So **no live mirror of groove/arp/parts/style** in the GUI. Gap §7. |
| `clip` state | *(dropped)* | — | not modeled — `parse_brain_event` yields `kUnknown` for it (`brain_event.hpp:142-145`). |

**Connection status** is not an event: each frame `main.cpp:472-474` reads `BrainSession::status()`
→ `AppState::set_connected` → the green/red dot (`transport_panel.cpp:8-12`).

**Activity gate** (`harmony_active()`, `app_state.hpp:71`): true when transport is playing OR a chord
was steered this run. It governs whether green reads as "live". Opened by an optimistic
`note_manual_steer()` hint, by a valid `kChordFollowed`, or by `kBeat`/`kTransport playing`; closed on
stop (`app_state.cpp:64,78-82,95`).

---

## 4. Every user flow (entry → steps → L1 verb → feedback → end state)

> Reality check: the GUI sends only **six** L1 command shapes today — `transport start|stop|continue`,
> `panic`, `style load <name>`, `part <role> mute|solo on|off` (all `.send()` call sites:
> `browser_panel.cpp:40`, `parts_panel.cpp:18,24`, `transport_panel.cpp:16,21,26`,
> `main.cpp:274,277,280,283`), plus `midi-source load <path>` which is **wired in the session but has
> no UI trigger** (`in_process_brain_session.cpp:322-362`). Flows the spec lists beyond these are not
> reachable from the GUI yet; each is marked below.

### F1 — Connect to the engine
- **Entry:** launch. **Default** (no `--control`): `InProcessBrainSession::start()` spins a dedicated
  engine thread running arrangrr `Shell` + `AlsaMidi` + tick clock inside this binary
  (`main.cpp:414-421`, `in_process_brain_session.cpp:209-312`), auto-wiring `port open in in0`,
  `port open out out0`, `thru in0 out0` (`in_process_brain_session.cpp:240-243`). **`--control <path>`
  or `SONOTRON_CONTROL_PATH`:** `UdsBrainSession::connect_to()` opens the AF_UNIX socket
  (`main.cpp:405-413`, `uds_brain_session.cpp:56-`).
- **Feedback:** Transport dot turns green when `status()==kConnected` (`transport_panel.cpp:8-9`);
  the in-process engine reaches `kConnected` after topology setup (`in_process_brain_session.cpp:246`).
- **End state:** connected, ready to command.

### F2 — Load a style (Browser)
- **Entry:** Browser ▸ Styles branch. **Steps:** click a style name (or drag it — drag targets a grid
  cell, F7). **Sends:** `style load <name>` (`browser_panel.cpp:39-41`). In-process this resolves the
  builtin index via `Shell::resolve_style_index` and enqueues a `Param::kStyleLoad` Command
  (`in_process_brain_session.cpp:142-151`). **Feedback:** none direct — no ack, no style chip
  (gap §11.4/§11.5); audible band change once transport plays. **End state:** engine style changed;
  GUI shows nothing until sound/section events arrive.

### F3 — Start / Stop / Continue / Panic transport
- **Entry:** Transport panel buttons (`transport_panel.cpp:15-27`) or Transport menu
  (`main.cpp:272-285`). **Sends:** `transport start` / `stop` / `continue` / `panic`. **Feedback:**
  optimistic label flip via `note_transport_sent` (`transport_panel.cpp:17,22`); then LIVE confirmation
  as `kTransport`/`kBeat` arrive — the state label and the bar·beat playhead animate
  (`app_state.cpp:57-96`). Stop parks the playhead to `bar -- . beat --`
  (`transport_panel.cpp:39-42`). **End state:** playing (playhead scrolling) or stopped (parked).

### F4 — Watch the harmonic visualizer follow chords
- **Entry:** any chord commit in the engine (manual steer, live detect, or sequencer bar-promote — the
  four `kChordFollowed` emit sites, `ux-workstation.md:430-445`). The **GUI itself has no chord-input
  widget** (by design, `ux-workstation.md:383-385`); the gesture enters the core via MIDI/keyboard.
- **Steps:** as the engine commits a chord, it emits `chord-followed {cur, next, src}`. **Reacts to:**
  `kChordFollowed`. **Feedback:** Intention shows **GREEN** `follows <cur>` (when active) and **AMBER**
  `next <staged>` (`intention_panel.cpp:19-30`); a valid current also flips the header to
  `Intention (live)`. **End state:** the followed harmony is legible; amber→green as staging promotes
  on the bar boundary. **In-process caveat:** the integrated engine has **no MIDI-in** wired (Phase 2b
  scope, `in_process_brain_session.cpp:24-26`), so manual/detected chords cannot be played *into* the
  default backend — this flow is fully exercisable only against an external engine over `--control`,
  or via the sequencer path.

### F5 — Watch the playhead scroll
- **Entry:** F3 Start. **Reacts to:** `kBeat` (24-PPQN pulses). **Feedback:** Transport reads
  `bar N . beat M .PP`, the `.PP` pulse making motion visible within a beat
  (`transport_panel.cpp:44-49`). **End state:** numeric playhead advancing. **Note:** there is no
  graphical bar-progress bar and **no seqedit playhead** (both spec'd, §7 gap).

### F6 — Mute / solo a part (Parts / Mixer)
- **Entry:** Parts rail `M`/`S` checkboxes. **Steps:** toggle. **Sends:**
  `part <role> mute|solo on|off` (`parts_panel.cpp:16-25`). **Feedback:** checkbox flips immediately
  (optimistic, `parts_model.hpp:40-41`); the band change is audible; **no readback** confirms it
  (`parts_model.hpp:18-30`). **End state:** part muted/soloed locally-and-on-engine, not mirrored from
  truth.

### F7 — Launch / stop a clip in the hero grid
- **Entry:** Repeat Zone cells / `>` scene buttons. **Steps:** *drag* a style from the browser into a
  cell → **REAL**: `GridModel::set_cell(kStyleSection, name)` records content
  (`grid_panel.cpp:47-54`). *Click* the cell or `>` to launch → **INERT**: guarded by
  `kGridLaunchWired==false`, greyed with a "awaits the core clip primitive" tooltip
  (`grid_panel.cpp:26-43`, `grid_model.hpp:19-25`). **Sends:** nothing on launch today.
  **Becomes real** when the clip primitive + `launch/stop clip <id> quantize <n>` /
  `launch scene <n> quantize <q>` verbs ship (`ux-workstation.md:308-311,405-412`,
  `pipeline-p0-mechanical-plan.md:85-89`). **End state today:** cell holds content; launch is a
  no-op placeholder.

### F8 — Load a MIDI source for Accompany (melody in → band under detected chords)
- **Spec:** `ux-workstation.md` A6/§9310, `flows.md`. **Status:** the **transport/session layer is
  plumbed but there is no UI**: `InProcessBrainSession::send("midi-source load <path>")` routes a path
  over a dedicated ring to `Shell::load_midi_source` (`in_process_brain_session.cpp:322-362,268-`), and
  failures surface as `kError` (`in_process_brain_session.cpp:407-415`). **But no panel, menu item, or
  file picker calls it** (grep: only the session references `midi-source`). **Not reachable by the
  user today.** Gap §7.

### F9 — Restyle an imported melody into a genre idiom
- **Spec:** `restyle <style>` L1 verb exists in the shell (`components/hostrt/shell*`, verb list §6).
  **Status:** **not sent by any GUI control**, and the in-process translator does not translate it
  (`command_line_to_command` covers only the six shapes, `in_process_brain_session.cpp:120-170`) — an
  untranslated line yields an `kError` note *"integrated mode does not translate this command … yet"*
  (`in_process_brain_session.cpp:369-376`). **Not reachable.** Gap §7.

### F10 — Export the session to `.mid`
- **Spec:** `export-smf` L1 verb exists in the shell. **Status:** **no GUI affordance** (File ▸ Save
  is disabled, `main.cpp:245`), **not translated** in-process. **Not reachable.** Gap §7.

### F11 — Read the MIDI monitor
- **Spec:** the TUI has a rich MIDI monitor (`components/hostrt/midi_monitor.*`). **Status:** the GUI
  has **no MIDI-monitor zone**. `midi-out` events are printed to **stdout only**
  (`main.cpp:466-471`; the comment there notes no log zone exists yet). **Not reachable in-GUI.** Gap §7.

### F12 — Adjust groove / adjust arp
- **Spec:** `groove <field> <v>`, `arp <field> <v>` (`ux-workstation.md:299,316`; verbs exist in the
  shell, and the TUI has `groove_view.*`/`arp_view.*`). **Status:** the GUI has **no groove or arp
  panel at all** and sends neither verb (no `.send("groove …")`/`"arp …"` anywhere). **Not reachable.**
  Gap §7 (the wireframe's implied groove/swing controls are unbuilt).

---

## 5. States & edge cases per panel

- **Transport:** *disconnected* → red dot, buttons still clickable but sends are dropped/queued;
  *connected/stopped* → green dot, `stopped`, `bar -- . beat --`; *playing* → `playing`, scrolling
  bar·beat; *paused* → `paused`, playhead frozen at last position (only `stopped` parks it to 0,
  `app_state.cpp:60-70`). Mid-session connect while already playing: a stray `kBeat` forces the label
  to `playing` even without a `transport playing` event (`app_state.cpp:84-96`).
- **Browser:** *empty search* → all 16 styles; *filtered* → case-insensitive substring subset
  (`browser_model.hpp:50-53`); Clips/MIDI-seqs branches always show `(none authored yet)`.
- **Grid:** *at rest* → all cells `.`; *content dropped* → cell shows the style name; *launch* always
  disabled/greyed (`kGridLaunchWired==false`); *scene count* clamped 3–8 (`+ Scene` hidden at 8,
  `grid_panel.cpp:90`).
- **Intention:** *at rest / no chord* → disabled `follows --`, `next --`, header `Intention (at rest)`;
  *pending staged at rest* → amber `next` shows even while grey elsewhere (`intention_panel.cpp:25`);
  *active + current* → green `follows`; energy/tension/valence always disabled zeros.
- **Parts:** *default* → all M/S off, all `gm --`; another client's mute is **not** reflected (no
  readback). Solo does not visibly dim other rows (no solo-implied-mute UI).
- **Sequence Edit:** always shows the placeholder canvas regardless of part/clip/view/rec toggles;
  view radio and rec checkbox change local state only.
- **Whole screen (no style loaded):** nothing blocks — every zone renders at rest; there is no
  "load a style first" gate (contrast `ux-workstation.md`'s no-style states are not modeled).
- **View toggles:** hiding Intention and/or Parts removes them and the right rail redistributes
  (`compute_rows`); if both hidden, col 2 vanishes and browser/grid widen.
- **Stale/hand-edited `layout.json`:** an unknown zone `id` falls through to a titled empty frame
  (`layout_renderer.cpp:20-23,37`); a bad `font_size` falls back to 13 (`main.cpp:188-193`).

---

## 6. Interaction model

- **Mouse-first.** Buttons, checkboxes, tree nodes, combos, radio buttons, drag-and-drop (browser
  style → grid cell). No custom canvas interactions (grid launch and seqedit canvas are inert).
- **Keyboard:** only ImGui defaults (text entry in the search field, combo navigation). The
  advertised **Ctrl+P** is a *label only*, not a bound global shortcut (`main.cpp:273`); the TUI's
  backtick chooser is **not** ported. No focus/tab-order model beyond ImGui's implicit widget order.
- **Modes:** none — a single screen, everything visible at once (`ux-workstation.md:33`,
  the GarageBand-immediacy posture). The only "mode-like" toggles are View ▸ Intention/Parts
  visibility and the seqedit piano-roll/step radio.
- **`PanelManager`:** the class named in the brief lives in the **TUI** (`components/hostrt/
  panel_manager.*`), **not** in `gui-sonotron`. The GUI has no PanelManager and no tab-order/focus
  seam — layout is the JSON zone grid + ImGui's own focus handling. (Relevant to the brief's
  focus/tab-order question: there is nothing to design against yet on the GUI side.)
- **Window sizing:** proportional reflow via normalized weights (§1.4); no user-resizable splitters,
  no docking (ImGui docking not enabled; the shell is a fixed 6-zone grid).
- **Quit safety:** File ▸ Quit and window-close close the GUI only; `quit`/`exit` are blacklisted at
  the session boundary (`uds_brain_session.cpp:39-50`, `in_process_brain_session.cpp:323-325`).

---

## 7. Gap list — spec (ux-workstation.md) vs built reality

Placeholder/disabled-but-built (honest stubs, designed to light up):
- **Grid launch** — cells + `>` scene buttons inert until the clip primitive; `kGridLaunchWired=false`
  (`grid_model.hpp:19-25`). Spec §5/§11.3.
- **Sequence Edit note canvas** — no `Track` step grid, no note rendering, no `track step …` writes;
  placeholder text only (`seqedit_panel.cpp:57-59`). Spec §6.
- **Intention energy/tension/valence** — pinned disabled zeros (no Director, node 10000)
  (`intention_panel.cpp:34-36`). Spec §4.7.
- **Parts GM program / volume** — `gm --` disabled, no volume bar, no readback
  (`parts_panel.cpp:29-31`). Spec §4.6 (`vol ▓▓▓ gm`).
- **Browser Clips / MIDI seqs** — always `(none authored yet)` (no authoring/recorder, no clip
  primitive) (`browser_panel.cpp:51-63`). Spec §4.3.

Transport bar missing widgets (spec §3/§4.2, wireframe line `ux-workstation.md:122`):
- **No tempo control** (`♩=120` / `bpm <n>`), **no meter** (`4/4`), **no key** (`Key Cm` /
  `key <root> <mode>`), **no clickable Style ▸ Section chip**, **no graphical bar-progress bar**.
  The panel renders only connect-dot, Play/Stop/Panic, state+section text, and the numeric bar·beat
  readout (`transport_panel.cpp`).

Whole zones/flows the spec describes that are absent from the GUI:
- **Groove panel & Arp panel** — no zone, no `groove …`/`arp …` sends (spec §4.6 defaults, B9). Exists
  only in the TUI (`groove_view.*`, `arp_view.*`).
- **Style/section chooser** — no GUI chooser (TUI `style_chooser.*` only); style is loaded by
  click/drag from the browser, sections are not selectable (`style section <type>`, A4/B7, unsent).
- **MIDI monitor** — no GUI zone; `midi-out` → stdout only (F11). TUI `midi_monitor.*` only.
- **Accompany (`midi-source load`)** — session-plumbed, **no UI trigger** (F8).
- **Restyle (`restyle <style>`)** — no UI, not translated in-process (F9).
- **Export SMF (`export-smf`)** — no UI, File ▸ Save disabled (F10).
- **Menu placeholders** — File (New/Open/Save), Edit (Undo/Redo/Preferences), View ▸ Layout density,
  all of Help — disabled stubs (`main.cpp:243-292`). Preferences §12 (font/socket-path/quant/theme)
  is unbuilt.
- **`chord` event display** — decoded into `AppState` but no panel renders `chord_in/out/deg`
  (`app_state.hpp:63-65` unused). Only the stdout log shows it.
- **`param-state` readback** — engine emits it; GUI drops it
  (`brain_event_from_outevent.cpp:103-113`), so no live mirror of mutes/style/groove/arp (gap §11.4).
- **Positive ack / snapshot-on-connect** — none (`brain_session.hpp:19-35`, spec §11.4/§11.5); the
  GUI's view is reduced purely from broadcast events, so a mid-session connect starts "unknown".
- **Keybindings** — Ctrl+P etc. are labels only, not bound (§6); backtick chooser not ported.
- **In-process MIDI-in** — the default integrated engine has no MIDI input wired
  (`in_process_brain_session.cpp:24-26`), so live manual chord-steering (F4) needs the external
  `--control` engine.

Already delivered vs spec (for contrast): the two P0 live surfaces are **real** — the harmonic
visualizer green/amber (`kChordFollowed`, `intention_panel.cpp`) and the playhead
(`kBeat`, `transport_panel.cpp`), matching `pipeline-p0-mechanical-plan.md` "done". The 6-zone
workstation layout, nested right-rail split, drag-drop of styles into grid cells, mute/solo, style
load, and the View toggles are all built and working.

---

## 8. Backend note (post-dates the older docs)

The brief's docs describe `UdsBrainSession` as the first transport. The binary has since gained a
**default in-process backend** (Phase 2b, `sonotron-server-phase2-brief.md`,
`main.cpp:8-29,395-421`): with no `--control`, an `InProcessBrainSession` runs the real arrangrr
engine on a dedicated thread inside the GUI process, exchanging `Command`/`OutEvent` over SPSC rings
(`src/spsc_ring.hpp`, `in_process_brain_session.cpp:180-201`) and bridging `OutEvent`→`BrainEvent`
in-process (`brain_event_from_outevent.cpp`). Panels are unaffected — they only ever see the abstract
`BrainSession` (`brain_session.hpp`), exactly the swap the spec's §9 abstraction was built to allow.
Both backends surface the same 7 event kinds and accept the same six command shapes; the in-process
one additionally routes `midi-source load <path>` over a path ring but nothing calls it.

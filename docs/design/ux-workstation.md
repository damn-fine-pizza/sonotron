# UX — the sonotron workstation screen

Status: **authoritative for the GUI screen (node 11600), decided 2026-07-10.** This document
governs the *center of the screen* and the two working flows. It **supersedes the
conductor-dashboard-as-the-whole-screen framing** of `ux-concept.md` (whose three laws and harmony
colour semantics still hold — see §10). It stays **bound** by:

- `gui-contract-map.md` — the shipped GUI↔brain wire (text L1 in / JSONL out). Non-negotiable.
- `components/arrangrr/include/arrangrr/abi.hpp` — the frozen v1 command/event vocabulary.
- The naming/architecture locks in `product-identity.md` (sonotron = host workstation, arrangrr =
  core brain, melodd = audio peer).

> The protocol in `DESIGN.md §24` was **never shipped**. Build to `gui-contract-map.md`, not §24.

---

## 1. Status & supersession

The first GUI spike centred the whole screen on an abstract "conductor dashboard": an
energy/tension/valence **podium** and an arrangement **streamgraph**. It was a clean, tested shell
(window, HiDPI text, JSON-driven zone layout, model/panel split) filled with **mock data, no brain
connection, and three of five zones empty**. It could not be *used*: no browser, no clip grid, no
sequence editor, no menu bar.

This restart keeps the mechanical shell and throws away the concept content. The screen is
re-centred on a concrete, immediate workstation. The intention/conductor idea is **demoted to an
optional side panel/mode** (§4.7), not deleted — it returns when the Director (node 10000) is real.

---

## 2. Product posture

- **Immediacy of GarageBand for iPad, with better flows.** One screen, no modes to hunt through,
  you are making sound within seconds. The launch grid is the hero.
- **Depth of clip management closer to Bitwig.** The grid is a real clip/scene matrix with a
  piano-roll/step editor one gesture away — not a toy.
- **MIDI-only symbolic.** No linear audio timeline (that gravity is what the whole design refuses).
  Audio realisation is a separate peer engine (melodd), out of scope here.
- **Two operations the screen must serve, first-class and equal:**
  1. **Track preparation** — build the set: pick a style/key/tempo, author clips, assign voices,
     route, set groove — before you perform.
  2. **Performance** — play the prepared set live: launch scenes/cells, steer harmony by hand,
     trigger fills, mute/solo, tweak feel.

---

## 3. The screen at a glance

Principle: **one screen, the repeat grid is the hero.** The menu bar is chrome; everything below it
is the JSON-driven zone grid the existing layout engine already renders.

```
┌ File  Edit  View  Transport  Help ─────────────────────────────── menu bar (chrome) ┐
├──────────────────────────────────────────────────────────────────────────────────────┤
│ ▶ ■  ♩=120 4/4  Key Cm  Style: Funk ▸ [Var A]  ● Connected     bar 3·beat 2  ▓▓░░    │ transport
├───────────────┬──────────────────────────────────────────────┬─────────────────────────┤
│ BROWSER       │ REPEAT ZONE  (Live-Loops launch grid)        │ INTENTION (optional)    │
│ ▾ Styles      │        Scene1  Scene2  Scene3  +             │  energy  ▓▓▓▓▓░░░       │
│   Funk        │ Drums  [▶A ]  [▶B ]  [ · ]                   │  tension ▓▓░░░░░░       │
│   House       │ Bass   [▶wlk] [ · ]  [ · ]                   │  valence ◑ neutral      │
│ ▾ Clips       │ Chord1 [▶cmp] [▶stab][ · ]                   │  (View-toggle; §4.7)    │
│   comp-8ths   │ Pad    [ · ]  [▶swl] [ · ]                   │─────────────────────────│
│   stab-Am     │ Arp    [ · ]  [ · ]  [▶up]                   │ PARTS / MIXER           │
│ ▾ MIDI seqs   │ Lead   [ · ]  [ · ]  [ · ]                   │ Drums  M S  vol▓▓▓ gm   │
│   verse-prog  │  (drag from browser into any cell)           │ Bass   M S  vol▓▓░ 33   │
│   chorus-prog │  ▸ launch quantize [1 bar ▾]   Scene ▶ all   │ Chord1 M S  vol▓▓▓ 4    │
│ [search…]     │                                              │ Pad    M S  vol▓░░ 89   │
├───────────────┴──────────────────────────────────────────────┴─────────────────────────┤
│ SEQUENCE EDIT  Part: Bass ▾  Clip: wlk ▾  ●rec ▾grid 1/16  [piano-roll|step]  [▏playhead]│
│  C ───────────────────────────────────────────────                                       │
│  A ──────████──────────████──────────                     ← your notes are solid          │
│  G ──██──────────██────────────██────                                                     │
│  now │  bars 1 · 2 · 3 · 4                                       [playhead ▏]             │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

Small readable font: keep the vendored JetBrains Mono NL + GLFW content-scale pipeline unchanged;
set the layout default `font_size_px` to **13** (via the existing `font_size` JSON knob — no code
change, just a smaller default).

---

## 4. Zones

Each zone is a `Zone{id,title,row,col,weight}` in `default_layout()` (layout_model.cpp) and is
dispatched by `id` in `render_zone_content()` (layout_renderer.cpp). The menu bar is **not** a zone.

Default layout:

| Zone `id` | title | row / col | notes |
|---|---|---|---|
| — | menu bar | chrome | `ImGui::BeginMainMenuBar()` in main.cpp, outside the grid |
| `transport` | Transport | 0, full-span | live (was an empty frame) |
| `browser` | Browser | 1, col 0 | new |
| `grid` | Repeat Zone | 1, col 1 | **hero** (replaces `arrangement`) |
| `intention` | Intention | 1, col 2 (top) | **rewritten, demoted, optional** (replaces old podium) |
| `parts` | Parts / Mixer | 1, col 2 (bottom) | new — stacked under Intention in the right rail |
| `seqedit` | Sequence Edit | row 2, full-span | new |

The right rail (`intention` over `parts` in col 2) is a **vertical stack inside one column** — this
needs a nested vertical split in the layout engine, which the restart adds now (§4.6, §13).

### 4.1 Menu bar (chrome)
- **File** — new/open/save set (GUI-local for v1, §5/§11), quit *the GUI* (never a bare `quit` on
  the socket — that kills the brain for every client; §10).
- **Edit** — undo/redo (GUI-side authoring), preferences (§12).
- **View** — toggle Intention rail, toggle Parts, choose layout density.
- **Transport** — start/stop/continue/panic mirrors of the transport bar.
- **Help** — about, key bindings, contract/version.

### 4.2 Transport
Play/stop/continue, tempo, meter, key, current style ▸ section, brain-connection status, and a
bar·beat readout with a bar-progress bar. Sends `transport start|stop|continue`, `bpm <20..400>`,
`key <root> <mode>`, `panic`. The bar·beat readout is driven by the beat/position event that the
restart builds up front (§11, P0-2) — a true playhead, not an inferred one.

### 4.3 Browser
A searchable tree of the three draggable material kinds:
- **Styles** — the 16 builtins (basic, pop, rock, ballad, funk, disco, house, swing, bossa, samba,
  reggae, country, blues, shuffle, latin, motown). Drag a style → sets the style; drag a *section*
  of a style → a grid cell.
- **Clips** — chord-sequence and step-track material the user has authored/recorded.
- **MIDI seqs** — recorded chord progressions (`ChordSequence`, degrees stored relative to key so
  they transpose/re-harmonise, D28).

Each entry is an ImGui drag source; the drop target is a grid cell (§4.4).

### 4.4 Repeat Zone — the hero
A **Live-Loops launch grid**: rows are parts (`TrackRole`: drums/perc/bass/chord1/chord2/pad/arp/
phrase/lead), columns are **scenes**. A cell holds a launchable clip; launching a whole column
fires a scene. Detailed in §5.

### 4.5 Sequence Edit
A piano-roll / step editor over a `Track` (per-step note/vel/gate/tie/probability/ratchet/micro —
`timeline/timeline.hpp`). Your notes are solid; the copilot's proposals (dashed) are deferred (§6).
Detailed in §6.

### 4.6 Parts / Mixer
Per-part mute/solo/volume/GM-voice, shaped like `Arranger::PartInfo {routed, port, channel,
gm_program, muted, soloed, present}`. Sends `part mute <role> <0|1>`, `part solo <role> <0|1>`,
`program <gm> <port:ch>`, `style route <role> <port:ch>`.
**Right-rail stack (decided):** Parts sits **under Intention in the right column** (col 2), matching
the wireframe literally. The salvaged layout engine renders single-level rows (`compute_rows`), so
this requires a **nested vertical split within a column** — the restart adds that capability to the
engine now rather than flattening the rail into full-width rows (§13, decision §14.3).

### 4.7 Intention (optional, demoted)
A minimal, **read-only** rail: energy/tension/valence as coarse bars, plus current groove/style
implications. Not the old podium, not the center, not authoritative — the Director that would drive
it (node 10000) is not built. Toggled by **View ▸ Intention**. Mechanism: an additive `bool visible`
field on `Zone` (default true, backward-compatible); `render_layout` skips `!visible` zones.

---

## 5. The Repeat Zone (Live-Loops grid) in depth

**Cell model.** A cell is `{part_role, scene_index, content}` where content is one of:
- a **style section** (varA..D / fillA..D / intro / break / ending),
- a **chord sequence** (`seq` — the one producer that emits a `chord` event today),
- a **step track** (`track` — a hand-authored pattern for that part).

**Drag from browser** drops material into a cell; the cell records which primitive it launches.

**Scene columns.** Launching a column ("Scene ▶") fans out its cells' launch commands. Launching a
single cell launches just that clip; an empty cell in a launched scene mutes its part.

**Launch quantize.** A per-grid setting (1 bar / 2 bars / instant), honoured by the **core**: launch
timing is a musical decision that belongs where the clock lives, not smeared across the socket in the
GUI.

**Built on a real core clip primitive (decided — do it at the root, do not fake it).** Today the core
has *no* first-class clip/scene object: the standalone Looper (node 6000) and recall/song-mode (node
8000) are unbuilt. Rather than ship a throwaway GUI-side launcher that fakes clips by scripting
`style section`/`seq*`/`track*`, the restart **brings the real clip/scene primitive forward** as
front-of-line additive core work (§11): a first-class clip object, `launch/stop clip <id> quantize
<n>` verbs, and a `clip` state event. The Repeat Zone binds to *that* — each cell is a real clip the
core owns and launches on the quantize boundary.

The existing primitives it generalises (so the primitive is designed to subsume them, not ignore
them): a clip's content is a style section, a recorded chord sequence, or a step track — the three
material kinds the browser already offers. `grid_model` holds the matrix and scene structure; the
launch/stop/quantize semantics live in the core behind the new `clip` verbs.

---

## 6. The Sequence Edit surface

A piano-roll (and a step view for drum-style parts) over a single `Track`. Editing a note writes
`track step <i> <note> <vel> <gate> …` (the param-locked step form in `abi.hpp`). Record arms the
part and captures incoming notes. The **pull-only copilot** (Ask ▸ Propose/Adjust/Add, dashed ghost
notes) from `ux-concept.md` is **deferred** — v1 is a plain, honest editor: you always hold the pen.

---

## 7. Flow — Track preparation

Each step: the L1 command(s) sent, the JSONL event(s) reacted to, and any gap.

| # | Action | Sends (L1) | Reacts to (JSONL) | Gap |
|---|---|---|---|---|
| A1 | Pick a style | `style load <0..15>` | infer from later `midi-out` | no ack/snapshot (§11.4–5) |
| A2 | Set key / BPM | `key <root> <mode>`, `bpm <n>` | — | no ack (§11.5) |
| A3 | Audition the band | `transport start` … `transport stop` | `transport` (unreliable on start), `midi-out` | transport-on-start (§11-2) |
| A4 | Drop a style section into a cell | `style section <type>` | `section` | — |
| A5 | Drop a chord sequence into a cell | `seq new`/`seq use <i>`/`seq add …`, `seq play` | `chord` (this path only) | — |
| A6 | Drop a step track into a cell | `track new <role> <port:ch>`, `track length`, `track step …`, wrapped as a `clip` | `clip`, `midi-out` | clip primitive **built** (§11) |
| A7 | Edit notes in Sequence Edit | `track step …` | `midi-out` | — |
| A8 | Assign a GM voice | `program <gm> <port:ch>` | — | ack/snapshot (§11.4–5) |
| A9 | Route a part | `style route <role> <port:ch>` / `arp out` / `chord out` | — | — |
| A10 | Set groove/arp defaults | `groove <field> <v>`, `arp <field> <v>` | — | — |
| A11 | Name the set (Scene/Song) | *no L1 command* | — | no persistence (§11.6) |

---

## 8. Flow — Performance

| # | Action | Sends (L1) | Reacts to (JSONL) | Gap |
|---|---|---|---|---|
| B1 | Press Play | `transport start` | `transport` + `beat` heartbeat drives the playhead | P0-2 **built** (§11) |
| B2 | Launch a scene column | `launch scene <n> quantize <q>` (core fans out on the boundary) | `clip`, `section`, `chord`, `midi-out` | clip primitive **built** (§11) |
| B3 | Launch/stop a cell | `launch clip <id> quantize <q>` / `stop clip <id>` | `clip`, `midi-out`, `section` | clip primitive **built** (§11) |
| B4 | Steer harmony by hand | `chord play <note>` (SHIFT variant stages to next bar) | `chord-followed {current, …}` | P0-1 **built** (§11) |
| B5 | Queue next chord (amber→green) | `chord play …` staged | `chord-followed {current, pending, valid, source}` | P0-1 **built** (§11) |
| B6 | Live piano→chord detection | `chord detect on <port>`, `chord follow live`, `input zone <port> harmony` | `chord-followed` stream | P0-1 **built** (§11) |
| B7 | Trigger a fill | `style section fillA` | `section` | — |
| B8 | Mute/solo parts live | `part mute <role> <0|1>`, `part solo <role> <0|1>` | `midi-out` reflects it | no readback (§11.4) |
| B9 | Tweak feel live | `groove swing 55`, `arp octaves 2` | — | — |
| B10 | Panic / stop | `panic`, `transport stop` — **never bare `quit`** | `transport` | — |
| B11 | Open Intention rail | (host-side only; Director not on the core) | — | Director unbuilt (§15) |

The harmony visualiser (B4–B6) and the playhead (B1) are **not shipped degraded**: the two P0 events
that light them (`chord-followed`, `beat`) are built up front as the first additive core work (§11),
so the live surface is real from day one rather than inferred from the `midi-out` stream.

---

## 9. The brain interface — `BrainSession`

A small GUI-side interface hiding **whether the brain is in-process (static link to `hostrt`) or
across the UDS socket**. Per the architectural-quality requirement, the physical transport is a
swappable detail behind an abstract command/event/snapshot boundary — not a decision baked into the
panels.

```cpp
namespace sonotron {

struct BrainEvent {                    // one decoded inbound event (JSONL parsed here)
  enum class Kind { MidiOut, Chord, Section, Transport, Warn,
                    ChordFollowed /*additive, gap P0-1*/, Beat /*additive, gap P0-2*/ };
  Kind kind;
  // small POD payload: port, tick, name/code, note strings … panels never see raw text
};

struct BrainSnapshot { /* style/section/key/bpm, per-part mute/solo/program, groove/arp,
                          follow mode — every field optional/"unknown" until gap §11-3 lands */ };

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

- **Abstraction boundary = text-command in / decoded-event out** — mirrors the *shipped* contract,
  not §24. Command ack/correlation (§11.5) is not modelled yet; addable without breaking callers.
- **First concrete impl — `UdsBrainSession`** (build this now): `AF_UNIX`/`SOCK_STREAM`, newline
  framing, `kMaxLineLength = 4096`, connects to the path passed as `arrangrr --control /path.sock`.
  Non-blocking drain in `poll()`; a slow GUI drops events rather than stalling (matches best-effort
  broadcast). Template: `components/hostrt/tests/test_host.cpp::test_uds_server_end_to_end()`.
  `send()` blacklists `quit`/`exit`.
- **Second impl (future) — `InProcessBrainSession`**: links `hostrt`, calls `Shell::exec_line`
  directly for `send()`, subscribes to the in-process event broadcast for `poll()`. Because the
  interface is text-command / decoded-event, **panels do not change** when the transport is swapped.
- The JSONL decoder lives GUI-side, scoped to the 5 shipped shapes plus the additive ones — same
  "small local parser for a trusted schema" precedent as `layout_json` (which deliberately does not
  reuse `hostrt/jsonl`).

---

## 10. What the GUI mirrors vs does not duplicate

Carried forward from `gui-contract-map.md §4` and `ux-concept.md`:
- The GUI is **a mirror that reflects and a keyboard that commands — never a truth and never a note
  in the timing path.** The live musical gesture (the chord the band follows) enters the core via
  MIDI/computer-keyboard, not through the GUI.
- **GUI = 2-D structure, inspection, and a big readable harmony surface.** Live keyboard/MIDI
  steering stays first-class but is **not re-invented** here (the TUI already does it well).
- **No central clickable chord-pad.** Put the harmony *readout* at the centre, never a chord-input
  widget.
- **Colour semantics, preserved exactly:** bold **GREEN** = chord followed *this bar* (lit only when
  transport plays or a chord is explicitly steered), **AMBER** = pending chord staged for next bar,
  off otherwise; green wins over amber on a shared pitch class.
- **The three laws hold:** observable / overridable-and-you-win / hand-operable. The machine's help
  is optional and on top, never a gate.

---

## 11. Required additive brain work

The restart's key decision (2026-07-10): **build the foundation at the root, not the GUI shortcut.**
The Repeat Zone, the harmony visualiser, and the playhead all depend on core work the GUI must NOT
fake. So Fase 2 is **not GUI-only** — it opens with an additive-core batch in `arrangrr`/`hostrt`.
All additive to the frozen v1 ABI: none touches an existing id, none breaks the freeze.

**Front-of-line batch (built before/with the GUI zones that depend on them):**
1. **[P0] Followed-chord event.** `chord play` and live detection change harmony *silently*; the
   only `chord` event today is the recorded-sequencer path. Add `kChordFollowed {current, pending,
   valid, source}`, fired on every commit (manual/detect/sequencer/bar-promote). Lights B4–B6.
2. **[P0] Beat/position heartbeat.** `Transport::position()` (bar/beat/tick) is in-process only.
   Add `kBeat`/`kPosition` (and make `kTransport` fire on start/stop/continue). Lights the playhead
   and bar-progress (B1, transport readout).
3. **Clip/scene primitive + launch-quantize (nodes 6000/8000, brought forward).** A first-class clip
   object, `launch/stop clip <id> quantize <n>` and `launch scene <n> quantize <q>` verbs, and a
   `clip` state event, with launch-quantize honoured by the core clock. The Repeat Zone binds to
   this real primitive — we **do not** ship the throwaway GUI-side launcher. This is the concrete
   meaning of "aspettiamo il clip vero nel core, ma anticipiamolo".

**Follow-on (after the front batch, do not block the first GUI slice's *shape*):**
4. **State-on-connect snapshot.** All Params are write-only, `Op::kGet` is unwired; a GUI attaching
   mid-session cannot learn current style/key/mutes/groove/follow-mode. Add a snapshot-on-connect or
   per-domain readback. Makes reconnection honest.
5. **Positive ack / request correlation.** No success ack today; `Command` has no correlation field.
   Add an ack event. Polish.
6. **Persistence/recall (node 8000).** Saving the grid/Scene/Song; folds into the clip/scene work
   above. Until it lands, the GUI stores its grid in its own config.

Order: **1, 2, 3 up front** (the live surface and the hero grid are real, not degraded), then 4/5/6.

---

## 12. Preferences

Reachable via **Edit ▸ Preferences**. v1 contents:
- **Font size** (the existing `font_size` knob, live).
- **Control socket path** (what `UdsBrainSession` connects to; matches `--control` on the brain).
- **Default port/channel** for new parts/routes.
- **Default launch quantize** (1 bar / 2 bars / instant).
- **Theme** (dark default; light later).

---

## 13. Salvage / rebuild map (the code milestone, gated on this doc)

Fase 2 runs in two strands: a **core strand** (the §11 front batch — the additive-ABI work) and a
**GUI strand** (below). The GUI zones that depend on the core strand (grid, harmony visualiser,
playhead) land as their core piece lands. Convention preserved: only `*_panel.cpp` and
`layout_renderer.cpp` include ImGui; models are pure data.

**Core strand (`components/arrangrr`, `components/hostrt`):** the §11 front batch — `kChordFollowed`
event, `kBeat`/`kPosition` heartbeat, and the clip/scene primitive with `launch/stop clip`,
`launch scene`, quantize, and a `clip` event — plus their `to_jsonl()` shapes and L1 verbs in the
shell. Additive-only; `test_abi_frozen` must stay green.

**GUI strand — KEEP (light edit):** `main.cpp` (add `BeginMainMenuBar`; instantiate + pump a
`BrainSession`; reduce events into an `AppState`; wire View toggles), `layout_model.{hpp,cpp}` (new
`default_layout()` zones; add `bool visible`; **add a nested vertical-split capability so a column
can stack zones** — `compute_rows` grows a sub-column concept; font default 13), `layout_json.{hpp,
cpp}` (read/write `"visible"` and the nested-split shape), `layout_renderer.{hpp,cpp}` (drop the
`arrangement`/`intention` mock cases; add `transport`/`browser`/`grid`/`seqedit`/`parts`/`intention`
cases; render the nested split; skip `!visible` zones), `screenshot.*`, `assets/fonts/*`, the three
layout tests (update fixtures + add nested-split cases), the CMake source lists.

**GUI strand — DELETE (the abandoned concept):** `intention.{hpp,cpp}`, `intention_panel.{hpp,cpp}`,
`arrangement.{hpp,cpp}`, `arrangement_panel.{hpp,cpp}`, `test_intention.cpp`, `test_arrangement.cpp`,
and the `mock_*` generators.

**GUI strand — ADD:** `brain_session.hpp`, `uds_brain_session.{hpp,cpp}`, `brain_event.hpp` (POD +
JSONL decoder incl. the new `chord-followed`/`beat`/`clip` shapes), `app_state.{hpp,cpp}` (event
reduction); and per-zone model+panel pairs: `transport_panel.*`, `browser_panel.*`+`browser_model.*`,
`grid_panel.*`+`grid_model.*` (binds to the core clip primitive), `seqedit_panel.*`+`seqedit_model.*`,
`parts_panel.*`+`parts_model.*`, `intention_panel.*` (new, minimal, read-only). New tests:
`test_brain_event.cpp`, `test_grid_model.cpp`, `test_app_state.cpp`, `test_layout_nested_split.cpp`.

---

## 14. Decisions (resolved 2026-07-10, in review of this doc)

1. **Clip primitive.** **Wait for the real clip in the core, but bring it forward** — build the
   first-class clip/scene primitive (§11.3) as front-of-line core work; do **not** ship the throwaway
   GUI-side launcher. The Repeat Zone binds to the real primitive.
2. **Two P0 wire gaps.** **Build them, do not degrade.** `kChordFollowed` and `kBeat`/position are
   front-of-line core work; the harmony visualiser and playhead are real from day one, not inferred.
3. **Layout engine.** **Invest in the nested vertical split now** — the right rail stacks Intention
   over Parts in one column (matches the wireframe); no full-width-rows fallback.
4. **First transport impl.** **`UdsBrainSession` first** (matches the shipped contract, keeps process
   separation); `InProcessBrainSession` proves the swap later.
5. **No ack / no snapshot.** Follow-on additive work (§11.4–5); event-reduction covers the interim.

---

## 15. Deferred / out of scope for v1

- The Director and the intention rail beyond a read-only view (node 10000).
- Audio realisation / colour (melodd).
- The pull-only sequencer copilot (propose/adjust/add).
- Song/Scene persistence and recall (node 8000).

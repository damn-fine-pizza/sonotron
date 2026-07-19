# GarageBand-for-iPad feature-gap map — where Sonotron stands, competitively

Status: STRATEGY DOC (Verdi, roadmap-strategist pass, 2026-07-16). Read-only research —
no product code, no roadmap doc, touched. This maps the owner-supplied GarageBand-for-iPad
inventory against Sonotron's actual built/planned state, cites the evidence for every call,
and ranks what would extend Sonotron's identity best. It does not replace `docs/roadmap.md`;
any accepted item here is the owner's to fold into that tree.

## 0. The framing constraint, and a grounding correction the pass turned up

Sonotron is a **dual-target MIDI ARRANGER brain** — freestanding, no-heap,
no-float-in-realtime STM32H743 core (`arrangrr`) plus a host Linux GUI/orchestrator
(`sonotron`) and an optional host-only audio peer (`melodd`, a soundfont GM synth via
`tinysoundfont`) — never a linear audio DAW. `docs/product-vision.md` states this as
identity, not aspiration: *"NOT a linear DAW... NOT: a linear audio timeline, audio
tracks, audio editing/warp/comp, a mixer, or a plugin host."* Every verdict below is
weighed against that identity and against what the tree already ships or has already
decided against — not against a generic "products like this" instinct.

**A finding load-bearing for this whole map, verified in code, not assumed:** the two
canonical status trackers (`docs/roadmap.md:251-260,284` and `docs/DESIGN.md`) still mark
node `6000` (Looper) and `8100` (Scenes/song mode) as `○ planned`. That is **stale**.
Phase 7 shipped both:

- **Node `6000` (Looper) is SHIPPED**, not planned: `LoopBuffer`
  (`components/core/arrangrr/include/arrangrr/loop/loop_buffer.hpp`, commit `eaf9d50`
  "add node 6000 Looper primitive (Phase 7 SLICE 1)", QA'd in `bc693d1` and again in
  `6e89bda` "keep the most-recent tail on a dense retro-capture grab (6300 QA)") ships
  record/overdub/erase/undo (`Param::kLoopNew/kLoopRecordStart/kLoopRecordStop/
  kLoopErase/kLoopUndo/kLoopLength = 59–64`, `abi.hpp`), quantize-after, retroactive
  capture (node `6300`, QA'd), loop length, and follow-chord re-harmonize (node `6500`
  — `test_loop_reharmonize.cpp` exists and is green). The whole `6100`–`6500` band is
  built and tested (`test_loop*.cpp`, `test_loop_buffer_sounding_overflow_regression.cpp`
  ×2 QA passes).
- **Node `8100` (Scenes/song mode) is SHIPPED**, not planned: `SceneChain`
  (`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`, `Param::kSceneAdd/
  kScenePlay/kSceneStop/kSceneClear = 65–68`), 844 lines across `test_scene.cpp`/
  `test_scene_hardening.cpp`/`test_scene_meter_gate_regression.cpp`. Confirmed
  independently by `docs/proposals/repeat-zone-real-contract.md:22-35` (Corelli), which
  flags the same stale-docs finding.
- **The Repeat-Zone launch grid (Sonotron's Live-Loops analog) is now REAL, not
  inert.** `docs/gui-and-ux.md` (§4.4) — the doc this task asked me to skim — still
  describes launch as `kGridLaunchWired = false`, INERT. That has since shipped: the
  owner signed off all four forks in `docs/proposals/repeat-zone-real-contract.md`
  §8b (2026-07-16) and the implementation landed same-day, commit `3398f04` "Repeat
  Zone real — readback + Shape-A clip binding". `apps/gui-sonotron/docs/feature-list.md`
  confirms: launch cells are `[wired]`, scene-column launch is `[wired]`, mute/solo
  rows are `[wired]`, and `kClip`/readback consumption is real. **The single gap left
  by design** (owner decision 2, `repeat-zone-real-contract.md:488-491`): a cell's
  *content* is real only for a style dropped from the Browser — in-app authoring of a
  bespoke step/chord/loop clip from inside the GUI is explicitly out of scope for this
  pass, still CLI/script-only.
- Node `9210` (generative style / Motif+transforms) is also SHIPPED, not the
  "○ planned, behind-the-line item 8" `docs/roadmap.md:617` still shows: commit
  `f850ea2` "wire MotifSpec into 12 built-in styles (Phase 7, node 9210, Option-1
  batch)".

This matters for the mapping below in one direction only: several "GarageBand-like"
capabilities I might otherwise have called gaps (record-into-a-cell, scene/song
recall, launch-quantize) are **already built at the engine level** — the actual gap
left is almost always **GUI authoring surface**, not missing core capability. That
reframes several TOP-10 entries as *"wire the GUI to what already exists"* rather
than *"build a new engine."*

Sources read for grounding: `docs/roadmap.md` (802 lines, the canonical WBS, dated
2026-07-06 ground-truth — now stale in the two places above), `docs/DESIGN.md` (§0–§3,
the invariants and module table), `docs/product-vision.md`, `docs/gui-and-ux.md`
(1268 lines — the GUI spec, also partially stale as noted), `docs/backlog-future.md`
(the melodd/audio-input/arranger-vs-sequencer analysis), `docs/proposals/
repeat-zone-real-contract.md`, `docs/proposals/isoundengine-contract.md`,
`docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md`, `apps/gui-sonotron/
docs/feature-list.md`, `components/core/arrangrr/include/arrangrr/{loop,clip,
scene}/*.hpp`, `components/core/arrangrr/include/arrangrr/config.hpp`,
`components/core/arrangrr/include/arrangrr/abi.hpp`, and the git log (`3398f04`,
`3aa3b3a`, `1008280`, `50809da`, `617f453`, `38a7efc`, `b27c46c`, `0241dcd`, `f850ea2`,
`eaf9d50`, `bc693d1`, `6e89bda` and neighbors). GarageBand facts I was not certain of
(Jam Session's exact sync/leader model, Remix FX Gyro Control) were verified by web
search, cited at the end.

---

## 1. Four-bucket mapping, by the owner's feature groups

Legend: **HAVE-S** = have, shipped (cited evidence) · **HAVE-R** = have, on roadmap
(node cited, not yet built) · **PARTIAL** = genuine fit only in a reduced/adapted
form (form stated) · **MAKES SENSE** = real gap, not yet planned, worth adding ·
**NEVER** = will never fit, reason stated.

### 1.1 Creazione e struttura del progetto

| Feature | Bucket | Evidence / reason |
|---|---|---|
| Vista Tracks, linear timeline | **NEVER** | Refused as product identity: `product-vision.md` "NOT a linear audio timeline... Arrangement view, no." Not a gap — a deliberate non-goal. |
| Vista Live Loops (grid) | **HAVE-S** | Repeat Zone is the GUI's declared HERO zone (`docs/gui-and-ux.md` §3, §4.4); launch is now real (§0 above, commit `3398f04`). |
| Fino a 32 tracce | **PARTIAL** | Sonotron has 9–10 fixed musical ROLES (`TrackRole`: drums/perc/bass/chord1/chord2/pad/arp/phrase/lead + cc, `kRoleCount=10`, `docs/roadmap.md:230-233`), not 32 arbitrary tracks. A role-based band by design, not a lesser multitrack. |
| Tracce audio / strumenti virtuali / Drummer / loop | **PARTIAL** | Sound is realized by `melodd` (GM soundfont, host-only) per style-part `Program`, not per free-assigned audio track (`product-vision.md` "Sound & audio"). The arranger's drum style-part + section/fill system (node `3160`, 13 section types) is a *generative* Drummer analog, arguably stronger (see §3). |
| Sezioni configurabili; duplicazione/riordino/variazione | **HAVE-S** / PARTIAL | Section model shipped (node `3160`), 16 style variations (`3260`); the *engine* has sections, but no drag-to-duplicate/reorder section UI exists in the GUI — PARTIAL on the UI half. |
| Metronomo e count-in, tap-tempo | **HAVE-R** | Node `7400` "Metronome/click, tap-tempo, tempo-nudge" — `○ SHIPPABLE`, not built. Tempo-nudge itself IS built (commit `17ea925`), the click/count-in is not. |
| Tempo, tonalità | **HAVE-S** | `bpm <n>`, `key <root> <mode>` (node `2110`), engine-side and GUI-wired (transport rack BPM/key, `feature-list.md` §1). |
| Indicazione di tempo (time signature, selectable) | **MAKES SENSE, but LARGE** | `kBeatsPerBar=4` is a global `constexpr`, consumed at 13+ call sites (`docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md` §1, Fork D). A real variable-time-sig engine is a cross-cutting rewrite, explicitly descoped even for `8100`. Real, not close. |
| Loop/cycle di una sezione | **PARTIAL** | Style sections already loop until changed (arranger behavior); no user "set a cycle range in bars" on a linear ruler — doesn't fit the non-linear model, served differently by "hold this section." |
| Righello in battute o min:sec | **NEVER** | Needs a linear ruler timeline — refused by identity. |
| Blocco note integrato | **MAKES SENSE, trivial** | No roadmap mention; a small host-only text field on the project state. |
| Salvataggio automatico | **HAVE-R** | Node `8400` "Project/Preset manager" `○ SHIPPABLE`, not built. `Performance` binary save/load (node `8500`) IS shipped and could back an autosave cheaply. |

### 1.2 Live Loops e performance

| Feature | Bucket | Evidence |
|---|---|---|
| Griglia clip/celle, avvio cella, avvio colonna/scena, quantizzazione lancio | **HAVE-S** | `ClipMatrix` (`clip_matrix.hpp`), `kClipLaunch/kClipStop/kSceneQuantize` (`abi.hpp:221-227`), launch-quantize honored by the core clock (`ClipMatrix::on_bar`); GUI-wired and real per §0. |
| Celle audio o generate da strumenti | **PARTIAL** | Cells launch MIDI content only (style section / chord sequence / step track / loop buffer, `ContentKind` in `clip_matrix.hpp`). Raw *audio* cells are explicitly Flow #6 in `docs/gui-and-ux.md` ("Deploy audio color... HOST-ONLY, deferred... melodd does not exist yet as an engine peer for this") — named, not built. |
| Registrazione diretta dentro una cella | **HAVE-S (engine) / GAP (GUI)** | `LoopBuffer` record/overdub/erase/undo (node `6000`) is fully shipped (§0). The GUI has **zero authoring affordance** for it today — `repeat-zone-real-contract.md` §3 explicitly scopes "in-app step/chord/loop authoring... OUT of scope for this pass." Content is CLI/script-only. |
| Importazione Apple Loops / file audio | **NEVER (literal) / PARTIAL (concept)** | No Apple-Loops format; the general idea ("import an opaque asset into a launchable clip") is the entire subject of `docs/backlog-future.md` §3 (external audio input), judged musically worth it but explicitly "not yet scheduled." |
| Editing/copia/spostamento celle | **GAP** | `seqedit_panel.cpp` renders a procedural preview only (`neon::clip_pattern(hash_label(...))`) — no move/copy/duplicate affordance anywhere (`docs/gui-and-ux.md` §4.7, confirmed in code). |
| Registrazione in tempo reale dell'intera performance Live Loops | **PARTIAL** | `SceneChain` (node `8100`, shipped) captures rig-state-over-time, not "record my whole live jam as a linear performance" the way GarageBand converts a Live-Loops take into Tracks-view regions — different mechanism serving a related intent. |
| Template Live Loops preconfigurati | **MAKES SENSE** | No starter-grid concept; cheap to build on `ClipMatrix` + the 16 built-in styles. |
| Creazione griglie personalizzate | **HAVE-S** | Drag-from-browser + "+", scene count 3–8 (`grid_panel.cpp`, `feature-list.md` §2b). |
| Remix FX (filtri, repeater, scratch, tape-stop) | **NEVER (literal) / MAKES SENSE (MIDI-domain reframe)** | Needs an audio-DSP effects engine Sonotron's core structurally refuses to own (`product-vision.md`: arrangrr never processes audio). The MIDI-domain analog — glitch/ratchet/probability-mangle via the already-shipped MIDI-FX chain (node `5100`: echo/note-repeat/velocity-proc/scale-lock) exposed as a live performance panel — is a real, cheap opportunity; see TOP-10 #6. |
| Controllo Remix FX via movimento iPad (Gyro) | **NEVER** | No touchscreen/accelerometer target class; `12400` (physical UI, pads/encoders/display "if/when in scope") does not include motion sensors. |

### 1.3 Strumenti virtuali

| Feature | Bucket | Evidence |
|---|---|---|
| Tastiere, organi, synth, chitarre, bassi, archi, drum kit, percussioni, strumenti etnici | **PARTIAL** | Realized only via `melodd`'s GM soundfont playback (`components/melodd`, wraps `tinysoundfont`) selected per role `Program` (node `3230`/`3240`) — no dedicated instrument modeling/browsing UI. This is intentionally thin: `product-vision.md` scopes audio as "subordinate color," never the product's surface. |
| Alchemy Synth (wavetable/spectral + Transform Pad morphing) | **NEVER** | A deep synthesis engine is audio-DSP-owning territory. The closest conceptual future seat is the `analog` engine SLOT (`docs/proposals/isoundengine-contract.md` §4 — "no code," a from-scratch subtractive synth, not Alchemy-class) — distant and unstarted. |
| Multitouch controls, pitch bend/mod/sustain/velocity, glissando | **PARTIAL / GAP** | These exist at the MIDI-transport level (raw thru); an on-screen playable multitouch surface does not exist anywhere in `apps/gui-sonotron` — the app is mouse-first, desktop, no touch target (`docs/gui-and-ux.md` §8). |
| Scale configurabili, tastiera mono/poli | **PARTIAL (done differently, arguably deeper)** | Key/scale engine (node `2110`) + NTT chord resolver (node `3110`) + planned scale-lock MIDI-FX (`5230`) go well beyond a "scale keyboard" — but there is no touch keyboard widget to hang it on. |
| Salvataggio preset | **PARTIAL** | `Performance` (node `8200`, shipped) recalls whole-rig state including per-role program — subsumes "instrument preset" at the role level; no per-patch preset browser. |

### 1.4 Smart Instruments e accompagnamento

| Feature | Bucket | Evidence |
|---|---|---|
| Chord Strips, accordi personalizzabili | **HAVE-S, and stronger** | Chord modes (diatonic/single-finger/shell, node `2200`) + Chord sequencer (`2400`, the **1st WOW**) + live harmonizer/detector (`2300`) — this is the product's central identity, not a bolt-on. |
| Smart Keyboard/Guitar/Bass/Strings (manual per-instrument autoplay) | **NEVER, superseded on purpose** | Sonotron's whole-band arranger (node `3000`, the **2nd WOW**) generates all parts from ONE chord input; per-instrument manual "Smart" patterns is the exact granularity the arranger was built to make unnecessary. Say this plainly: the product does this *differently and better for its own identity*, not worse. |
| Smart Drums grid, Drummer virtuali, complessità/fill/parti controls | **PARTIAL, and the better answer is already queued** | No manual drummer grid exists; the arranger's fill/section system (`3160`) auto-generates fills. The *intent* GarageBand serves (dial complexity continuously) is exactly what the unbuilt Director capstone (node `10000`, energy/tension/valence, `product-vision.md`'s "Director vocabulary") is FOR — the roadmap already has the stronger answer designed, just not shipped. |
| Drummer che segue un'altra traccia | **HAVE-S, broader** | `9310` Accompany (SHIPPED HOST-ONLY: `AccompanyPipeline`, `components/orchestrator/include/orchestrator/accompany.hpp`) feeds a whole band — drums included — from chords detected in an imported melody. More general than "drummer follows my track": it re-derives harmony, not just rhythm. |

### 1.5 Sequencing e campionamento

| Feature | Bucket | Evidence |
|---|---|---|
| Step sequencer (on/off, pattern length, velocity) | **HAVE-S** | `Timeline`/`Track`/`Step` (node `1400`/`4000`). |
| Probabilità, ratchet, tie, micro-timing | **HAVE-S** | Node `4100` family (`4110`–`4140`), seeded, tested, fire-order-invariant (`4150`). |
| Rest/conditional-trig, euclidean/rotation | **HAVE-R** | Node `4200` (`4210`/`4220`), `○` planned, deferred as polish. |
| Registrazione pattern nella timeline (overdub into the hand-authored step grid) | **HAVE-R, narrowly** | Node `4300` "Track record/overdub" `○ SHIPPABLE`, still not built — distinct from `LoopBuffer`'s own record path (which IS shipped, §0). Precise nuance: live-record-to-**loop** exists; live-record-to-**step-grid** does not yet. |
| Sampler (load/trim/tune/reverse/loop/chromatic playback) | **NEVER (now) / HAVE-R (slot)** | `components/platform/engines/samplrr/README.md`: *"SLOT — no code yet."* Architecturally reserved in the audio-engines family plan, zero implementation. |
| Registrazione da microfono, import audio, trim/tune/reverse | **HAVE-R, explicitly judged** | The single most thorough existing analysis of exactly this ask is `docs/backlog-future.md` §3 (external audio input): judged musically worthwhile (the "un-regenerable artifact" argument), a precise two-tier clip model already designed, explicitly "not yet scheduled." |

### 1.6 Registrazione audio

| Feature | Bucket | Evidence |
|---|---|---|
| Mic interno/esterno, multicanale, monitoring, noise gate, preset vocali, effetti realtime, 24-bit, Multi-Take, amp/cabinet sims, stompbox | **NEVER, save one exception** | Structural refusal repeated in `product-vision.md` and `docs/backlog-future.md`'s "one sentence" governing rule: *"`melodd` may own the sound, never the time, and the core may never learn its name."* A multitrack audio-recording/effects rack is DAW gravity the whole design exists to refuse. **Exception:** raw external-audio *capture* (mic/DI) as an opaque, content-addressed, never-authoritative asset is judged worthwhile and precisely scoped in `backlog-future.md` §3 — bucket that one sliver as **PARTIAL/future**, everything else (amp sims, stompboxes, 24-bit multitrack comping, noise gate) is a flat **NEVER**: it needs an audio-processing/mixing surface the product's identity is built to reject. |

### 1.7 MIDI

| Feature | Bucket | Evidence |
|---|---|---|
| Registrazione MIDI da strumenti Touch | **PARTIAL** | Recording exists engine-side (LoopBuffer/ChordSequencer); no on-screen touch instrument exists in the GUI to record FROM. |
| Editing note via piano roll (move/copy/resize/velocity/quantize/transpose) | **GAP (GUI) / HAVE-S (engine, CLI only)** | Sequence Edit is a read-only procedural preview (`seqedit_panel.cpp:103`); real note editing exists only via TUI/CLI `track step <i> <note> <vel> <gate>` verbs. A concrete, sizeable gap. |
| Import MIDI | **HAVE-S, host-only, not GUI-exposed** | `midisrc`'s SMF parser + MIDI-source stage (node `9300` family), `midi-source load <path>` verb plumbed but **no GUI trigger** (`docs/gui-and-ux.md` §7, "Flows plumbed... NOT reachable"). |
| Tastiere MIDI USB | **HAVE-S** | Multi-port HAL, DIN+USB (node `1260`). |
| Bluetooth MIDI | **GAP, low priority** | No BLE-MIDI stack of Sonotron's own; anything working today would be incidental host-OS ALSA behavior, not an owned feature. |
| Controller MPE | **MAKES SENSE, distant** | Not mentioned anywhere in the roadmap; the MIDI-In parser (node `1220`) is note/running-status based, no MPE per-note channel-rotation handling documented. |
| Controllo da tastiera HW dell'iPad | **N/A** | No iPad-class device target. |
| Registrazione movimenti dei controlli (control automation) | **HAVE-R** | CC/pitchbend automation lanes (node `4400`) `○ SHIPPABLE`, not built. |

### 1.8 Editing e arrangiamento

| Feature | Bucket | Evidence |
|---|---|---|
| Cut/copy/paste/duplicate/split/trim/move+snap/reverse/gain/fade/bounce regions | **NEVER, structural** | All assume a linear-region editing model the product refuses (`product-vision.md` "NOT: ...audio editing/warp/comp"). |
| Transpose regions | **HAVE-S, and better** | `ChordSequence::transpose_to/transpose_by` (node `2410`, D28) is functional/degree-relative, not literal pitch-shift — it re-harmonizes correctly on transpose, which literal audio transpose cannot do. Named explicitly in §3 as a place Sonotron does it better. |
| Import da File/USB/SD/dischi/libreria musicale | **PARTIAL** | MIDI import exists (host-only, plumbed, not GUI-exposed); no generic file-browser/USB/SD import UI — desktop-local-path only. |

### 1.9 Mixer ed effetti

| Feature | Bucket | Evidence |
|---|---|---|
| Volume/pan/mute/solo per traccia | **PARTIAL** | Mute/solo per role: **HAVE-S**, real and GUI-wired (`PartInfo`, node `8200`; grid + parts rows). Volume "amount" knobs exist in the v02 GUI but are **local-only** — no wire verb (`feature-list.md`: "NO per-part amount/volume verb on the wire"). Pan: **GAP**, no dedicated concept. |
| Compressione/EQ/riverbero/echo master, Bitcrusher/Overdrive/Distortion/Chorus/Flanger/Tremolo/Vocal Transformer, Visual EQ | **NEVER** | All are audio-DSP effects requiring an audio-processing engine the core structurally refuses to own. The honest MIDI-domain substitute (an "echo" that repeats *notes*, not samples) already exists — see next row — and should never be conflated with the audio version. |
| Plug-in per traccia (AUv3), MIDI-FX in the chain | **PARTIAL/HAVE-S** | Sonotron's own MIDI-FX insert chain (node `5000`/`5100`, SHIPPED core: scale-lock/velocity-proc/echo/note-repeat, commit `95f542b`) is the functional analog — MIDI-only, not audio-plugin hosting (VST/AUv3 hosting is named as a *future* workstation capability in `product-vision.md`, not built). |
| Automazione grafica volume, registrazione modifiche controlli | **HAVE-R** | Same CC-lane gap as `4400` above. |

### 1.10 Libreria dei contenuti

| Feature | Bucket | Evidence |
|---|---|---|
| Sound Library scaricabile, sound pack, Producer Packs, notifiche, gestione contenuti | **NEVER** | Needs a cloud/network content-distribution service — against the dependency-free, offline-first core policy (`0800`) and against the whole "no host cloud dependency" posture of the project. |
| Apple Loops, drum kit, patch strumenti, Remix Sessions | **NEVER** | Same reason, plus the audio-content-library gravity the design refuses. |
| Ricerca/filtraggio loop | **HAVE-S, over a different corpus** | Browser search filter exists today over styles (`browser_model.cpp`, case-insensitive substring, `docs/gui-and-ux.md` §4.3) — same UX pattern, applied to styles/clips, not audio loops. |
| Preview sincronizzata al tempo, adattamento automatico tempo/tonalità | **HAVE-S, and arguably better** | Style loading auto-conforms to current tempo/key by construction (NTT resolver + per-style tempo, `3110`/`9120`) — generative, not sample time-stretching, so there is no audible artifact to hide. |

### 1.11 Collaborazione, integrazione ed esportazione

| Feature | Bucket | Evidence |
|---|---|---|
| Jam Session multi-device | **NEVER (near-term)** | Needs realtime multi-device network discovery/sync Sonotron has zero groundwork for — the UDS-JSONL adapter (node `11500`) is a **local socket only**. Verified (WebSearch): Jam Session requires Wi-Fi peer discovery, a leader/member model, and — notably — Apple's own version explicitly does **not** support the Live-Loops grid, only Tracks view. The cheap, MIDI-native version of "play together" is external clock-in (node `4500`, `○ SHIPPABLE`, unstarted) — slaving to a shared MIDI clock, not a network session. |
| iCloud Drive sync | **NEVER** | Cloud dependency, against `0800`. |
| Apri in GarageBand Mac / Logic Pro iPad / aggiungi tracce remote a Logic | **NEVER** | Apple-ecosystem-specific, no analog and no reason to build one. |
| Condivisione progetto modificabile | **PARTIAL** | `Performance` save/load (node `8500`, versioned binary + CRC, shipped) is a real project-portable format; no "share with another user" flow exists on top of it. |
| Export audio, AirDrop/email, SoundCloud, suonerie, mirroring Apple TV | **NEVER** | Apple-ecosystem/audio-export specific. The honest MIDI-native analog, `export-smf`, already exists as a shell verb (host-only, no GUI affordance, `docs/gui-and-ux.md` §7). |
| VoiceOver, scorciatoie da tastiera HW | **MAKES SENSE, currently under-delivered** | No accessibility layer; keyboard shortcuts are minimal and partly cosmetic (Ctrl+P is advertised as a label but **not actually wired**, `docs/gui-and-ux.md` §4.1). Cheap, host-only, worth fixing regardless of GarageBand parity. |

---

## 2. TOP 10 — best fit for Sonotron's identity, ranked

Ranked by how strongly each reinforces "live, playable arranger/looper instrument,"
not by raw GarageBand prominence. Each states WHY it fits, effort/regime, and what
it extends.

1. **Wire the shipped LoopBuffer to a Repeat-Zone cell gesture (record/overdub/
   erase/undo on press-and-hold).** Why: this is GarageBand's single most iconic
   Live-Loops move ("record into a cell") and the *entire engine side is already
   done* — `kLoopRecordStart/Stop/Erase/Undo` (node `6000`, Phase 7 SLICE 1). Effort:
   **HOST-ONLY GUI wiring, small-medium**; zero core work. Extends: the just-shipped
   Repeat Zone (`3398f04`) + `LoopBuffer`.

2. **Make Sequence Edit a real note editor** (move/copy/resize/velocity/quantize on
   `Timeline` steps, via the existing `track step` verb family). Why: the single
   biggest concrete GAP against GarageBand's celebrated ease-of-editing; today it is
   a read-only procedural preview. Effort: **HOST-ONLY GUI, medium-large** (new
   canvas interactions); no new core verb needed. Extends: node `4000` step params
   (already rich: probability/ratchet/tie), the Sequence Edit zone.

3. **Cell/scene management: drag-copy/duplicate/rename cells and scenes** in the
   Repeat Zone, with the `scenes.json` persistence already scoped. Why: natural next
   increment on a grid that just went from inert to real; closes the "stable across
   restarts" gap the owner already flagged. Effort: **HOST-ONLY, small** (persistence
   design already written, `repeat-zone-real-contract.md` §6 item 4). Extends: the
   Repeat Zone.

4. **Auto-song / scene auto-advance** (grid-COLUMN auto-follow at bar/section
   boundaries) — this is not a proposal, it is **already owner-signed-off and
   scoped** (`repeat-zone-real-contract.md` §8b item 4: new `m_active_scene` cursor +
   `auto_song` flag, reusing the existing `kSceneQuantize` dispatch). Why: the direct
   analog of GarageBand's "scene launch, auto-follow." Effort: **SHIPPABLE core,
   small new verb + bar-boundary driver**, no new dependency. Extends: `ClipMatrix`.

5. **Metronome click + count-in + confirm tap-tempo** (node `7400`). Why: table-
   stakes GarageBand feature Sonotron is missing outright; a live-performance product
   without a click is a real, embarrassing gap for Persona A (the solo arranger-
   keyboard player, `docs/gui-and-ux.md` §2). Effort: **SHIPPABLE core, small** (tap-
   tempo/nudge already exists, commit `17ea925`; click is a scheduled MIDI/audio
   pulse). Extends: Transport.

6. **A MIDI-domain "Remix FX" performance panel** — expose the already-shipped
   MIDI-FX chain (echo/note-repeat/velocity-proc/scale-lock, node `5100`) as live
   XY-style knobs during play, reframed honestly in MIDI terms (glitch/ratchet/
   probability-mangle, never audio filter/tape-stop). Why: captures the *spirit* of
   GarageBand's most fun performance feature without owning an audio-DSP engine the
   product refuses. Effort: **HOST-ONLY GUI, small-medium**; core is done. Extends:
   node `5100`, the Intention rail's existing knob widgets (`neon::knob`, already
   built for energy/tension/valence).

7. **Surface retroactive capture ("grab the last N bars") in the GUI.** Why: node
   `6300` is shipped and QA'd (`6e89bda`) but has **zero GUI affordance** — the
   cheapest possible "new feature" since the engine cost is already paid. Effort:
   **HOST-ONLY GUI, small**. Extends: `LoopBuffer`, the Repeat Zone.

8. **Port the Groove and Arp panels into the GUI** (today TUI-only,
   `docs/gui-and-ux.md` §7: "exist only in the TUI... no GUI equivalent"). Why: two
   of the product's own signature engines (groove/humanize, node `3250`/`7300`; live
   arp, `7110`) are invisible in the flagship GUI — an odd asymmetry to leave open
   while building GarageBand-parity features elsewhere. Effort: **HOST-ONLY GUI,
   small**, verbs already exist (`groove <field> <v>`, `arp <field> <v>`). Extends:
   the Parts/Mixer rail.

9. **Style/Live-Loops starter templates** (a handful of pre-populated grids per
   genre, seeded from the 16 built-ins + the in-flight corpus importer, node
   `9430`). Why: GarageBand's own "pick a Live Loops template" is its fastest path to
   a first WOW; Sonotron's analog (a genre grid ready to press play) is cheap given
   the browser/ClipMatrix machinery already exists. Effort: **HOST-ONLY, small**
   (author a handful of `.arrangrr.init`-style seed scripts or a `clip add` batch on
   startup). Extends: Browser + Repeat Zone.

10. **Project/Preset manager + autosave** (nodes `8300`/`8400`), riding the already-
    shipped `Performance` binary format (`8500`). Why: closes the persona explicitly
    named as "REAL but NOT v1-serviceable" (`docs/gui-and-ux.md` §2, Persona C — the
    performer who must prep 15 songs and recall them without panic). Effort:
    **HOST-ONLY, small-medium** (slot management UI + autosave timer over an existing
    format — no new binary layout needed). Extends: `Performance`/node `8200`.

*Runner-up, worth naming but not top-10:* CC/pitchbend automation lanes (`4400`) and
euclidean/rest step params (`4200`) are good, cheap, SHIPPABLE-core polish but lower
leverage than the ten above because nothing currently blocks on them.

---

## 3. Where GarageBand's approach would dilute Sonotron, and where Sonotron already wins

**Dilution risks, named plainly, not just dismissed:**
- **Any audio-recording/mixing/effects surface** (mic input, amp sims, stompboxes,
  a compressor/EQ/reverb rack, AUv3 hosting) would import exactly the "DAW gravity"
  `docs/product-vision.md` spends its whole design resisting, and would compete with
  Ableton/Logic on ground Sonotron cannot win (`gui-and-ux.md` §1: "measured against
  Ableton it has lost the ground where it actually wins"). Building any of these
  would spend engineering effort on a battle the product doesn't need to fight.
- **A cloud content library / Sound Packs** contradicts the dependency-free,
  offline-first core policy (`0800`) for no proportionate product gain — Sonotron's
  content model (generative styles + a growing corpus importer) is a *better* fit
  for a product whose whole differentiator is deterministic, offline, reproducible
  output (`0100`).
- **A literal linear Tracks-view timeline** — even as an "advanced mode" — would
  quietly re-open the exact DAW-gravity trap `backlog-future.md` §2/§3 spends pages
  warning against (the "monitor-rec continuous" appetite for scrolling audio is
  named as the specific force that pulls a Session product toward becoming a
  DAW). Refuse it, as designed.

**Where Sonotron already does it differently — and better, by the product's own
lights:**
- **Auto-arrangement from styles+chords vs. manual Smart-instrument comping.**
  GarageBand's Smart Guitar/Bass/Strings still require picking a pattern per
  instrument by hand. Sonotron's arranger (`3000`, the 2nd WOW) generates a whole
  coherent band from ONE chord, live. This is the product's actual thesis
  (`product-vision.md` pillar #2, "Harmonic intelligence") — not a smaller version
  of GarageBand's feature, a structurally different and more ambitious one.
- **Functional, degree-relative transpose/re-harmonize (D28) vs. literal pitch-shift.**
  A GarageBand region transpose moves samples; a Sonotron `ChordSequence`/`LoopBuffer`
  transpose re-derives the correct chord tones against the new key — it cannot
  produce a wrong note by construction. Loop `6500` follow-chord re-harmonize takes
  this further than GarageBand attempts at all.
- **"No wrong notes" for the human hand, not just the machine** (NTT pointed at live
  input) — named in `gui-and-ux.md` as a persona ("D — the beginner") "almost nobody
  ships well," and GarageBand does not ship it at all; it only offers fixed scale
  keyboards, not a resolver over a live band.
- **Reproducible generativity (seed-as-object, node `0100`).** GarageBand has no
  concept of "lock this variation, recall it byte-exact next week" — every Smart
  Instrument pattern-choice is a one-off manual decision, not a seeded, recallable
  fact. This is Sonotron's first pillar and has no GarageBand analog at all.

---

## 4. Headline counts and the 2-3 things that matter most

Across the ~95 feature lines mapped in §1 (grouped from the owner's ~11 sections):
roughly **18 HAVE-SHIPPED**, **14 HAVE-ON-ROADMAP** (node cited, not built),
**~22 PARTIAL** (real fit only in reduced/adapted form), **~9 MAKES-SENSE** (genuine
gap, not yet planned), and **~24 NEVER** (mostly the entire audio-recording/mixing/
effects/cloud-content/Apple-ecosystem clusters — a majority of the "NEVER" count sits
in exactly two GarageBand sections: "Registrazione audio" and "Libreria dei
contenuti," confirming those two sections are where the category genuinely diverges,
not where Sonotron happens to be behind).

**Three takeaways for the owner:**

1. **The Repeat Zone just crossed from inert to real (commit `3398f04`, today), and
   the Looper/Scenes engine work behind it (nodes `6000`/`8100`) has been done since
   Phase 7 — but two canonical docs (`docs/roadmap.md`, `docs/DESIGN.md`) still show
   both as `○ planned`.** Every "GarageBand parity" conversation from here should
   start from that corrected ground truth, or it will keep re-proposing engine work
   that is already shipped and only needs a GUI surface (§0, and TOP-10 #1/#3/#4/#7).
2. **The single largest genuine GarageBand-parity gap is Sequence Edit** — a
   read-only procedural preview standing in for what should be a real piano-roll/step
   editor (TOP-10 #2). Every other "editing" gap in §1.8 is a deliberate non-goal;
   this one is not — it is a placeholder the GUI restart has not filled yet, and it
   blocks in-app authoring of the very clips the newly-real Repeat Zone launches.
3. **GarageBand's audio-centric two-thirds (recording, mixer/effects, content
   library, ecosystem export) is not a gap to close — it is the boundary that
   defines what Sonotron is instead of.** The honest, high-leverage GarageBand
   features to chase are concentrated in exactly one bucket: **Live Loops
   performance mechanics** (launch, scenes, cell record, remix-style live mangling),
   which is also the bucket where Sonotron's engine work is already most complete.
   That is not a coincidence worth arguing with — it is the roadmap's own
   through-line, and the TOP-10 above simply names its next three notes.

---

## Sources (GarageBand facts verified, not assumed)

- [Jam with other users in GarageBand for iPad — Apple Support](https://support.apple.com/guide/garageband-ipad/jam-with-other-garageband-users-chsf2f99ff5/ipados)
- [GarageBand for iPad Live Loops overview — Apple Support](https://support.apple.com/guide/garageband-ipad/live-loops-overview-chsca7ff9ced/ipados)
- [Remix a song with GarageBand for iPad — Apple Support](https://support.apple.com/en-ae/guide/garageband-ipad/chsabb64c325/ipados)
- [Work in the Live Loops grid in GarageBand for iPad — Apple Support](https://support.apple.com/guide/garageband-ipad/work-in-the-live-loops-grid-chsd95b06794/ipados)
- [GarageBand User Guide for iPad (PDF) — Apple](https://help.apple.com/pdf/garagebandipad/en_US/garageband-ipad-user-guide.pdf)

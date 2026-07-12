# Orchestrator + pipeline extraction — pulling the runtime out of arrangrr

Status: **PROPOSAL (Corelli, 2026-07-12), design-first, no code moved.** This document is
the design mandated by `docs/design/project-structure.md` §"The orchestrator + the
composable pipeline" and its "Open / deferred" list ("Designing the orchestrator
pipeline", "Standing up `apps/sonotron-host`"). It is a **milestone**, not a move: the
owner reviews this doc; nothing here has been cut yet.

**Binding owner decisions this doc designs around (not relitigated):**
1. FULL extraction — the whole runtime (transport/clock, scheduler, dispatch, routing,
   the per-tick fire order) leaves `components/arrangrr`. `arrangrr` becomes a **pure
   arranger stage**. This OVERRIDES `project-structure.md`'s current "transport is
   intrinsic to arrangrr, stays" position — see "Reconciliation" below.
2. The ABI freeze (`components/arrangrr/include/arrangrr/abi.hpp`,
   `components/arrangrr/tests/test_abi_frozen.cpp`) is **waived** for this milestone.
3. A dedicated backend app, **`apps/sonotron-server`** (the owner's name — the doc
   historically said `sonotron-host`; naming reconciled below), links the components,
   builds the pipeline, and serves the UDS-JSONL socket. `gui-sonotron` and
   `cli-arrangrr` become pure socket clients (D38).
4. This is a design milestone per `project-structure.md`'s own governance, which names
   Corelli as the designer of the transport↔engine port and the N-instance model.

---

## 1. Cosa ho tracciato

Decisions read: `docs/design/project-structure.md` (full), `docs/design/workstation-vision.md`
(full), `docs/product-identity.md` (full), `docs/design/director-vocabulary.md` (full),
`docs/DESIGN.md` §0 invariants (`0100`–`0910`) and the D→node migration table
(D4→`0800`, D23→`1320`, D26→`1330`/`0700`, D29→`1310`/`1230`, D32→`0200`, D33→`0400`/
`12200`, D38→`11500`/`11600`, D43→`0910`), `docs/strategy/roadmap-numbered.md` node
`9310` (Accompany, ~line 286 and its rationale at line 445).

As-built graph, walked with the fresh (22 700-node) knowledge graph plus direct reads:

- **`arrangrr::Engine`** — `components/arrangrr/include/arrangrr/engine.hpp` (486
  lines) + `components/arrangrr/src/engine.cpp` (per-domain command handlers). `Engine`
  has in-degree 16 in the graph (16 direct callers/includers) — the single most-depended
  class of the freestanding core.
- **Members it owns** (engine.hpp:514–538): `Transport m_transport`,
  `MidiParser m_parsers[kMaxPorts]`, `Router m_router`, `Timeline m_timeline`,
  `ChordEngine m_chords`, `ChordSequencer m_seq`, `Arranger m_arranger`,
  `ChordDetector m_detector`, `ArpeggiatorEngine m_arp`, `NoteTracker m_tracker`,
  `OutScheduler<kSchedulerCapacity> m_scheduler`.
- **The D29 fire order**, `Engine::advance_ticks` (engine.hpp:185–216): per tick —
  `fire_clock_pulse` → `fire_timeline` → `fire_chord_seq` → (at bar boundary)
  `m_chords.commit_bar()` + `emit_chord_followed` (D53) → `fire_arranger` → `fire_arp` →
  `flush` (pops `m_scheduler`, the total-order emission queue, `engine.hpp:439-444`).
- **`push_command`** (`components/arrangrr/src/engine.cpp:8-72`) dispatches one binary
  `Command` (D26/`0700`) into `cmd_transport`, `cmd_routing`, `cmd_chord`, `cmd_seq`,
  `cmd_track`, `cmd_style`, `cmd_voice`, `cmd_arp` — transport, routing, harmony, the
  step sequencer, the style/arranger, program-change and the live arp all ride ONE flat
  `Param` enum (`components/arrangrr/include/arrangrr/abi.hpp:44-73`, kNone..kInputZone,
  next free id 43).
- **Inbound callers of `advance_ticks`** (`trace_path`, depth 3): only
  `components/hostrt/shell.hpp` → `Shell::advance_to` → `Shell::cmd_advance`
  (`components/hostrt/shell_io_commands.cpp`) → `Shell::dispatch_transport`, and the
  freestanding link-gate `components/arrangrr/src/engine_checks.cpp`. **Nobody else on
  the whole tree drives the clock** — confirming the clock-drive path is single-threaded
  through `hostrt::Shell`.
- **Inbound callers of `push_command`**: ~30 call sites, ALL inside
  `components/hostrt/shell_music_commands.cpp` / `shell_io_commands.cpp` /
  `shell_input.cpp` / `shell_chooser.cpp`, plus one `run_live` call in
  `apps/tools/cli-arrangrr/main.cpp` (indirectly — via `shell.exec_line`, not shown
  directly at this depth) and the engine's own link-gate.
- **`components/hostrt/shell.hpp`** (435 lines): `Engine m_engine;` is a **direct member**
  (shell.hpp:380, composition not reference) — the Shell owns the engine outright. The
  same class also owns: `PanelManager m_panels`, `PianoViewState m_piano`,
  `UiStyle m_style`, `StyleChooser m_chooser`, `MidiMonitor m_monitor`,
  `ActiveNoteTracker m_piano_held`/`m_harmony_held`, the kitty-keyboard momentary state,
  the style/section debounce state — i.e. **L2 command dispatch + core ownership + TUI
  panel/rendering state are one class**, confirmed by ~70 public methods spanning
  `cmd_*` dispatch, `advance_by`, AND `groove_key`/`arp_panel_key`/`chords_key`/
  `piano_key_event`/`chooser_nav_style` (pure TUI concerns).
- **`apps/tools/cli-arrangrr/main.cpp`** (823 lines): `run_live()` constructs
  `AlsaMidi alsa`, `Console console` (TUI), `UdsServer control` (the D38 control
  socket — `main.cpp:184-191`), and `Shell shell` in one function; the poll loop
  (`main.cpp:612-773`) multiplexes stdin, a 0.5 ms tick timer (driving
  `shell.advance_by` → `Engine::advance_ticks`, `main.cpp:650-661`), ALSA input,
  `control`'s listen + client fds, and kitty-protocol framing, ALL in one `while`. The
  control socket is served here (`control.broadcast(to_jsonl(ev, flats))`,
  `main.cpp:208-210`) — this literally is "today the socket-serving role is played by
  `cli-arrangrr`" from `project-structure.md:22`.
- **`apps/gui-sonotron`** — confirmed a genuine pure client already: CMakeLists.txt
  (`apps/gui-sonotron/CMakeLists.txt:1-11`) states and enforces "links neither
  arrangrr_core nor hostrt … zero core headers"; `src/uds_brain_session.cpp` +
  `src/app_state.cpp` reduce the JSONL event stream into client-side view state. D38 is
  **real** for the GUI today, not aspirational — this is the pattern to replicate for
  `cli-arrangrr`.
- **Golden tests**: `tests/golden/CMakeLists.txt` — 18 `arrangrr_golden(name)` targets,
  each running `$<TARGET_FILE:cli_arrangrr> --script name.acmd` and diffing canonical
  JSONL byte-for-byte against `name.golden` (`tests/golden/run_golden.cmake`). Example
  read: `tests/golden/hello_chord.acmd` drives `key`/`port open`/`chord out`/`play`/
  `advance`/`quit` — the L2 grammar, through the CLI binary, against one `Engine`'s
  wire output.
- **`components/arrangrr/tests/test_abi_frozen.cpp`**: pins every `Op`/`Param`/
  `OutEvent::Kind`/`WarnCode` numeric value and `sizeof(Command)==20`,
  `sizeof(OutEvent)==16`, `kProtocolVersion==1` via `static_assert`
  (`test_abi_frozen.cpp:25-111`) — the enforcement mechanism decision #2 waives.
- **`components/arrangrr/tests/test_engine*.cpp`**: `test_engine.cpp`,
  `test_engine_fire_order.cpp` (directly tests
  `test_chord_seq_resolves_before_arranger_same_tick`,
  `test_bar_one_downbeat_is_c_root` — the D29 order, in-process against `Engine`), plus
  `test_chord_seq_vs_live_steer.cpp`, `test_step_locks.cpp`, `test_input_zone.cpp` — 20
  files instantiating `Engine` directly.
- **`tests/arm-smoke/main.cpp`**: calls `arrangrr::engine_link_gate()`
  (`components/arrangrr/src/engine_checks.cpp`) which "instantiates the full Engine:
  transport, parser, router, scheduler, chord engine, sequencer, arranger and timeline"
  (comment, `tests/arm-smoke/main.cpp:14-16`) — this IS the freestanding proof that
  today's whole runtime cross-builds `arm-none-eabi`. Once `Engine` sheds transport/
  scheduler, this smoke test's claim changes shape (§2.4 below).
- **Dependency-freedom check (D4/`0800`)**: no `imgui`/`ImGui` symbol anywhere under
  `components/arrangrr/` (`grep -rl` returned empty) — the `get_architecture` boundary
  summary reported an `arrangrr → imgui` edge (`call_count: 415`) that does **not**
  survive a direct grep; I flag it as graph noise (almost certainly duplicate-symbol
  aggregation across `operator==`/`operator+` etc., which the same summary shows as
  imgui's top hotspots) — **not** a real dependency leak. D4 holds as-built.
- **Reusable prior art for Accompany's MIDI-source stage**: a dependency-free SMF reader
  already exists at `apps/tools/arrstyle-converter/src/smf.{hpp,cpp}` +
  `midi_import.cpp` — no new dependency is needed to feed a plain MIDI file into the new
  pipeline (§3.3, §6).
- **`components/hostrt/rc_config.hpp`** includes `panel_manager.hpp` — confirms
  `rc_config` (the `~/.arrangrr.rc` layout parser) is TUI-presentation-coupled and stays
  with the CLI, not the server. `jsonl.hpp`, `gm_program.hpp`, `note_names.hpp`,
  `uds_server.hpp` have **no** panel/TUI include — confirmed clean runtime-glue, safe to
  move to the server (§4).

---

## 2. L'architettura com'è costruita

### 2.1 Confini e proprietà

`Engine` is the de-facto runtime AND the arranger AND the harmony brain AND the
sequencer AND the arpeggiator, in one class, because nothing above it has ever needed
to distinguish them — `hostrt::Shell` is the only consumer and it happily owns `Engine`
whole (`shell.hpp:380`). The dependency direction is correct today (host depends on
core, never the reverse) but the **granularity** is wrong for the target state: there is
no seam between "the clock that must exist once, host-driven, dual-target" and "the
arranger that decides what to play" — decision #1 requires manufacturing that seam,
which does not exist on disk anywhere today.

### 2.2 Coupling e coesione

`hostrt::Shell` is the single highest-coupling class in the tree by construction: it
must change whenever ANY of {a new `cmd_*` domain, a new panel, a new TUI key binding,
a new ABI param} changes, because all four live in its ~70-method surface. This is not
a hypothetical smell — it is visible directly in the file: `cmd_transport`/`cmd_bpm`/
`cmd_route` (transport-and-routing) sit beside `groove_key`/`arp_panel_key`/
`chooser_nav_style` (TUI-only) in the SAME class, sharing `m_engine`. What changes
together (a new command) and what does not (a new panel) are entangled in one
compilation unit family (`shell*.cpp`, 7 files).

### 2.3 Qualità dell'astrazione

`Engine::EventSink = FunctionRef<void(const OutEvent&)>` (engine.hpp:58) is a clean,
monomorphic sink at the ABI boundary — a genuinely good abstraction, dependency-free
and no template bloat. But there is **no seam above `Engine` at all**: no `Stage`
interface, no pipeline type, nothing the orchestrator slot (`components/orchestrator`,
currently empty) could compose against. The abstraction that is "screaming for a seam"
(Corelli's brief) is precisely the boundary between "drives ticks + owns the total-order
queue" and "resolves one domain's notes for this tick" — `advance_ticks` conflates both
in one loop body.

### 2.4 Stabilità dell'ABI

The v1 ABI is honestly documented and enforced: `abi.hpp:14-32`'s frozen-baseline
comment plus `test_abi_frozen.cpp`'s `static_assert` wall is a real, working freeze —
not aspirational. It is however a **flat, single-engine** ABI: one `Param` enum spanning
every domain, one `OutEvent::Kind` enum, no stage/source tag on either struct. The
freeze is waived for this milestone (decision #2) precisely because a flat single-engine
ABI cannot express "which stage produced this event" or "which stage this command
targets" without a reshape — which is the whole point of moving to N composable stages.

### 2.5 I giunti dual-target

This is the axis decision #1 puts under the most stress. `docs/product-identity.md`'s
deployment matrix (`product-identity.md:89-96`) states STM32 firmware uses "**arrangrr
alone**" as the MIDI brain — which today literally means "the whole `Engine`, transport
and scheduler included" (proven by `engine_link_gate` above). Decision #1 removes
transport/scheduler from `arrangrr`, and `project-structure.md:119-120` already commits
the rich orchestrator to **host-regime only** ("VST/audio hosting does not run on
STM32… on the chip the minimal wiring (arrangrr → MIDI out) is done by the arm
entrypoint"). Put those two together and there is a **genuine gap on disk today**: there
is no component that is (a) freestanding/dual-target AND (b) owns the clock/scheduler/
fire-loop. `components/orchestrator` cannot be that component (it is declared
host-only); `arrangrr` is not allowed to be that component after decision #1. Nothing
currently fills it. §3.2 below proposes the fill.

### 2.6 Layering e deriva

The intended layering (`components/arrangrr` freestanding / `components/hostrt` host /
`apps/tools/cli-arrangrr` host CLI) IS real in the include graph — `hostrt`'s CMakeLists
links `arrangrr` PUBLIC + `ALSA::ALSA` (`components/hostrt/CMakeLists.txt:30`), never
the reverse; `arrangrr`'s own CMakeLists (`components/arrangrr/CMakeLists.txt`) has zero
external `target_link_libraries`. This part of the design has NOT drifted. What has
drifted is the **assumption baked into `project-structure.md`'s prose** ("the sequencer
stays a module in arrangrr… transport… intrinsic, stays in arrangrr",
`project-structure.md:97-98`) — the owner has now explicitly overridden that stance for
transport (decision #1). This document reconciles that below.

---

## 3. Deriva dalle decisioni

**`project-structure.md` §"Sequencer — one transport, N sequencing-engines"**
(lines 93-110) states: *"There is one [transport/clock master]… arrangrr cannot exist
without it → intrinsic, stays in arrangrr."* **The owner has overridden this for
decision #1 of this milestone.** The code (today) matches the stale doc position exactly
(`Engine` owns `Transport` — engine.hpp:515). **The doc yields, not the code-as-was**:
`project-structure.md` needs a follow-up edit (Palladio's placement call, not mine) once
this proposal is accepted, striking the "intrinsic, stays" sentence and pointing at this
document. Until edited, that section of `project-structure.md` is a **known, flagged
drift** — the owner's decision from the task brief is what governs, not the currently
-committed prose.

**`project-structure.md` §"The orchestrator + the composable pipeline"**
(lines 112-124) already anticipates the orchestrator becoming a real composed pipeline
and correctly scopes it host-only for VST/audio — that part does NOT drift and this
proposal keeps it. What the doc did not anticipate (because sequencing was assumed to
stay inside arrangrr) is that pulling transport out creates a **second, dual-target
seam** the orchestrator cannot hold (§2.5, §3.2) — this is new information this
milestone surfaces, not a contradiction of what's written.

**`docs/product-identity.md`'s deployment matrix** (STM32 = "arrangrr alone",
lines 89-96) is literally true only while arrangrr owns the transport/scheduler. After
decision #1 the STM32 row must read "the runtime kernel (§3.2) + arrangrr", a strictly
more honest statement of what was always physically true (arrangrr never made a sound or
drove real time by itself even conceptually — a HAL/main loop always drove `advance_
ticks`). **The doc should be updated by its owner-designated steward once this lands**;
flagged here, not edited by me (read-only on docs per my mandate beyond this one new
file).

**D26/`0700` (ABI discipline: "the core contract is typed BINARY commands/events…
param IDs are append-only")** is explicitly waived for this milestone by decision #2. I
still recommend NOT cashing the waiver immediately (§5, phase 1) — deferring the actual
`Command`/`OutEvent` reshape until the N-stage need (Accompany) makes the new shape
provable, rather than reshaping speculatively. This is a sequencing recommendation, not
a re-assertion of the freeze; the owner may of course reshape immediately if preferred.

---

## 3.5 The target component topology

```
STM32 / arm-none-eabi (freestanding, dual-target — D2/D33 preserved)
┌─────────────────────────────────────────────────────────────────┐
│  arm entrypoint (thin main, tests/arm-smoke successor)           │
│    links: components/runtime (kernel)  +  components/arrangrr    │
│    wiring: ONE fixed pipeline, MIDI-in → arrangrr stage → MIDI-out│
└─────────────────────────────────────────────────────────────────┘

Host (Linux x86, dev + product)
┌─────────────────────────────────────────────────────────────────┐
│  components/orchestrator   HOST-ONLY                             │
│    composes N stage instances into a named pipeline;             │
│    VST/CLAP/LV2 + audio-source adapters live here (future);      │
│    depends on components/runtime for the clock/scheduler/fire-loop│
│    — does NOT reimplement time or total order.                   │
│         │ uses                                                   │
│  components/runtime        dual-target, freestanding, no-heap    │
│    Transport (moved as-is) + OutScheduler (moved as-is) +        │
│    the StagePort pipeline-driver (the NEW seam, §3.6) +          │
│    the D29 total-order emission queue, now GLOBAL across stages. │
│         │ drives                                                 │
│  components/arrangrr       dual-target — REDUCED to a pure stage │
│    harmonic context + transport tick in → band MIDI OutEvents out;│
│    keeps: Arranger, ChordEngine (D47 gate), ChordSequencer,       │
│    ArpeggiatorEngine, Timeline (module — sequencrr fork, §6),     │
│    Router, NoteTracker. LOSES: Transport, OutScheduler,           │
│    advance_ticks' fire-loop (all → components/runtime).           │
│         │ fed by (fork, §6)                                       │
│  components/chorddet(?)    dual-target candidate — the live       │
│    ChordDetector + resolver, promoted OUT of arrangrr per the     │
│    component-vs-module criterion (project-structure.md's own      │
│    test: "absent, substituted, or multiplied" — the detector      │
│    passes: Accompany's chord-detect stage IS a substitution).      │
│                                                                    │
│  apps/sonotron-server       HOST-ONLY, the new backend            │
│    links: orchestrator + runtime + arrangrr(+chorddet) + the      │
│    non-TUI half of hostrt (jsonl, uds_server, gm_program,         │
│    note_names, alsa_midi) + AlsaMidi I/O + the live clock loop.   │
│    Serves the UDS-JSONL socket. Owns exactly what cli-arrangrr's  │
│    run_live() owns today MINUS the TUI.                           │
│         │ UDS-JSONL socket (D38, unchanged wire discipline)       │
│  apps/gui-sonotron          pure client (ALREADY TRUE — no change) │
│  apps/tools/cli-arrangrr    becomes a pure client TOO (§4) —      │
│    keeps ONLY the TUI half of hostrt (console, kitty_keys, panels,│
│    piano/style/groove/arp views, rc_config) + a UDS session.      │
└─────────────────────────────────────────────────────────────────┘
```

**See §14 (Phase-1 review addendum) for the CORRECTED dependency graph, which inserts
`components/common` beneath both `runtime` and `arrangrr` to resolve a real dependency
cycle Palladio's move-plan surfaced.**

---

## 3.6 The STAGE port / pipeline contract

**Design law (from `workstation-vision.md`/`product-identity.md`): the product has the
POWER of a DAW but never its free-for-all graph.** The port below is deliberately a
**fixed, declarative composition**, not a general dataflow/audio-graph engine with
cycles or runtime rewiring — that would import exactly the DAW gravity the whole
product refuses. This directly answers the "how far to generalize now" fork (§7): as
narrow as Accompany needs, not a VST-style node editor.

```cpp
// components/runtime/include/runtime/stage.hpp  (illustrative shape, NOT frozen —
// ABI is waived; the exact field layout is an implementation decision, not this doc's;
// SEE §14.2 — this virtual-class sketch is SUPERSEDED by a template/concept shape)

struct StageContext {
  Tick now;                    // stream tick — SAME injected clock every stage sees
  Tick transport_tick;         // 0 while stopped; the transport's own musical position
  bool transport_playing;
};

class Stage {
 public:
  // Called once per tick, in FIXED pipeline order (declared at construction, never
  // re-ordered at runtime — no cycles, no dynamic rewiring). `in` is whatever this
  // stage's declared input type is (raw MIDI bytes, a HarmonicContext delta, a
  // Command) — a stage is name-blind to what feeds it, same D43 discipline as
  // arrangrr/melodd today. `emit` is the SAME EventSink monomorphic callback
  // arrangrr::Engine already uses (engine.hpp:58) — reused, not reinvented.
  virtual void on_tick(const StageContext& ctx, EventSink emit) = 0;
  virtual ~Stage() = default;
};
```

- **Time injection**: `components/runtime`'s `Transport` (moved byte-for-byte from
  `arrangrr/transport/transport.hpp` — it already "owns no I/O and never reads a clock"
  per its own header comment, transport.hpp:7-10) is driven exactly as today: the host
  clock thread / virtual golden clock / STM32 timer calls `Runtime::advance_ticks(n)`.
  `arrangrr`-as-stage never sees a raw clock — it only ever sees the `StageContext` the
  runtime hands it, preserving "time is injected" verbatim from the current design intent
  (engine.hpp:22-24's own comment), just one level up.
- **Same-tick ordered composition (preserves D53/D29)**: stages fire in **fixed
  sequential order within one tick**, exactly mirroring today's in-engine order
  (`fire_timeline` → `fire_chord_seq` → chord-commit → `fire_arranger` → `fire_arp`).
  A chord-detect stage wired BEFORE the arrangrr stage in the pipeline order guarantees
  its output is visible to arrangrr in the SAME tick, preserving the "no wrong notes,
  same-tick steer" semantics D53 relies on today — the port is NOT a one-tick-lag
  message queue between stages.
- **Total order across stages**: `components/runtime` owns ONE `OutScheduler` instance
  (moved as-is from `arrangrr/scheduler/out_scheduler.hpp`), fed by every stage's `emit`
  calls, tagged with the stage's declared priority — generalizing the current
  `(tick, class_priority, seq_no)` tie-break (`out_scheduler.hpp:9-14`) from one
  engine's internal calls to N stages' calls, same tie-break rule.
- **Accompany (`9310`) end-to-end, concretely**:
  `[MIDI-source stage]` (reads a plain SMF file via the existing dependency-free reader
  at `apps/tools/arrstyle-converter/src/smf.{hpp,cpp}`, HOST-ONLY because file I/O —
  same "core never touches the filesystem" precedent already established for
  `state.dump`, `DESIGN.md:1696`) → emits raw MIDI thru events AND feeds notes into →
  `[chorddet stage]` (the promoted `ChordDetector`+resolver, dual-target-capable) →
  emits `HarmonicContext` changes → `[arrangrr stage]` (unchanged internals: Arranger +
  ChordEngine's D47 gate resolve the NTT-safe band notes against the fed harmonic
  context and the shared transport tick) → the runtime's scheduler merges the melody
  thru-events and the band's MIDI in D29 total order → one JSONL/MIDI output stream.
  `components/orchestrator` (host-only) is what WIRES these three named stage instances
  into this specific named pipeline — it is the thing that "knows" the topology; no
  individual stage knows its neighbours (D43 held).

---

## 4. `apps/sonotron-server`

**Naming**: the owner's brief names it `sonotron-server`; `project-structure.md`
currently says `sonotron-host` in its repo-layout figure and prose
(`project-structure.md:21,32,67,188`) and in its own "Open/deferred" list line 188. I
default to **`apps/sonotron-server`** per the explicit owner instruction for this
milestone and flag the resulting doc/code name mismatch as a FORK for the owner to
settle formally (§7) — whoever wins, `project-structure.md`'s repo-layout figure needs a
follow-up edit by its steward (Palladio), out of my read-only scope beyond this one new
file.

**What it links**: `components/runtime` + `components/orchestrator` (even trivially,
a single-pipeline instance at first) + `components/arrangrr` (+`chorddet` if that fork
resolves toward "own component") + the **non-TUI half** of `components/hostrt`
(`jsonl.{hpp,cpp}`, `uds_server.{hpp,cpp}`, `gm_program.{hpp,cpp}`,
`note_names.{hpp,cpp}`, `alsa_midi.{hpp,cpp}`; verified clean of `panel_manager.hpp`/
TUI includes by direct grep, §1) + `ALSA::ALSA`.

**What moves out of `apps/tools/cli-arrangrr/main.cpp`**: `AlsaMidi alsa`
(`main.cpp:169`), `UdsServer control` and its whole wiring block
(`main.cpp:184-222`), the tick-timer + `TickAccumulator` clock-drive loop
(`main.cpp:359-366`, `650-661`), and the `poll()` fan-in for ALSA + control fds
(`main.cpp:621`, `627-643`, `672-682`) — this is the literal "socket-serving role
MOVES OUT of `cli-arrangrr`" instruction, traced to exact line ranges.

**What `hostrt::Shell` must split into** (the actual surgery, sized honestly as the
single biggest cost item in this migration, §5 phase 3): a server-side `Shell`
retaining `Engine` ownership + `exec_line`/`push_command` dispatch (today's
`cmd_*`/`seq_*`/`track_*`/`dispatch_*` methods, shell.hpp:278-323), versus a
client-side presentation object retaining `PanelManager`/`PianoViewState`/`UiStyle`/
`StyleChooser`/the kitty-momentary state and the `groove_key`/`arp_panel_key`/
`chords_key`/`piano_key_event`/`chooser_nav_*` methods — fed by parsed JSONL events,
the SAME pattern `gui-sonotron/src/app_state.cpp` already proves works for a GUI. This
is not a mechanical move; it is a real split of one 435-line class along a seam that
has never existed before.

**How `gui-sonotron` and `cli-arrangrr` become pure clients**: `gui-sonotron` needs NO
change (already there, §1). `cli-arrangrr` needs to grow a UDS session
(architecturally identical to `apps/gui-sonotron/src/uds_brain_session.cpp`) sending L1
lines from the REPL/panels and receiving JSONL, replacing its current in-process
`shell.exec_line`/`Engine` calls — while KEEPING its TUI rendering (`console.cpp`,
`kitty_keys.cpp`, `panel_manager.cpp`, the `*_view.cpp` files, `rc_config.cpp` — all
confirmed TUI-only, §1) unchanged in spirit, re-fed from received events instead of
direct engine state.

---

## 5. Phased migration plan

Each phase names: what moves, what breaks, the test-migration strategy, and a green gate.

**Phase 1 — Stand up `components/runtime` + the STAGE port; arrangrr adapted behind
it, single-stage pipeline, NO ABI reshape yet.**
- Moves: `Transport` + `OutScheduler` relocate byte-for-byte from `components/arrangrr`
  to the new `components/runtime`; the `advance_ticks` fire-loop body relocates to a
  `Runtime` class that drives exactly one `Stage` instance (arrangrr, adapted behind
  the new `Stage` interface, engine.hpp's `push_command`/`push_midi_in` kept as-is on
  the arrangrr side for now — command dispatch does NOT need to reshape to make this
  phase work). **Superseded/refined by §14 (Phase-1 review addendum) — read that
  section before executing.**
- Breaks: `components/arrangrr/src/engine_checks.cpp`'s `engine_link_gate` (now must
  instantiate `runtime` + `arrangrr` together, not `Engine` alone) and every
  `test_engine*.cpp` file that calls `advance_ticks`/constructs `Engine` directly —
  these 20 files move to a new `components/runtime/tests/` (the D29 fire-order tests,
  `test_engine_fire_order.cpp`, are now genuinely runtime tests, not arrangrr tests) or
  get rewritten against the new two-object shape. `hostrt::Shell::advance_to` retargets
  from `m_engine.advance_ticks` to `m_runtime.advance_ticks` (single call-site change,
  traced in §1).
- Test-migration strategy: because the wire format (`Command`/`OutEvent`) is untouched
  in this phase, the 18 golden `.golden` files need NOT change — same JSONL bytes, new
  internal ownership. `test_abi_frozen.cpp` stays exactly as-is (nothing in
  `abi.hpp`'s shape changed yet) — the waiver is available, not spent.
- Green gate: all 18 goldens pass byte-identical; `tests/arm-smoke` cross-builds
  `runtime` + `arrangrr` together for `arm-none-eabi` and link-succeeds (the smoke
  test's claim becomes "the runtime kernel + the arranger stage link freestanding
  together", a strictly accurate restatement of today's claim).

**Phase 2 — `apps/sonotron-server` stood up.**
- Moves: `AlsaMidi` ownership, the clock-timer loop, `UdsServer` + its wiring, and the
  non-TUI hostrt files (§4) relocate from `apps/tools/cli-arrangrr/main.cpp` into
  `apps/sonotron-server/main.cpp`. The script/golden `--script` mode (today's
  `run_script()`, `main.cpp:36-68`) moves too — it is pure logic (a `Shell` + a
  `std::getline` loop), so it ports mechanically.
- Breaks: `tests/golden/CMakeLists.txt`'s `$<TARGET_FILE:cli_arrangrr>` must retarget
  to `$<TARGET_FILE:sonotron_server>` (one-line change per test, mechanical).
  `cli-arrangrr` still works exactly as today in this phase (nothing removed from it
  yet) — this phase is purely additive, lowest-risk step of the whole plan.
- Test-migration strategy: goldens re-point their CLI target; byte output unchanged
  (same `Shell`/`Runtime`/`arrangrr` code, different binary). `test_abi_frozen.cpp`
  still untouched.
- Green gate: all 18 goldens pass against `sonotron_server --script`; a manual/CI smoke
  connects a client to `sonotron_server --control PATH` and round-trips one L1 line.

**Phase 3 — Retire the embedded server in `cli-arrangrr`; it becomes a pure client.**
- Moves: `cli-arrangrr/main.cpp` drops `AlsaMidi`/`Shell`(engine-half)/`UdsServer`
  ownership; gains a UDS client session. `hostrt::Shell` SPLITS (§4) into the
  server-side dispatch object (stays in `sonotron-server`) and the client-side
  presentation object (moves with `cli-arrangrr`) — sized as the largest single cost
  in this plan; recommend its own sequenced sub-PRs (parse-event-into-panel-state
  first, behind a flag, before deleting the in-process path), not a single big-bang
  commit.
  # WHAT BREAKS #
  Every `hostrt/tests/test_host.cpp`-style unit test that constructs `Shell` and calls
  BOTH a `cmd_*` method AND a panel method in the same test breaks by construction —
  each such test must be split to target whichever half of the split `Shell` it
  actually exercises.
- Test-migration strategy: `components/hostrt/tests/` inventoried and split
  per-concern (dispatch tests move with the server-side Shell's new home; panel/key
  tests stay with the client-side Shell). Golden tests are UNAFFECTED (already
  re-pointed in phase 2).
- Green gate: goldens still green (unaffected); a manual TUI smoke session against a
  live `sonotron-server` reproduces today's `cli-arrangrr` interactive experience
  (piano/chords/styles/parts/groove/arp panels all functional over the socket).

**Phase 4 — Accompany (`9310`) as the first real multi-stage pipeline; cash the ABI
waiver if the stage-tagging need proves out.**
- Moves: the chord-detect fork (§6) resolves in code (own component vs. promoted
  module); a MIDI-source stage wraps the existing `smf.{hpp,cpp}` reader (§3.6);
  `components/orchestrator` gains its first real multi-stage wiring.
- Breaks (conditionally, only if a stage-id/source-tag is added to `Command`/
  `OutEvent`): EVERY existing golden becomes stale by construction (byte format
  changed) — regenerate mechanically (diff, review, re-bless), do not hand-patch.
  `test_abi_frozen.cpp` at that point should be **retired and replaced wholesale** with
  a new v2 pin-set (bump `kProtocolVersion` to 2 per `abi.hpp`'s own documented escape
  hatch, `abi.hpp:27-29`), not incrementally edited.
- Test-migration strategy: a NEW golden category for Accompany (`tests/golden/
  accompany_*.acmd`) proves the 3-stage pipeline deterministically; existing goldens
  either survive untouched (if no reshape was needed) or are regenerated once
  (if it was).
- Green gate: `9310` ships HOST-ONLY per the roadmap's own scoping (`roadmap-numbered.
  md:286`, "zero new core, zero dependencies"); the MIDI-source stage's file I/O
  confirms it truly needs no new dependency (the SMF reader already exists, §1).

---

## 6. Open forks for the owner

1. **Runtime-component name and placement.** I recommend a NEW freestanding component
   (`components/runtime` above; also plausible: `components/pipeline`, `components/
   sequencrr` if the owner wants to fold this with the already-deferred sequencer-engine
   extraction). Whatever it's called, it MUST be dual-target (§2.5) and MUST NOT be
   `components/orchestrator` (already committed host-only in `project-structure.md:119-
   120`). **This is the load-bearing gap this document surfaces** — nothing on disk
   fills it today, and decision #1 cannot be honestly executed without inventing it.
   **RESOLVED by the owner as `components/runtime` — see §14.**
2. **Chord detector: own component vs. module.** I recommend "own component" (§3.5) on
   the project's own component-vs-module criterion (absent/substituted/multiplied —
   Accompany substitutes it) and because Accompany's pipeline diagram literally draws
   it as a separate box. But this is a real cost (a new boundary, new tests, a new POD
   port) the owner may prefer to defer — exactly the same "not now" logic already
   applied to `sequencrr` in `project-structure.md:105-110`. Flagged, not resolved by me.
3. **`hostrt`'s fate.** Its TUI half (panels/kitty/console/views/rc_config) stays with
   `cli-arrangrr`; its runtime-glue half (jsonl/uds_server/gm_program/note_names/
   alsa_midi) moves to `sonotron-server`. Whether `hostrt` survives as a shared library
   both link, or splits permanently into two named components, is the owner's call —
   this document only shows the split IS clean (verified by grep, §1), not which
   packaging wins.
4. **How far to generalize the STAGE port now vs. later.** §3.6 proposes the narrowest
   port that makes Accompany work (fixed sequential order, no cycles, no runtime
   rewiring) explicitly to avoid importing DAW-graph gravity
   (`product-identity.md`/`workstation-vision.md`'s own refusal of that gravity). A
   richer port (dynamic graph, N-to-N wiring, VST-style) is deferred until a real need
   forces it — the owner should confirm this scoping rather than have it default by my
   silence.
5. **`sequencrr`'s relationship to this milestone.** `project-structure.md:105-110`
   already assigns Corelli the future job of designing "the transport↔engine port and
   the N-instance model" for the per-lane sequencing engine. This milestone's STAGE port
   (§3.6) is architecturally most of that same port. I recommend NOT doing the full
   `Timeline`→`sequencrr` N-instance generalization inside this milestone (scope
   discipline — one extraction at a time), but the owner should explicitly bless
   keeping `Timeline` as a module riding inside the arrangrr stage for now, on the
   project's own placement criterion ("stays a module if arrangrr cannot function
   without it") — noting that the STAGE port's shape (§3.6) already leaves room for
   `sequencrr` to graduate later as "one more stage instance", not a rearchitecture.
6. **`sonotron-server` vs `sonotron-host` naming.** Owner's brief says `-server`;
   `project-structure.md` says `-host` in six places. I default to `-server` per the
   explicit instruction and flag the doc as needing a follow-up edit once this proposal
   is accepted (Palladio's placement call, not mine).

---

## Files read / cited (absolute paths)

- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/design/project-structure.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/design/workstation-vision.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/product-identity.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/design/director-vocabulary.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/DESIGN.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/strategy/roadmap-numbered.md`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/engine.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/src/engine.cpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/abi.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/transport/transport.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/scheduler/out_scheduler.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/routing/router.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/routing/note_tracker.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/arranger/arranger.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/chord/chord_detector.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/chord/chord_sequencer.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/timeline/timeline.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/common/time.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/common/assert.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/common/function_ref.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/include/arrangrr/midi/message.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/tests/test_abi_frozen.cpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/arrangrr/CMakeLists.txt`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/hostrt/shell.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/hostrt/CMakeLists.txt`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/components/hostrt/rc_config.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/tools/cli-arrangrr/main.cpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/tools/cli-arrangrr/CMakeLists.txt`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/gui-sonotron/CMakeLists.txt`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/gui-sonotron/src/uds_brain_session.cpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/tools/arrstyle-converter/src/smf.hpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/tests/golden/CMakeLists.txt`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/tests/golden/run_golden.cmake`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/tests/golden/hello_chord.acmd`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/tests/arm-smoke/main.cpp`
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/docs/design/runtime-extraction-phase1-move-plan.md`

---

## 14. Phase-1 review addendum (Corelli, 2026-07-12)

Triggered by: owner acceptance of this milestone's design + Palladio's
`docs/design/runtime-extraction-phase1-move-plan.md` (physical move-plan for Phase 1).
This addendum reviews Decision A (the `components/common` cycle-break) and resolves the
two logical seams Palladio flagged and handed to me (`flush()`/`NoteTracker`,
`push_command`/`push_midi_in`'s scheduler access) — read-only on product code; the only
write is this section.

### 14.1 Decision A — the dependency graph, corrected

**The owner's one-line description of `components/common` ("Tick, the time types, and
the 3 bar constants") is NECESSARY but NOT SUFFICIENT to break the cycle.** I traced the
actual `#include` lists of the two files Palladio moves byte-for-byte and found two more
live reverse-dependencies that the stated scope does not cover:

- **`components/arrangrr/include/arrangrr/scheduler/out_scheduler.hpp:5-7`** includes
  `arrangrr/common/assert.hpp` (for `ARR_ASSERT`) **and** `arrangrr/midi/message.hpp`
  (for `MidiMessage`, used concretely: `ScheduledEvent::msg`,
  `classify(const MidiMessage&)`, `cancel_note_off`'s `e.msg.type()`/`e.msg.d1` checks,
  `out_scheduler.hpp:25-45,105-123`). Neither of these is "Tick" or "a bar constant" —
  if they stay under `components/arrangrr/include/arrangrr/`, `components/runtime`
  still needs `target_link_libraries(runtime PUBLIC arrangrr)` for these two headers
  alone, **the exact cycle re-forming one file later**, because `arrangrr` also needs
  `runtime/stage.hpp` (Palladio §3: "the one sanctioned new include arrangrr-side code
  takes on `runtime/`").
- **The `Stage`/`EventSink` port itself** (§3.6 above, and Palladio §3's "`Stage`
  abstract class exactly as Corelli's §3.6 illustrative shape") declares a method
  taking an `EventSink = FunctionRef<void(const OutEvent&)>`. If `runtime/stage.hpp`
  names `arrangrr::OutEvent` concretely to declare that signature, `runtime` needs
  `arrangrr/abi.hpp` too — a THIRD reverse edge, and one I will NOT resolve by moving
  `OutEvent`/`abi.hpp` into `common` (that would misplace the whole ABI-freeze surface,
  which is genuinely arrangrr's own vocabulary — D26/`0700`, `kChord`/`kSection`/
  `kChordFollowed` are stage-specific semantics, not a base primitive). See §14.2 for
  the fix (a template/concept `Stage`, not a virtual class naming `OutEvent`).

**Corrected `components/common` scope** — verified dependency-free by direct read of
each file's own `#include` block (all four are self-contained beyond `<cstdint>`/
`<memory>`/`<type_traits>`, zero further arrangrr-internal includes):

| Promote to `components/common` | Why | Verified from |
|---|---|---|
| `common/time.hpp` content: `Tick`, `TickOffset`, `BpmX100`, `TickAccumulator`, `kPpqn`, `kGridPpqn`, `kMidiClockDivider`, PLUS the 3 hoisted bar constants `kBeatsPerBar`/`kTicksPerBeat`/`kTicksPerBar` (Palladio's own §0 finding) | `transport.hpp`'s only include; `arranger.hpp`/`chord_sequence.hpp`'s only reason to include `transport.hpp` today | `common/time.hpp:1-61` (current), `transport.hpp:5,22-24` |
| `common/assert.hpp` (`ARR_ASSERT`) | `out_scheduler.hpp`'s 2nd include; zero-cost, zero further deps | `assert.hpp:1-15` (whole file, no includes at all) |
| `common/midi/message.hpp` (`MidiMessage`, the `midi::` namespace) | `out_scheduler.hpp`'s 3rd include, used concretely (not just transitively) | `out_scheduler.hpp:7,25-45,105-123`; `message.hpp` self-contained, only `<cstdint>` |

**`function_ref.hpp` (`FunctionRef<Sig>`) does NOT need to move**, conditional on
§14.2's resolution (a template/concept `Stage`, not a virtual class) — see below. If the
owner or Palladio instead prefers to keep `Stage` a virtual abstract class (my own §3.6
sketch, explicitly marked "illustrative… NOT frozen"), then `function_ref.hpp` MUST also
promote to `common` for the identical reason as the other three (it too is
self-contained, `function_ref.hpp:1-4`, only `<memory>`/`<type_traits>`) — flagged as a
consequence of that choice, not decided by me.

**Corrected dependency graph** (confirming and refining the coordinator's read):

```
components/common      (NEW, base, dual-target, header-only/INTERFACE)
  Tick/TickOffset/BpmX100/TickAccumulator/kPpqn/kGridPpqn/kMidiClockDivider/
  kBeatsPerBar/kTicksPerBeat/kTicksPerBar   (time.hpp)
  ARR_ASSERT                                (assert.hpp)
  MidiMessage + midi::*                     (midi/message.hpp)
      ▲                    ▲
      │ PUBLIC             │ PUBLIC
components/runtime    components/arrangrr
  Transport (moved)      Arranger/ChordEngine/ChordSequencer/ArpeggiatorEngine/
  OutScheduler (moved)   Timeline/Router/NoteTracker/ChordDetector (unchanged) +
  Stage<…> / Runtime<…>  the Stage-adapter (implements runtime's Stage contract)
      ▲
      │ PUBLIC (the ONE sanctioned arrangrr→runtime edge, for the Stage port only)
      └──────────────────────┘
components/arrangrr  →  components/runtime   (for runtime/stage.hpp ONLY)
components/arrangrr  →  components/common    (for Tick/MidiMessage/ARR_ASSERT — arrangrr's
                                               OWN code already uses these pervasively;
                                               this is not a new logical need, only a new
                                               physical include path once the headers move)
components/runtime   →  components/common    (for the same reason, transport.hpp/
                                               out_scheduler.hpp's own bodies)
components/runtime   ✕  components/arrangrr  (Palladio's flagged edge — DELETE. This
                                               was the cycle; it must not be added.)
```

**Confirmation of the coordinator's read**: `common (base) ← runtime (kernel) ← arrangrr
(stage)` is the right shape, **with one correction**: it is not a strict linear chain —
BOTH `runtime` and `arrangrr` depend directly on `common` (not `arrangrr` transitively
through `runtime`), and `arrangrr → runtime` is real but narrow (stage.hpp only, not
transport.hpp/out_scheduler.hpp, which arrangrr's Stage-adapter never touches directly —
it only ever reaches the scheduler through the reference `Runtime` injects, §14.3). No
edge points from `runtime` or `common` back toward `arrangrr`. This is confirmed acyclic.

### 14.2 A related correction: `Stage` should be a template/concept, not a virtual class

Beyond the cycle itself, naming `arrangrr::OutEvent` inside `runtime/stage.hpp` (needed
if `Stage` is a virtual abstract class with a concrete `EventSink` parameter type) is
**avoidable**, and avoiding it is preferable on the project's own doctrine:
`docs/DESIGN.md:577` — *"Compile-time polymorphism (templates/CRTP) in the hot path;
`virtual` only at the HAL boundaries."* A per-tick stage dispatch is squarely the hot
path, not a HAL boundary. I recommend `Runtime` be a template (`Runtime<StageT>` for
Phase 1's single, statically-known stage) and `Stage` be expressed as a C++20 `concept`
(`docs/DESIGN.md:577` also names "`concepts` for contracts" as the doctrine's own
preferred contract mechanism) rather than a base class with a `virtual on_tick`:

```cpp
// components/runtime/include/runtime/stage.hpp — NO arrangrr include, NO OutEvent named.
#include "common/time.hpp"     // Tick
#include "runtime/transport/transport.hpp"  // TransportState, Position (intra-component)

namespace runtime {

struct StageContext {
  Tick now;                    // stream tick, always advances (was Engine::m_now)
  Tick transport_tick;         // transport's own musical position; meaningless if !playing
  bool transport_playing;
  bool is_clock_pulse;         // NEW — was Transport::is_midi_clock_tick(), computed here
                                // once by Runtime so the Stage never reaches into Transport
  TransportState transport_state;  // NEW — needed for OutEvent::transport(...) (§14.3, B3)
  Position transport_position;     // NEW — needed for OutEvent::beat(...) (emit_beat)
};

// A concept, not a base class: StageT must be tick-callable and flush-able with
// SOME sink type — the sink's concrete type (arrangrr::Engine::EventSink today) is
// never named here, so this header stays fully arrangrr-agnostic.
template <typename StageT, typename SinkT>
concept StageLike = requires(StageT& s, const StageContext& ctx, SinkT sink) {
  { s.on_tick(ctx, sink) } -> std::same_as<void>;
  { s.flush(sink) } -> std::same_as<void>;
};

}  // namespace runtime
```

This makes `Runtime<StageT>::advance_ticks(n, SinkT sink)` a template member,
instantiated only where `StageT` = arrangrr's own Stage-adapter type and `SinkT`
deduces to `arrangrr::Engine::EventSink` at the actual call site inside `hostrt` (which
already includes `arrangrr/abi.hpp` for its own reasons) — `runtime/stage.hpp` and
`runtime/runtime.hpp` compile without ever naming `OutEvent`, closing the third reverse
edge without touching `abi.hpp` or promoting `function_ref.hpp`. It also removes a
vtable indirection from the per-tick hot path, consistent with doctrine. **This corrects
(does not merely restate) my own §3.6 illustrative sketch, which I had explicitly marked
"NOT frozen" — and it corrects Palladio's §3, which followed that sketch literally.** If
the owner prefers the virtual-class shape anyway (e.g. anticipating a FUTURE world where
stages really are runtime-selected, which I do not currently recommend — see §6 item 4's
"fixed, no dynamic rewiring" stance), that is a live option, but it must then also
promote `function_ref.hpp` to `common` (§14.1) and accept the vtable-on-hot-path
trade-off against the doctrine cited above. Flagged as a NEEDS-DECISION fork, not
decided unilaterally: **fork 7 in §14.5.**

### 14.3 Seam B1 — `flush()` splits `OutScheduler` (runtime) and `NoteTracker` (arrangrr)

**Resolution: it does not split. `flush()` stays exactly where it is (a private method
of the arrangrr Stage-adapter), textually unchanged, because the Stage-adapter holds a
*reference* to Runtime's `OutScheduler`, not a copy.**

`Runtime<StageT>` owns the ONE `OutScheduler<N> m_scheduler` instance (constructed once,
sized `N` = `kSchedulerCapacity`, moved as-is per Palladio). It constructs its `StageT`
member by **injecting that scheduler by reference**:

```cpp
// components/runtime/include/runtime/runtime.hpp (illustrative)
template <typename StageT>
class Runtime {
 public:
  template <typename... StageArgs>
  explicit Runtime(StageArgs&&... args) : m_stage(m_scheduler, std::forward<StageArgs>(args)...) {}
  // ...
 private:
  runtime::Transport m_transport;
  OutScheduler<kSchedulerCapacity> m_scheduler;
  StageT m_stage;   // constructed AFTER m_scheduler (declaration order = init order)
  Tick m_now = 0;
};
```

```cpp
// components/arrangrr — the reduced Engine/Stage-adapter (illustrative diff, not the
// full file): m_scheduler changes from a VALUE member to a REFERENCE member; every
// call site that used it (schedule_or_warn, schedule_pattern, cancel_note_off-driven
// retrigger, flush's pop_due) is BYTE-IDENTICAL in body — only the declaration changes.
class Engine {  // (or its renamed successor)
 public:
  explicit Engine(runtime::OutScheduler<kSchedulerCapacity>& scheduler) : m_scheduler(scheduler) {}
  // ...
 private:
  void flush(EventSink sink) {                              // UNCHANGED BODY
    m_scheduler.pop_due(m_now, [&](const ScheduledEvent& ev) {
      m_tracker.observe(ev.port, ev.msg);                   // NoteTracker STAYS here
      sink(OutEvent::midi(ev.port, ev.msg, ev.tick));
    });
  }
  runtime::OutScheduler<kSchedulerCapacity>& m_scheduler;    // was: OutScheduler<..> m_scheduler;
  NoteTracker m_tracker;                                     // UNCHANGED, stays arrangrr-side
};
```

**Why this is correct, not a workaround**: "ownership" (decision #1's "no scheduler
ownership") means *who constructs/sizes/frees the queue and who is the single source of
truth for it across N future stages* — that is unambiguously `Runtime` (one instance,
constructed once, handed to whichever stages need to push into it). It does **not** mean
"the arranger stage may never call a method on it" — exactly as `Engine::EventSink`
today is a *reference* the caller owns, not a value `Engine` owns, and nobody reads that
as `Engine` "owning" the sink. Because it is the literal same `OutScheduler` instance,
same heap array, same `m_seq` monotonic counter, same `before()` tie-break — the D29 pop
order and every `seq_no` assigned during `fire_timeline`→`fire_chord_seq`→`fire_arranger`
→`fire_arp` (unchanged calling order, per Palladio's relocated loop body) are **bit-for-
bit identical** to today's output. Byte-identical goldens hold.

**Caveat for later phases (flagged, not blocking Phase 1)**: this reference-injection
pattern is a **Phase-1-only simplification**, legitimate because there is exactly one
stage. Once a second real stage exists (Phase 4, Accompany), letting every stage hold a
raw `OutScheduler&` would let a chord-detect or MIDI-source stage schedule arbitrary
future MIDI with arrangrr-specific retrigger semantics (`cancel_note_off`) that are not
theirs to invoke — breaking stage encapsulation/name-blindness (D43). Before Phase 4,
this should generalize into a narrow `SchedulerPort` interface (`schedule(port,tick,msg)`
/ `cancel_note_off(...)` only, no direct heap access) that `Runtime` implements and hands
uniformly to every stage — noted here so Nazzareno's Phase-1 code does not calcify the
raw-reference shape as if it were final.

### 14.4 Seam B2 — `push_command`/`push_midi_in`'s direct `m_scheduler.schedule()` calls

**Resolution: already covered by §14.3's reference-injection — these compile and behave
unchanged**, because `schedule_or_warn`/`schedule_pattern` (engine.hpp:272-276, 349-357)
call `m_scheduler.schedule(...)`, and `m_scheduler` is now a reference to the SAME
`Runtime`-owned instance. Forward-scheduled events (`schedule_pattern`'s
`on_tick = m_now + delay`, used by ratchets/ties/the arp's gate-off, and
`fire_clock_pulse`'s clock-byte scheduling) push into that instance exactly as before;
`cancel_note_off`'s retrigger-tombstoning (used by `schedule_pattern`, out_scheduler.hpp:
105-123) queries and mutates the same heap it always did. **No change to any of these
method bodies is required** beyond the one declaration-site edit in §14.3.

**One consequence this seam analysis surfaced beyond the two named ones (flagging it as
B3, not silently skipping it — it is load-bearing for Phase 1 to even compile):**
`push_command`'s dispatch to `cmd_transport` (`engine.cpp:9-14,74-122`) directly
mutates `m_transport` (`m_transport.set_bpm(...)`, `.start()`, `.stop()`, `.resume()`,
reads `.state()`) — a member that **no longer exists** on the arrangrr Stage once
`Transport` moves to `Runtime`. This is not optional to resolve; `cmd_transport` cannot
compile unchanged. **Resolution, mirroring the existing `Arranger::on_transport_start()`
precedent already in the codebase (`engine.cpp:90`)**:

- `Runtime<StageT>::push_command(const Command& cmd, SinkT sink)` becomes the new
  top-level entry point `hostrt::Shell` calls (replacing `m_engine.push_command`). It
  inspects `cmd.param`: for the four transport params
  (`kTransportTempo`/`Start`/`Stop`/`Continue`) it does the `m_transport.*()` mutation
  itself, then calls a **new, narrow Stage hook** — `m_stage.on_transport_start(ctx,
  sink)` / `on_transport_stop(ctx, sink)` — for the tick-0 side effects that are
  intrinsically arranger/harmony business (today's `cmd_transport`'s
  `emit_realtime(kStart)`/`emit_realtime(kClock)`/`m_seq.play(0)` rebase/
  `m_arranger.on_transport_start()`/`m_chords.establish_default()`+`reset_pending()`/
  `fire_timeline(0,sink)`/`fire_chord_seq(0,sink)`/`fire_arranger(0,sink)`/`flush(sink)`
  — moved **verbatim** into `on_transport_start`'s body, minus the `m_transport.start()`
  line itself, which `Runtime` already did). Finally `Runtime` emits
  `sink(OutEvent::transport(ctx.transport_state, ctx.now))` — this is why §14.2's
  `StageContext` carries `transport_state`/`transport_position`: so neither `Runtime`
  nor the Stage needs the other's private state to construct these events, only the
  values `Runtime` already injects every call.
- For every OTHER param (`kRouteAdd`, `kChordPlay`, `kSeqNew`, `kTrackNew`, `kStyleLoad`,
  `kProgram`, `kArp`, …), `Runtime::push_command` delegates **verbatim** to
  `m_stage.push_command(cmd, sink)` — arrangrr's own `push_command` switch, UNCHANGED,
  minus only the `case Param::kTransportTempo/Start/Stop/Continue: cmd_transport(...)`
  branch and the `cmd_transport` method itself (both relocate to `Runtime`). This is the
  precise, honest content of "`push_command` kept as-is": true for 100% of Params except
  the four whose owning object physically moved — an expected, not a silent, consequence
  of decision #1.

### 14.5 Verdict on Palladio's move-plan

**APPROVED WITH REQUIRED CORRECTIONS.** The physical mechanics (file moves, CMake
scaffolding order, test-relocation table, coverage-gate extension, arm-smoke retargeting)
are sound and I have no changes to §2 (`git mv` list), §4 (test-relocation table), §6
(`engine_checks.cpp` → `link_gate.cpp` rationale), or §10 (coverage-gate scope) — those
stand as written. The following must be corrected **before Nazzareno executes**:

1. **§1's flagged CMake dependency is WRONG and must be replaced.** Palladio's plan adds
   `target_link_libraries(runtime PUBLIC arrangrr)`. This edge must be **deleted**, not
   added — it is precisely the cycle-forming edge Decision A exists to kill. Replace
   with `target_link_libraries(runtime PUBLIC common)` (§14.1).
2. **`components/common` must be scaffolded FIRST, with the wider scope in §14.1** — not
   just `time.hpp`'s constants, but also `assert.hpp` and `midi/message.hpp` promoted
   wholesale, each with every consumer across `components/arrangrr/` (a materially
   larger set of files than Palladio's §0 currently lists — `assert.hpp`/`message.hpp`
   are included pervasively: parser, router, note_tracker, chord engine, arranger,
   scheduler, timeline, plus every test file that constructs a `MidiMessage`) repointed
   from `arrangrr/common/…`/`arrangrr/midi/message.hpp` to their new `common/…` paths.
   **I ask Palladio for a companion revision enumerating this wider git-mv/repoint
   list** — that inventory is his lane, not mine; I have identified WHAT must move and
   WHY, not produced the exhaustive file list.
3. **`components/arrangrr/CMakeLists.txt` gains TWO new link edges, not zero.** Today's
   "zero external `target_link_libraries`" property (correctly prized by Palladio's plan
   in its unchanged form) **cannot survive this milestone** — `arrangrr` now needs
   `target_link_libraries(arrangrr PUBLIC common runtime)` (`common` because arrangrr's
   own pervasive code uses the promoted types; `runtime`, narrowly, only for
   `runtime/stage.hpp`). This is an accepted, sanctioned cost of decision #1, not an
   oversight — but Palladio's §5 CMake deltas must be updated to show it (currently §5
   only edits `hostrt`'s and `tests/arm-smoke`'s CMakeLists, and explicitly says
   "nothing else in this file [`arrangrr`'s] changes" — that line is now incorrect).
4. **`Stage`'s shape should be corrected per §14.2** (template/concept, not virtual
   class) before Nazzareno writes `stage.hpp`'s body — Palladio's §3 followed my
   original illustrative sketch, which I had explicitly marked non-frozen; this
   addendum supersedes it.
5. **B3 (§14.4) must be folded into Palladio's plan as an explicit, load-bearing item.**
   `cmd_transport`'s split (the four transport `Param`s move to `Runtime`; the rest of
   `push_command` stays) is not a file-placement question but it IS a compile-blocking
   consequence of the file moves Palladio already scheduled — his own §11 green-gate
   checklist item "`git grep -n 'm_engine\.transport\|m_engine\.advance_ticks'` returns
   empty in `components/hostrt/`" should be extended with an analogous check that no
   `cmd_transport`/`m_transport.*` reference survives inside the arrangrr Stage-adapter
   itself.
6. **Namespace fork (Palladio §8)**: no objection to "Option A, keep `namespace
   arrangrr` for Phase 1" — but note `components/common`'s NEW types (`Tick`,
   `MidiMessage`, `ARR_ASSERT`) need their OWN namespace decision too, not previously
   posed. I recommend `namespace arrangrr` for these as well (same minimal-churn
   argument — every one of dozens of call sites across the whole tree already spells
   `arrangrr::Tick`/`arrangrr::MidiMessage` and this way none of them need a qualifier
   edit), explicitly deferred to the SAME later "one dedicated rename pass" Palladio
   already flagged for `Transport`/`OutScheduler`.
7. **Sub-folder fork (Palladio §9)**: no objection; extend the same "flat for Phase 1"
   lean to `components/common/include/common/` (i.e. `common/time.hpp`,
   `common/assert.hpp`, `common/midi/message.hpp` — the last keeps a `midi/` subfolder
   since `MidiMessage` is conceptually distinct from the bare time/assert primitives and
   `arrangrr`'s own tree already uses `midi/message.hpp` as a path, minimizing the
   include-path diff at every one of its many call sites).
8. **Palladio's own item 4 (`runtime`'s `src/`, header-only vs `INTERFACE`)**: given
   §14.2's template/concept `Stage`, `Runtime<StageT>` also stays fully header-only —
   I recommend `add_library(runtime INTERFACE)` (matching `components/common`'s own
   INTERFACE nature, §14.1), sidestepping the "invalid empty STATIC library" problem
   Palladio flagged, rather than inventing a ceremony `.cpp`. A Nazzareno-level call to
   confirm, but the template shape removes the forcing reason for a `.cpp` to exist.

**New fork surfaced by this addendum (fork 7, additive to §6):** `Stage` as a
template/concept (my recommendation, §14.2) vs. a virtual abstract class (Palladio's
plan as literally written, following my own non-frozen sketch) — the owner should
confirm before Nazzareno writes `stage.hpp`'s body, since the two shapes have different
`components/common` scopes (concept: `function_ref.hpp` stays in `arrangrr`; virtual
class: `function_ref.hpp` must ALSO promote to `common`) and different CMake library
kinds (`INTERFACE` vs `STATIC`+ceremony-`.cpp`).

### 14.6 Phase-1 green gate — confirmed, with one addition

The green gate stated in §5's "Phase 1" entry **still holds**, unchanged in substance:
18 goldens byte-identical, `test_abi_frozen.cpp` compiles and passes unedited, arm-smoke
cross-builds freestanding and link-succeeds. Restated precisely against the corrected
three-component shape:

- [ ] All 18 `tests/golden/*.golden` files diff byte-identical against their `.acmd`
      scripts through `cli_arrangrr --script` (target name unchanged in Phase 1) — this
      is the direct proof that §14.3/§14.4's reference-injection resolution preserves
      D29 emission order and every `seq_no` tie-break exactly.
- [ ] `components/arrangrr/tests/test_abi_frozen.cpp` compiles and passes **unedited** —
      confirms `abi.hpp` truly did not reshape (the waiver stays unspent in Phase 1, as
      recommended in §3).
- [ ] `tests/arm-smoke` (via the relocated `link_gate.cpp`) cross-builds
      **`components/common` + `components/runtime` + `components/arrangrr`** together
      for `arm-none-eabi` and link-succeeds — the three-component chain, not the
      two-component one the coordinator's message names; `common` must be in the
      freestanding cross-build too (it is dual-target by construction, header-only,
      zero risk, but must not be silently skipped from the CI matrix).
- [ ] No `target_link_libraries` cycle exists in the generated CMake graph — confirmable
      mechanically (a clean `cmake --build` configure step is itself the test: a real
      cycle fails configure, it does not silently link).
- [ ] `scripts/coverage.sh`'s CORE gate (Palladio §10) extends to
      `components/arrangrr/ + components/runtime/` as already planned; I add
      `components/common/` should be **excluded** from the enforced ≥80% branch gate
      (it is a pure data/constant/macro layer with no branches to cover — the same
      reasoning that already excludes `components/arrangrr/tests/` and `tests/` from
      the numerator, not the same reasoning as "narrow the gate," just "don't demand
      branch coverage of a header with no branches").

**Verdict**: Phase-1 green gate holds, PROVIDED the corrections in §14.5 land first.

---

## 15. Phase-2 in-process review (Corelli, 2026-07-12)

Triggered by: `docs/design/sonotron-server-phase2-brief.md`, the owner's mid-milestone
reshape of Phase 2 (library-in-one-binary/two-threads/in-process rings, superseding the
separate-binary-over-UDS reading of §4/§5 above for Phase 2 only). This section reviews
that brief against the stable `Command`/`OutEvent` ABI (`abi.hpp:167-296`), against
`apps/gui-sonotron/src/app_state.cpp`'s `BrainEvent` decode, and against the Phase-1
tree as it exists right now (`components/common`, `components/runtime/{runtime,stage}.hpp`
already landed, per §14). Read-only on product code; the only write is this section. The
topology fork itself (library not binary; POD-on-ring, JSONL inter-process only; naming)
is **owner-closed and not reopened below** — every verdict here either holds it or flags a
collision with a DIFFERENT, separately-locked decision.

### 15.1 Topology: lib-not-binary + Option A — HOLDS, with one load-bearing collision to flag

**Option A (compose `Runtime<StageT,N>` + the arrangrr stage directly, orchestrator
deferred to Phase 4) does not need to be reopened by threading.** `runtime/runtime.hpp`
as landed (`components/runtime/include/runtime/runtime.hpp:43-89`) is an ordinary
synchronous template — `advance_ticks`/`push_command` are plain method calls with no
internal synchronization, driven by whatever loop calls them. Today that loop is
`cli-arrangrr`'s single-threaded tick timer; tomorrow it is Phase 2's dedicated engine
thread. **Which thread calls `Runtime::advance_ticks` is a driving-loop concern, not a
multi-stage-composition concern** — the orchestrator's job (per `project-structure.md`
and this doc's own §3.5) is composing N *stage* instances into a pipeline, which is
orthogonal to which OS thread owns the single `Runtime` instance. Threading in-process
does **not** manufacture a need for the orchestrator abstraction ahead of schedule. Verdict:
**HOLD.**

**The collision to flag (not a reopening of the topology fork, but a genuine hit on a
DIFFERENT locked decision):** the brief's "GUI binary links the server library and runs
the engine on a dedicated thread" (brief lines 26-28) means the GUI **binary** now links
`components/runtime` + `components/arrangrr` + the non-TUI `hostrt` files directly. This
is a literal, unavoidable collision with the **currently locked, tested, and cited** D38
GUI-purity rule:
- `docs/design/gui-contract-map.md:7`: *"The GUI is a **separate process, pure client**. It
  never links or `#include`s the core."*
- `docs/design/gui-contract-map.md:61-63` (Rule 1): *"the GUI target links **neither
  `arrangrr` nor `hostrt`**, and `#include`s **zero** core headers."*
- `docs/design/gui-contract-map.md:64-70` (Rule 2): *"the GUI carries its own wire layer
  … **own** name tables … **Never** `static_cast<ChordQuality>`/`static_cast<SectionType>`
  a core enum."*
- Enforced today, not aspirational: `apps/gui-sonotron/CMakeLists.txt:1-11` states and
  builds against "links neither arrangrr_core nor hostrt … zero core headers", and
  `apps/gui-sonotron/src/brain_event.hpp:7-13` restates the same invariant in the source
  itself ("no arrangrr core headers … The GUI is a pure client … it never links the core
  and never sees a core enum").

The brief supersedes §4/§5's **process topology** ("separate binary over UDS" → "one
binary, two threads"). It does **not**, anywhere in its text, say it also supersedes D38's
**GUI-purity** invariant — and taken literally it cannot be honoured alongside it: you
cannot construct/drive an `arrangrr` Stage on a thread inside a binary that also
"never links `#include`s zero core headers." These are two different decisions
(process-count vs. dependency-purity) that happen to have been co-located in the same
GUI target until now; the brief silently collapses both when only the first was
owner-directed. **This needs an explicit owner call, not a silent default** — see §15.7 for
the concrete resolution that satisfies both without reopening either fork.

### 15.2 The ring contract — the brief contradicts itself, and that is the actual defect to fix before Nazzareno touches it

The brief states, in the same paragraph: *"the engine drain is **wait-free** — it never
blocks and never allocates"* (line 42-43) and, three lines later, *"a simple
`std::mutex`-guarded bounded queue is acceptable for the first cut"* (line 59-60). **These
two sentences cannot both be true.** A mutex-guarded queue is not wait-free by
construction: if the GUI-thread producer is preempted by the OS scheduler while inside the
locked critical section (a real, not hypothetical, event under GPU/vsync-thread load —
exactly the stall condition the brief is trying to protect the engine from), the
engine-thread consumer's lock acquisition blocks for an OS-scheduling-defined, unbounded
duration. That is priority inversion, and it is precisely the failure mode "wait-free"
promises does not happen. **This is the load-bearing open decision the brief itself flags
(line 91-94) — and it is not resolved by picking either horn silently; it must be picked
before Nazzareno writes the ring.**

Concretely worse than the brief's prose suggests: the engine's `EventSink` is called once
**per produced `OutEvent`**, not once per tick (`arrangrr::Engine::EventSink =
FunctionRef<void(const OutEvent&)>`, `engine.hpp:58`, called from `flush()`/`fire_*` for
every scheduled note, chord, beat, transport, warn event due that tick). A single bar
boundary can synchronously emit a burst of several `OutEvent`s from one `advance_ticks`
call. Under a mutex-first design that is a lock/unlock pair **per event in the burst**, not
once per pump — multiplying the inversion-exposure surface, not a single small window.

**Traps to constrain, whichever the owner picks:**
- **Full/empty disambiguation**: with only `head`/`tail` indices, `head == tail` is
  ambiguous between empty and full unless the ring reserves one slot (capacity `N`, usable
  `N-1`) or carries an explicit atomic count. Nazzareno's implementation must pick one
  explicitly, not discover it works "by luck" for the current 4/8-slot test sizes.
- **False sharing**: `head` (engine-owned) and `tail` (GUI-owned) must sit on separate
  cache lines (padding to the platform's cache-line size) — otherwise every push/pop
  ping-pongs the same cache line between cores, degrading exactly the "never stalls" claim
  even in a correct lock-free implementation.
- **Memory ordering**: producer publishes the slot's payload, *then* releases the updated
  index (`memory_order_release`); consumer acquires the index before reading the payload
  (`memory_order_acquire`). This is the standard SPSC pattern (no CAS loop needed — single
  writer, single reader — genuinely the easiest lock-free shape to get right, not the
  general lock-free-queue hard problem).
- **Asymmetric overflow policy, not one rule for both rings**: the brief states "a full
  ring drops / applies backpressure" as if symmetric. It should not be. The existing,
  precedented policy (which the brief itself invokes — "same logical shape as today's D38
  socket") is **asymmetric**: engine→GUI (`OutEvent`) is best-effort/lossy today (the UDS
  broadcast already drops for a slow client — `gui-contract-map.md:23-24`, "best-effort; a
  slow client drops events rather than stalling MIDI"), so dropping under backpressure on
  that ring is a faithful continuation, not a new behavior. GUI→engine (`Command`) has **no
  drop precedent today** — the existing UDS path is a reliable byte stream (the kernel
  socket buffer absorbs backpressure; `Shell::exec_line` processes every line). Silently
  dropping a user-authored command (a keypress, a chord-play) on a full ring would be a
  **regression**, not parity. Recommendation: size the `Command` ring generously (human
  input rate is orders of magnitude below tick rate; even 512-1024 slots of a 20 B struct
  is ~10-20 KB, trivial) so overflow is a near-unreachable pathology, and treat an actual
  overflow as a `WarnCode`-class event (mirroring `kSchedulerFull`'s own precedent,
  `abi.hpp:179`), not a silent drop.

**My recommendation on mutex-vs-lock-free**: skip the mutex-first phase. SPSC lock-free (no
mutex, atomic indices with acquire/release, one reserved empty slot) is a small,
well-understood, independently-unit-testable primitive — not the general lock-free-anything
problem the "harden later" framing implies. Starting mutex-guarded resolves the
contradiction above only by *quietly downgrading* the "wait-free" claim to false for
Phase 2's entire first landing, and "harden later" migrations of this exact kind routinely
never happen once goldens are green. This is **SHIPPABLE, HOST-ONLY, no new dependency**
(plain `<atomic>`), so it is not blocked on anything the owner needs to flag/cost — it is a
straight technical call I recommend making now rather than deferring.

### 15.3 Double decode — real drift risk, concretely located, not hypothetical

**Yes, a real risk, and it is already visible in the shape of the existing code, not a
speculative future problem.** `to_jsonl(const OutEvent&, bool prefer_flats)`
(`components/hostrt/jsonl.cpp:192-256`) is the **only** current OutEvent→label renderer,
and it is backed by a set of `namespace {}`-private helpers in that same file:
`quality_suffix` (86-112), `roman_degree` (114-133), `note_label`/`followed_label`
(68-76, 170-176), `section_name` (78-84), `warn_name` (15-26, with a `static_assert` tying
its table size to `kWarnCodeCount` — a real safety net), `transport_name` (28-37),
`producer_name` (178-188). Today the GUI's `parse_brain_event`
(`apps/gui-sonotron/src/brain_event.cpp:284-343`) never re-derives any of these — it only
**extracts already-rendered strings** the host computed (`ev.chord_out =
obj.get_string("out")`), which is exactly what `gui-contract-map.md`'s Rule 2 mandates
("own name tables … never `static_cast` a core enum") and why there is no drift today.

The brief's `brain_event_from_outevent(OutEvent) → BrainEvent` (line 65-68) breaks this
invariant by construction: to populate `BrainEvent`'s string fields directly from a raw
`OutEvent`, it must **recompute** the same label set `to_jsonl`'s private helpers already
compute — degree Roman numerals, chord-quality suffixes, note/pitch names, section/warn/
transport/producer names. Written as a second, independent implementation (which is what
"in-process, more fundamental decode" implies if nothing is refactored), this is precisely
a drift risk: two enum→string tables that must be kept in lockstep by hand, one of which
(`warn_name`) currently enjoys a compile-time `static_assert` guarding it against a missed
append and the other of which would not, unless duplicated too.

**One more concrete, currently-invisible gap in the brief's one-argument signature**:
`to_jsonl`/`to_human` take a **second** parameter, `prefer_flats`, which is not part of the
ABI at all — it is derived host-side state (`Shell::m_prefer_flats`, set by
`key_prefers_flats(root, mode)` in `components/hostrt/shell_music_commands.cpp:106,599`
whenever the key changes) threaded in from whichever `Shell`/dispatch object is rendering.
`brain_event_from_outevent(OutEvent) → BrainEvent`, as literally specified with one
argument, has nowhere to get this from. Whatever holds the engine-thread's dispatch state
post-split must also carry (and pass) this per-connection-equivalent rendering preference,
exactly as `to_jsonl` does today — an implicit second input the brief's contract text
omits.

**Recommendation (answers the owner's own question 3 directly, but corrects the proposed
mechanism):** the fix is not "JSONL confluisces into the OutEvent→BrainEvent path" (that
direction is strictly harder and lossy — `to_jsonl` already destroys the raw enum values
into rendered text like `"Cmaj7"`; parsing that back into `(root_pc, quality)` would need a
new note-name parser that does not exist and is not needed). The fix is the **other**
direction: extract the *currently-private* label-computation helpers out of
`jsonl.cpp`'s anonymous namespace into a small, exported, host-only module (a
`components/hostrt/event_format.hpp` or similar — Palladio's placement call, not mine),
and have **both** `to_jsonl` (unchanged, still serving the socket/goldens path) **and** a
new `to_brain_event(const OutEvent&, bool prefer_flats)` call the **same** shared helpers —
one emitting JSONL text, the other populating `BrainEvent`'s fields directly. This
collapses "two decode paths" into "one label-computation core, two thin serializers,"
closing the drift risk instead of accepting it as a cost of "more fundamental."

### 15.4 "Ring and socket are interchangeable, same contract" — true for the POD, not (yet) for the full pipeline

The raw-struct claim is true and cheap to keep true: `Command`/`OutEvent` are the same 20 B/
16 B PODs on both paths, and nothing in Phase 2 reshapes `abi.hpp` (`test_abi_frozen.cpp`
stays unedited, confirmed no ABI drift). But "interchangeable" oversells the **maintenance**
picture unless §15.3's extraction lands: as specified, the brief creates a second labeling
implementation living only in-process, alongside the first living only in `jsonl.cpp` —
that is a hidden seam, not parity, exactly the "manutenzione nascosta" the owner's question
4 suspected. With §15.3's shared-helper extraction, the claim becomes durably true: same
POD, same label computation, two thin front ends (text serializer vs. struct populator).
Without it, the claim is true only until the first `WarnCode`/`SectionType`/quality
enumerator is appended and only one of the two tables is updated — a silent, hard-to-catch
divergence (the golden tests would not catch it: they exercise the socket/JSONL path only,
never the in-process decode).

### 15.5 Mutex-first vs. lock-free — see §15.2 (folded there; not a separate independent question)

Answered above: the two claims in the brief are mutually exclusive as written. My verdict:
go lock-free SPSC from the first landing (SHIPPABLE, HOST-ONLY, no new dependency) rather
than accept a mutex-guarded queue that quietly falsifies "wait-free" for however long
"harden later" takes to actually happen.

### 15.6 Conflicts with the Phase-1 topology in flight — none structural; one ownership-discipline note

No collision found against the landed `components/common`/`components/runtime` shape
(`runtime.hpp:43-89`, `stage.hpp:36-48`, both read directly, current tree state). `Runtime`
is an ordinary synchronous template with no internal synchronization by design — correct,
since synchronization is not its job. **The one thing to make explicit before Nazzareno
implements the engine thread**: `Runtime<StageT,N>` exposes public mutable accessors
(`transport()`, `stage()`, `runtime.hpp:51-55`) with no thread-tagging of any kind. The
Phase 2 design's safety entirely depends on an unenforced convention — "only the engine
thread ever touches the `Runtime` instance; the GUI thread only ever touches the two ring
endpoints" — that nothing in the type system currently protects. A future maintainer
"just reading `runtime.stage()` for a quick UI hint" from the GUI thread would be a silent
data race the ring's own correctness does nothing to prevent. Recommend the Phase 2 code
make this ownership explicit structurally (e.g., the `Runtime` instance constructed
**inside** the engine-thread function's own stack/closure, never returning or storing a
reference reachable from GUI-thread code) rather than leaving it as a comment-only
discipline.

### 15.7 Synthesis and required corrections

**Verdict: APPROVED WITH REQUIRED CORRECTIONS.** The core reshape — library not binary, two
threads, in-process POD rings replacing an inter-thread socket — is architecturally sound
and the right call for a non-distributed single-process product; nothing here reopens that
fork. The following must be resolved before Nazzareno is dispatched:

1. **D38 GUI-purity collision (§15.1) — resolve explicitly, do not let it default silently.**
   Recommended resolution that satisfies BOTH the owner's topology directive AND D38's
   dependency-purity intent: split the merged GUI **binary** internally along the SAME
   boundary the OS process boundary used to enforce. A new, core-linking "engine-host"
   library (owns the `Runtime`/arrangrr Stage, the engine thread, the ring endpoints, and
   §15.3's shared label helpers) is the ONLY thing in the binary that includes
   `arrangrr`/`runtime`/`hostrt` headers or names `OutEvent`/`ChordQuality`/etc. Everything
   that exists today as `gui_sonotron_brain`/`gui_sonotron_layout`/`gui_sonotron_models`
   (`apps/gui-sonotron/CMakeLists.txt:20-65`) stays exactly as core-free as it is now,
   consuming only the finished `BrainEvent` POD handed across the ring — i.e., "the GUI"
   in the D38 sense becomes an internal library boundary rather than an OS process
   boundary, preserving the dependency direction and the "never sees a core enum"
   discipline at the granularity that actually matters (compilation units and
   `target_link_libraries` edges), even though the OS-level executable is now singular.
   This needs an explicit owner sign-off (it is a real amendment to a locked D38 reading,
   not a mechanical consequence of the already-approved topology change) — flagged as
   NEEDS-DECISION, not decided unilaterally here.
2. **Ring contract (§15.2) — pick lock-free SPSC now, not mutex-first.** The brief's own
   "wait-free" claim and its "mutex-guarded acceptable for the first cut" concession
   contradict each other; resolve by committing to atomic head/tail SPSC (padded, one
   reserved slot or explicit count, acquire/release ordering) from the first landing.
   SHIPPABLE, HOST-ONLY, no new dependency.
3. **Asymmetric overflow policy (§15.2) — specify per-direction, not one rule for both
   rings.** `OutEvent` (engine→GUI): drop under backpressure, matching the existing D38
   broadcast precedent. `Command` (GUI→engine): size generously and treat overflow as a
   `WarnCode`-class event, never a silent drop — there is no existing precedent for
   dropping a user-authored command and today's reliable socket path does not either.
4. **Shared label-helper extraction (§15.3/§15.4) — do this BEFORE writing
   `brain_event_from_outevent`, not after.** Export `jsonl.cpp`'s currently-private
   `quality_suffix`/`roman_degree`/`note_label`/`followed_label`/`section_name`/
   `warn_name`/`transport_name`/`producer_name` into a shared, testable module both
   `to_jsonl` and the new in-process decode call, closing the two-independent-tables
   drift risk instead of accepting it. Thread `prefer_flats` (or its Phase-2-split
   equivalent) through the new decode's signature explicitly — it is not optional context,
   `to_jsonl` already needs it for the identical labels.
5. **Ownership discipline for the `Runtime` instance (§15.6)** — construct it so it is
   structurally unreachable from GUI-thread code (closure-local to the engine-thread
   function), not merely documented as engine-thread-only.

No new dependency is implied by any of the above; every correction is either a
`std::atomic`-based ring (already core-doctrine vocabulary, `DESIGN.md:60,582`) or a
code-organization move within already-owned files. Item 1 is the one genuine
**NEEDS-DECISION** for the owner; items 2-5 are technical corrections I recommend making
directly, not forks.

---

## 16. Phase-4 execution design — Accompany (`9310`) as the first real N-stage pipeline (Corelli, 2026-07-12)

Triggered by: the coordinator's Phase-4 authoring request, following the owner's
committed Phase 1 / Phase 2a / Phase 2b landings. Read-only on product code; the only
write is this section. Grounded against `docs/strategy/roadmap-numbered.md:286,445-448`
(Accompany's own stated scope), `docs/DESIGN.md` D43/D53/D29, the landed
`components/runtime/include/runtime/{runtime,stage,out_scheduler}.hpp`, and the
freestanding `ChordDetector`/`FollowedContext` headers still living inside
`components/arrangrr` today.

### 16.0 Ground-truth check before designing on top of it

Verified directly, not assumed: the corrections `§15` required did land. `apps/gui-sonotron/src/spsc_ring.hpp:36-99` is genuinely lock-free (atomic `m_head`/`m_tail`, `alignas(kCacheLineSize)` padding, one reserved slot, acquire/release ordering) — no mutex, matching §15.5's recommendation, not the brief's original mutex-first concession. `components/hostrt/event_labels.hpp` exports exactly the helper set §15.3 asked for (`warn_name`, `roman_degree`, `note_label`, `followed_label`, …), and `apps/gui-sonotron/src/in_process_brain_session.cpp:1-45,294-304` shows the asymmetric overflow policy landed precisely as specified (Command ring never drops — a `command_ring_full` warn surfaces instead; OutEvent ring may drop, matching the D38 broadcast precedent) and the `Runtime`/`Shell` instance is constructed as a true engine-thread-local (`Impl::run_engine`'s header comment cites §15.6 by name). `apps/gui-sonotron/CMakeLists.txt:1-18,85-105` confirms D38 was formally relaxed by the owner and the internal-boundary device I proposed in §15.7 item 1 was built almost exactly as described: a single `gui_sonotron_engine` library is the ONLY thing in the binary linking `hostrt`/core headers; every other GUI library stays core-free. **This is a well-executed landing; §16 builds on real, verified ground, not the brief's aspiration.**

### 16.1 Fork — `components/chorddet`: own component, not a promoted module

**Recommendation: own component, dual-target-capable.** `components/arrangrr/include/arrangrr/chord/chord_detector.hpp:1-149` is already exactly what a dual-target component needs to be: `constexpr` throughout, a 128-bit held-note bitset (16 bytes), zero heap, zero host dependency — its own header comment already says "Core-portable and freestanding" (line 22-23). On the project's own component-vs-module test (absent/substituted/multiplied — cited already in §6 item 2 above): Accompany's own pipeline literally substitutes a file-fed detector for a live-keyboard one, and a future STM32 "detect a chord live, drive the band" deployment would want `[chorddet]→[arrangrr]` as a standalone two-stage chain with NO `arrangrr`-internal detector at all. That is the textbook "substituted" case. **Cost, honestly sized**: extracting `ChordDetector` out of `arrangrr` is small on its own (one self-contained header, already free of `arrangrr`-internal includes beyond `theory.hpp` — check whether `theory.hpp` itself needs to travel too, or stays shared via `components/common`/`arrangrr` the way `abi.hpp` stayed arrangrr's own vocabulary in §14.1's reasoning). The REAL cost is not the detector — it is `FollowedContext` (§16.3 below), which is the part that actually has to move ownership, not just address.

### 16.2 The ABI fork (the heavy one) — verdict: **NO reshape needed for Accompany's stated scope**

The coordinator's framing ("un solo OutScheduler che fa il total-order D29 su N stage con tag di priorità") conflates two genuinely different mechanisms that the current code keeps separate — tracing both settles the fork without touching `abi.hpp`.

**(a) Scheduled MIDI (`OutEvent::Kind::kMidi`) — the D29 total order generalizes to N stages with ZERO ABI change, because the tie-break was never stage-aware to begin with.** `runtime/out_scheduler.hpp:60-74,132-140`: `schedule(port, tick, msg)` computes `EventClass` from the raw `MidiMessage` **content itself** (`classify(msg)`, realtime < NoteOff < Other < NoteOn — line 29-41), not from any caller-supplied priority argument; `seq_no` is the scheduler's own monotonic emission counter, assigned at `schedule()`-call time. There is no field in `ScheduledEvent` (line 43-49) or parameter in `schedule()`'s signature for a "which stage called this" tag, and none is needed: with N stages sharing the ONE `OutScheduler` instance by reference (exactly `§14.3`'s established Seam-B1 pattern — Transport and the scheduler are ALREADY injected by reference into the one stage that exists today; a second and third stage get the identical treatment, not a new mechanism), `seq_no` ordering falls out of the **fixed, declared pipeline order** the same way it already falls out of today's single `Engine`'s internal fixed fire order (`fire_timeline`→`fire_chord_seq`→…→`fire_arp`→`flush`). Deterministic and golden-safe by construction, no reshape required.

**(b) Non-MIDI structural OutEvents (`kChordFollowed`/`kSection`/`kTransport`/`kBeat`/`kWarn`/`kChord`) never go through the scheduler at all — a fact the design doc's own §3.6 prose obscures.** Traced directly: `engine.hpp:226,323,351,432,448,461` all call `sink(OutEvent::…)` **synchronously and directly**, never `m_scheduler.schedule(...)`. Only `flush()` (`engine.hpp:237-240`) pops the scheduler and turns due `ScheduledEvent`s into `OutEvent::midi(...)`. So "the total order" is really two independent mechanisms today: a real priority-queue tie-break for scheduled MIDI, and a plain in-fixed-call-order synchronous emission for everything else. For N stages, (b) generalizes exactly like (a): the Pipeline composite (§16.4) calls each substage's `on_tick(ctx, sink)` in fixed declared order, passing through the SAME `sink` — non-MIDI events from different stages interleave in pipeline order, precisely mirroring what one `Engine`'s internal fire order already does. **No stage-tag is needed here either.**

**(c) The actual same-tick cross-stage data flow (D53) is not event-based at all — it is the SAME shared-reference-injection idiom already used twice.** `components/arrangrr/include/arrangrr/chord/followed_context.hpp:63-166` — `FollowedContext` is the single owner of `current`/`pending` (`m_state`/`m_pending`), already brokering **three** producers (`Producer::kDetect`/`kSequencer`/`kManual`, line 57-61) through `stage()`/`commit_now()`/`commit_bar()`, gated by the D47 `ChordFollow` policy — all without any producer knowing about any other (a D43-clean seam already, confirmed by `chord_engine.hpp:184`'s `m_followed.commit_bar()` delegation). Today it lives inside `ChordEngine`, inside the one `Engine`. **The correct Phase-4 move is to promote `FollowedContext` ownership to the Pipeline composite, injected BY REFERENCE into both the `chorddet` stage (writer, via `stage()`/`commit_now()`, replacing today's `Engine::observe_chord_input`) and the `arrangrr` stage (reader, via `state()`/`pending()`, replacing today's internal `m_chords.m_followed` access)** — the exact same pattern §14.3 already sanctioned for `OutScheduler` and §14.2/B3 for `Transport`. `OutEvent::chord_followed(...)` stays exactly what it always was: a one-way, host/GUI-facing NOTIFICATION of a change that already happened via the shared object, not the mechanism that makes the change visible cross-stage. This is why no ABI reshape is needed for D53 either — the wire event and the actual state-sharing mechanism are, and remain, two different things.

**(d) Roadmap scope confirms no tag is wanted, not just no tag is needed.** `roadmap-numbered.md:286,447-448`: "feed a plain MIDI, get the band under it"; the design doc's own §3.6 (line ~360): "the runtime's scheduler merges the melody thru-events and the band's MIDI in D29 total order → **one** JSONL/MIDI output stream." Accompany's entire point is a merged, undifferentiated output — like a real band, the melody and the accompaniment share one stream. A stage-id/source-tag would be solving a problem Accompany does not have. **Verdict: the ABI waiver stays unspent. `test_abi_frozen.cpp` requires no edit for Phase 4.** The waiver should be cashed only if/when a REAL future need surfaces (e.g., the GUI wanting to mute/solo/visualize the imported melody independently of the band) — flagged as a live future fork, not decided here, not needed now.

**One real, smaller risk this analysis surfaces (flag, not a blocker): `cancel_note_off`'s dedup key is `(port, channel, note)`, stage-blind by construction** (`out_scheduler.hpp:109-127` — it tombstones the earliest matching pending NoteOff regardless of which stage scheduled it). If the MIDI-source stage's melody thru and the arrangrr band stage were ever configured onto the SAME `port:channel`, one stage's retrigger logic could incorrectly cancel a note-off belonging to the other. In practice this is a **configuration discipline requirement** (assign the melody thru and the band distinct ports/channels, exactly like any real multi-track arrangement would), not an ABI or scheduler defect — and moot in practice for Phase 4c specifically, since a straight SMF-file playback stage has no reason to call `cancel_note_off` at all (it has no ratchet/tie retrigger concept of its own; it just replays fixed note-on/off pairs at their recorded ticks). Document the constraint; do not build a stage-scoped `cancel_note_off` for a risk that is currently theoretical.

### 16.3 The orchestrator / Pipeline design

**API shape — concept-based, not virtual, exactly continuing the Phase-1 idiom.** `runtime::Runtime<StageT, N>` (`runtime.hpp:43-89`) is untouched by this design: it stays exactly as generic as it is today, because the thing it drives becomes a single composite type that itself satisfies `runtime::StageLike` (`stage.hpp:44-48`) — `Runtime<Pipeline<Stage1,Stage2,Stage3>, N>`. Concretely:

```cpp
// illustrative, NOT frozen — the exact template arity/argument-forwarding shape is
// Nazzareno's to finalize, same caveat §3.6's original sketch carried.
template <typename MidiSourceT, typename ChorddetT, typename ArrangrrT>
class Pipeline {
 public:
  Pipeline(arrangrr::OutScheduler<N>& sched, arrangrr::Transport& transport,
           arrangrr::FollowedContext& followed, /* per-stage ctor args */)
      : m_midi_source(sched, transport, /*...*/),
        m_chorddet(followed, /*...*/),
        m_arrangrr(sched, transport, followed, /*...*/) {}

  // Fixed declared order (D53): MIDI-source produces notes THIS tick, chorddet
  // observes them THIS tick, arrangrr resolves against the now-current
  // FollowedContext THIS tick — same-tick visibility by construction, no lag.
  template <typename SinkT>
  void on_tick(const StageContext& ctx, SinkT sink) {
    m_midi_source.on_tick(ctx, sink);
    m_chorddet.on_tick(ctx, sink);
    m_arrangrr.on_tick(ctx, sink);
  }

  // ONE scheduler, ONE drain point — see §16.4's flush analysis: delegates to
  // arrangrr's EXISTING flush() unchanged; the other two stages need no flush
  // of their own (they have nothing private left to drain once the schedule
  // moved out).
  template <typename SinkT>
  void flush(SinkT sink) { m_arrangrr.flush(sink); }

 private:
  MidiSourceT m_midi_source;
  ChorddetT m_chorddet;
  ArrangrrT m_arrangrr;
};
```

This is deliberately the narrowest possible generalization: a **fixed, declared, 3-slot composite**, not a dynamic graph — continuing §3.6's own explicit refusal of DAW-style rewiring (`product-identity.md`/`workstation-vision.md`'s "power of a DAW, never its free-for-all graph," already the standing law this whole milestone works under).

**Placement fork (flag, mild NEEDS-DECISION):** the generic `Pipeline<...>` composite TEMPLATE is, by construction, dual-target-safe (plain reference members, no heap, no host facility) — it is HOST-ONLY only insofar as ONE of its three member types (`MidiSourceT`) is host-only. `project-structure.md:119-120` already commits `components/orchestrator` to host-only for VST/audio-source adapters and *dynamic* pipeline construction; that commitment does not by itself require the fixed-composite MECHANISM to be host-only too. I recommend the generic `Pipeline<...>` template live in `components/runtime` (dual-target, alongside `Runtime`/`Stage`) so a future firmware entrypoint wanting a bare `[chorddet]→[arrangrr]` two-stage chain (no MIDI-source, no host-only orchestrator) can reuse the exact same composite mechanism `tests/arm-smoke`-style, rather than hand-rolling an equivalent. `components/orchestrator` then becomes the layer that specifically **instantiates and names** the Accompany 3-stage pipeline (the one host-only stage included) — this is squarely orchestrator's job per its own charter ("composes N stage instances into a named pipeline… does not reimplement time or total order," §3.5 above) without inventing new composition machinery. Flagged for Palladio/owner confirmation since it is a placement call, not purely mechanical.

### 16.4 Seam C (new) — inbound MIDI fan-out, a gap the linear on_tick diagram does not cover

**§3.6's pipeline diagram only ever draws the OUTPUT direction.** Tracing the INBOUND side surfaces a real, previously-unaddressed mechanical gap: `components/arrangrr/include/arrangrr/engine.hpp:82-116` (`push_midi_in`) today does **two unrelated things from one parsed MIDI byte, in one method**: routing/arp-capture/harmony-suppress (arrangrr's own concern, `m_router.route(...)`, line 106-108) **and** live chord-detect observation (`observe_chord_input`, gated by `m_chord_detect && port == m_chord_detect_port`, line 110-112) — both fed by the SAME `m_parsers[port].feed(byte, …)` callback. `runtime::Runtime` does not mediate this path at all today: `components/hostrt/shell.hpp:71` calls `m_engine.push_midi_in(port, bytes, m_sink)` directly against the single Stage instance, bypassing `Runtime` entirely (inbound MIDI is not tick-scheduled, so it was never routed through `advance_ticks`).

Once `ChordDetector` promotes to its own peer stage, this single method must **split and fan out**: the SAME inbound byte stream needs to reach BOTH the arrangrr stage (routing) and the chorddet stage (recognition), and `Pipeline` — not either individual stage — is the natural place to own the fan-out, exactly mirroring how it already fans `on_tick` unconditionally to every member and lets each stage's own internal gate (arrangrr's routing table/arp port, chorddet's `m_chord_detect_port`) decide relevance. **Resolution recommended**: extend the `StageLike`-adjacent contract with an optional `push_midi_in(port, bytes, sink)` hook Pipeline calls on every member that has one; **each stage keeps its OWN `MidiParser` instance** rather than sharing a fourth cross-stage singleton — `MidiParser` is small, stateless-per-byte, per-port state, and duplicating it per interested stage is cheaper (in code and in reasoning) than promoting yet another shared object, and it keeps chorddet genuinely name-blind (no need to reach into arrangrr's parser state at all). This must be folded into Nazzareno's Phase-4 intake explicitly — it is compile-blocking the moment `ChordDetector` actually leaves `Engine`, the same way §14.4's B3 was compile-blocking the moment `Transport` left.

### 16.5 The MIDI-source stage

**Confirmed HOST-ONLY, zero new dependency**: `apps/tools/arrstyle-converter/src/smf.hpp:1-57` (`parse_smf`) already exists, hand-rolled, no dependency, but is unambiguously host-only by its own shape — `std::vector<std::uint8_t>` input, `std::string`/`std::vector<SmfTrack>` output, actual file bytes to parse. This is the same "core never touches the filesystem" precedent the original doc already cited for `state.dump` (`DESIGN.md:1696`). Design: parse the file ONCE, off the tick loop (at pipeline-construction time, host-only, heap freely used — this is exactly the allowed "laptop tool" regime `DESIGN.md:6`'s float caveat already carves out), building a flat, tick-sorted note-on/off event list merged across `SmfFile::tracks`. `on_tick(ctx, sink)` then just advances a plain index cursor comparing against `ctx.now` and calls the shared `OutScheduler&`'s `schedule(port, tick, msg)` for due events — reusing the SAME reference-injection pattern as every other stage, so its notes participate in the identical `(tick, class, seq_no)` total order as the band's (§16.2a), with zero bespoke merge logic. `flush()` is a no-op (nothing of its own to drain — see §16.3's Pipeline flush design). **Placement fork (flag for Palladio, not mine to resolve)**: does the new Stage adapter live under `apps/tools/arrstyle-converter` (which already owns `smf.{hpp,cpp}`) with `orchestrator`/`sonotron-server` linking it, or does `smf.{hpp,cpp}` itself get promoted to a shared host-only location? Either is structurally fine; I flag only that duplicating the SMF reader would be a real regression (an existing, tested, dependency-free parser must not get a second copy).

### 16.6 Host-only vs. dual-target boundary, confirmed per-stage

- **`chorddet`**: dual-target-capable, confirmed by direct read (`chord_detector.hpp`'s own `constexpr`/no-heap shape, §16.1).
- **MIDI-source**: host-only, confirmed by direct read (`smf.hpp`'s `std::vector`/`std::string`/file-I/O shape, §16.5) — never cross-builds `arm-none-eabi`, and is not expected to.
- **`Pipeline<...>` (the composite template)**: dual-target-safe AS A MECHANISM (plain references, no heap); host-only only through the specific `MidiSourceT` it is instantiated with for Accompany. A firmware entrypoint instantiating `Pipeline<NoOpSource, ChorddetStage, ArrangrrStage>` (or a 2-slot variant) stays freestanding.
- **`components/orchestrator`**: stays host-only per the already-committed `project-structure.md:119-120`, unaffected by the above — it is the layer that NAMES and WIRES the Accompany-specific, host-inclusive pipeline, not the layer that invents the fixed-composition mechanism.

### 16.7 Test strategy

**A new golden category, driven the SAME way every other golden is driven — through the `.acmd` L1 script grammar, not a second CLI mechanism.** Recommend `Shell` gain one new L1 verb (e.g. `midi-source load <path>`) rather than a parallel `--midi-source PATH` CLI flag on `sonotron-server`/`cli-arrangrr` — this keeps exactly ONE way to drive a golden fixture (the existing `--script FILE` virtual-clock harness already used by all 18 goldens), avoiding a second, less-tested code path for exactly the same purpose. `tests/golden/accompany_*.acmd` scripts would then read: `midi-source load fixtures/accompany_basic.mid` / `style load N` / `advance <ticks>` / `quit`, diffed byte-identical against a checked-in `.golden`, same harness (`tests/golden/run_golden.cmake`), same determinism guarantee (virtual clock, no wall-clock dependency). **Flag (Palladio's lane)**: a binary `.mid` fixture is a new kind of golden-test asset (every existing fixture is plain text `.acmd`/`.golden`); its placement under `tests/golden/` needs a naming/layout call, not an architectural one.

Additional test surfaces this milestone needs, sized honestly:
- `components/runtime/tests/`: new unit tests for the `Pipeline<...>` composite itself — a **1-stage** instantiation first (proving Pipeline is a transparent wrapper, zero behavior change against today's `Runtime<Engine,N>`), then a **2-stage** fixed-order test asserting `on_tick` calls members in declared order and `flush` delegates correctly, independent of Accompany's real stages (pure mechanism tests, cheap, fast).
- Whatever `test_engine*.cpp`/`components/arrangrr/tests/` exercised `ChordDetector`/`FollowedContext` in-Engine moves to `components/chorddet/tests/` (detector-only) and a new cross-stage integration test (two real stages, one shared `FollowedContext`, asserting same-tick visibility — the concrete regression guard for D53 in the N-stage world).
- `tests/arm-smoke`: gains a `[chorddet]→[arrangrr]` two-stage freestanding link-gate proof (the dual-target claim §16.1/§16.6 makes must be enforced by CI, not asserted in prose) — additive to, not a replacement of, the existing single-stage smoke.

### 16.8 Phased execution plan

**Phase 4a — Stand up the generic `Pipeline<...>` composite, single real stage (arrangrr), zero behavior change.**
Moves: `runtime::Runtime<Engine,N>`'s current direct single-stage instantiation is rewrapped as `Runtime<Pipeline<Engine>, N>` (a 1-tuple composite) — purely mechanical, `Pipeline::on_tick`/`flush` degenerate to a single forwarding call. Breaks: nothing behaviorally; every call site (`shell.hpp`'s `m_runtime.stage()` accessor) needs one indirection level added. Green gate: 18 goldens byte-identical (proves the composite is a transparent wrapper before any real N-stage composition is attempted) + the new Pipeline unit tests (§16.7) green.

**Phase 4b — Promote `ChordDetector` + `FollowedContext` out of `Engine`/`ChordEngine`; two-stage pipeline `[chorddet, arrangrr]`.**
Moves: `components/chorddet` stood up (§16.1); `FollowedContext` ownership moves to the Pipeline level, injected by reference into both stages (§16.2c); `Engine::push_midi_in` splits per Seam C (§16.4) — routing stays, `observe_chord_input` moves to chorddet's own `push_midi_in`, each stage keeping its own `MidiParser`. This is the highest-cost sub-phase in the plan (mirrors Phase 3's `Shell`-split cost precedent) — recommend its own sequenced sub-PRs (stand up `components/chorddet` + move `FollowedContext` behind a flag first, prove byte-identical goldens with the OLD single-stage wiring still active, THEN cut over the fan-out). Breaks: every test that constructed `Engine` and drove `push_command(kChordDetect,...)` + fed input on the detect port in one call needs to split, same shape as §5 Phase 3's `hostrt::Shell` test-split cost. Green gate: 18 goldens byte-identical (chord-detect behavior must be bit-identical, only its owning object changed) + `tests/arm-smoke`'s new 2-stage freestanding link-gate.

**Phase 4c — MIDI-source stage; 3-stage Accompany pipeline end-to-end.**
Moves: the SMF-reader wrap (§16.5) lands as the third pipeline member; `components/orchestrator` gains its first real named-pipeline wiring (§16.3's placement resolution). New: `midi-source load` L1 verb, the `accompany_*` golden category (§16.7). Breaks: nothing existing (purely additive — same "lowest-risk, purely additive" shape §5 Phase 2 already used for `sonotron-server`'s standup). Green gate: the new goldens byte-identical on first landing (no re-bless cycle, since nothing pre-existing changes) + a manual smoke feeding a real small MIDI file through the live `sonotron-server`/GUI path.

**Phase 4d — Close the loop: `components/orchestrator` stops being a placeholder.**
Moves: the Accompany pipeline-construction function becomes `components/orchestrator`'s first real, non-trivial content (today it is empty). Breaks: nothing. Green gate: `roadmap-numbered.md:445-448`'s own acceptance bar — "feed a plain MIDI, get the band under it" — demonstrated live in the GUI, the cheapest possible proof the roadmap itself names.

### 16.9 Synthesis

**Verdict: APPROVED WITH REQUIRED CORRECTIONS.** The pipeline shape (`[MIDI-source]→[chorddet]→[arrangrr]`, one shared `OutScheduler`, fixed sequential order, `components/orchestrator` composing, D43 held) is architecturally sound and matches the already-landed Phase-1 idiom closely enough that `Runtime<StageT,N>` needs **zero** code change — only a new composite `StageT`. Required corrections before Nazzareno is dispatched:

1. **The ABI waiver stays unspent (§16.2)** — ground truth confirms no stage-tag is needed for Accompany's actual scope; do not cash it speculatively. `test_abi_frozen.cpp` needs no edit.
2. **`FollowedContext` promotion (§16.2c) is the real cost center, not the ABI.** It must move from `ChordEngine`-owned to Pipeline-owned, reference-injected into two peer stages — size Phase 4b accordingly, not as a "just extract a header" move.
3. **Seam C (§16.4, inbound MIDI fan-out) must be folded into the intake explicitly** — it is compile-blocking the moment `ChordDetector` leaves `Engine`, exactly like §14.4's B3 was for `Transport`. This is new information this section surfaces; it does not appear anywhere in the design doc's existing §3.6.
4. **Pipeline's `flush()` delegates solely to arrangrr's own, unchanged `flush()` (§16.3)** — do not invent a multi-stage flush protocol; `NoteTracker`'s own header comment ("Observes the OUTPUT stream") confirms it is CORRECT, not merely convenient, for it to keep observing every scheduled event regardless of which stage produced it (Panic must silence the melody thru too).
5. **Two placement forks flagged, not resolved here (Palladio/owner)**: (a) does the generic `Pipeline<...>` composite live in `components/runtime` (my recommendation, dual-target-safe) or `components/orchestrator`; (b) does the MIDI-source Stage adapter live beside `smf.{hpp,cpp}` in `arrstyle-converter` or does the reader promote to a shared location.
6. **The `cancel_note_off` port/channel collision risk (§16.2) is a documented configuration constraint, not a defect to fix** — moot for Phase 4c's simple SMF-replay stage, worth a code comment so a LATER stage that does retrigger its own notes does not silently inherit the risk unexamined.

No new dependency anywhere in this plan; every SHIPPABLE item reuses an already-existing, already-tested primitive (`ChordDetector`, `FollowedContext`, `smf.{hpp,cpp}`, `OutScheduler`'s existing tie-break). Items 5(a)/5(b) are the only genuine NEEDS-DECISION placement calls; everything else above is a technical correction I recommend making directly during Phase 4b/4c, not a fork.

---

## 17. Phase-3 execution design — `cli-arrangrr` becomes a pure client; `hostrt::Shell` splits (Corelli, 2026-07-12)

Triggered by: the coordinator's Phase-3 authoring request, Accompany (Phase 4a-4d) landed at
`17f8f43`. Read-only on product code; the only write is this section. Grounded against the
REAL current `Shell` (`components/hostrt/shell.hpp:1-509`, `shell.cpp`, `shell_input.cpp`,
`shell_view.cpp`, `shell_music_commands.cpp`, `shell_io_commands.cpp`), not the §4 sketch —
`Shell` has moved since that sketch was written (it now drives a 3-stage
`orchestrator::AccompanyPipeline`, owns `FollowedContext`, and has a `midi-source load` verb).

### 17.0 Ground-truth check that changes the sizing before any cut is drawn

**`apps/sonotron-server` (Phase 2a) already exists and is feature-complete** —
`apps/sonotron-server/main.cpp:94-237` (`run_server`) already owns `AlsaMidi`, `UdsServer`,
the tick-timer clock drive, and the `poll()` fan-in, driven by the identical `Shell`/pipeline
code `cli-arrangrr` uses. **This means Phase 3's server side needs NO new code at all** — the
binary the original §4/§5 sketch treated as Phase-3 critical path was actually built two
milestones ago, for a different reason (Phase 2b's in-process mode needed the same headless
shape). Phase 3 is therefore narrower than originally scoped: it is *only* "turn `cli-arrangrr`
into a client of the ALREADY-EXISTING `sonotron-server`," not "stand up a server AND split the
client." **`apps/tools/cli-arrangrr/main.cpp` is confirmed untouched** (`main.cpp:168-822` still
constructs `AlsaMidi`/`Console`/`UdsServer control`/`Shell shell` together, exactly as the
original doc found) — Phase 3 has not started in code.

### 17.1 The exact cut line — traced against the real `Shell`, not the sketch

**Server-side (stays with `sonotron-server`, unchanged in spirit):**
- State: `m_runtime` (the 3-stage `Pipeline`), `m_engine`, `m_followed`, `m_sink`, `m_port_hook`,
  `m_ports`/`m_tracks`/`m_seqs` (+ `find_port`/`find_track`/`find_seq`), `m_next_in`/`m_next_out`,
  `m_prefer_flats`, `m_pending`/`m_pending_order` (the `@tick` queue — D29-adjacent, belongs with
  the clock driver), `m_quit`.
- Dispatch: `dispatch_midi` (`shell.cpp:541-565` — `cmd_port`, `cmd_route`, `cmd_thru`,
  `cmd_clock`, `cmd_midi_send`, `cmd_midi_source`, `cmd_panic`), `dispatch_music`
  (`shell.cpp:567-601` — `cmd_key`, `cmd_play`, `cmd_chord`, `cmd_style`, `cmd_seq` (+
  `seq_add`/`seq_transpose`/`seq_del`), `cmd_track` (+ `track_new`/`track_step`), `cmd_program`,
  `cmd_part`, `cmd_groove`, `cmd_arp`), `dispatch_transport` (`shell.cpp:604-616` — `cmd_transport`,
  `cmd_bpm`, `cmd_advance`). **`exec_line`/`exec_now` themselves stay server-side wholesale** — a
  pure client never re-implements L1 parsing, it forwards the raw line text, exactly as
  `sonotron-server`'s own `control.set_line_handler` already does today
  (`sonotron-server/main.cpp:130-135`) for every OTHER control-plane client.
- **A finding that shrinks the split's cost, traced directly, not assumed**: `m_ports`/
  `m_tracks`/`m_seqs`/`find_port`/`find_track`/`find_seq` do **not** need to be duplicated or
  shadowed client-side at all. Name resolution (`"in0"` → a numeric port index) happens
  entirely inside the server-side `cmd_*` handler bodies; the client only ever needs to
  forward the exact text a user typed (`"route in0 -> out0"`) over the socket verbatim — the
  identical mechanism `gui-sonotron`'s `uds_brain_session` already uses for every L1 line it
  sends. This is real, not hopeful: it is the SAME pattern already proven end-to-end today.

**Client-side (moves to `cli-arrangrr`'s presentation object — call it `TuiClient`, name not
mine to fix):**
- State: `m_panels` (`PanelManager`), `m_piano` (`PianoViewState`), `m_style` (`UiStyle`),
  `m_ui_mode`, `m_help_pinned`, `m_chooser` (`StyleChooser`) + `m_styles_was_focused`/
  `m_await_panel_digit`, the style-step debounce (`m_step_style_index`/`m_step_section`/
  `m_style_step_gen`/`m_style_step_pending`), `m_monitor` (`MidiMonitor`), `m_filter`
  (`MidiEventFilter`), `m_view_options`, `m_piano_held`/`m_harmony_held`
  (`ActiveNoteTracker`), `m_piano_key_mode`, `m_momentary_available`, `m_pending_source_key`,
  the toggle-autorepeat debounce (`m_input_time_us`/`m_toggle_last_us`), `m_parts_selected`/
  `m_groove_selected`/`m_arp_selected`.
- Dispatch: `dispatch_ui` wholesale (`shell.cpp:508-539` — `quit`/`exit`, `cmd_help`,
  `cmd_panel`/`panel_layout`/`panel_target`, `cmd_piano`/`piano_octave`/`piano_view`,
  `cmd_notes`, `cmd_filter`, `cmd_view`, `cmd_theme`, `cmd_colors` — verified by direct read,
  `shell_view.cpp:1-259`, that every one of these mutates ONLY `m_piano`/`m_filter`/
  `m_view_options`/`m_style`/`m_monitor` and never reaches the engine). Plus every `*_focused`/
  `*_key`/`*_select`/`*_adjust`/`chooser_nav_*`/`refresh_*_content`/`handle_ui_key`/
  `piano_key_event`/`try_global_steer`/`configure_terminal`/`configure_default_surfaces`
  (this last one, however, needs re-homing — see §17.3(a)).
- **`quit`/`exit` needs special handling, not a straight move**: today `dispatch_ui` sets
  `m_quit` locally. Post-split this must become a purely LOCAL client action (close the TUI,
  disconnect, exit the `cli-arrangrr` PROCESS) — it must NEVER be forwarded as an L1 line to
  the server, exactly the precedent `gui-sonotron`'s own `uds_brain_session.cpp:35-45`
  (`is_blocked_command`) already established and enforces today. Reuse that guard verbatim,
  do not reinvent it.

### 17.2 Seam D (new) — the panel-rendering layer is core-signature-coupled, not just Shell

**This is the finding that changes the honest sizing of Phase 3 the most.** Five files that
are unambiguously CLIENT-destined by concern already `#include` `arrangrr` headers, verified
by direct grep, not assumed:
- `components/hostrt/parts_view.hpp:6` → `arrangrr/arranger/arranger.hpp`; its
  `render_parts_panel(const Arranger& arranger, const MidiMonitor&, int selected, int cols,
  const UiStyle&)` (`parts_view.hpp:25`) takes the **live `Arranger&`** and calls its query
  methods (`.muted(role)`/`.soloed(role)`, per `shell.cpp:239-240`'s call site) — not just a
  data read, a genuine live-object dependency.
- `components/hostrt/groove_view.hpp:6` → `arrangrr/arranger/groove.hpp`;
  `render_groove_panel(const GrooveParams&, ...)` — a plain POD struct by reference, lighter,
  but still an `arrangrr`-namespaced type requiring the header.
- `components/hostrt/arp_view.hpp:6` → `arrangrr/arp/arpeggiator.hpp`;
  `render_arp_panel(const ArpeggiatorParams&, bool enabled, ...)` — same shape as groove.
- `components/hostrt/style_chooser.hpp:6` → `arrangrr/arranger/style.hpp` (style/section
  vocabulary for the chooser's builtin list).
- `components/hostrt/midi_monitor.hpp:10` → `arrangrr/abi.hpp` (`MidiMonitor` logs raw
  `OutEvent`s directly).

(`panel_manager.hpp` and `piano_view.hpp`, by contrast, are confirmed ALREADY core-free —
genuinely client-safe as-is, no work needed there.)

**Why this matters for the split, concretely**: once the split happens, `sonotron-server` is
headless (confirmed — it never renders a panel, `sonotron-server/main.cpp` has no TUI code
at all) — so **every one of these five files' sole remaining caller, after Phase 3, is the
CLIENT**. Leaving them signature-coupled to `Arranger&`/arrangrr-namespaced structs means
`cli-arrangrr`'s client half would `#include` real arrangrr headers, which is a genuine,
not-cosmetic collision with the SAME D38 "pure client, links neither `arrangrr` nor `hostrt`
[core headers]" intent the original doc's decision #3 already named for `cli-arrangrr`
specifically (not just `gui-sonotron`) — the exact wording `gui-contract-map.md:61-63`
already enforces for the GUI. **Resolution**: reshape `render_parts_panel`/`render_groove_panel`/
`render_arp_panel` (and `style_chooser`'s internal vocabulary) to accept plain, client-owned
mirror structs (e.g. a `PartsViewState{role, muted, soloed}[]`, reusing `GrooveParams`/
`ArpeggiatorParams`' own field shapes but redeclared arrangrr-free) fed by the wire echoes
§17.3(b) proposes — migrating these five files wholesale to the client side AS PART OF the
reshape, not before it. This is a genuine code change, not a `git mv`, and must be sized as
such — it is the single most-underestimated line item if this milestone is scoped as "just
split Shell."

**One deliberate exception, flagged as a precedent question for the owner, not decided here**:
`midi_monitor.hpp`'s dependency on `abi.hpp` (the frozen `Command`/`OutEvent` structs
themselves, not engine machinery) is architecturally different in kind from the other four —
`abi.hpp` is the wire vocabulary itself, which `gui-sonotron` deliberately avoided even so
(Rule 2, "own name tables … never `static_cast` a core enum") in favor of a self-maintained
string mirror. `cli-arrangrr`'s TUI rendering is richer and closer to a reference
implementation than `gui-sonotron`'s dashboard; I flag rather than silently resolve whether
`cli-arrangrr`'s client half may include `abi.hpp` directly (less duplication, reuses the
frozen enums) or must, like `gui-sonotron`, maintain its own mirror (more consistent with the
stricter precedent, more code). Either is structurally sound; they are not equivalent in
spirit, and the owner should pick one rather than have it default silently per-file.

### 17.3 The client↔server contract — two distinct wire gaps, both additive, waiver stays unspent

**(a) Inbound gap — several interactive gestures bypass L1 text entirely today.** Traced
directly, not assumed: `shell_input.cpp:41-55` (`surface_send_note`, backing every piano/chords
key) constructs raw MIDI bytes and calls `feed_midi(port, bytes)` — i.e.
`push_midi_in`/`Pipeline` fan-out — never `exec_line`. `shell_input.cpp:301-323` (`parts_key`),
`:336-369` (`groove_adjust`), `:371-382` (`groove_key`), `:394-432` (`arp_adjust`) each
construct a raw ABI `Command` and call `m_engine.push_command(c, m_sink)` directly, also never
through L1 text. **None of these has a wire-safe equivalent today** — the UDS-JSONL protocol
(`gui-contract-map.md:7-24`) carries L1 TEXT lines inbound, never a binary `Command` and never
raw MIDI bytes. A pure client cannot reproduce these gestures as-is. **Resolution recommended**:
extend the L1 grammar with narrow new verbs mirroring the shape these already have as Commands
— e.g. a note-level verb for piano/chords key-driven note-on/off (`play <note> on|off` variants
already exist in spirit via `cmd_play`; confirm/extend rather than invent), and existing
`groove`/`arp`/`part` L1 verbs (`cmd_groove`/`cmd_arp`/`cmd_part` already exist server-side,
`shell.cpp:592-599`) already accept absolute values — so the arrow-adjust gestures need the
client to compute the delta **client-side against a locally-shadowed current value** (§17.3(b))
and then send the already-existing L1 verb with the new absolute value, not a new wire concept.
This keeps the wire's own discipline (`gui-contract-map.md:15-21`: text in, JSONL out, no binary
on the socket) intact — it is additive grammar reuse, not a protocol reshape.

**(b) Outbound gap — no event reports several panels' CURRENT values, needed for both display
and arrow-adjust deltas.** Traced directly against `abi.hpp`'s seven `OutEvent::Kind` values
(`kMidi`/`kTransport`/`kWarn`/`kChord`/`kSection`/`kChordFollowed`/`kBeat`) and against what
`refresh_groove_content`/`refresh_arp_content`/`refresh_parts_content`/`refresh_styles_content`/
`refresh_chords_content` (`shell.cpp:209-284`) actually read: `m_engine.arranger().groove_params()`
(6 fields), `m_engine.arp().params()` + `.arp_enabled()` + `.arp().held_count()`,
`m_engine.arranger()`'s per-role mute/solo, the current loaded style index (no event reports
this — `kSection` only ever carries the SECTION, confirmed by `engine.cpp:533,582` — never the
style), `m_engine.chord_detect()`/`chord_follow()`/`chords().mode()`/`chords().key()`. **None of
these has a corresponding `OutEvent` today.** This is the real, substantial wire-contract gap
the coordinator's question 2 was right to suspect — bigger than a nitpick, smaller than a
reshape. **Recommendation, sized to be minimal**: append exactly ONE new `OutEvent::Kind`
(next free id, per `abi.hpp`'s own frozen-baseline comment: `Kind = 7`) — call it
`kParamState` — that reuses the ALREADY-STABLE `Param` enum as its field tag (`code` field,
reinterpreted as `Param`, exactly the same enum `Command::param` already carries for every one
of these domains: `kGroove`/`kArp`/`kPartMute`/`kPartSolo`/`kStyleLoad`/`kChordDetect`/
`kChordFollow`/`kChordMode`/`kKeySet`) plus `msg.d1`/`d2` for the value(s) and `port` where a
role/index is needed — ONE new Kind covers every missing echo instead of seven-to-nine bespoke
ones. Emitted server-side whenever the corresponding `cmd_*` mutates state (mirroring
`kChordFollowed`/`kBeat`'s own precedent — both were themselves additive `OutEvent::Kind`
growth for the identical reason, GUI needing live state it could not otherwise see, per this
repo's own P0-1/P0-2 commits) plus once per field on a client's initial connect (a `state dump`
— reusing the `state.dump` precedent already cited in this document, §3.6, for file I/O; here
it is just "replay every current value once"). **This is additive-only growth, not a reshape**:
`test_abi_frozen.cpp` gains new pinned values, it does not change any existing one — the
waiver stays unspent, `kProtocolVersion` stays 1.

### 17.4 What breaks — the test inventory, counted, not guessed

Direct census (not a sample) of `components/hostrt/tests/`, by whether a test function calls
an engine/dispatch operation (`exec_line`/`push_command`/`.engine()`/`advance_by`) AND a
panel/UI operation (`*_focused`/`*_key`/`panels()`/`chooser()`/`refresh_*_content`/
`handle_ui_key`/`piano_key_event`) in the same body:

| File | engine-only | **MIXED** | UI-only | neither |
|---|---|---|---|---|
| `test_host.cpp` (73 fns) | 11 | **15** | 20 | 27 |
| `test_panel_nav.cpp` (4 fns) | 0 | **1** | 1 | 2 |
| `test_panels.cpp` (21 fns) | 0 | 0 | 1 | 20 |
| `test_style_chooser.cpp` (9 fns) | 0 | 0 | 0 | 9 |

**16 test functions across the suite genuinely straddle the cut line** and must split, not
move wholesale — e.g. `test_shell_chord_detect_panel`, `test_note_letters_steer_from_every_
panel_but_repl`, `test_permanent_transpose_persists_across_bars`,
`test_pressing_a_then_s_yields_different_roots_when_properly_released`,
`test_single_finger_new_key_replaces_previous_root`, `test_parts_solo_migrated_to_i_key`,
`test_ctrl_p_play_stop`, `test_styles_panel_chooser`, `test_style_section_stepping`,
`test_step_mirrors_chooser`, `test_tab_enters_piano_from_repl` (full list traceable by the same
grep pattern used here). **Split strategy**: each such test currently proves "a physical key
press produces the right engine effect" in one in-process assertion (press a key → check
`m_engine`'s resulting state). Post-split this single assertion becomes TWO, in two different
test binaries: (1) a server-side integration test driving the now-established L1/`Command`
equivalent (§17.3(a)) and asserting the resulting `OutEvent`/state-echo, and (2) a client-side
unit test asserting the key press produces the RIGHT outbound line/Command (mocked transport,
no real engine) — the same "assert the translation, not the effect" pattern
`in_process_brain_session.cpp`'s own `command_line_to_command` unit tests already use today
for the Phase-2b integrated mode. This is mechanical PER test once the pattern is set, but 16
tests is a real, non-trivial line item, not a footnote.

### 17.5 Sub-phased plan, gates, and a revision of §5's original sequencing advice

**§5's original Phase-3 recommendation — "parse-event-into-panel-state behind a flag BEFORE
deleting the in-process path" — is RECONFIRMED, not just repeated, now that Seam D (§17.2) and
the wire gaps (§17.3) are known.** It is more clearly correct now than when written: the panel
reshape (§17.2) and the new `kParamState` echo (§17.3b) can each be built and unit-tested
**against the EXISTING in-process `Shell`** (feed it synthetic `OutEvent`s, assert the reshaped
render functions produce the same panel text they do today) before `cli-arrangrr` ever opens a
real socket — derisking the two hardest, least-mechanical pieces first, independent of the
socket plumbing itself (which is the EASY, already-proven part, `uds_brain_session.cpp` is a
working template).

**Phase 3a — Additive ABI growth + the panel-rendering reshape (Seam D), entirely IN-PROCESS,
zero client/server split yet.**
Moves: append `OutEvent::Kind::kParamState` (§17.3b) and wire it into every relevant `cmd_*`;
reshape `render_parts_panel`/`render_groove_panel`/`render_arp_panel`/`style_chooser`'s
vocabulary to plain mirror structs, called from the EXISTING single-process `Shell` (which
now populates the mirror struct from its own live `Arranger&`/`GrooveParams`/`ArpeggiatorParams`
— behavior-preserving, since the live values and the mirror are populated from the same source
in the same process). Breaks: nothing behaviorally — TUI output must be byte-identical to
today's (a snapshot/golden-text comparison of `cli-arrangrr`'s panel rendering is the concrete
gate). Green gate: `test_abi_frozen.cpp` passes with the new `kParamState` pinned; existing
`test_panels.cpp`/`test_style_chooser.cpp` pass unedited (they test rendering shape, not the
data source); 18 goldens + Accompany goldens unaffected (no `cmd_*` behavior changed, only an
extra emitted event + a data-plumbing reshape).

**Phase 3b — `cli-arrangrr` grows a UDS client session, BEHIND A FLAG, alongside the untouched
in-process path.**
Moves: a `UdsBrainSession`-equivalent (architecturally identical to
`apps/gui-sonotron/src/uds_brain_session.cpp`) added to `cli-arrangrr`; a parsed-event → the
Phase-3a mirror-struct decode (the SAME shape `gui-sonotron`'s `brain_event_from_outevent`
proves for the in-process ring, reused here for the JSONL-over-socket path — one canonical
decode target, two wire sources, consistent with the §15.3 principle already established).
Both paths (in-process `Shell`, socket `UdsBrainSession`) coexist selectable by a launch flag
(`--control PATH` already exists on `cli-arrangrr` today for the CONTROL socket it *serves*;
this is the mirror direction — connecting AS a client — needs its own flag,
e.g. `--connect PATH`, distinct from the existing server-side `--control`). Breaks: nothing —
purely additive, lowest-risk shape (mirrors Phase 2's own "additive, not yet cutting over"
sequencing). Green gate: a manual smoke — `cli-arrangrr --connect PATH` against a running
`sonotron-server`, TUI renders and responds to input, byte-comparable to the in-process mode's
own rendering for the same script of actions.

**Phase 3c — The 16 mixed tests split (§17.4); cut over the default; retire the embedded
engine.**
Moves: `hostrt::Shell` itself splits into the server-side dispatch object (unchanged shape,
already named in §4) and the client-side presentation object; `cli-arrangrr/main.cpp` drops
`AlsaMidi`/`UdsServer`(-serving)/the engine-half `Shell` ownership, keeping only the client
presentation object + the new session. Breaks: the 16 mixed tests (§17.4), split per the
strategy given; every call site assuming `cli-arrangrr` is self-contained (any doc/script
launching it without a running `sonotron-server`) needs updating. Green gate: goldens
unaffected (they exercise `sonotron-server --script`, already true since Phase 2a); a full
manual TUI session against a live `sonotron-server` reproduces every interactive surface
(piano/chords/styles/parts/groove/arp) — the ORIGINAL Phase-3 green gate from §5, still the
right bar.

### 17.6 Owner forks, explicit

1. **`abi.hpp` inclusion precedent (§17.2)** — may `cli-arrangrr`'s client half include the
   frozen ABI structs directly (less duplication) or must it, like `gui-sonotron`, maintain its
   own string/enum mirror (stricter, more consistent, more code)? Not resolved here.
2. **New CLI flag naming** (`--connect` or otherwise) for the client-connecting mode, distinct
   from the existing server-serving `--control` — Palladio's naming lane, flagged not decided.
3. **`kParamState`'s exact field layout** (which values fit in `msg.d1`/`d2` vs need a second
   event for wider fields, e.g. `GrooveField::kSeed`'s value range) is an implementation detail
   for Nazzareno to finalize against real field ranges, not a design fork — noted so it is not
   silently treated as fully specified by this section.

### 17.7 Synthesis

**Verdict: APPROVED WITH REQUIRED CORRECTIONS, sized honestly as the highest-cost remaining
piece of this whole milestone — confirmed, not just asserted.** Three cost centers compound,
not one: (1) the mechanical `Shell` split itself (real, but the smallest of the three — the
cut line is clean and traced in §17.1, and `m_ports`/`m_tracks`/`m_seqs` turn out NOT to need
duplication at all, a genuine cost REDUCTION versus the original sketch); (2) Seam D's panel-
rendering reshape (§17.2), previously unnamed anywhere in this document, now the single most
underestimated line item; (3) the additive ABI growth for state-echo (§17.3b), small in ABI
terms (one new `Kind`) but wide in surface (touches every `cmd_*` that mutates displayed
state). Working against this, one genuine, evidenced cost REDUCTION: `apps/sonotron-server`
already exists (§17.0) — Phase 3 is "make `cli-arrangrr` a client of it," not "build a server
and split a client" as the original sketch assumed. Required corrections before Nazzareno is
dispatched: land Phase 3a (ABI growth + Seam D reshape) fully in-process and green FIRST,
proven byte-identical, before any socket code is written (§17.5, reconfirming and sharpening
§5's original sequencing) — this is not optional given how much of the real cost lives in the
reshape, not the transport.

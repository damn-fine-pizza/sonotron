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

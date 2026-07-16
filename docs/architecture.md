# sonotron architecture

The canonical description of what sonotron is, how the repository is organized, and how
its components compose at runtime. Companions: `docs/product-vision.md`,
`docs/gui-and-ux.md`, `DESIGN.md`
(the D1..Dn decision log and the numbered-node roadmap).

## 1. What sonotron is

**sonotron** is the project: a **collection of musical-object components** plus the apps
that orchestrate them. `arrangrr` is **one component among several** under the sonotron
umbrella (`project(sonotron)`) — the **arranger component** (decides *what* to play). The
"MIDI brain" is the set of core components together, not a single library.

The collection can spawn many deliverables. Apps either **link** a chosen set of
components (an orchestrator/daemon) or are **pure clients** over the UDS-JSONL socket
(D38); a component is **name-blind** to its consumers (D43), wired only through a small
POD port.

## 2. Repository layout

```
sonotron/                    project(sonotron)
├── apps/                    deliverables — orchestrators (link components) and pure clients (socket)
│   ├── sonotron-server/     host backend — links the components, builds the pipeline, serves the socket
│   ├── gui-sonotron/        desktop frontend — ImGui/GLFW; pure-client GUI over an internal engine library
│   ├── demo/                demonstration apps (clean · jam · shared lib)
│   └── tools/               cli-arrangrr (host CLI) · arrstyle-converter · arrstyle-extractor · melodd
├── components/              OUR libraries — split on the regime axis (core/ vs platform/, see §3)
│   ├── core/                platform-AGNOSTIC, freestanding-capable (compiles on STM32 M7 too)
│   │   ├── common/          base, dual-target — Tick/time types, MidiMessage, ARR_ASSERT, bar constants
│   │   ├── runtime/         dual-target kernel — Transport, OutScheduler, Runtime, Stage, Pipeline, MidiParser
│   │   ├── chorddet/        dual-target — live ChordDetector + FollowedContext + theory, as a stage
│   │   └── arrangrr/        dual-target — the arranger stage (transport/scheduler removed → runtime)
│   ├── platform/            platform-DEPENDENT, needs a hosted OS — FLAT, no further regime nesting
│   │   ├── midisrc/         host — SMF reader + MIDI-source stage (file I/O)
│   │   ├── orchestrator/    host — composes named stage pipelines (Accompany)
│   │   ├── hostrt/          host — runtime glue + TUI (jsonl, uds, ALSA, panels, views)
│   │   └── engines/
│   │       └── melodd/      host — audio engine (SoundFont synth)
│   └── samplrr/             host — SLOT — sampler (not yet folded into platform/, deferred)
├── third_party/             vendored host-only deps: imgui · glfw (off-limits to freestanding)
├── tests/                   integration + golden + arm-smoke (freestanding link gate); unit tests live in-component
├── build/                   linux-x64/ · arm64/  (per-target output)
└── cmake/ · docs/ · scripts/
```

## 3. Componentization principles

**`components/` splits on the regime axis: `core/` vs `platform/`.** This reverses an
earlier decision (this section used to say the opposite — components stayed flat and the
target boundary was a per-component CMake declaration, never a folder). The owner locked
the two-tier split instead: `components/core/` holds every platform-AGNOSTIC,
freestanding-capable component (`common`, `runtime`, `chorddet`, `arrangrr` — compiles for
BOTH host and `arm-none-eabi`, added unconditionally at the top level); `components/
platform/` holds every component that needs a hosted OS (`midisrc`, `orchestrator`,
`hostrt`, plus the `engines/` family, e.g. `melodd` — added only in the host `else()`
branch). `platform/` itself stays **flat** (no further regime nesting inside it, `engines/`
is a namespacing convenience for audio-realization components, not a third tier) — the
folder boundary now *is* the regime contract for the two top-level tiers, no per-component
prose declaration needed to know which side of the line a component sits on.

The regime is still independently **verified**, not just declared by placement: CI
cross-builds every `core/` component for `arm-none-eabi` and link-checks it (`nosys.specs`).
Today this proves compile+link, not symbolic heap-absence — a real no-heap symbol scan is
outstanding hardening. `components/samplrr` is a still-unwired placeholder slot and, as of
this restructure, deliberately **not yet folded** into `platform/engines/` — a deferred
follow-up, not an exception to the rule (it has no code and is not `add_subdirectory`'d
anywhere today). A target specialization that cannot fit the two-tier split (e.g. a genuine
third regime) would be a **sibling component**, created only when actually needed — not a
pervasive folder split invented ahead of a real second case.

**`apps/` are the deliverables.** `sonotron-server` (backend — links the components,
serves the socket), `gui-sonotron` (desktop frontend), `cli-arrangrr` (host CLI), `demo/`
(examples). Neither firmware nor an stm32 app is a top-level dir; the arm target is
verified by a freestanding link gate under `tests/arm-smoke/`, and a real chip app can
become `apps/stm32/` later.

**`third_party/` is external and host-only.** Vendored deps (Dear ImGui, GLFW) live here,
distinct from `components/` (which is *ours*), never linked by a freestanding component —
arrangrr stays dependency-free (D4/`0800`).

**The component-vs-module criterion.** A thing is its **own component** (a top-level
library with a POD port + hooks) if it can be **absent, substituted, or multiplied**
across the deployment matrix or across a real runtime seam. Otherwise it is a **module
inside a component**. Applied: `melodd`, `samplrr`, `orchestrator` are components (host,
optional/absent on STM32); `chorddet` earned its own component because Accompany
substitutes a file-fed detector for the live-keyboard one; `harmony`, `midi-fx`, `arp`,
`looper`, `groove` are modules inside `arrangrr`.

**Sequencer — one transport, N sequencing-engines.** Two different things:
1. **Transport / clock master** — *the time*, the "when". There is **one** (subdivisions/
   polymeter layer on top). It now lives in `components/core/runtime` (§4), driven
   host-side.
2. **Sequencing-engine (per lane)** — *what* is scheduled on a part (step, euclidean,
   generative…). These are **N** and composable; extracted, an engine becomes a component
   (`sequencrr`), instantiated many times and wired by the pipeline. `Timeline` stays a
   module inside the arrangrr stage for now (arrangrr cannot function without a lane); the
   Stage port (§4) already leaves room for `sequencrr` to graduate later as one more stage
   instance, needing no further restructure.

## 4. The runtime kernel and the composable pipeline

The runtime is pulled out of `arrangrr` and generalized so the product can compose a
**pipeline** of stages — MIDI sources, chord detection, the arranger, and (future)
sampler/virtual instruments/VST·CLAP·LV2. It has the *power* of a DAW (routing, many
sources, composable graph) but presents it through **precise workflows**, never the
linear-timeline free-for-all — that is sonotron's identity.

### 4.1 The component dependency graph

```
components/core/common      (base, dual-target, header-only)
  Tick/TickOffset/BpmX100/TickAccumulator/kPpqn/kGridPpqn/kMidiClockDivider,
  kBeatsPerBar/kTicksPerBeat/kTicksPerBar, ARR_ASSERT, MidiMessage + midi::*
      ▲ PUBLIC                          ▲ PUBLIC
components/core/runtime            components/core/arrangrr
  Transport, OutScheduler,      Arranger, ChordEngine, ChordSequencer,
  MidiParser,                   ArpeggiatorEngine, Timeline, Router, NoteTracker,
  Runtime<StageT,N>,            + a Stage-adapter implementing runtime's Stage contract
  Stage / StageLike concept,        ▲
  Pipeline<...> composite           │ PUBLIC (the ONE arrangrr→runtime edge, for stage.hpp only)
      ▲___________________________ _│
```

`common (base) ← runtime (kernel) ← arrangrr (stage)`, acyclic: both `runtime` and
`arrangrr` depend directly on `common`; the single `arrangrr → runtime` edge is narrow
(the Stage port only). No edge points from `runtime`/`common` back to `arrangrr`.

- **`components/core/common`** holds the base primitives every layer shares — the time
  vocabulary, `MidiMessage`, the `ARR_ASSERT` macro, and the three bar constants. Pure
  data/constant/macro layer, header-only, dual-target.
- **`components/core/runtime`** owns the clock and the emission machinery: `Transport`
  (owns no I/O, never reads a clock — time is injected), one `OutScheduler` (the
  total-order emission queue), `MidiParser`, the `Runtime<StageT,N>` driver, the
  `Stage`/`StageLike` contract, and the `Pipeline<...>` composite. Dual-target,
  freestanding, no-heap.
- **`components/core/arrangrr`** is reduced to a **pure arranger stage**: harmonic
  context + transport tick in → band MIDI `OutEvent`s out. It keeps `Arranger`,
  `ChordEngine` (the D47 gate), `ChordSequencer`, `ArpeggiatorEngine`, `Timeline`,
  `Router`, `NoteTracker`; it lost `Transport`, `OutScheduler`, and the `advance_ticks`
  fire-loop to `runtime`.
- **`components/core/chorddet`** holds the live `ChordDetector` + `FollowedContext` +
  `theory`, promoted out of `arrangrr` as a dual-target-capable stage (`constexpr`
  throughout, a 128-bit held-note bitset, zero heap).
- **`components/platform/midisrc`** (host-only, file I/O) wraps a dependency-free SMF
  reader as a MIDI-source stage. **`components/platform/orchestrator`** (host-only) is
  the layer that **instantiates and names** specific pipelines (e.g.
  `orchestrator::AccompanyPipeline`); it composes stage instances but does not
  reimplement time or total order. VST/CLAP/LV2 and audio-source adapters live here
  (future). **`components/platform/engines/melodd`** is the host audio engine
  (SoundFont synth); **`components/samplrr`** is a still-empty slot.

### 4.2 The Stage port / pipeline contract

**Design law: the product has the POWER of a DAW but never its free-for-all graph.** The
port is a **fixed, declarative composition** — no cycles, no runtime rewiring — not a
general dataflow engine. It is expressed as a **compile-time concept**, not a virtual
class (per doctrine `DESIGN.md:577`, "compile-time polymorphism in the hot path; `virtual`
only at HAL boundaries" — a per-tick stage dispatch is the hot path):

```cpp
// components/core/runtime/include/runtime/stage.hpp — no arrangrr include, no OutEvent named.
struct StageContext {
  Tick now;                    // stream tick — the SAME injected clock every stage sees
  Tick transport_tick;         // transport's own musical position; meaningless if !playing
  bool transport_playing;
  bool is_clock_pulse;         // computed once by Runtime so a Stage never reaches into Transport
  TransportState transport_state;
  Position transport_position;
};

template <typename StageT, typename SinkT>
concept StageLike = requires(StageT& s, const StageContext& ctx, SinkT sink) {
  { s.on_tick(ctx, sink) } -> std::same_as<void>;
  { s.flush(sink) } -> std::same_as<void>;
};
```

`Runtime<StageT,N>` owns the single `Transport` and the single `OutScheduler<N>
m_scheduler`, and injects that scheduler **by reference** into the stage(s). "Ownership"
means who constructs/sizes/frees the queue and is its single source of truth (that is
`Runtime`); a stage still calls `schedule()`/`flush()` on the injected reference — the
same way `Engine::EventSink` is a reference the caller owns.

- **Time injection.** The host clock thread / virtual golden clock / STM32 timer calls
  `Runtime::advance_ticks(n)`. A stage never sees a raw clock — only the `StageContext`
  the runtime hands it.
- **Same-tick ordered composition (D53/D29).** Stages fire in **fixed sequential order
  within one tick** (`fire_timeline` → `fire_chord_seq` → chord-commit → `fire_arranger`
  → `fire_arp`, then `flush`). A chord-detect stage wired *before* the arrangrr stage is
  visible to arrangrr the SAME tick — the port is not a one-tick-lag message queue.
- **Total order across stages.** The one `OutScheduler` merges every stage's `emit` calls
  under the `(tick, class_priority, seq_no)` tie-break — `class_priority` is computed from
  the raw `MidiMessage` content (`classify`: realtime < NoteOff < Other < NoteOn), never
  from a caller tag, so N stages need **no stage-id/source tag**. `seq_no` ordering falls
  out of the fixed pipeline order. Non-MIDI structural events (`kChordFollowed`/`kSection`/
  `kTransport`/`kBeat`/`kWarn`/`kChord`) bypass the scheduler entirely — each stage emits
  them synchronously through the shared `sink` in pipeline order.

### 4.3 Cross-stage data flow and inbound MIDI fan-out

Same-tick harmonic hand-off is **not** event-based: `FollowedContext` (the single owner of
the followed chord, §9) is owned by the `Pipeline` composite and injected **by reference**
into the `chorddet` stage (writer) and the `arrangrr` stage (reader).
`OutEvent::kChordFollowed` is a one-way host/GUI notification of a change that already
happened through the shared object — the wire event and the state-sharing mechanism are
two different things.

Inbound MIDI fans out at the `Pipeline`, not inside a stage: the same parsed byte stream
reaches both the arrangrr stage (routing/arp-capture/harmony-suppress) and the chorddet
stage (recognition). Each interested stage keeps its **own** `MidiParser` rather than
sharing one — cheaper than a cross-stage singleton and keeps `chorddet` name-blind.

### 4.4 Accompany — the exemplar pipeline

`orchestrator::AccompanyPipeline` wires three stage instances (`9310`, host-only):

```
[MIDI-source stage]  reads a plain SMF file (parsed once, off the tick loop), emits raw
       │             MIDI thru events AND feeds notes into →
[chorddet stage]     the promoted ChordDetector+resolver, emits HarmonicContext changes →
       │
[arrangrr stage]     Arranger + ChordEngine's D47 gate resolve the NTT-safe band notes
                     against the fed context and the shared transport tick →
       runtime's OutScheduler merges melody thru-events + band MIDI in D29 total order
       → one JSONL/MIDI output stream.
```

The output is a single merged stream — like a real band, melody and accompaniment share
one stream. `cancel_note_off`'s dedup key is `(port, channel, note)`, stage-blind by
construction: assign the melody thru and the band distinct ports/channels (ordinary
multi-track discipline), a documented configuration constraint rather than a scheduler
defect.

The `Pipeline<...>` composite mechanism is itself dual-target-safe (plain reference
members, no heap); it is host-only only through the specific host-only `MidiSourceT` an
Accompany instance uses. A firmware entrypoint can instantiate a bare `[chorddet] →
[arrangrr]` two-stage chain freestanding.

### 4.5 STM32 / freestanding wiring

On the chip the arm entrypoint links `common` + `runtime` + `arrangrr` (+`chorddet` where
a live detector is wanted) and wires ONE fixed pipeline, MIDI-in → arrangrr stage →
MIDI-out. VST/audio hosting does not run on STM32, so `orchestrator` and the rich pipeline
are host-only; `tests/arm-smoke/` cross-builds the freestanding chain (`common` +
`runtime` + `arrangrr`, plus the `[chorddet]→[arrangrr]` chain) for `arm-none-eabi` and
link-checks it.

## 5. The apps

**`apps/sonotron-server`** is the host backend: it links `runtime` + `orchestrator` +
`arrangrr`(+`chorddet`) + the non-TUI half of `hostrt` (`jsonl`, `uds_server`,
`gm_program`, `note_names`, `alsa_midi`) + `ALSA::ALSA`, owns `AlsaMidi`, the tick-timer
clock loop, and the `UdsServer`, and serves the UDS-JSONL socket. It is headless — no TUI
code. Its `--script FILE` virtual-clock mode drives the golden tests deterministically.

**`apps/gui-sonotron`** is the desktop frontend (Dear ImGui + GLFW, vendored under
`third_party/`). It is a pure client in the D38 sense — its presentation libraries link
neither `arrangrr` nor `hostrt` and include zero core headers, carrying their own wire
layer and name tables (never `static_cast` a core enum). Where the engine runs in-process
on a dedicated thread, a single internal `gui_sonotron_engine` library is the ONLY unit in
the binary that links core/`hostrt` headers or names `OutEvent`/`ChordQuality`; it exchanges
finished `BrainEvent` PODs with the GUI over an SPSC ring. This preserves the D38 dependency
boundary at the granularity that matters (compilation units and `target_link_libraries`
edges) even though the executable is singular.

The in-process rings are **lock-free SPSC** (atomic head/tail, cache-line padding, one
reserved slot, acquire/release ordering; no mutex), with an **asymmetric overflow policy**:
the engine→GUI (`OutEvent`) ring may drop under backpressure, matching the D38 broadcast
precedent (best-effort; a slow client drops events rather than stalling MIDI); the
GUI→engine (`Command`) ring never silently drops a user-authored command — it is sized
generously and an overflow surfaces as a `command_ring_full` warn.

**`apps/tools/cli-arrangrr`** is the host CLI/TUI on the brain. It keeps the TUI half of
`hostrt` (console, kitty-keys, panels, piano/style/groove/arp views, `rc_config`). It
still owns the engine in-process today; converting it to a pure socket client of
`sonotron-server` (splitting `hostrt::Shell` into a server-side dispatch object and a
client-side presentation object, the same pattern `gui-sonotron` already proves) is a
sequenced follow-up.

## 6. Placement rules — where new things go

- **A new arrangrr module** (new header/source inside the freestanding brain, e.g. a new
  MIDI-FX): `components/core/arrangrr/include/arrangrr/<module>/*.hpp` + `src/*.cpp` if it
  needs a translation unit (most of `arrangrr` is header-only) — never a `host/`
  subfolder; the component stays flat and the target-regime is declared by its placement
  under `components/core/` (`CMakeLists.txt`'s `-fno-exceptions -fno-rtti`).
- **A new base primitive** shared by both `runtime` and `arrangrr`: `components/core/common`.
- **A new host-only runtime facility** (a new Shell command family, a TUI panel, a host
  adapter): `components/platform/hostrt/<name>.{cpp,hpp}` — flat.
- **A new CLI-only concern** (argument parsing, a subcommand's wiring):
  `apps/tools/cli-arrangrr/main.cpp`.
- **A new dev/import tool**: its own `apps/tools/<name>/` sibling, self-contained,
  dependency-free unless explicitly cleared with the owner.
- **A new cross-component test** (drives the CLI end-to-end, or spans two components):
  `tests/golden/` (deterministic `.acmd`/`.golden` pair) or `tests/integration/`
  (OS-level, may SKIP) — never inside a component's own `tests/`.
- **A new component-local unit test**: beside its component, in that component's own
  `tests/`, following the existing `<component>_test()` CMake function pattern.

**Naming/coverage anchors:** target names `arrangrr` (freestanding lib), `hostrt` (host
lib), `cli_arrangrr` (CLI exe, dir `apps/tools/cli-arrangrr`, `OUTPUT_NAME cli-arrangrr` —
demo launch scripts `pgrep` it by that name). The enforced CORE coverage gate measures
`components/core/arrangrr/` + `components/core/runtime/` (with their own `tests/` and
`tests/` excluded); `components/core/common/` is excluded from the branch gate (a
header-only data/constant/macro layer with no branches). Golden tests run against
`sonotron_server --script`.

## 7. The command/event ABI and the hook interface

### 7.1 The as-built binary ABI

`components/core/arrangrr/include/arrangrr/abi.hpp` defines the typed binary contract (D26/
`0700`), enforced by `test_abi_frozen.cpp`'s `static_assert` wall:

- `Op { kSet, kDo, kGet }`. `Command { op, param, idx, a, b, c }` (`sizeof <= 20`).
  `OutEvent { kind, port, msg, tick, code }` (`sizeof == 16`). `kProtocolVersion = 1`.
- A flat `Param` enum spanning every domain; ids are append-only, never reused
  (`kChordFollow = 41`, `kInputZone = 42`, next free id 43).
- **Two colliding addressing lineages inside `Param`.** `kGroove`/`kArp` already use
  `(family, field-in-a, value-in-b)`; everything else (`kChordHold`, `kPartMute`,
  `kTransportTempo`, …) is one enumerator per leaf. Both are live.
- **`Op::kGet` is dead.** Dispatch switches on `cmd.param`, never on `cmd.op`; no handler
  constructs or consumes `kGet`.
- **Seven `OutEvent::Kind`s:** `kMidi` (0), `kTransport` (1), `kWarn` (2), `kChord` (3),
  `kSection` (4), `kChordFollowed` (5, the followed harmonic context changed from any
  producer — carries cur/next roots+qualities, valid bits, and the `Producer`), `kBeat`
  (6, a 24-PPQN transport heartbeat that lets a client draw a moving playhead).
- **Observability is bifurcated.** Channel A crosses the process: `OutEvent` →
  `host::to_jsonl` → `UdsServer::broadcast`. Channel B never crosses: direct const
  accessors on the engine (`chords()/arranger()/sequences()/arp()/transport()`), read
  in-process by `hostrt`'s `refresh_*_content()` panel functions. The TUI's truth and the
  wire's truth are two reads of two surfaces and can disagree.
- **Quantization-to-boundary exists, inconsistently spelled:** `kChordPlay` overloads
  `idx`, `kStyleSwitch` overloads `c` for the same immediate/next-bar concept.
  `Runtime`/engine advance recognizes bar boundaries at one sequential point, where the
  followed-context commit already runs before the arranger fires — the one true commit
  point.

Doctrine that binds regardless of ABI shape: no heap, no RTTI/exceptions/`std::string` in
the core, dual-target freestanding, dependency-free core (`0200`/`0300`/`0700`/`0800`).

### 7.2 The proposed uniform hook interface

This is the debt of D17b / DESIGN.md §24 ("every component exposes uniform hooks —
observability + interaction") — designed in the docs as an addressable L1 param-space,
never made real in the binary. The v1 ABI freeze (node `11720`) is lifted so the
interface can generalize `Param`'s dual duty into the one lineage that already works.

**One POD language.** Every hookable fact in every component is addressed by the same
triple `(ComponentFamily family, InstanceId instance, FieldId field) → Value{a,b,c}`:

```cpp
struct HookCommand {
  Op              op;        // kGet | kSet | kDo
  ComponentFamily family;    // ONLY ever a family (kTransport, kHarmony, kArranger, …), never a leaf
  std::uint16_t   field;     // family-local leaf id (was GrooveField/ArpField, now universal)
  std::uint16_t   instance;  // was `idx`; 0 for singleton families
  Origin          origin;    // kDefault | kDirector | kHuman  — the arbitration currency
  Boundary        boundary;  // kImmediate | kNextTick | kNextBar — replaces the ad hoc bits
  std::int32_t    a, b, c;   // unchanged payload convention
};
```

`family` is only ever a family; `instance` generalizes today's `idx` into the uniform
collection selector (singletons = 0); `field` is the family-local leaf; `Value{a,b,c}` is
the unchanged three-`int32_t` payload (every packing convention rides unmodified). The
existence-proof sizing is `1+1+2+2+1+1+12 = 20` bytes — the same budget `Command` has.

- **Observability = one channel.** A `FieldEvent{family, field, instance, origin, tick,
  a,b,c}` replaces Channel A's `kChord`/`kSection`/`kTransport` and Channel B's accessors
  at the same emission sites, delta-on-change. The envelope narrows from the current kinds
  to **3**: `kMidi` (unchanged — a note-on is an occurrence, not a value at rest), `kField`
  (universal), `kWarn` (unchanged). Adding one more observable fact is a new `(family,
  field)` pair, never a wire-shape change. A host-only `HookMirror` — a bounded table keyed
  by `(family, instance, field) → FieldEvent`, populated exclusively by the same stream a
  socket client receives — lets the TUI read the identical channel a remote client would;
  every panel refresh reads the mirror, not the engine directly.
- **Snapshot-on-connect = `Op::kGet` given a body.** A point read returns exactly one
  `FieldEvent` synchronously; a wildcard read (`instance = kAllInstances` and/or
  `field = kAllFields`) walks the component's declared field table and emits one
  `FieldEvent` per leaf in one synchronous burst. The wildcard `kGet` *is* the snapshot —
  no second read mechanism; `get *` at `@0` diffs like any golden event block.
- **Human always wins — uniform arbitration.** `Origin` rides every `Set`/`Do`; ordering
  is `kHuman > kDirector > kDefault`, always, for every family (`kLivePriority`
  generalized). Each field carries an **explicit-origin latch**: a `kDefault` write is a
  no-op once any non-default origin has committed. At the commit boundary a `kHuman` write
  beats a `kDirector` one targeting the same field in the same window. **Boundary, not
  thread:** `kImmediate` lands on the same call; `kNextTick`/`kNextBar` stage into the
  pending slot consumed at the single sequential commit point.
- **Where the Director lives: nowhere inside the core.** The core only ever sees
  `Origin::kDirector` on an inbound `HookCommand`, through the same entry point a human
  keystroke or remote GUI click uses. "Director" is a *caller role*, not a core dependency
  — a firmware build that never links a Director still compiles. The hook interface **is**
  the D43 name-blind POD port: `melodd`/`samplrr`/`orchestrator` speak the same
  `(family, instance, field, origin, boundary) → value` language to arrangrr and to each
  other, not a second thing built beside it.

Open forks for the owner: position/beat heartbeat cadence (per-tick position would flood
a delta-on-change channel — coarser cadence vs poll-only); whether arbitration is a
permanent latch or `kLivePriority`'s richer held/release-with-expiry behavior (harmony
needs this right on day one); the `HookMirror` storage shape; and whether diagnostics stay
a distinct `kWarn` kind or fold into a pseudo-family.

## 8. Chord following

sonotron turns live playing into the chords the band follows. Two separate things live
here: **literal chord follow** (the standard arranger behaviour, shipping) and **Pivot**
(a novel, parked feature).

**How commercial arrangers do it.** Yamaha (Genos/PSR), Korg (Pa) and Roland (E-A7) keep
three orthogonal mechanisms, never fused: (1) **literal chord detection** — play a chord,
the arranger detects the literal chord and re-harmonises in real time; (2) **chord looper/
sequencer** — a separately recorded progression that loops hands-free; (3) **global
transpose** — a dedicated semitone-shift button. No mainstream arranger lets you play a
chord to *relatively transpose a running progression* — that fusion is sonotron's Pivot.

### 8.1 Literal chord follow (what ships)

The style has no progression of its own; every part re-roots to a single *followed chord*.
Play a chord → the band follows and **holds** it until you play another. This is the
default and what a player expects.

- **`FollowedContext`** (`components/core/chorddet/include/chorddet/followed_context.hpp`) is the
  single owner of the followed chord. Live detection commits it immediately; it persists
  (nothing overwrites it) until the next live chord.
- **Single-finger vs fingered** (`ChordDetector`): single-finger = one key names a
  scale-aware chord on that root; fingered = spell the chord by holding its notes. In
  single-finger a **new key REPLACES** the previous one — one key == one chord (the fix for
  the "A then S give the same chord" reports; without it a plain-TTY toggle accumulated the
  held set and the detector rooted on the lowest note).
- **The `ChordSequencer` is a separate, optional backing track**, not the style. The engine
  default is **`ChordFollow::kLivePriority`**: while you HOLD a live chord it beats the
  sequencer — the band follows your finger and the sequencer comps its rhythm on your chord
  (no clash); on release the sequencer's next step resumes its own progression. With no
  sequencer a live chord latches (chord memory). `kAuto` (last-writer race) is kept only as
  an explicit legacy mode. `kLivePriority` is decided at the `fire_chord_seq` call site
  (held-vs-released state), not the static gate.

### 8.2 Two-zone harmony input (D49, node `2330`)

The PIANO panel and the CHORDS panel drive two distinct playable surfaces, selected by
panel focus, not by pitch split. The PIANO panel feeds `kPianoInputPort` (in0), zone
`kMelody`: keys sound and steer nobody. The CHORDS panel feeds `kHarmonyInputPort` (in1),
zone `kHarmony`: the same key bindings are output-suppressed (silent) but OBSERVED by the
`ChordDetector`, re-harmonizing the band without sounding a note. On the wire the zone is
set via `kInputZone` (ABI value 42): `a` = input port, `b` = `InputZone` (0 = melody,
1 = harmony); `kHarmony` suppresses the port's note output, `kMelody` routes/sounds and is
the default. Chord recognition thus has its own silent surface and no longer competes with
the piano's melodic role.

### 8.3 Scale-aware single-finger (D45, node `2220`)

Single-finger mode maps one pressed key to the diatonic MAJOR-or-MINOR triad of that root
(`single_finger_quality`), never diminished or augmented: root = the pressed key; quality
= the diatonic degree's quality; where the degree is diminished or augmented it SNAPS by
fifth-restoration — a diminished triad snaps to MINOR (keep the minor third, restore the
perfect fifth), an augmented triad snaps to MAJOR; a chromatic (out-of-scale) root defaults
to MAJOR. Single-finger is triads only — it never adds 7ths/extensions (that is diatonic
mode's `smart_quality`), and unlike diatonic mode (D20-strict: out-of-key is rejected/
silent) it never rejects a chromatic root, so a performance shortcut never blocks
mid-phrase. This is sonotron's own scale-aware single-finger (Casio-Chord/"smart"
lineage), not the key-independent classic Yamaha convention.

### 8.4 Pivot — the parked novel feature (node `2590`)

Playing a chord relatively transposes a running chord progression by the interval between
the chord you play and the **current bar's original chord**: absolute from the original
(never cumulative), **root-only** (each chord keeps its quality), persistent across loop
wraps, reset by playing the current bar's original chord. It fuses the Chord Looper and the
Transpose button into **one played gesture**. Musically sound — it is a live key change that
preserves the progression's function (`I-vi-IV-V` stays `I-vi-IV-V`), and root-only is
inarguable because the feature *is* a key change. Its weak points are ergonomic, not
conceptual (reset-by-original-chord is arithmetic a performer won't intuit → prefer an
explicit `seq home`; a bare press is too overloaded → ships as an **opt-in mode**, "play
the Transpose button as a chord"). Full model, edge cases, and implementable spec in
`docs/style-corpus-and-generation.md`.

## 9. Followed-context ownership and routing seams

### 9.1 The single owner of the followed chord

`FollowedContext` is the **only** code able to change the followed chord (`current`/
`m_state`, read every tick and shown as host `current key:`; `pending`/`m_pending`, shown
as `next key:`). It brokers three producers — `Producer { kDetect, kSequencer, kManual }`
— through one gate, none knowing about any other, exposing exactly:

- `stage(Producer, ChordState)` — the single entry every producer calls; the D47
  `ChordFollow` follow-gate is applied *inside* (one policy field, not three threaded
  bools). A non-selected producer's stage is a no-op. It also sets the `m_explicit` latch:
  "a real producer has established a context since the last reset."
- `commit_bar()` — the only writer of `current` for quantized producers, at the bar
  boundary (D53's staging model).
- `commit_now(Producer, ChordState)` — for producers D53 keeps immediate (the sequencer
  fires on its own step grid; optionally manual): writes `current` and sets `m_explicit`,
  still through this one object and gate.
- `establish_default(Key)` — sets the home-key context **only if `!m_explicit`**; a no-op
  once any producer has set a real chord. Lifecycle handlers (transport-start, style-load,
  style-switch, section) call THIS — they have **no** API that can overwrite an explicit
  chord.
- `reset()` — the genuine new-song reset: clears `m_explicit` + `pending`, then re-homes.

**The ownership invariant (makes a whole class of bugs impossible by construction):**
`current` changes ONLY via `commit_bar()`/`commit_now()` from a producer-staged value, or
via `establish_default()` when no explicit context exists. This is why transport-start and
style-load no longer clobber a manual chord, why a chord persists across a section/style
change, and why the followed context does not self-drift: its only writer is a
producer stage gated by one visible D47 policy, and the sequencer's per-step re-assert is
one arbitrated decision the user can gate off (`chord follow manual`), not a silent
overwrite.

`ChordFollow` (D47, node `2340`) also carries **sound ownership** (D48): the direct chord
voice rides the SAME predicate as steer. `ChordEngine` owns one shared voicing
(`m_sounding[4]` on one out port/channel); a non-selected producer must still fire its
`OutEvent::chord` (display/record) and advance its state, but the audible `sound()` is
gated — you cannot hear the sequencer's G7 while the band follows a manual C. It is **one
axis, not two**: a second free "who-sounds-the-pad" axis only ever manufactures harmonic
contradiction. The directly-stacked voicing is one optional monitor/comp voice (its reason
to exist is D13: hear a chord NOW, transport idle, no style loaded); the arranger parts are
the real chord sound. `kAuto` stays byte-identical legacy (last-writer-wins). Switching
follow mid-play releases the sounding voice on any de-selecting change, routed through the
scheduler so the note-tracker stays honest. (The adjacent "keep the sequencer's rhythm,
re-voiced onto a live override" want is a separate future re-voicing feature, not a second
sound axis.)

### 9.2 Routing seams

"Where does sound go out" is answered by **four independent output-destination owners plus
three input-side selectors** — a correct separation, each a distinct producer, low coupling
and high cohesion:

Output destinations
- `Router m_router` — the general MIDI in→out matrix (`kRouteAdd`/`kRouteClear`), used only
  in the inbound-MIDI callback.
- Arranger per-role routes `m_routes[kRoleCount]` (`kStyleRoute`) — a separate,
  arranger-owned table, NOT through `m_router`.
- ChordEngine destination `m_out_port`/`m_out_channel` (`kChordOut`).
- Arp destination `m_arp_out_port`/`channel` (`kArpOut`).

Input-side selectors
- `m_chord_detect_port` (`kChordDetect`) — which port the detector OBSERVES.
- `m_input_zone[port]` (`kInputZone`, D49) — route-vs-suppress per port.
- `m_arp_in_port` — which port the arp captures.

The whole input-side decision lives in one place — the inbound-MIDI callback: three
booleans (`arp_captures`, `harmony_suppress`, detect-observe) read the three selectors and
fan to router / arp / detector. Changing a zone or a detect port is a single field write in
a command handler read at exactly one site; it does not ripple. `kStyleRoute` is equally
local (one `m_routes[idx]` write plus an idempotent voices re-emit). The ABI stays honest
and additive; each seam is freestanding, no new dependency. The only mild smell is that the
four output owners have no unified read model — acceptable, but worth remembering if a
future "output map" panel wants one.

## 10. Open / deferred

- Extracting `sequencrr` as a component-engine (the transport↔engine port and the
  N-instance model; the Stage port already leaves room for it as one more stage instance).
- `components/samplrr` (still a slot) and the wider VST/CLAP/LV2 audio-source adapters in
  `orchestrator`.
- Converting `cli-arrangrr` into a pure socket client of `sonotron-server` (splitting
  `hostrt::Shell` server/client, the panel-render reshape off live `arrangrr` types, and an
  additive state-echo event so panels can render from the wire).
- Landing the uniform hook interface (§7.2) and its owner forks.
- A real no-heap symbol scan to fully gate the freestanding contract.

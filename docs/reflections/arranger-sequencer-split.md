# Reflection — arrangrr vs sequencrr: split the arranger and the sequencer?

> **Status — future / not current phase.** Partially executed: the fire-order invariant
> (recommendation *d*) shipped as DESIGN.md node `4150` (2026-07-05, pre-freeze). The `TickProducer`
> compile-time extraction remains open, gated on the step-sequencer's Step/Track model
> (nodes `4200`/`4300`/`4400`) settling — deep-sequencer work behind the GUI freeze line.

Status: DIRECTION / verdict. Read-only branch context: `chord-single-finger`.
Author: Prospero (dual-axis critique). Scope: module/project topology, not code.
This file is the only mutation of the review; no source was touched.

## Sul tavolo (the reflection, restated)

Today, are the ARRANGER and the SEQUENCER separate in arrangrr, or entangled?
How much "sequencer" is actually already there? Does it make sense to cleave the
project into two — `arrangrr` (the arranger) and `sequencrr` (the sequencer) — as
a MODULE / PROJECT boundary, independent of runtime (on a PC they could be two
libs/threads/processes; on STM32 they would be one binary regardless)? Concretely:
(1) map the real seams — arranger-only vs sequencer-only vs SHARED state/logic;
(2) is a sequencer a distinct MUSICAL object from an arranger or two faces of one
engine; (3) if split, what is the boundary — is this the D43 name-blind-peers-
wired-by-an-orchestrator pattern applied *inside* the MIDI brain, or over-
engineering; (4) separate code-topology from runtime-topology per target
(Linux-sim / STM32H743 / SBC), respecting freestanding/no-heap/one-clock/POD-ABI;
(5) name the trap — does splitting BUY anything real, or is it interface tax on a
thing that shares clock + scheduler + chord-context anyway.

## The seam, as the code actually stands (not as the docs wish it)

`Engine::advance_ticks` is the whole answer. Each tick, while playing, it fires —
in this fixed order — `fire_timeline` → `fire_chord_seq` → `fire_arranger` →
`fire_arp`, then flushes the scheduler. Four PRODUCERS, one loop.

Producer-owned, disjoint state (the seam already exists at class level):
- **Step sequencer** = `Timeline` / `Track` / `Step` (`timeline/timeline.hpp`).
  Absolute authored notes on a 16th grid, per-track length (polymeter), mute/solo.
  It emits `note_on(channel, s.note)` DIRECTLY — no chord resolution. This is the
  "write" gesture, and it is THIN: no probability / ratchet / tie / rest / micro-
  timing / CC-lanes / song-mode / live-record (§12 wants all of these; §27 gap
  analysis confirms they are "entirely absent from `Step`"). Mostly aspirational.
- **Chord sequencer** = `ChordSequencer` (`chord/chord_sequencer.hpp`). Record /
  quantize-after / loop / D28-functional-degree storage / musical transpose. The
  MATURE "sequencer" here — but it is an arranger *feeder*: on tick it calls
  `m_chords.sound(...)` AND emits `OutEvent::chord`, i.e. it PRODUCES into the
  harmonic bus that the arranger consumes. It is a harmony source, not a peer
  instrument.
- **Arranger** = `Arranger` (`arranger/`). Style material resolved through the NTT
  kernel against the live chord (`on_tick(tick, key, chord_state, schedule)`),
  sections, groove, voice-leading. Chord-driven, reactive.

Shared substrate every producer sits on (the "kernel"):
- ONE transport / clock — `m_transport`, `m_now` (D16, D27, D29).
- ONE out-scheduler — `m_scheduler`, a single min-heap in D29 total order
  `(@tick, class_priority, seq_no)` with a single monotonic `seq_no` tie-break.
- ONE note tracker (`m_tracker`, anti-stuck/panic), ONE router, ONE `ChordEngine`
  (`m_chords`) — the harmonic-context bus the chord-seq writes and the arranger
  reads.
- ONE ABI — the flat `Param` enum in `abi.hpp` (`kTrack*` step-seq, `kSeq*`
  chord-seq, `kStyle*`/`kPart*`/`kGroove` arranger, `kArp*`), one `Command` /
  `OutEvent` POD surface for all of them (D26).

So: **already separate in the data model, hardwired in the orchestration.** The
four producers share the common shape `on_tick(Tick, <context>, ScheduleFn)` and
never reference each other; the Engine hand-wires them and pins their order. That
fixed order is not incidental — the comment in `Arranger::on_tick` is explicit:
feed the chord AFTER the chord sequencer has fired this tick "so bar downbeats
resolve against the fresh chord." The tie-break order IS harmony correctness. It
is groove.

## Verdict by bucket

### Tenere

1. **The instinct that arranger and sequencer are already separable — correct.**
   *Asse ingegneristico.* They are distinct classes with disjoint state and a
   shared `on_tick(..., ScheduleFn)` producer shape; neither references the other.
   The seam exists; it does not need to be manufactured, only named.
2. **The "shared bus" model — chord/harmonic context as the bus the arranger
   consumes and the chord-sequencer produces — is exactly the implemented
   reality.** *L'intersezione.* `ChordEngine` IS that bus; `fire_chord_seq` writes
   it, `fire_arranger` reads it. The mental model in the question is not a proposal,
   it is a description. Keep it; it is right on both axes.
3. **Separating code-topology from runtime-topology as explicit axes — the right
   lens.** *Asse ingegneristico.* Conflating "two modules" with "two processes" is
   the classic category error; naming the two axes up front is what lets the
   runtime answer be "never split" while the code answer stays open.
4. **Making "does the split BUY anything, or is it interface tax" the deciding
   test — keep it.** *L'intersezione.* It is the correct knife. Applied honestly,
   it does most of the cutting below.

### Rilavorare

1. **The `sequencrr` framing itself — two symmetric products — is the wrong model;
   rework it to "one kernel + N tick-producers."** *L'intersezione (severe).* The
   two things are not peers. The mature "sequencer" (`ChordSequencer`) is an
   arranger FEEDER — it produces the harmonic context the arranger eats. The only
   truly time-driven, user-authored, chord-blind sequencer (`Timeline`) is the
   LEAST-built component in the tree. "Split into arrangrr + sequencrr" over-
   dignifies the thinnest part and mis-types the chord-seq's role. The honest
   topology is one MIDI brain = a **kernel** (transport + scheduler + tracker +
   router + ABI + harmonic-context bus) + a set of **tick producers** (arranger,
   step-seq, chord-seq, arp, looper-to-come). Not two products.
2. **"Apply D43 inside the MIDI brain" — half right; scale it down.** *Asse
   ingegneristico.* The name-blind producer shape D43 describes ALREADY exists:
   `on_tick(tick, ctx, ScheduleFn) -> TickResult`, with `ScheduleFn` as the blind
   sink. Worth making explicit as a compile-time `TickProducer` concept (D32:
   compile-time polymorphism in the hot path, no vtables, no heap). But D43's FULL
   apparatus — POD-over-a-transport, process-agnostic, mutually name-blind across a
   wire — is over-engineering HERE, because these producers must share one clock,
   one scheduler and one chord bus WITHIN a single tick: they couple by reference,
   not by message. A ring buffer between the arranger and the sequencer would buy a
   decoupling that the same-tick harmonic dependency forbids. Reserve D43's real
   apparatus for melodd (the cross-chip audio peer); inside the brain, formalize
   the concept and the fire-order invariant, nothing heavier.
3. **The timing of the whole question is premature.** *L'intersezione.* You cannot
   draw a durable boundary around the step sequencer while its data model (§12:
   prob/ratchet/tie/rest/micro-timing/CC-lanes/song-mode/live-record) is still
   almost entirely unbuilt. A boundary drawn around an unfinished shape gets drawn
   wrong and then ossifies. Settle the `Step`/`Track` model first; the seam will
   tell you where it wants to be once the sequencer has actually earned its shape.
   Corollary: this also revisits D10 — the code has ALREADY diverged from "one
   Living Timeline, three gestures" into de-facto parallel engines (the arranger
   does not write into `Timeline`; it runs its own fire loop). That divergence is
   fine, but it should be acknowledged rather than left as an unstated
   contradiction.

### Buttare

1. **Two repositories / two projects. Kill it.** *Asse ingegneristico (most
   severe).* The ABI is deliberately ONE surface (D26); the clock is ONE (D16/D29);
   the scheduler is ONE min-heap with ONE monotonic `seq_no`. Two repos force a
   shared kernel vendored across a repo boundary plus cross-repo versioning of a POD
   ABI that exists precisely to be singular — pure tax. Worse, it endangers the
   green-from-day-one dual-target build (D3): every ABI change would now be a two-
   repo lockstep migration. Zero gain on either axis; real cost on both. Refuse.
2. **Any runtime split (two threads / two processes) between arranger and
   sequencer, on ANY target. Kill it.** *L'intersezione (most severe).* D29's
   total-order determinism requires a single scheduler and a single emission
   counter; a second thread or a second scheduler destroys the goldens and breaks
   the SPSC discipline. The chord-seq → arranger order is a same-tick data
   dependency on the harmonic bus — a second thread would race it, and "the band
   resolved last bar's chord" is an audible wrong note, not a subtle bug. On
   STM32H743 it is simply outside the budget and the discipline (D32/D33). The
   producers are one tick, one clock, one heap, one thread — everywhere.

## Verdetto

The instinct is sound and the seam is real — but it is already in the code, and it
is not the seam the name `sequencrr` implies. What arrangrr has is not two products
straining to separate; it is one deterministic kernel (clock + scheduler + tracker
+ router + ABI + harmonic bus) with a growing set of tick-producers that couple by
reference within a single tick. Do NOT split the project: no second repo, no second
thread, no second scheduler — the one-clock/one-heap/one-ABI constraints (D16, D26,
D29, D32) and the same-tick chord-seq → arranger dependency make a runtime split a
determinism-and-groove regression, and a repo split pure interface tax. The honest,
cheap move is the opposite of a split: NAME the topology you already run — a
`TickProducer` compile-time concept the four producers already satisfy, and the
chord-seq-before-arranger fire order promoted from a comment to a documented
invariant — and defer even that until the step sequencer's `Step`/`Track` model
(§12) is real enough to have an opinion about its own boundary. Two names, one
brain.

## Concrete recommendation (the four asks)

- **(a) Module / project boundary:** ONE repo, ONE binary. Do not create
  `sequencrr`. Re-describe the existing structure as **kernel + tick-producers**,
  not arranger-vs-sequencer. Keep `arranger/`, `timeline/`, `chord/`, `arp/` as the
  modules they already are.
- **(b) Interface, if you formalize anything:** a compile-time `TickProducer`
  concept — `on_tick(Tick, const HarmonicContext&, ScheduleFn) -> TickResult` — that
  the four producers already de-facto satisfy (verify with `static_assert`, D32; no
  vtables, no heap, no ring buffer between producers). The harmonic context stays a
  by-reference `ChordEngine`, not a POD-over-a-wire — that wire is melodd's (D43),
  not the sequencer's.
- **(c) Runtime topology per target:** one thread / one process for
  arranger + sequencer on Linux-sim, STM32H743 AND SBC alike — the sim must match
  the device or the goldens lie (D2/D3). The ONLY cross-process peer is melodd
  (D43), and audio never crosses that seam.
- **(d) Smallest first step, IF worth it:** not a split. Make the implicit contract
  explicit — extract the `TickProducer` concept, route the four `fire_*` through it,
  and lift the chord-seq → arranger ordering into a named invariant with a
  `static_assert`-backed producer registry. But do it AFTER the step sequencer's
  data model settles; before that, the correct first step is to write nothing and
  keep it monolithic. (Recommendation only — Prospero does not implement.)

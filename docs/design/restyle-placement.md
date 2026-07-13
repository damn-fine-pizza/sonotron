# Restyle (#1, node 9320) — structural placement review

Author: Corelli (architecture critic). Read-only review; Item A / review C of
`docs/design/phase5-execution-plan.md`. Ottorino's musical-scope review (B) is
a separate document; this one is placement/shape only.

## 0. Scope and what was traced

Decisions read: `docs/DESIGN.md` D4 (dependency-free core), D24 (NTT —
chord-tone resolution at playback), D29 (total-order emission), D32/D33
(no-heap/freestanding, STM32H743 budgets), D40 (per-role step-resolution
pipeline), D43 (chorddet must never depend on arrangrr), D47 (chord-follow
gate), D53 (chorddet-before-arrangrr same-tick harmonic visibility); the
Phase-5 execution plan's own framing of Item A (`transform policy inside
arrangrr`, `[HOST-only]`, `S-core`, overlaps arrangrr core).

Code traced (component → component, not recalled):
- `components/runtime/include/runtime/pipeline.hpp` — the `Pipeline<StageTs...>`
  composite, its `fire_on_tick`/`fire_flush`/`fire_push_midi_in`/`fire_forward`
  fan-out, and the **terminal specialization** that deliberately stops
  forward-flow one level short of the last stage.
- `components/runtime/include/runtime/stage.hpp` — the `StageLike` concept
  (`on_tick`/`flush` only) and `StageContext`.
- `components/runtime/include/runtime/runtime.hpp` — `Runtime<StageT,N>`,
  owner of the one `Transport`+`OutScheduler`, injected by reference.
- `components/runtime/include/runtime/out_scheduler.hpp` — the D29
  `(tick, class_priority, seq_no)` total order; `classify()` keys off MIDI
  message type only, never producer identity.
- `components/midisrc/include/midisrc/midi_source_stage.hpp` — the SMF-replay
  source stage: schedules its own "thru" notes directly into the shared
  `OutScheduler` (`m_scheduler.schedule(...)`, line 128) AND, via the 3-arg
  `on_tick` overload, forwards the same raw wire bytes downstream
  ("forward-flow").
- `components/chorddet/include/chorddet/stage.hpp` (`ChorddetStage`) and
  `components/chorddet/include/chorddet/followed_context.hpp`
  (`FollowedContext`) — physically in `components/chorddet`, C++ namespace
  `arrangrr` (deliberate, documented: "Namespace stays `arrangrr` (minimal
  churn); physical component is `chorddet`, which never depends on arrangrr
  (D43)"). `FollowedContext` is injected by reference into both the writer
  (`ChorddetStage`) and the reader (`arrangrr::Engine`/`ChordEngine`).
- `components/orchestrator/include/orchestrator/accompany.hpp` — names the
  concrete `AccompanyPipeline<N> = Pipeline<midisrc::MidiSourceStage<N>,
  arrangrr::ChorddetStage<kMaxPorts>, arrangrr::Engine>` and the stage-index
  constants every consumer uses.
- `components/arrangrr/include/arrangrr/engine.hpp` — the terminal stage:
  `push_midi_in` (routing + `arp_captures`/`harmony_suppress` fork), `on_tick`
  (fixed fire order: clock → timeline → chord-seq → D53 bar-promote → arranger
  → arp), `flush` (the ONLY drain point of the shared scheduler).
- `components/arrangrr/include/arrangrr/arp/arpeggiator.hpp` —
  `ArpeggiatorEngine`: captures a live note set and re-renders it rhythmically
  (rate/direction/octaves/gate), bounded, no heap. The closest existing
  precedent for "reshape a captured note stream through style-like rhythm
  rules," and it lives **inside** `arrangrr::Engine`, not as its own Pipeline
  stage.
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp` — the NTT
  resolver (`resolve()`), the per-role step pipeline (D40: gather → gesture
  expand → resolve → voice → groove → schedule), `VoicingState`, `GrooveParams`
  — all reused, none of it re-derives pitch from an existing note (it
  generates from authored `StyleEvent` specs against the live chord).
- `components/arrangrr/include/arrangrr/routing/router.hpp` — `route_class_bit`
  (routing filter bits, unrelated to D29's `EventClass`).
- `components/hostrt/shell.hpp:420-460` — the one composition point:
  `hostrt::Shell` owns `FollowedContext`, drives
  `runtime::Runtime<orchestrator::AccompanyPipeline<N>, N>`, and documents the
  "inert-by-default stage, byte-identical when unused" convention that lets
  one Shell/one CLI binary serve both the plain and Accompany shapes.
- `components/arrangrr/CMakeLists.txt`, `components/chorddet/CMakeLists.txt`,
  `components/orchestrator/CMakeLists.txt` — actual link direction:
  `chorddet` → `common`+`runtime` only; `arrangrr` → `common`+`runtime`+
  `chorddet` (PUBLIC, one direction); `orchestrator` → all of the above +
  `midisrc` (HOST-ONLY, explicitly forbidden on the arm-none-eabi branch).
- `tests/arm-smoke/link_gate.cpp` / `main.cpp` — the actual arm-none-eabi smoke
  target drives the 2-stage `Pipeline<ChorddetStage<N>, Engine>`, **not** the
  3-stage Accompany shape (which needs `midisrc`, host-only).

## 1. New Stage vs. policy inside the terminal (`Engine`) — verdict: NEW STAGE

**Recommendation: Restyle is a new declared Pipeline stage
(`arrangrr::RestyleStage`), inserted between `ChorddetStage` and `Engine`,
physically living inside `components/arrangrr` (not a new sibling package).**

Weighed against the actual contract:

- **D53 (same-tick harmonic visibility) is satisfied by either option** — not
  a discriminator. `Pipeline::on_tick` fires every declared stage in fixed
  order every tick (`pipeline.hpp:43-46`); a `RestyleStage` placed after
  `ChorddetStage` sees the same-tick-fresh `FollowedContext` exactly as
  `Engine` already does. A policy inside `Engine` would also fire after
  chorddet (Engine is always last). Either shape is D53-clean.
- **D29 (total order) is satisfied by either option, and needs zero scheduler
  change either way.** `OutScheduler::classify()` (`out_scheduler.hpp:29-40`)
  keys the tie-break on **MIDI message type only** (realtime < NoteOff <
  other < NoteOn), never on which stage produced the event. A `RestyleStage`
  with its own injected `OutScheduler&` reference (the same idiom already
  used three times — Transport, OutScheduler, FollowedContext) drops into the
  same total order with **no new `EventClass`, no ABI change**.
- **The forward-flow seam already fans out to every non-terminal stage,
  unmodified, if Restyle is a new Stage.** `PipelineChain::fire_forward`
  (`pipeline.hpp:182-190`) SFINAE-probes every level's `push_midi_in` and
  recurses — a `RestyleStage` with a `ChorddetStage`-shaped `push_midi_in`
  hook receives `MidiSourceStage`'s forwarded melody bytes **for free**, zero
  Pipeline code touched. This is the single strongest argument for the new-
  stage shape: the seam Restyle needs already exists and already reaches
  exactly one slot short of the terminal — put Restyle in that slot.
- **A policy inside `Engine` would have to break the terminal's own
  documented boundary.** `pipeline.hpp:244-248` states forward-flow stops one
  level short of the terminal **on purpose**: "the terminal already sees this
  same note through the shared `OutScheduler`... routing it a second time
  through arrangrr's OWN `push_midi_in` would apply routing/arp-capture/
  harmony-suppress semantics a melody replay must never trigger." Restyle-as-
  Engine-policy needs exactly the thing this boundary exists to prevent: it
  wants the melody stream treated as *capturable input*, not silent thru. That
  forces either (a) a third fork alongside `arp_captures`/`harmony_suppress`
  inside `Engine::push_midi_in` (`engine.hpp:124-156`) — re-entangling the
  exact god-method Seam C (4b/4c/4d) was built to pull apart — or (b) a
  parallel `Engine::observe_restyle_input(...)`, which still grows the
  terminal's surface every time a new per-tick concern appears, the opposite
  direction from the whole `orchestrator-pipeline-extraction.md` trajectory
  (chorddet was promoted OUT of `Engine` for exactly this reason). The
  Phase-5 plan's own framing ("transform policy inside arrangrr") is right
  about the **package** (arrangrr) but conflates it with the **class**
  (`Engine`); the evidence says: new class, same package, not a new Engine
  fork.
- **`ArpeggiatorEngine` is the load-bearing precedent, and it argues FOR
  "new class, arrangrr package," not for "grows Engine."** It already proves
  "capture a note stream, re-render it rhythmically, bounded, no heap" is a
  self-contained, freestanding *class* (`arrangrr/arp/arpeggiator.hpp`) that
  `Engine` merely *owns and drives* — it is not woven into `push_midi_in`'s
  dispatch beyond the one-line `arp_captures` fork. A `RestyleStage`,
  structured the same way `ChorddetStage` wraps `ChordDetector`, is the
  Pipeline-level version of that same idiom, one composition level higher.

**Why arrangrr the package, not a new sibling like chorddet:** Restyle needs
`VoicingState`, `groove::apply`, `gesture::expand`, `Style`/`StylePattern`,
register-anchor tables — all private/arrangrr-owned today
(`arranger.hpp:365-456`). D43 forbids `chorddet` from depending on `arrangrr`;
it says nothing forbidding a NEW class inside `arrangrr` from depending on
arrangrr's own machinery — the Phase-5 plan itself classifies Restyle as
overlapping arrangrr core (`phase5-execution-plan.md`'s parallelization
table). Building a fifth sibling package (`components/restyle`) that then
needs to link `arrangrr` PUBLIC to reach `VoicingState`/`groove` would
recreate the one-directional `X → arrangrr` edge `orchestrator` already has,
for no boundary gained — pure ceremony. Put it in `arrangrr/restyle/`.

**Composition point:** grow `orchestrator::AccompanyPipeline` into a 4-stage
`Pipeline<MidiSourceStage<N>, ChorddetStage<kMaxPorts>, RestyleStage<...>,
Engine>` (one alias, `components/orchestrator/include/orchestrator/
accompany.hpp`), constructed **inert by default** — the exact "byte-identical
when unused" convention `MidiSourceStage` already established and
`shell.hpp:437-446` documents as the reason one Shell/one CLI binary serves
every pipeline shape. This is additive to the type, not a fork of it; no
second `RestylePipeline` alias, no second Shell wiring path.

## 2. State/context that must flow to Restyle — reuse map

| Restyle needs | Existing seam to reuse | New code |
|---|---|---|
| The input melody's own notes (pitch/velocity/timing) | `MidiSourceStage`'s existing forward-flow (`on_tick(ctx, sink, forward)`, `midi_source_stage.hpp:121-133`) — already fans raw wire bytes to every non-terminal stage | none — `RestyleStage` gets a `ChorddetStage`-shaped `push_midi_in(port, bytes, count, on_steer)` hook, same shape as `chorddet/stage.hpp:83-96` |
| Somewhere to schedule the transformed notes | Inject `OutScheduler<N>&` by reference (the precedent already used 3×: Transport/OutScheduler §14.3, FollowedContext §16.2c) | none — same constructor-injection idiom `ChorddetStage`/`MidiSourceStage`/`Engine` already use |
| The current harmony (deferred per scope-gate, but the seam exists now) | Inject `FollowedContext&` by reference, same object `ChorddetStage` writes and `ChordEngine` reads (`chorddet/followed_context.hpp:28-37`) — same-tick visible by construction (D53), no message lag | none for slice 1 (scope-gated out: "defer melodic reharmonization"); the reference is free to add later without a shape change |
| Voicing/register/groove machinery | `arrangrr::VoicingState` (`arranger/voicing.hpp`), `arrangrr::groove::apply` (`arranger/groove.hpp`), `arrangrr::gesture::expand` (`arranger/gesture.hpp`) — all already public, free-standing types independent of `Engine`/`Arranger` | `RestyleStage` gets its **own** `VoicingState` instance (a distinct voice-leading lineage from the generated-accompaniment one — they must not share memory, they are different musical streams) |
| Target style's rhythm grid / idiom data | `arrangrr::Style`/`StylePattern` compiled tables (`arranger/style.hpp`, `arranger/styles/*.hpp`) — same compiled, flash-mappable (D33) data the `Arranger` already resolves against | none — reused as data, read-only |

Nothing here invents a new cross-stage mechanism. Every piece of state Restyle
needs already has a sanctioned reference-injection seam; the placement work
is entirely about which slot in the existing `Pipeline<...>` gets a new
declared stage, not about inventing new plumbing.

**One real gap, flagged, not solved here:** `MidiSourceStage` unconditionally
schedules its own raw "thru" notes into the shared scheduler
(`midi_source_stage.hpp:128`) — that path is untouched by adding a downstream
`RestyleStage`, so today's design would produce **two** note streams per
source event: the raw thru (on the source's own port, per the file's existing
"give the melody thru its own port/channel" convention) and the restyled
rendition (on `RestyleStage`'s own output port). If both ever reach the same
audible sink, notes double. Two structurally honest fixes, both small:
(a) host-side discipline — never route/mute the raw thru port while Restyle
is active (zero code change, but relies on host wiring discipline, not the
type system); or (b) an additive boolean gate on `MidiSourceStage` (e.g.
`set_thru_enabled(bool)`, guarding line 128's `schedule()` call) so the source
stage itself can suppress the raw path when a downstream transform owns
output — mechanical, HOST-only (`midisrc` never cross-builds arm-none-eabi
regardless), doesn't touch D43 (the flag is transform-agnostic, `midisrc`
stays name-blind to `arrangrr`/Restyle). **(b) is the cleaner fix and belongs
to the implementor, not this review** — flagged for Nazzareno.

## 3. ABI surface — verdict: NONE required for the scope-gated first slice

The scope gate (`phase5-execution-plan.md`, Item A) is explicit: "start with
rhythmic re-quantization + register/voicing; defer melodic reharmonization."
For exactly that slice:

- **The transformed notes need no new `OutEvent`.** `RestyleStage`'s `flush()`
  is a no-op (same convention as `ChorddetStage`/`MidiSourceStage`) — the
  scheduler is producer-agnostic (`ScheduledEvent` carries no producer tag,
  `out_scheduler.hpp:43-49`), and `Pipeline` delegates `flush()` solely to the
  terminal (`pipeline.hpp:214-217,234-237`). `Engine::flush()`
  (`engine.hpp:272-277`) drains the shared scheduler and emits the existing
  `OutEvent::midi(...)` for every due event regardless of which stage
  scheduled it — restyled notes surface through the **existing** wire event,
  the same way `Arranger`-generated notes already do. Zero new ABI surface
  for output.
- **Target-style/quantize-grid selection can be a construction-time/L1-verb
  parameter, not a live wire `Command`.** Given the plan's own `[HOST-only]`
  tag and the precedent `midi-source load <path>` already set (a Shell L1
  verb, resolved once, `shell.hpp`/`orchestrator-pipeline-extraction.md
  §16.7`), a `restyle <style>` verb selecting `RestyleStage`'s target at load
  time needs no ABI at all — it is a host-tool argument, exactly like
  `midi-source load`'s path argument.

**Deferred, NEEDS-DECISION, not settled here:** if a later slice wants a
GUI-live-toggleable restyle target/groove (not just load-time), that DOES
want a wire `Command` — natural shape is additive, an `ArpField`-style
selector enum (`Command::kRestyle` + a `RestyleField`), reusing the existing
`Param`-selector idiom (`ArpField`, `GrooveField`) rather than inventing a
new reshape. Even with the ABI freeze lifted, there is no structural reason
to reshape `Command`/`OutEvent` wholesale for Restyle — that license is better
spent on items that actually need a shape change (#2 Clip, #9 Pad/Scene
persisted state). **Recommendation: do not spend the reshape budget here.**
This is a flag, not a decision — the owner picks when/if live-tweak lands.

## 4. Dual-target doctrine — what must not break arm-none-eabi

The doctrine holds (only the ABI-*shape* freeze lifted, per the owner
directive quoted in the execution plan) and nothing in this placement
requires touching it:

- **`RestyleStage`'s own header must be written freestanding-clean** —
  bounded static storage (`StaticVector`/fixed arrays, the same discipline
  `VoicingState`/`NoteReq group[kMaxVoiceNotes]` already use in
  `arranger.hpp:315,371`), no heap, no exceptions/RTTI. This is enforced
  structurally the instant the file lives under `components/arrangrr/include`
  — `arrangrr`'s `CMakeLists.txt` applies `-fno-exceptions -fno-rtti
  -fno-threadsafe-statics` **PUBLIC**, so every dependent TU (including a
  future firmware one) inherits it. Writing it clean is still the
  implementor's discipline, not automatic from the flag alone.
- **The concrete instantiation stays host-only, and that's fine, but it must
  stay a property of the *pipeline*, not smuggled into the *class*.**
  `RestyleStage`'s only real-world producer today is `MidiSourceStage`, which
  is unconditionally HOST-ONLY (heap, `std::vector`/`std::string`, file I/O —
  `midi_source_stage.hpp` header comment). The 4-stage
  `AccompanyPipeline`-with-Restyle is therefore transitively host-only when
  actually wired — same as today's 3-stage Accompany. That is a property of
  `components/orchestrator` (already marked HOST-ONLY, forbidden on the
  arm-none-eabi branch in its own `CMakeLists.txt`), not of `RestyleStage`
  itself. Keep `RestyleStage`'s own header portable anyway (matching
  `ChorddetStage`/`ArpeggiatorEngine`'s existing discipline) so a *future*
  non-SMF, live-input restyle mode (not in scope now) never needs a rewrite.
- **Do not let `RestyleStage` grow into `components/orchestrator`.** If any
  part of the transform ends up needing `std::vector`/heap-shaped state (e.g.
  buffering a lookahead window across notes), that state belongs at the
  `orchestrator`/host level (already HOST-ONLY, already forbidden on
  firmware), never inside the `arrangrr`-package `RestyleStage` class itself
  — keep the heap-shaped and no-heap-shaped halves on opposite sides of the
  `arrangrr`/`orchestrator` boundary that already exists, don't blur it.
- **`tests/arm-smoke/link_gate.cpp` stays untouched and stays the real gate.**
  It drives the 2-stage `Pipeline<ChorddetStage<N>, Engine>` today and will
  keep doing so — a `RestyleStage` never needs to appear there, since its only
  real producer (`MidiSourceStage`) is already excluded from that smoke
  target. If a firmware-relevant restyle scenario is ever proposed, it needs
  its own smoke pipeline instantiation to prove the cross-build — not implied
  by this Item A slice.

## 5. What is flagged for the owner / not decided here

- **The `MidiSourceStage` raw-thru-vs-Restyle double-note risk (§2)** — a
  small additive fix (`set_thru_enabled(bool)`), HOST-only, no doctrine
  conflict, but it is a scope call: is muting the thru port at the host layer
  acceptable for the first slice, or does Nazzareno add the gate now? Not a
  dependency/ABI question, just sequencing — flagged, not decided.
- **Whether Restyle ever needs a live wire `Command` (§3)** — deferred by the
  scope gate today; when/if a live-tweak slice lands, the shape is additive
  (`ArpField`-style selector), not a reshape. No new dependency either way.
- **No new dependency is proposed anywhere in this placement** — `arrangrr`,
  `chorddet`, `runtime`, `midisrc`, `orchestrator` link exactly what they link
  today; a `RestyleStage` inside `arrangrr` adds no new `target_link_libraries`
  edge. No `0800` flag applies to this item.
- **This review does not adjudicate the musical transform itself**
  (rhythmic re-quantization grid, which voicing rule set, articulation
  mapping) — that is Ottorino's review B. This document is placement/shape
  only.

# Generative motif engine (#7, node `9210`) — structural placement review

Author: Corelli (architecture critic). Read-only review; Item B of
`docs/design/phase5-execution-plan.md` (as committed on `gui-sonotron`,
`6f7a424`, read via `git show gui-sonotron:docs/design/phase5-execution-plan.md`
— not yet merged into this worktree's branch). Companion document:
`docs/design/restyle-placement.md` (Item A / #1 Restyle, `9320`), read for
coherence, not amended.

## 0. Scope and what was traced

**Decisions read** (`docs/DESIGN.md`): `0100`/D16 (seeded-PRNG determinism,
"same seed ⇒ same output," no PRNG *state* required — a position hash is
enough), `0200` (no heap, bounded, `constexpr`-first), `0300` (dual-target,
core cross-builds arm-none-eabi), `0400` (STM32 budget), `0700`/`0800` (ABI
discipline / dependency policy), `3120`/D40 (resolution pipeline: gather →
gesture::expand → resolve → voice → groove), `9200`/`9210` itself ("Motif +
transforms (diatonic transpose/retrograde/displacement, seeded) — ○
SHIPPABLE"), `9320` (Restyle, "○ HOST-ONLY"), the anti-sameness-arc framing at
line 1096 ("`9210` (generative style) + `9320` (Restyle). The anti-sameness
arc's second half"), and `10000`/D37 (Generative Director, the future
parameter-piloting capstone — groove.hpp's own comment already names it as
the consumer of `GrooveParams`).

**Code traced** (component → component, read not recalled):
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp` —
  `Arranger::on_tick` (lines 229–363): the fixed per-role, per-step loop that
  IS the `3120` pipeline. For each `StylePattern`, for each `StyleEvent`
  matching this step's grid slot: `gesture::expand` → `resolve()` → collect
  into a bounded `NoteReq group[kMaxVoiceNotes]` (line 315, `ARR_ASSERT`-
  guarded overflow, `kMaxVoiceNotes=16`) → `m_voicing.voice(...)` →
  `groove::apply(...)` → `schedule(...)`. Nowhere in this file does
  `runtime::Pipeline<StageT...>` appear — this is a private, hand-written
  in-component transform chain, a different mechanism from the Pipeline/Stage
  machinery entirely.
- `components/arrangrr/include/arrangrr/arranger/gesture.hpp` — the load-
  bearing precedent. Its own header comment (lines 7–18) states the exact
  contract a generative addition must meet: "the generative layer above the
  NTT note vocabulary... produces StyleEvent *specs* (not resolved pitches):
  the caller resolves each spec through the unchanged, wrong-note-proof
  kernel... any output must be bounded (no heap) and reproducible (seed by
  position, like the arp's D16 hash)." `gesture::expand` takes one
  `StyleEvent` and the live `ChordState`, and emits 1..`kMaxGestureFan` (8)
  `StyleEvent`-shaped specs deterministically, with zero mutable state beyond
  its own stack locals.
- `components/arrangrr/include/arrangrr/arranger/style_model.hpp` —
  `StyleEvent` (10-byte POD, `static_assert(sizeof(StyleEvent)==10, ...
  D33)`), `NoteSource` (`kChordTone`/`kScaleDegree`/`kInterval`), `RolePolicy`
  (`kFixed`/`kChordTone`), `ChordGesture`, `StylePattern` (`Span<const
  StyleEvent> events`, `VoicingPolicy`), `StyleSection`, `Style`. Three prior
  extensions (`NoteSource`, `ChordGesture`, `VoicingPolicy`) all followed the
  same additive discipline: "kept last with a default so existing tables stay
  valid, fits existing padding (no size growth)."
- `components/arrangrr/include/arrangrr/arranger/groove.hpp` —
  `groove::hash` (lines 52–60): the concrete D16 idiom in production: a pure
  `constexpr` position hash `(seed, tick, role, step) -> uint32`, no state
  carried, "same inputs always yield the same value... no PRNG state to
  carry or reset" (header comment line 51).
- `components/arrangrr/include/arrangrr/arranger/voicing.hpp` —
  `VoicingState`: one instance per `Arranger`, internally indexed by
  `TrackRole` (`m_last[kRoleCount][kMaxVoiceTones]`, `m_count[kRoleCount]`),
  reset on style load/transport start. Confirmed role-scoped, not
  per-pattern — any producer feeding a role's slot shares that role's
  existing voicing memory by construction.
- `components/arrangrr/include/arrangrr/arp/arpeggiator.hpp` —
  `ArpeggiatorEngine::hash` (lines 195–201): a SECOND, near-identical,
  independently hand-rolled position-hash function (same shape: `seed *
  2654435761u + step`, xor/shift/mult chain), confirming the D16 idiom is
  copy-pasted per module today rather than shared.
- `components/arrangrr/include/arrangrr/engine.hpp` — `fire_arranger`
  (lines 481–503): `Arranger::on_tick`'s `schedule` callback resolves to
  `schedule_pattern(port, delay, msg, sink)`, which (traced via
  `components/runtime/include/runtime/out_scheduler.hpp`, `classify()`,
  lines 29–41) enters the SAME shared `OutScheduler` total order (D29) as
  every other arranger-produced note, keyed on MIDI message type only, never
  on producer identity — confirmed no new `EventClass` is needed for a new
  in-arranger note producer.
- `components/runtime/include/runtime/pipeline.hpp` /
  `components/runtime/include/runtime/stage.hpp` — the `Pipeline<StageT...>`
  composite and `StageLike` concept (`on_tick`/`flush`, `push_midi_in`
  forward-flow one level short of the terminal). This is the machinery
  `docs/design/restyle-placement.md` places `RestyleStage` into, precisely
  because Restyle transforms an ALREADY-EXISTING external note stream that
  must be sourced from another component (`midisrc::MidiSourceStage`) and
  composed across component boundaries. Read to confirm it is architecturally
  a different granularity from `3120`'s intra-`Arranger` per-tick loop, not a
  more-generic version of it.
- `components/orchestrator/include/orchestrator/accompany.hpp` — the concrete
  `AccompanyPipeline<N> = Pipeline<MidiSourceStage<N>, ChorddetStage<...>,
  Engine>` (3-stage today, 4-stage with `RestyleStage` per the Restyle
  placement doc) — confirmed this alias's only reason to exist is composing
  HOST-only external sources; `Arranger` (inside `Engine`, the pipeline's
  terminal) never appears as an independent `Pipeline` stage itself.
- `tests/arm-smoke/link_gate.cpp` — the arm-none-eabi smoke target drives
  `Pipeline<ChorddetStage<N>, Engine>` (2-stage, no `midisrc`). `Engine`
  (hence `Arranger`, hence `3120`, hence any motif producer living inside it)
  is already proven to cross-build today; nothing about this placement adds
  risk to that gate.

## 1. Verdict — a producer feeding the existing resolver, NOT a new resolver
stage, NOT a Pipeline stage

**Recommendation: the motif engine is a new *gather-phase producer* inside
`Arranger`'s existing `3120` loop — architecturally the same slot
`gesture::expand` already occupies — emitting `StyleEvent`-shaped specs that
flow UNCHANGED through the existing `resolve() → VoicingState::voice() →
groove::apply()` kernel. It is neither a new stage alongside gesture/voicing/
groove that duplicates their logic, nor a `runtime::Pipeline<StageT...>`
stage.**

Weighed against the options the task poses:

- **Not "a new resolver stage inside per-role resolution, alongside
  gesture/voicing/groove."** `resolve()`, `VoicingState::voice()`, and
  `groove::apply()` are each a distinct TRANSFORM over an already-produced
  note-spec (chord-tone-index→pitch, re-voice, humanize/swing). A motif
  engine does not transform an existing spec at any of those three stages —
  it *originates* the spec in the first place, in the exact slot
  `pattern.events` (authored, literal) and `gesture::expand`'s *input*
  already occupy: the "gather" half of `3120`, upstream of resolve/voice/
  groove, not a fourth peer stage downstream of them. Placing it as a peer
  stage after groove, say, would make no musical sense (groove reshapes
  timing/velocity of an already-pitched note; a motif has not chosen a pitch
  yet) — the *only* structurally coherent slot is "gather."
- **Is "a producer feeding the existing resolver."** This is the evidenced
  answer. `gesture::expand`'s own header comment (quoted above) already
  describes precisely this contract for a *different* generative concern
  (chord-tone fan-out): produce `StyleEvent` specs, bounded, position-seeded,
  no heap, and let the unchanged kernel resolve/voice/groove them. A motif
  producer — `motif::generate(pattern, step, section_repeat, transform,
  seed) -> StyleEvent specs[]` — is the same idiom one level upstream:
  instead of reading `ev.step`/`ev.tone` literally off `pattern.events`, a
  motif-driven pattern computes them from a stored motif shape + a
  transform (diatonic transpose / retrograde / displacement) + a seed. Both
  gesture expansion and motif generation are then "producers of `StyleEvent`
  specs that resolve/voice/groove never has to know exist" — genuinely one
  seam, reused twice, not two seams invented.
- **Not "a new Pipeline Stage (like `RestyleStage`)."** `runtime::Pipeline
  <StageT...>` composes INDEPENDENT COMPONENTS around a shared per-tick fan-
  out and a shared `OutScheduler`/`FollowedContext` reference — its entire
  reason to exist (per `restyle-placement.md` §1, confirmed by re-reading
  `pipeline.hpp`) is stitching together content that ARRIVES from outside the
  terminal (raw SMF replay, detected chords from an external melody). A
  motif engine needs no external arrival: it is driven entirely by the style
  data already resident in `Arranger` (compiled `Style`/`StylePattern`
  tables) plus the SAME `Key`/`ChordState` `Arranger::on_tick` already
  receives as parameters. There is no second component to compose with, so
  there is no Pipeline-shaped seam to use — introducing one anyway would be
  ceremony without a boundary gained, the same objection `restyle-placement.md`
  raised against a hypothetical fifth sibling `components/restyle` package.

**Why this is also the SAFER classification, not just the tidier one:** the
`Pipeline<StageT...>` shape, wherever it is concretely instantiated today
(`AccompanyPipeline`), is transitively HOST-ONLY, because its one real
external producer (`midisrc::MidiSourceStage`) is unconditionally host-only
(heap, `std::vector`, file I/O). `docs/DESIGN.md` classifies `9210` itself as
**"○ SHIPPABLE"** — core, dual-target, must cross-build arm-none-eabi (it is
explicitly the on-device generative feature, confirmed by the task). Forcing
Motif into the Pipeline-stage shape would silently inherit Restyle's
HOST-only transitive dependency shape (there being no non-host-only producer
to feed it through that mechanism today) — a real demotion of a locked
SHIPPABLE decision to HOST-only by construction, not by owner choice. Placing
it instead inside `Arranger`'s own `3120` loop keeps it exactly where
`ArpeggiatorEngine` and `gesture::expand` already prove this class of
generative, no-heap, position-seeded logic cross-builds today (`tests/arm-
smoke/link_gate.cpp` already exercises `Engine`, hence `Arranger`, on the real
target).

## 2. Coordination with #1 Restyle — two mechanisms, deliberately not one

**The task asks explicitly whether #1 and #7 should compose as a 5-stage
Pipeline. The answer, grounded in the evidence above, is no — and forcing
them together would be a structural mistake, not a simplification.**

| | #1 Restyle (`9320`) | #7 Motif (`9210`) |
|---|---|---|
| Input | An ALREADY-EXISTING external note stream (an imported SMF melody) | Nothing external — a stored motif shape + the live `Key`/`ChordState` `Arranger` already has |
| Mechanism | New `runtime::Pipeline` stage (`arrangrr::RestyleStage`), composed between `ChorddetStage` and `Engine` in `orchestrator::AccompanyPipeline` | New gather-phase producer inside `Arranger::on_tick`'s existing `3120` loop, feeding `resolve()`/`voice()`/`groove()` unchanged |
| Package | `components/arrangrr` (new class, `arrangrr/restyle/`), instantiated by `components/orchestrator` | `components/arrangrr` (new class, e.g. `arrangrr/arranger/motif.hpp`), instantiated by `Arranger` itself |
| Feasibility | **HOST-ONLY** by construction (its producer, `MidiSourceStage`, is unconditionally host-only) | **SHIPPABLE** — must cross-build arm-none-eabi, exercised by the same smoke path as today's `Engine` |
| Why the shape differs | Restyle transforms a stream that only exists because a HOST tool imported a file — inherently a cross-component composition problem | Motif generates from data already resident in the core — inherently an intra-component data-source problem |

They share ONE real seam: both eventually emit ordinary `StyleEvent`-shaped
notes that flow through the same `resolve()`/`voice()`/`groove()` kernel and
the same `OutScheduler` (D29) — that kernel is the genuine common
infrastructure, and it needs no change for either. Beyond that, they are
different mechanisms at different architectural levels for a structurally
sound reason (input provenance), not two ad hoc solutions to the same
problem. **Do not build a unifying "5-stage Pipeline"**: it would (a) buy no
shared code neither mechanism already lacks, (b) force Motif's SHIPPABLE
classification to ride on Restyle's HOST-ONLY producer, and (c) blur the one
distinction (`9210` generates; `9320` re-clothes) the roadmap itself uses to
justify shipping both under one "anti-sameness arc" heading (`DESIGN.md`
line 1096) — they are siblings in INTENT, not in MECHANISM, and the tree
should say so by keeping them structurally separate.

## 3. Seeded determinism (D16) and dual-target/no-heap

- **Reuse the exact `groove::hash`/`ArpeggiatorEngine::hash` idiom**: a pure
  `constexpr` function of `(seed, position)` — no PRNG object, no carried
  state, "same seed ⇒ same output" by construction, not by discipline. For
  Motif, "position" naturally decomposes to `(pattern.role, step,
  section_repeat_count)` — the same granularity `groove::apply` already
  keys on (`tick, role, step`), so a transform like "transpose +1 diatonic
  step every repeat" or "retrograde every other repeat" is `f(seed,
  section_repeat_count) -> transform-amount`, computed fresh every tick from
  a small counter, not accumulated drift.
- **State budget**: at most one small per-pattern counter (which repeat of
  the section this is, needed to know the current transpose/retrograde
  phase) — bounded, `Arranger`-member-resident, reset on style load exactly
  like `VoicingState::reset()` and `m_section_start` already are. This is
  categorically NOT proportional to note count or heap-shaped; it is smaller
  state than `VoicingState` already carries per role.
- **No heap anywhere in this path**: motif specs are produced into the SAME
  stack-local buffers `gesture::expand` already writes
  (`StyleEvent specs[gesture::kMaxGestureFan]` at `arranger.hpp:321`, or an
  equivalently-sized motif-local buffer) — no new dynamic allocation, no new
  container type needed; `kMaxVoiceNotes`'s existing `ARR_ASSERT`-guarded cap
  (`arranger.hpp:329-332`) already bounds the worst case regardless of
  whether a step's events are authored, gesture-expanded, or motif-generated.
- **Flag, not a defect**: `groove::hash` and `ArpeggiatorEngine::hash` are
  two independently hand-written, nearly identical position-hash functions
  today. A third hand-rolled copy for Motif would make it three. This is a
  minor, real DRY smell (evidenced duplication, not a guess) — worth
  extracting to one shared `constexpr` utility (e.g.
  `arrangrr/common/seeded_hash.hpp`) the day Motif is implemented, purely
  as internal tidying. **Not load-bearing to the placement verdict, not a
  dependency, HOST-and-device both fine either way** — flagged for
  Nazzareno at implementation time, not a gate.
- **Dual-target verdict, stated plainly**: this entire mechanism lives inside
  `components/arrangrr`, which already applies `-fno-exceptions -fno-rtti
  -fno-threadsafe-statics` PUBLIC to every dependent TU (per the same
  reasoning `restyle-placement.md` §4 already traced for that package's
  build flags) — a motif producer written with the same discipline
  `gesture.hpp`/`groove.hpp`/`arpeggiator.hpp` already demonstrate inherits
  freestanding-cleanliness structurally, not by hope. It never needs to
  touch `components/orchestrator`, `components/midisrc`, or any HOST-only
  package — unlike Restyle, it has no host-only producer to be transitively
  host-only THROUGH.

## 4. ABI — no new surface required for generation itself; an additive
selector if the GUI wants live control

- **Output needs zero new ABI.** Motif-generated notes resolve to ordinary
  MIDI note-on/off scheduled through the existing `Arranger::on_tick`
  `ScheduleFn` → `OutScheduler` → existing `OutEvent::midi(...)` path,
  exactly like every other arranger-produced note today. No new `OutEvent`
  variant, no `EventClass` change.
- **If the motif shape/seed is fixed at style-load time only** (authored in
  the compiled `Style` table, changed only by loading a different style or
  variant), it needs **no ABI at all** — the same "zero live surface" answer
  `restyle-placement.md` gives for Restyle's target-style selection.
- **The task explicitly asks about exposing/seeding the generator from the
  GUI** — that DOES want a live wire surface, and the codebase already has
  the exact idiom for it: `GrooveField`/`ArpField` are additive
  `enum class` field-selectors paired with a `set_field(...)` clamp function,
  each requiring only one `Command` variant plus the selector enum (no
  reshape of `Command`/`OutEvent` itself). **Recommendation: add
  `Command::kMotif` + a `MotifField` selector** (candidates: `kEnabled`,
  `kTransformKind` [transpose/retrograde/displacement], `kAmount`, `kSeed`,
  mirroring `GrooveField::kSeed`/`ArpField::kSeed` exactly) the day a live
  slice is implemented. This is additive even under the OLD append-only ABI
  discipline, so it does not need to spend any of the Phase-5 ABI-unfreeze
  license — the same restraint `restyle-placement.md` §3 recommends for
  Restyle ("do not spend the reshape budget here").
- **This also matches the roadmap's own trajectory**: `groove.hpp`'s header
  comment already names `GrooveParams` as "the first parameter layer the
  future Generative Director (`D37`/`10000`) will drive," and `10000`'s own
  entry (`10300`) wants "parameter-delta drive into arranger/style/
  sequencer/arp inputs" through a uniform tunable surface. A `MotifField`
  selector shaped exactly like `GrooveField`/`ArpField` is what lets the
  eventual Director pilot Motif's transform/seed the same way it will pilot
  groove and arp — an L2/L1 field-selector surface, not a one-off shape.

## 5. What is flagged for the owner / not decided here

- **The motif-data authoring shape is a real NEEDS-DECISION, not solved by
  this placement.** `StyleEvent` is pinned at 10 bytes
  (`static_assert(sizeof(StyleEvent) == 10, ...)`) and has already absorbed
  three additive fields (`NoteSource`, `ChordGesture`, `VoicingPolicy`) "kept
  last... fits existing padding." Whether a motif is (a) a NEW parallel data
  type alongside `StylePattern` (e.g. a `MotifPattern` with `Span<const
  MotifStep>`, requiring `StyleSection` to carry a second/tagged pattern
  list), or (b) folded into `StylePattern`/`StyleEvent` itself as another
  `RolePolicy`/`NoteSource`-style tag, is a data-model call this document
  does not make — it is exactly the kind of shape decision
  `restyle-placement.md` deferred for its own live-tweak slice, and belongs
  to the same review class (Ottorino for the musical vocabulary the motif
  transforms should support; this reviewer only if the shape question
  becomes structural again once a concrete proposal exists).
- **Whether Motif ever wants to CAPTURE a live-played phrase** (not just
  transform an authored/stored shape) is out of scope for `9210`'s own
  wording ("diatonic transpose/retrograde/displacement" of something already
  defined) but would be a natural follow-on. If it comes, the precedent is
  `ArpeggiatorEngine`'s note-capture idiom (`note_on`/`note_off`, held-set
  state) — still intra-`Engine`, not Pipeline-shaped — flagged for whenever
  that scope is actually proposed, not decided now.
- **The shared position-hash utility (§3)** — an internal tidy-up, no
  dependency, no ABI, not a gate on this feature; flagged for the
  implementor.
- **No new dependency is proposed anywhere in this placement.** `arrangrr`
  links exactly what it links today; a motif producer inside it adds no new
  `target_link_libraries` edge. No `0800` flag applies.
- **This review does not adjudicate the musical transform vocabulary itself**
  (which motif shapes ship, how "diatonic transpose" degree-maps against a
  non-diatonic key, how many repeats before retrograde fires) — that is
  Ottorino's territory, mirroring the Restyle review's own boundary.

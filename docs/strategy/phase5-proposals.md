# Phase 5 candidates — 10 directions after the pipeline extraction

Status: **STRATEGY PROPOSAL (Verdi, 2026-07-13)**. Ground-truth as read on branch
`gui-sonotron`, HEAD `365a748`. Phase 4 (Accompany, node `9310`) and Pipeline P0
(`kChordFollowed`/`kBeat`) are the milestones this proposal follows; Phase 5 itself is
undefined in any existing doc — this file's job is to populate that gap with ten
genuinely different candidates, not ten variants of one idea. Nothing here edits
`docs/DESIGN.md`; recording a chosen direction into the canonical tree remains the
owner's act.

---

## 0. Ground truth (what is actually built, not the plan's optimism)

- **Phase 4 (Accompany, `9310`) is DONE end to end**, all five sub-phases committed:
  `8a0f701` (4a, `Pipeline<StageT...>` in `components/runtime`), `65f16bc` (4b,
  `ChordDetector`+`FollowedContext` promoted to `components/chorddet`), `85cd454`
  (4c, SMF parser + MIDI-source stage into `components/midisrc`), `17f8f43` (4d,
  `components/orchestrator` instantiates the concrete 3-stage pipeline), `2e55d9b`
  (4e, chord detection driven from the imported melody), `365a748` (GUI accepts
  `midi-source load`). A musician can feed a plain SMF melody in and get a genre
  band under the detected chords — golden `accompany_melody_detect` passes.
- **Pipeline P0 is DONE**: `52008e4` (`kChordFollowed`, harmonic visualizer) and
  `ba568ca`/`f4c6188` (`kBeat`, live transport/playhead). The GUI is no longer only
  mechanically wired (G0–G3, `docs/design/gui-fase2-mechanical-plan.md`) — it is
  *live*: the harmony visualizer and the playhead now move from real engine state,
  not placeholders.
- **The tree is a real composable dual-target pipeline**: `components/runtime`
  (Transport, `OutScheduler`, `Pipeline<Stage...>`) is dual-target and unconditional
  in the top `CMakeLists.txt`; `components/chorddet` is also dual-target and
  unconditional; `components/midisrc` and `components/orchestrator` are host-only
  (correctly excluded from the `arm` preset). Three frontends share one ABI
  (`apps/gui-sonotron`, `apps/tools/cli-arrangrr`, `apps/sonotron-server`), and the
  ABI is frozen v1 (`test_abi_frozen.cpp`, node `11720`).
- **Dual-target reality, checked, not assumed**: the `arm` CMake preset
  (`cmake/toolchains/arm-cortex-m7.cmake`) builds `common`/`runtime`/`chorddet`/
  `arrangrr` + `tests/arm-smoke` only — a cross-compile smoke test, **never run on
  real silicon**. `12100` (STM32H743 HAL), `12300` (watchdog), `12400` (physical UI)
  are all `○ planned`; `12200` (budget validation) is `◑`: *"asserts exist; no HW
  run"* (DESIGN.md line 1009). There is no firmware binary anywhere in the tree.
- **Real external MIDI I/O already ships** — `components/hostrt/alsa_midi.{hpp,cpp}`
  is a working ALSA backend (*"ALSA first, others behind the same HAL later"*), so
  "talk to real hardware synths on Linux" is not a Phase-5 gap; it is already true.
- **`melodd` and `samplrr` are empty slots** (`components/melodd/README.md`,
  `components/samplrr/README.md`: *"SLOT — no code yet"*) — captured directions
  (`0910`), zero code, zero CMake wiring.
- **The product-identity documents have moved further than the canonical roadmap
  has caught up with.** `docs/product-identity.md` and
  `docs/design/workstation-vision.md` (owner-validated 2026-07-07) rename the outer
  product **sonotron**, retire "MIDI-only" as the *product* slogan (arrangrr itself
  stays MIDI-only/STM32-capable), and add a three-tier vision — Director/Intention
  Engine → Arranger → Engines (MIDI + audio/VST/sampler) — that `docs/DESIGN.md` §22
  (the "canonical tree") still represents only as `0910` ("captured direction… not
  scheduled") and `10000` (Director, capstone, last). `docs/design/flows.md` already
  admits the gap in its own words: *"the intention/conductor idea of flow #3 is
  demoted… to an optional, read-only side rail… pending the Director (node `10000`,
  planned CAPSTONE — not built)"*.
- **`docs/design/hook-interface.md`** is a live, unresolved proposal to reopen the
  frozen v1 ABI (*"free to replace `Op`/`Param`/`Command`/`OutEvent`… wholesale"*)
  under an *"explicit, owner-granted lift"* of the freeze — sitting in the tree with
  no disposition recorded anywhere in `docs/DESIGN.md` §22 or the owner-decisions
  list.
- **A real, studied style corpus already exists and is unused**:
  `../resources/` (`psrtutorial-zips`, `kb`, `extra-sources`, `pages`) plus
  `docs/backlog/yamaha-style-corpus-and-rules.md` (70+ genre pattern rules,
  already researched) — feeding the deferred `9400`/`9430` importer, currently idle.
- **Zero fuzzing harness exists anywhere in the tree** (checked: no
  `LLVMFuzzerTestOneInput`, no AFL artifacts, no `fuzz` target in any
  `CMakeLists.txt`) — this is a fact, not an inference, and it matters more today
  than a week ago: Accompany (`9310`) just turned "load an arbitrary SMF file
  found on a musician's disk" from a design intention into a **shipped, reachable
  capability** through `components/midisrc`.

These facts frame every proposal below: none of the ten invents a need: each is
tied to a real gap, a real shipped capability, or a real quarantined design already
sitting in `docs/backlog/`.

---

## 1. Fractures worth naming before the ten (context, not the main deliverable)

- **F1 — stale canonical roadmap.** `docs/DESIGN.md` §22 (the declared "ONE canonical
  roadmap and decision record") does not mention Phase 4, Pipeline P0, the
  `sonotron`/workstation product-identity pivot, or `hook-interface.md`. A tree this
  far ahead of its own "single source of truth" is a coherence risk for a
  solo-developer project: the next `9310`-scale decision risks being made against a
  doc that no longer describes the product.
- **F2 — the product's own headline pitch is scheduled last.** `docs/product-identity.md`
  names *"reproducible generativity"* as pillar #1 of what makes arrangrr not a
  clone; `workstation-vision.md`'s tier-1 is the Director. The roadmap schedules the
  Director (`10000`) dead last, behind `5000`/`6000`/`8000`, and the GUI has already
  demoted its own Intention rail to read-only pending it (`flows.md`, cited above).
  This is not necessarily wrong — `0500` scope-honesty defends exactly this kind of
  sequencing discipline — but it means the product's stated differentiator has no
  path in the next several milestones, Phase 5 included. Named here so it is not
  accidentally solved by a Phase-5 pick that isn't actually built for it.
- **F3 — an unresolved ABI-reopening fork.** `hook-interface.md` proposes breaking
  `11720`'s freeze; nothing in `DESIGN.md` records whether this is accepted,
  rejected, or still pending. Whatever Phase 5 becomes, if it touches the ABI
  surface at all, this fork should be closed first or explicitly deferred with that
  fact written down — not left ambient.

---

## 2. Ten Phase-5 candidates

### 1. Restyle (`9320`) — transform the input's own parts into the genre idiom

- **Cosa.** Given a melody (or a fuller import), instead of only adding a band under
  the detected chords (Accompany), *transform the input's own material* — melody,
  bass line, whatever parts are present — into the target style's idiom (rhythm,
  voicing, articulation), keeping the harmonic content but not the literal notes.
- **Perché.** It is the second half of the anti-sameness arc the roadmap already
  named (`docs/backlog/style-differentiation-and-generation.md`) and the natural
  "wow #2" after Accompany: a musician imports a plain piano sketch and hears it
  played *as* bossa/funk/latin, not just accompanied. Direct product value for the
  same persona Accompany already targets (someone with a melody, no band).
- **Come sfrutta la pipeline.** Reuses the exact same `Pipeline<MidiSourceStage,
  ChorddetStage, ArrangrrStage>` shape `9310` just proved end-to-end; the new work
  is a transform stage/policy inside `arrangrr`, not new plumbing.
- **Costo/fattibilità.** HOST-ONLY, zero new core dependency. Already the
  *scheduled* next step in the existing sequence (§27 step 8, "behind the line").
  Depends on `9100` (done). Ships-soon, not research.
- **Rischio principale.** Harmonic-preserving transformation of an *existing*
  melodic line is musically harder than laying a band under a *given* chord — the
  wrong-note-proof NTT resolver (`3110`) guarantees harmony, not idiomatic
  rhythm/phrasing transfer. Risk of a "close but off" result that reads as a bug,
  not a style choice, unless scope is kept narrow (start with rhythmic
  re-quantization + register/voicing changes, defer melodic reharmonization).

### 2. The Repeat-Zone clip/launch primitive — make the GUI's own hero zone real

- **Cosa.** Give the already-shipped GUI's "hero" zone (the Live-Loops/Repeat grid,
  `ux-workstation.md` §4.4–§5) a real launchable-cell primitive: `launch`/`stop`/
  `scene-quantize` verbs and a `clip` state event, quantized to the bar/beat
  boundary — lighter-weight than the full Looper (`6000`), closer to "arm a
  pre-existing pattern/section to start on the next boundary."
- **Perché.** The GUI (`11600`) is DONE mechanically and now partially live
  (`kChordFollowed`, `kBeat`), but its own most prominent surface — the grid the
  screen spec calls the hero — is an *honest placeholder*
  (`gui-fase2-mechanical-plan.md`: *"the real *launch* awaits the core clip
  primitive"*). Shipping a musician a GUI whose headline zone doesn't do anything
  yet is a credibility risk for the very artifact just built.
- **Come sfrutta la pipeline.** Extends the existing ABI additively (new `OutEvent`/
  `Command` kinds, same append-only discipline that shipped `kChordFollowed`/
  `kBeat`); no new stage, no new component — it lives inside `arrangrr`+`hostrt`+
  the GUI decoder, the same three layers P0 just proved.
- **Costo/fattibilità.** SHIPPABLE core + HOST-ONLY GUI; small, additive ABI slice
  (`0700`-compliant); no new dependency. Distinct scope from `6000` (record/overdub)
  — a NEEDS-DECISION only in the sense that "how much of `6000`'s launch semantics
  does this borrow" should be settled explicitly, not silently, before coding.
- **Rischio principale.** Scope creep into the full Looper (`6000`) if "launch a
  clip" quietly grows record/overdub/undo along the way — must be kept to
  "arm/launch/stop pre-existing content," with capture left to `6000`.

### 3. STM32 hardware bring-up — first slice of `12100`, on real silicon

- **Cosa.** Stand up the actual STM32H743 HAL (USB-MIDI class-compliant + UART DIN
  + a hardware timer for the tick source) and get the *already-built* dual-target
  core running — playing a style, following a chord — on a real board, not just
  cross-compiling and running `tests/arm-smoke`.
- **Perché.** The entire architecture (§2 of `DESIGN.md`, invariants `0200`/`0300`/
  `0400`) has been justified from day one by "this must run on a chip" — and it has
  never once actually run on a chip. Every SHIPPABLE-labelled leaf in the tree is a
  bet that this is true; the bet has never been tested against reality, only
  against `static_assert` and a cross-compiler. Hobbyist STM32-MIDI projects are a
  real, active scene today (MidiBox/MIOS, Dekrispator, the commercial PGB-1
  groovebox shipped October 2025 with firmware updates into 2026 — see Sources) —
  there is a real audience of makers who would want an open, chord-intelligent,
  dependency-free MIDI brain on a board, but only if the dual-target promise is
  proven, not asserted.
- **Come sfrutta la pipeline.** Directly exercises the unconditional dual-target
  branch of the pipeline (`common`/`runtime`/`chorddet`/`arrangrr`) exactly as
  built — this is validation of existing work, not new musical surface.
- **Costo/fattibilità.** NEEDS-DECISION + real cost: requires buying/choosing actual
  hardware (an STM32H743 dev board — Nucleo-H743ZI or similar, low cost but a real
  purchase decision), and the HAL layer is new device-target code (not SHIPPABLE
  core — it is the `12000` band, `○ device`). No new *library* dependency expected
  (bare-metal HAL can be hand-rolled per `0800`), but ST's HAL/CMSIS headers, if
  used instead of hand-rolled registers, would be a dependency to flag.
- **Rischio principale.** This is the bet whose failure is most consequential
  structurally: if the timing/memory/allocation model that looks clean on paper
  turns out wrong on real hardware (DTCM placement, interrupt latency, USB-MIDI
  descriptor quirks), it invalidates assumptions baked into every SHIPPABLE leaf
  in the tree, not just this one. Running it now, even a thin slice, de-risks
  everything scheduled after it far more cheaply than running it after `5000`/
  `6000`/`8000` have all been built on the same unverified assumption.

### 4. Untrusted-parser fuzzing harness — SMF today, SFF/CASM tomorrow

- **Cosa.** Stand up a libFuzzer/AFL++ harness around `components/midisrc`'s SMF
  parser (the same parser Accompany's `midi-source load` just made reachable from
  an arbitrary user file) and around `apps/tools/arrstyle-converter`'s SFF/CASM
  importer, with a seed corpus and a CI-run fuzz target.
- **Perché.** A week ago, "parse an untrusted MIDI file" was a design intention;
  today, via `9310`, it is a shipped, user-reachable code path
  (`midi-source load`). The product's own MIDI-compliance checklist (`DESIGN.md`
  §9.A) already demands *"robustness: corrupted bytes… discarded without
  crashing"* — but nothing in the tree currently tests that claim against
  adversarial input (checked: zero `LLVMFuzzerTestOneInput`/AFL artifacts anywhere).
  For a project whose engine is written in `-fno-exceptions`/no-heap C++ that also
  parses attacker-controllable binary files, this is not a nicety.
- **Come sfrutta la pipeline.** Targets the exact new surface Phase 4 created
  (`components/midisrc`) plus the existing SFF/CASM import path, both host-only —
  it is quality work *on* what Phase 4/5 candidate #8 create, not a new musical
  feature.
- **Costo/fattibilità.** HOST-ONLY, dev/CI-only. A fuzzing engine (libFuzzer ships
  with clang; AFL++ is a separate host build dependency) would need a `0800` flag
  if AFL++ is chosen over clang's built-in libFuzzer, which needs none beyond a
  clang toolchain already implied by `1130`'s dual-toolchain CI. Ships-soon: a
  first harness on one parser is a day or two of work, not research.
- **Rischio principale.** Low technical risk, but real opportunity cost if
  mis-prioritized as *the* next step rather than a fast parallel track — it
  produces no new thing a musician can play. Its risk is being skipped
  indefinitely, not being hard.

### 5. External clock-in (`4500`) — slave sync to a DAW or hardware clock

- **Cosa.** Implement the receive side of MIDI clock sync: lock the internal
  960-PPQN transport to an incoming external clock (Start/Stop/Continue + a
  PLL/moving-average filter absorbing jitter), so arrangrr can slave to a DAW or
  another sequencer instead of only being the master.
- **Perché.** The roadmap already names this as *"the one pure-interop
  MIDI-engine/sequencer gap; master-out only today"* (`4500`, DESIGN.md line 823) —
  and the product's own central value proposition (§1: *"excellent MIDI timing…
  total MIDI interoperability/compliance"*) is only half-true without it. A
  musician running arrangrr alongside a DAW or a hardware sequencer cannot use it
  as a follower today.
- **Come sfrutta la pipeline.** Pure `1200`-band (Transport/Clock) work, already
  dual-target and unconditional — extends `components/runtime`'s existing
  Transport, no new component.
- **Costo/fattibilità.** SHIPPABLE core, no new dependency, small additive ABI (an
  incoming-clock mode toggle). Medium complexity (jitter/PLL correctness needs
  real testing against real hardware clock sources, tying back to candidate #3's
  bet if pursued together).
- **Rischio principale.** Jitter-absorption correctness is easy to get "mostly
  right" and subtly wrong (drift under real-world clock jitter, not just the
  virtual-clock golden tests) — needs a real external clock source in the test
  loop, not only deterministic golden replay, to be trusted.

### 6. `melodd` first slice — the audio-realization companion, minimally real

- **Cosa.** Turn `components/melodd` from an empty README slot into a minimal
  working host-only audio peer: even a single built-in synth voice (or a thin
  wrapper around one small open-source softsynth) that can realize the MIDI
  arrangrr already emits, wired through the "opaque reference" interface
  `workstation-vision.md` already specifies (arrangrr decides, melodd realizes,
  never embeds).
- **Perché.** `docs/product-identity.md` and `workstation-vision.md` (both
  owner-validated) have already repositioned the *outer* product as sonotron, a
  workstation with audio as *arrangeable color* — not a MIDI-only pitch — but zero
  code exists for it (`melodd`/`samplrr` are literally "SLOT — no code yet").
  Today the only way to *hear* arrangrr's output is the demo launcher's ad hoc
  FluidSynth wiring (`apps/demo/lib/launch.sh`), which is explicitly "cosmetic demo
  tooling," not a product component. If the workstation vision is real, something
  has to start proving the Engines tier exists.
- **Come sfrutta la pipeline.** Consumes arrangrr's MIDI output as a peer via the
  existing name-blind orchestrator pattern Phase 4 just proved (`components/
  orchestrator` composing stages by contract, not by knowing concrete types) — the
  same architectural shape, a different peer.
- **Costo/fattibilità.** HOST-ONLY, and this is the one candidate that most likely
  needs a **flagged core-adjacent dependency** — a softsynth/DSP library (or, if
  hand-rolled, a real audio-callback/PortAudio-class backend) is a `0800` fork the
  owner must approve explicitly; cost it before starting (which library, license,
  maintenance weight).
- **Rischio principale.** Scope risk is the largest of all ten: "audio companion"
  can silently grow into exactly the DAW-gravity the product identity docs spend
  paragraphs refusing (*"NOT a linear audio timeline… mixer… plugin host"*). A
  first slice must be scoped ruthlessly narrow (one voice, no mixing, no editing)
  or it undermines the very identity document that justifies building it.

### 7. Generative motif engine (`9210`) — on-device melodic variation

- **Cosa.** A motif+transform engine (diatonic transpose, retrograde,
  displacement, all seeded/deterministic) that generates or varies melodic
  material procedurally, distinct from Restyle's corpus-idiom transfer — this
  generates new content, Restyle re-clothes existing content.
- **Perché.** It is the procedural half of the anti-sameness arc, and it is the
  only one of the ten that runs identically well on both host and — eventually —
  the device: a genuinely on-chip-capable generative feature, matching the
  product's "reproducible generativity" pillar with something that could actually
  ship inside the STM32 budget, unlike a corpus importer or an audio synth.
- **Come sfrutta la pipeline.** A new resolver stage sitting alongside the existing
  NTT pipeline (`3120` gather→expand→resolve→voice→groove) — architecturally
  small, reuses the seeded-PRNG discipline already established for
  humanize/probability (`0100`).
- **Costo/fattibilità.** SHIPPABLE core, no new dependency, dual-target from the
  start (unlike `9220`'s offline-trained Markov half, whose training stays
  HOST-ONLY). Medium build effort.
- **Rischio principale.** Musical risk, not technical: procedurally-generated
  motifs are easy to make *correct* (no wrong notes, thanks to NTT) and hard to
  make *interesting* — the classic generative-music failure mode is technically
  valid but musically inert output. Needs real musician ears in the loop early,
  not just golden-test correctness.

### 8. Style corpus import pipeline (`9400`/`9430`) — stop hand-authoring styles

- **Cosa.** Build the deferred CASM/SFF→NTT importer body (`arrstyle-converter`
  already has an *inspect-only* SFF importer, `sff_import.cpp`) far enough to pull
  real content from the already-harvested corpus at `../resources/` (documented in
  `docs/backlog/yamaha-style-corpus-and-rules.md`: 70+ genre pattern rules already
  studied) into the compiled-`.cpp` style format the device path already uses.
- **Perché.** All 16 current styles are hand-authored; the anti-sameness work so
  far (`9100`, tempo/feel/groove) improved *feel* but not *variety of content* — the
  real ceiling on "how many genuinely different styles can this ship" is
  authoring bandwidth, and there is already a studied, unused corpus sitting on
  disk. This is the highest-leverage move for raw content variety, and uniquely,
  most of the research cost is already sunk (the corpus study exists).
- **Come sfrutta la pipeline.** Feeds the exact same compiled-style format `3210`/
  `3220` already consume — no new runtime shape, only a new *producer* of the
  existing format, host-only tooling.
- **Costo/fattibilità.** HOST-ONLY tooling, no core dependency, no ABI change.
  Explicitly named in the roadmap as deferred *"until authoring pain is real or
  the corpus import becomes the priority"* (DESIGN.md line 1083) — this proposal
  is the argument that with two anti-sameness rounds (`9100`, and #1/#7 above)
  behind it, that threshold may now be close.
- **Rischio principale.** SFF/CASM is a proprietary-adjacent, messy, real-world
  format (per the corpus study); the importer body is likely the least
  predictable-effort item of the ten — "inspect-only" today for a reason. Also the
  proprietary-corpus provenance of `../resources/` should be checked for
  redistribution/licensing risk before any imported style ships in a public
  build, distinct from the technical risk.

### 9. Live-performance depth — Phrase/Pad engine (`7200`) + Performance/Scene recall (`8100`/`8200`)

- **Cosa.** Banks of 4 performance pads (phrase/chord/drum/CC/fill/scene triggers,
  one-shot/loop/hold/toggle, quantized to the boundary) plus one-button recall of a
  full live state (style, variation, mute, routing, transpose) — the "Registration/
  STS" concept from the Korg/Yamaha analogy already documented in `DESIGN.md` §8.
- **Perché.** The product identity claims 50/50 live and studio use (`0120`), but
  everything shipped since the GUI froze (`9310` Accompany, P0 kChordFollowed/
  kBeat) is studio/composition-flavored (import a file, watch harmony). Nothing
  yet answers "I am on a stage, I need instant recall and a hand full of
  triggers" — the other half of the product's stated identity has had zero
  Phase-4/5-era investment.
- **Come sfrutta la pipeline.** Pads reuse the scheduler exactly as a Pattern
  playback source (`15. Phrase Pads` design already specifies "no separate
  engine"); Scene recall is a snapshot over existing state, not new musical logic.
- **Costo/fattibilità.** SHIPPABLE core for pads/scenes storage; the GUI-side pad
  bank and one-button recall UI is HOST-ONLY. No new dependency. Both are already
  `○ SHIPPABLE` leaves in the tree (`7200`, `8100`–`8200`), just unscheduled behind
  `5000`/`6000` in the advisory "behind the line" order.
- **Rischio principale.** `8200` Performance/Registration touches a lot of engine
  surface (style+variation+mute+routing+transpose all at once) — the risk is an
  ABI/state-model shape that has to be gotten right the first time, since scenes
  will be saved/recalled data, not transient state; a rushed shape here is exactly
  the kind of thing `0700`'s "append-only, never reused" discipline is designed to
  survive, but only if the initial shape is reviewed carefully (Corelli-class
  review), not shipped quickly to hit a "first live demo" deadline.

### 10. MIDI-FX / Transform chain, first increment (`5000`/`5100`/`5200`)

- **Cosa.** Build the composable, bounded (`kMaxInserts=8`) insert-chain framework
  whose shape is already locked at the GUI freeze line, starting with 2–3 inserts
  (echo/delay, note-repeat, scale-lock) and refactoring groove/arp into chain
  instances rather than disconnected modules.
- **Perché.** This is the concrete realization of the product's own `0600`
  "open/hackable, first-class" north-star, and the roadmap itself flags it as
  *"Owner's stated strong current interest"* (DESIGN.md line 1065) — plus it
  strategically widens the tunable surface the Generative Director (`10000`) will
  eventually need to pilot, so building it now is not orthogonal to the capstone
  bet, it is the on-ramp to it.
- **Come sfrutta la pipeline.** This is the one candidate that is architecturally
  *about* the pipeline concept itself — turning today's ad hoc modules (groove,
  arp, scale-lock) into instances of one uniform, inspectable, per-track chain is
  the same "componibile" idea Phase 4 just proved for whole stages, applied one
  level down, inside a single stage.
- **Costo/fattibilità.** SHIPPABLE core, shape pre-fixed (NEEDS-DECISION already
  RESOLVED at `11720`), no new dependency, ABI-none for the data-model increment.
  Medium-large effort (framework + refactor + 2–3 real inserts).
- **Rischio principale.** The refactor of groove/arp into chain instances touches
  code that ships in every one of the 15 differentiated styles (`9100`/`3260`) —
  regression risk on musical feel that already took real work to differentiate;
  must be gated by the existing regression goldens (`feel_swing/shuffle/blues`,
  etc.) staying byte-identical through the refactor, not just "the chain works."

---

## 3. Synthesis

### Grouping by theme

- **Anti-sameness / musical capability** — #1 Restyle, #7 Generative motif, #8
  Style corpus import. Three genuinely different mechanisms (idiom transfer,
  procedural generation, raw content volume) toward the same diagnosed problem
  (`docs/backlog/style-differentiation-and-generation.md`).
- **Product/GUI depth on the just-built instrument** — #2 Clip/launch primitive,
  #9 Live pads/scenes. Both fill holes in what the GUI already promises (the hero
  zone; the "50/50 live" identity) rather than adding new engine concepts.
- **Platform bets** — #3 STM32 bring-up, #6 melodd first slice. The two candidates
  that test whether a *foundational architectural premise* (dual-target; the
  workstation's Engines tier) is real, not just designed.
- **Quality/robustness** — #4 Fuzzing harness. The only candidate that produces no
  new musical surface but closes a door Phase 4 just opened.
- **Interop** — #5 External clock-in. A named, scoped, small gap in the product's
  own compliance checklist.
- **Architecture/hackability** — #10 MIDI-FX chain. The candidate most aligned
  with the owner's stated current interest and the long-term Director dependency.

### Top-3 recommendation and the through-line

If the arc since the GUI freeze line reads *"prove the engine's worth inside the
new GUI"* (which is exactly how `11730` step 6 frames `9310` Accompany:
*"cheapest possible demo of the engine's worth inside the new GUI"*), the
strongest Phase-5 through-line continues that sentence rather than jumping to a
new one:

1. **#1 Restyle (`9320`)** — it is already the scheduled next step (§27/step 8),
   it is the direct sequel to the capability just shipped, it costs nothing new
   (no dependency, no ABI, same pipeline shape), and it is the second half of an
   anti-sameness arc the roadmap already committed to. The cheapest, most
   load-bearing next move.
2. **#4 Fuzzing harness** — run in parallel, not in sequence, because it targets
   exactly the surface Restyle's sibling feature (`9310`) just made reachable and
   costs a day, not a milestone. Doing this *now*, while `components/midisrc` is
   fresh in memory, is cheaper than doing it after two more features have grown
   around the same parser.
3. **#2 Clip/launch primitive** — because the GUI is the newest, most visible
   artifact in the tree and its own headline zone is currently inert; letting a
   musician actually press a cell and hear it launch is a small, scoped, additive
   piece of work with an outsized credibility payoff for the thing just built.

Genuinely absent from this top-3, deliberately: #3 (STM32) and #6 (melodd) are
real, important bets — but they are *platform* bets, not *Phase-5-sized* features;
each deserves its own owner-decision fork (see below) before being folded into an
ordinary next-milestone sequence, because each opens a new front the solo-developer
risk (`0500`) explicitly warns against opening casually.

### The real forks the owner should decide (not blocking, surfaced for the record)

1. **Does Phase 5 stay "grow the GUI-era product" (candidates #1/#2/#7/#9/#10) or
   does it spend the milestone de-risking a foundational bet (#3 STM32 or #6
   melodd)?** Both are legitimate; they are not the same kind of work and
   shouldn't be scheduled as if they were. STM32 bring-up in particular is cheap
   to defer further (nothing currently depends on it having happened) but gets
   more expensive to de-risk the longer the SHIPPABLE-labelled core grows on an
   unverified assumption.
2. **Is the `sonotron` workstation vision (audio/VST/Director) still the intended
   destination, or has it been quietly shelved in favor of the narrower
   MIDI-only arrangrr product `docs/DESIGN.md` §1 still describes?** The tree
   currently holds both descriptions with no reconciliation in the canonical
   roadmap. This matters directly for candidate #6 (melodd): building it assumes
   the workstation vision is still live; skipping it indefinitely is itself a
   quiet answer to this fork that should be made explicit rather than accumulate
   by default.
3. **`docs/design/hook-interface.md`'s ABI-reopening proposal** — accept, reject,
   or explicitly defer, and record which, before any Phase-5 candidate that
   touches the ABI surface (notably #2 and #10) is scheduled, so that work isn't
   built against a freeze that may be about to move.
4. **Reconcile `docs/DESIGN.md` §22 with the tree** (F1). Not a Phase-5 feature,
   but arguably the cheapest single action that reduces the risk of every
   candidate above: the canonical roadmap should reflect Phase 4/P0 and the
   `sonotron` pivot before the next milestone is chosen against it.

---

## Sources (competitive/technical context)

- [PG Music Band-in-a-Box 2026 Pro](https://www.sweetwater.com/store/detail/BIAB26ProW--pg-music-band-in-a-box-2026-pro-for-windows) — the mature commercial accompaniment-software incumbent; now adding AI audio-to-MIDI transcription, underscoring that arrangrr/sonotron's differentiation has to be the deterministic, hackable, embeddable substrate, not "auto-accompaniment" as a category alone.
- [Global MIDI Software Market Analysis 2026](https://www.cognitivemarketresearch.com/midi-software-market-report) — market-trend context (AI-assisted arrangement, subscription shift).
- [Wee Noise Makers PGB-1 groovebox](https://synthanatomy.com/2026/04/wee-noise-makers-pgb-1.html) — an actively-updated (2025–2026) commercial open-ish hardware groovebox, evidence the STM32-class hobbyist/maker hardware market is live, not hypothetical.
- [MrBlueXav/Dekrispator_v2](https://github.com/MrBlueXav/Dekrispator_v2), [retro16/MidiBox](https://github.com/retro16/MidiBox), [MIDIbox Operating System](https://www.ucapps.de/mios.html) — active/maintained STM32-class open-source MIDI hardware/firmware projects, the realistic peer set for candidate #3 if pursued.

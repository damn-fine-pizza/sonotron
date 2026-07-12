# Phase 4 execution plan — Accompany (node 9310)

Status: PREPARED. Corelli-designed (§16 of `orchestrator-pipeline-extraction.md`,
APPROVED WITH CORRECTIONS), placement resolved by Palladio (below). No open
owner-decision fork (the ABI fork resolved to "no reshape" — waiver unspent).
Detail lives in §16; this file is the execution index across the 4 sub-phases.

## What Accompany is
HOST-ONLY, zero new core, zero new deps. Feed a plain SMF melody → get the genre
band playing under the detected chords. Pipeline: `[MIDI-source] → [chorddet] →
[arrangrr]`, wired by a host-only orchestrator; chorddet BEFORE arrangrr so its
harmonic context is visible same-tick (D53); one `OutScheduler` does the D29
total order across stages.

## Placement (Palladio, build-graph-decided)
- **`Pipeline<StageT...>` template → `components/runtime/include/runtime/pipeline.hpp`**
  (NOT orchestrator). `runtime` is the only unconditional target of the two;
  the mechanism is dual-target-safe (reference members, no heap), degenerates to
  the 1-stage case for 4a. Zero CMake delta (runtime is INTERFACE).
- **MIDI-source parser + Stage adapter → new `components/midisrc`** (STATIC,
  host-only, name без hyphen per component convention). `git mv`
  `smf.{hpp,cpp}` + `diagnostics.{hpp,cpp}` out of `apps/tools/arrstyle-converter`
  into `components/midisrc`; `smf.hpp` includes `diagnostics.hpp` so they
  co-move. Links `common`+`runtime`, NOT arrangrr (D43). ~15 arrstyle-converter
  files need include-path-only edits (mechanical). A private `read_binary`
  (`cli.cpp`) must be exported minimally into midisrc rather than duplicated.
- **Concrete Accompany `Pipeline<MidiSourceStage, ChorddetStage, ArrangrrStage>`
  instance → `components/orchestrator`** (when it stops being an empty slot, 4d).
- **chorddet → own `components/chorddet`** (dual-target; ChordDetector is already
  constexpr/no-heap — the real cost is promoting `FollowedContext`).

## Corelli's mandatory corrections (§16.9)
1. ABI waiver stays UNSPENT — `test_abi_frozen.cpp` untouched (no stage/source
   tag needed: scheduler tie-break derives from message content; non-MIDI
   OutEvents bypass the scheduler; same-tick visibility rides a shared-by-ref
   `FollowedContext`, not the wire).
2. The real cost is `FollowedContext` promotion, NOT the detector.
3. **Seam C** (new, must be in the 4b/4c intake): `push_midi_in` today does
   routing AND chord-detect in one method → split with fan-out, each stage its
   own `MidiParser`.
4. `flush()` delegates ONLY to the arrangrr stage (confirmed correct by
   NoteTracker "observes the OUTPUT stream", not merely convenient).
5. Config constraint: `cancel_note_off` tombstones by `(port,channel,note)` blind
   to stage — a configuration discipline, not a defect.
6. Orchestrator composes/names stage instances; it does NOT reimplement time or
   total order (that's runtime's `OutScheduler`).

## Sub-phases (each its own Nazzareno dispatch + green gate + commit)
- **4a — composite, transparent, zero behavior.** Introduce `Pipeline` in
  runtime; the composition drives arrangrr through a 1-stage `Pipeline`. Gate: 18
  goldens byte-identical, ctest green, arm cross-build of common/runtime/arrangrr
  unchanged, `test_abi_frozen` intact.
- **4b — promote chorddet + FollowedContext into `components/chorddet`** (the real
  cost). Includes Seam C fan-out split. Gate: goldens byte-identical (chorddet
  still wired same as today, just relocated), dual-target cross-build green.
- **4c — MIDI-source stage** in `components/midisrc` (the git-mv + 15 include
  edits + the Stage adapter + exported file-read) + a NEW Accompany golden
  category driven by the existing `.acmd` grammar (new L1 verb `midi-source
  load`). Gate: existing goldens byte-identical, new Accompany goldens pass.
- **4d — orchestrator stops being a placeholder**: `components/orchestrator`
  instantiates the concrete 3-stage Accompany pipeline; sonotron-server / GUI can
  select it. Gate: Accompany end-to-end (melody in → band under detected chords),
  goldens green.

## Constraints (every sub-phase)
`common`/`runtime`/`arrangrr`/`chorddet` stay freestanding no-heap; `midisrc` and
orchestrator-instance are host-only; codebase-memory-mcp first; rtk; no new deps;
no commit by the implementor. LSP diagnostics on apps/gui-sonotron are false
positives — verify via cmake+ctest.

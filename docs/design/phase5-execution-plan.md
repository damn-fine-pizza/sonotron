# Phase 5 execution plan — the owner-ordered 8-item program

Status: **ORDERED + ABI-UNFROZEN (owner, 2026-07-13).** From the ten Verdi
candidates (`docs/strategy/phase5-proposals.md`) the owner selected **eight**, in
this execution order:

> **`1 → 7 → 8 → 2 → 9 → 6 → 4 → 10`**
> Restyle · Motif generativo · Import corpus · Clip · Pad/Scene · melodd ·
> Fuzzing · MIDI-FX chain

**Deferred (not in this program):** **#3 STM32 bring-up** and **#5 external
clock-in** — platform/interop bets, each its own future owner-decision fork.

**Owner directive — the ABI freeze is LIFTED for Phase 5.** Verbatim intent: *in
this phase don't ever worry about breaking the ABI — you may break it, rewrite it
from scratch, whatever you want.* This **resolves fork F3** (`hook-interface.md`
ABI-reopen = ACCEPTED) and retires the append-only-only discipline for Phase-5
work. `test_abi_frozen` may be rewritten or removed; `Op`/`Param`/`Command`/
`OutEvent` may be reshaped wholesale. **Boundary:** the in-flight extraction cutover
(Phase 3c) still closes under the OLD discipline (additive `kNoteRaw`, goldens
byte-identical) — the extraction's identity is "same behaviour, restructured," so
its gate stays golden-identical + frozen. The unfreeze applies from Phase 5 forward.

**A fork this ordering resolves by inclusion:** placing **#6 melodd** in the
program answers Verdi's fork #2 — the `sonotron` workstation vision (audio/Engines
tier) **is still the destination**.

This file is the execution index (mirrors `phase4-execution-plan.md`).

## Reading of the order
Three anti-sameness moves first (**1, 7, 8** — idiom transfer, procedural
generation, raw content volume), then two GUI-depth items (**2, 9** — the inert
hero zone, then the "50/50 live" half), then the first audio peer (**6**), then
robustness (**4**), then the hackability framework (**10**, on-ramp to the Director
capstone). Musical capability leads.

## Gate discipline (every item)
- 18 default + 2 Accompany goldens: a feel-changing item ships a NEW golden, or
  **intentionally regenerates** an affected golden with the diff reviewed — never a
  silent rewrite. (With the ABI unfrozen, wire-serialization changes MAY move golden
  bytes; when they do, the regeneration is deliberate and reviewed, not a
  regression.)
- **ABI: free to reshape** (freeze lifted). No `test_abi_frozen` constraint on
  Phase-5 items; if the ABI is rewritten, update/retire that test deliberately.
- arm-none-eabi cross-build green for every dual-target component touched (the
  no-heap/freestanding core constraint stays — only the *ABI shape* freeze is
  lifted, NOT the dual-target/no-heap doctrine).
- ctest green ex-flaky (`live_alsa`, `tui_console` excluded).
- codebase-memory-mcp first; rtk-proxied commands; no commit by the implementor;
  **no new dependency without a `0800` flag** (the ABI unfreeze does NOT cover new
  dependencies — notably Item F/melodd's audio lib).

---

## Parallelization map (the constraint is file-overlap on `arrangrr` core)

| Item | Primary surface | Overlaps `arrangrr` core? | Parallel group |
|------|-----------------|---------------------------|----------------|
| #4 Fuzzing | `components/midisrc/fuzz/` (new) | no | **P-now** (disjoint, new files) |
| #8 Corpus import | `apps/tools/arrstyle-converter` + style data | no (produces existing format) | **P-now** (own tool) |
| #6 melodd | `components/melodd` (new component) | no (consumes output via orchestrator) | **P-dep** (blocked on `0800` audio-dep) |
| #1 Restyle | transform policy inside `arrangrr` | **yes** | **S-core** (serialized) |
| #7 Motif | new resolver stage in `arrangrr` NTT (`3120`) | **yes** | **S-core** |
| #2 Clip | ABI events + `arrangrr`+`hostrt`+GUI | **yes** | **S-core** |
| #9 Pad/Scene | scheduler source + `hostrt`+GUI | **yes** | **S-core** |
| #10 MIDI-FX | groove/arp→chain refactor in `arrangrr` | **yes (deepest)** | **S-core** |

**The five S-core items all mutate `arrangrr` and would collide with each other
(and with the in-flight cutover's `abi.hpp`) — they are serialized in program
order.** But their **design reviews are read-only and DO parallelize now**.

**What runs in parallel RIGHT NOW (this turn):**
- **A. #4 Fuzzing** — Torquato, worktree-isolated, libFuzzer on `midisrc` SMF.
  Real code, disjoint from the cutover. Cherry-picked in after (new files + guarded
  CMake option).
- **B. #1 Restyle musical scope** — Ottorino, read-only proposal doc.
- **C. #1 Restyle placement/shape** — Corelli, read-only, now free to propose an
  ABI reshape (not additive-only).
- **D. #8 Corpus extraction scope + license/provenance flag** — Ottorino (2nd
  instance; he studied `../resources/`), read-only, so #8 implementation lands on a
  scoped plan, not blind into messy SFF/CASM.

**What waits (and on what):**
- **#1 implementation** — after the cutover commits (frees the `arrangrr`/`abi.hpp`
  surface) + reviews B/C land. The spine; gets main implementation attention.
- **#8 implementation** — after Ottorino's scope (D). Disjoint tool, so it can run
  in parallel with #1 implementation once scoped.
- **#7 / #2 / #9 / #10** — serialized in order after #1; each gets its Ottorino/
  Corelli design pass one step ahead of its implementation slot.
- **#6 melodd** — blocked on the `0800` audio-dependency decision (which lib,
  license, weight); surfaced to the owner before its slot. Its component scaffold
  (opaque-reference interface, silent stub) is disjoint and could start earlier if
  the owner wants, independent of the dep.

---

## Item detail (in program order)

### Item A — #1 Restyle (`9320`)  [HOST-only]  ← FIRST, the spine
Transform the imported input's own parts into the target style's idiom (rhythm,
voicing, articulation), keeping harmony not literal notes. Reuses the
`Pipeline<MidiSourceStage, ChorddetStage, ArrangrrStage>` shape; new work is a
transform policy inside `arrangrr`. **Scope gate:** start with rhythmic
re-quantization + register/voicing; defer melodic reharmonization. Reviews B (Ottorino
musical scope) + C (Corelli placement, ABI now free) precede implementation.

### Item B — #7 Generative motif (`9210`)  [SHIPPABLE core, dual-target]
Seeded/deterministic motif+transform engine (transpose/retrograde/displacement)
that *generates* new material. New resolver stage alongside NTT (`3120`), seeded-PRNG
discipline (`0100`). Risk is musical (correct-but-inert), not technical.

### Item C — #8 Style corpus import (`9400`/`9430`)  [HOST tooling]
Extend `arrstyle-converter`'s inspect-only `sff_import.cpp` to pull real content
from `../resources/` (70+ rules studied in `docs/backlog/yamaha-style-corpus-and-rules.md`)
into the compiled-`.cpp` style format. **Risks:** messy proprietary-adjacent format
(least-predictable effort) + **`../resources/` provenance/licensing must be checked
before any imported style ships publicly** (ship-gate, not build-gate).

### Item D — #2 Clip / launch primitive  [core + HOST GUI, ABI reshape now free]
Real launchable-cell primitive for the GUI hero zone: `launch`/`stop`/
`scene-quantize` verbs + a `clip` state event, bar/beat-quantized. Lighter than the
Looper (`6000`) — arm pre-existing content, not record/overdub. With F3 resolved, the
event/command shape is designed freely (Corelli review), not squeezed into the old
append-only ABI.

### Item E — #9 Pad/Scene live (`7200`/`8100`–`8200`)  [core + HOST GUI]
4-pad performance banks (quantized triggers) + one-button full-state recall
(Registration/STS, `DESIGN.md` §8). The 50/50-live half with zero post-GUI
investment. **Risk:** `8200` persisted state-model shape must be right (saved/recalled
data) — Corelli review of the persisted shape.

### Item F — #6 melodd first slice  [HOST-only, FLAGGED dependency]
Minimal host-only audio peer (one synth voice) realizing arrangrr's MIDI through the
opaque-reference interface (`workstation-vision.md`). **`0800` dependency fork —
owner approval required before start** (which softsynth/DSP or PortAudio-class lib,
license, weight). **Largest scope risk:** must stay one voice / no mixing / no
editing, or it becomes the DAW-gravity the identity docs refuse.

### Item G — #4 Fuzzing harness  [HOST/CI-only, clang libFuzzer, no dep]
libFuzzer around `midisrc` SMF (+ SFF/CASM follow-on), `option(SONOTRON_FUZZ)`
OFF-by-default in `components/midisrc/CMakeLists.txt`. **Started now in parallel**
(owner placed it 7th in the program, but it is disjoint and cheapest run while
`midisrc` is fresh; pulling it forward disturbs no S-core item). Torquato pins any
crash as a RED regression; he does not fix product code.

### Item H — #10 MIDI-FX / Transform chain (`5000`)  [core]
Bounded (`kMaxInserts=8`) insert-chain framework; 2–3 inserts first (echo/delay,
note-repeat, scale-lock) + refactor groove/arp into chain instances. The `0600`
hackability north-star, on-ramp to the Director (`10000`). **Risk:** groove/arp
refactor touches all 15 styles — gated by the feel goldens (`feel_swing`/`shuffle`/
`blues`) staying stable through the refactor.

---

## Forks (updated)
- **F1 — RESOLVED (Vasari mechanical pass):** `docs/DESIGN.md` §22 and
  `docs/strategy/roadmap-numbered.md` reconciled against the tree — `0910` (`melodd`)
  marked SCHEDULED (Phase-5 Item F), `11500`/`11700`/`11720` record the Phase-3
  extraction wire-work (`807c140`/`b98dbda`, deferred `hostrt::Shell` split/`--connect`
  parity noted) and the ABI-freeze-lift for Phase 5, `9400`/`9420`/`9430` corrected
  (real CASM decode, `9420` first-slice in-flight), and a new Phase-5-program bullet
  under `11730` records the ordered `1→7→8→2→9→6→4→10` sequence with #4 shipped
  (`c2251f2`) and #8 in-flight (`f6611e5`, pending cherry-pick). Every change cites its
  commit/path; see the Vasari pass report for the full diff. Left uncommitted per
  Vasari's own role boundary (does not commit) — for the dispatching agent/owner to
  commit and cherry-pick.
- **F3 — RESOLVED:** owner lifted the ABI freeze for Phase 5 (`hook-interface.md`
  ACCEPTED). No longer gates #2/#10.
- **Verdi fork #2 — RESOLVED** by including Item F (melodd): audio destination live.
- **Open dep fork:** Item F/melodd `0800` audio dependency — owner decides before
  its slot.
- **Deferred bets:** #3 STM32, #5 clock-in.

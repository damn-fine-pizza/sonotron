# Phase 6 — plan

Status: **owner-agreed 2026-07-14** (themes + order confirmed). Successor to
`phase5-plan.md` (Phase 5 complete: items #1/#2/#4/#6/#7/#8/#9/#10 shipped).
Order and per-theme scope below; each theme keeps the Phase-5 gate discipline
(Corelli shape-review before code on anything structural → implement → Torquato
QA → host `ctest` green excl. hw/tty + `arm-none-eabi` green + goldens
byte-identical → orchestrator verifies and commits).

Execution order (owner-confirmed): **1 Cleanup → 2 Audio-in-GUI → 3 Complete-#9
→ 4 Deeper-MIDI-FX.**

---

## Theme 1 — Cleanup sprint (foundational, fast)

Land everything on an honest baseline before feature work.

- **1a Status/doc reconciliation** (`vasari-status-steward`, docs-only): mark the
  shipped Phase-5 nodes done in `roadmap.md`/`DESIGN.md` (`7200`, `8100`–`8200`,
  `8500`, `5100`); fix `DESIGN.md`'s own title (still "arrangrr"); record the
  `roadmap.md` ↔ `DESIGN.md §22` scope overlap Saverio flagged.
- **1b Coverage-gate scope decision** (owner sign-off, then a small
  `scripts/coverage.sh` change): metric-1 "unit ≥ 80%" is a *local* discipline
  (no CI) and was already red on branches at baseline (74.8%). The ABI `cmd_*`
  dispatch layer is functional-tested by the project's own convention, so it
  structurally cannot count toward the unit gate. Proposal: exclude the ABI
  dispatch layer from metric-1 (as `hostrt`/`ARR_ASSERT`/throw-branches already
  are), then close the pre-existing non-dispatch debt (`runtime/midi_parser.hpp`
  57%, `common/function_ref.hpp` 54%, `clip/clip_matrix.hpp` 7%). See
  memory `coverage-gate-local-not-ci`.
- **1c Firmware-assert robustness** (`nazzareno`, real safety item): `ARR_ASSERT`
  is `__builtin_trap()` in *every* preset — no `ARRANGRR_NO_ASSERT` release path
  — so a reachable cap-hit traps the firmware (the class of bug just fixed in
  `InsertChain::apply`). Add a firmware-release build path that compiles
  `ARR_ASSERT` out (graceful degradation, D-STM32), and audit the remaining
  trap-when-hit sites (`Arranger::kMaxVoiceNotes` uses the same idiom — harmless
  today but same class).

## Theme 2 — Audio in the standalone GUI (node `0910` follow-on)

Make `gui-sonotron` audible on its own (today sound only comes from
`apps/tools/melodd`, `apps/demo`, or an external synth over ALSA).
- Wire `melodd::Synth` into `gui-sonotron`'s in-process brain session: a
  dedicated audio thread consuming the engine's `OutEvent` MIDI stream →
  `Synth.render()` → miniaudio device. `Synth` already exists
  (`components/melodd`), realization-free core boundary intact (arrangrr never
  sees audio — the peer realizes).
- SoundFont (`.sf2`) load surface + thread/latency model. HOST-ONLY (melodd is
  not on the arm target). Corelli/Prospero review the thread + realtime seam
  before code.

## Theme 3 — Complete the #9 rig

- `master_transpose`: add the missing engine backing state (musical-scope
  decision on what "global transpose" shifts — Ottorino/owner), then wire the
  reserved `Performance.master_transpose` field.
- Pad types Drum/CC (need new note/CC emission in the pad layer — the piece #9
  deferred). NoteRepeat likely folds into the FX note-repeat insert.
- `Performance format_version 2`: add the FX chain snapshot + the general
  `Router` thru-matrix, with an explicit migrator from v1 (Principle #8).
- "Active pad bank" ABI verb (`m_pad_bank` is fixed 0 in v1).

## Theme 4 — Deeper MIDI-FX (`5210`/`5220`)

The biggest architectural fork — do it deliberately, last.
- Decide the `Insert` interface: pure stream-transform vs a capability-probed
  optional `on_tick` hook (Corelli's option 1 vs 2) — this decides the signature
  for all slots.
- `5210` groove-as-insert: bit-identity discipline (graft at the same D40 point
  or accept the pitch-overlap edge-case golden risk).
- `5220` arp-as-insert: per-role instances vs the `on_tick` capability.

---

Notes carried from Phase 5 (memories): `coverage-gate-local-not-ci`,
`phase5-abi-unfrozen`, `extraction-closed-at-seam`,
`gui-sonotron-lsp-false-positives`.

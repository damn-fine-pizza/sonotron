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
- **1c Firmware-assert robustness** (`giotto`, real safety item): `ARR_ASSERT`
  is `__builtin_trap()` in *every* preset — no `ARRANGRR_NO_ASSERT` release path
  — so a reachable cap-hit traps the firmware (the class of bug just fixed in
  `InsertChain::apply`). Add a firmware-release build path that compiles
  `ARR_ASSERT` out (graceful degradation, D-STM32), and audit the remaining
  trap-when-hit sites (`Arranger::kMaxVoiceNotes` uses the same idiom — harmless
  today but same class).

## Theme 2 — Audio in the standalone GUI (node `0910` follow-on) — ✅ SHIPPED (4f97af5)

Make `gui-sonotron` audible on its own (today sound only comes from
`apps/tools/melodd`, `apps/demo`, or an external synth over ALSA).

Shipped 2026-07-14 (commit `4f97af5`): new arrangrr-free `gui_sonotron_audio`
library (`AudioEngine` = `melodd::Synth` + miniaudio + a lock-free SPSC note
ring + a narrow render/soundfont/panic mutex); `OutEvent → AudioMidiEvent`
translation gated to `kPrimaryAudioOutPort == 0` at `run_engine()`'s existing
`kMidi` filter; shared `melodd::dispatch_midi_message()`; `load_soundfont()`
split (disk-read vs state-swap); "Load SoundFont…" field auto-prefilled via
`find_system_soundfont()`. Reachability gap closed: `InProcessBrainSession`
auto-routes the default band to `out0` on `style load` (reusing
`apps/demo/jam/setup.acmd`'s canonical map) — zero new UI. Shape review:
`docs/phase6-design-reviews.md`. Deferred (NEEDS-DECISION): audio in
`--control`/external-server mode (JSONL wire addition); `OutEvent.port`
filtering once multiple output ports exist. Owner chose load-from-path only
(no bundled `.sf2`) and filter-by-port.
- Wire `melodd::Synth` into `gui-sonotron`'s in-process brain session: a
  dedicated audio thread consuming the engine's `OutEvent` MIDI stream →
  `Synth.render()` → miniaudio device. `Synth` already exists
  (`components/melodd`), realization-free core boundary intact (arrangrr never
  sees audio — the peer realizes).
- SoundFont (`.sf2`) load surface + thread/latency model. HOST-ONLY (melodd is
  not on the arm target). Corelli/Prospero review the thread + realtime seam
  before code.

## Theme 3 — Complete the #9 rig — ✅ SHIPPED

All four items landed 2026-07-14 (each Corelli/Ottorino design → implement →
Torquato QA → gates → commit):
- **#1 `master_transpose`** (`27e437a`): signed ±12 semitone offset applied
  "late" (absolute note) in `resolve()` + `ChordEngine::sound()`, drums exempt
  via `RolePolicy::kFixed`, drop-not-fold. Live-change safety: voicing memory
  delta-shifted so `nearest_octave()` doesn't fold. Scope doc:
  `docs/reflections/phase6-theme3-master-transpose-scope.md`. DEFERRED: the
  "early"/re-key transpose (needs an octave-carry + voicing-history redesign).
- **#4 active pad bank** (`83a8ade`): `Param::kPadBankSelect` (58) + `pad_bank_id`
  capture/recall round-trip; flat pad addressing unchanged (persisted view cursor).
- **#2 Drum/CC pads** (`c316a1d`): `PadType::kDrum`/`kCC` emit note/CC directly
  in `fire_pad` via the existing choke point (pad-owned dest, ~120-tick drum
  gate, sync-boundary honored via per-slot `BoundaryLatch`, panic resyncs pad
  state). NoteRepeat NOT a pad type — folds into Theme 4's FX note-repeat insert.
- **#3 Performance format v2** (`d242348`): per-role FX-chain snapshot restored
  on recall; `routing_profile_id` RESERVED (0xFFFF-only) awaiting a future
  RoutingProfileStore; `master_transpose` widened to native int16. `sizeof`
  96→576, wire 94→574, `format_version`→2; host-only v1→v2 migrator
  (`components/hostrt/perf_v1_migrate`), core still refuses non-current versions.
  Design: `docs/reflections/phase6-theme3-performance-format-v2-review.md`.

Original scope notes (for reference):
- Pad types Drum/CC (need new note/CC emission in the pad layer — the piece #9
  deferred). NoteRepeat likely folds into the FX note-repeat insert.
- `Performance format_version 2`: add the FX chain snapshot + the general
  `Router` thru-matrix, with an explicit migrator from v1 (Principle #8).
- "Active pad bank" ABI verb (`m_pad_bank` is fixed 0 in v1).

## Theme 4 — Deeper MIDI-FX (`5210`/`5220`) — ✅ SHIPPED (`8428a4d`)

The biggest architectural fork — done deliberately, last. Closes Phase 6.
- `Insert` interface: **Option 2** adopted — optional, switch-dispatched
  `ingest`/`on_tick` capabilities, a genuine no-op for the five stateless
  types (one runtime-tagged type, not SFINAE).
- `5210` groove-as-insert: shipped as `InsertType::kGroove`, auto-present and
  pinned effectively last, reproducing `groove::apply` inside the chain — all
  22 goldens byte-identical; genuinely reorderable, fan-order footgun
  documented.
- `5220` arp-as-insert: shipped as `InsertType::kArp` (slim 4-byte config in
  the union) with a per-role `ArpeggiatorEngine m_role_arp[kRoleCount]`
  (session-only) and a new ungated per-role-per-tick pass in
  `Arranger::on_tick`.
- QA (Torquato) found and Giotto fixed two defects before landing: stale
  arp on live style switch (`request_style` now resets), and a dual-arp
  output collision (`OutScheduler` retrigger-care now scoped per-producer).
  Design source of truth: `docs/reflections/phase6-theme4-insert-interface-fork.md`.

---

Notes carried from Phase 5 (memories): `coverage-gate-local-not-ci`,
`phase5-abi-unfrozen`, `extraction-closed-at-seam`,
`gui-sonotron-lsp-false-positives`.

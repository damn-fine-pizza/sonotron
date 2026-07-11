# Pipeline P0 — execution plan: make the GUI *live* (two additive engine events)

Status: **planned — not started.** Follows the GUI mechanical strand (`gui-fase2-mechanical-plan.md`,
G0–G3, all DONE, commit `473ab60`). Derived from `ux-workstation.md` §11 (the core strand). This plan
turns the already-built, already-connected GUI from **honest placeholders** into a **live, playable**
surface by teaching the engine to broadcast two pieces of state it already holds internally. The **clip /
scene primitive** is explicitly **P1, out of scope here** (a new engine concept, not a wire) and gets its
own plan.

## Context — the cable already exists

The GUI ↔ engine link is DONE (G2): a **Unix domain socket** (`AF_UNIX`/`SOCK_STREAM`, non-blocking),
newline-framed **JSONL** out / text commands in, served by `components/hostrt` (node `11500`), consumed by
`UdsBrainSession` as a pure client that never links the core. Today the engine emits **5 event kinds**
(`kMidi`/`kTransport`/`kChord`/`kSection`/`kWarn`, see `components/arrangrr/include/arrangrr/abi.hpp`). The
GUI decoder already reserves two **inert stub slots** — `kChordFollowed`, `kBeat`
(`apps/gui-sonotron/src/brain_event.hpp`, "never decoded yet") — and every panel was built to light up when
those arrive **with no rewrite**. So this is not "connect the GUI": it is "make the engine say two more
things, then fill the two slots."

## The two gaps (ux-workstation.md §11 numbering)

- **P0-1 — `kChordFollowed`**: the chord the arranger is *currently following* in real time (root+quality,
  current vs pending), sourced from `FollowedContext`. Feeds the harmonic visualizer (green = committed,
  amber = pending). **Largely already built in the spike** (see Salvage).
- **P0-2 — `kBeat`**: the live musical position (bar, beat, and the sub-beat phase for a smooth playhead),
  sourced from the 960-PPQN transport clock. Feeds the transport + seqedit playhead. **Greenfield.**

## Invariants (do not break)

- **ABI append-only, freeze stays intact.** Add new `OutEvent::Kind` enumerators at the end; `Kind` is a
  `std::uint8_t` field, so `sizeof(OutEvent)`/`sizeof(Command)` are UNCHANGED — `test_abi_frozen` needs only
  its `Kind`-count / snapshot bumped (5→7 across both events), not a size change. This is exactly the
  additive extension the freeze line (`11700`) was designed to permit.
- **No-heap, dual-target core.** Both events must cross-build host GCC + arm-none-eabi; no allocation, no
  `std::string` in the core emit path. Position/chord payloads are packed into the existing POD `OutEvent`.
- **GUI stays a pure client** — decode the JSONL only; never link the core, never see a core enum.
- **No new dependency** (core or host). The JSONL parser stays the hand-rolled flat reader.

## Salvage from `spikes/kchordfollowed` (commit `4145660`)

The P0-1 core+host work is already written and tested on that branch, on the **old tree layout**. Port it,
translating paths `app/core/ → components/arrangrr/` and `app/platform/host/jsonl.cpp →
components/hostrt/jsonl.cpp`:

| Spike file (old path) | Destination (current tree) | Role |
|---|---|---|
| `app/core/include/arrangrr/abi.hpp` (+27) | `components/arrangrr/include/arrangrr/abi.hpp` | the additive `kChordFollowed` `OutEvent` |
| `app/core/.../engine.hpp` `.../engine.cpp`, `chord_engine.hpp`, `followed_context.hpp` | same under `components/arrangrr/` | emit on followed-chord change |
| `app/platform/host/jsonl.cpp` (+67) | `components/hostrt/jsonl.cpp` | serialize `kChordFollowed` → JSONL line |
| `app/core/tests/test_chord_followed_event.cpp` (+207), `test_abi_frozen.cpp` (+3/-1) | `components/arrangrr/tests/` | core tests + frozen-ABI count bump |
| `app/tests/golden/chord_followed.{acmd,golden}` + updated goldens | `components/.../tests/golden/` | golden regression |

Read with `git show spikes/kchordfollowed:<old-path>` and `git ls-tree -r spikes/kchordfollowed`. Treat the
spike as a REFERENCE to re-apply, not a merge (the branch predates the reorg + the freeze). The spike does
**not** touch `kBeat` — P0-2 is written fresh.

## Sequence (small slices, each: cross-builds + green tests + observable in the GUI)

### P0-1 — `kChordFollowed` (harmonic visualizer)  ← recommended first (spike-backed, lower risk)
- Core: port the additive `OutEvent::Kind::kChordFollowed` + its emit from `FollowedContext` (spike).
- Host: port the `jsonl.cpp` serializer → `{"ev":"chord-followed","root":…,"quality":…,"pending":…}` (exact
  shape to match the spike / gui-contract-map).
- GUI: decode the `kChordFollowed` stub in `brain_event.*`; reduce in `app_state`; light `intention_panel`
  (and the harmony surface) green/amber. The green-at-rest/amber gate already exists in `AppState`.
- Tests: ported core golden `chord_followed`, host serializer test, GUI `test_brain_event` + `test_app_state`
  extensions; `test_abi_frozen` count bump.
- DoD: engine following a live chord → the GUI visualizer lights green/amber (screenshot).

### P0-2 — `kBeat` (playhead)
- Core: add `OutEvent::Kind::kBeat`; emit from the transport clock. **Decide granularity** (per-beat is
  enough for a moving playhead; per-grid-tick is smoother but chattier — pick per-beat + a sub-beat phase
  byte). Pack bar/beat/phase into the POD `OutEvent`.
- Host: serialize → `{"ev":"beat","bar":N,"beat":M,"phase":…}`.
- GUI: decode the `kBeat` stub; feed `transport_panel` + the seqedit playhead (replace "playhead awaits core
  kBeat").
- Tests: core golden (deterministic beat emission under the virtual clock), host serializer test, GUI decoder
  + reduction; `test_abi_frozen` count bump.
- DoD: engine playing → the playhead scrolls in transport and seqedit (screenshot).

Order is flexible (the two events are independent); P0-1 first because the spike de-risks it and it matches
the §11 numbering.

## Boundary — P1, out of scope for this plan

The **clip / scene primitive** (launchable cells, `launch`/`stop`/`scene-quantize` verbs, the grid's real
launch) is a new engine concept plus new command verbs, not a broadcast event — it gets its own plan after
P0. The `grid_panel` launch stays an honest placeholder until then.

## End-to-end verification

1. `cmake --build` host preset → green; **and** the arm-none-eabi cross-build → green (dual-target gate).
2. `ctest` → green (core goldens + host serializer + GUI decoder/reduction + `test_abi_frozen`).
3. Live: `cli-arrangrr --control <sock>` then `gui-sonotron --control <sock>`; play a chord and start
   transport → the visualizer lights and the playhead scrolls. Headless screenshot
   (`SONOTRON_GUI_MAX_FRAMES` + `SONOTRON_GUI_SCREENSHOT`) captures both.

## Notes

- Suggested routing: `nazzareno` implements the slices (may request bounded slices from `taddeo`/`filippino`);
  `torquato` owns the goldens + red-before-green; `corelli`/`fabrizio` for the ABI-shape / line review.
- This plan adds NO dependency and does not touch the clip primitive; it is the last step of node `11600`
  ("a living, playable GUI") before the behind-the-line features (`9310` Accompany, `5000` MIDI-FX, …).

# GUI Phase 2 — execution plan for the *mechanical* strand

Status: **in progress — G0 and G1 DONE (2026-07-11), G2→G3 pending.** Derived from `ux-workstation.md` §13
(salvage/rebuild map); this plan implements only the part **with no dependency on the core** (the GUI
mechanical strand). The core strand (§11: `kChordFollowed`, `kBeat`, the clip primitive) is **out of scope
for this plan** and will be sequenced afterwards. G0 (concept demolition, commit `38b5826`) and G1
(workstation layout + nested vertical split + `visible` + font 13, commit `c49f8c6`) are implemented,
tested (4 green GUI tests, including `test_layout_nested_split`) and committed; the headless screenshot
confirms the §3 wireframe. Next up: **G2 (brain session)**. Note: the source spike `spikes/gui-skeleton`
lives on a separate, unmerged branch — it is not in the working tree.

## Context

The 2026-07-10 pivot (`ux-workstation.md`, node 11600) makes the *workstation screen* authoritative and
supersedes the current "conductor dashboard" shell. Today the GUI (`apps/gui-sonotron/`) is a mock shell:
a single-row layout engine plus two concept zones (`arrangement*`, `intention*`) fed by fake data, with no
connection to the brain and 3 of its 5 zones empty. This plan reuses the mechanical shell and **discards the
concept content**, re-centering the screen on the 6 workstation zones — staying inside the *already-shipped*
contract (`gui-contract-map.md`: L1 text in / JSONL out), without touching the frozen ABI and without
waiting on core work.

Entry decision (owner, 2026-07-10): **GUI mechanical first**, then core P0, then the clip primitive.

## Invariants

- Only `*_panel.cpp` and `layout_renderer.cpp` include ImGui; the *models* are pure, host-only testable
  data (harness `tests/test.hpp`, macro `CHECK`).
- `test_abi_frozen` is untouched (no ABI change in this strand).
- No new host/core dependency (the spike introduces none: POSIX + STL only).

## Salvage from the `spikes/gui-skeleton` spike

The spike holds code that is already written and tested, to be **ported** (namespace `arrangrr::gui` →
`sonotron`, path `app/gui/` → `apps/gui-sonotron/src/`):

| Spike file | Destination | Role |
|---|---|---|
| `wire.{hpp,cpp}` | `brain_event.{hpp,cpp}` | `LineBuffer` (framing) + flat-JSON parser + `Event` decoder (POD) |
| `uds_client.{hpp,cpp}` | transport inside `uds_brain_session.{hpp,cpp}` | non-blocking AF_UNIX client, `send_line`/`poll_lines`, never `quit` |
| `app_state.{hpp,cpp}` | `app_state.{hpp,cpp}` | event reduction, green-at-rest/amber gating |
| `tests/test_wire.cpp`, `tests/test_uds_client.cpp` | `tests/test_brain_event.cpp`, ... | host-only tests, already written |

Delta versus the spike: wrap transport + decoder + state behind the abstract `BrainSession` interface
(spec §9) so that `InProcessBrainSession` is substitutable without touching the panels. `send()` keeps the
`quit`/`exit` blacklist.

## Sequence (small slices, each one compiles + green tests + screenshot)

### G0 — Concept demolition ✅ DONE (commit `38b5826`)
- DELETE: `arrangement.{hpp,cpp}`, `arrangement_panel.{hpp,cpp}`, `intention.{hpp,cpp}`,
  `intention_panel.{hpp,cpp}`, `tests/test_arrangement.cpp`, `tests/test_intention.cpp`, and the
  `mock_*` generators.
- `layout_renderer.cpp`: remove the two mock `case`s and their includes.
- Update `apps/gui-sonotron/CMakeLists.txt` and `tests/CMakeLists.txt` (source/test lists).
- DoD: green build, green suite (with the zones rendered as titled empty frames).

### G1 — Layout engine: nested vertical split + `visible` + font 13 ✅ DONE (commit `c49f8c6`)
- `layout_model.{hpp,cpp}`: add `bool visible = true` to `Zone`; extend `compute_rows` — which before G1
  grouped only by `Zone::row` (single-level rows) — with a notion of a **sub-column that stacks zones
  vertically inside a cell** (the right rail: `intention` above `parts` in col 2). Set `kDefaultFontSizePx`
  to **13** (the default only).
- `layout_json.{hpp,cpp}`: read/write `"visible"` and the nested-split shape.
- `layout_renderer.cpp`: render the nested split; skip `!visible` zones.
- `layout_model.cpp::default_layout()`: rewrite the 6 workstation zones
  (`transport` full-span row 0; `browser`/`grid`/right-rail on row 1; `seqedit` full-span row 2).
- Tests: update `test_layout_model`, `test_layout_json`, `test_layout_roundtrip`; add
  `test_layout_nested_split.cpp`.
- DoD: green build + suite; screenshot shows the 6 zones in the wireframe layout (§3).

### G2 — Brain session (already-shipped contract, no core work)
- ADD: `brain_session.hpp` (abstract interface, §9), `brain_event.{hpp,cpp}` (ported from `wire.*`),
  `uds_brain_session.{hpp,cpp}` (ported from `uds_client.*` + decoder), `app_state.{hpp,cpp}` (ported).
- Decoder scoped to the **5 already-shipped shapes** (`midi-out`, `chord`, `section`, `transport`, `warn`);
  the additive fields (`chord-followed`, `beat`, `clip`) stay stubbed until the core strand emits them.
- `main.cpp`: instantiate `UdsBrainSession` (path from `--control`/preferences), `poll()` once per frame,
  reduce into `AppState`; show connection state in the transport.
- Tests: `test_brain_event.cpp`, `test_app_state.cpp` (ported/extended from the spike).
- DoD: with a brain listening on UDS, the transport shows ● Connected and the event log scrolls.

### G3 — Zone panels (mock/partial; the core-dependent fields stay placeholders)
- ADD model+panel pairs: `transport_panel.*`, `browser_panel.*`+`browser_model.*` (the 16 builtin styles
  as a drag-source), `grid_panel.*`+`grid_model.*` (matrix/scenes; the real *launch* awaits the core clip
  primitive), `seqedit_panel.*`+`seqedit_model.*`, `parts_panel.*`+`parts_model.*`, `intention_panel.*`
  (new, minimal, read-only).
- `main.cpp`: menu bar (`BeginMainMenuBar` — File/Edit/View/Transport/Help), View→Intention/Parts toggles.
- Tests: `test_grid_model.cpp` (plus any additional model tests).
- DoD: screen navigable end-to-end; playhead / harmonic visualizer / launch show honest placeholders while
  the core strand is pending.

## Boundary with the core strand (out of scope for this plan)

These stay placeholders until §11 lands: the real playhead (depends on `kBeat`), the live green/amber
harmonic visualizer (depends on `kChordFollowed`), and real cell launch (depends on the `clip` primitive
plus the `launch/stop/scene quantize` verbs). The panels are designed to light up when the event arrives,
with no rewrite.

## End-to-end verification

1. `cmake --build` of the host preset → green.
2. `ctest` label `unit` for `apps/gui-sonotron` → green (pure models + brain_event + app_state).
3. Headless screenshot (`screenshot.*`) after G1 and G3 → compared against the §3 wireframe.
4. Start `cli-arrangrr --control /tmp/son.sock`, then the GUI: transport ● Connected, event log scrolls,
   send `transport start` from the menu → `midi-out` events become visible.

## Notes

- The spec cites `product-identity.md` under `docs/design/`; the real file is `docs/product-identity.md`
  (reference to correct in a doc sweep, non-blocking).
- Suggested routing when executing: `nazzareno` implements the slices, `torquato` owns the tests/goldens
  and the red-before-green discipline, `corelli`/`fabrizio` for architecture/line review.

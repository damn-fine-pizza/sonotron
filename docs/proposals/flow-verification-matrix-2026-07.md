# Flow -> Verification Matrix, gui-sonotron (2026-07)

**Author:** Guido (process analyst). **Scope:** inbound inventory + outbound
verification mapping for every user-facing flow apps/gui-sonotron supports or
intends to support. This is a PROCESS ARTIFACT, not a test suite: it names
what must be proven and how it is (or is not) proven today; it authors no
tests and edits no product code.

**Why this exists (2026-07-16 owner ask):** the auto-song / beat-playhead
flow had a GREEN functional test
(`apps/gui-sonotron/tests/test_grid_panel_auto_song.cpp`) while the LIVE app
was broken — the real `InProcessBrainSession` backend never advanced `bar()`
in the render loop. The test was green because it drove a `SpyBrainSession`
double and injected synthetic `"beat"` JSONL directly into `AppState`,
bypassing the entire engine-thread/ring/decode pipeline a real user's "press
Play" actually walks. **The lesson encoded below: a flow is only "verified"
if the test exercises the flow the way a user actually triggers it (real
backend + real trigger path), not a stubbed shortcut that happens to share a
CTest label with one that does.**

Grounding: read directly from `apps/gui-sonotron/src/*.cpp`,
`apps/gui-sonotron/main.cpp`, `apps/gui-sonotron/tests/*.cpp` and
`apps/gui-sonotron/tests/CMakeLists.txt`, plus `scripts/ci.sh` and
`scripts/coverage.sh`, on branch `gui-sonotron` as of 2026-07-16. Every
trigger-path and test-file citation below was read, not assumed. Torquato is
separately authoring the real-backend RED/GREEN tests referenced here as the
exemplar pattern — this document reports on that work, it does not duplicate
or extend it.

---

## 1. Verification-mechanism taxonomy

The project already has a CTest label convention (`unit` / `functional`),
applied consistently across `components/core/arrangrr/tests/CMakeLists.txt`,
`components/core/runtime/tests/CMakeLists.txt`,
`components/platform/midisrc/tests/CMakeLists.txt` and this app's own
`apps/gui-sonotron/tests/CMakeLists.txt`. `scripts/coverage.sh` measures
three metrics off these labels (`-L unit`, `-L functional`, `-L regression`):
metric 1 (unit, core-scoped, advisory 80% l/f/b), metric 2 (functional,
report-only), metric 3 (regression, a census of anti-regression tests for
fixed bugs, report-only but meant to be monotonically non-decreasing).
`scripts/ci.sh` runs the FULL untagged suite (`ctest --preset host`, no `-L`
filter) as the hard gate; the labels only matter for `coverage.sh`'s
per-category breakdown.

The mechanisms actually in play, or that should be, for this app:

| Mechanism | When it applies | Example already in this tree |
|---|---|---|
| **Pure unit** | A logic seam with no I/O, no ImGui, no core (`gui_sonotron_models` / `gui_sonotron_brain` targets) | `test_grid_model.cpp`, `test_app_state.cpp`, `test_brain_event.cpp` |
| **Headless functional/integration** | The REAL production entry point (a `render_*_panel` function, or a REAL `InProcessBrainSession`) driven end-to-end, no GPU/window, font atlas self-built | `test_grid_panel_auto_song_real_backend.cpp` (real backend + real render loop), `test_in_process_brain_session_bar_advance.cpp` (real backend, no ImGui) |
| **Golden/regression (core)** | Deterministic style/pattern output pinned byte-for-byte | not used in gui-sonotron today (owned by `app/tests/golden` at the core level) |
| **Fuzz** | An untrusted parser | not applicable inside gui-sonotron (`brain_event.cpp`'s JSONL parser is a candidate, currently untested this way — see gap list) |
| **Sanitizer runs** | Memory/UB safety net over the whole suite | not currently wired for this app specifically (project-wide ASan/UBSan status is outside this doc's read) |
| **Real-UI automation** | Verifying an actual injected mouse click through the actual render loop, located via real draw-data (not a state mutation that mimics one) | **UPDATE 2026-07-17 (Torquato):** a real seam now exists, `apps/gui-sonotron/tests/imgui_headless_harness.hpp` — real `io.AddMousePosEvent`/`AddMouseButtonEvent` injection, widget-rect discovery by scanning rendered draw-data vertex colors (`find_color_clusters`/`find_single_color_rect`/`find_child_window_rect`), no `IMGUI_ENABLE_TEST_ENGINE` needed. This closes the click-injection gap for every `ImGui::InvisibleButton`/`pad_button`-shaped widget (transport Play/Stop, grid cells, scene-header ▶, M/S latches, browser style rows) — see the 6 new `_ui_automation.cpp` tests below. The ONE widget still NOT closable this way is `ImGui::SmallButton` with an alpha-0-at-rest fill (the auto-song arm toggle): it paints no distinctive vertex color to locate until hovered/engaged, so `click_arm_auto_song`'s direct-field-write stand-in remains, documented as a residual gap in each test that uses it. |

### The label gap this task exists to close

`test_grid_panel_auto_song.cpp` (SpyBrainSession, injected `"beat"` JSONL,
**false-confidence**) and `test_grid_panel_auto_song_real_backend.cpp` (real
`InProcessBrainSession`, real engine thread, real render loop,
**end-to-end**) are BOTH labeled `functional`
(`apps/gui-sonotron/tests/CMakeLists.txt:148` and `:163`). Nothing in CTest,
`coverage.sh`, or `ci.sh` distinguishes them — a reviewer running `ctest -L
functional` sees two green results and cannot tell, from the label alone,
that one of them would have stayed green through the entire live-bug window.
The fidelity distinction currently lives ONLY in hand-written header
comments (which are excellent, and exactly what let this analysis reconstruct
the story — but comments are not a gate).

**Recommendation: do not introduce a new `integration` CTest label.**
`functional` already means "the real production entry point, driven
end-to-end" in this project's own vocabulary (`coverage.sh`'s own comment:
"functional / interaction / engine-level / golden / integration tests");
splitting it would fragment metric 2 without buying a mechanical distinction
that a fourth label wouldn't ALSO need to be manually applied correctly.
The cheaper, higher-leverage fix is process, not taxonomy: mandate a one-line
"REAL vs STUBBED" tag in the file's own header comment (already present, as
demonstrated above, in every test that matters) PLUS a naming convention —
`_real_backend` / `_real_render_loop` suffix for the end-to-end flavor, mirroring
what Torquato already did unprompted for `test_grid_panel_auto_song_real_backend.cpp`
— and a one-line CMake comment cross-referencing the stubbed sibling test it
supersedes-in-confidence (also already the pattern at CMakeLists.txt:150-157).
This is Mechanism #1 in the adoption order below. If the owner later wants a
mechanical (not just textual) distinction, the fallback is a THIRD CTest
label `functional-stubbed` applied ONLY to the small set of tests that are
known to fake the backend or the trigger — an opt-in "confess" label, cheaper
to introduce than a blanket `integration` label because it only touches the
handful of tests that need the caveat, not a rename of the whole functional
set.

---

## 2. The flow inventory (grouped by zone / subsystem)

Trigger-path citations are `file:line`. "Verification today" cites the exact
test file + CTest label, or states `MANUAL ONLY` / `NONE`.

### 2a. Transport zone (`src/transport_panel.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| Press Play -> transport starts | `transport_panel.cpp:55` `send("transport start")` + `app_state.note_transport_sent(true)` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_transport_play_stop_ui_automation.cpp` (functional/UI-automation) — a REAL injected click on `render_transport_panel`'s own Play pad, real `InProcessBrainSession`, asserts `AppState::transport()==kPlaying` and `bar()>0` land for real. Still also covered at the backend layer by `test_in_process_brain_session_bar_advance.cpp`. | End-to-end, click through readback | Residual gap closed. |
| Press Stop -> transport stops, bar parks to 0 | `transport_panel.cpp:61` `send("transport stop")`; reduction in `src/app_state.cpp:80-94` (`kTransport` "stopped" case zeroes `m_bar/m_beat/m_pulse`) | **CLOSED 2026-07-17:** same `test_transport_play_stop_ui_automation.cpp` clicks the real Stop pad after a real Play, and asserts BOTH `transport()==kStopped` AND `bar()==0` together (found and fixed a TEST-DESIGN race here: `note_transport_sent(false)`'s optimistic GUI-side hint flips `transport()` one poll cycle before the real `kTransport("stopped")` OutEvent actually zeroes the bar — a break condition checking `transport()` alone races that hint; this is a test-harness bug, not a product bug, and is fixed in the test, not the app). | End-to-end, click through readback | Residual gap closed. |
| Press Panic -> all-notes-off | `transport_panel.cpp:66` `send("panic")` | `NONE` in gui-sonotron's own test tree (panic's ALSA-level behavior may be covered at `components/platform/hostrt/` level, out of this app's read) | NONE (from the GUI's perspective) | **Gap: hand to Torquato** — at minimum, a real-backend test that `panic` is accepted without error/warn through `InProcessBrainSession::send()`. |
| Scroll/arrow over BPM field -> `bpm <n>` (nudge, clamped 20..400) | `transport_panel.cpp:94-108` | `NONE` | NONE | Backend-side clamping likely covered at `components/core/arrangrr/tests`; the GUI-side clamp (`std::clamp(bpm + d, 20, 400)`, a local `static int`, never read back from the engine) has no test. **Gap: hand to Torquato** for a unit test of the clamp math alone (no ImGui needed if the clamp logic is extracted — currently it is NOT extracted, it lives inline in the render function, an anonymous-namespace-adjacent testability gap worth flagging to Giotto/Corelli, not fixed here). |
| Scroll/arrow over transpose field -> `transpose <n>` (clamped -12..12) | `transport_panel.cpp:109-124` | `NONE` | NONE | Same as BPM above. |
| bar:beat:pulse readout (real, from `kBeat`) | `transport_panel.cpp:130-142` reads `app_state.bar()/beat_num()/pulse()` | Covered TRANSITIVELY by `test_in_process_brain_session_bar_advance.cpp` (proves `bar()` climbs) and `test_app_state.cpp` (unit, proves the reduction) | End-to-end for the underlying data; the DISPLAY format itself (the "— : — : ··" stopped-state string, the `%03d` padding) is untested | Low priority: cosmetic. If ever a bug is reported here, it becomes a `regression`-labeled unit test on the display helper, not a new mechanism. |
| Glow toggle (visual only, no send) | `transport_panel.cpp:175-178` | `NONE` | NONE (no backend involvement, purely local state) | Low priority. A pure unit test on `fx.glow` toggling is trivial to add if ever churn touches it; not worth prioritizing today. |
| Playing/stopped status dot + blink | `transport_panel.cpp:151-165` | `NONE` | NONE (display-only, derived from `playing`) | Low priority, same class as the glow toggle. |
| Transport menu mirrors (Start/Stop/Continue/Panic) | `main.cpp:408-422` | `NONE` — note `"transport continue"` is sent ONLY from this menu, never from `transport_panel.cpp`'s own buttons, and has no test anywhere in this tree | NONE | **Gap, flagged explicitly:** `"transport continue"` is a real wire verb with zero test coverage at any layer within this app. **Hand to Torquato.** |

### 2b. Browser zone (`src/browser_panel.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| Click a style leaf while stopped -> `style load <name>` | `browser_panel.cpp:114` | `test_in_process_brain_session.cpp` (functional) sends `"style load basic"` through a real backend elsewhere (reused, not from this trigger site); the actual browser CLICK is never automated | Backend-only end-to-end; the click/leaf-selection UI itself untested | The panel-level click path is only exercised implicitly via `main.cpp`'s own default-boot `style load` (main.cpp:653-661), never via a headless `render_browser_panel` harness. **Gap: hand to Torquato** — a `render_browser_panel`-driving headless test analogous to the grid-panel real-backend test, OR judged low priority since the wire effect is already proven from other call sites. |
| Click a style leaf while playing -> `style switch <name>` (live morph, quantized) | `browser_panel.cpp:110-115` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_browser_style_switch_while_playing_ui_automation.cpp` (functional/UI-automation) — a REAL injected click on a REAL browser style leaf row while a REAL `InProcessBrainSession` is really playing, distinguishing `style switch` from `style load` via the real, engine-observable `AppState::section()` flip (never a spy on the sent wire text). | End-to-end, click through readback | Closed. |
| Drag a style onto a grid cell -> registers a real ClipMatrix clip + fills cell | `browser_panel.cpp:117-121` (drag source) + `grid_panel.cpp:561-573` (drop target, `send("clip add ...")`) | `NONE` (drag-drop is an ImGui interaction with no headless simulation seam in this codebase, same class of gap as the SmallButton click) | NONE — and structurally DEFERRED per owner steer (drag-drop payload simulation would test ImGui's own drag machinery, not this feature) | **DEFERRED (owner: needs real UI).** Not proposed for construction now. |
| Drag a "variations" row onto a scene header -> sets that column's SectionType (host-only, no send) | `browser_panel.cpp:130-164` (drag source) + `grid_panel.cpp:369-374` (drop target, `GridModel::set_scene_section`) | `NONE` for the drag path; `GridModel::set_scene_section` itself may be indirectly unit-covered via `test_grid_model.cpp` (not confirmed to include this exact call) | Partial at best | **DEFERRED (drag-drop, same class as above).** The pure `GridModel::set_scene_section` mutation IS a testable unit seam without any drag simulation — **hand to Torquato** to confirm/add a direct unit test bypassing the drag entirely. |
| Browser search filter (live, all sections) | `browser_panel.cpp:207-214`, `model.set_search_filter` | `test_browser_model.cpp` (unit): `test_search_filter_is_case_insensitive_substring`, `test_search_filter_empty_matches_everything` | End-to-end for the MODEL logic (pure, no ImGui needed); the InputText widget wiring itself untested (low-value, standard ImGui idiom) | Adequate. |
| "kits · GM" list | `browser_panel.cpp:45-48`, rendered via `render_list`, explicitly a "design-intent, local-only list" — no verb wired | `NONE` | N/A — nothing to verify (no behavior beyond display) | None needed until a kit-load verb exists. |
| "clips" section placeholder | `browser_panel.cpp:201-204` | `NONE` | N/A (static text) | None needed. |

### 2c. Repeat Zone / launch grid (`src/grid_panel.cpp`) — includes the auto-song exemplar

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| Click an EMPTY cell -> local demo clip fill (no send) | `grid_panel.cpp:526-532` | `NONE` | NONE (host-only mutation) | Low priority; a headless-render click-equivalent state check would need the same InvisibleButton-click-injection gap noted elsewhere — **DEFERRED (needs real UI / or at minimum a state-mutation-only unit test on `GridModel::set_cell`, which IS already covered generically by `test_grid_model.cpp`)**. |
| Click a FILLED cell -> real `launch clip <id> quantize <n>` + opens Sequence Edit | `grid_panel.cpp:526-546` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_grid_cell_launch_open_seqedit_ui_automation.cpp` (functional/UI-automation) — a REAL injected click on the drums/scene-0 cell through a REAL `InProcessBrainSession`, asserting BOTH halves: the real per-cell `AppState::clip_state` readback flip AND the "open in Sequence Edit" bookkeeping (`fx.open_cell/open_row/open_section`, `SeqEditModel::part_index/clip_label`). | End-to-end, click through readback | Closed. |
| Drag a browser style onto a filled/empty cell -> registers real ClipMatrix clip | `grid_panel.cpp:561-573` | `NONE` | NONE | **DEFERRED (drag-drop, needs real UI).** |
| Click scene-header ▶ -> `style section <name>` + `launch scene <s> quantize <n>`, sets `fx.active_scene` baseline | `grid_panel.cpp:397-421` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_repeat_zone_scene_header_next_bar_ui_automation.cpp` (functional/UI-automation) — a REAL injected click on the real scene-header ▶ through a REAL `InProcessBrainSession`. | End-to-end, click through readback | Closed — also see the dedicated next-bar-timing row immediately below (owner task #19). |
| **Owner task #19 PIN: a manual scene-header click while playing must take effect at the NEXT BAR, never later** (owner-perceived symptom: "waits until the cell/section ends") | Same trigger as above; the next-bar contract is `Arranger::request(section, immediate=false)` (`arranger.hpp`) + `launch scene <s> quantize 1` -> `Boundary::kNextBar` (`clip_matrix.hpp`'s `arm()`/`on_bar()`), NEVER `GridModel::kDefaultSceneBars` (8 bars, a GUI-only auto-song bookkeeping length) | **CLOSED 2026-07-17 — PIN RAN GREEN, NOT RED:** the same `test_repeat_zone_scene_header_next_bar_ui_automation.cpp` measures, in real bar units read back from the real engine's own `section`/`clip` OutEvents, the gap between the click and the FIRST observable flip of both `AppState::section()` and `AppState::clip_state(...)`. Both flips landed within 1 bar of the click, on 5 consecutive runs. **This test does NOT reproduce the owner's reported symptom** at the engine/GUI-readback layer — see Torquato's own QA report for the two real test-locator bugs this pin surfaced and fixed along the way (a shared window-size-starvation defect, and a color-cluster collision between the scene-header caret and two unrelated same-colored widgets), and for the honest alternate theory left open (the previous scene is never explicitly stopped when a new one launches — a real but DIFFERENT defect from a next-bar-quantize violation, not exercised by this pin). | End-to-end, click through readback, real bar-unit timing | GREEN as authored. If the owner still perceives the symptom live, the next step is either a `main()`-level headless repro (closing the `SONOTRON_GUI_MAX_FRAMES` gap noted in §2g) or confirming the alternate "previous scene never stopped" theory — both are new work, not a fix to this pin. |
| Double-click scene header -> inline rename, commit on Enter/focus-loss, Esc cancels | `grid_panel.cpp:343-366`, `393-396` | `NONE` for the INTERACTION; the underlying `GridModel::set_scene_name` + persistence IS covered (see scenes.json row below) | Partial (persistence proven, interaction not) | **DEFERRED (needs real UI to prove the double-click/Esc/Enter state machine itself)** — the underlying mutation is already provably correct, which is the higher-value half. |
| Drag "variations" onto scene header -> sets column SectionType | (duplicate of the browser-side row above) | see above | see above | see above |
| M/S latch click -> real `part <token> mute\|solo on\|off` + `PartsModel` toggle | `grid_panel.cpp:460-469` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_grid_ms_latch_ui_automation.cpp` (functional/UI-automation) — REAL injected clicks on the M latch (drums row) and the S latch (bass row) through a REAL `InProcessBrainSession`, asserting the real `PartsModel` toggle AND that the real engine accepts the wire verb without a warn/error `OutEvent` (an explicit, documented scope limit: there is still no shipped mute/solo readback verb on the wire, so an independent engine-side CONFIRMATION beyond "accepted, no warning" is not yet possible). | End-to-end for the click + model + accept-without-warning; readback-confirmation not yet possible (wire gap, not a test gap) | Closed to the limit the wire protocol currently allows. |
| **Auto-song toggle button -> arms/disarms `fx.auto_song`, resets bookkeeping bars (no send)** | `grid_panel.cpp:228-234` | `test_grid_panel_auto_song.cpp:87-91` (`click_arm_auto_song`, a hand-written field-mutation stand-in — explicitly documented as "there is no click-injection seam") | STUBBED BY NECESSITY (owner-accepted: no ImGui click-injection seam exists; both the stubbed and real-backend test reuse the identical mutation) | Acceptable as-is; this is the one part of the auto-song story that genuinely needs real UI to close fully. **DEFERRED per owner steer.** |
| **Auto-song advance -> fires `style section <name>` once the active scene's section elapses (THE PINNED LIVE BUG)** | `grid_panel.cpp:270-312` (`update_auto_song`), reads `app_state.bar()` | **TWO tests exist, same CTest label, opposite fidelity:**<br>1. `test_grid_panel_auto_song.cpp` (functional) — `SpyBrainSession` (permanent no-op `poll()`) + hand-injected `apply_line(R"({"ev":"beat",...})")` straight into `AppState`. **STAYED GREEN through the entire live-bug window** because it never touches the real engine thread, the real ring, or the real decode.<br>2. `test_grid_panel_auto_song_real_backend.cpp` (functional) — REAL `InProcessBrainSession`, real `"transport start"` send, real per-frame `poll()->apply()->render_grid_panel()` pipeline (main.cpp:696-697's own shape), 6 real wall-clock seconds, asserts both `bar()` climbs AND `fx.active_scene != 0`. | **Test #1 is FALSE-CONFIDENCE** — the canonical example this whole task exists to name. **Test #2 is END-TO-END** and is the correct shape going forward. | **THIS IS THE CASE STUDY.** Mechanism: rename/re-comment convention (§1 above) so `functional` no longer silently conflates the two; keep BOTH tests (the stubbed one still proves the pure decision logic in isolation, which is legitimate and fast — it should just never be mistaken for proof the live pipeline works). |
| Auto-song bookkeeping surviving a transport stop/restart (bar rewinds to 0) | `grid_panel.cpp:283-292` | **CLOSED 2026-07-17:** `apps/gui-sonotron/tests/test_grid_panel_auto_song_stop_restart_ui_automation.cpp` (functional/UI-automation) — goes further than the gap asked for: REAL injected clicks (not `session.send()` calls) drive Play -> Stop -> Play through a REAL `InProcessBrainSession`, then asserts auto-song genuinely advances again post-restart (`update_auto_song`'s bar-rewind re-anchor guard). `test_grid_panel_auto_song.cpp`'s stubbed sibling test remains as a fast, legitimate pure-decision-logic check (never to be mistaken for this end-to-end proof, per this doc's own §1 convention). The auto-song ARM itself is still a direct `V02State` field write (`click_arm_auto_song`) — no click-injection seam exists yet for `ImGui::SmallButton`'s alpha-0-at-rest fill, see the taxonomy note above. | End-to-end for Play/Stop/Play and the re-anchor; the auto-song arm click itself still stubbed (documented gap, not this test's to close) | Closed to the limit the current click-injection harness allows. |
| Beat-synchronized playhead sweep on the active cell | `grid_panel.cpp:188-199`, `619-633` (`section_playhead_phase`) | `test_grid_model.cpp` covers `section_playhead_phase` as a pure unit; no functional/visual proof it is actually invoked correctly from `render_grid_panel` | Partial (pure math proven, render-time wiring not) | Low priority beyond what the auto-song real-backend test already indirectly exercises (it does call `render_grid_panel` every frame while playing, so a phase regression would likely surface there too, just not asserted explicitly). |
| Zoom +/- cell size (local only) | `grid_panel.cpp:251-257` | `NONE` | N/A (pure UI cosmetic, no backend/model effect) | None needed. |

### 2d. Sequence Edit zone (`src/seqedit_panel.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| Opening a cell shows its real resolved note content (piano-roll canvas) | `seqedit_panel.cpp:44-47`, `preview_for(...)` | `test_preview.cpp` (unit) pins `preview_for` is real/deterministic; the CANVAS DRAW itself (steps/pitches -> rectangles) is untested | Partial — the DATA is proven real, the RENDER is not | Low priority; a render bug here would be visually obvious and cheap to catch manually; not worth a headless pixel-diff mechanism today. |
| Mode tab piano-roll/step (local only) | `seqedit_panel.cpp:82-87` | `NONE` | N/A (pure local state, `SeqEditModel::set_view` unit-tested generically via `test_seqedit_model.cpp`) | Adequate. |
| Playhead sweep while playing | `seqedit_panel.cpp:144-149` — **NOTE:** driven by `std::fmod(fx.time, 2.0F) / 2.0F`, i.e. WALL-CLOCK frame time, NOT `app_state.bar()`/beat-synced like the grid's own playhead (`grid_panel.cpp:188-199` was explicitly retired FROM this exact wall-clock scheme, per that file's own comment: "Retired: the former neon::sweep_bar(..., fx.time, ...) call... unrelated to tempo or the section length") | `NONE` | NONE — and this is a **suspected latent inconsistency**, not a confirmed bug: the grid's playhead was deliberately moved OFF wall-clock time for exactly the reason the Sequence Edit canvas's playhead still uses it. This is a process gap in the requirements-capture sense (§ below), not just a test gap. | **Flag for owner**, do not silently "fix" (out of scope for a process analyst) — capture as a requirement decision: should the Sequence Edit playhead be beat-synced like the grid's, or is wall-clock intentional here? **If beat-synced is wanted, hand a red-before-green repro to Torquato once the owner rules.** |

### 2e. Parts zone / rail (`src/parts_panel.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| DRUMS/BASS/CHORD amount knobs | `parts_panel.cpp:41-51`, explicitly "local-only intent", no send | `NONE` | N/A (no backend effect exists to verify; this is pinned in the file's own header comment as by-design) | None needed unless/until this becomes a real wire verb — at which point it re-enters the inventory as a new flow requiring the same real-backend discipline as the transport/grid sends. |

### 2f. Intention zone / rail (`src/intention_panel.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| FOLLOWS/NEXT chord cards (real chord-followed readback) | `intention_panel.cpp:48-63`, reads `app_state.chord_followed_current()/next()` | `test_app_state.cpp` covers the `kChordFollowed` reduction (unit); no functional proof the CARD renders it correctly | Partial — data proven real, display not | Low priority, cosmetic risk only. |
| XY pad (valence/energy, local only) | `intention_panel.cpp:68`, `neon::xy_pad` | `NONE` | N/A (no backend effect) | None needed. |
| ENERGY/TENSION/VALENCE knobs (local only) | `intention_panel.cpp:79-83` | `NONE` | N/A | None needed. |
| "live" / "at rest" harmony-activity gate | `intention_panel.cpp:38-45`, `app_state.harmony_active()` | Likely covered by `test_app_state.cpp`'s activity-gate tests (not individually confirmed line-by-line here) | Presumed adequate (unit) | None beyond confirming coverage exists — low priority to re-verify. |

### 2g. Menu bar / app lifecycle (`main.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| App boot: load `layout.json` (create default if missing) | `main.cpp:515-524`, `load_or_create_default` | `test_layout_roundtrip.cpp` (functional): real temp file, real disk round trip | End-to-end for the FILE I/O; never exercised through the actual `main()` entry point itself | Adequate for the library seam; `main()` itself has NO test at all (see next row). |
| App boot: legacy/incompatible `schema_version` -> reset to default + self-upgrade on disk | `layout_json.cpp` (via `load_or_create_default`) | `test_layout_schema_upgrade.cpp` (functional) | End-to-end (real temp file) | Adequate. |
| App boot: `layout.json`/`scenes.json` load, font load, window creation, GLFW/GL init, full frame loop | `main.cpp:478-740` (the entire `main()`) | `MANUAL ONLY` — no automated test drives `main()` at all. The `SONOTRON_GUI_MAX_FRAMES` / `SONOTRON_GUI_SCREENSHOT` env-var escape hatches (`main.cpp:163-198`) exist FOR EXACTLY THIS PURPOSE but **are not invoked from `scripts/ci.sh`, `scripts/coverage.sh`, or `.github/workflows/build-release.yml`** (confirmed by grep — no hit for either variable outside `main.cpp` itself) | NONE — a genuine, structural verification hole: the app's own boot-to-first-frame path is proven only by a human launching it | **Real gap, distinct in kind from the others: this is a MANUAL-ONLY flow with a ready-made automation hook that nobody wired up.** Recommend (not authored here): a headless smoke — `SONOTRON_GUI_MAX_FRAMES=N SONOTRON_GUI_SCREENSHOT=<path> gui-sonotron` run in CI, asserting exit code 0 and that the PNG was written and is non-trivially-sized (not a full golden-pixel diff, just "it booted and drew something"). This needs an EGL/offscreen-GL-capable CI runner — **flag for owner approval** if the current Linux CI runner lacks a GL context (untested by this analysis; Corelli/Giotto should confirm feasibility before this is scheduled). |
| `--control <path>` mode: pure client of an external `sonotron-server` | `main.cpp:569-577`, `control_path_from_args` | `test_uds_brain_session.cpp` (unit): real `AF_UNIX` listen socket, synchronous, in-process | End-to-end for the `UdsBrainSession` class itself; never exercised through `main()`'s own arg-parsing/branch selection | Adequate at the class level; `main()`'s own branch-selection logic (`--control` vs default) is untested — same class of gap as the boot flow above, lower priority since it is a simple `if`. |
| File > Load SoundFont... | `main.cpp:329-354`, `render_soundfont_dialog` | Device-independent `AudioBackend`/`SoundfontEngine` smoke test lives OUTSIDE this app now, at `components/platform/audio/tests` (per `tests/CMakeLists.txt:89-93`'s own comment) | Moved out, not this app's to re-verify | Out of scope for this doc (belongs to `components/platform/audio`'s own inventory). |
| File > Quit -> `quit_requested` (GUI window close ONLY, never a bare `quit` on the socket) | `main.cpp:383-385`; the blacklist itself lives in `BrainSession::send()` | `test_uds_brain_session.cpp` pins "the quit/exit blacklist... a GUI window-close or a stray Enter must never tear down the shared host" | End-to-end for the blacklist logic; the MENU CLICK -> `quit_requested` -> loop-exit wiring itself untested (trivial boolean, low risk) | Adequate; low-priority residual gap only. |
| View > Intention/Parts visibility toggle | `main.cpp:397-406`, mutates `Zone.visible` | `NONE` directly for the toggle; layout SAVE/LOAD of `visible` is covered generically by `test_layout_nested_split.cpp` (unit, pure `compute_rows()` math) and the roundtrip test | Partial | Low priority. |
| Transport menu mirrors | (already listed under Transport zone above) | | | |
| App exit: persist `layout.json` + `scenes.json` | `main.cpp:719-736` | Persistence mechanics covered generically by `test_layout_roundtrip.cpp` / `test_scenes_json.cpp`; the EXIT-TIME call site itself (inside `main()`) is untested | Partial (library proven, call site not) | Same class as the boot-flow gap; folds into the same "smoke-test `main()` headlessly" recommendation above. |

### 2h. Persistence (`layout.json` / `scenes.json`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| Layout load/save round trip (window size, font size, zone geometry) | `layout_json.cpp` / `layout_model.cpp` | `test_layout_roundtrip.cpp` (functional, real temp file), `test_layout_json.cpp` (unit, text<->struct), `test_layout_model.cpp` (unit, pure computation) | End-to-end | Adequate — this is the model flow to imitate elsewhere in the app. |
| Nested vertical split / `visible` toggle math | `layout_model.cpp`'s `compute_rows` | `test_layout_nested_split.cpp` (unit) | End-to-end for the pure math (no ImGui needed, correctly scoped) | Adequate. |
| Scenes.json round trip (renamed scene column survives save/load) | `scenes_json.cpp` | `test_scenes_json.cpp` (functional, real temp file) | End-to-end | Adequate. |

### 2i. Backend / engine plumbing (`src/in_process_brain_session.cpp`, `src/brain_event_from_outevent.cpp`, `src/uds_brain_session.cpp`)

| Flow | Trigger path | Verification today | Fidelity verdict | Gap / recommended mechanism |
|---|---|---|---|---|
| `InProcessBrainSession::start()` spins the engine thread; `send()`->ring->tick->OutEvent->`poll()`->decode round trip | `in_process_brain_session.cpp` | `test_in_process_brain_session.cpp` (functional) — the Gate-2b smoke, no ALSA device required | End-to-end (real thread, real ring, real decode) | Adequate; this is the file the auto-song real-backend test's own header comment credits as the shape it reuses. |
| `midi-source load <path>` replay round trip | `in_process_brain_session.cpp:837-1015` | `test_in_process_brain_session.cpp` (functional, `GUI_SONOTRON_TEST_MIDI_FIXTURE`, reuses `components/platform/midisrc/tests/fixtures/tiny.mid`) | End-to-end for the BACKEND verb; **there is no GUI panel/button that sends `"midi-source load ..."` at all** — confirmed by grep, zero hits outside `in_process_brain_session.cpp` itself | Not a GUI-user-facing flow YET (backend-only, reachable only via the raw `send()` grammar). Correctly excluded from "needs a click test" — **flag for the owner/requirements ledger**: is a GUI affordance for this planned, or is it CLI/automation-only by design? |
| Audio routing: `style load` auto-routes the default band -> primary-port MIDI reaches the audio ring | `in_process_brain_session.cpp` (`kDefaultStyleRoutes`) | `test_audio_primary_port_reachable.cpp` (functional) — explicitly documents flipping a prior RED pin to GREEN (Giotto closing Torquato's QA gap) | End-to-end | Adequate; a clean example of the red-before-green discipline actually operating (RED pin -> real fix -> GREEN, file renamed to drop "unreachable" from its name). |
| Audio routing negative case: non-primary-port MIDI must NEVER reach the ring | same file | `test_audio_port_gate.cpp` (functional) | End-to-end for the negative case; the file's OWN header comment states there is deliberately no positive case in it (split cleanly into the sibling test above) | Adequate — good example of two tests each owning one half of a gate rather than one test trying to prove both directions loosely. |
| `--control` client round trip (connect/send/poll against an external `sonotron-server`) | `uds_brain_session.cpp` | `test_uds_brain_session.cpp` (unit, real `AF_UNIX` socket, synchronous) | End-to-end (real socket, no sleeps/threads) | Adequate. |
| Looper backend verbs (`loop new`/`record`/`stop`/`erase`/`undo`/`length`) | `in_process_brain_session.cpp:565-620`ish | `test_in_process_brain_session.cpp` (functional): `test_note_raw_reaches_engine_via_loop_record_round_trip`, `test_loop_verbs_translate_and_reach_engine_without_error` | End-to-end at the BACKEND — **but `AppState` has no per-slot loop recording-state view yet** (`app_state.cpp:140-145`'s own comment: "item 10/11's GUI panel work, a later slice") and **no gui-sonotron panel exposes a Looper button today** (confirmed: zero non-comment hits for a Looper UI affordance in `src/*_panel.cpp`) | **Not yet a user-facing GUI flow** — correctly out of the "needs a click test" set until a panel exists. **Flag for the requirements ledger**: this is exactly the kind of "objective partially captured" case Guido exists to catch — the Looper is `docs/DESIGN.md` §13 / node 6000, "◑ partial (Phase 7 SLICE 1 shipped)" — the ENGINE side has acceptance criteria and tests; the GUI-EXPOSURE side does not yet have its own capture entry with its own acceptance criteria. When that slice is scoped, it should get its OWN inbound-capture pass (§3 template below), not be assumed "done" because the engine half is green. |
| `brain_event.cpp`'s JSONL wire parser (LineBuffer framing + flat-JSON decode) | `brain_event.cpp` | `test_brain_event.cpp` (unit) | End-to-end for the KNOWN-SHAPE cases (5 original + chord-followed + beat + error); **no fuzz/malformed-input coverage** confirmed | Partial — this is the one clear FUZZ CANDIDATE in the whole inventory (an untrusted-shaped text parser). **Flag for owner**: worth a fuzz harness (host-only, dependency question: does the project already have a fuzzer wired anywhere, e.g. libFuzzer via a sanitizer build? Not confirmed by this read — if none exists, introducing one is a new host-tool dependency and must be costed/approved, not assumed free). Dependency-free alternative: a small handwritten "malformed-input corpus" unit test (missing keys, wrong types, truncated JSON, embedded nulls) achieves most of the value without a new tool. |

---

## 3. Count and top gaps

**~55 distinct user-facing (or backend-adjacent) flows inventoried** across 9
zones/subsystems (Transport, Browser, Repeat Zone/Grid, Sequence Edit, Parts,
Intention, Menu/lifecycle, Persistence, Backend plumbing).

**Top gaps, ranked by how many days they could cost:**

1. **The auto-song false-confidence pair** (§2c) — the exemplar this task was
   commissioned to name. Both tests are labeled `functional`; only the header
   comments distinguish them. This is the single highest-value finding: the
   SAME failure mode (a stubbed send/backend, still green) could recur
   silently anywhere else in the grid/transport/browser click-handler flows
   that currently have `NONE` verification (the filled-cell click, the
   scene-header ▶ click, the M/S latch click — all listed `NONE` above,
   not `STUBBED`, because no test of any fidelity exists yet for them at
   all).
2. **`main()` itself has zero automated coverage** (§2g) — the boot-to-
   first-frame path, including layout/scene load, font load, GL/GLFW init,
   and the full per-frame poll/apply/render loop, is proven only by a human
   launching the app. The `SONOTRON_GUI_MAX_FRAMES`/`SONOTRON_GUI_SCREENSHOT`
   hooks exist precisely to make this automatable and are wired nowhere in
   CI. This is the largest "manual only" surface in the inventory.
3. **Zero `regression`-labeled tests in gui-sonotron** — `scripts/coverage.sh`
   metric 3 (bug-regression census, "meant to be monotonically
   non-decreasing") reports ZERO for this app's own test tree, even though
   at least two real, owner-reported live bugs were found and fixed here
   this cycle (the auto-song advance bug, and the primary-port-audio
   unreachability bug that `test_audio_primary_port_reachable.cpp`'s own
   header comment documents as a RED-to-GREEN flip). Both bug-fix tests were
   labeled `functional`, not `regression` — the census that is supposed to
   make red-before-green "operate as a process, not stay aspirational" is
   invisible for this app specifically, even though the DISCIPLINE was
   followed. This is a pure labeling/bookkeeping gap, cheap to close.
4. **Several real wire-sends with `NONE` coverage at any layer**:
   `"transport continue"` (menu-only, never tested), `style switch <name>`
   (the live-morph browser click, distinct code path from `style load`),
   `panic`, the BPM/transpose nudge clamps.
5. **A suspected latent inconsistency, not yet a confirmed bug**: the
   Sequence Edit canvas's playhead (`seqedit_panel.cpp:144-149`) still uses
   wall-clock `fx.time`, the exact scheme the Repeat Zone's own playhead was
   deliberately RETIRED from (per `grid_panel.cpp`'s own comment) in favor of
   beat-synced `section_playhead_phase`. Flagged for the owner as a
   requirements question, not silently fixed.

---

## 4. What I flagged / what is the owner's call

- **Owner fork:** should `functional` stay a single label with a textual
  "real vs stubbed" convention (my recommendation, §1), or should a
  mechanical `functional-stubbed` opt-in label be introduced now rather than
  later? I did not use AskUserQuestion for this because it is answerable from
  reading the existing convention and its cost/benefit is stated above; if
  the owner disagrees with the recommendation, that is their call to make,
  not mine to force.
- **Owner fork, flagged not decided:** is a CI-wired headless
  `SONOTRON_GUI_MAX_FRAMES`/`SONOTRON_GUI_SCREENSHOT` smoke run (closing gap
  #2 above) feasible on the current CI runner (does it have a usable
  GL/EGL context)? This needs a feasibility check from Corelli/Giotto
  before scheduling — I did not assume an answer.
- **Tool/dependency flag:** a fuzz harness for `brain_event.cpp`'s JSONL
  parser (gap in §2i) would be a NEW host-only tool/dependency (e.g.
  libFuzzer) if none already exists in this project — flagged for owner
  approval under the CLI-deps policy, with a dependency-free alternative
  offered (a handwritten malformed-input corpus as a plain unit test).
- **Requirements-ledger item, not a test gap:** the Looper's GUI-exposure
  half (§2i) has no acceptance criteria of its own yet — the engine half is
  green and tested, but "expose the Looper in the GUI" has not been captured
  as its own objective with its own DoD. This is exactly the class of gap
  Guido's mandate exists to close; flagged for the owner to decide whether/
  when to open that capture.
- **"Must be proven" items handed to Torquato** (not authored here): every
  row above marked "Gap: hand to Torquato" — in priority order: (1) the
  filled-cell click / scene-header ▶ click / M/S-latch click real-backend
  tests (closes the largest false-confidence blast radius, mirroring the
  already-proven auto-song pattern), (2) `transport stop`/`panic`/`transport
  continue`/`style switch` real-backend round trips, (3) the auto-song
  stop/restart case through the REAL backend (currently only proven
  stubbed), (4) a malformed-JSONL corpus for `brain_event.cpp` (or a fuzz
  harness, pending the tool-dependency decision above).
- **Explicitly NOT proposed for construction now (owner: real UI, deferred):**
  every drag-and-drop flow (style-onto-cell, style-onto-scene-header,
  variation-onto-scene-header), the inline scene-rename double-click/Esc/
  Enter state machine, and the auto-song arm-click itself — all genuinely
  need a real click/drag-injection seam this codebase does not have, and
  building one is out of scope per owner steer this pass.

---

## 5. Torquato pass addendum (2026-07-17)

Closed 6 of the flows this doc handed off (rows updated in place above):
transport Play/Stop, the filled-cell launch+open-Sequence-Edit click, the
scene-header ▶ click (including a dedicated timing pin for owner task #19),
the M/S latch click, auto-song surviving a real stop/restart, and the
browser style-switch-while-playing flow. All 6 are real injected-click
tests against the real production render functions and a real
`InProcessBrainSession`, via the new `imgui_headless_harness.hpp` seam.

**Independent finding, fixed as test infra (not a product bug):** the
pre-existing shipped `test_grid_panel_auto_song_real_backend.cpp` sibling,
`apps/gui-sonotron/tests/test_repeat_zone_playhead_ui_automation.cpp`, was
ALSO silently broken in the tree before this pass — the same
window-size-starvation defect this pass root-caused elsewhere (a missing
`ImGui::SetNextWindowSize` before `ImGui::Begin("test")` left the grid body
starved, `window->SkipItems` true, zero vertices past `y~115`). Fixed with
the same one-line addition already used correctly by
`test_grid_cell_preview_vs_seqedit_ui_automation.cpp`; verified GREEN.

**Task #19 verdict:** the pin (§2c, new row) ran GREEN, not RED. The
manual scene-header click's real engine/GUI readback (`section()` and
`clip_state(...)`) flips within 1 bar of the click on 5 consecutive runs.
This test does not confirm the owner's reported symptom at this layer —
see that row for the two test-locator bugs this investigation found and
fixed along the way, and the one alternate theory (previous scene never
explicitly stopped) still open and unexercised.

---

**Doc path:** `docs/proposals/flow-verification-matrix-2026-07.md`

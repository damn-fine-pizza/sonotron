# Completed-node evidence archive

This file holds the full forensic evidence (commit hashes, file paths, test
names, scoping notes) for roadmap nodes that are **✅ done** and whose
detailed history was trimmed out of the live tree to keep
`docs/roadmap.md` lean. `docs/roadmap.md` is the canonical status document —
this file exists only so a done node's proof-of-work is not lost, not to
duplicate status tracking. If a node's status ever needs re-litigating,
start here for the receipts, then update the one-line record in
`docs/roadmap.md`.

Archived 2026-07-18 (branch `gui-sonotron`, HEAD `a0517af`) as part of the
`docs/roadmap.md` rewrite that made it the single canonical roadmap (see that
file's own header for the reconciliation record).

---

## 6000 — Looper: full evidence for the four ✅ leaves

- `6100` Record / overdub / replace / erase / undo — ✅ done: `LoopBuffer`
  (`components/core/arrangrr/include/arrangrr/loop/loop_buffer.hpp`), commit
  `eaf9d50` "add node 6000 Looper primitive (Phase 7 SLICE 1)" —
  `kLoopNew`/`kLoopRecordStart` (record/overdub/replace)/`kLoopRecordStop`/
  `kLoopErase`/`kLoopUndo` = ABI Param 59–63 (`abi.hpp:330-350`). Tests:
  `test_loop.cpp`, `test_loop_ops.cpp`,
  `test_loop_wrap_and_idempotency_regression.cpp`. Playback-capacity
  hardening in `bc693d1`.
- `6200` Quantize-after (non-destructive) — ✅ done: `kLoopRecordStop`'s
  quantize-grid argument, same commit `eaf9d50` (`abi.hpp:342-345`,
  "non-destructive to event count/positions").
- `6300` Retroactive capture (always-on ring, "grab last N bars") — ✅ done:
  `RetroCaptureRing`
  (`components/core/arrangrr/include/arrangrr/loop/retro_capture.hpp`),
  commit `95a3a7d` "add node 6300 retroactive capture ('grab last N
  bars')" — `kRetroCaptureArm`/`Disarm`/`Grab` = ABI Param 69–71
  (`abi.hpp:391-399`); hardened by QA commit `6e89bda` "keep the
  most-recent tail on a dense retro-capture grab (6300 QA)" +
  `test_retro_capture.cpp`/`test_retro_capture_ring.cpp`.
- `6400` Loop length (fixed/auto/quantized), per-track/global — ✅ done:
  `kLoopLength` = ABI Param 64 (`abi.hpp:351-354`), `LoopLengthMode`
  0=auto (content-derived) / 1=fixed (explicit tick length) / 2=quantized
  (snap to grid), same commit `eaf9d50`.

---

## 8000 — Structure & recall: full evidence for 8100/8200/8500

- `8100` Scenes / song mode (snapshot + chain + tempo/time-sig) — ✅ done
  (Phase 7, commit `67fdd13` "add SceneChain (node 8100, Scenes/song mode)
  + re-anchor the bar-boundary gate"): `SceneChain`
  (`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`) is a
  bounded, ordered chain of steps, each holding a `PerformanceStore` slot
  index (the "snapshot") + its own `TimeSig` ("tempo/time-sig" — tempo
  rides the referenced Performance), advancing at bar boundaries via
  `Engine::fire_scene`/`Engine::scenes()` (`engine.hpp:151-152,608-611`).
  ABI `Param::kSceneAdd`/`kScenePlay`/`kSceneStop`/`kSceneClear` = 65–68
  (`abi.hpp:364-383`). Tests: `test_scene.cpp` (220 lines),
  `test_scene_hardening.cpp` (341), `test_scene_meter_gate_regression.cpp`
  (283) — 844 lines combined. Supersedes a former "explicitly NOT built by
  Phase-5 Item #9" note: that note correctly described `Performance`'s own
  deliberately narrower scope AT THAT TIME (commit `e980665`,
  2026-07-14); a later, separate Phase-7 pass (commit `67fdd13`,
  2026-07-15) built node `8100` itself. `SceneTransitionKind` ships one
  value (`kCut`, a hard switch) today — a future crossfade kind is
  reserved as metadata, not required by this node's own description.
- `8200` Performance/Registration (recall live state) — ✅ done (Phase-5
  Item #9, commit `e980665`, 2026-07-14): `Performance` POD (96 B,
  `components/arrangrr/include/arrangrr/perf/performance.hpp`) snapshots
  style/variation/per-role routes/mute+solo/groove/tempo/key/chord-mode/
  chord-follow/chord-sequence; `apply_performance` validates every
  referenced id before applying anything (atomic recall, no half-applied
  rig). ABI `Param::kPerformanceStore`/`kPerformanceRecall` (Param 51–52,
  `abi.hpp`); host `perf store|recall|save|load` verbs
  (`components/hostrt/shell_pad_commands.cpp`). Tests:
  `test_performance.cpp`, `test_performance_validate.cpp`,
  `test_performance_style_id_regression.cpp` (91/91 host green per the
  commit message). Scoped narrower than DESIGN.md §17's full sketch
  (Corelli-reviewed choice): `master_transpose` is reserved with no
  engine backing yet, routing is per-role Arranger routes only (not the
  general Router thru-matrix/Zones), and `scene_refs[]` (song mode) stays
  under `8100`, not built here.
- `8500` Versioned binary storage + CRC (save/load round-trip) — ✅ done
  (Phase-5 Item #9, commit `e980665`, 2026-07-14):
  `serialize_performance`/`deserialize_performance`
  (`components/arrangrr/include/arrangrr/perf/performance.hpp`,
  `components/arrangrr/src/performance.cpp`) write/read an explicit
  field-by-field little-endian wire format with `magic`/`format_version`/
  CRC32 trailer (not a struct memcpy, so `GrooveParams` padding and
  host/arm layout can't desync). Round-trip proven by
  `test_performance_wire.cpp`; file I/O is host-only (`perf save`/`perf
  load` verbs). Scoped to the `Performance` object only — the broader §21
  multi-table Project binary format (styles/patterns/programs/device
  profiles/songs) that `8400` would need remains unbuilt.

---

## 9100 — Per-style feel: full evidence for 9110/9120/feel-genre swing

- `9110` Style owns its default GrooveParams — ✅ done (`Style::groove`,
  ABI/struct-additive, seeded into the arranger on every style load/switch).
- `9120` Style owns its tempo — ✅ done (`Style::tempo`, wired through
  `Engine::apply_style_tempo` → `Transport::set_bpm` on load/switch, no new
  ABI; Ottorino's per-style tempos applied to all 16 builtins; `latin` held
  at 120 BPM behind an owner TODO).
- Feel-genre swing (swing/shuffle/blues) — ✅ done. Each of the three now
  seeds its `.groove` (swing 62/12, shuffle 72/10, blues 75/10,
  `swing_grid=8`) and its note table was re-authored (Rule R: swung eighths
  moved from steps 3/7/11/15 onto the even off-8ths 2/6/10/14 that
  `groove::apply` actually swings; fills/pickups/chromatic-approach
  exceptions preserved). The other 13 styles stay byte-identical. Three
  regression goldens (`feel_swing/shuffle/blues`) lock the swung ticks.
  Spec in `docs/style-corpus-and-generation.md` §5 (Swing re-authoring
  spec).

---

## 9210 — Motif + transforms: full evidence

`9210` Motif + transforms (diatonic transpose/retrograde/displacement,
seeded) — ✅ done. Engine:
`components/core/arrangrr/include/arrangrr/arranger/motif.hpp`, commit
`6fe5869` "add generative motif engine first slice (9210)";
`test_motif.cpp` (487 lines). Owner directive recorded in
`docs/reflections/phase7-9210-motif-authoring-all16.md`: "9210 is 'done'
only when the motif engine is wired into all 16 built-in styles, not just a
blues demonstrator." Now true: commit `f850ea2` "wire MotifSpec into 12
built-in styles (Phase 7, node 9210, Option-1 batch)" attached existing
per-style content as motif seeds across 12 styles; commit `417a674` "wire 4
Option-2 motif::generate leads (bossa/samba/funk/ballad)" added the last 4
(generated, not authored, seeds), "closes the last 4 of the 16-style kLead
model gap"; commit `f46d758` un-held samba's Finding-B-flagged bass motif;
golden regression `690b35c` locks the 4 new lead fixtures (26/26 goldens
green, per its own commit message). Verified against the tree: every one of
the 16 `arranger/styles/*.hpp` headers carries at least one `.motif=&...Spec`
attachment; 15/16 also carry a `TrackRole::kLead` motif — `basic` is the
deliberate exception (no genre convention to generate toward, per the
authoring plan's own §3.16), matching `3260`'s already-established "basic
kept as baseline" precedent, not a residual gap.

**Note on scope (owner-locked, 2026-07-18 pass):** `9210`'s motif/transform
work is the SHIPPED slice of "evolves over time" (fills at phrase
boundaries, variation, motif development) — the OPEN generative work is
`9220` (offline-trained Markov/grammar) and `10000` (Director); the
never-loops-byte-identically-to-infinity property is not yet built. See
`docs/roadmap.md`'s `9000` band header note.

---

## 9310 — Accompany stylizer: full evidence

`9310` Accompany (keep the melody, play the genre band under detected
chords) — ✅ done HOST-ONLY. End-to-end: `components/orchestrator`'s
`AccompanyPipeline` (MIDI-source → chorddet → arrangrr,
`components/orchestrator/include/orchestrator/accompany.hpp`) drives the
band from chords detected FROM the imported melody, not a scripted steer —
all five sub-phases committed: 4a `Pipeline<StageT...>` composite in
`components/runtime` (`8a0f701`), 4b `ChordDetector`+`FollowedContext`
promoted to `components/chorddet` (`65f16bc`), 4c the SMF parser +
MIDI-source stage extracted to `components/midisrc` (`85cd454`), Phase 4d
stands up the 3-stage pipeline (`17f8f43`), Phase 4e wires melody-driven
detection (`2e55d9b`). Proven by `tests/golden/accompany_basic.golden` +
`tests/golden/accompany_melody_detect.golden` (both green, `ctest -R
accompany`). No ABI break: `test_abi_frozen` untouched,
`sizeof(OutEvent)==16` unchanged — the ABI waiver this pipeline could have
spent (a stage/source tag) stays UNSPENT, per the ABI-fork analysis in
`docs/architecture.md` §16.2.

---

## 11400/11500 — full evidence

- `11410` Piano visualizer GREEN+AMBER — ✅ done (green note-on now bold;
  new `kMidiNotePending` amber role; `PianoChordOverlay` from existing host
  chord state, no core ABI; committed-green gated OFF at rest — lit only
  when transport plays or the chord is explicitly steered).
- `11500` UDS-JSONL control adapter (one protocol, three consumers) — ✅
  done, and extended (Phase 3): `kParamState` given a wire shape
  (`param_state_wire.{hpp,cpp}` + `param_state_mirror.{hpp,cpp}`, commit
  `807c140`) and `Param::kNoteRaw` + the `note` L1 verb added so
  `apps/tools/cli-arrangrr --connect` runs as a genuinely arrangrr-free
  pure socket client (`TuiClient`+`UdsClient`+`client_event`,
  `apps/tools/cli-arrangrr/client_mode.{hpp,cpp}`, commit `b98dbda`) — both
  additive, `test_abi_frozen`-safe (`sizeof` unchanged). Deliberately
  scoped: per `b98dbda`'s own commit message, does NOT split
  `hostrt::Shell` or retire cli-arrangrr's embedded live mode; `--connect`
  is a flat REPL, not full TUI parity. `components/hostrt/shell.hpp`'s
  `Shell` is still one class (unsplit, verified). Scoped as a DEFERRED
  OPTIONAL follow-up (owner-decided 2026-07-13), not outstanding work
  against this node.

---

## 11610 (formerly the pre-split `11600`) — full mechanical/GUI-foundation history

**Tech stack.** DECIDED & vendored: Dear ImGui (upstream `ocornut/imgui`,
pinned v1.92.8) + GLFW3, backends `imgui_impl_glfw` / `imgui_impl_opengl3`,
under `third_party/imgui` + `third_party/glfw` (each with an
`ARRGRR_VENDOR.md` pin), built and linked by `apps/gui-sonotron/`
(dependency fork resolved under `0800` and executed in code; rationale
as-built in `docs/gui-and-ux.md` §1).

**Mechanical strand COMPLETE** — G0 concept demolition `38b5826`, G1
workstation layout `c49f8c6`, G2 brain session + G3 zone panels `473ab60`
(66/66 host tests green); `docs/gui-and-ux.md` records "mechanical strand
COMPLETE — G0, G1, G2 and G3 all DONE".

**Core-dependent strand.** `kChordFollowed` (P0-1, `52008e4`) and
`kBeat`/position (P0-2, `ba568ca` + `f4c6188`) wired end-to-end
(`apps/gui-sonotron/src/brain_event.cpp`, `app_state.cpp`,
`transport_panel.cpp`; `test_chord_followed_event`, `test_brain_event`,
`test_app_state`, `golden_chord_followed` all green) — the harmony
visualizer and the live playhead are real, not placeholders.

**Repeat Zone (2026-07-16).** The clip/scene launch primitive gap is
CLOSED for slices 1–3 of `docs/proposals/repeat-zone-real-contract.md`.
Commit `3398f04` "Repeat Zone real — readback + Shape-A clip binding" wires
`AppState` to a real per-clip `{id -> LaunchState}` map reduced from the
existing `clip` `OutEvent` (replacing a local click-time echo with honest
core readback) and gives `kClipAdd` an explicit-`id` form
(`ClipMatrix::add_at`) so the GUI's grid cells register real content
instead of addressing an empty pool slot; commit `f531d8f` "renamable scene
columns with scenes.json persistence" adds slice 3 (host-only scene naming
+ persistence, zero ABI). Tests: `test_app_state.cpp`,
`test_in_process_brain_session.cpp`, `test_grid_model.cpp`,
`test_scenes_json.cpp`. Slice 4 ("auto-song") was originally sketched as a
"new grid-column active-scene cursor auto-advancing at bar/section
boundaries" (`repeat-zone-real-contract.md` §5/§8b decision 4) — that FSM
design is SUPERSEDED by the Song-mode Phase 1 adoption of core `SceneChain`
(see below); the auto-advancing cursor is real, but core-driven, not the
bespoke FSM originally planned. By explicit owner decision (§8b decision
2), in-app step/chord/loop authoring of a cell's own content from inside
the GUI stays out of scope for this workstream — a cell's content is real
only for a style dropped from the Browser; every other content kind is
still CLI/script-only. Toolkit dependency flag: RESOLVED / vendored.

**Song-mode Phase 1 (2026-07-18).** Commit `465bb48` ("adopt core
SceneChain as the song engine (Song-mode Phase 1)") replaces the
hand-rolled per-frame auto-song FSM (double section-trigger, immediate
ClipMatrix clip promotion clobbering the queued section, per-launch
re-anchor) with the core `SceneChain` primitive (node `8100`) as the GUI's
real song engine, driven by a new host wire verb `song build <count>
<section> <bars>...`; `grid_panel.cpp` gains
`build_and_play_song`/`reconcile_active_scene`, reading the engine's REAL
reported Arranger section instead of a per-frame guess. The two
SpyBrainSession-based tests that pinned the retired FSM
(`test_grid_panel_auto_song.cpp`,
`test_grid_panel_auto_song_launches_next_scene.cpp`) are retired;
`test_song_mode_scenechain_contract.cpp` is the new contract pin (single
`song build`, no launch-scene/style-section double-send, monotonic scene
transitions, harmony continuity across a scene boundary).

**Browser redesign F1/F2 (2026-07-18).** Phase F1 (commit `bce71ab`) adds a
category selector combo + per-tab search and wires the long-standing
Sections/Variations "click does nothing" gap to the existing `style
section <name>` verb, plus a new Voices/GM-program picker sending `program
<port>[:ch] <voice>` (`Param::kProgram`); Phase F2 (commit `6a8799e`) adds a
real GM percussion-Kit category on the SAME `program` verb (channel 10, no
new ABI Param), backed by 9 canonical GM2 kit names added to
`components/platform/hostrt/gm_program.{hpp,cpp}` (proposal:
`docs/proposals/browser-redesign-taxonomy.md`).

**Seqedit column-view + Repeat-Zone zoom (2026-07-18, commit `2bcfd4d`).**
Wires a Repeat-Zone cell click to a whole-column highlight and a
Sequence-Edit column-view with visibility-only checkboxes
(`seqedit_model.{hpp,cpp}`, real mute/solo unchanged on the grid M/S
squares) and raises the zoom cap +3 notches with a sub-linear font scale.

**Architecture fact (Phase 2a/2b, owner-decided).** The GUI now hosts the
engine IN-PROCESS by default — a dedicated thread driven by lock-free SPSC
Command/OutEvent rings; `apps/gui-sonotron/CMakeLists.txt`'s
`gui_sonotron_engine` library links `hostrt`/`runtime`/`arrangrr` directly
(commits `bf2c4b2` Phase 2a `sonotron-server`, `8c9e54d` Phase 2b
in-process integration), with `--control <path>` kept as an alternative
pure-client mode against an external `sonotron-server`. D38 is retired for
that one library only — every other GUI library
(`gui_sonotron_models`/`brain`/`layout`/`screenshot`) stays core-free. The
core ABI itself is UNCHANGED by this: `abi.hpp` stays FROZEN v1,
`sizeof(OutEvent)==16`, `test_abi_frozen` intact.

---

## 11700/11710/11720/11730 — full Phase-5 program inventory

`11730`'s Phase-5 program (owner-ordered, 2026-07-13) — the owner selected
and ORDERED eight of Verdi's ten Phase-5 candidates
(`docs/phase5-plan.md`): `1→7→8→2→9→6→4→10` — Restyle (`9320`) · Motif
(`9210`) · Corpus import (`9400`/`9430`) · Clip/launch primitive (no
canonical node assigned at the time) · Pad/Scene (`7200`/`8100`–`8200`) ·
`melodd` (`0910`) · Fuzzing harness (no canonical node assigned at the
time) · MIDI-FX chain (`5000`). Full detail: `docs/phase5-plan.md`.
Deferred out of this program: #3 STM32 bring-up (`12100`), #5 external
clock-in (`4500`).

Shipped inventory: the Fuzzing harness — `components/midisrc/fuzz/`,
`option(SONOTRON_FUZZ)`, commit `c2251f2`. Pad/Scene (`7200` partial,
`8200`/`8500` done at the time of commit `e980665`; `8100` itself shipped
separately in a later Phase-7 pass — commit `67fdd13`). Motif (`9210`) —
commits `f850ea2`/`417a674`/`f46d758`/`690b35c`. MIDI-FX chain core
(`5100`) — commit `95f542b`. In-flight, not yet on this branch at the time
of the previous reconciliation pass: the corpus-import lowering first
slice (commit `f6611e5`, on sibling worktree branch
`worktree-agent-a3f3c787996a2023e`, pending cherry-pick — STILL pending as
of this 2026-07-18 pass, re-verified: `git merge-base --is-ancestor
f6611e5 HEAD` returns false on `gui-sonotron`). Also resolved by this
program: the ABI freeze LIFTED for Phase-5 and Verdi's fork #2 (the
`sonotron`/workstation audio destination) answered by including `melodd`
(see `0910`).

**`11720` at-the-freeze-line actions, full text:**
- Froze the current ABI command/event surface (`0700`): `abi.hpp` carries a
  FROZEN-v1 banner with the additive-only invariant, and
  `test_abi_frozen.cpp` compile-time-pins every id value, `kWarnCodeCount`,
  `sizeof(Command)==20`, `sizeof(OutEvent)==16` and `kProtocolVersion==1` —
  a breaking change now fails the build (fix = append, or bump to v2).
  Corelli's verdict held: the ABI is additive/healthy, so later features
  (`5000`/`6000`/`8000`/`10000`) extend it.
- Reserved the `5100` MIDI-FX shape as ABI-none: `constexpr kMaxInserts=8`
  + a RESERVED block documenting the future per-track `kFx…` verbs and
  their `(idx,a,b,c)` packing, no live enum values — the GUI is born
  aware, appended when `5000` lands.
- Phase-5 update: this freeze is LIFTED for Phase-5 items going forward.

# Repeat Zone made REAL — as-built contract + gap analysis

Status: SCOPE DOC (read-only architecture analysis, Corelli). No product code
touched. This is a contract + gap map for Giotto to implement against, not
an implementation.

Scope trigger: owner-approved workstream — bind Repeat-Zone launch cells to
real `ClipMatrix` content, make the 5 columns real renamable scenes, add an
auto-song toggle with bar/section-boundary scene-chain advance.

---

## 0. Where this sits on the roadmap

`docs/DESIGN.md`'s own "Constraints & flags that gate ordering" section
(`docs/DESIGN.md:1265-1286`) records the **GUI freeze line (`11700`)** as
already crossed ("the freeze line `11700` is ready to cross", `:1286`) and the
project now in the `11600` Host GUI client era. This workstream — binding an
existing host GUI surface to existing core primitives — is squarely inside
that sanctioned phase, not a doctrine violation.

**A stale-docs finding, stated once and not re-litigated below:** `docs/
DESIGN.md:899` and `docs/roadmap.md:284` both still mark node `8100`
("Scenes / song mode") as `○ SHIPPABLE ... Still planned, unchanged`, citing
`Performance`'s header as proof it's unbuilt. That citation is now **stale**:
`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`,
ABI `Param::kSceneAdd/kScenePlay/kSceneStop/kSceneClear = 65..68`
(`components/core/arrangrr/include/arrangrr/abi.hpp:341-360`),
`Engine::scenes()`/`fire_scene` (`components/core/arrangrr/include/arrangrr/
engine.hpp:151-152,608-611`), and three test files (`test_scene.cpp`,
`test_scene_hardening.cpp`, `test_scene_meter_gate_regression.cpp`, 844 lines
combined) all show node `8100` **shipped** in a prior Phase 7 pass. The docs
tracker simply never got updated. Side that should yield: **the roadmap doc**
— it should be corrected to ✅, independent of this workstream (a note for
whoever owns `docs/roadmap.md`/`docs/DESIGN.md` upkeep; not mine to edit).

This matters directly for §3 below: the owner's "scene-chain auto-advance"
ask can be answered precisely only once it's clear that `SceneChain` already
exists — but, critically, **answers a different question than the one the
Repeat Zone needs answered.**

---

## 1. As-built inventory — the core

**`ClipMatrix`** (`components/core/arrangrr/include/arrangrr/clip/
clip_matrix.hpp`) is the Repeat-Zone launch primitive. Its own header states
the scope tripwire explicitly (`:16-19`): *"ClipMatrix owns ONLY {content ref,
launch state, pending boundary}. NO record field on Clip, NO capture()
method"*. `Clip` (`:55-80`) is a 12-byte POD: `{part_role, scene_index,
kind (ContentKind), content_index, state (LaunchState), n_bars,
due_bar_index}`. `ClipMatrix::add()` (`:88-99`) assigns a **sequential pool
id in REGISTRATION ORDER** — the id is *not* derived from `{role, scene}`,
it is whatever order `add()`/`kClipAdd` calls happened to arrive in.
`ClipMatrix::arm()`/`force()`/`on_bar()` (`:113-170`) drive the
armed→playing / queued-stop→stopped bar-boundary state machine; `on_bar` is
called from `Engine::on_tick`'s existing bar gate, **before** `fire_arranger`
(`clip_matrix.hpp:44-46`).

`ContentKind` (`clip_matrix.hpp:30-40`) has four values, each dispatched by
`Engine::apply_clip_content` (`components/core/arrangrr/src/engine.cpp`,
`apply_clip_content`):
- `kStyleSection` → `Arranger::request(section, immediate=true)` — a full
  style-section switch (hand-authored, hosted in the loaded Style).
- `kChordSequence` → `ChordSequencer::use()`/`play()` — a recorded chord
  progression (see `docs/reflections/phase7-scope-6000-8100-clip-timeline-
  seam.md:53-71`).
- `kStepTrack` → toggles `Timeline::Track::mute` — the "content" is an
  **always-running, hand-authored step grid** (`timeline.hpp:66-74`,
  `set_step()` at `:103-127`); "launching" this clip means *unmuting an
  already-looping track*, not starting fresh playback. Musically distinct
  from the other three kinds — worth knowing before designing one uniform
  "playing" semantic across ContentKinds.
- `kLoopBuffer` → `LoopBuffer::start_playback`/`stop_playback` (Phase 7 node
  6000, the Looper — a captured note loop).

**Registration is HOST/SCRIPT-only.** `clip add <role> <scene> style|seq|
track <selector>` is an L1 shell verb (`components/platform/hostrt/
shell_clip_commands.cpp:35-79`), dispatched to ABI `Param::kClipAdd = 44`
(`abi.hpp:189-197`). **The GUI never sends `clip add`** (confirmed by grep
across `apps/gui-sonotron/`) — grid_panel.cpp only ever sends `launch clip
<id>`/`launch scene <n>`. There is also **no startup init script** that
populates `ClipMatrix` for the GUI's benefit: `main()` (`apps/gui-sonotron/
main.cpp:565-572`) starts either `InProcessBrainSession` or connects to an
external `sonotron-server` — neither path runs a `clip add` batch. The only
`.arrangrr.init` auto-load is the **live TUI**'s own convenience
(`docs/arrangrr.init.example:1-7`), a different binary. **Net effect: when
gui-sonotron launches today, `ClipMatrix` is empty; every `launch clip <id>`
the grid sends addresses a pool slot that does not exist**, and
`ClipMatrix::get()`/`arm()` return false/no-op silently (`clip_matrix.hpp:
101-124`) — no crash, no visible effect, no error surfaced to the GUI either
(`cmd_clip` in `engine.cpp` doesn't emit a warn on an unknown id — verify at
implementation time, flagged below).

**Readback already exists on the wire, contrary to the GUI's own stale
comment.** `ClipMatrix::on_bar`'s `on_due` callback and `force()` transitions
are wired to emit a real `OutEvent` — `apps/gui-sonotron/src/brain_event.hpp:
26-28,141-148` documents the decoded shape: `{"ev":"clip","id":N,"state":
"stopped"|"armed"|"playing"|"queued_stop","@":tick}`. **This is a per-clip
playing/stopped readback, already on the wire, already decoded into
`BrainEvent::clip_id`/`clip_state`.** The gap is entirely on the *consuming*
side (§2).

**`SceneChain`** (`components/core/arrangrr/include/arrangrr/scene/
scene_chain.hpp`) is the node-`8100` primitive: a `StaticVector<SceneStep,
kMaxScenes>` of `{TimeSig, performance_slot (PerformanceStore index),
n_bars, transition}` (`:51-58`), advancing **one BAR at a time**
(`on_bar()`, `:110-127`), started by `play()` (`:89-98`, fires step 0
synchronously), stopped by `stop()` (`:102`), **no implicit loop** — the
chain holds the last scene forever once it ends (`:33-36,121-124`). ABI:
`kSceneAdd/kSceneClear/kScenePlay/kSceneStop = 65-68` (`abi.hpp:341-360`).
Driven from `Engine::on_tick`'s existing bar gate via `fire_scene`
(`engine.hpp:367-372,608-611`), the **same bar-boundary mechanism**
`ClipMatrix::on_bar` and `fire_arranger` already use.

**`SceneChain` is explicitly, deliberately NOT the GUI's grid-column
"Scene".** Three independent citations converge on this, none of them mine to
resolve — this is a **locked decision already in the tree**:
1. `scene_chain.hpp:12-15`: *"Engine-owned peer of ChordSequencer, NOT wired
   through ClipMatrix (Fork B, RESOLVED = own transport) — distinct from the
   GUI's own grid-column 'Scene' (`kSceneQuantize`/`kSceneColumn`),
   performance.hpp's own header tripwire already draws that line."*
2. `performance.hpp:12-18`: *"Deliberately NOT the GUI's Repeat-Zone 'Scene'
   (a clip-column, `apps/gui-sonotron/src/grid_model.hpp`) and NOT DESIGN.md
   section 7's Song-anchored timed Scene (node `8100`...) — `Performance` is
   the only word this file uses for its own concept (locked decision)."*
3. `abi.hpp:338-340`: `kSceneQuantize = 47` *"stays the GUI's own grid-column
   'Scene'"*, separate from `kSceneAdd`..`kSceneStop = 65-68`.

**Consequence verified in code, not assumed**: `Engine::capture_performance`/
`apply_performance` (`engine.cpp:1645+,1722+`) never touch `m_clips` — a
`SceneChain` step recalls routing/mute/groove/style/transpose/chord state,
**never which `ClipMatrix` cells are playing.** SceneChain cannot represent
"these 6 Repeat-Zone cells are lit in column 3" — it operates one whole
abstraction layer up (rig-wide macro state), and its `PerformanceStore` slots
(`kMaxPerformances = 16`, `config.hpp`) have no concept of clip launch state
at all.

**`Op::kGet` (`abi.hpp:49-53`) is dead code.** Grepped across the whole
`Engine` dispatch (`engine.cpp`): `cmd.op` is **never read anywhere** —
dispatch is keyed entirely on `cmd.param`. There is no synchronous
query/response path in the ABI today; every piece of state the GUI knows
about the core arrives via an async `OutEvent` pushed on a state transition
(exactly like `kClip` above). **Any new readback must follow that same
push-event shape** — building a working `kGet` request/response path from
scratch is a new subsystem, not a small addition, and is explicitly out of
scope for this workstream unless the owner wants to open it.

---

## 2. As-built inventory — the GUI

**`GridModel`** (`apps/gui-sonotron/src/grid_model.hpp`) is pure host data:
`std::string`/`std::vector` cells, 9 `TrackRole` rows × up to 8 scene
columns. Its own header comment is candid about the gap: *"the cell does not
yet reference a real core object, since none exists until the clip primitive
lands"* (`:33-35`) and *"Registering each cell's own content with the core's
ClipMatrix... is a follow-up, not yet done here"* (`:44-46`).
`kGridLaunchWired = true` (`:28`) only certifies that the **launch verb**
round-trips for real — not that it addresses real content.

**`grid_panel.cpp`** computes the id it sends as `cell_id(role_index, scene,
scene_count) = role_index * scene_count + scene` (`:55-57`) and sends
`"launch clip " + id` on a filled-cell click (`:306-307`). This assumes — as
a bare, un-enforced convention, not a contract anything checks — that
`ClipMatrix` ids are assigned in exactly this row-major order. Nothing in the
tree establishes that invariant (§1: no registration call exists at all from
the GUI side).

The scene-header loop (`grid_panel.cpp:218-244`) sends `"launch scene " + s +
" quantize " + n` (→ `kSceneQuantize`, the **grid-column** meaning, correctly
— not `SceneChain`). It draws a bare `s+1` number and a green triangle; there
is **no name storage, no rename affordance, no double-click handler**
anywhere in this file or in `GridModel`.

**On-click "playing" state is 100% local echo, not a readback.**
`grid_panel.cpp:308`: `fx.row_playing[r] = playing ? -1 : static_cast<int>
(s);` — set optimistically the instant the button is clicked, never
corrected by the real `kClip` OutEvent. `AppState::apply` (`apps/gui-sonotron/
src/app_state.cpp:100-107`) confirms this in its own comment: `kClip` is
logged (line 32/102) but produces **no other view-state change**, with the
comment *"a live per-cell armed/playing indicator on the grid itself is
follow-up work (GridModel has no runtime launch-state field yet...)"*.
`V02State`'s own comment (`v02_state.hpp:48-52`) additionally says *"there is
no per-cell playing readback on the wire"* — **this specific claim is
stale/imprecise** given §1's finding that `kClip` IS on the wire; the true
gap is narrower than the comment states (consumption, not transport).

**Sequence Edit is a procedural preview widget, not a content editor.**
`seqedit_panel.cpp:103`: `neon::clip_pattern(neon::hash_label(model.
clip_label()))` — the piano-roll/waveform rendered is **deterministically
derived from the clip's label string**, nothing else. There is no note
add/remove/drag affordance anywhere in this file; it is read-only rendering.
**There is currently no authoring path into a `ClipMatrix`-referenced content
store (Timeline step grid, ChordSequence, LoopBuffer) from the GUI at all** —
content is authored today only via the CLI/script L1 verbs (`seq step`/
`chord rec`/`loop record` families in `components/platform/hostrt/`), never
from gui-sonotron.

**Content source today is a browser drag or a demo seed, both local-only.**
`grid_panel.cpp:316-324`: dropping a style from the browser sets
`GridCellKind::kStyleSection` with the style's display name as the label —
sets **only the GUI's own display cell**, never calls `clip add`. `seed_demo`
(`:63-98`) does the same for a hard-coded demo pattern. Neither path ever
reaches the core.

---

## 3. The cell ↔ ClipMatrix binding contract

**Decision needed, not resolvable by convention alone: who assigns the id,
and when.** Two shapes, both structurally sound, with different cost:

**Shape A — GUI registers on demand, tracks the id itself.** On first fill of
an empty cell (browser drop or "+", `grid_panel.cpp:299-303`), the GUI sends
`clip add <role> <scene> <kind> <selector>` (the EXISTING `kClipAdd` verb,
`shell_clip_commands.cpp`), and needs the **assigned id echoed back** so it
can store `{role,scene} → clip_id` in `GridModel`. **Gap**: `kClipAdd` today
has *no return-value echo* by design (`abi.hpp:189-194`: *"mirrors kSeqNew's
own established convention of an implicit SEQUENTIAL id with no
return-value echo"*) — the host is expected to *count* registrations itself.
For a single always-CLI host this works; for a GUI that must reconcile its
own row-major cell index against a core-side sequential counter across
process restarts (no ClipMatrix persistence exists either — it's pure
runtime state, not part of `Performance`/any save format), this is fragile.
**NEEDS-NEW-ENGINE-VERB**: either (a) an explicit `id` argument on
`kClipAdd` (GUI picks `cell_id(role,scene)` itself, core validates
uniqueness/bounds) — the cleaner fix, since `GridModel` already computes a
stable deterministic id from `{role,scene}` today; or (b) a genuine
echoed-id `OutEvent` on registration (new `OutEvent::Kind`, mirrors
`kSection`'s reporting shape, `abi.hpp:359-364`). **Recommend (a)**: it turns
the GUI's already-assumed row-major convention into an *enforced* contract
instead of a silent hope, and needs no new OutEvent kind — just one new
optional `Command` field or a repurposed one (`Command` has headroom under
its `sizeof <= 20` pin, `abi.hpp:437` — verify exact byte budget at
implementation time).

**Shape B — a fixed startup script pre-registers all 45 possible cells (6
rows × up to 8 scenes, mirroring `kRows` in `grid_panel.cpp:46-53` and
`GridModel::kMaxSceneCount = 8`) with placeholder empty content, and the GUI
only ever *retargets* an existing slot's content.** Would need a NEW verb
(`kClipRetarget`/similar — there is no "change an existing clip's content
reference" verb today; `kClipAdd` only appends). More core surface, but
avoids the id-echo problem entirely (ids are stable from process start,
known to both sides by construction). **NEEDS-OWNER-DECISION** between A and
B — this is a real architectural fork, not a detail: A keeps `ClipMatrix`
append-only (matches its current shape exactly, zero new verbs beyond an
id-argument tweak); B adds a retarget verb but removes the id-echo problem
structurally. **Recommendation: Shape A** — it costs less new ABI surface
and `ClipMatrix`'s whole design (`add()` only, no update-in-place method
anywhere) is already append-only; forcing retarget-in-place fights the grain
of the existing primitive.

**Content authoring: still an open surface regardless of A/B.** Whichever
shape wins, "drag a style from the browser" only ever produces
`ContentKind::kStyleSection` (an existing Style's section — no new authored
musical content, just a reference to something already loaded). Real
per-cell *authored* content (a bespoke step pattern, a recorded chord
sequence, a captured loop) requires Sequence Edit to become a genuine editor
— **that is a separate, larger workstream** (wiring `seq step`/`chord rec`/
`loop record` verbs to real mouse/keyboard gestures in `seqedit_panel.cpp`,
which today has zero write-affordances, §2). **NEEDS-OWNER-DECISION**: does
"cell content is real" for THIS pass mean *only* "drag-a-style → real
ClipMatrix registration + real launch + real readback" (achievable now), or
does it also require in-app authoring of step/chord/loop content (a much
larger, separate follow-up)? This proposal's build plan (§6) assumes the
former; the latter is out of scope here and should be its own workstream.

**Readback: SHIPPABLE now, no new engine verb needed.** The `kClip`
`OutEvent`/`BrainEvent` already carries `{clip_id, clip_state}` (§1). The fix
is entirely GUI-side: `AppState::apply`'s `kClip` case
(`app_state.cpp:102`) needs to update a `{clip_id → LaunchState}` map (new
`AppState`/`GridModel` field, host-only, no core touch), and `grid_panel.cpp`
needs to read `playing`/`opened` off that map instead of `fx.row_playing`'s
local echo. Trivial once Shape A/B gives every cell a real, known id.

---

## 4. The scene model — names

**Names have no core precedent to reuse blindly, but a directly-applicable
one exists elsewhere in the core: `Performance::name`.**
`performance.hpp:85`: `char name[24]{};  // host-set display name (D26: the
core ABI never carries a string...`. This is the load-bearing existence
proof that **a bounded, fixed-size display name CAN live in a core POD
without violating D4/no-heap/freestanding** — it's a `char[24]`, not
`std::string`, set only by the host, never interpreted by the core.

Two structurally valid placements for scene names, genuinely a product
decision, not a technical one:

- **Host-only (GridModel gains `std::array<std::string, kMaxSceneCount>
  scene_names`).** Zero core touch, zero ABI change, zero dependency risk.
  Cost: scene names are **not part of any save/restore path** unless the
  owner also wires them into whatever project-persistence format ships
  (there isn't one for the GUI's own state yet — `layout.json` only persists
  window geometry, not scene identity). **A renamed scene reverts to "1..5"
  on every GUI restart** unless persistence is added alongside.
- **Core-resident (`char name[N]` on a new small scene-descriptor struct,
  mirroring `Performance::name`'s exact precedent), keyed by scene_index,
  living beside `ClipMatrix` or as a tiny new pool.** Survives whatever the
  core's own save/load format covers (today: nothing yet touches
  `ClipMatrix`'s runtime state at all — it isn't part of `Performance` or any
  serialized project; see §1). Costs a genuinely NEW small ABI surface
  (`kSceneName` set-verb + a small bounded pool, `kMaxSceneCount` entries ×
  ~24-32 B — trivially small against the STM32H743 512 KB envelope, same
  cost class as `Performance::name`'s own 24 B).

**Recommendation, stated as a recommendation, not a decision I'm entitled to
make**: since **stable identity across launches** is explicitly named in the
owner's brief ("Where does scene identity live so it's stable across
launches") and the GUI currently has **zero persistence for anything scene-
related**, host-only naming is the cheaper SHIPPABLE-now answer but does
*not* satisfy "stable across launches" without separately adding GUI-side
persistence (a small, host-only, unrelated piece of work — a `scenes.json`
sibling to `layout.json`, or an extension of it). Core-resident naming
satisfies stability for free (whatever already persists `ClipMatrix`/project
state persists it too) but that persistence path **does not exist yet
either** — `ClipMatrix` itself is pure runtime state today, never
serialized. **NEEDS-OWNER-DECISION**: this is really a question about
whether the *whole* Repeat-Zone grid (not just scene names) is meant to
survive a restart in this pass. If not, host-only + no persistence is the
right, small SHIPPABLE-now scope; if yes, this workstream's true size is
larger than "rename a column" and should say so explicitly before Giotto
starts.

---

## 5. Scene-chain auto-advance — the central finding

**The owner's "auto-song ON = scenes (the 5 columns) auto-advance at bar/
section boundaries" and the core's existing `SceneChain` (node `8100`) are
NOT the same capability**, despite sharing the word "scene" on the wire
(`kSceneQuantize=47` vs `kSceneAdd..kSceneStop=65-68`, both literally
present in `abi.hpp`, §1). This is not my speculation — it is a **locked,
deliberate separation already written into three files** (§1, citations 1-3).

- `SceneChain` advances through a chain of **`PerformanceStore` snapshots**
  (rig-wide: routing, mute/solo, groove, style/variation, transpose,
  chord-mode/follow, FX inserts) — verified NOT to touch `ClipMatrix` state
  at all (`capture_performance`/`apply_performance`, §1).
- The Repeat-Zone's 5 **grid columns** are `ClipMatrix::scene_index` values,
  fired today via `kSceneQuantize` — a single flat launch-everything-in-
  this-column verb, no chaining, no auto-advance, no "next column" concept
  anywhere in `ClipMatrix`/`Engine`.

**If "auto-song" is meant to literally cycle which grid column is active**
(the Ableton-Live "scene launch, auto-follow" reading — very plausibly what
the owner means, since that's the standard meaning of "scene" in exactly
this kind of launch-grid UI), **that is a genuinely NEW capability. Nothing
in `SceneChain` gets you there** — reusing it would silently redefine "scene"
a third way and reintroduce the very ambiguity the codebase went out of its
way to kill (§1, citation 3). Building it correctly means a NEW, small,
additive mechanism, structurally cheap because it mirrors an existing
precedent exactly:

- A new engine-owned cursor (`std::uint8_t m_active_scene`) + an `auto_song`
  bool flag, living beside `ClipMatrix`/`SceneChain` in `Engine` (same
  placement discipline as every other Engine-owned subsystem, `engine.hpp:
  896,934`).
- Driven from the **same bar-boundary gate** `ClipMatrix::on_bar`/
  `SceneChain::on_bar`/`fire_arranger` already share
  (`Engine::on_tick`, `engine.hpp:367-374`) — this is the cheapest possible
  addition precisely because the hook point already exists and is already
  proven safe for exactly this kind of "do something every N bars" logic.
  Advancing "every bar" vs "every section boundary" (the owner's own phrase)
  needs a decision: `SceneChain::SceneStep::n_bars` gives a clean per-scene
  hold-length precedent to copy (`scene_chain.hpp:54`) if section-boundary
  granularity is wanted per scene, or a single global bar-count if uniform.
- Firing the next column reuses the **existing** `kSceneQuantize` dispatch
  path verbatim (`Engine`'s handling of `Param::kSceneQuantize`) — no new
  clip-firing logic, just a new trigger source for the same effect.

**If, instead, "auto-song" is meant as "chain whole rig-state scenes
(SceneChain, already built) and the 5 Repeat-Zone columns are a red
herring/separate concern"** — then the feature is **almost entirely
SHIPPABLE TODAY**: `kSceneAdd`/`kScenePlay`/`kSceneStop`/`kSceneClear`
already exist, are tested (844 lines across 3 test files), and just need a
GUI surface (an "auto-song" toggle button sending `kScenePlay`/`kSceneStop`,
plus authoring `kSceneAdd` calls from *something* in the GUI — itself a
smaller open question than a whole new engine mechanism).

**This is the single owner-decision that determines whether this feature
costs a new engine primitive or almost nothing.** I flag it as
**NEEDS-OWNER-DECISION**, not resolvable from the tree alone, because the
tree genuinely supports both readings and the owner's brief uses "scene" in
exactly the ambiguous way the ABI comments warn about.

---

## 6. New wire surface required — enumerated

| # | Surface | Kind | Why | Label |
|---|---|---|---|---|
| 1 | `kClipAdd` gains an explicit `id` argument (GUI-chosen, e.g. `cell_id(role,scene)`), core validates bounds/uniqueness instead of always-append | ABI change, additive-compatible if the new arg defaults to "append" when absent | Removes the id-echo fragility in §3 Shape A | NEEDS-NEW-ENGINE-VERB (small) |
| 2 | GUI-side `{clip_id → LaunchState}` consumption of the EXISTING `kClip` OutEvent | Host-only, zero ABI | Real per-cell readback | SHIPPABLE now |
| 3 | Scene name storage | Host-only (`GridModel`) OR new `kSceneName` verb + small core pool (mirrors `Performance::name`) | §4 fork | NEEDS-OWNER-DECISION (placement), then either SHIPPABLE (host) or NEEDS-NEW-ENGINE-VERB (core) |
| 4 | GUI-side scene-name **persistence** (a `scenes.json` sibling to `layout.json`, or extending it) if names must survive restart and stay host-only | Host-only, zero ABI | §4 "stable across launches" | SHIPPABLE now (small, separate from the core question) |
| 5 | Auto-song engine mechanism (active-scene cursor + auto-advance flag + bar-boundary driver reusing `kSceneQuantize`'s dispatch) | NEW ABI verb(s): e.g. `kAutoSongEnable` (set, a=0/1), reuse of existing `kSceneQuantize` firing | §5, IF "scene" means grid-column | NEEDS-NEW-ENGINE-VERB + NEEDS-OWNER-DECISION (§5 fork must resolve first) |
| 6 | Auto-song via existing `SceneChain` (`kScenePlay`/`kSceneStop` as the toggle) | none — reuse as-is | §5, IF "scene" means SceneChain/Performance-chain | SHIPPABLE now, contingent on §5 fork |
| 7 | `Op::kGet` | dead ABI value, no dispatch anywhere | Explicitly flagged per the task brief | FLAG ONLY — do not build a synchronous query path for this feature; every readback here rides the existing push-`OutEvent` shape (item 2) |

No item above requires a new host or core **dependency** — every piece is a
bounded POD/verb addition in the exact style of the primitives already in
the tree (`ClipMatrix`, `SceneChain`, `Performance::name`). If Giotto's
implementation of any of these discovers a need for something heavier (e.g.
a real JSON library for scene persistence beyond what `layout_json.hpp`
already hand-rolls), that is a **new flag for the owner at that time**, not
assumed here.

---

## 7. Dual-target check

Every surface in §6 is either:
- **HOST-ONLY** (GUI consumption of `kClip`, scene-name storage/persistence
  if host-only) — never crosses into `components/core/`, no freestanding/
  no-heap concern at all.
- **Core-side but bounded POD, mirroring existing precedent exactly**
  (`kClipAdd` id argument, a scene-name `char[N]` pool, an `m_active_scene`
  cursor + bool flag) — same cost class as `ClipMatrix`/`Performance::name`/
  `SceneChain`, all of which are already `static_assert`-pinned, bounded,
  and proven to cross-build `arm-none-eabi` (they ship today). Nothing
  proposed here introduces a container, an allocation, a float, or an
  exception/RTTI dependency on the realtime path.

No item touches `-fno-exceptions`/`-fno-rtti` discipline, no item adds
heap, no item is STM32-infeasible. The one item worth a second look at
implementation time (not a blocker, a note): if auto-song's bar-boundary
driver (§5, item 5) fires `kSceneQuantize` internally rather than through
the normal command-queue path, confirm it goes through the SAME
`EventSink`/`schedule_or_warn` choke point every other emission already
uses (`ClipMatrix::on_bar`'s `on_due` callback already does this correctly
for ordinary clip launches — the new auto-advance driver should call the
exact same dispatch, not invent a second path).

---

## 8. SHIPPABLE-now / NEEDS-NEW-ENGINE-VERB / NEEDS-OWNER-DECISION — the split

**SHIPPABLE now (no new ABI, host-only work):**
- Wire `AppState`/`GridModel` to consume the existing `kClip` OutEvent into
  a real per-clip `{id → LaunchState}` map; replace `V02State::row_playing`'s
  local echo with it (§3).
- GUI-side scene-name storage in `GridModel` + double-click-to-rename UI in
  `grid_panel.cpp`'s scene-header loop (contingent on §4's placement
  decision landing on host-only).
- GUI-side scene-name persistence (`scenes.json` or `layout.json` extension)
  if names are host-only (§6 item 4).
- Auto-song toggle button UI (the button itself is trivial either way; its
  wire target depends on §5's fork).

**NEEDS-NEW-ENGINE-VERB (small, additive, mirrors existing shape):**
- `kClipAdd` explicit-id argument (§3, §6 item 1).
- Scene-name core verb + pool, IF the owner picks core-resident naming
  (§4, §6 item 3).
- Auto-song active-scene cursor + advance mechanism, IF "scene" means
  grid-column (§5, §6 item 5) — the larger of the two `NEEDS-NEW-ENGINE-VERB`
  items, genuinely new engine logic, not a verb tweak.

**NEEDS-OWNER-DECISION (blocks sizing/sequencing until answered):**
1. Shape A (id-argument on `kClipAdd`) vs Shape B (pre-registered fixed pool
   + a new retarget verb) for the binding contract (§3).
2. Does "cell content is real" mean drag-a-style-only for this pass, or does
   it also require in-app step/chord/loop authoring in Sequence Edit (a
   separate, larger workstream) (§3)?
3. Scene names: host-only (cheap, non-persistent unless paired with new
   host persistence) vs core-resident (`Performance::name`-style, persists
   for free once/if `ClipMatrix` itself gets a save path — which does not
   exist yet either) (§4).
4. **The load-bearing one**: does "auto-song" mean auto-advancing which
   Repeat-Zone grid COLUMN is active (a new engine mechanism), or does it
   mean chaining whole `Performance` snapshots via the already-built
   `SceneChain` (almost free, wrong granularity for "which cells light up")
   (§5)? This must be settled before Giotto estimates or starts item 5/6
   of §6 — the two answers are not incremental refinements of each other,
   they are different features that happen to share a button label.

---

## 8b. OWNER SIGN-OFF (2026-07-16) — LOCKED

The owner resolved all four §8 `NEEDS-OWNER-DECISION` forks:

1. **Binding contract → Shape A** (explicit `id` argument on `kClipAdd`, GUI
   picks `cell_id(role,scene)`, core validates bounds/uniqueness). NOT Shape B
   — no retarget verb; `ClipMatrix` stays append-only.
2. **"Cell content is real" → drag-a-style ONLY for this pass.** Real
   `ClipMatrix` registration + real launch + real readback for a style dropped
   from the browser (`ContentKind::kStyleSection`). In-app step/chord/loop
   authoring in Sequence Edit is explicitly OUT of scope — a separate, larger
   future workstream.
3. **Scene names → host-only (`GridModel`) + `scenes.json` persistence.** A
   small host-only sibling to `layout.json` so renamed columns survive restart.
   NO core `kSceneName` verb, NO core scene-name pool.
4. **auto-song → the grid-COLUMN reading (Ableton "scene launch, auto-follow").**
   auto-song advances which Repeat-Zone COLUMN is active at bar/section
   boundaries. This is the NEW engine mechanism of §5/§6 item 5 (active-scene
   cursor `m_active_scene` + `auto_song` flag + bar-boundary driver reusing the
   EXISTING `kSceneQuantize` dispatch), NOT `SceneChain`. `SceneChain` (node
   8100) stays a separate concept — this workstream does not touch it.

Resulting locked build surface (from §6): item 1 (kClipAdd id-arg), item 2
(kClip readback consumption, host-only), item 3 → host-only branch, item 4
(scenes.json persistence), item 5 (new auto-song mechanism). Items 6 (SceneChain
reuse) and 7 (Op::kGet) are NOT built. Ordered slices in §9 stand as written.

Separately noted for whoever owns doc upkeep (not this workstream): correct
`docs/DESIGN.md:899` / `docs/roadmap.md:284` node `8100` status ○ → ✅ (§0).

---

## 9. Ordered build plan (for Giotto, once owner decisions land)

1. **Owner resolves §8's four decisions first** — items 2, 3, and especially
   4 change the shape of what gets built, not just its size.
2. **Readback wiring** (`kClip` OutEvent → `AppState`/`GridModel` real
   per-cell state) — zero ABI risk, ships independent of every other
   decision, immediately removes the "local echo" gap regardless of what
   else lands. Do this first; it de-risks and de-scopes everything after.
3. **Binding contract** (§3 Shape A or B, per decision 1) — `kClipAdd`
   id-argument change (or the new retarget verb for Shape B), plus the GUI
   sending real `clip add` on first-fill instead of only touching its own
   `GridModel` cell.
4. **Scene naming** (§4, per decision 3) — host-only rename UI +
   persistence, or the core verb + pool, whichever the owner picked.
5. **Auto-song** (§5/§6 items 5 or 6, per decision 4) — either wire the
   toggle to existing `kScenePlay`/`kSceneStop` (SceneChain reading) or
   build the new active-scene-cursor mechanism (grid-column reading). This
   is last because it is the most expensive item if decision 4 lands on the
   new-mechanism reading, and every earlier step (readback, binding, naming)
   is a prerequisite for the auto-advance to have anything real to show.

---

## What I flagged / dependency stance

No new host or core dependency anywhere in this workstream — every surface
in §6 reuses existing bounded-POD/verb idioms (`ClipMatrix`, `Performance::
name`, `SceneChain`, the existing `OutEvent` push shape). `Op::kGet` is
named and flagged (§1, §6 item 7) as dead ABI surface that this workstream
should NOT be the one to bring to life — every readback need here is already
served by the existing push-event pattern. The one open dependency-adjacent
question (§6 item 4, scene-name persistence) is scoped as "extend
`layout_json.hpp`'s existing hand-rolled reader/writer" — if that turns out
insufficient at implementation time, that is a new flag for the owner, not
assumed settled here.

# Repeat-count Phase 2: core `SceneStep::repeat_count` — ABI decision analysis

Status: analysis only, NOT approved, NOT scheduled. Written by Corelli
(architecture critic) at the owner's request to settle the "BOTH, phased"
question left open when Phase-1 (host-expand) shipped (commit `7784dea`).

Scope: read-only. No product code touched. This document is the ONLY
artifact this review produced.

---

## 1. What is actually built today (ground truth)

### 1.1 SceneStep / SceneChain

`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp:51-58`:

```cpp
struct SceneStep {
  TimeSig time_sig{};                  // 8 B (u8 beats_per_bar + pad3 + u32 ticks_per_beat)
  std::uint16_t performance_slot = 0;  // 2 B
  std::uint8_t n_bars = 1;             // 1 B
  SceneTransitionKind transition = SceneTransitionKind::kCut;  // 1 B
};
static_assert(sizeof(SceneStep) == 12, ...);
```

Verified byte-exact: `TimeSig` (`components/core/common/include/common/time.hpp:53-60`) is
`u8 + pad(3) + u32` = 8 B, alignof 4. `8 + 2 + 1 + 1 = 12`, itself a multiple of 4 — **zero
trailing padding today**. This is the reason the struct sits at exactly 12 B with no slack.

`SceneChain` (`scene_chain.hpp:60-134`) is a bounded `StaticVector<SceneStep, kMaxScenes>`
(no heap, D32), one linear song, no implicit loop (`on_bar()`, lines 110-127): a step holds
for `n_bars` bar-boundary calls, then the chain advances to the next step and fires
`TransitionFn`; reaching the last step simply stops advancing (`m_playing = false`) — the
last `Performance`/`TimeSig` stay in effect forever. This "hold-last-forever" semantic is
the mechanism Phase-1's "infinite" repeat already rides for free.

`kMaxScenes = 64` (`components/core/arrangrr/include/arrangrr/config.hpp:187-191`), pinned
under D33's SRAM discipline: `kMaxScenes * 12 <= 512 KB` — **768 bytes total**. The config
comment (`config.hpp:179-186`) states plainly that this pool is "orders of magnitude
smaller" than the genuinely SRAM-pressured `LoopBuffer` pool, and the 12 B pin itself is
explicitly a **stylistic match** to `LoopEvent`/`ChordStep`'s own 12 B precedent, not a
tight budget wall. Growing `SceneStep` to 16 B (the next alignment step) costs 1024 B total
— still trivial against the 512 KB envelope.

### 1.2 The ABI freeze state (the request's premise does not match the tree)

`abi.hpp:14-45` is unambiguous:

> "v1 ABI — ADDITIVE-ONLY BASELINE, **UNFROZEN for Phase 5** (owner, 2026-07-13)... the
> owner lifted the freeze for Phase 5... `Op`/`Param`/`Command`/`OutEvent` may be reshaped
> wholesale for Phase-5 work."

`SceneStep`/`SceneChain`/`kSceneAdd` are **Phase 7** work (`scene_chain.hpp:10-36`, `abi.hpp:377-405`),
i.e. born entirely inside the unfrozen window and **never shipped in a released build**.
`SceneStep` is not even declared in `abi.hpp` — it lives in its own header, included by
`engine.hpp`, and crosses the GUI/core boundary as a live in-process C++ struct
(`apps/gui-sonotron/src/in_process_brain_session.cpp` links the core directly; there is no
separate shared-library ABI boundary being crossed here today). The task's framing —
"grows the pinned 12-byte SceneStep struct → an ABI decision (frozen v1, additive-only)" —
**restates a policy that the tree's own banner says no longer applies to this surface.**
Corelli's structural verdict: the code is right, the premise handed down was stale. This
is worth surfacing to the owner explicitly rather than silently overriding it.

### 1.3 `Command` operand budget for `kSceneAdd` (opcode 65)

`Engine::scene_add` (`components/core/arrangrr/src/engine.cpp:970-995`) reads exactly three
fields of the 20-byte `Command` (`abi.hpp:466-482`):

- `cmd.a` → `performance_slot`
- `cmd.b` → packed `n_bars` (low byte) | `beats_per_bar` (high byte)
- `cmd.c` → `SceneTransitionKind`

`cmd.idx` (u16, "collection index") and the top-level `cmd.n_bars` (u8, meaningful only for
`Boundary::kNextNBars`) are **both unread by `scene_add`** — confirmed by direct inspection
of the function body, which never references either. `cmd.idx` is genuinely free capacity
on the wire command itself: a repeat-count operand could ride it with **zero growth of
`Command`** (already `sizeof(Command) <= 20`, unchanged since Phase-5 Item #2's own
padding-reuse reshape, `abi.hpp:28-38`).

### 1.4 The "8-entry wire cap" and the silent capacity coupling

Two independent caps exist, in two different layers, and they are only safe because
someone did the arithmetic by hand:

- `GridModel::kMaxSceneCount = 8` (`apps/gui-sonotron/src/grid_model.hpp:81`), mirrored as
  `kMaxSongScenes = 8` in `in_process_brain_session.cpp:158` — the GUI's populated-column
  cap.
- `GridModel::kMaxSceneRepeat = 8` (`grid_model.hpp:191`) — the per-scene repeat cap the "-
  K +" stepper enforces.
- `kMaxScenes = 64` — the CORE chain's own step-count ceiling.

`apply_song_build`'s own comment states the invariant outright
(`in_process_brain_session.cpp:1229-1231`): *"kMaxSongScenes(8) populated columns x
GridModel::kMaxSceneRepeat(8) each == exactly kMaxScenes"* — **Phase-1 host-expand runs at
exactly 100% of the core's step budget by construction, with zero headroom.** The function
does carry a defensive per-call clamp (`steps_emitted < kMaxScenes`, lines 1274-1301) so a
future breach degrades to silent truncation rather than a crash or an ABI warn surfacing to
the user — but nothing statically enforces `kMaxSongScenes * kMaxSceneRepeat <= kMaxScenes`
across the two files that each declare half of the equation. This is a real, if currently
inert, structural coupling: raise either GUI-side constant without also revisiting the
core's `kMaxScenes` and songs start silently losing tail scenes. Flagged here because it
bears directly on the Phase-2 "worth it" analysis (§3) even though fixing it is not itself
an ABI decision.

### 1.5 Wire/serialization format — SceneStep has none

`components/core/arrangrr/include/arrangrr/perf/performance.hpp` is the real precedent for
"what a frozen wire format with a migration story looks like" in this codebase: `Performance`
carries `format_version` (currently 3), a magic (`kPerformanceMagic`, "SNPF"), an explicit
field-by-field little-endian `serialize()`/`deserialize()` (never a `memcpy`/
`reinterpret_cast`, because `GrooveParams` has real inter-field padding that must never
cross the wire — the header comment names this "Corelli fix" directly, `performance.hpp:78-83`),
and a **host-only** migrator: `components/platform/hostrt/perf_v1_migrate.cpp`,
`migrate_performance_v1_to_v2`. The device itself never migrates (`performance.hpp:161-170`:
"the DEVICE never migrates" — Architectural Principle #8, `docs/DESIGN.md:588`); `deserialize()`
hard-rejects any `format_version` it does not recognize.

`SceneStep` has **none of this**. It is not part of `Performance`'s on-disk record, has no
`serialize()`/`deserialize()`, no magic, no `format_version`, and — confirmed by grep —
`apps/gui-sonotron/src/layout_json.cpp` contains **zero references to "scene" in any form**.
`GridModel`'s entire scene-column state (`m_scene_names`, `m_scene_sections`, `m_scene_bars`,
`m_scene_repeat`) is **not persisted to the project file today, at all** — Phase-1's repeat
count, like every other scene-column field, is session-only. The core `SceneChain` is
rebuilt from scratch by `apply_song_build` on every `Play` press or scene-header click
(`in_process_brain_session.cpp:1254-1308`); nothing about a `SceneStep` ever survives a
reload, close, or reopen regardless of which layer (host or core) owns the repeat count.
This is a genuine, citable gap, independent of the Phase-2 decision — but it directly
undercuts any argument that pushing `repeat_count` into the core primitive buys durability:
today, neither layer's scene state is durable.

---

## 2. Is an additive `repeat_count` possible without breaking v1?

Yes, trivially, on both axes, and for a reason stronger than "it fits": **there is no v1 to
break here.** `SceneStep` was never frozen, is not in `abi.hpp`, and was never shipped.

- **Byte layout**: appending `std::uint8_t repeat_count = 1;` after `transition` grows
  `SceneStep` from 12 → 13 B, which the compiler pads to 16 B (alignof 4, inherited from
  `TimeSig::ticks_per_beat`). That is 3 bytes of new trailing padding, not a reshuffle of any
  existing field's offset — genuinely additive at the struct level. `kMaxScenes * 16 = 1024 B`
  — still trivial under D33's 512 KB envelope; the `static_assert` messages ("12 B", "LoopEvent/
  ChordStep precedent") become stale prose that needs rewriting, not a design obstacle.
- **Wire/versioning**: none applies — see §1.5. There is no on-disk format to bump, no
  migrator to write, no `format_version` field on `SceneChain` to introduce (unless the
  owner separately decides scene-column state should start being persisted at all, which is
  a different, larger decision than repeat-count).
- **Firmware/dual-target cost**: `scene_chain.hpp` uses only `StaticVector`, `FunctionRef`,
  and `TimeSig` — dependency-free, no heap, no exceptions, consistent with D4/D32. Grepping
  `apps/firmware` turns up **zero references** to `SceneChain`/`scene_chain` — the firmware
  target does not yet wire Scenes/song-mode into its own runtime at all (Phase 7 node 8100
  compiles under the `arm` CMake preset as part of the core library, but nothing in
  `app/firmware/stub` currently calls into it). A `repeat_count` field costs the firmware
  build nothing beyond the same trivial RAM delta already priced above; it changes no
  cross-build behavior because there is no cross-build behavior yet to change.
- **`kSceneAdd` operand budget**: `cmd.idx` is free (§1.3). A repeat-count operand can ride
  the EXISTING `Command` shape with no struct growth at all, if the chosen implementation
  needs one (see the fork in §3.3 below — it may not).

So the mechanical answer to "can this be added additively" is an unqualified yes, and it is
cheaper than the request's framing assumed. That is not, on its own, a reason to build it.

---

## 3. THE KEY QUESTION: is the core primitive worth it?

### 3.1 What Phase-1 (host-expand) already delivers, proven

`apps/gui-sonotron/tests/test_song_mode_repeat_count_contract.cpp` (lines 19-36) pins three
claims with real bar-timing readback through the real engine thread: a `repeat=K` scene
genuinely holds for `K * n_bars` real bars (not a parsed-and-ignored field), and an infinite
scene holds forever until an explicit resume gesture. For every **finite, pre-configured**
repeat — set the stepper, then press Play — Phase-1 is functionally indistinguishable at
playback time from a hypothetical core primitive: both produce the same sequence of
`apply_scene_transition` calls landing on the same bars. The "infinite" case is not merely
"good enough" — it is **already optimal**: it costs exactly one `kSceneAdd` step today,
riding `SceneChain::on_bar()`'s own "last step holds forever" semantic. A core primitive
would add machinery to reproduce a behavior Phase-1 already gets for free.

### 3.2 What a core primitive would genuinely add: live mid-playback repeat editing

The one capability host-expand structurally cannot offer is editing a scene's repeat count
**while that scene is the one currently playing**, without rebuilding (and thereby
restarting) the whole chain. Host-expand has already baked `K` identical steps into the
chain by the time `Play` fires; `apply_song_build`'s own comment is explicit that any later
`GridModel::set_scene_repeat` call is inert until "a fresh song-build call replaces the
whole chain" (`in_process_brain_session.cpp:1217-1220`).

**But this is not a live product need today.** Traced the only UI surface that touches
`scene_repeat` — the "- K +" stepper in `render_scene_header_cell`
(`apps/gui-sonotron/src/grid_panel.cpp:942-965`) — and it calls `model.set_scene_repeat(s,
repeat ± 1)` and nothing else: **it never calls `brain_session.send(...)`, never touches the
engine thread, never checks `scenes().playing()`.** It is a pure pre-Play configuration
control, gated implicitly by the fact that nothing propagates until the next `Play`/
scene-header click rebuilds the chain. There is no existing gesture anywhere in the product
that attempts to change a repeat count on a scene that is actively looping and expects an
immediate effect. The gap §3.2 identifies is real in principle, unexercised in practice.

### 3.3 The capability is not free inside the core, either — a real abstraction fork

If the owner does want live mid-playback repeat, building it is not a drop-in field. The
current contract of `TransitionFn` (`scene_chain.hpp:62-67`, "Fired at every scene
TRANSITION") and `Engine::apply_scene_transition`
(`components/core/arrangrr/src/engine.cpp:1034-1049`) is: `fire()` is called exactly once
per step index change, and `apply_scene_transition` reapplies `Performance` (via
`apply_performance`, which drives `Arranger::request_scene` and governs the section's own
phase/length through `scene_hold_bars`) every single time it is called. A repeat-aware
`on_bar()` has to choose, and this is a genuine design fork, not an implementation detail:

- re-fire `TransitionFn` on every repeat cycle (same step, `m_index` unchanged) → risks
  re-triggering `Arranger::request_scene`'s phase/anchor logic on every repeat lap, the
  exact class of bug already on record in this codebase (memory: "intro/outro=1 core
  stale-anchor bug"); or
- suppress firing on repeat cycles and only fire on a genuine index change → then the host
  has no signal that a repeat lap occurred (no UI feedback, no "lap 3 of 5" readback)
  without a NEW `OutEvent`/`LoopEventKind`-style signal, which is itself a small ABI
  addition on top of the field addition.

Either branch is buildable, but it is a decision the owner must make deliberately, not one
Corelli should default for a proposal that has not been asked for yet.

### 3.4 The real argument in favor: the 8×8 capacity coupling (§1.4), not liveness

The strongest case for a core primitive is **not** live-editability — it is memory. Because
`kMaxSongScenes(8) * kMaxSceneRepeat(8) == kMaxScenes(64)` exactly, Phase-1 is running the
core's entire step budget at 100% by design, for the CURRENT caps only. A core `repeat_count`
collapses an 8-times-repeated scene from 8 steps to 1 step + a count — an up-to-8x reduction
in step consumption for the exact same song, and it decouples the GUI's own future growth
(more columns, a higher repeat cap) from the core's step ceiling, removing the silent
cross-file invariant flagged in §1.4. This is a genuine structural win, but it is a
future-proofing argument, not a "Phase-1 is broken today" argument: nothing in the shipped
product currently approaches the 64-step ceiling (8 populated columns × 8 repeat is the
worst case, and it is exactly at the limit, never over it).

### 3.5 Honest verdict

Phase-1 covers every real, exercised use case today: finite pre-configured repeat and
infinite hold both work, are tested, and are behaviorally identical to what a core
primitive would produce. The one capability a core primitive would add — live mid-playback
repeat editing — has no existing UI gesture attempting it and is not blocked by anything
the owner has asked to ship. The one capability worth banking now — decoupling the GUI's
column/repeat caps from the core's 64-step ceiling — is a real but non-urgent structural
improvement, not a functional gap.

---

## 4. Recommendation

**Keep Phase-1. Do not build the core `repeat_count` primitive now.** Close Phase-2 as
"not currently justified" rather than leaving it open as a phased commitment — reopening it
later costs nothing (§2 shows it is cheap and unconstrained by any freeze), and building it
now would be paying an abstraction-design cost (§3.3's fork) against a capability with zero
present demand.

If the owner later wants live mid-playback repeat (a product decision, not an architecture
one — route that through Puccini/the owner, not through this document), the additive spec
is:

- **SHIPPABLE, HOST/CORE joint change, NO new dependency**: append `std::uint8_t
  repeat_count = 1;` to `SceneStep` (13 → 16 B padded); `Engine::scene_add` reads a new
  operand — `cmd.idx` is free today (§1.3) and needs no `Command` growth — clamped to
  `[1, 255]` with an explicit "0 means infinite" or a separate sentinel decision mirroring
  `GridModel::kSceneRepeatInfinite`'s own "one past max" convention.
- **NEEDS-DECISION (owner)**: does a repeat lap re-fire `TransitionFn`/`apply_scene_transition`
  (§3.3, stale-anchor risk) or fire a new, distinct signal? This is the one fork that
  actually shapes the feature and must be settled before implementation, not during it.
- **NEEDS-DECISION (owner, separate from repeat-count)**: should `GridModel` scene-column
  state (names, sections, bars, repeat) start being persisted in `layout_json.cpp` at all
  (§1.5)? Today it is entirely session-only regardless of which layer owns repeat, which
  weakens any "durability" argument for the core primitive specifically.
- **Test burden**: extend `test_scene_abi_verbs_round_trip_via_raw_wire_ids`
  (`components/core/arrangrr/tests/test_scene_hardening.cpp:288-326`) and
  `test_abi_frozen.cpp`'s raw-value pins for the new operand; a new core unit test for
  repeat-cycling inside `on_bar()`; a new functional test mirroring
  `test_song_mode_repeat_count_contract.cpp`'s bar-timing-readback discipline but proving
  the LIVE-EDIT path specifically (change repeat while the scene is playing, observe the
  effect on the CURRENT lap, not the next song-build).

Independent of the repeat-count decision: the silent `kMaxSongScenes * kMaxSceneRepeat ==
kMaxScenes` coupling (§1.4) is worth a defensive `static_assert` or at minimum an explicit
cross-file comment anchor, so a future bump to either GUI-side constant fails loudly at
compile time instead of silently starting to truncate songs. That is a small, Giotto-
sized hardening item, not an architecture decision, and not contingent on Phase-2.

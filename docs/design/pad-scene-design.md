# Pad/Scene live — as-built architecture review (Phase 5, Item #9)

Status: **DESIGN REVIEW (Corelli, read-only), 2026-07-13.** Structural design for
`docs/design/phase5-execution-plan.md` Item E (nodes `7200`/`8100`–`8200`), ahead of
implementation. This document decides the *shape*; it does not implement.

Scope, per the request: (a) pad banks reusing the scheduler as a Pattern playback
source, quantized to the boundary; (b) the persisted Performance/Scene state-model
shape for one-button recall. Both judged against what the tree *actually contains
today*, not against the wish-list.

---

## 1. Sources read

- `docs/DESIGN.md` §2 (architectural principles, `0100`–`0800`), §7 (data taxonomy —
  `Scene`, `Performance`, `Pad` entity definitions, POD/index discipline), §8.1–8.6
  (Korg/Yamaha/Roland Registration/STS/Multi-Pad/Scene analogy), §15 (Phrase Pads /
  Pad Engine), §16 (Chord Sequencer — an existing quantize precedent), §17
  (Performance/Registration/Preset model), roadmap lines 866/872–873 (`7200`, `8100`,
  `8200`), and the D-number cross-reference (`D4=0800`, `D21=8400/8500`,
  `D26=1330,0700`, `D32=0200`, `D33=0400`).
- `docs/design/phase5-execution-plan.md` — Item E scope and the explicit boundary:
  ABI *shape* is unfrozen, dual-target/no-heap doctrine is **not**.
- `docs/design/ux-workstation.md` §4.4/§5 (Repeat Zone / Live-Loops launch grid —
  the GUI's own, already-shipped use of the word "Scene") and §11 (front-of-line
  additive-core batch: item 3 bundles a clip *and* scene launch primitive under
  nodes `6000`/`8000`; item 6 explicitly folds Scene/Song persistence into that same
  clip work).
- Code traced: `components/arrangrr/include/arrangrr/arranger/arranger.hpp`,
  `components/arrangrr/include/arrangrr/abi.hpp`,
  `components/hostrt/param_state_wire.cpp`,
  `components/arrangrr/include/arrangrr/routing/router.hpp`,
  `components/runtime/include/runtime/transport.hpp`,
  `components/arrangrr/include/arrangrr/chord/chord_sequence.hpp`,
  `components/arrangrr/src/engine.cpp` (`Engine::chord_play`),
  `components/arrangrr/include/arrangrr/config.hpp`,
  `apps/gui-sonotron/src/grid_model.hpp`/`.cpp` + `tests/test_grid_model.cpp`,
  `apps/gui-sonotron/src/track_roles.hpp`,
  `components/arrangrr/include/arrangrr/common/crc.hpp`.
- Confirmed absent by grep across `components/` and `apps/` (excluding
  `third_party`): `master_transpose` / `kTranspose` / `GlobalTranspose` (zero
  hits) and any `magic`/version/CRC **file-format** precedent outside the bare
  `crc32()` utility itself.

---

## 2. What exists today (evidence)

### 2.1 Recallable Arranger state — and the transpose gap
`arrangrr::Arranger` (`components/arrangrr/include/arrangrr/arranger/arranger.hpp`)
holds: `m_style` (pointer to a `Style`), `m_current`/`m_return_to`/`m_pending`
(`SectionType`), `m_muted`/`m_solo` (per-role `uint16_t` bitmasks, `set_mute`/
`set_solo`/`muted`/`soloed`, lines 74–92), `m_routes[kRoleCount]` (`Route{port,
channel,enabled}`, lines 373–377, 465), and `m_groove` (`GrooveParams`). That is
four of the five fields a Scene/Performance is asked to snapshot: **style,
variation(section), mute, routing** all have real, live backing state.

**Transpose does not.** A repo-wide grep for `master_transpose`/`kTranspose`/
`GlobalTranspose` returns nothing in `components/` or `apps/`. The only
"transpose" that exists is `ChordSequence::transpose_to`/`transpose_by`
(`components/arrangrr/include/arrangrr/chord/chord_sequence.hpp:107-115`) — a
*compositional* re-derivation of a recorded chord sequence's key, not a live
global output-transpose knob. §17's `Performance.master_transpose` field
describes state that **does not exist in the engine**. This is not a
serialization problem to solve in this item; it is a missing engine feature.

### 2.2 Two "routing" concepts, not one
`Arranger::m_routes` is a per-`TrackRole` `{port, channel, enabled}` table (one
destination per style part). `arrangrr::Router`
(`components/arrangrr/include/arrangrr/routing/router.hpp:45-89`) is a *separate*,
general in→out matrix (`Route{in_port, in_channel, out_port, out_channel, pass}`,
`StaticVector<Route, kMaxRoutes>`) with message-class filters — the MIDI-thru/
merge/filter layer (§9.E). §7/§18's `RoutingProfile`/`Zone` maps loosely onto the
*general* Router, while the arranger-part routing is `Arranger::m_routes`. A
"Scene must snapshot routing" instruction is ambiguous until the design states
**which** of the two it means (§4.2 below flags this as a decision, not
something I resolve unilaterally).

### 2.3 Three independent "fire at the boundary" mechanisms already exist
1. `Arranger::request`/`request_style`/`on_tick` — a `m_pending`/`m_pending_valid`/
   `m_pending_style` state machine that lands a style/section switch at the next
   bar boundary while playing, immediate when stopped
   (`arranger.hpp:141-291`).
2. `ChordSequence::quantize(Tick grid)` — a *different* operation: post-hoc
   snap-to-grid of already-recorded step starts/durations
   (`chord_sequence.hpp:81-103`), unrelated to "stage a future action."
3. `Engine::chord_play` threads a `quantize` bool (`cmd.idx != 0`) into
   `ChordEngine::play`/`play_single`/`play_shell` — a **third**, independently
   coded "stage this chord for the next bar like a SHIFT-note" path
   (`engine.cpp:284-327`; `abi.hpp`'s `kChordPlay` comment: "idx != 0
   SHIFT-quantized (staged to the next bar like a shift note)").

No shared "boundary-quantized action" primitive exists. Pad triggers need
exactly this behaviour (`Sync: Immediate/ToBeat/ToBar/ToPattern`, §15) — a naive
implementation would be a **fourth** bespoke copy of the same state machine.

### 2.4 The ABI readback surface is narrower than what a Performance needs
`components/hostrt/param_state_wire.cpp:14-37` enumerates the **only** 9 domains
the on-connect/on-mutate `kParamState` echo covers: `groove`, `arp`, `part-mute`,
`part-solo`, `style-load`, `chord-detect`, `chord-follow`, `chord-mode`,
`key-set`. Missing from readback: the **active section/variation** (only
`style-load` echoes, not `kStyleSection`/the current `SectionType`), **routing**
(neither `Arranger::m_routes` nor `Router`'s table has a readback echo), and
**transpose** (no backing state, §2.1). A Performance recall that must *apply*
these fields is one problem; a GUI that must *display* the currently-recalled
Performance without maintaining a shadow copy is a second, and today's ABI
cannot answer it for section/routing/transpose.

### 2.5 The GUI already ships a "Scene," and it means something else
`apps/gui-sonotron/src/grid_model.hpp` (comment lines 11–13, 38–42) implements
the **Repeat Zone / Live-Loops launch grid**: rows = `TrackRole` parts, columns =
**scenes**, `GridCell{kind, label}` where `kind` is `kStyleSection`/
`kChordSequence`/`kStepTrack`. `ux-workstation.md` §5 confirms: "launching a
column ('Scene ▶') fans out its cells' launch commands." `kGridLaunchWired =
false` is a pinned "honest placeholder"
(`test_grid_model.cpp:74-78`, `test_launch_wired_is_honest_placeholder`) — the
GUI already has a shipped, tested `Scene` = *a column of launchable clips*. This
is **not** the same entity as `DESIGN.md` §7/§17's `Scene` (a Song-anchored
timed snapshot of variation/mute/routing/transpose) or `Performance` (the
one-button full-state recall). Three different things are one word right now:
GUI Scene (clip-column), `DESIGN.md` `Scene` (node `8100`, time-anchored song
snapshot), and the execution plan's own casual "Pad/Scene" title for what
`DESIGN.md` actually calls `Performance` (node `8200`). §4.3 resolves this.

### 2.6 No persisted-format precedent exists yet
`components/arrangrr/include/arrangrr/common/crc.hpp` provides a bare `crc32()`
function — used today, from the test grep, only in `test_common.cpp`'s
`test_crc`, not wired into any file format. A repo-wide search for `magic`
outside comments/vendor code returns nothing that is our own versioned binary
layout. **Architectural Principle #8** ("One single versioned binary format...
`magic + version + CRC`") and node `8500` ("Versioned binary storage + CRC") are
locked decisions with **zero code** realizing them. A Scene/Performance store is
the first real exercise of this principle — get it right the first time is not
a figure of speech here, it is literally greenfield.

### 2.7 A pre-existing coupling risk that a Performance struct inherits
`apps/gui-sonotron/src/track_roles.hpp:7-17` documents, in its own comment, that
the GUI's 9-row `TrackRole` vocabulary is "a hand-copied literal" of the core's
`TrackRole` enum (which has **10** entries, `kRoleCount = 10` in
`arranger.hpp:366`, including `kCc` which the GUI deliberately drops). Any
per-role array inside a persisted Performance (mute mask, route table) is
indexed by the **core's** 10-role enum; the GUI's 9-role subset is a display
convenience, not the authoritative ordinal space. This is not new drift I am
introducing — it is an already-acknowledged, hand-maintained parallel
enumeration across the ABI boundary (D38: gui-sonotron never includes the
core). The Performance design must be explicit about which ordinal space it
serializes.

---

## 3. Drift from locked decisions

**`0700` (ABI discipline) vs Architectural Principle #8 (versioned storage) — DO
NOT CONFLATE.** `phase5-execution-plan.md` lifts the ABI freeze for Phase 5
("wire-serialization changes MAY move golden bytes... deliberate and
reviewed"). That applies to the **live Command/OutEvent wire contract**
(`abi.hpp`'s `Op`/`Param`/`Command`/`OutEvent`) between a running host and a
running core. It says nothing about, and does **not** relax, the discipline a
*persisted* Performance/Scene file needs: once a user saves one, it is data
that must survive a future software/firmware update whose wire ABI may look
nothing like today's. The two disciplines are orthogonal:
- Wire ABI (`0700`): append-only enum growth, no version field needed because
  both ends are recompiled together.
- Persisted format (Architectural Principle #8, node `8500`): needs its **own**
  `{magic, format_version, CRC}` header and an explicit migration function,
  because the reader (a future build) and the writer (today's build) are NOT
  guaranteed to be the same binary. Append-only-forever is the wrong discipline
  here — a persisted format is *allowed* to reshape across a major
  `format_version` bump with an explicit migrator, precisely because that
  bump is visible and versioned, unlike an in-place Param renumber.
No side "yields" here — both decisions stand; the risk is a future
implementor treating "ABI is unfrozen" as license to skip the versioned-storage
discipline for Scene data specifically, which Architectural Principle #8 never
excused.

**Naming: `Scene` is already spoken for.** `DESIGN.md` §7 defines `Scene` as
"snapshot of performance state at a point in the song... anchored to time," and
explicitly distinguishes it from `Performance` (§17: "the Performance is a
*state preset*; a Scene (inside Song) is a timed snapshot. They share the
fields but Scene is anchored to time"). The GUI (§2.5) already ships a *third*
sense of "Scene" (a launch-grid column of clips) that is live, tested, and in
the wireframe vocabulary (`ux-workstation.md` §4.4/§5). Phase 5's own execution
plan then titles Item E "Pad/Scene" for what is functionally the one-button
**Performance** recall (§8, Registration/STS) — reusing the word for a third
meaning inside one planning document. Code should not inherit the planning
document's shorthand. **Recommendation: code speaks `Performance` for the
one-button full-state recall (matches `DESIGN.md` §17 exactly and needs no new
word), and leaves `Scene` exclusively to the GUI's already-shipped clip-column
sense.** `DESIGN.md` §7's Song-anchored `Scene` (node `8100`) is a real,
separate, later concern (a Song is a chain of Scenes) that this item does not
need to build to deliver Item E's two stated halves — flagged, not built.

---

## 4. Proposed structural direction

### 4.1 PADS (`7200`) — SHIPPABLE, core, dual-target

**Reuse, don't re-invent:** a Phrase/Chord pad's content plays back through the
existing scheduler exactly as the Arranger's step-events do today —
`Arranger::on_tick`'s inner loop already does "resolve → voice → groove →
`schedule()`" (`arranger.hpp:293-361`); a pad is a second, independently-clocked
*content source* feeding the same `ScheduleFn`/`OutScheduler`, not a new
playback engine. This matches `DESIGN.md` §15's own text ("no separate engine")
and costs nothing extra dual-target-wise: the scheduler is already sized and
budgeted (`kSchedulerCapacity = 4096`, `config.hpp:13`, D33).

**New state, sized like every other bounded table in `config.hpp`:**
- `PadBank[4]` per bank (Yamaha/Korg's own "4," §8.1/§8.2), a small
  `kMaxPadBanks` (e.g. 4–8, owner call, not mine) of `PadBank`.
- `Pad{type, mode, sync, source_kind, source_idx(u16), destination(port,ch),
  pitch_policy}` — POD, index-referenced into existing tables (a style
  section index, a chord-sequence index, a track index), never an owning
  pointer, exactly the discipline `DESIGN.md` line 222 already mandates for
  every other entity.
- `PadRuntimeState` (NOT persisted, transient): whether each pad's trigger is
  currently held/looping/pending-boundary. This is per-session, not
  save-data.

**Do not let Pad quantize become the 4th bespoke boundary state machine
(§2.3).** Before Pad's `Sync: ToBeat/ToBar/ToPattern` lands, extract the shared
shape once: a small `BoundaryLatch{Tick target_tick; bool pending;}` helper (or
equivalent) that `Arranger`'s pending-switch, `ChordEngine`'s
quantize-a-chord-play, and the new Pad trigger all instantiate, rather than
each hand-rolling its own copy of "is this tick a boundary, and do I have
something staged." This is a real abstraction gap the pad work should close,
not one more site that widens it. **Feasibility: SHIPPABLE, no new
dependency** — this is an internal refactor-and-extract inside `arrangrr`, pure
POD/functions.

**One-shot/loop/hold/toggle:** these are trigger-*policy* on top of the
boundary-latch + scheduler-feed, not separate playback paths — a small enum
dispatch at the point a pad is triggered/released, mapping to "fire once,"
"keep re-firing until stopped," "sound while held," "flip a bool." All four
reuse the identical scheduling call; only the *when to call it again* differs.

### 4.2 SCENES → `Performance` (`8200`) — the persisted shape, NEEDS-DECISION on two fields

Proposed persisted struct (POD, fixed-size, index-referenced, matching §7's
existing discipline and the `kParamState` domain vocabulary already spoken by
the wire for consistency):

```
struct Performance {
  char name[24];                    // fixed, no std::string (D32/no-heap)
  u16 style_id;                     // builtin index OR compiled-style slot id
  SectionType variation;            // the MISSING readback field, §2.4
  u16 tempo_x100;
  u16 master_transpose;             // RESERVED until the engine has the field, §2.1
  u32 track_enable_mask;            // per-CORE-role (10 roles, §2.7), not GUI's 9
  u32 track_solo_mask;
  Route routes[kRoleCount];         // Arranger-style routing, NOT the general Router — see decision below
  u16 pad_bank_id;
  GrooveParams groove;              // reuses the existing type verbatim
  u16 chord_sequence_id;            // 0xFFFF = none
  u16 controller_map_id;            // 0xFFFF = none (unbuilt today; reserved)
};
```

Two fields are **NEEDS-DECISION, not mine to settle unilaterally**:
1. **`master_transpose`** has no backing engine state (§2.1). Ship the field as
   reserved/zero and wire it in a follow-up once the owner/Ottorino define what
   "global transpose" actually shifts (Arranger output only? live keyboard
   input too? the ChordEngine root?) — that is a musical-scope decision, not a
   structural one, and inventing the semantics here to fill a struct field
   would be exactly the kind of unbacked claim this review's method forbids.
2. **Which "routing" a Performance snapshots** (§2.2): the proposal above
   snapshots `Arranger::m_routes` (per-role, already lives beside mute/solo/
   section) and leaves the general `Router` thru-matrix **out of v1**
   Performance scope, flagged for the owner. Folding both in is possible but
   doubles the surface for a "get it right the first time" item; my
   recommendation is to ship the narrower, already-backed one first.

**Storage: the first real exercise of Architectural Principle #8.**
```
struct PerformanceStoreHeader { u32 magic; u16 format_version; u16 count; };
// followed by count * Performance, followed by u32 crc32 over the whole blob.
```
This is genuinely new work (node `8500` has zero precedent, §2.6) — **not**
something to bolt on casually. File I/O itself is **HOST-ONLY**: Architectural
Principle #2 puts every world-interaction (including storage) behind a HAL the
freestanding core never touches directly; on-device (arm-none-eabi) persistence
is a flash-write HAL that does not exist yet and is out of this item's scope
(flag: firmware storage HAL is a separate, undecided fork, likely tied to the
deferred STM32 bring-up, #3). The **shape** (POD blob, magic/version/CRC) is
target-agnostic and is exactly what a future flash writer would consume
unchanged — that part is SHIPPABLE now in `arrangrr`/tooling; the flash HAL
itself is NEEDS-DECISION/out of scope.

**Recall must be atomic, not a burst of individual Sets.** `Performance` bundles
seven-plus fields that today would each be applied as a *separate* `Set`
Command (`kStyleSwitch`, `kPartMute`×N, `kStyleRoute`×N, `kGroove`...). Applying
them one at a time risks a mid-bar audible glitch (style switches before mutes
land). Recommend a **new** `Op`/`Param` verb — `kPerformanceRecall(idx)` — that
the core applies as one atomic unit at the *next* quantized boundary (reusing
the boundary-latch primitive from §4.1), emitting a single confirmation event
rather than N. This is new ABI surface, which the Phase-5 unfreeze explicitly
permits designing freely; it is additive to nothing (no existing id touched),
so even under the old discipline it would be uncontroversial — under the new
one it is simply the right shape. **Feasibility: SHIPPABLE, no new dependency.**

### 4.3 Relationship to Item #2 Clip — distinct primitives, shared vocabulary discipline

They are **not** the same primitive, and forcing them to be would recreate
exactly the ambiguity §2.5/§3 already describes:

- **Clip / Scene(-column)** (Item D, `ux-workstation.md` §5, nodes `6000`/`8000`
  brought forward): a **content launch** — "what plays now," ephemeral,
  GUI-facing, quantized, cheap to change every few bars. `GridModel`'s Scene
  (a column) already lives here and should stay exactly what it is.
- **Performance** (Item E, `DESIGN.md` §17): a **rig-configuration** preset —
  "how the whole setup is dialed in right now" (style/variation/mute/route/
  transpose), recalled far less often, explicitly NOT a playback object.

Where they **do** share a primitive: two of the six Pad *types* from `DESIGN.md`
§15 (`Phrase`, `Chord` as one-shot/loop content) should be thin wrappers over
the **same** clip-launch verb the Repeat Zone binds to
(`launch/stop clip <id> quantize <n>`, `ux-workstation.md` §11 item 3) —
addressed by a physical pad index instead of a GUI cell click. Reuse the verb,
do not re-derive it.

Where the existing vocabulary is genuinely ambiguous and **must** be
disambiguated before code, not after: `DESIGN.md` §15 lists a single
`SceneTrigger`/`VariationTrigger` pad type. `VariationTrigger` is unambiguous
(an Arranger section request — the existing `kStyleSection` Param). But
`SceneTrigger` could mean "launch a Repeat-Zone Scene-column" (Clip family) OR
"recall a Performance" (Registration family) — precisely the collision named
in §2.5/§3. **Recommendation:** split `DESIGN.md` §15's pad-type enum into
`SceneColumnTrigger` (clip family — fires a Repeat-Zone column) and
`PerformanceTrigger` (registration family — fires `kPerformanceRecall`),
dropping the overloaded `SceneTrigger` name. This is a small, mechanical
doc/enum correction, but it is exactly the kind of naming decision that, left
unresolved, produces two conflicting launch/recall code paths built by two
different implementors under the same word — the failure mode the task asked
me to guard against.

---

## 5. Dual-target summary

| Piece | Target | Notes |
|---|---|---|
| `Pad`/`PadBank` POD + boundary-latch primitive | **SHIPPABLE**, core, arm-none-eabi | fixed arrays, index refs, no heap |
| Scheduler reuse for pad playback | **SHIPPABLE**, core | no new engine, existing `OutScheduler` |
| `Performance` POD (minus transpose semantics) | **SHIPPABLE**, core | fixed arrays, `char[24]` name (no `std::string`) |
| `kPerformanceRecall` ABI verb | **SHIPPABLE**, core | new `Op`/`Param`, additive, no existing id touched |
| `PerformanceStoreHeader` (magic/version/CRC) shape | **SHIPPABLE**, target-agnostic | POD layout only |
| File load/save of the store | **HOST-ONLY** (today) | Architectural Principle #2: storage is a HAL concern; firmware flash HAL is undecided |
| `master_transpose` backing engine state | **NEEDS-DECISION** | musical scope, not structural — flagged to owner/Ottorino |
| Whether Performance snapshots the general `Router` too | **NEEDS-DECISION** | scope call, flagged to owner |

No step above implies a new dependency. No proposal here assumes a heap
allocation, RTTI, exceptions, or `std::string` on the shared-core path.

---

## 6. Flags for the owner (not mine to decide)

1. **`master_transpose` semantics** — what a "global transpose" actually shifts
   (Arranger output, live input, chord root) is undefined today; `Performance`
   can carry the field as reserved, but someone must define its behavior
   before it does anything.
2. **Performance's routing scope** — per-role `Arranger::m_routes` only (my
   recommendation, narrower and already-backed), or also the general `Router`
   thru-matrix (§2.2)? Doubling the scope doubles the "get it right the first
   time" risk.
3. **Firmware-side persisted storage** — out of scope here; tied to the
   deferred STM32 bring-up fork (#3). The blob *shape* proposed is
   target-agnostic; the flash-write HAL is not designed and is not this item's
   job to design.
4. **The `DESIGN.md` §15 pad-type rename** (`SceneTrigger` →
   `SceneColumnTrigger`/`PerformanceTrigger`) is a documentation correction I
   recommend but do not have standing to make unilaterally in a locked design
   doc — flagged for the owner's sign-off alongside the implementation slot.

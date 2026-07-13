# Phase-5 design reviews

Consolidated architecture/scope reviews for the Phase-5 items of
`docs/phase5-plan.md`. Each top-level section is one design
item. Authors: Corelli (as-built architecture/placement) and Ottorino
(musical scope), as noted per section.

Several facts recur across items and are stated once here as shared ground:

- **The D40 resolution pipeline.** `Arranger::on_tick`
  (`components/arrangrr/include/arrangrr/arranger/arranger.hpp`, the per-role,
  per-step fire loop) is gather → `gesture::expand` → `resolve()` (the forward
  NTT kernel, `{tone, octave, NoteSource}` + live `Key`/`ChordState` →
  absolute pitch) → `VoicingState::voice()` (re-octave chord-tone members
  toward the nearest prior voicing) → `groove::apply()` (deterministic
  position-hashed timing/velocity) → `schedule()`. Every stage is a small,
  freestanding, no-heap, deterministic function; `groove::apply()` is fused
  into the loop (note-on and note-off share one locally-computed offset, gate
  preserved) rather than being a step over an independent event stream.
- **Seeded determinism (D16, node `0100`).** The same `constexpr` position-hash
  `h = seed*2654435761u + pos + 0x9E3779B9u; h ^= h>>15; h *= 2246822519u;
  h ^= h>>13;` is hand-copied in three places — `groove::hash`,
  `ArpeggiatorEngine::hash`, `Timeline::hash`. "Same seed ⇒ same output" with
  no PRNG state to carry; the seed IS the state.
- **Dual-target doctrine.** The realtime core (`components/arrangrr` and its
  freestanding dependencies) cross-builds `arm-none-eabi`: no heap, no
  exceptions/RTTI, bounded static storage, `StaticVector`/fixed arrays.
  `arrangrr`'s `CMakeLists.txt` applies `-fno-exceptions -fno-rtti
  -fno-threadsafe-statics` PUBLIC to every dependent TU. Host-only packages
  (`midisrc`, `orchestrator`, tools) may use heap/`std::vector`/`std::string`
  and file I/O; Architectural Principle #2 keeps every world-interaction
  behind a HAL the core never touches.
- **The Phase-5 ABI position.** The execution plan lifts the freeze on the
  live Command/OutEvent *wire shape* only ("wire-serialization changes MAY
  move golden bytes, deliberate and reviewed"). The no-heap/dual-target
  doctrine is NOT relaxed, and neither is the discipline a *persisted* format
  needs.
- **No committed SMF-writer / audio-render path exists.**
  `components/midisrc/include/midisrc/smf.hpp` reads but does not write; no
  `fluidsynth`/audio-render tool is committed (`tests/integration/live_alsa.sh`
  is the closest, excluded from ctest as flaky). Every feel-changing item that
  wants ear-validation outside a live MIDI session hits this same gap — worth
  scoping once (a small host-only SMF writer, or routing through
  `hostrt::Shell`'s existing live MIDI-out path) rather than re-discovering it
  per item.

---

## Clip / launch primitive (#2, Item D)

Corelli, DESIGN (2026-07-13). Coherent with the Pad/Scene and MIDI-FX reviews
below.

### What #2 is

Give the GUI's inert hero zone (Live-Loops/Repeat grid, `docs/gui-and-ux.md`
§4.4–§5) a real launchable-cell primitive: `launch`/`stop`/`scene-quantize`
verbs + a `clip` state event, quantized to the bar/beat boundary. LIGHTER than
the full Looper (node `6000`) — arm a PRE-EXISTING pattern/section to start on
the next boundary, NOT record/overdub.

### Three incoherent "arm-at-boundary" mechanisms today

For what is conceptually ONE primitive, the tree currently has three
inconsistent spellings:

1. `FollowedContext::stage()/commit_bar()` (chord) — real stage/commit at bar
   boundary.
2. `Arranger::m_pending`/`m_pending_style` (`arranger.hpp`) — a second staging,
   spelled differently: `request`/`request_style`/`on_tick` land a
   style/section switch at the next bar while playing, immediate when stopped.
3. `Engine::chord_play` threads a `quantize` bool (`cmd.idx != 0`) into
   `ChordEngine::play`/`play_single`/`play_shell` — a third, independently
   coded "stage this chord for the next bar like a SHIFT-note" path
   (`abi.hpp`'s `kChordPlay` comment: "idx != 0 SHIFT-quantized"). Note
   `ChordSequence::quantize(Tick grid)` is a *different*, unrelated operation
   (post-hoc snap-to-grid of already-recorded step starts/durations).

The third content a clip must encapsulate (a step track) has NO play/stop state
in `Timeline` at all (`Track` has only mute/solo). This is exactly the
incoherence `docs/architecture.md` §0 diagnoses. No shared "boundary-quantized
action" primitive exists; Pad triggers (`Sync: Immediate/ToBeat/ToBar/
ToPattern`, §15) need exactly this behaviour and a naive implementation would
be a FOURTH bespoke copy.

### Decisions

1. **Placement — `ClipMatrix` as an Engine-owned value member** (mirrors
   `ArpeggiatorEngine`), NOT a Pipeline peer (Clip orchestrates subsystems
   Engine already owns — Arranger/ChordSequencer/Timeline — and shares no raw
   cross-stage data). SHIPPABLE core, bounded POD pool
   `StaticVector<Clip, kMaxClips>`; `Clip = {TrackRole part_role, scene_index,
   ContentKind(kStyleSection|kChordSequence|kStepTrack), content_index,
   LaunchState}` — enum-tag + index, never a pointer/variant.
2. **Boundary hook** — inside `Engine::on_tick`'s EXISTING
   `tick % kTicksPerBar == 0` block, BEFORE `fire_arranger` (same reason the
   chord commit precedes the arranger). No new clock; reuse `m_transport.tick()`.
   Quantize "N bars" generalizes the check to `% (N * kTicksPerBar)` per-clip/
   per-request — same Transport, no new infra.
3. **ABI — spend the reshape budget here** (freeze lifted, F3 accepted).
   Recommendation: do NOT add a 4th ad-hoc "quantize-at-boundary" spelling.
   Introduce a real `Boundary` field on `Command` (kImmediate/kNextBar/
   kNextNBars) SHARED by `kChordPlay`, `kStyleSwitch`, and the new clip verbs,
   closing `docs/architecture.md` item #3 and consolidating the three mechanisms.
   (Additive fallback exists — new `Param::kClipLaunch/kClipStop/
   kSceneQuantize` + `OutEvent::Kind::kClip` mirroring chord_followed/beat — but
   it perpetuates the drift.)
4. **Correct the stale `FROZEN v1` banner** in `abi.hpp` (lines ~14–31) — it
   now contradicts the 2026-07-13 unfreeze. #2 is the natural first item to
   fix it.
5. **Scope line vs Looper (`6000`)** — `ClipMatrix` owns ONLY {content ref,
   launch state, pending boundary}. Tripwire: if a `record` field appears on
   `Clip` or a `capture()` method on `ClipMatrix`, that is node-`6000`
   territory. Grep-enforced.
6. **Dual-target** — new budget `static_assert` in `config.hpp` for `kMaxClips`
   (the STM32H743 budget is already tight on `ScheduledEvent`); no heap, no
   variant.
7. **GUI wiring (HOST-only, already staged)** — `render_grid_panel` takes a
   `BrainSession&` (like `render_styles_branch`); the disabled branch becomes
   `brain_session.send("launch clip " + id + " quantize " + n)`; `brain_event`
   decodes a new `kClip` at the already-empty case (`brain_event.cpp` ~340–341);
   `grid_model.hpp`'s `kGridLaunchWired` flips true once the verb + jsonl exist.

### Open decisions for the owner (flagged, not blocking)

- Minimal `Boundary`-reshape (recommended) vs additive-only vs the full
  `docs/architecture.md` `HookCommand` reshape — three cost/benefit points.
- Per-grid vs per-launch quantize (does `Command` need a persistent quantize
  Param?).
- Read `runtime/pipeline.hpp` before final sign-off (confirm a new `m_clips`
  value member doesn't perturb Engine's construction order).

### Coherence with #9 / #10

- #9 Pad/Scene reuses this boundary primitive (`BoundaryLatch` there); the two
  are distinct primitives — Clip = "what plays now" (ephemeral), Performance =
  "how the rig is configured" (rare). Only Phrase/Chord pads call the
  clip-launch verb.
- #10 MIDI-FX's RESERVED `Param` block (`kFx*`) is separate and
  non-conflicting.

---

## Pad/Scene live → Performance (#9, Item E)

Corelli, DESIGN REVIEW (read-only), 2026-07-13. Structural design for nodes
`7200`/`8100`–`8200`. Scope: (a) pad banks reusing the scheduler as a Pattern
playback source, quantized to the boundary; (b) the persisted Performance/Scene
state-model shape for one-button recall — judged against what the tree actually
contains today.

### What exists today (evidence)

**Recallable Arranger state — and the transpose gap.** `arrangrr::Arranger`
holds `m_style` (pointer to a `Style`), `m_current`/`m_return_to`/`m_pending`
(`SectionType`), `m_muted`/`m_solo` (per-role `uint16_t` bitmasks),
`m_routes[kRoleCount]` (`Route{port, channel, enabled}`), and `m_groove`
(`GrooveParams`). That is four of the five fields a Scene/Performance snapshots:
**style, variation(section), mute, routing** all have real live backing state.
**Transpose does not** — a repo-wide grep for `master_transpose`/`kTranspose`/
`GlobalTranspose` returns nothing in `components/` or `apps/`. The only
"transpose" is `ChordSequence::transpose_to`/`transpose_by` — a *compositional*
re-derivation of a recorded chord sequence's key, not a live global
output-transpose. §17's `Performance.master_transpose` describes state that does
not exist in the engine; this is a missing engine feature, not a serialization
problem.

**Two "routing" concepts, not one.** `Arranger::m_routes` is a per-`TrackRole`
`{port, channel, enabled}` table (one destination per style part).
`arrangrr::Router` (`routing/router.hpp`) is a *separate* general in→out matrix
(`Route{in_port, in_channel, out_port, out_channel, pass}`,
`StaticVector<Route, kMaxRoutes>`) with message-class filters — the
MIDI-thru/merge/filter layer. §7/§18's `RoutingProfile`/`Zone` maps onto the
*general* Router; arranger-part routing is `m_routes`. "Scene must snapshot
routing" is ambiguous until the design states which of the two it means.

**The ABI readback surface is narrower than a Performance needs.**
`hostrt/param_state_wire.cpp` enumerates the only 9 domains the on-connect/
on-mutate `kParamState` echo covers: `groove`, `arp`, `part-mute`, `part-solo`,
`style-load`, `chord-detect`, `chord-follow`, `chord-mode`, `key-set`. Missing:
the **active section/variation** (only `style-load` echoes, not the current
`SectionType`), **routing** (neither table has a readback echo), and
**transpose** (no backing state). A GUI that must *display* the currently-
recalled Performance without a shadow copy cannot, today, for section/routing/
transpose.

**The GUI already ships a "Scene," and it means something else.**
`apps/gui-sonotron/src/grid_model.hpp` implements the Repeat Zone / Live-Loops
launch grid: rows = `TrackRole` parts, columns = **scenes**,
`GridCell{kind, label}` where `kind` is `kStyleSection`/`kChordSequence`/
`kStepTrack`. `docs/gui-and-ux.md` §5: "launching a column ('Scene ▶') fans out
its cells' launch commands." `kGridLaunchWired = false` is a pinned, tested
"honest placeholder." Three different things are one word right now: GUI Scene
(clip-column), `DESIGN.md` `Scene` (node `8100`, a Song-anchored timed snapshot
of variation/mute/routing/transpose), and the execution plan's casual
"Pad/Scene" title for what `DESIGN.md` §17 actually calls `Performance` (node
`8200`, the one-button full-state recall).

**No persisted-format precedent exists yet.** `common/crc.hpp` provides a bare
`crc32()`, used today only in `test_common.cpp`, not wired into any file format;
a repo-wide search for `magic` finds no versioned binary layout of our own.
Architectural Principle #8 ("one single versioned binary format... `magic +
version + CRC`") and node `8500` are locked decisions with zero realizing code.
A Scene/Performance store is the first real exercise of this principle —
literally greenfield.

**A pre-existing coupling risk a Performance inherits.**
`apps/gui-sonotron/src/track_roles.hpp` documents that the GUI's 9-row
`TrackRole` vocabulary is "a hand-copied literal" of the core's `TrackRole`
enum (which has **10** entries, `kRoleCount = 10`, including `kCc` the GUI
drops). Any per-role array inside a persisted Performance (mute mask, route
table) is indexed by the core's 10-role enum; the GUI's 9-role subset is a
display convenience. The Performance design must be explicit about which ordinal
space it serializes.

### Drift from locked decisions

**`0700` (ABI discipline) vs Architectural Principle #8 (versioned storage) — DO
NOT CONFLATE.** The Phase-5 unfreeze applies to the live Command/OutEvent wire
contract between a running host and a running core. It says nothing about, and
does not relax, the discipline a *persisted* file needs. The two are orthogonal:

- Wire ABI (`0700`): append-only enum growth, no version field needed because
  both ends are recompiled together.
- Persisted format (Principle #8, node `8500`): needs its own `{magic,
  format_version, CRC}` header and an explicit migration function, because the
  reader (a future build) and the writer (today's build) are NOT guaranteed to
  be the same binary. A persisted format is *allowed* to reshape across a major
  `format_version` bump with an explicit migrator, precisely because that bump
  is visible and versioned.

Both decisions stand; the risk is a future implementor treating "ABI is
unfrozen" as license to skip versioned-storage discipline for Scene data.

**Naming: `Scene` is already spoken for.** `DESIGN.md` §7 defines `Scene` as a
timed snapshot "anchored to time," §17 distinguishes it from `Performance` (a
state preset). The GUI ships a third sense (a launch-grid column of clips) that
is live, tested, and in the wireframe vocabulary. **Recommendation: code speaks
`Performance` for the one-button full-state recall (matches `DESIGN.md` §17
exactly, needs no new word), and leaves `Scene` exclusively to the GUI's
already-shipped clip-column sense.** `DESIGN.md` §7's Song-anchored `Scene`
(node `8100`) is a real, separate, later concern this item need not build.

### Proposed structural direction

**PADS (`7200`) — SHIPPABLE, core, dual-target. Reuse, don't re-invent.** A
Phrase/Chord pad's content plays back through the existing scheduler exactly as
the Arranger's step-events do (`Arranger::on_tick`'s inner "resolve → voice →
groove → `schedule()`" loop); a pad is a second, independently-clocked *content
source* feeding the same `ScheduleFn`/`OutScheduler`, not a new playback engine
(matching `DESIGN.md` §15's "no separate engine"). Costs nothing extra
dual-target-wise: the scheduler is already sized (`kSchedulerCapacity = 4096`).

New state, sized like every other bounded `config.hpp` table:
- `PadBank[4]` per bank (Yamaha/Korg's own "4"), a small `kMaxPadBanks` (4–8,
  owner call) of `PadBank`.
- `Pad{type, mode, sync, source_kind, source_idx(u16), destination(port,ch),
  pitch_policy}` — POD, index-referenced into existing tables, never an owning
  pointer.
- `PadRuntimeState` (transient, NOT persisted): whether each pad's trigger is
  held/looping/pending-boundary.

**Do not let Pad quantize become the 4th bespoke boundary state machine.**
Before Pad's `Sync: ToBeat/ToBar/ToPattern` lands, extract the shared shape
once: a small `BoundaryLatch{Tick target_tick; bool pending;}` helper that
`Arranger`'s pending-switch, `ChordEngine`'s quantize-a-chord-play, and the new
Pad trigger all instantiate, rather than each hand-rolling "is this tick a
boundary, and do I have something staged." An internal refactor-and-extract
inside `arrangrr`, pure POD/functions — SHIPPABLE, no new dependency. This is
the same primitive #2 Clip introduces.

**One-shot/loop/hold/toggle** are trigger-*policy* on top of the boundary-latch
+ scheduler-feed, not separate playback paths — a small enum dispatch at
trigger/release ("fire once," "keep re-firing," "sound while held," "flip a
bool"); all four reuse the identical scheduling call, only *when to call it
again* differs.

**SCENES → `Performance` (`8200`) — the persisted shape, NEEDS-DECISION on two
fields.** Proposed POD struct (fixed-size, index-referenced, matching §7's
discipline and the `kParamState` domain vocabulary):

```
struct Performance {
  char name[24];                    // fixed, no std::string (D32/no-heap)
  u16 style_id;                     // builtin index OR compiled-style slot id
  SectionType variation;            // the MISSING readback field
  u16 tempo_x100;
  u16 master_transpose;             // RESERVED until the engine has the field
  u32 track_enable_mask;            // per-CORE-role (10 roles), not GUI's 9
  u32 track_solo_mask;
  Route routes[kRoleCount];         // Arranger-style routing, NOT the general Router
  u16 pad_bank_id;
  GrooveParams groove;              // reuses the existing type verbatim
  u16 chord_sequence_id;            // 0xFFFF = none
  u16 controller_map_id;            // 0xFFFF = none (unbuilt today; reserved)
};
```

Two fields are NEEDS-DECISION, not settled unilaterally:
1. **`master_transpose`** has no backing engine state. Ship it reserved/zero;
   wire it in a follow-up once "global transpose" semantics are defined
   (Arranger output only? live keyboard input? the ChordEngine root?) — a
   musical-scope decision.
2. **Which "routing" a Performance snapshots.** The proposal snapshots
   `Arranger::m_routes` (per-role, already lives beside mute/solo/section) and
   leaves the general `Router` thru-matrix out of v1, flagged for the owner.
   Folding both in doubles the surface for a "get it right the first time" item.

**Storage: the first real exercise of Principle #8.**
```
struct PerformanceStoreHeader { u32 magic; u16 format_version; u16 count; };
// followed by count * Performance, followed by u32 crc32 over the whole blob.
```
File I/O itself is **HOST-ONLY**: Principle #2 puts storage behind a HAL the
core never touches; on-device (`arm-none-eabi`) persistence is a flash-write HAL
that does not exist yet and is out of scope (tied to the deferred STM32
bring-up). The **shape** (POD blob, magic/version/CRC) is target-agnostic and
SHIPPABLE now in `arrangrr`/tooling; a future flash writer would consume it
unchanged.

**Recall must be atomic, not a burst of individual Sets.** `Performance` bundles
seven-plus fields that today would each apply as a separate `Set` Command
(`kStyleSwitch`, `kPartMute`×N, `kStyleRoute`×N, `kGroove`...); applying them
one at a time risks a mid-bar glitch (style switches before mutes land).
Recommend a new `Op`/`Param` verb — `kPerformanceRecall(idx)` — that the core
applies as one atomic unit at the next quantized boundary (reusing the
boundary-latch), emitting a single confirmation event. Additive to nothing,
SHIPPABLE, no new dependency.

### Relationship to #2 Clip — distinct primitives, shared vocabulary discipline

They are **not** the same primitive:
- **Clip / Scene(-column)** (Item D, nodes `6000`/`8000` brought forward): a
  content launch — "what plays now," ephemeral, GUI-facing, quantized, cheap to
  change every few bars. `GridModel`'s Scene (a column) already lives here.
- **Performance** (Item E, `DESIGN.md` §17): a rig-configuration preset — "how
  the whole setup is dialed in" (style/variation/mute/route/transpose), recalled
  far less often, explicitly NOT a playback object.

Where they share a primitive: two of the six Pad types (`Phrase`, `Chord` as
one-shot/loop content) should be thin wrappers over the same clip-launch verb
the Repeat Zone binds to (`launch/stop clip <id> quantize <n>`), addressed by a
physical pad index instead of a GUI cell click. Reuse the verb, do not
re-derive it.

Where the vocabulary is genuinely ambiguous: `DESIGN.md` §15 lists a single
`SceneTrigger`/`VariationTrigger` pad type. `VariationTrigger` is unambiguous
(an Arranger section request — the existing `kStyleSection` Param). But
`SceneTrigger` could mean "launch a Repeat-Zone Scene-column" (Clip family) OR
"recall a Performance" (Registration family). **Recommendation:** split it into
`SceneColumnTrigger` (fires a Repeat-Zone column) and `PerformanceTrigger`
(fires `kPerformanceRecall`), dropping the overloaded `SceneTrigger` name — a
small mechanical doc/enum correction, flagged for the owner's sign-off in a
locked design doc.

### Dual-target summary

| Piece | Target | Notes |
|---|---|---|
| `Pad`/`PadBank` POD + boundary-latch primitive | **SHIPPABLE**, core | fixed arrays, index refs, no heap |
| Scheduler reuse for pad playback | **SHIPPABLE**, core | no new engine, existing `OutScheduler` |
| `Performance` POD (minus transpose semantics) | **SHIPPABLE**, core | `char[24]` name, no `std::string` |
| `kPerformanceRecall` ABI verb | **SHIPPABLE**, core | additive, no existing id touched |
| `PerformanceStoreHeader` (magic/version/CRC) shape | **SHIPPABLE**, target-agnostic | POD layout only |
| File load/save of the store | **HOST-ONLY** (today) | storage is a HAL concern; firmware flash HAL undecided |
| `master_transpose` backing engine state | **NEEDS-DECISION** | musical scope, flagged to owner/Ottorino |
| Whether Performance snapshots the general `Router` too | **NEEDS-DECISION** | scope call, flagged to owner |

No proposal assumes a heap allocation, RTTI, exceptions, or `std::string` on the
shared-core path.

### Flags for the owner

1. `master_transpose` semantics (what a "global transpose" shifts).
2. Performance's routing scope — per-role `m_routes` only (recommended,
   narrower and already-backed) or also the general `Router` thru-matrix.
3. Firmware-side persisted storage — out of scope, tied to the deferred STM32
   bring-up fork; the blob shape is target-agnostic, the flash-write HAL is not.
4. The `DESIGN.md` §15 pad-type rename (`SceneTrigger` →
   `SceneColumnTrigger`/`PerformanceTrigger`).

---

## MIDI-FX / Transform chain (#10, Item H, nodes `5000`/`5100`/`5200`)

Corelli, structural design review. The chain body (`5100`/`5200`/`5300`) has NO
code yet — confirmed: `abi.hpp` reserves `kMaxInserts = 8` and names
`kFxSet/kFxParam/kFxEnable/kFxClear` (unassigned) with the `(idx=track, a=slot,
b=type/param, c=value)` packing pre-documented, but no `insert`/`chain`/`fx`
symbols exist beyond that reserved block. Everything below about the chain's
shape is a structural proposal against present evidence; the critiques of
groove, arp, `RestyleStage`, and the Pipeline idiom ARE built-code observations.

### How the kernel is built — where the chain would graft in

**The D40 kernel is gather → resolve → voice → groove → schedule, not a
stream.** `Arranger::on_tick` does not produce an event stream a second pass
could operate on: it produces a `NoteReq group[kMaxVoiceNotes]` per role/step,
voices it, then **in the same `for` loop** calls `groove::apply()` and calls
`schedule()` twice (note-on, note-off) with the offset shared between both
calls. No resolved "event" lives as an independent value with its own identity
(to correlate on↔off) before reaching the scheduler. The one place in the
codebase where that correlation already happens is `RestyleStage::note_on`/
`note_off`, via a fixed `Pending m_pending[128]` array keyed by input pitch —
overhead that is NEW relative to fused `groove::apply()` (which carries NO state
between on and off, a shared local variable in one scope), because Restyle's
on and off arrive in separate calls potentially many ticks apart. An Insert that
"processes an event stream" requires exactly this event-as-value identity that
the kernel does not currently provide.

**Groove is already duplicated, not unique — two independent call sites.**
`groove::apply` has two structurally different callers: `Arranger::on_tick`
(with `step`/`tick` from the style section's grid) and `RestyleStage::note_on`
(with a `step` from `snap_to_grid(m_now)` on a live arrival tick). They do not
converge — two independent re-reads of the same pure function applied to two
different note producers, each with its own local notion of "step." A single
shared insert chain cannot absorb this without first deciding whether groove
stays a parameter EACH producer applies to its own output (the status quo,
duplicated but coherent) or becomes a single downstream stage ALL producers
traverse (which would require both Arranger and RestyleStage to stop calling
`groove::apply()` themselves and emit toward a shared chain — a flow change, not
a mechanical refactor).

**Arp is not a stream transform; it is a stateful generator on its own clock.**
`ArpeggiatorEngine` does not process an in→out note flow within one tick: it
accumulates held notes on `note_on`/`note_off` and re-emits at its own
independent rate, driven by `fire_arp` **outside** the Arranger's per-role loop,
on a single global `m_arp` with one in/out port. The "Insert processes an event
and produces zero-or-more events in the same instant" form does not describe the
arp: the arp transforms *time* itself (a few held notes → many notes over time).
Its class comment (9–14) correctly anticipates reuse as a "track MIDI-FX," but
the interface shape needed to host it honestly in a generic chain is NOT the
same one a scale-lock or velocity-proc insert needs.

### Drift from decisions

**`5100`/`5200` (canonical plan) vs the D40 reality.** The plan says
"Arp/groove/scale-lock become INSTANCES of the chain, not disconnected modules"
(`DESIGN.md:831-832`). The code shows groove and arp are disconnected in
*structurally different* ways (groove: fused by value inside another loop, no
event identity; arp: autonomous generator on its own clock, outside the loop).
Not a betrayal of a made decision — `5100`/`5200` are still `○ planned` — but
the plan describes a single symmetric refactoring where the graph shows TWO
different structural problems that no single uniform `Insert` interface solves
honestly. **The plan yields, not the code:** node `5200` must be split
explicitly into two paths, not treated as one refactor.

**`0600` (open/hackable) vs the absence of a single extension point.** `0600`
asks for "a uniformly addressable parametric surface." Two unreconciled "per-X"
addressing schemes already exist — `Route m_routes[kRoleCount]` (10 fixed roles)
and `kMaxTracks = 16` (Timeline) — and the `5100` text itself says "per-track"
without specifying which. Not yet a coded drift, but a pre-existing vocabulary
ambiguity an honest design must resolve explicitly before writing
`StaticVector<Insert, kMaxInserts>` anywhere, or it inherits the same
two-parallel-enumerations confusion already documented for `TrackRole` (10 core
vs 9 GUI, hand-copied).

### Structural proposals

**Where the chain lives and its addressing unit — NEEDS-DECISION.**
Recommendation: "per-track" in `5100` should mean "per-ROLE-slot"
(`kRoleCount = 10`, the same ordinal space as `Arranger::m_routes`), NOT
"per-Timeline-Track" (`kMaxTracks = 16`). The first three insert targets (echo,
note-repeat, scale-lock) and the two refactors (`5210` groove, `5220` arp) all
already live indexed by role (`m_routes[kRoleCount]`, `kRoleAnchor[kRoleCount]`,
the single `GrooveParams` applied "to every part"). Timeline is a hand-written
step-sequencer primitive (`1400`), not the container the Arranger or arp
traverse; treating "track" as "role" avoids inventing a third ordinal space.
Owner call, because if `5100`'s original intent was genuinely the Timeline Track
(applying a chain to hand-written sequences too), the total capacity and the
graft point in the code change entirely.

Proposed form (SHIPPABLE, no new dependency):
```cpp
// one array, indexed by the SAME ordinal space as Arranger::m_routes
StaticVector<Insert, kMaxInserts> m_chain[kRoleCount];
```
`Insert` is **not** a virtual base class (D32: "virtual only at HAL boundaries"
— a per-note dispatch in the hot path is not a HAL boundary, exactly as
`stage.hpp` already argues for `StageLike`). The form coherent with the existing
idiom (the compile-time `enum`→handler `ArpField::set_field`/
`GrooveField::set_field` already use) is a **closed tag + union**, not a
`std::variant`:
```cpp
enum class InsertType : std::uint8_t { kEcho, kNoteRepeat, kScaleLock, kVelocityProc, /* ... */ };
struct Insert {
  InsertType type = InsertType::kEcho;
  bool enabled = true;
  union { EchoParams echo; NoteRepeatParams note_repeat; ScaleLockParams scale_lock; /* ... */ } params;
  // process(): switch (type) { case kEcho: return echo_process(params.echo, ev); ... }
};
```
Compile-time enum-dispatch via `switch`/`if constexpr`, zero vtable, zero heap,
fixed size = `max(sizeof of each ParamsT)` — D32-coherent.

**Groove as insert — SHIPPABLE but NOT a free bit-identical refactor.** Byte
identity for the existing goldens is reachable IF AND ONLY IF the refactor does
not move groove out of the point where the Arranger applies it today. If `5210`
means "the Arranger stops calling `groove::apply()` itself and instead invokes
`m_chain[role]` with a `kGrooveLegacy` insert that ENCAPSULATES the same call,
in the same place, with the same `step`/`tick`/`role`" — goldens stay
bit-identical by construction. If `5210` means "groove becomes a true
post-scheduling insert re-reading an event stream independent of the loop's
local `role`/`step`" — it needs `RestyleStage`'s `Pending`-per-pitch, the on↔off
correlation changes from "same local variable" to "lookup in a pitch-indexed
array," and this **can** change emission order if two notes of the same pitch
overlap in one role (voice-leading may reuse a pitch) — a real edge case the
`feel_swing`/`shuffle`/`blues` goldens may not cover. Honest flag: bit-identity
is reachable only if the chain grafts at the same kernel point where
`groove::apply()` lives today (after `voice()`, before `schedule()`), not as a
truly independent step downstream of the scheduler.

**Arp as insert — NEEDS-DECISION, a uniform interface alone is not honest.**
`5220`/`7130` cannot share the §Insert interface without weakening it. Two
honest paths, neither a mechanical refactor:
1. **Arp stays an Engine-level component but becomes a per-role ARRAY** instead
   of a single global `m_arp`, still driven by `fire_arp` outside the Arranger
   loop, with its own `EmitFn` writing to the SAME `OutScheduler` — "chain
   instance" in the sense of "one `ArpeggiatorEngine` per slot," not "traverses
   the same `Insert::process` interface as the other seven types." SHIPPABLE,
   small per-instance cost (`ArpeggiatorParams` ~9 B + `m_notes`/`m_vels
   [kMaxArpNotes=8]` ~16 B + counters).
2. **`Insert` grows a second optional capability** (`on_tick`, probed via
   `if constexpr (requires {...})` — the same SFINAE idiom `pipeline.hpp`'s
   `fire_on_tick`/`fire_push_midi_in` already use) for inserts that, like the
   arp, need their own clock beyond `process(event)`. More honest
   architecturally (one type-erased `Insert`, heterogeneous capabilities
   declared at compile time), but more ceremony for a chain with only 2–3
   candidate inserts today.

Not the reviewer's choice — it changes the `Insert` signature for all eight
slots, not just the arp; an owner interface-shape decision before `5100`/`5220`.

**ABI — SHIPPABLE, the surface is already well drawn.** The RESERVED block
(`abi.hpp:159-189`) is good work: `kFxSet/kFxParam/kFxEnable/kFxClear` with
`(idx=track, a=slot, b=type|flags, c=value)` mirrors the form `kGroove`/`kArp`
already use (`a=Field, b=value`) — no new Command pattern to invent. One caveat
to coordinate with the Pad/Scene review: `kFx...` is an orthogonal verb
namespace, never overlapping the clip-launch verbs, so there is no conflict
today — but if a future insert ever wanted the bar-quantize semantics
`kChordPlay`'s `idx` already uses, that reuse must be explicit, not re-derived a
fourth time.

**Dual-target / budget — SHIPPABLE, cost quantified.** With per-role slots:
`kRoleCount=10 × kMaxInserts=8 = 80` slots. If `Insert` is a tag+union sized on
the largest candidate params (`EchoParams`/`ScaleLockParams`, ~8–16 B) plus ~2 B
tag/enable, the whole chain config is ~`80 × 16-20 B ≈ 1.3-1.6 KB` —
negligible against the `≤512 KB` budget (D33). The real hidden cost is the
runtime STATE for stateful inserts: if `5210` (groove) needs a `Pending`-per-
pitch like `RestyleStage` (128 B/instance) × each hosting slot, and `5220` (arp)
needs a full `ArpeggiatorEngine` per slot (~30–40 B), runtime cost rises to
several hundred bytes per active role — still well inside budget, but that is
the figure that must appear in the pool `static_assert` when `5100` is
implemented, not just the `Params` POD size.

**Relationship to `runtime::Pipeline<StageT...>` — genuinely different, idiom
partly reusable.** `Pipeline<StageTs...>` composes INDEPENDENT
whole-engine-level components (MidiSource, chorddet, arrangrr), with per-stage
construction from heterogeneous factories and an optional-CAPABILITY fan-out
(`push_midi_in`, 3-arg `on_tick`) probed via `if constexpr (requires {...})`.
The MIDI-FX chain composes HOMOGENEOUS transforms over the SAME event type,
inside the tick of ONE stage (the arrangrr stage) — a level below, exactly as
motif is "a producer one level above" in the Motif placement verdict. Not the
same stage-shape: forcing `Pipeline` here would inherit the same trap that
document diagnosed for Motif — ceremony without a boundary gained. What IS
reusable is the IDIOM, not the type: the capability-probing SFINAE for giving an
insert like the arp an optional `on_tick` hook (option 2 above) is worth
copying; the class that hosts it is not.

### Flagged for the owner

1. "Per-track" in `5100` = per-ROLE (`kRoleCount=10`) or per-Timeline-Track
   (`kMaxTracks=16`)? Recommend role; owner call (changes total capacity and
   graft point).
2. Groove as insert: fused at the same D40 point (bit-identity guaranteed) or a
   true independent post-scheduler step (bit-identity NOT guaranteed on
   pitch-overlap edge cases)?
3. Uniform `Insert` shape: pure stream-transform, or capability-probed with an
   optional `on_tick` hook to host the arp honestly? Decides the signature for
   all eight slots.
4. No proposal requires a new dependency — all `StaticVector`/tag-union/
   enum-dispatch, already in the codebase vocabulary.

---

## Restyle (#1, Item A, node `9320`)

Musical scope by Ottorino (dual axis, style-and-arrangement); placement/shape by
Corelli. HOST-ONLY per the plan; the no-heap core doctrine is unaffected
(nothing here runs on the realtime device path). The first slice is
**already implemented** as `arrangrr::RestyleStage`
(`components/arrangrr/include/arrangrr/restyle/restyle_stage.hpp`).

**Restyle ≠ Accompany.** Accompany (`9310`, shipped) keeps the input's own
melody verbatim and lays the style's OWN band underneath the chords that melody
implies — the input part is never touched by NTT/gesture/voicing/groove; it is a
raw, tick-exact, register-exact MIDI replay ("melody-thru") running in parallel
with the generated band, while chorddet watches the same forwarded bytes and
publishes a `ChordState` via `FollowedContext`. Restyle must instead
**transform the input's own part** — its rhythm, register, voicing — into the
target style's idiom, while preserving *which notes/harmony it plays*. That is a
different operation on a different piece of data; today's Accompany machinery
(chorddet detection + the D40 stages) is exactly what Restyle reuses for
harmonic context rather than re-detecting anything.

### The corpus is more differentiated than the stale premise assumes

Re-measured across the current 16 files
(`arranger/styles/*.hpp`, ~3700+ note events):
- `RolePolicy` is still binary — `kFixed` (drums, 14–23 events/style) +
  `kChordTone` (everything pitched, 32–35 events/style).
- `NoteSource::kScaleDegree` = 35 events, `kInterval` = 14 events across the
  whole corpus — an uneven sprinkle concentrated in country/disco/house/latin/
  motown/pop/reggae/rock/shuffle/swing/blues; ballad, basic, bossa, funk, samba
  have zero melodic-source events.
- `ChordGesture` (strum/roll) = 22 total, concentrated in reggae (skank chop)
  and samba (cavaquinho stabs).
- `VoicingPolicy::kLead` (D41) is comping-only (chord1/pad/chord2, never bass).
- **But the drum/perc/bass layer IS genuinely idiomatic:** `reggae.hpp` is a
  real one-drop (kick+snare together on step 8 = beat 3) with an off-beat skank
  chop (always an upstroke); `samba.hpp` is a real surdo/tamborim/agogo pattern
  (syncopated kick on steps 4/12, continuous 16th tamborim, agogo clave);
  `blues.hpp` has a genuine blue-note harp line via `kInterval` (b3/b5/b7) and a
  boogie root-fifth-seventh bass (`kBoogieBass`). `9100` (per-style default
  `GrooveParams` + tempo) is done; swing/shuffle/blues carry a real
  engine-driven swung feel (`blues.hpp`: `.groove={.swing=75,.accent=10,
  .swing_grid=8}`).

**Net correction:** "no genre-defining rhythmic signature, no characteristic
basslines" is no longer accurate for drums/perc and the FEEL layer (both now
real and per-style); it IS still accurate for bass FUNCTION (root/fifth/seventh
only, zero walking/chromatic-approach bass) and for melodic variation (no
generator). The engine already renders a genre's rhythm section idiomatically;
the gap is specifically transforming an ARBITRARY INPUT part into that idiom — a
different problem from authoring one more built-in style well.

### The one genuinely missing piece — a reverse-NTT classifier

The D40 stages are the reusable toolkit: `resolve()` (forward NTT),
`VoicingState::voice()` (voice-leading re-octave, per-role memory reset on style
load), `groove::apply()`, `gesture::expand()` (fans out only `kChordTone`
events, never `kFixed`/melodic — a comping-only tool, not a melody tool). One
caveat on `groove::apply()`'s `quantize`: it only scales the offset THAT
FUNCTION just computed (swing+humanize) back toward zero; it does NOT snap an
arbitrary externally-timed tick onto the 16th grid. That "snap an arbitrary tick
to the nearest grid slot" function did not exist and was added — small,
integer round-to-`kTicksPerStep` (240 ticks), no-heap/dual-target-safe (as
built, `restyle_stage.hpp` `snap_to_grid`).

What did NOT exist and is Restyle's one genuinely new piece: a function taking
one INPUT note (absolute pitch) + the live `ChordState`/`Key` and classifying it
back into an NTT tone-index — the inverse of `resolve()`. `theory.hpp` has the
primitives (`degree_of(key, pc)` → diatonic degree or −1 chromatic;
`shape_of(quality)` → chord-tone offsets) but the classifier itself is new code,
now `restyle::classify()` (`restyle_stage.hpp:99-116`).

### Placement — a new Pipeline stage, verdict: NEW STAGE

Restyle is a new declared Pipeline stage (`arrangrr::RestyleStage`), inserted
between `ChorddetStage` and `Engine`, physically inside `components/arrangrr`
(not a new sibling package). Weighed against the actual contract:

- **D53 (same-tick harmonic visibility) satisfied by either option** — not a
  discriminator. `Pipeline::on_tick` fires every declared stage in fixed order
  every tick; a `RestyleStage` after `ChorddetStage` sees the same-tick-fresh
  `FollowedContext` exactly as `Engine` already does.
- **D29 (total order) satisfied by either option, zero scheduler change.**
  `OutScheduler::classify()` keys the tie-break on MIDI message type only, never
  producer identity; a `RestyleStage` with its own injected `OutScheduler&`
  drops into the same total order with no new `EventClass`, no ABI change.
- **The forward-flow seam already fans out to every non-terminal stage.**
  `PipelineChain::fire_forward` SFINAE-probes every level's `push_midi_in` and
  recurses — a `RestyleStage` with a `ChorddetStage`-shaped `push_midi_in` hook
  receives `MidiSourceStage`'s forwarded melody bytes for free, zero Pipeline
  code touched. This is the single strongest argument for the new-stage shape:
  the seam Restyle needs already exists and reaches exactly one slot short of
  the terminal — put Restyle in that slot.
- **A policy inside `Engine` would break the terminal's documented boundary.**
  Forward-flow stops one level short of the terminal on purpose: "the terminal
  already sees this same note through the shared `OutScheduler`... routing it a
  second time through arrangrr's OWN `push_midi_in` would apply routing/arp-
  capture/harmony-suppress semantics a melody replay must never trigger."
  Restyle-as-Engine-policy needs exactly what that boundary prevents (the melody
  stream treated as *capturable input*), forcing either a third fork inside the
  `Engine::push_midi_in` god-method or a parallel
  `Engine::observe_restyle_input`, both growing the terminal's surface — the
  opposite of the pipeline-extraction trajectory (chorddet was promoted OUT of
  `Engine` for exactly this reason). The plan's framing ("transform policy
  inside arrangrr") is right about the *package* but conflates it with the
  *class* `Engine`.
- **`ArpeggiatorEngine` is the load-bearing precedent, and it argues FOR "new
  class, arrangrr package."** It already proves "capture a note stream,
  re-render it rhythmically, bounded, no heap" is a self-contained freestanding
  class that `Engine` merely owns and drives. A `RestyleStage` structured the
  way `ChorddetStage` wraps `ChordDetector` is the Pipeline-level version of
  that same idiom, one composition level higher.

**Why arrangrr the package, not a new sibling like chorddet:** Restyle needs
`VoicingState`, `groove::apply`, `gesture::expand`, `Style`/`StylePattern`,
register-anchor tables — all arrangrr-owned. D43 forbids `chorddet` from
depending on `arrangrr`; it says nothing forbidding a new class inside
`arrangrr`. A fifth sibling package (`components/restyle`) that then linked
`arrangrr` PUBLIC to reach `VoicingState`/`groove` would recreate the
`X → arrangrr` edge `orchestrator` already has, for no boundary gained.

**Composition point:** grow `orchestrator::AccompanyPipeline` into a 4-stage
`Pipeline<MidiSourceStage<N>, ChorddetStage<kMaxPorts>, RestyleStage<...>,
Engine>` (one alias), constructed **inert by default** — the "byte-identical
when unused" convention `MidiSourceStage` established and `shell.hpp` documents
as the reason one Shell/one CLI binary serves every pipeline shape. Additive to
the type, not a fork; no second `RestylePipeline` alias, no second Shell wiring.

### State/context reuse map

| Restyle needs | Existing seam to reuse | New code |
|---|---|---|
| The input melody's own notes | `MidiSourceStage`'s existing forward-flow (`on_tick(ctx, sink, forward)`) — already fans raw wire bytes to every non-terminal stage | none — `RestyleStage` gets a `ChorddetStage`-shaped `push_midi_in` hook |
| Somewhere to schedule transformed notes | Inject `OutScheduler<N>&` by reference (precedent used 3×: Transport/OutScheduler/FollowedContext) | none — same constructor-injection idiom |
| The current harmony (deferred per scope-gate, seam exists now) | Inject `FollowedContext&` — same object `ChorddetStage` writes and `ChordEngine` reads, same-tick visible (D53) | none for slice 1; the reference is free to add later without a shape change |
| Voicing/register/groove machinery | `VoicingState`, `groove::apply`, `gesture::expand` — public, freestanding, independent of `Engine`/`Arranger` | `RestyleStage` gets its OWN `VoicingState` (a distinct voice-leading lineage — different musical stream, must not share memory) |
| Target style's rhythm grid / idiom data | `Style`/`StylePattern` compiled tables — same flash-mappable data the `Arranger` resolves against | none — reused as read-only data |

**One real gap, flagged:** `MidiSourceStage` unconditionally schedules its own
raw "thru" notes into the shared scheduler — untouched by adding a downstream
`RestyleStage`, so today's design produces TWO note streams per source event
(raw thru on the source's own port + the restyled rendition on `RestyleStage`'s
output port). If both reach the same audible sink, notes double. Two honest
fixes: (a) host-side discipline — never route/mute the raw thru port while
Restyle is active (zero code change, relies on host wiring); or (b) an additive
`set_thru_enabled(bool)` gate on `MidiSourceStage` guarding the `schedule()`
call — mechanical, HOST-only, transform-agnostic (`midisrc` stays name-blind to
Restyle, D43-clean). (b) is the cleaner fix and belongs to the implementor.

### Dimensions for a FIRST SLICE (rhythm + register/voicing) — the concrete transform

For each classified input note (pitch, tick, gate, velocity) arriving from
`MidiSourceStage`'s replay, before it reaches the raw scheduler:

1. **Classify against the live `ChordState`/`Key`** (reused as-is): `pc =
   pitch % 12`, `rel = (pc − chord.root_pc + 12) % 12`; if `rel` matches an
   offset in `shape_of(chord.quality)` the note is a CHORD TONE at that index;
   otherwise fall back to `degree_of(key, pc)` — a diatonic degree if in-key,
   else chromatic/passing, which the first slice PASSES THROUGH at its original
   pitch (no reharmonization) but still rhythmically requantized. The one
   genuinely new code (host-only, small, existing `theory::` constexpr only).
2. **Rhythmic re-quantization — SAFE, highest-leverage.** Round the note's
   absolute tick to the nearest `kTicksPerStep` (240-tick, 16th-grid) slot, THEN
   run that slot through the target style's own `groove::apply()` with the
   style's `GrooveParams` (the `9100`-seeded default) — so an input note lands
   exactly where a style-authored event at that slot would, including the
   target's swing/accent/humanize. Gate is preserved through the same snap. No
   new state, no heap, both targets clean by construction (arithmetic over
   `Tick`/`TickOffset`). This is *the* thing that makes an input "feel IN the
   groove" rather than laid over it.
3. **Register/voicing transfer, chord tones only — SAFE, reusing D41
   wholesale.** For notes classified as chord tones, run them through the target
   style's `VoicingState` for whichever comping role they're assigned, under
   that role's authored `VoicingPolicy`. `nearest_octave` moves the note by
   whole octaves toward the anchor register the target's comping lives in
   (`kRoleAnchor[]`) and toward continuity with the target's voicing memory — so
   an input melody that jumped two octaves for effect settles into the style's
   characteristic register. Non-chord-tone notes are NOT re-voiced (mirrors
   `VoicingState::voice()`'s own guard).
4. **Role assignment:** the input becomes ONE part on an existing role.
   `TrackRole::kLead` (index 8) inherits a plausible register/idiom immediately
   (blues harp `kLeadLick`, reggae `kMelodica`); `TrackRole::kPhrase` (index 7)
   is a genuinely free slot used by zero built-in styles, a clean slate that
   needs its own anchor/voicing defaults authored. The ABI/data-shape call is
   Corelli's; the musical tradeoff is stated.
5. **What a musician recognizes as "played AS the style":** the SAME melodic
   shape (same chord tones in the same order, same passing tones), but (i)
   locked to the target's grid and swung/accented like its native parts — a
   straight-eighths input played "as bossa" acquires bossa's `9110`-seeded feel,
   not just a bossa band underneath it; (ii) settled into the target's
   characteristic register/voicing rather than the input's arbitrary octaves. A
   genuinely different experience from Accompany (which changes nothing about
   the input).

Both dimensions reuse EXISTING, already-tested, already-golden-covered stages
with no change to their internals — the risk surface is the new classifier, not
the reused machinery.

### DEFER: melodic reharmonization, and why

Changing a melody note's actual scale-degree/chord-tone content to fit a NEW
harmonic language must be deferred, for concrete corpus reasons:

1. **No existing classifier to build on** (the reverse-NTT was itself new); a
   reharmonizer would compound two unproven pieces.
2. **The corpus has almost no melodic-generation precedent to imitate** — 35
   `kScaleDegree` + 14 `kInterval` events total; where a melodic line exists it
   is one FIXED four-note authored phrase, not a generative rule. Reharmonizing
   an arbitrary melody "in the style of reggae" has no in-corpus reference to
   validate against — the same generative-motif problem the plan scoped
   SEPARATELY as `9210` (Item B). Solving it inside Restyle would silently
   duplicate `9210`'s scope.
3. **"Close but off" risk is real and specific.** A rhythmic snap a few ticks
   wrong reads as "slightly stiff" — still the same tune. A voicing re-octave
   off reads as "a different inversion" — still the same chord. A REHARMONIZED
   note that picks the wrong chord tone reads as **a wrong note in a piece the
   listener already knows** — the single worst failure class the entire NTT
   design (D24) exists to make structurally impossible for the arranger's own
   parts.
4. **The no-ML boundary rules out the strong version.** `docs/roadmap.md`:
   "ML style-transfer — stays OUT (`9300` scope): on-device infeasible,
   host-only would need a dependency flag." A rule-based reharmonizer is not
   banned the same way, but a *convincing* one is a hard, ear-validated
   rule-authoring problem deserving its own scoping pass.

**Recommendation:** ship (rhythm)+(register/voicing) as the full first slice;
treat reharmonization as OUT for `9320`'s first slice, resumable later as a
`9320` follow-on (once a validated reverse-NTT classifier exists) or folded into
`9210`.

### ABI surface — NONE required for the scope-gated first slice

- **Transformed notes need no new `OutEvent`.** `RestyleStage`'s `flush()` is a
  no-op (like `ChorddetStage`/`MidiSourceStage`); the scheduler is
  producer-agnostic (`ScheduledEvent` carries no producer tag) and `Pipeline`
  delegates `flush()` solely to the terminal. `Engine::flush()` drains the
  shared scheduler and emits the existing `OutEvent::midi(...)` for every due
  event regardless of producer — restyled notes surface through the existing
  wire event, as `Arranger`-generated notes already do.
- **Target-style/quantize-grid selection can be a construction-time/L1-verb
  parameter, not a live wire `Command`.** Given the `[HOST-only]` tag and the
  `midi-source load <path>` precedent, a `restyle <style>` verb selecting the
  target at load time needs no ABI — a host-tool argument like `midi-source
  load`'s path.

**Deferred, NEEDS-DECISION:** if a later slice wants a GUI-live-toggleable
restyle target/groove, that DOES want a wire `Command` — the natural shape is
additive, an `ArpField`-style selector (`Command::kRestyle` + a `RestyleField`),
reusing the existing `Param`-selector idiom rather than reshaping
`Command`/`OutEvent`. Even with the freeze lifted, there is no structural reason
to spend the reshape budget here — better spent on #2 Clip and #9 Pad/Scene
persisted state.

### Dual-target — what must not break arm-none-eabi

- `RestyleStage`'s own header is written freestanding-clean (bounded static
  storage, no heap/exceptions/RTTI); enforced structurally by living under
  `components/arrangrr/include` (PUBLIC `-fno-exceptions -fno-rtti` flags),
  though writing it clean is still implementor discipline.
- The concrete instantiation stays host-only — `RestyleStage`'s only real
  producer today is `MidiSourceStage` (unconditionally HOST-ONLY: heap,
  `std::vector`, file I/O), so the 4-stage `AccompanyPipeline`-with-Restyle is
  transitively host-only when wired, same as today's 3-stage Accompany. That is
  a property of `components/orchestrator` (already HOST-ONLY), NOT of
  `RestyleStage`. Keep the class's header portable anyway so a future non-SMF
  live-input restyle mode never needs a rewrite.
- **Do not let `RestyleStage` grow into `components/orchestrator`.** If any
  transform needs `std::vector`/heap state (e.g. a lookahead window), that state
  belongs at the `orchestrator`/host level, never inside the arrangrr-package
  class — keep the heap-shaped and no-heap-shaped halves on opposite sides of
  the existing boundary.
- `tests/arm-smoke/link_gate.cpp` stays untouched and stays the real gate: it
  drives the 2-stage `Pipeline<ChorddetStage<N>, Engine>`; `RestyleStage` never
  needs to appear there since its only producer is already excluded.

### Testing musical acceptability beyond golden byte-equality

Byte-identical goldens prove REPRODUCIBILITY (same seed ⇒ same output, D16); they
do not prove the transform sounds right the first time. Beyond ctest-green:

1. **Harmony preservation, by ear and by assertion** — every transformed note
   must still land on the SAME functional tone (root stays root, third stays
   third) even though tick/register moved; assertable in a unit test (classify
   the OUTPUT note against the same `ChordState`, confirm the tone-index is
   unchanged).
2. **Groove authenticity against a REFERENCE, not just an internal golden** —
   "requantized to bossa" must actually feel like `9100`'s bossa
   `GrooveParams`/tempo, checkable against the swing-ratio table in
   `docs/style-corpus-and-generation.md` (e.g. swing=100/swing_grid=8 = an exact 2:1
   triplet).
3. **No "stiff" artifacts from the snap** — a requantization can produce
   clusters (two input notes collapsing onto one slot) or unnaturally short/long
   gates; a listening pass on real, varied melodies (not just synthetic golden
   inputs) before the golden is frozen, precisely because a golden only proves
   stability of whatever was first produced.
4. **Register-transfer naturalness across an OCTAVE JUMP** — `nearest_octave` is
   a mechanical "closest register" rule; a musician checks whether a line that
   leaps by a tenth still reads as one coherent phrase after re-voicing, a mode
   the existing `test_voicing_*` suite checks for authored sequences but never
   for an imported melody's larger leaps.
5. **A real listening path** — no committed SMF/audio export exists (see the
   shared SMF-writer gap above); a musician verifying by ear needs either
   `hostrt::Shell`'s live MIDI-out path or a small new host-only SMF writer.

### Open forks / flags

1. Role placement — `kLead` vs `kPhrase` (ABI/data-shape is Corelli's, musical
   tradeoff stated).
2. Non-chord-tone (passing/chromatic) handling — the first slice keeps them at
   original pitch (only requantized), a conservative choice that avoids
   reharmonization risk but leaves a passing tone in an arbitrary register.
   Whether that residual arbitrariness is acceptable, or passing tones should
   get a harmony-preserving octave-only nudge, is a product-scope call.
3. A host-only SMF writer for ear-testing (shared gap) — not itself in the
   `9320` plan item; flag it as test infrastructure this item needs.
4. No new dependency proposed — the whole first slice is host-only C++ over
   existing `theory::`/`groove::`/`voicing.hpp` primitives; the ML temptation
   stays OUT.

---

## Generative motif engine (#7, Item B, node `9210`)

Musical scope by Ottorino; placement/shape by Corelli. Per the plan: **SHIPPABLE
core, dual-target** — unlike Restyle (HOST-only) and unlike a corpus importer,
this is the one generative Phase-5 item on the `arm-none-eabi` realtime path. The
anti-sameness arc's second half: Restyle (`9320`) RE-CLOTHES existing material;
this item GENERATES new melodic material from a seed. Nothing here needs a new
dependency.

### The reusable spine, and where a motif stage sits — verdict

A motif engine's natural seam is **upstream of `gesture::expand`**, at the point
the D40 loop currently reads `pattern.events` (authored `Span<const
StyleEvent>`). A motif+transform stage produces the SAME `StyleEvent` value type
— `{step, tone, octave, vel, gate, NoteSource src, ChordGesture gesture}`, pinned
at 10 bytes (`static_assert(sizeof(StyleEvent)==10)`) — so it plugs into the
existing pipeline as a *source* of specs, unchanged downstream. **Generation
output is representationally identical to authored content**, so `resolve()`,
`gesture::expand`, `VoicingState`, `groove::apply` need ZERO new code to consume
it. The only new code is what PRODUCES the `StyleEvent` sequence.

**Verdict: a gather-phase producer feeding the existing resolver — NOT a new
resolver stage, NOT a `runtime::Pipeline` stage.**

- **Not "a new resolver stage alongside gesture/voicing/groove."** Each of
  `resolve()`, `voice()`, `groove::apply()` is a TRANSFORM over an
  already-produced spec (index→pitch, re-voice, humanize). A motif engine does
  not transform an existing spec at any of those — it *originates* the spec, in
  the exact slot `pattern.events` (authored) and `gesture::expand`'s *input*
  occupy: the "gather" half, upstream of resolve/voice/groove. Placing it as a
  peer after groove makes no musical sense (groove reshapes the timing/velocity
  of an already-pitched note; a motif has not chosen a pitch yet).
- **Is "a producer feeding the existing resolver."** `gesture::expand`'s header
  comment already describes exactly this contract for a different generative
  concern: "produces StyleEvent *specs* (not resolved pitches); the caller
  resolves each spec through the unchanged, wrong-note-proof kernel... bounded
  (no heap) and reproducible (seed by position, like the arp's D16 hash)." A
  motif producer — `motif::generate(pattern, step, section_repeat, transform,
  seed) -> StyleEvent specs[]` — is the same idiom one level upstream: instead
  of reading `ev.step`/`ev.tone` literally, a motif-driven pattern computes them
  from a stored motif shape + a transform + a seed. One seam, reused twice, not
  two seams invented.
- **Not "a new Pipeline Stage (like `RestyleStage`)."** `Pipeline<StageT...>`
  composes INDEPENDENT COMPONENTS around a shared per-tick fan-out — its reason
  to exist is stitching content that ARRIVES from outside the terminal (raw SMF
  replay, detected chords). A motif engine needs no external arrival: it is
  driven entirely by the compiled `Style`/`StylePattern` tables already resident
  in `Arranger` plus the same `Key`/`ChordState` `Arranger::on_tick` already
  receives. No second component to compose with, so no Pipeline-shaped seam.

**Why this is also the SAFER classification.** The `Pipeline<StageT...>` shape,
wherever concretely instantiated today (`AccompanyPipeline`), is transitively
HOST-ONLY (its one real producer `MidiSourceStage` is unconditionally host-only).
`DESIGN.md` classifies `9210` as **SHIPPABLE** — it must cross-build
arm-none-eabi. Forcing Motif into the Pipeline-stage shape would silently demote
a locked SHIPPABLE decision to HOST-only by construction. Placing it inside
`Arranger`'s own D40 loop keeps it where `ArpeggiatorEngine` and `gesture::
expand` already prove this class of generative, no-heap, position-seeded logic
cross-builds (`tests/arm-smoke/link_gate.cpp` already exercises `Engine`, hence
`Arranger`, on the real target).

### Coordination with #1 Restyle — two mechanisms, deliberately not one

The task asks whether #1 and #7 should compose as a 5-stage Pipeline. **No — and
forcing them together would be a structural mistake.**

| | #1 Restyle (`9320`) | #7 Motif (`9210`) |
|---|---|---|
| Input | An ALREADY-EXISTING external note stream (an imported SMF melody) | Nothing external — a stored motif shape + the live `Key`/`ChordState` `Arranger` already has |
| Mechanism | New `runtime::Pipeline` stage (`RestyleStage`) between `ChorddetStage` and `Engine` | New gather-phase producer inside `Arranger::on_tick`'s D40 loop |
| Package | `arrangrr` (`arrangrr/restyle/`), instantiated by `orchestrator` | `arrangrr` (e.g. `arrangrr/arranger/motif.hpp`), instantiated by `Arranger` itself |
| Feasibility | **HOST-ONLY** by construction (its producer is host-only) | **SHIPPABLE** — cross-builds, same smoke path as `Engine` |
| Why the shape differs | Transforms a stream that only exists because a HOST tool imported a file — a cross-component composition problem | Generates from data already resident in the core — an intra-component data-source problem |

They share ONE real seam: both emit ordinary `StyleEvent`-shaped notes flowing
through the same `resolve()`/`voice()`/`groove()` kernel and the same
`OutScheduler` (D29) — that kernel needs no change for either. A unifying
5-stage Pipeline would (a) buy no shared code neither lacks, (b) force Motif's
SHIPPABLE classification to ride on Restyle's HOST-ONLY producer, and (c) blur
the one distinction (`9210` generates; `9320` re-clothes) the roadmap uses to
justify both under one "anti-sameness arc." Siblings in INTENT, not in
MECHANISM — the tree should keep them structurally separate.

Relationship, evidence-checked by reading `restyle_stage.hpp` (the already-
implemented `9320` first slice) in full: **Restyle's one new piece is an INVERSE
classifier** (`restyle::classify()`, absolute pitch → NTT tone-index); **Motif's
core direction is FORWARD** (it authors/transforms `StyleEvent` specs that flow
through the unchanged `resolve()`). Opposite-direction operations on the same
theory vocabulary, not the same problem twice. They share `theory.hpp`
primitives (Restyle uses `shape_of`+`degree_of`; Motif would use
`degree_to_semitones` — complementary halves) and the seeded-hash discipline
(Restyle's first slice does NOT use it — its snap is pure rounding — but Motif
will). Restyle instantiates its OWN `VoicingState` (a distinct lineage,
different input source); Motif, whose output IS an Arranger pattern's content,
uses the Arranger's OWN `m_voicing`/`m_groove` and needs no separate lineage. No
shared new code is required between the two — independently implementable and
testable; the only coordination point is the shared `hash()` cleanup.

### The three named transforms, defined against the model

The plan names diatonic transpose, retrograde, displacement. A "motif" is a
small, fixed-length `StyleEvent[kMaxMotifLen]` array — `kMaxMotifLen` one bar
(16 steps) or a half-bar (8), small enough to live on the stack exactly like
`NoteReq group[kMaxVoiceNotes]`.

- **(a) Diatonic transpose — needs a source motif; clean for `kScaleDegree`.**
  `tone += N` shifts the whole contour by N diatonic scale-steps while
  `degree_to_semitones`'s floor-division wrap keeps it correct at any N
  (including negative/octave-crossing; tested `degree_to_semitones(kMajor, -7)
  == -12`). For `kInterval` events, adding a constant to `tone` is a *chromatic*
  transpose — related but distinct, worth naming separately. For `kChordTone`
  events "transpose" does NOT mean the same thing: `tone` there is a
  chord-relative index (0=root, 1=third...), so adding a constant reassigns
  which FUNCTIONAL tone plays — a "chord-tone step shift"/restacking, not a
  melodic transposition. A first slice should scope diatonic transpose to
  `kScaleDegree`/`kInterval` motifs and treat `kChordTone` motifs as a distinct
  (lower-priority) case.
- **(b) Retrograde — a temporal reordering, fully expressible today.**
  `new_events[i] = old_events[N-1-i]` with each `step` remapped to
  `(motif_length - 1 - old_step)` (or the nearest occupied slot if gate lengths
  must not overlap reversed) — pure integer arithmetic over `step`/`gate`, no
  new `resolve()` path. Equally cheap for ALL three `NoteSource` kinds (never
  touches `tone`), including drum/perc `kFixed` patterns — retrograding a DRUM
  pattern is a real, cheap technique with no wrong-note risk (`kFixed` never
  resolves through NTT).
- **(c) Displacement — a rhythmic phase shift, fully expressible today.**
  `step = (step + shift) mod kStepsPerBar` — pure arithmetic on the existing
  field, applicable to every `NoteSource`/`RolePolicy`. This automates a pattern
  a human author is ALREADY doing by hand (e.g. `blues.hpp`'s hand-typed
  `kAD`/`kBD` tables shift kick/snare onsets by a few steps between variations).

All three are operations ON an existing ordered sequence — none originate
content. **A fourth, un-named-by-the-plan piece is therefore required: a
seed-motif generator** (the actual "generate new melodic material" half). This
is the genuinely open design surface (§seed generation below).

**The concrete manual-labor gap this targets.** In `blues.hpp` the same literal
`kBoogieBass` (6 events) is referenced byte-identical in 8 separate
`StylePattern` tables (`kIn2P`, `kAP`, `kBP`, `kFAP`, `kFBP`, `kFCP`, `kFDP`,
`kDP`) — the bass NEVER varies across Intro/VarA/VarB/VarC/VarD/every Fill —
while the same file hand-authors 4 genuinely different comping tables.
`funk.hpp`'s `kAD`/`kBD`/`kCD`/`kDD` are each fully hand-typed 16-step tables
with no shared derivation. A motif+transform engine could derive B/C/D variation
from ONE authored seed via transpose/retrograde/displacement, at flash cost of
the seed plus a few transform-parameter bytes instead of N duplicated tables —
a flash SAVINGS opportunity, not just a variety one.

### Keeping generated motifs musical

`shape_of` returns up to 4 chord-tone offsets; `resolve()`'s `kChordTone` branch
already wraps a tone-index past `shape.count` an octave up
(`ev.tone / shape.count`) — so an out-of-range chord-tone index from a transform
is absorbed by existing, tested wrap logic. **Every transform output stays
inside the NTT wrong-note-proof envelope (D24) by construction** — the single
strongest feasibility argument for shipping on-device: no new "is this a valid
note" logic. The CORRECT half is solved structurally; the INTERESTING half needs
concrete constraints:

1. **Contour bounding, not free random walk.** An unconstrained walk over `tone`
   (even NTT-safe) reads as noise; constrain consecutive `tone` deltas to a
   bounded range (`|delta| <= kMaxLeap`, stepwise motion dominant, occasional
   larger leap), computed over the SAME seeded hash — no floating point.
2. **Phrase shape, not a flat sequence.** The corpus's own `SectionType::
   kVarA..kVarD` is already a statement/variation/return form; a generator
   should generate ONE seed as the "statement," then apply retrograde/
   displacement to produce an "answering" phrase (call-and-response;
   Schoenberg's "developing variation" — vary a motif's rhythm/contour while
   keeping its identity recognizable).
3. **Onsets anchored to the STYLE's idiomatic grid, not 16 uniform slots.** The
   corpus's drum/perc layer IS idiomatic (a real reggae one-drop, a real samba
   clave); a comping/lead generator should draw candidate onset steps from the
   same steps the style's own authored drum pattern accents, so a generated
   reggae bassline still implies the one-drop even though its PITCHES are
   generated. Needs no new data — the style's existing `kFixed` drum
   `StyleEvent.step` values are directly readable as an "allowed onset mask."
4. **Cadential resolution.** The last strong-beat tone-index of a generated
   motif should resolve to a stable scale-degree/chord-tone (root or fifth) more
   often than chance — assertable as a property test over many seeds.
5. **Non-triviality / anti-repetition.** A trivial parameter (`shift=0`, or a
   shift equal to the motif's length) reproduces the original. A cheap
   deterministic guard: reject (re-hash) any transform whose output step-onset
   set equals the input's, bounded to a handful of tries.

None of this needs statistics, ML, or floating point — integer arithmetic over
the existing `StyleEvent`/`theory` vocabulary at the same cost class as
`groove::apply`.

### Seeded determinism (D16), dual-target/no-heap

Reuse the exact `groove::hash`/`ArpeggiatorEngine::hash` idiom (the shared
position-hash above): a pure `constexpr` function of `(seed, position)`, no PRNG
object, no carried state. For Motif, "position" decomposes to `(pattern.role,
step, section_repeat_count)` — the same granularity `groove::apply` keys on — so
"transpose +1 diatonic step every repeat" or "retrograde every other repeat" is
`f(seed, section_repeat_count) -> transform-amount`, computed fresh every tick
from a small counter, not accumulated drift.

State budget: at most one small per-pattern counter (which repeat of the section
this is), bounded, `Arranger`-member-resident, reset on style load exactly like
`VoicingState::reset()` — smaller than `VoicingState` already carries per role
(`m_last[10][8]` + `m_count[10]`). No heap: motif specs are produced into the
SAME stack-local buffers `gesture::expand` already writes; `kMaxVoiceNotes`'s
existing `ARR_ASSERT`-guarded cap bounds the worst case regardless of whether a
step's events are authored, gesture-expanded, or motif-generated. The mechanism
lives entirely inside `components/arrangrr`, inheriting freestanding-cleanliness
structurally from that package's PUBLIC build flags; it never touches
`orchestrator`/`midisrc` or any host-only package — unlike Restyle, it has no
host-only producer to be transitively host-only THROUGH.

**The shared `hash()` cleanup:** three independent hand-written copies exist
today (`groove.hpp`, `arpeggiator.hpp`, `timeline.hpp`); Motif would be a natural
fourth. It should reuse one shared `hash()` (promoted to a small `common` header,
e.g. `arrangrr/common/seeded_hash.hpp`) rather than adding a fourth copy — a
real, cheap, evidence-based cleanup worth doing alongside `9210`'s
implementation, not scope creep, not a gate.

### ABI — no new surface for generation; an additive selector if the GUI wants control

- **Output needs zero new ABI** — motif-generated notes resolve to ordinary
  note-on/off scheduled through the existing `Arranger::on_tick` `ScheduleFn` →
  `OutScheduler` → `OutEvent::midi(...)` path, like every other arranger note.
- **If the motif shape/seed is fixed at style-load time only** (authored in the
  compiled `Style` table, changed only by loading a different style/variant), it
  needs no ABI at all.
- **The task asks about exposing/seeding the generator from the GUI** — that
  DOES want a live wire surface, and the codebase has the exact idiom:
  `GrooveField`/`ArpField` are additive `enum class` field-selectors paired with
  a `set_field(...)` clamp, each needing only one `Command` variant plus the
  selector enum. **Recommendation: add `Command::kMotif` + a `MotifField`
  selector** (`kEnabled`, `kTransformKind` [transpose/retrograde/displacement],
  `kAmount`, `kSeed`, mirroring `GrooveField::kSeed`/`ArpField::kSeed`) the day a
  live slice is implemented — additive even under the OLD append-only discipline,
  so it does not spend the Phase-5 unfreeze license. This matches the roadmap's
  trajectory: `groove.hpp`'s comment already names `GrooveParams` as the first
  parameter layer the future Generative Director (`D37`/`10000`) will drive; a
  `MotifField` shaped like `GrooveField`/`ArpField` is what lets the eventual
  Director pilot Motif the same way.

### Seed-motif generation — the genuinely open design fork

Three real alternatives for producing the FIRST motif a transform then varies:

- **Option 1 — Author-seeded (one hand-written motif per style/role, transforms
  do the rest).** A style authors ONE motif per comping/lead role; B/C/D
  variations are DERIVED via transpose/retrograde/displacement instead of
  hand-typed. **SHIPPABLE, dual-target, no-heap.** Directly fixes the measured
  `kBoogieBass`-reused-8× redundancy with almost no new runtime code — the
  "generation" is really "guided variation of curated material," the safest and
  most easily-validated option. Flash SAVINGS; runtime cost is transform
  application O(motif length), once per section entry. Engineering risk LOW,
  musical risk LOW-MEDIUM (ceiling bounded by the seed's own quality — no risk
  of inventing bad melody from nothing).
- **Option 2 — Constrained-random seed (generate the FIRST motif from a seeded
  walk under the guardrails above).** **SHIPPABLE, dual-target, no-heap** — same
  cost class as Option 1's transforms. Higher ceiling (motifs no author wrote,
  still inside every guardrail). Engineering risk LOW; **musical risk
  MEDIUM-HIGH — exactly the "hard to make interesting" surface the plan flags**:
  a constrained walk can still sound characterless even with contour/cadence
  rules. Mitigating needs real ear-testing against genre reference, not just
  property tests.
- **Option 3 — Statistically-trained seed (Markov/n-gram over scale degrees,
  trained OFFLINE, baked to a `constexpr` transition table).** This is
  explicitly the SEPARATE roadmap item `9220` ("Offline-trained Markov/grammar
  on scale degrees, baked constexpr — runtime SHIPPABLE / training HOST-ONLY").
  The runtime lookup is SHIPPABLE and dual-target (no different from a baked
  constexpr `Style` table); the training step is HOST-ONLY tooling comparable to
  `arrstyle-converter`'s pipeline, needing no ML library (a frequency-count
  Markov model). Its obvious training data (the Yamaha corpus) shares the
  unresolved provenance/licensing flag with Item C. **`9210` should NOT quietly
  absorb `9220`'s scope.**

**Ranking:** Option 1 is the safest, cheapest first slice and directly fixes a
measured redundancy — recommend it as the actual FIRST slice. Option 2 is the
natural second step once the transform machinery exists (shared code either way).
Option 3 is a distinct roadmap item (`9220`) that stays out of `9210`'s first
slice.

### Testing musical acceptability beyond golden byte-equality

1. **Property tests over MANY seeds, not one golden seed.** The cadence-
   resolution and leap-bound rules are directly assertable as loop-over-N-seeds
   property tests (`test_arp_random_deterministic` is the existing precedent:
   assert reproducibility AND assert the output is not degenerate, e.g.
   `CHECK(any_off_root)`). Assert BOTH "same seed reproduces" and "over 100+
   seeds, no motif violates the leap bound / fails to cadence."
2. **Anti-repetition assertion** — generate B/C/D from A under the same seed
   policy and assert no two share an identical onset-step set.
3. **A real listening path is still missing** (the shared SMF-writer gap) —
   scope it ONCE rather than re-discovering per generative item; implementation
   should not reach "sounds right" sign-off without one.
4. **Genre-reference check, by ear, against known convention — NOT invented by
   the property tests.** Whether a generated bossa comping motif sounds like
   idiomatic bossa (not just "correct and non-repeating") is a musician's ear
   judgment against real reference, qualitative and not reducible to a unit test.

### Open forks / flags

1. Seed-generation option — Option 1 first (transforms over an authored seed),
   Option 2 as a fast-follow; a product-scope call.
2. `kChordTone`-motif transpose semantics — whether a first slice attempts
   "transpose" on `kChordTone` motifs (where it is a chord-tone restack) or
   scopes it OUT; a naming/scope precision question.
3. The shared `hash()` cleanup — a small, low-risk refactor worth doing
   alongside `9210`.
4. The missing SMF-writer / audio-render path — the same gap Restyle flagged,
   blocking both anti-sameness items' ear-validation; scope it once.
5. `9220` boundary — a distinct roadmap item with its own corpus/provenance
   dependency; `9210`'s first slice should not absorb it.
6. **The motif-data authoring shape is a real NEEDS-DECISION.** `StyleEvent` is
   pinned at 10 bytes and has already absorbed three additive fields kept last
   in existing padding. Whether a motif is (a) a NEW parallel data type
   alongside `StylePattern` (e.g. a `MotifPattern` with `Span<const MotifStep>`,
   requiring `StyleSection` to carry a second/tagged pattern list) or (b) folded
   into `StylePattern`/`StyleEvent` as another `RolePolicy`/`NoteSource`-style
   tag is a data-model call — Ottorino for the musical vocabulary, the reviewer
   only if the shape question becomes structural once a concrete proposal exists.
7. Whether Motif ever wants to CAPTURE a live-played phrase (not just transform
   an authored shape) is out of `9210`'s wording but a natural follow-on; the
   precedent is `ArpeggiatorEngine`'s note-capture idiom (still intra-`Engine`,
   not Pipeline-shaped).
8. No new dependency proposed anywhere; the ML temptation stays OUT per the same
   roadmap line Restyle reaffirmed.

---

## Style corpus import (#C, nodes `9400`/`9420`/`9430`)

Ottorino, scoping analysis (read-only). Feeds Nazzareno's `9420`/`9430`
implementation slot.

### Headline correction — the roadmap premise is stale

Every document that describes this work (`DESIGN.md` `9430`,
`docs/style-corpus-and-generation.md`,
`arrstyle-converter/DESIGN.md`) says the SFF importer is inspect-only and CASM
decode is not implemented. Measured against the tree, that is no longer true:

- `apps/tools/arrstyle-converter/src/casm.cpp` (468 lines) is a real,
  bounds-checked big-endian CASM/CSEG/Ctab/Ctb2 decoder: per-channel source
  chord root/type, `NoteTranspositionRule`, `NoteTranspositionTable`,
  note-register limits, retrigger threshold, and section markers
  (`Intro/Main/Fill In/Ending/Break` + variation letter) — `to_ntt`/`to_ntr`,
  `role_from_destination_channel` (the canonical Yamaha channel-9..15 → `Role`
  map), `parse_style_section`.
- `sff_import.cpp` `import_sff()` actually calls `decode_casm`, builds real
  `SectionSpan`s, groups notes into per-role `PhraseLane`s with
  `policy_from_channel`/`retrigger_from_channel`, applies a bass-register
  fold/drop filter, and reports every lossy decision through `Diagnostics`.
- `test_sff_import.cpp::test_real_corpus_samples` runs this against two files
  under `../resources/styles/extra/**`, asserts `import_sff()` succeeds and the
  model validates; CI is green (`ctest -R sff_import` → Passed 0.07s). A direct
  CLI run on a real corpus file decoded 15 sections from CASM (SFF1), with real
  per-role lanes (`drums/fixed`, `bass/chord_tone`, `chord1/chord_tone`,
  `phrase/chord_tone`, `percussion/fixed`), real event counts (20–56/lane), real
  `source_root_pc=0`/`source_quality=maj7` provenance.

**One inconsistency survives inside the code:** `cmd_inspect` still routes SFF
files to `inspect_sff()`, which prints the stale "SFF: unsupported subset —
inspect-only" / "casm: present (not decoded)" even though `import-sff` on the
same file now decodes it fully — `inspect` was never updated when `import_sff`
grew CASM decoding. A one-line fix for Nazzareno.

**Net effect on scope:** the least-predictable reverse-engineering slice —
byte-level CASM/CSEG parsing — is already done and tested for SFF1 (partially
SFF2). The actual gap is downstream: nothing lowers the resulting `StyleModel`
into the constexpr `Style`/`StyleSection`/`StylePattern` shape the device
consumes. That lowering function does not exist anywhere (confirmed by
`search_code`/`grep`: zero hits producing `arrangrr::Style` from `StyleModel`).
`DESIGN.md` names it `9420` "Style compiler (data → .cpp constexpr for the device
path) — HOST-ONLY," still unbuilt; `docs/style-corpus-and-generation.md` converges on
the same conclusion ("teach `arrstyle-converter` to emit the existing constexpr
header shape from its `StyleModel`"). This scope targets exactly that gap.

### What is concretely extractable, and the field mapping

**Two independent existing pipelines (do not conflate):**

| Tool | Language | Input | Output | State |
|---|---|---|---|---|
| `arrstyle-converter` | C++, in the CMake host build | one `.sty`/`.sst`/`.prs` | `StyleModel` → `.arrstyle.json` | CASM decode real; no device emitter |
| `arrstyle-extractor` (`extract_kb.py`, 534 lines) | Python 3 stdlib-only, NOT wired into CMake/CI, offline research | whole corpus tree | per-file abstracted JSON + genre/role/section aggregates | already run over the full corpus |

The extractor predates the C++ CASM decoder (its own README says "CASM is not
decoded... top follow-up") and its bass/melody degree extraction still assumes
the Yamaha default CMaj7 source chord rather than reading it from CASM —
consistent evidence that `casm.cpp` was built after the extractor and the
roadmap prose was never reconciled. The extractor's output already exists on
disk and is large: `../resources/kb/styles/discovery-summary.json` measures
`files_total: 1184`, `parse_status {ok: 1109, partial: 21, failed: 54}`,
`avg_confidence: 0.932`, `patterns_extracted: 63564`. Per-file entries carry
`musical_profile` (genre, tempo, swing, time-sig, feel, energy) and a
`patterns[]` array tagged `pattern_type` (`drum_grid`/`bass_movement`/
`chord_rhythm`/…), `rhythm_cell_16`, `syncopation`, `density`, plus
`generation_notes`/`mutation_axes` — genre-tagged, abstracted, almost directly
generator-ready data for 1109 SFF files. `aggregate/*.json` roll these up per
genre and are exactly the input `arrstyle-converter`'s existing
`canon.cpp`/`build-canon` command consumes to emit a coarser constexpr artifact
(top-N rhythm cells + bass templates per genre). **`build-canon` is a cousin,
not the missing tool** — it answers "what does funk statistically do" for a
future rule-based generator, not "reproduce this specific Yamaha style."

So there are three candidate sources feeding `9420`:
1. Raw SFF bytes → `import-sff` → `StyleModel` (per-file, exact, CASM-grounded,
   but un-reduced/un-quantized).
2. The extractor's `extracted/*.json` (per-file, abstracted, genre-tagged, but
   NOT CASM-validated for the bass/melody degree assumption).
3. The extractor's `aggregate/*.json` (cross-corpus statistical rules per genre
   — feeds `build-canon`).

**Recommendation:** `9420`/`9430` should build on source (1), because it alone
carries verified `source_root_pc`/`source_quality`/NTT provenance per lane —
the one thing that makes chord-tone reduction honest rather than assumed.

**SFF/CASM → compiled-`.cpp` field mapping.** Target confirmed by reading
`bossa.hpp`: a `Style` is a `Span` of `StyleSection`s (`type`, `bars`,
`Span<StylePattern>`); each `StylePattern` is `{role, policy,
Span<StyleEvent>, gm_program, voicing}`; each `StyleEvent` is `{step: 0..15,
tone, octave, vel, gate, src, gesture}`, pinned to 10 bytes.

| `StyleModel` field | Device `Style` field | Status |
|---|---|---|
| `name` | `Style::name` | clean, 1:1 |
| `tempo_milli_bpm` | `Style::tempo` (`BpmX100`) | clean, unit conversion only |
| `SectionKind`+`SectionVariation` | `SectionType` (13-way enum) | clean, already isomorphic |
| `StyleSection.bars` | `StyleSection.bars` | clean, direct |
| `PhraseLane.role` (`Role`, 10-way) | `TrackRole` | clean, near-1:1; `kArp`/`kLead` naming needs a 1-line reconciliation against `TrackRole`'s enumerators |
| `TranspositionPolicy` (`kFixed`/`kChordTone`) | `RolePolicy` | clean, literally the same two values |
| `PhraseLane.note_low/note_high` | not modeled today | **model gap** — the device clamps register only implicitly via authored `tone`+`octave`; CASM's explicit limits have nowhere to land without a new field or a compiler-side clamp-and-drop |
| `PhraseEvent.tick` (absolute, `source_ppqn` units, e.g. 1920) | `StyleEvent.step` (0..15, 16th-grid) | **NOT built** — needs `step = (tick mod ticks_per_bar) / (ticks_per_bar/16)`; mechanically simple ONLY when the event already sits on a 16th boundary |
| `PhraseEvent.note` (absolute MIDI) | `StyleEvent.tone` (chord-tone index 0..3 for `kChordTone`) | **NOT built** — the reduction is a tractable deterministic `(note - source_root_pc) mod 12` lookup against `{0,4,7,11}` for maj7 (verified by hand: 59→11→7th, 64→4→3rd, 67→7→5th, 71→11→7th, bass 36→0→root, 31→7→5th) FOR notes that ARE chord tones; extensions/passing/chromatic-approach tones do NOT fit the 4-slot table and need a policy: fall back to `NoteSource::kInterval` (semitone offset from root — already in the vocabulary, D39, unused by any emitter) or `kScaleDegree` for melodic/phrase roles. A real musical judgment per role. |
| `RetriggerPolicy` | not modeled today | **model gap** — the arranger re-resolves chord-tone notes every tick (approximating "retrigger"), but "sustain" (a genuinely held note through a chord change) has no way to be expressed |
| Yamaha guitar NTR / strum offset | `ChordGesture::kStrumUp/Down` | model exists (D40) but nothing infers it from CASM's NTR=Guitar; would need a heuristic (fast near-simultaneous onsets across adjacent channel notes) |
| `OTS` voice presets | `StylePattern.gm_program` | not read at all — `decode_casm` skips `OTS`; would need a small chunk parser |
| Two chord channels (Chord1 on-beat / Chord2 offbeat) | `TrackRole::kChord1`/`kChord2` | model is fine; source data carries it via `destination_channel` 11 vs 12 — genuinely easy, already resolved by the channel map |

Summary: **cleanly-mapping (no new engineering)** — name, tempo, section
kind/variation, bars, role, transposition policy, Chord1/Chord2 distinction.
**New function (mechanical, low-risk)** — tick→16th-step quantization,
chord-tone-index reduction for the common case. **Judgment call (musical)** —
non-chord-tone reduction fallback, register-limit handling, retrigger semantics,
guitar-strum gesture inference, OTS voice mapping.

### Least-predictable-effort parts, and the minimal first slice

Where the mess actually is:
1. **SFF2 `Ctb2` is only partially decoded** (`casm.cpp:132-134`: the SFF2 Ctb2
   stores NTT/limits in a richer per-chord-group sub-structure not fully decoded;
   the role-derived policy covers those files). Most of the corpus by PPQN (1920)
   is Tyros/PSR-S-series, i.e. likely SFF2-heavy — a large fraction fall back to
   the role-inferred `kFixed`/`kChordTone` split rather than true per-channel NTT
   (losing `kBass`/`kMelodicMinor`/`kHarmonicMinor` distinctions the format
   encodes). Reported via `diag.info` (not silent to the operator) but a real
   fidelity ceiling.
2. **Swing/microtiming vs a fixed 16-step grid.** The device grid is exactly 16
   steps/bar; genre feel is expressed via `GrooveParams.swing` at PLAYBACK. Real
   SFF recordings already have swing/humanization baked into absolute tick
   positions. Naively snapping either destroys the feel (swing flattened to
   straight steps) or double-swings (device `GrooveParams.swing` applied on top
   of already-swung quantized data). Snap-and-let-GrooveParams-restore vs bake
   exact micro-offsets is a genuine per-genre call.
3. **Chord-tone reduction beyond the 4-slot table** — dominant for walking-bass
   (jazz/blues), funk ghost-note runs, bossa/samba syncopated comping.
4. **The ~1% non-`MThd` and ~2% CASM-absent files** fall to
   `import_raw_fallback` (single flat section, role from channel only, no
   harmony provenance) — fine as an honest degrade, not a source for a showcase
   first slice.
5. **OTS-driven voice choice is unread** — imported styles would ship without
   `gm_program`, silently using the arranger's current voice. Minor but will
   look wrong on first listen if not flagged.

**Minimal first slice — one genre, one file, full section set, SFF1-preferred.**
- **Genre:** bossa or samba — the discovery-summary shows `bossa: 28` tagged
  files, the built-in `bossa.hpp`/`samba.hpp` already encode the idiom by hand
  (side-stick clave, two-feel root-fifth bass, 7th comping), so a fresh import is
  directly A/B-listenable against a known-good reference, and the rhythmic
  signature is a sharp pass/fail signal — much sharper than "pop."
- **File:** prefer an SFF1 file if one exists for the genre (`decode_casm` fully
  resolves NTT/register/retrigger for SFF1, sidestepping risk #1 for the first
  slice). If only SFF2 is available, that is useful signal — it forces an early
  SFF2 decision rather than deferring the hardest case.
- **Section scope:** the FULL section set from that one file (Intro/Main A-D/
  Fill/Ending — whatever it has), since section mapping is already clean and a
  partial slice would understate how much is already free.
- **What "proves the path end to end" means:** a single new
  `arrstyle-converter` command/mode taking one `.arrstyle.json` (the
  already-working `import-sff` output) and emitting ONE `.hpp` in the exact shape
  of `bossa.hpp` — same `namespace styles::<name>`, same `kStyle` constant, wired
  into `style.hpp`'s `kBuiltins[]` as entry 17. There is **no loader/cap
  infrastructure to build**: today's 16 built-ins are plain compiled-in constexpr
  data with no `kMaxSections`/`kMaxPatterns` runtime caps at all. Layer B (the
  no-heap runtime-loadable `.arrsty` blob) is future work, correctly out of
  scope.
- **Acceptance:** the new style builds on both host AND `arm-none-eabi` (plain
  constexpr, no new dependency), the golden suite is unaffected (an additional
  17th style is additive — no existing golden moves), and the style is manually
  auditioned against the genre rules in
  `docs/style-corpus-and-generation.md` Part 5 before calling it done.

### License / provenance — SHIP-GATE, not build-gate

This is **not** a blocker on building the importer for private/dev use against
`../resources/` (which sits OUTSIDE the repo, is never committed, and the tool's
`DESIGN.md` §11 already states it "only parses user-provided files at the user's
direction" with "no shipped proprietary format internals... no copyrighted
factory-style content"). It IS a hard gate on distributing ANY style produced
from this corpus in a public build. Provenance measured directly:

- `../resources/extra-sources/archive-urls.txt` and `download-styles.sh` show
  the corpus was scraped from third-party fan/community hosts:
  `sandsoftwaresound.net`, `a-mc.biz/makemusic`, `psrtutorial.com` — a
  long-running Yamaha-arranger community hub aggregating and redistributing style
  files, official and fan-made, with no visible per-file licensing metadata.
- `../resources/styles/extra/E-pop-v1/E-Pop/README.TXT`: "quick and dirty
  conversions to SFF2... Please feel free to create new styles of your own" —
  fan-derivative content, community recreations modeled on copyrighted
  commercial songs (the README maps each style to a specific pop song). "Feel
  free to create your own" is permission to REMIX, not a redistribution license
  for the files, and says nothing about the underlying songs.
- `../resources/www.jjazzlab.org/pkg/*.zip` (JJazzLab-Jazz-1460, -Pop-400,
  -Beatles-71, -PinkFloyd-60, -StevieWonder-30) are official packs from an
  open-source arranger, but several pack NAMES reference copyrighted
  artists/albums — a signal these are artist-idiom packs, not necessarily ones
  JJazzLab holds unrestricted redistribution rights to onward. Their specific
  license terms were not fetched — **unverified, not cleared**.
- Bulk factory-adjacent Yamaha `.sty` content (PSR-S910/S950/1700, Tyros) is
  Yamaha's proprietary format and, where sourced from factory presets, is Yamaha
  IP redistributed without an apparent license by the fan sites above.

**Risk statement:** this corpus is proprietary-adjacent at best and
copyright-uncleared at worst across every source. **No style compiled from
`../resources/` may ship in a public build until the owner explicitly clears its
specific source file** — a per-file/per-pack gate, not a blanket "the importer is
fine so the output is fine" assumption, because a compiled `.hpp` derived from a
copyrighted pattern is still a derivative. The private/dev use case (an engineer
importing one file locally, never committing the corpus or its derivatives) is
unaffected and is what the first slice should be — but that first-slice `.hpp`,
if from any file in the categories above, must not be committed to the public
tree without explicit owner sign-off naming the source and its clearance.

### Validating an imported style musically

Three layers, cheapest first:
1. **Structural validation (already exists).** `validate_style()` checks JSON
   invariants (enum names, ranges, non-decreasing ticks). Run it first — it
   catches "broken," not "wrong genre."
2. **Rule-based genre audit (mechanical, a small NEW checker, HOST-only, no new
   dependency).** `docs/style-corpus-and-generation.md` Part 5 gives 79 numbered,
   per-genre checkable rules, several assertable against the emitted
   `StyleEvent` tables without listening: RG1/RG2 (reggae: chord stabs only on
   offbeats, kick+snare on beat 3 not 1), D1 (disco: kick on every quarter, steps
   0/4/8/12), C1 (country train bass: alternating root/fifth on 1 and 3),
   F1/F2 (funk: ghost-note velocities 30-50 — a velocity-histogram bimodality
   test). Turns "sounds right" into a small deterministic test in
   `arrstyle-converter`'s own suite — SHIPPABLE tooling, cheap, and exactly the
   check that should exist BEFORE any imported style is trusted.
3. **Actual listening (human, non-negotiable).** Load the compiled style through
   the host player path (`hostrt`/`gui-sonotron`), play against a plausible
   chord progression, judge by ear against the genre's convention. Rule-checking
   proves a style is NOT obviously wrong; only listening proves it IS musically
   right. Neither layer replaces the other.

### Open decisions for the owner

1. Which genre for the first slice — recommend bossa or samba for the
   A/B-listenable signature; a product taste call.
2. SFF2 `Ctb2` full decode — worth the reverse-engineering now (unlocks the
   majority-PPQN=1920 slice at full NTT fidelity) or does the role-inferred
   fallback suffice for a first wave? An effort/value tradeoff.
3. Chord-tone-reduction fallback policy for non-chord-tone notes
   (`kInterval` vs `kScaleDegree` vs drop) — a musical-engineering call best
   made with a concrete file in hand.
4. License clearance of the first-slice source file(s), and every subsequent
   one, before any imported style is committed to the public tree — the hard
   ship-gate, the owner's call per source/pack.
5. `inspect_sff`'s stale message — a one-line correction whoever picks up
   `9420`/`9430` should make in passing.

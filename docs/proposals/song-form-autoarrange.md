# Song-form auto-arrange for the GUI's auto-song (task #12, "style depth D")

Status: proposal / read-only analysis. No product code touched.

## 0. The question

Today the GUI's "auto-song" advances scenes in pure ROUND-ROBIN:
`(active_scene + 1) % scene_count`, every `scene_bars` bars, forever, with no
notion of song FORM (intro once -> body -> ending once -> stop). This document
answers three questions with code evidence: (1) does the engine already have a
song-form/auto-arrangement concept, or is form entirely host-driven; (2) what
is the minimum to give the GUI a real song form; (3) how should auto-song use
the Ending sections that already exist in every style.

## 1. Does the engine already have a song-form concept?

**Answer: partially, and in TWO separate, non-overlapping mechanisms — neither
of which is "sequence of sections for a song" in the intro/build/ending sense
the task asks about; both are LOCKED, deliberate designs.**

### 1.1 The Arranger's own one-shot resolution (real, but LOCAL, not a song)

`components/core/arrangrr/include/arrangrr/arranger/arranger.hpp:441-499`
(`Arranger::on_tick`, the bar-boundary switch) resolves section endings by
TYPE, not by an authored sequence:

```
if (section_is_fill(m_current) || section_is_intro(m_current)) {
  next = m_return_to;               // arranger.hpp:470-471
} else if (section_is_ending(m_current)) {
  result.stop_transport = true;     // arranger.hpp:472-474
  return result;
} else {
  ++m_motif_repeat;                 // plain variation loop
}
```

- `section_is_intro`/`section_is_ending`
  (`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:43-44`)
  are ordinal range checks (`t <= kIntro2`, `t >= kEnding1`), not an authored
  chain.
- `m_return_to` (`arranger.hpp:827`) is a single scalar, always the LAST
  variation entered (`request()` arranger.hpp:309-310, `request_style()`
  arranger.hpp:338-339, `on_transport_start()` arranger.hpp:365-366) — there is
  no stack, no "return to the *song's* next planned section."
- Confirmed by tests: `test_fill_one_shot_returns_to_variation`,
  `test_intro_leads_to_variation`, `test_ending_stops_transport`
  (`components/core/arrangrr/tests/test_arranger.cpp:588-633`) — the last one
  proves the engine genuinely halts: `CHECK(!b.e.transport().playing())` plus a
  `kStopped` `OutEvent`, after only sending `style section ending1` and letting
  it run out its own bars.
- Wired at the Engine level unconditionally:
  `components/core/arrangrr/include/arrangrr/engine.hpp:829-838` — on
  `stop_transport`, `Engine::fire_arranger` calls `m_transport.stop()`,
  releases held chord notes, emits MIDI Stop, and publishes a transport
  `OutEvent`. This runs for BOTH the CLI and the GUI host, unconditionally —
  it is not something either frontend has to opt into.

This is real, tested, and **entirely host-agnostic** — but it is a *local*
rule ("this section type resolves to that other section"), not a *song*
("do intro, then A, then B, then ending"). It cannot express "play VarA for 8
bars then VarB for 8 bars then Ending" — it only knows one-shot vs loop vs
stop by TYPE.

### 1.2 `SceneChain` (node `8100`) — a real linear song-chain primitive, LOCKED OUT of this feature by the owner

`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp` is a
complete, tested (`test_scene.cpp`, `test_scene_hardening.cpp`, 3 files/844
lines per `docs/proposals/repeat-zone-real-contract.md:378`), dependency-free,
no-heap **core** primitive that is structurally almost exactly "song form":

- A `StaticVector<SceneStep, kMaxScenes>` chain (`scene_chain.hpp:130`), each
  `SceneStep` referencing a `PerformanceStore` slot index + `n_bars` +
  `TimeSig` (`scene_chain.hpp:51-58`).
- Advances **once per bar** (`on_bar`, `scene_chain.hpp:110-127`), holding
  `step.n_bars`, then firing the next step's transition.
- **Is linear, does NOT loop**: "the last scene simply holds once the chain
  stops advancing... looping the whole song is a host-level gesture, not a
  SceneChain-internal concept" (`scene_chain.hpp:33-36`, confirmed by
  `on_bar`'s own `m_index + 1 >= m_steps.size()` branch, `scene_chain.hpp:121-124`,
  which sets `m_playing = false` and stops).
- Each `PerformanceStore` slot's `Performance.variation` field IS a real
  `SectionType` (`components/core/arrangrr/include/arrangrr/perf/performance.hpp:124`,
  comment `// SectionType`), applied atomically via
  `Engine::apply_scene_transition` -> `Engine::apply_performance`
  (`components/core/arrangrr/src/engine.cpp:1034-1037, 1751`).
- Wired end to end: `Engine::cmd_scene` dispatches
  `kSceneAdd/kSceneClear/kScenePlay/kSceneStop`
  (`components/core/arrangrr/src/engine.cpp:948-1021`).

**This is exactly a "song = ordered chain of scene snapshots, holds N bars
each, no implicit loop" primitive** — and it composes cleanly with §1.1: a
`SceneStep` can select `variation = kEnding1`, and once that step's *own*
section reaches its bar length, the Arranger's own one-shot resolution (§1.1)
independently fires `stop_transport` — the two mechanisms do not conflict.

**But it is locked OUT of the GUI's "auto-song" by an explicit owner
decision.** `docs/proposals/repeat-zone-real-contract.md` §5 ("Scene-chain
auto-advance — the central finding") and §8b ("OWNER SIGN-OFF... LOCKED") are
unambiguous:

> "auto-song → the grid-COLUMN reading (Ableton 'scene launch, auto-follow').
> auto-song advances which Repeat-Zone COLUMN is active at bar/section
> boundaries. This is the NEW engine mechanism of §5/§6 item 5... **NOT
> `SceneChain`. `SceneChain` (node 8100) stays a separate concept — this
> workstream does not touch it.**" (`repeat-zone-real-contract.md:496-501`)

Grepping the GUI source confirms the decision was honored: `kSceneAdd`,
`kScenePlay`, `kSceneStop`, `kSceneClear` do not appear anywhere in
`apps/gui-sonotron/src/*.cpp`. The GUI has never sent a single `SceneChain`
verb. `SceneChain` is real, shippable, core-resident song-form — and entirely
unused today.

### 1.3 What actually shipped instead of "item 5"

The contract's own §8b locked "item 5" as *"a new engine mechanism (an
engine-owned `m_active_scene` cursor + `auto_song` flag, bar-boundary driven,
reusing `kSceneQuantize`)"*. What is in the tree today
(`apps/gui-sonotron/src/grid_panel.cpp:519-606`, `update_auto_song`) is
**simpler than even that spec: a purely HOST-SIDE approximation** — `fx.active_scene`/`fx.auto_song`
are GUI-local `V02State` fields (not Engine state), and advancing sends two
ordinary wire verbs (`style section <name>` + `launch scene <n>`,
`grid_panel.cpp:586-604`) rather than any dedicated engine primitive. No new
ABI, no engine change — a further-simplified, host-side-only implementation of
the locked decision (matching the "auto-song is GUI-driven host-side" note
already on file). This is *why* question 2 below has real host-side headroom:
the currently-shipped mechanism is deliberately the cheapest possible reading
of the locked decision, not the ceiling of what's feasible host-side.

### 1.4 Conclusion for Q1

Form is **not** authored as an intro→body→ending sequence anywhere today.
The Arranger contributes real, tested, engine-level ONE-SHOT SEMANTICS by
section TYPE (intro/fill resolve back to the running variation; ending stops
transport) — a necessary ingredient, not a sufficient one. A genuine linear,
non-looping, bar-held CHAIN primitive (`SceneChain`) exists in core and is
production-ready — but is explicitly, currently, by owner decision, reserved
for a *different* concept (whole-rig `Performance` recall scenes) and is not
wired to the GUI's grid-column auto-song. **Today, "song form" for the GUI's
auto-song is 100% host-side, and the host side currently only implements
round-robin — it authors no form.**

## 2. Minimum to give the GUI real song form

Two genuinely different answers depending on the SHAPE the owner wants; both
are evidence-grounded below, with feasibility labels.

### Option A — host-side authored per-song section sequence (SHIPPABLE now, no core change)

Replace `next_scene_to_launch`'s modulo-wrap
(`apps/gui-sonotron/src/grid_model.cpp:110-124`,
`return (normalized + 1) % scene_count;`) with a lookup into an
**authored, ordered sequence of scene indices** that does NOT wrap: e.g.
`[0 (Intro), 1 (VarA), 2 (VarB), 3 (VarC), 5 (Ending1)]`, played once,
stopping (not wrapping) after the last entry. Concretely, minimal shippable
shape (illustrative, not a patch):

- `GridModel` (or a new small host-only struct beside it) gains an authored
  `std::vector<std::size_t> song_order` (or reuses the existing 5 demo scene
  slots verbatim, just changing what "next" means).
  `next_scene_to_launch` changes from `(active+1) % count` to "the next entry
  in `song_order` after `active_scene`'s position in it, or `std::nullopt` if
  `active_scene` is the LAST entry" — mirroring `SceneChain::on_bar`'s own
  "last step holds, does not advance" discipline (`scene_chain.hpp:121-124`)
  exactly, just implemented host-side over the grid columns instead of over
  `PerformanceStore` slots.
- **Ending as the terminal step, not a stop-triggering special case the host
  must detect**: put an Ending-mapped column (`kEnding1`/`kEnding2`, already
  reachable today via the "outro" drag row,
  `apps/gui-sonotron/src/browser_panel.cpp:26-44`, per the prior
  `intra-style-section-differentiation.md` §4.3 finding) LAST in `song_order`.
  The Arranger's own §1.1 one-shot resolution then fires `stop_transport` on
  its own once that column's bars finish — the host does not need to special-
  case "this is the end," it only needs to stop *scheduling further advances*
  past the last entry, which is a no-op it already needs for any non-wrapping
  sequence.
- Cost: a handful of lines in `grid_model.cpp`/`grid_panel.cpp`, a small
  authoring UI (or, cheapest, a fixed default sequence to replace
  `kDemoSections`, `grid_panel.cpp:207-213`, exactly like the prior doc's own
  item 1 proposal for adding an Ending demo column). No ABI change, no core
  change, no new wire verb — reuses `style section` + `launch scene` verbatim.
- **Label: SHIPPABLE.** Host-only, dependency-free, no-heap-irrelevant (host
  tooling, not realtime core), fits the existing test harness style
  (`test_grid_panel_auto_song*.cpp`).

**What Option A cannot do:** per-scene REPEAT counts ("play VarB for exactly
2 loops, not just N bars") without also tracking a loop counter — but that is
a small additive extension of the same host-side model (a `repeat_count` per
`song_order` entry), not a different mechanism.

### Option B — a real core mechanism (SceneChain, reused or generalized)

Reopen the locked §1.2 decision and let auto-song actually author + play a
real `SceneChain` (`kSceneAdd`×N + `kScenePlay`), where each `SceneStep`'s
`Performance.variation` is the section for that "movement" of the song. This
gets the SAME non-looping, bar-held, linear chain semantics as Option A, but
for FREE at the core level (already implemented, tested, no-heap,
dual-target) instead of reimplementing the same shape host-side.

- **Cost of reopening it**: `SceneChain` operates on whole `Performance`
  snapshots (style + variation + mute/routing/transpose/groove/chord-mode,
  `performance.hpp`), not bare "which grid column." Retrofitting today's 5
  Repeat-Zone clip-launch columns onto `Performance` slots would require
  either (a) a `Performance` per song-step that mirrors the column's current
  style+section (extra authoring, and a second source of truth alongside
  `GridModel`'s own `scene_section`/`scene_bars`), or (b) redefining what a
  "scene column" IS in terms of `Performance`, which is precisely the
  conceptual collision `repeat-zone-real-contract.md` §5 says the owner
  explicitly avoided ("reusing it would silently redefine 'scene' a third
  way and reintroduce the very ambiguity the codebase went out of its way to
  kill").
- **Label: HOST-ONLY-WIRING-of-an-EXISTING-CORE-MECHANISM, but
  NEEDS-OWNER-DECISION** to reopen the §8b lock — I will not silently
  re-litigate a signed-off decision. If the owner wants a "real" song mode
  distinct from the Repeat-Zone launch grid (e.g. a separate "Song" transport
  mode, matching `docs/DESIGN.md`'s own `Song[]`/`Scene[]` entities, lines
  194-196), `SceneChain` is READY TODAY and costs zero new core engineering —
  it is a GUI authoring/wiring task only.

### Option C — generalize the Arranger's one-shot rule into an authored sequence at the CORE level (MODEL GAP — not shippable as-is)

A THIRD path, not reducible to A or B: give the Arranger itself (not
`SceneChain`, not the host) a small authored table of "which section comes
after which," so a *single* `style section X` request can express "and then
Y, then Z, then stop" without any external driver polling bars. This is
musically closer to how real hardware arrangers author a "song list" per
style/performance, but:

- Nothing in `arranger.hpp`/`style_model.hpp` today models a sequence *longer
  than one hop* — `m_return_to` is a scalar (§1.1), not a queue.
- Building it would mean adding a bounded queue/table to `Arranger` (still
  no-heap-compatible, `StaticVector`-shaped, in the same spirit as
  `SceneChain`) plus new wire verbs to author it — real core surface, real
  ABI cost, and materially overlaps `SceneChain`'s existing job.
- **Label: INSTRUCTIVE-BUT-INFEASIBLE as a NEW core primitive right now** —
  not because it's technically hard (it's a small, no-heap, dual-target-safe
  addition, structurally similar to `SceneChain`), but because it would be a
  THIRD "sequence of scenes" concept alongside `SceneChain` and the GUI's own
  round-robin, worsening exactly the naming/ownership ambiguity
  `repeat-zone-real-contract.md` already fought to kill. If either A or B is
  pursued first and still feels insufficient, THIS is the fallback — not a
  parallel-track first move.

## 3. How should auto-song use the existing Endings, and does the engine honor a stop after an Ending one-shot?

**"End of song" should mean: the LAST scene in an authored (non-wrapping)
sequence is an Ending-typed column; once that column's own section reaches
its authored bar length, the Arranger's own §1.1 one-shot rule
(`arranger.hpp:472-474`) fires `stop_transport` on its own — the host does
not compute or request the stop, it only has to stop *scheduling the next
round-robin advance* past that point (which the non-wrapping fix in Option A
gives for free: `next_scene_to_launch` simply returns `std::nullopt` once
there is no "next" entry).**

Does the engine already honor this? **Yes, at the core level, proven by
test**: `test_ending_stops_transport`
(`components/core/arrangrr/tests/test_arranger.cpp:617-633`) asserts
`!b.e.transport().playing()` and a `kStopped` transport `OutEvent`, from
nothing but `style section ending1` + letting the bars run out.
`Engine::fire_arranger` (`engine.hpp:829-838`) performs the stop
unconditionally for any host, GUI included.

Does the GUI *react* correctly to that stop? **Traced, believed correct, but
NOT covered by a dedicated regression test** — flagged as an open item by the
prior `intra-style-section-differentiation.md` §4.3 item 3 and independently
re-confirmed here by tracing the actual data path:

- `main.cpp:900` (`brain_session.poll(brain_events)`) runs BEFORE
  `render_frame(...)` in the same frame iteration (`main.cpp:882-912`), so any
  `transport` `OutEvent` this bar is applied to `AppState` before the frame
  renders.
- `AppState::apply` (`apps/gui-sonotron/src/app_state.cpp:80-93`): any
  `transport_state` other than `"playing"`/`"paused"` sets
  `m_transport = Transport::kStopped` and zeroes the playhead.
- `render_layout` (`apps/gui-sonotron/src/layout_renderer.cpp:82`) sets
  `fx.playing = state.app_state.transport() == AppState::Transport::kPlaying`
  at the very TOP of the frame's render pass, before any panel (including
  `update_auto_song`) runs.
- `next_scene_to_launch` (`grid_model.cpp:113`) returns `std::nullopt`
  whenever `!playing` — so once `fx.playing` goes false, auto-song stops
  scheduling further advances on the very next frame it is evaluated.

The wiring is real and the ordering is correct by construction — but no test
in `apps/gui-sonotron/tests/` exercises "arm auto-song, land on an
Ending-mapped column, let it play out, assert the GUI itself now reads
stopped and does not re-fire." This is the one item in this whole analysis
that is a genuine code-behavior claim rather than a static read, and it
should be pinned by a real (Torquato/Cennino) regression test before Option A
ships, not asserted as done from headers alone.

## 4. What needs deciding / what I flagged

1. **Option A vs B is the owner's call.** A is cheaper and ships now inside
   the current Repeat-Zone grid model; B is "free" at the core level but
   requires reopening a LOCKED decision (`repeat-zone-real-contract.md` §8b
   item 4) that the owner explicitly closed in the other direction 2026-07-16.
   I will not reopen it myself — flagged, not decided.
2. **The Intro/Fill one-shot race, a secondary correctness risk I found while
   tracing §1.1/§1.4**: if any AUTHORED song sequence places an Intro- or
   Fill-typed column somewhere other than exactly matching its own true
   section length, the Arranger will silently resolve back to `m_return_to`
   (the last variation) *before* the host's own `scene_bars` hold timer says
   to advance (`arranger.hpp:470-471` fires purely off the SECTION's own bar
   count, independent of whatever the host believes the "scene" length is).
   Concretely: a 2-bar `Intro1` held by the host for 8 bars will actually
   revert to `VarA` audibly after bar 2, while the GUI still shows the Intro
   column "active" for 6 more bars. This does not corrupt anything (worst
   case: a column's highlighted state briefly disagrees with the audible
   section) but it means Option A's authored sequence should size each
   entry's hold to the section's REAL bar length (`GridModel::scene_bars`
   already supports per-scene override, `grid_model.cpp:85-97`) — this is an
   authoring-discipline note for whoever builds Option A, not a blocking gap.
3. **No regression test proves the GUI honors an engine-triggered stop after
   an Ending one-shot** (§3, restated as an explicit ask): before shipping
   Option A's "Ending as terminal step," get a test that arms auto-song,
   drives it onto an Ending column, lets the section run out, and asserts
   both the core transport and `fx.playing`/`AppState::transport()` read
   stopped with no further scene advance. This is QA territory
   (Torquato/Cennino), not mine to write.
4. **No new dependency, no new ABI surface is required by Option A** — flagged
   for completeness, not because anyone proposed one.

## Files read for this analysis (for traceability)

- `apps/gui-sonotron/src/grid_panel.cpp` (`update_auto_song`, `seed_demo`,
  `render_scene_header_cell`)
- `apps/gui-sonotron/src/grid_model.cpp`/`.hpp` (`next_scene_to_launch`,
  `scene_bars`, `scene_section`, `section_wire_name`)
- `apps/gui-sonotron/src/app_state.cpp`/`.hpp`, `layout_renderer.cpp`,
  `main.cpp` (transport-stop propagation path)
- `components/core/arrangrr/include/arrangrr/arranger/arranger.hpp` (one-shot
  resolution, `stop_transport`)
- `components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`
  (`section_is_intro`/`section_is_ending`)
- `components/core/arrangrr/include/arrangrr/engine.hpp`
  (`fire_arranger`'s `stop_transport` handling)
- `components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`,
  `components/core/arrangrr/src/engine.cpp` (`cmd_scene` family)
- `components/core/arrangrr/include/arrangrr/perf/performance.hpp`
  (`Performance.variation`)
- `components/core/arrangrr/tests/test_arranger.cpp` (one-shot/ending tests)
- `docs/DESIGN.md` (lines 185-273 model taxonomy incl. `Song[]`/`Scene[]`;
  §10000 Generative Director; §9400/8100 status notes)
- `docs/proposals/repeat-zone-real-contract.md` (§5, §8b — the locked
  SceneChain-vs-grid-column decision)
- `docs/proposals/intra-style-section-differentiation.md` (§4 — Ending
  corpus coverage and the GUI demo-content gap)

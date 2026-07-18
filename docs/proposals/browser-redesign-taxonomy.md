# Browser redesign — taxonomy & hierarchy proposal

Status: PROPOSED 2026-07-18. **PLAN-ONLY.** Read-only on product code; this
document is the only write. Answers the owner-owed deliverable: a taxonomy for
the redesigned left-rail Browser (`apps/gui-sonotron/src/browser_panel.cpp` +
`browser_model.{hpp,cpp}`, `kBrowserW = 210.0F`, `layout_renderer.cpp:32`) that
scales from today's 16 built-in styles to the ~1010-style imported corpus
(DESIGN.md `9400`/`9430`) **and** surfaces the other first-class resource
families the core already exposes but the GUI does not.

---

## 0. Decision summary — the forks the owner must pick

1. **Top-level shape: category selector as a reused COMBO (recommended) vs a
   literal `ImGui::BeginTabBar` strip vs one deep tree vs a sidebar-of-
   sidebars.** I recommend the combo — `render_family_filter_combo`
   (`browser_panel.cpp:297-317`) is *already in the tree*, already proven at
   210px, and needs no new widget pattern. A literal tab bar is the more
   conventional shape but its per-tab label width at 210px/6-8 categories is
   ≈25-30px — too narrow for text (§3). This is a widget-shape call, not just
   a taste call, because it changes what gets built later.
2. **Search scope: per-active-tab (recommended) vs one global cross-family
   search.** Global search returns incomparable result types (a style name
   next to a GM voice name next to a chord-progression name) in one list.
   Per-tab reuses every existing `matches()`/`style_matches_filter` predicate
   unchanged (§3).
3. **Drum kits: fold into the Voices/Sounds tab as a percussion-channel filter
   (recommended) vs cut the family entirely vs keep today's decorative
   `kKits` list.** No `kit`-load ABI verb and no `DrumKit` entity exist
   anywhere in the tree or in DESIGN.md §7's own data taxonomy (verified by
   grep, §1.3) — "kits" today is UI ahead of any backing primitive. This is a
   scope/ambition question (does the product want real multi-sample kit
   swapping, which is new CORE scope, not a GUI task) that only the owner can
   settle.
4. **Controller maps / Routing profiles: keep OUT of the Browser rail
   entirely, destined for a future Settings/Preferences surface (recommended)
   vs include them as a later browsable family.** DESIGN.md §7 already
   classifies `RoutingProfile`/`ControllerMap` as config-tier, not
   performance material — the same reasoning that already put `Project`
   save/load in the File menu instead of the Browser (feature-list.md §2a is
   silent on Projects for exactly this reason). Flagged, not resolved here.

---

## 1. What's actually in the tree today (verified, not recalled)

### 1.1 The current Browser, mechanically

`render_browser_panel` (`browser_panel.cpp:321-357`) renders, in a single
scrolling child under one shared bottom search box: `styles` (7+1
`StyleFamily` tree, gated behind `kFlatListThreshold = 24` — below it, one
flat list; at/above it, family headers + `ImGuiListClipper`, `browser_panel.
cpp:36-46, 158-232`), `variations` (8 fixed drag-only rows, click does
nothing — explicit comment at `browser_panel.cpp:238-241`: *"Clicking a row
does nothing yet... only from the drag"*), `kits · GM` (10 hand-picked names,
*"design-intent, local-only list... no kit-load verb is wired from here"*,
`browser_panel.cpp:51-52`), and `clips` (a static *"(none authored yet)"*).
One search field at the bottom filters all four sections' own predicates
(`browser_panel.cpp:349-356`).

The `StyleFamily` taxonomy and the flat-list threshold are the direct,
already-landed product of task #30 and its correction: commit `f722000`
("scalable style browser") added 8 genre headers + a filter combo; that
regressed the small 16-style corpus (2 of 8 headers, `kBallroomTraditional`/
`kWorldRegional`, are *always* empty for today's built-ins — `browser_model.
hpp:96-113`); commit `1cab1ab` ("gate style-browser genre grouping behind a
flat-list threshold") added `kFlatListThreshold=24` to fall back to the old
flat list below it, keeping the scale machinery intact and dormant above it.
This is the single most load-bearing precedent for this whole proposal: **a
mechanism can be structurally correct for 1010 items and still be a UX
regression at 16** — the redesign must not repeat that mistake at the OUTER
(family) level the way #30 first made it at the INNER (genre) level.

### 1.2 Resource-family inventory, cross-checked against the tree

| Family | Core mechanism | GUI today | Verdict |
|---|---|---|---|
| **Styles** | `styles::kBuiltins` (16), corpus importer `9420`/`9430` (◑ partial, `9420` unmerged pending cherry-pick, `9430`'s SFF2 sub-structure gap — DESIGN.md:1032-1061) | ✅ 7+1 `StyleFamily` tree + flat-fallback, click→`style load/switch` | Done for built-ins; corpus NOT runtime-loadable yet (no bulk `style list-meta` verb, no D44 Layer B blob loader — confirmed still true, `style-browser-corpus-scale.md` §1.5) |
| **Variations/Sections** | `style section <name>` verb (already sent from `grid_panel.cpp`'s scene-header ▶, flow-verification-matrix.md 2c) | ⚠️ drag-only, click is a no-op | Cheapest possible win — verb exists, only the click handler is missing |
| **Voices/GM programs** | `program <port>[:ch] <voice>` (`kProgram=35`, `abi.hpp:160`; `Shell::cmd_program`, `shell_music_commands.cpp:379-404`); full 128-name table + parser round-trip in `components/platform/hostrt/gm_program.{hpp,cpp}` (tested, `test_host.cpp:818-826`) | Not exposed anywhere | Mostly host wiring, but genuinely needs ONE new thing: a destination (port[:channel]) picker, since `program` targets a port, not a global slot |
| **Drum kits** | **None found.** No `kit` token anywhere in `abi.hpp`/`shell*.cpp` (grepped); no `DrumKit` entity in DESIGN.md §7's data taxonomy | ⚠️ `kKits` decorative list, no verb | UI ahead of any core primitive — see decision fork 3 |
| **Chord progressions** | `kSeqUse=20` (`abi.hpp:129`) — index-only selection, no name registry | Not browsed | Same "index-only, no browsable name" state Styles was in before `kBuiltinStyleNames` existed |
| **Songs/scene-chains** | `SceneChain` (node `8100`); Phase 1 (engine foundation) **"must be RE-dispatched (session interrupted, nothing landed)"** — memory `song-mode-scenechain-adoption` | Not browsed | Genuinely not in the tree yet; building UI for it now is fantasy scheduling |
| **Performances** | `kPerformanceStore`/`kPerformanceRecall` (`abi.hpp:278-283`) — index-based, no name registry | Not browsed | Entangled with song-mode's own per-scene Performance model (`song-mode-scenechain-adoption.md` §2, decision 2) |
| **Loops** | `kLoopNew`/`kLoopRecordStart`/… (`abi.hpp:339-373`) — *"the note-level peer of ChordSequencer"* (`abi.hpp:333-337`) | Not browsed | Real core mechanism, zero GUI surface, and a THIRD distinct "loop/clip" concept — see §2 |
| **Samples/WAV** | **None.** No audio-sample-playback engine anywhere in the tree | Nothing | Cut from this redesign entirely — do not design a tab for a resource with no backing implementation at all |
| **Pad banks/FX inserts/Groove presets** | `Pad` exists in §7's taxonomy; groove is a live per-role insert (`kGroove` pinned-last, memory `phase6-theme4-shipped`) — no save/recall-by-name mechanism for any of the three | Not browsed | Needs a "save as preset" primitive before it can be a tab; none exists |
| **Controller maps/Routing profiles** | `RoutingProfile`/`ControllerMap` are config-tier entities in §7, no confirmed runtime implementation | Not browsed | See decision fork 4 — likely never belongs in the Browser at all |
| **Projects** | Perf save/load | ✅ already in the File menu | Correctly excluded already — no change |

---

## 2. A fracture worth naming before any tab is built: three "clip/loop" concepts, one ambiguous placeholder

The tree already has **three separate, real mechanisms** that a naive
redesign would likely collapse into one "Clips" or "Loops" tab, which would
be wrong:

1. **`ClipMatrix`** — the Repeat Zone's per-cell content, already real and
   wired (`grid_panel.cpp:561-573`, `launch clip <id> quantize <n>`,
   flow-verification-matrix.md 2c).
2. **`LoopBuffer`** — a distinct core sequencer-level recorder ("the
   note-level peer of ChordSequencer", `abi.hpp:333-337`), with its own
   register/record/erase/undo verbs, entirely unwired to the GUI.
3. **The Browser's own placeholder `clips` section** — `BrowserModel`'s
   comment describes it as *"user-authored material... there is no
   recorder/authoring UI and no clip primitive on the wire"* (`browser_model.
   hpp:121-125`) — i.e., it was written to describe a FOURTH, not-yet-built
   authoring concept, not either of the two real mechanisms above.

Building a "Clips" or "Loops" tab without first deciding which of these three
it exposes — or whether it's a fourth, still-unbuilt thing — would perpetuate
exactly the kind of silent conflation this proposal exists to prevent. This
is flagged in the wire-later staging below (§4), not resolved here.

---

## 3. Top-level structure: tabs vs the alternatives

**Against a single deep tree (status quo before #30's gate, and what a naive
"add more sections" redesign would produce):** already falsified at n=16 by
one family's own history (§1.1). Extrapolating (not measured, since no build
today has 6-8 resource-family tree siblings to benchmark) — stacking Styles,
Sections, Voices, Songs, Performances, Loops, Chords, and Pad/FX/Groove as
permanent SIBLING branches in the same 210px column compounds the exact
failure #30 already produced once: some outer branches will be permanently
empty for long stretches (Songs before song-mode Phase 1 ships, Drum-kits
until fork 3 resolves) the same way `kBallroomTraditional`/`kWorldRegional`
sat empty for the built-in 16 — and `1cab1ab`'s own fix (§1.1) was written
specifically to stop rendering empty buckets at all. A permanent, un-gated
outer tree has no equivalent per-family "hide if empty/small" escape hatch
the way the inner Styles tree now does.

**Against flat+global-filter (no grouping, one giant searchable list across
everything):** loses the "browse without knowing the name" affordance that
both cited precedents (Yamaha Music Finder's BEAT/GENRE/TEMPO fields, Korg
Pa5X's User/Favorite banks — both already researched in
`style-browser-corpus-scale.md` §3.1) rely on, and merges incomparable
result types (a style name, a GM voice name, a chord-progression name) into
one list a player has to mentally re-sort by kind.

**Against a sidebar-of-sidebars (a second narrow icon strip beside the main
list):** the column is already only 210px (`kBrowserW`, `layout_renderer.
cpp:32`) — there is no width budget left for a second vertical strip once
the primary list needs its own room; whatever solves category-switching must
do it in one horizontal line, not a second column.

**Recommended: category selector (one active family at a time) + a focused
list/tree for that family + one persistent bottom search box scoped to the
active family.** The concrete widget question — literal `ImGui::BeginTabBar`
vs reusing the existing `render_family_filter_combo` idiom
(`browser_panel.cpp:297-317`) as the outer selector — is decision fork 1.
The combo already fits 210px today (it renders the family-filter dropdown
for the Styles tab's OWN sub-taxonomy); a literal tab strip with 6-8 text
labels at ≈25-30px each will not, without icon assets that don't exist yet.
Reusing the combo costs zero new pattern; it is the evidence-based
recommendation, not merely the cheaper one.

**Search scope.** Recommend per-active-tab: the existing `matches()` (used
today for `kits`/`variations`) and `BrowserModel::style_matches_filter`
predicates are already scoped per-list; keeping search scoped to whichever
category is active reuses both unchanged. A global cross-family search (fork
2) is a legitimate later addition but should not be the Phase-1 default —
it's new merge/ranking logic across heterogeneous entity types that nothing
in the tree does today.

---

## 4. Staged sequence: wire-now / next / defer / cut

**Phase 0 — already shipped, no action.** Styles as today (7+1 `StyleFamily`
tree + `kFlatListThreshold`, task #30 + `1cab1ab`) becomes the content of the
Styles category, unchanged. Its internal scale logic is untouched by moving
it under a category selector — see §5 on why these are orthogonal axes.

**Phase 1 — SHIPPABLE, HOST-ONLY, do next (no dependency, no core work):**
- **Sections/Variations: finish click→apply.** Wire a plain click on a
  variation leaf to send `style section <name>` directly — the verb already
  exists and is already sent from `grid_panel.cpp`'s scene-header ▶
  (flow-verification-matrix.md 2c). Keep the existing drag-to-scene-header
  behavior alongside it (repeat-zone-real-contract.md SLICE 4a) — click
  targets "apply now", drag targets a specific grid column; not mutually
  exclusive. The single cheapest, most isolated win available: no new core
  verb, no new data model, one click handler.
- **Voices/Sounds tab.** Hand-copy the 128 GM names already in `gm_program.
  cpp`'s `kGmNames` into the browser model — the SAME D38-compliant
  hand-copied-literal discipline `kBuiltinStyleNames` already uses (`browser_
  model.hpp:20-26`'s own comment explains why: the GUI never `#include`s the
  core). Click sends `program <port>[:ch] <voice>`. Genuinely new surface
  needed: a destination picker (defaulting to whichever part/port is
  currently focused, same resolution style the M/S latches already use
  per-row), since `program` targets a port, not an implicit global slot. Also
  note honestly: there is **no per-part program readback on the wire**
  (`parts_model.hpp:24-25`: *"gm_program stays -1... no per-part program
  readback exists on the wire today"*) — a Voices tab can send but cannot
  wire-confirm which voice is currently loaded per part; track a client-side
  "last sent" echo (the same class of local-only bookkeeping `fx.active_style`
  already is for Styles), never presented as wire-confirmed.

**Phase 2 — SHIPPABLE, HOST-ONLY, after Phase 1 settles:**
- **Drum kits.** Resolve fork 3 first. Default recommendation: fold into the
  Voices tab as a percussion-channel filter/sub-view once Phase 1(b)'s
  `program` send is proven, retiring the standalone decorative `kKits` list.
  If the owner wants real multi-sample kit-swapping, that is new CORE scope
  (a `DrumKit` primitive that doesn't exist today), not a Browser redesign
  task — do not build a bigger kits tab against a mechanism that isn't there.

**Phase 3 — NEEDS-DECISION, blocked on other workstreams, do not start
Browser UI for these yet:**
- **Songs/scene-chains.** Blocked on song-mode Phase 1 (engine foundation)
  actually landing (currently "must be RE-dispatched, nothing landed" per
  memory). Earliest sane start for a browsable "Song" is after song-mode
  Phase 2 (`ScenePerformance` per-scene record + editor UI,
  `song-mode-scenechain-adoption.md` §"Phasing") — that is the point a Song
  becomes a named, save/recall object distinct from "whatever's currently on
  the grid," not before.
- **Performances.** Entangled with the same SceneChain work (song-mode's own
  "per-scene Performance = base capture + per-scene overrides" design,
  §2 decision 2) — defer alongside Songs.
- **Chord progressions, Loops, Pad/FX/Groove presets.** Each has a real core
  mechanism (`kSeqUse`, `kLoopNew` family, `Pad`/groove-as-insert) but no
  name/metadata registry — the same "index-only" gap Styles had before
  `kBuiltinStyleNames` existed. Each needs its own small host-side naming
  registry before it can be a tab; none is scheduled. Sequence these after
  Voices/Sections prove the category-selector pattern in real use; owner
  ranks the order (no usage signal exists yet to rank them from evidence).
- **Controller maps/Routing profiles.** Per fork 4, recommend these never
  enter the Browser rail — config-tier per DESIGN.md §7, destined for a
  future Settings surface, mirroring the Projects/File-menu precedent
  already in place.

**Phase 4 — CUT, not staged, not planned:**
- **Samples/WAV.** No audio-sample-playback engine, no data-taxonomy stub,
  nothing in the tree at all. Do not design a tab for a resource with zero
  backing implementation; revisit only if/when a sample engine actually
  lands.

---

## 5. How the 1010-style scale case coexists with small-corpus readability everywhere else

Two different axes answer two different problems, and neither substitutes
for the other:

- **Outer axis (which family is active)** — solved by the category selector
  (§3), which is O(number of families), not O(items within a family). It
  stays cheap and readable regardless of how large any one family's internal
  list grows, by construction.
- **Inner axis (how big is THIS family's own list right now)** — solved,
  for Styles specifically, by the already-shipped `kFlatListThreshold`
  mechanism (§1.1), which is untouched by moving Styles under a category
  selector. A design that added tabs but left Styles ungated would still
  regress at 16 items (already proven); a design that kept the threshold but
  crammed 8 resource families into one un-gated outer tree would still
  clutter at the outer level (the risk named, not yet measured, in §3).

Every OTHER family in Phase 1/2 (Sections: 8 fixed items; Voices: 128 fixed
items) is small enough on its own that none needs an inner threshold
mechanism of its own yet — Styles remains the only family with genuine
corpus-scale (1010) exposure, and it already owns the one mechanism built to
survive that.

---

## 6. What I flagged / what the owner decides

- Fork 1 (top-level widget shape), fork 2 (search scope), fork 3 (drum kits),
  fork 4 (controller maps/routing) — §0, restated with evidence in §3/§4.
- The three-way clip/loop conflation (§2) must be resolved — which mechanism
  any future "Clips"/"Loops" tab actually exposes — before that tab is built,
  not decided implicitly by whoever implements it first.
- No new host or core dependency is implied by anything in this proposal;
  nothing to flag on that front. Every mechanism cited (`ImGuiListClipper`,
  the combo idiom, the hand-copied-literal discipline, the existing verbs)
  is already vendored or already in the tree.
- Everything in Phase 1/2 is HOST-ONLY GUI work over verbs that already
  exist on the core side; nothing in this proposal asks the on-device
  (arm-none-eabi) regime for capacity it doesn't have — the Browser is a
  host-only client panel under D38, full stop.
- I did not use AskUserQuestion: all four forks are stated with their
  recommendation and evidence above, sharpened enough for a fast owner
  read; none required blocking on a live clarifying round given the context
  budget, but all four remain the owner's call to make, not mine.

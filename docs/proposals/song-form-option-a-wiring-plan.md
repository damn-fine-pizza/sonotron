# Song-form Option A: concrete wiring plan (task #12/#27/#35)

Status: proposal / read-only analysis. No product code touched by this
document's author. Builds directly on the prior analysis in
`docs/proposals/song-form-autoarrange.md` (§1-§4) — that document answers
"does a song-form concept exist" and sketches three options (A/B/C); THIS
document re-confirms the state it found still holds today, then turns its
Option A into a file-by-file, line-cited plan Nazzareno can execute without
re-deriving the design, and (§3, added for task #35) documents the
established cross-vendor arranger convention for Stop/Ending and matches
sonotron's own Stop→cue-Ending gesture to it.

## 1. Confirmed current state (re-verified against the tree, not recalled)

The prior doc (`song-form-autoarrange.md`, written 2026-07-17 10:44, commit
`f749072`) found three facts. All three were independently re-read against
`HEAD` (`d274930`, 2026-07-17 13:44) for this document — one host commit
(`9e0ecf0`, "single-trigger scene launch... style-length auto-song advance")
landed on `grid_panel.cpp`/`grid_model.*` AFTER the prior doc, so this was not
a safe assumption; re-reading confirms nothing that matters here drifted:

1. **The core `Arranger` has a real, tested, TYPE-based one-shot rule, not an
   authored song.** `Arranger::on_tick`
   (`components/core/arrangrr/include/arrangrr/arranger/arranger.hpp:469-474`):
   ```
   } else if (section_end) {
     if (section_is_fill(m_current) || section_is_intro(m_current)) {
       next = m_return_to;
     } else if (section_is_ending(m_current)) {
       result.stop_transport = true;
       return result;
   ```
   `section_is_ending(t)` is `t >= SectionType::kEnding1`
   (`style_model.hpp:44`) — an ordinal check, not a chain. Proven live by
   `test_ending_stops_transport`
   (`components/core/arrangrr/tests/test_arranger.cpp:617-633`): sending only
   `style section ending1` and letting the bars run out asserts
   `!b.e.transport().playing()` plus a `kStopped` `OutEvent` — re-read in full,
   unchanged. `Engine::fire_arranger` performs this stop unconditionally for
   any host (`engine.hpp:829-838`).

2. **`SceneChain` (node 8100) is a complete, linear, non-looping core
   primitive — and it is explicitly locked OUT of the GUI's auto-song.**
   `components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp:110-127`
   (`on_bar`): `m_index + 1 >= m_steps.size()` sets `m_playing = false` and
   holds — "no implicit loop" is the file's own header claim
   (`scene_chain.hpp:33-36`) and the code matches it. Wired end to end
   (`Engine::cmd_scene`, `engine.cpp:948-1021`; ABI verbs `kSceneAdd/kScenePlay/
   kSceneStop/kSceneClear`, `abi.hpp:386-405`) but grepping
   `apps/gui-sonotron/src/*.cpp` for those four verbs today returns nothing —
   still unused, still locked by `docs/proposals/repeat-zone-real-contract.md`
   §5/§8b. Not reopened by this plan (see §4.1).

3. **The GUI's own auto-song is pure, unconditional round-robin — it WRAPS.**
   Re-read `apps/gui-sonotron/src/grid_model.cpp:110-124`
   (`next_scene_to_launch`), current text:
   ```cpp
   const int normalized = ((active_scene % scene_count) + scene_count) % scene_count;
   return (normalized + 1) % scene_count;
   ```
   No terminal case exists: reaching the last column always wraps to `0`.
   This is independently pinned by a still-live unit test that asserts the
   wrap as CORRECT behavior —
   `apps/gui-sonotron/tests/test_grid_model.cpp:217-225`
   (`test_next_scene_wraps_at_last_scene`, `active_scene=4, scene_count=5` →
   `CHECK(*next == 0)`). This test will need to become the test that PROVES
   Option A (§2.1 below) — it is the single ground-truth regression for the
   exact behavior this task changes.
   `render_header`'s "next (intent)" hint
   (`apps/gui-sonotron/src/grid_panel.cpp:464`) hand-rolls the SAME wrap
   formula a second time: `(active_scene_index + 1) % scene_count` — a second
   site that must change in lockstep or the displayed hint will lie once the
   mechanism itself stops wrapping (§2.2).

4. **Endings ARE already reachable from the GUI today, by hand, but are not
   in the shipped demo content.** `apps/gui-sonotron/src/browser_panel.cpp:
   26-44`: the "variations" drag palette's `outro` row carries
   `kVariationSections[7] = 11` (`kEnding1`'s numeric value,
   `style_model.hpp:32`) — dragging it onto a scene header calls
   `GridModel::set_scene_section`, a real, already-wired path. But
   `seed_demo`'s `kDemoSections`
   (`apps/gui-sonotron/src/grid_panel.cpp:213-219`) seeds only 5 columns —
   Intro1, VarA, VarB, VarC, VarD — no Ending, and `main.cpp:815` constructs
   `GridModel grid_model(5)`, exactly matching. **Confirmed for completeness
   (owner roadmap item d): every one of the 16 built-in styles authors a real,
   non-zero-length `kEnding1` section** (`grep -c "SectionType::kEnding1"`
   across `components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp`:
   all 16 return ≥1; `basic.hpp:366-367` e.g. `{.type=SectionType::kEnding1,
   .bars=1, ...}`), so Option A's terminal step is content-ready for every
   style today, not just a demo special case.

5. **The GUI's reaction to an engine-triggered stop is real and correctly
   ordered, but still has no dedicated regression test** — re-traced and
   unchanged from the prior doc: `AppState::apply`
   (`apps/gui-sonotron/src/app_state.cpp:80-93`, re-read) maps any
   `transport_state` other than `"playing"`/`"paused"` to `Transport::
   kStopped`; `layout_renderer.cpp:81` sets `fx.playing` from that BEFORE any
   panel runs; `next_scene_to_launch` already refuses to advance whenever
   `!playing`. The wiring is sound by construction; there is still no test
   that arms auto-song, drives it onto an Ending column, and asserts the GUI
   itself reads stopped afterward (§2.7).

**Net: nothing has drifted. The only thing standing between today's tree and
Option A is the WRAP itself — one pure function and one duplicate formula —
plus, optionally, giving the shipped demo an Ending column to make the new
behavior audible without the user hand-authoring one.**

## 2. Option A: concrete plan

### 2.1 The mechanism change (the actual "wiring")

**File:** `apps/gui-sonotron/src/grid_model.cpp`, function `next_scene_to_launch`
(currently lines 110-124).

Change the terminal case from wrap to hold:

```cpp
// ILLUSTRATIVE, not a patch.
std::optional<int> next_scene_to_launch(bool auto_song, bool playing, int active_scene,
                                        int scene_count, int bars_elapsed_in_scene,
                                        int active_scene_section_bars) {
  if (!auto_song || !playing || scene_count <= 0) {
    return std::nullopt;
  }
  if (bars_elapsed_in_scene < active_scene_section_bars) {
    return std::nullopt;
  }
  const int normalized = ((active_scene % scene_count) + scene_count) % scene_count;
  if (normalized + 1 >= scene_count) {
    return std::nullopt;  // last column: hold, do not wrap (song-form Option A)
  }
  return normalized + 1;
}
```

This is the WHOLE functional change to the advance decision. Everything
downstream already handles `nullopt` correctly with zero further edits:
`update_auto_song` (`grid_panel.cpp:678-683`) already does
`if (!next.has_value()) { return; }` — it was already written to treat "no
next" as "do nothing," because that was needed for `auto_song==false`/
`!playing`/`scene_count<=0`. The last-column case simply becomes a fourth,
musically meaningful reason for the same `nullopt`, not a new branch anywhere
else in `grid_panel.cpp`.

**Companion doc-comment fix (house discipline in this file — every function
here carries a paragraph explaining its own contract):** the block comment
above `next_scene_to_launch` (`grid_model.hpp:172-193`) currently states "the
song WRAPS around the scene sequence rather than stopping at the last
column" — this sentence must flip to describe the new non-wrapping,
hold-at-last-column contract, or the header will contradict its own function
the moment this ships.

**Test that must be rewritten (this IS the regression pinning the new
behavior):** `apps/gui-sonotron/tests/test_grid_model.cpp:217-225`,
`test_next_scene_wraps_at_last_scene`. Rename to
`test_next_scene_holds_at_last_scene` (or similar) and flip the assertion:

```cpp
// ILLUSTRATIVE.
void test_next_scene_holds_at_last_scene() {
  // Song-form Option A: the last scene column does NOT wrap back to 0 --
  // it holds (nullopt), matching SceneChain::on_bar's own "last step holds"
  // precedent (scene_chain.hpp:121-124).
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/4,
                              /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                              /*active_scene_section_bars=*/1)
             .has_value());
}
```

Every OTHER existing case in that test file (`auto_song` off, not playing,
mid-scene, advances-at-boundary, advances-past-boundary, zero-scene-count) is
untouched — none of them exercise the last-column branch, so none of them
change meaning.

### 2.2 The second wrap site (must move in lockstep)

**File:** `apps/gui-sonotron/src/grid_panel.cpp`, `render_header`
(currently lines 463-470):

```cpp
if (fx.playing && fx.auto_song && scene_count > 0) {
  const std::size_t next_scene_index = (active_scene_index + 1) % scene_count;
  ImGui::TextColored(theme::kTextDim, "  next (intent): %d: %s", ...);
} else {
  ImGui::Dummy(ImVec2(1.0F, placeholder_row_h));
}
```

This hand-rolls the SAME wrap formula `next_scene_to_launch` used to encode —
it is display-only, but it must agree with the real mechanism or the UI shows
"next: 1: Intro" while the engine is actually about to hold and stop. Fix:
reuse `next_scene_to_launch` itself here (it already needs `bars_elapsed_in_
scene`/`active_scene_section_bars`, both of which `render_header`'s caller,
`render_grid_panel`, already computes for `update_auto_song` — check whether
they're in scope at this call site or need threading through as extra
parameters), and fall back to the placeholder `Dummy` row (same "reserve the
row's height either way" discipline the function already uses for the row
above it, per its own multi-paragraph comment about the `test_repeat_zone_
playhead_ui_automation.cpp` layout-shift regression) when the answer is
"no next" — i.e. show nothing/an explicit "— end —" rather than a wrong
number. This is a small, single-file, no-ABI change; the exact plumbing
(pass the already-computed value in vs. recomputing) is Nazzareno's call at
implementation time, not a design fork.

### 2.3 Content: give the shipped demo an audible Ending (recommended, separable)

Without this, Option A's new behavior is real but INVISIBLE in the
out-of-the-box demo (5 columns, all Intro/VarA-D, no Ending — the song would
simply hold on VarD forever instead of looping, never actually stopping,
because nothing in the demo is Ending-typed). Two small, independent changes,
both host-only, both already-precedented paths:

- `apps/gui-sonotron/main.cpp:815`: `GridModel grid_model(5)` → `GridModel
  grid_model(6)` (within `kMaxSceneCount == 8`, no structural change).
- `apps/gui-sonotron/src/grid_panel.cpp`, `seed_demo`'s `kDemoSections`
  (lines 213-219): add a 6th entry, e.g. `{5, preview::Section::kEnding1,
  "Ending"}` — numerically and structurally identical to the other 5 entries,
  reusing `preview::Section::kEnding1` (`preview.hpp:67`) exactly as
  `browser_panel.cpp`'s own `outro` row already does for hand-authored
  columns (§1 item 4).

**Flagged as separable, not required for the mechanism to be correct**: §2.1
alone is a complete, correct, testable fix to the advance decision. §2.3 is
what makes the shipped build actually demonstrate "plays through, ends on the
Ending, stops" without the user dragging one in first. Recommend both ship
together in the same task, but they are two independent diffs and could be
split if Nazzareno/the owner wants the mechanism reviewed alone first.

### 2.4 How the user arms/selects Option A — an OPEN FORK, not resolved here

The task asks for a plan for "how the user arms/selects it." Two genuinely
different answers, and the code does not tell me which the owner wants:

- **Fork 1 (recommended): `auto_song` itself simply stops wrapping.** The
  existing header toggle (`"auto-song"`/`"song"`, `grid_panel.cpp:379`)
  keeps its exact current meaning and UI; its behavior changes from "loop the
  scene sequence forever" to "play the authored sequence once, hold (and, if
  the last column is an Ending, stop) at the end" — i.e. exactly §2.1's
  change, no new UI element. Cheapest, matches the task's literal ask
  ("wire song-form Option A"), and mirrors `SceneChain`'s own philosophy
  (§1 item 2) of "the chain is linear by default, looping is a SEPARATE
  host-level gesture" — meaning a user who genuinely wants the OLD infinite
  round-robin loses it outright unless a later "loop" affordance is added.
- **Fork 2: a NEW, separate flag** (e.g. `V02State::song_mode`, alongside but
  distinct from `auto_song`) so the header offers a THIRD state — off / loop
  (today's wrap) / song (Option A) — preserving the current infinite-loop
  behavior as an explicit user choice rather than removing it. More UI
  surface (a new toggle or a 3-state cycle on the same button), more
  `V02State` bookkeeping, but no regression for anyone relying on today's
  "just keeps cycling" behavior for a live-jamming use case.

I am not deciding between these — it is a genuine product-UX fork, not
something derivable from the code (both are equally cheap to build; the
question is what USER EXPERIENCE the owner wants for the common "no Ending
authored" case: hold-forever-in-place, or keep-cycling). Flagged in §4.

### 2.5 Composition with per-scene K-repeats (owner task #6, not yet built)

`GridModel::scene_bars`/`set_scene_bars` already exist and are explicitly
"reserved for a future extension... a per-scene REPEAT COUNT" per their own
header comment (`grid_model.hpp:143-146`, citing the `auto-song-playhead-
and-repeats` memory). Option A composes with this cleanly and needs NO change
of its own to do so: `next_scene_to_launch`'s new "hold at last column" check
is orthogonal to whatever gate decides bars_elapsed_in_scene has crossed the
threshold (today `section_bars * kDefaultSectionRepeats`,
`grid_panel.cpp:656-663`; tomorrow, presumably, a per-scene `K` from task #6).
Whichever repeat-count mechanism task #6 lands, it only changes WHEN
`next_scene_to_launch` gets called with "boundary reached" for a given
column — it never touches whether that column is the LAST one, which is
Option A's entire addition. No fork here, no rework needed later.

### 2.6 Composition with the existing per-repeat gate

Also worth stating explicitly, since it affects what the ending actually
sounds like: `update_auto_song`'s own threshold
(`active_section_bars = section_bars * kDefaultSectionRepeats`, currently
`kDefaultSectionRepeats == 2`) governs when the HOST tries to advance past
the Ending column — but with §2.1 that attempt now resolves to `nullopt`
regardless, so it is moot for the terminal column. What actually stops
playback is the CORE's own one-shot rule (§1 item 1), which fires after the
Ending section's OWN authored bar length elapses ONE time (`section_end`,
not `kDefaultSectionRepeats` times) — e.g. `basic`'s `kEnding1` is 1 bar
(`basic.hpp:366`), so the ending plays once, for 1 bar, and stops, regardless
of the host's own 2x-repeat convention for ordinary variations. This is
already the tested, correct, and — I believe — musically right behavior
(an ending is a one-shot by genre convention, not a loop); flagged here only
so it is not mistaken for a bug when the Ending audibly stops sooner than a
casual reading of `kDefaultSectionRepeats` might suggest.

### 2.7 Acceptance criteria for the eventual test pass (QA territory, not mine)

Handed to Torquato/Cennino, not written here:

1. `test_grid_model.cpp`: the rewritten §2.1 unit test (hold, not wrap) plus
   every pre-existing case in that file still green (none of them touch the
   changed branch).
2. A NEW functional/UI-automation test (the still-open item from the prior
   doc's §3/§4 item 3, now made concrete): arm `auto_song`, drive the demo
   grid through Intro→A→B→C→D→Ending (or a small authored fixture, if §2.3 is
   deferred), let the Ending's own bars run out, and assert BOTH (a) the core
   `Engine::transport().playing()` is false and a `kStopped` `OutEvent` was
   seen, mirroring `test_ending_stops_transport`'s own assertions but through
   the GUI's real command path, and (b) `AppState::transport()` /
   `fx.playing` read stopped afterward, and (c) no further `activate_scene_
   column` call fires (no scene-launch verb sent) after the hold.
3. If §2.2's UI-text fix ships, a light assertion that the "next (intent)"
   row shows the placeholder/absent state on the last column rather than a
   wrapped-to-1 number.

## 3. Task #35 — researched convention: Stop → cue-Ending

The owner's directive was explicit: "how do others do it? let's not
reinvent this." Before designing our own Stop→cue-Ending gesture, this
section documents the ESTABLISHED cross-vendor convention (researched, not
recalled) and then matches our design to it — flagging, not guessing, the
one place sonotron's UI genuinely differs from every instrument surveyed
(a single Stop control where they have two).

### 3.1 The established convention, cross-vendor

| Control | Yamaha (PSR/Genos/Tyros) | Korg (Pa-series) | Quantization |
|---|---|---|---|
| **STOP** (a dedicated button, separate from Ending) | "causes the rhythm/accompaniment to stop playing immediately" — a hard, unconditional cut [Yamaha PSR forum/FAQ synthesis] | "simply press the START/STOP button to stop the Style cold" [Korg Pa Getting Started Guide] | none — immediate |
| **ENDING** (1/2/3, a SEPARATE dedicated button) | "Press the [ENDING] button if you want to go to the ending section and then stop. The ending section will begin from the top of the next measure." — cued, bar-quantized, plays through, THEN the style stops automatically [Yamaha PSR-330 Owner's Manual, manualslib.com, direct quote] | "Press one of the ENDING buttons to have the Style play a musical ending" — a distinct control from STOP, same cue-then-stop shape [Korg Pa Getting Started Guide] | next measure/bar boundary |
| **Second press of ENDING while it plays** | triggers a **ritardando** (gradual tempo slowdown into the stop), NOT a skip/hard-stop escape — "you can have the ending gradually slow down (rit.) by pressing the [ENDING] button again during ending playback" [Yamaha PSR manual synthesis] | not found documented; Ending 1/2/3 are alternate MUSICAL endings selected by pressing a DIFFERENT numbered button, not repeated presses of the same one | n/a |
| Ordinary section changes (Intro/Main A-D/Fill/Break) | cued at the next bar/measure, same quantization discipline as Ending | same shape (Style Element buttons: INTRO, VARIATION, AUTO FILL, BREAK, ENDING) [Korg Pa1000/Pa5X manuals] | next measure/bar boundary |
| Multiple Ending variations | Ending 1/2/3 (rit. on 2nd press of the SAME button) | Ending 1/2/3, same naming | — |

**The one finding I looked for specifically and did NOT find**: a documented
"double-press = skip the ending, hard-stop immediately" escape hatch on
either vendor. I searched directly for it and every source that describes a
second ENDING press describes ritardando, never a bypass. The real escape
hatch on both vendors is not a second press of the same control at all — it
is the SEPARATE, always-present STOP button, a different physical control
from ENDING. Reporting this honestly rather than inventing a convention that
isn't there: **the "double-press hard-stop" idea has no cross-vendor
precedent**; if sonotron wants one, it would be a deliberate, labeled
DEVIATION, not a matched convention (see the fork in §3.4).

### 3.2 What this means for sonotron, structurally

sonotron has exactly ONE physical "Stop" affordance today
(`apps/gui-sonotron/src/transport_panel.cpp:64-66`, the "■" pad button,
unconditionally sending `brain_session.send("transport stop")` — the wire
verb that maps to `Engine::cmd_transport`'s `Param::kTransportStop` case,
`components/core/arrangrr/src/engine.cpp:171-181`: an immediate
`m_transport.stop()`, chord release, MIDI Stop, and a `kStopped`
`OutEvent`, with NO cueing of any kind). There is no separate "Ending"
button in the transport rail at all — Ending is reachable today only by
dragging the "outro" row from the variations palette onto a scene header
(`browser_panel.cpp:26-44`, §1 item 4). **The owner's ask — "pressing Stop
should cue the Ending, not stop cold" — maps our single Stop control onto
the INDUSTRY'S Ending gesture, not the industry's Stop gesture.** That is a
deliberate, informed choice to fold two vendor controls into one, not a
misunderstanding of the convention — but it is worth stating plainly because
it is the reason §3.4 below is a real, not rhetorical, fork.

### 3.3 The mechanism costs ZERO new core/ABI surface — already proven by existing code

This is the best news in this whole document: sonotron's core ALREADY
implements the exact two halves of the Yamaha/Korg Ending gesture, both
already tested, neither touched by this proposal:

1. **Bar-quantized cueing, for free, already the default while playing.**
   `Engine::cmd_style`'s `kStyleSection` case
   (`components/core/arrangrr/src/engine.cpp:635-641`):
   ```cpp
   case Param::kStyleSection:
     if (cmd.a < 0 || cmd.a >= kSectionTypeCount ||
         !m_arranger.request(static_cast<SectionType>(cmd.a), !m_transport.playing())) {
   ```
   `immediate = !m_transport.playing()` — while the transport IS playing
   (the only case that matters for a Stop press), `immediate` is `false`,
   so `Arranger::request` (`arranger.hpp:302-318`) parks the switch in
   `m_pending` and it lands at the NEXT bar boundary
   (`arranger.hpp:441-468`'s `m_pending_valid` branch) — this is, structurally,
   the exact same "begins from the top of the next measure" contract the
   Yamaha manual states for its Ending button, already implemented, already
   used by every existing scene-header cue in `grid_panel.cpp`
   (`activate_scene_column`'s own `style section <name>` send).
2. **Plays through, then stops automatically, for free, already tested.**
   `Arranger::on_tick`'s `section_is_ending` branch
   (`arranger.hpp:472-474`) plus `Engine::fire_arranger`'s unconditional stop
   handling (`engine.hpp:829-838`) — the SAME mechanism §1 item 1 and Option
   A's automatic (non-wrapping) ending already rely on, proven by
   `test_ending_stops_transport` (`test_arranger.cpp:617-633`).

**Conclusion: the entire Stop→cue-Ending gesture is a HOST-ONLY change.**
Nothing in `arrangrr/` needs to move. The fix is: the "stop" pad's button
handler (`transport_panel.cpp:64-66`) sends `style section ending1`
(or `ending2`, see §3.4) INSTEAD OF `transport stop` whenever the transport
is currently playing — reusing the exact wire verb and exact quantization
`grid_panel.cpp` already sends for every ordinary scene cue, sent through
`brain_session.send(...)`, no new `Param`, no new `OutEvent`, no new ABI
entry. When the transport is NOT playing, Stop should keep sending
`transport stop` verbatim (there is nothing to cue-and-play-out from a
stopped state — `!m_transport.playing()` would make the request `immediate`
anyway per the same line quoted above, so it would not even cue).

**One necessary precision, already found and cited in §1's own
`activate_scene_column` comment**: send `style section ending1` ALONE, never
paired with `launch scene <n> quantize <q>` the way `activate_scene_column`
does for an ordinary scene launch. `grid_panel.cpp:494-508`'s own comment
proves `launch scene` fans out to `Arranger::request(section,
/*immediate=*/true)` when the column holds registered clips — i.e. it can
force an IMMEDIATE (non-quantized) switch, which would defeat the entire
"next measure" convention this gesture exists to match. The Stop→cue-Ending
handler must be a new, narrower call — NOT a reuse of `activate_scene_
column` — that sends only the bare `style section` verb.

### 3.4 Open forks (flagged, not decided)

1. **Ending1 vs Ending2.** Real instruments expose 2-3 alternate endings as a
   genuine musical CHOICE (different buttons, user decides which ending to
   play). A single repurposed Stop control has no room for that choice at
   press-time. Recommend defaulting to `kEnding1` — it is the one section
   EVERY built-in style authors (§1 item 4: all 16 define `kEnding1`; only
   some also define `kEnding2`), so it is the only choice that behaves
   identically across the whole corpus. This is a product default, not
   something the code dictates — flagged for owner confirmation, not
   assumed.
2. **The "already cueing, want it NOW instead" escape is a genuine sonotron
   invention, not a matched convention (§3.1's negative finding).** Since
   sonotron folds two vendor controls into one (§3.2), a user who presses
   Stop, then changes their mind and wants an IMMEDIATE cut, has no
   precedented gesture to fall back on — neither vendor's "second press"
   means "skip," and neither vendor needs this escape because they always
   have a separate, always-available hard-STOP button standing right next to
   Ending. Two honest options, both explicitly labeled as OUR OWN choice, not
   an industry pattern:
   - **(a) No escape**: once Stop is pressed while playing, the ending plays
     out (typically 1-2 bars per style, §1 item 4) and there is no faster
     path — matches the SPIRIT of "Stop now behaves like Ending" literally,
     accepts a short, bounded, musically-safe delay.
   - **(b) A second Stop press while the ending is cued/playing escalates to
     the OLD immediate `transport stop`** — cheap to build (a single `bool`
     latch mirroring `fx.master_play_launched`'s own once-per-press-cycle
     shape, `v02_state.hpp:123-129`) but should be labeled in the UI/docs as
     a sonotron-specific addition, since §3.1 found no vendor that does this.
   I recommend (a) for a first ship (matches the researched convention
   exactly, zero extra state) with (b) as a fast, low-risk follow-up if the
   owner finds the no-escape version too rigid in practice — but this is the
   owner's call, not mine.
3. **Composition with Option A (§2) — manual cue and automatic end-on-Ending
   are the SAME rail, not two mechanisms.** Both this gesture and Option A's
   automatic non-wrapping advance ultimately do the same two things: send a
   plain `style section ending1` (or whatever Ending the authored last
   column carries) while playing, and rely on the SAME core one-shot rule
   (§1 item 1 / §3.3 item 2) to stop the transport once that section's bars
   run out. Option A reaches the Ending AUTOMATICALLY (running out of
   authored columns, §2.1); task #35's gesture reaches it MANUALLY (a user
   decides to end the song early, e.g. mid-`VarB`). Neither needs to know
   about the other: Option A's `next_scene_to_launch` hold-at-last-column
   fix and task #35's Stop-button handler are two independent CALLERS of the
   identical `style section ending<N>` verb + the identical core stop rule.
   The only shared state worth a shared name is `fx.active_scene`/`fx.
   active_scene_start_bar` bookkeeping (`v02_state.hpp:96-121`) — a manual
   Stop-cue should update it exactly like `activate_scene_column` already
   does for every other section change, so the "engine: %s" / "next
   (intent)" readouts (`render_header`, §2.2) do not go stale the moment the
   user manually ends the song.

## 4. What I flagged, not decided

1. **§2.4 (Fork 1 vs Fork 2) is the owner's call** — whether `auto_song`
   itself becomes non-wrapping (my recommendation, cheapest, matches the
   task's literal wording) or a new distinct "song mode" toggle is added
   alongside the preserved infinite-loop behavior. I did not pick for the
   owner.
2. **§2.3 (demo content) is recommended but separable** — the mechanism
   (§2.1/§2.2) is complete and testable without it; whether to also update
   the shipped demo's column count/content is a small, independent product
   call, not a blocking dependency.
3. **No new core/ABI change, no new dependency** is required by anything in
   this plan, INCLUDING task #35's Stop→cue-Ending gesture (§3.3) — flagged
   for completeness, matching the prior doc's own conclusion; `SceneChain`/
   Option B stays untouched and still locked (`repeat-zone-real-contract.md`
   §8b), not reopened here.
4. **The §2.7 GUI-side stop regression test is a real, still-open gap** — the
   wiring is traced and believed correct (re-verified independently in §1
   item 5) but has never been exercised end-to-end by a test. Recommend it
   ships in the SAME pass as §2.1, not deferred, since it is the only claim
   in this whole plan that is behavioral rather than a static code read. A
   second, analogous test is now needed for §3.3's Stop→cue-Ending handler:
   press Stop mid-`VarB`, assert `style section ending1` is the only verb
   sent (no `launch scene`), assert the switch lands on the NEXT bar (not
   immediately), and assert the transport stops once Ending1's own bars run
   out.
5. **§3.4 items 1 and 2 (which Ending, and whether a hard-stop escape exists
   at all) are genuine owner forks** — the research found a clear, matched
   convention for the cue-then-stop behavior itself (§3.1-§3.3), but found NO
   precedent for either question, because both only arise from sonotron's
   own choice to fold two vendor controls (STOP, ENDING) into one. I am not
   guessing an answer to a question the research shows real instruments
   never had to answer.

## Sources consulted (task #35, web research — not repo files)

- [Yamaha PortaTone PSR-330 Owner's Manual, "The Synchro Stop Function; Sync
  Stop Button; Ending Button", p.26](https://www.manualslib.com/manual/196895/Yamaha-Portatone-Psr-330.html?page=26)
  — direct-quoted source for "go to the ending section and then stop... the
  next measure."
- [Yamaha PSR Style Controls tutorial](https://psrtutorial.com/lessons/start/s45_stylecontrol.html)
  — Intro/Main/Fill/Break/Ending vocabulary and START/STOP/Sync
  Start/Sync Stop definitions.
- [Yamaha Genos Owner's Manual](https://www.manualslib.com/manual/1309559/Yamaha-Genos.html?page=65)
  and [Genos Reference Manual PDF](https://usa.yamaha.com/files/download/other_assets/7/1131007/genos_en_rm_h0.pdf)
  — Ending/rit. button, multiple Ending variations.
- Web-search synthesis of Yamaha PSR forums/FAQ (queried directly for the
  STOP-vs-ENDING distinction and for any "double-press" escape; confirmed
  the immediate-vs-cued split and found no double-press-skip precedent) —
  usa.yamaha.com FAQ, yamahamusicians.com forum threads.
- [Korg Pa "Getting Started" Guide (Pa-80 lineage)](https://cdn.korg.com/us/support/download/files/eba6cb5d61c0548822981640f449e934.pdf)
  — direct source for "press one of the ENDING buttons... or simply press
  the START/STOP button to stop the Style cold."
- [Korg Pa1000 User Manual](https://www.manualslib.com/manual/1300117/Korg-Pa1000.html)
  and [Korg Pa5X manual](https://manuals.plus/korg/pa5x-professional-arranger-keyboard-manual)
  — Style Element button vocabulary (Intro/Variation/Auto Fill/Break/Ending),
  Ending 1/2/3 naming.

## Files read for this analysis (for traceability)

- `docs/proposals/song-form-autoarrange.md` (the prior analysis this plan
  builds on)
- `components/core/arrangrr/include/arrangrr/arranger/arranger.hpp`
  (`on_tick`'s one-shot resolution, re-read lines 430-504; `request`/
  `request_style`'s `immediate` semantics, lines 302-334)
- `components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`
  (`SectionType`, `section_is_ending`, `RolePolicy`, `Style::find`)
- `components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp` (whole
  file, re-read)
- `components/core/arrangrr/include/arrangrr/engine.hpp`,
  `components/core/arrangrr/src/engine.cpp` (`SceneChain` wiring,
  `fire_arranger`'s stop handling, `cmd_transport`'s `kTransportStop` case
  lines 171-181, `cmd_style`'s `kStyleSection` case lines 635-641)
- `components/core/arrangrr/include/arrangrr/abi.hpp` (`kSceneAdd/kScenePlay/
  kSceneStop/kSceneClear`, lines ~377-405; `kTransportStop`)
- `components/core/arrangrr/tests/test_arranger.cpp` (one-shot/ending tests,
  re-read lines 580-633)
- `components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (grepped
  all 16 for `SectionType::kEnding1`/`kEnding2` coverage; `basic.hpp:366-367`
  read directly)
- `apps/gui-sonotron/src/grid_model.hpp`/`.cpp` (whole file, re-read;
  `next_scene_to_launch`, `GridModel::scene_section`/`scene_bars`)
- `apps/gui-sonotron/src/grid_panel.cpp` (re-read `seed_demo`,
  `render_header`, `activate_scene_column`, `handle_master_play_launch`,
  `update_auto_song` in full)
- `apps/gui-sonotron/src/v02_state.hpp` (whole file, re-read; `auto_song`,
  `active_scene`, `master_play_launched`)
- `apps/gui-sonotron/src/browser_panel.cpp` (`kVariations`/
  `kVariationSections`, the `outro` → `kEnding1` drag row)
- `apps/gui-sonotron/src/preview.hpp` (`Section` enum, `kEnding1`/`kEnding2`)
- `apps/gui-sonotron/src/scenes_json.hpp`/`.cpp` (whole file, re-read — the
  existing `scenes.json` persistence format for names/sections/bars, an
  already-additive-extensible sibling schema, in case a future authored
  `song_order` ever needs on-disk storage)
- `apps/gui-sonotron/src/app_state.cpp`, `apps/gui-sonotron/src/layout_
  renderer.cpp` (transport-stop propagation path, re-read)
- `apps/gui-sonotron/src/transport_panel.cpp` (the "stop" pad button,
  `brain_session.send("transport stop")`, lines 56-66; task #35)
- `apps/gui-sonotron/main.cpp` (`GridModel grid_model(5)` construction site)
- `apps/gui-sonotron/tests/test_grid_model.cpp` (whole
  `next_scene_to_launch` test block, lines 170-233, incl. the wrap-pinning
  test that must be rewritten)
- `git log` on `grid_panel.cpp`/`grid_model.*`/the prior proposal doc, to
  confirm no drift between the prior analysis and `HEAD`
- Web sources for task #35's convention research — see "Sources consulted"
  above

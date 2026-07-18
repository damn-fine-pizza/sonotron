# Sequence Edit Phase-2: real note editing in the piano-roll view

Status: PROPOSED (2026-07-18). Author: Corelli (architecture critic), read-only
design pass — no product code touched. Builds on Phase-1 (commit `b4eda51` and
follow-ups): the host-side `StepPatternModel`/`StepPatternStore`
(`apps/gui-sonotron/src/step_pattern_model.hpp/.cpp`), the `SeqEditView::kStep`
interactive on/off step grid (`apps/gui-sonotron/src/seqedit_panel.cpp`'s
`render_step_grid`/`try_render_step_canvas`), and the write-through `track
new/step/length/mute/solo` wire grammar. Roadmap context: node `11650`
"song-mode Phase 2" is a different item; this document is the "entrambi a
fasi" (owner decision, cited verbatim in `seqedit_panel.cpp:255-259`) Phase-2
half of the Sequence Edit step-sequencer feature (`11600`/`11610`), i.e. the
deferred item explicitly named in `docs/proposals/seqedit-column-view-and-zoom.md`
Feature B item 8: *"Sequence Edit becomes an EDITOR ... Real edit support is
TBD; design the column-view so an editable model can slot in later."* This is
that later.

**NOTE on the in-flight parallel work**: `apps/gui-sonotron/src/seqedit_panel.cpp`
is actively being reworked (per-lane bar-visibility toggle) as this document is
written. Every citation below is to the tree as read on 2026-07-18; line numbers
in `draw_piano_roll_lanes`/`compute_piano_roll_lanes` will drift. The structural
recommendations do not depend on exact line numbers — they depend on the shape
already documented in this file's own header comments (which is stable: one
lane per visible `kRows` role, a per-lane visibility flag, the opened role
drawn distinctly).

---

## 1. What I traced

- `docs/DESIGN.md` §2 (architectural hard rules) and the decision-log entries
  reachable from `docs/roadmap.md`'s "Former decisions" table: **D38** ("the
  GUI never links/#includes the core") — `docs/roadmap.md:754-761` — is
  **relaxed, scoped to exactly one library**: `gui_sonotron_engine` (which
  includes `apps/gui-sonotron/src/in_process_brain_session.cpp`) now links
  `arrangrr`/`hostrt`/`runtime` directly and hosts the engine in-process by
  default. Every other GUI library — `gui_sonotron_models`, `brain`,
  `gui_sonotron_layout` (where `seqedit_panel.cpp`, `step_pattern_model.hpp`,
  `preview.hpp` live) — **still never includes the core**. This is the
  load-bearing seam for this whole design: Phase-2 lives entirely in
  `gui_sonotron_layout`, so it stays core-free; it talks to the core only
  through the wire-verb string grammar `in_process_brain_session.cpp`
  translates, exactly as Phase-1 did.
- `components/core/arrangrr/include/arrangrr/timeline/timeline.hpp` (the real
  `arrangrr::Track`/`arrangrr::Step`, `Timeline::set_step`/`set_length`,
  `on_tick`/`emit_step`/`suppressed_by_tie`) — the core shape Phase-1's
  `StepPatternModel` mirrors 1:1.
- `apps/gui-sonotron/src/step_pattern_model.hpp/.cpp` — the host echo model.
- `apps/gui-sonotron/src/seqedit_panel.cpp` (full file) and
  `apps/gui-sonotron/src/seqedit_model.hpp` (full file) — the current
  `kPianoRoll` (read-only multi-lane overlay) vs `kStep` (interactive on/off
  grid) split.
- `apps/gui-sonotron/src/neon_widgets.hpp` (`ClipPattern`, `pitch_grid_cell`,
  `clip_pattern_from_pitches`) and `apps/gui-sonotron/src/preview.hpp/.cpp`
  (`PreviewPattern`, `preview_for_track`, `kMaxBars`/`kMaxSteps`/
  `kMaxVoicesPerStep`) — the shared read-only preview pipeline both the
  Repeat-Zone mini-preview and today's Sequence-Edit overlay draw through.
- `components/platform/hostrt/shell_music_commands.cpp` (`Shell::track_new`,
  `Shell::track_step`, `Shell::cmd_track`, lines 840-1000) and
  `apps/gui-sonotron/src/in_process_brain_session.cpp` (lines 692-833, the
  `command_line_to_command` `"track ..."` branches) — the two independently
  hand-maintained parsers for the identical `track new/step/length/mute/solo`
  grammar (one for the external `--control` UDS server, one for the default
  in-process backend).
- `components/core/arrangrr/include/arrangrr/abi.hpp` lines 103-115 —
  `Param::kTrackNew/kTrackStep/kTrackLength/kTrackMute/kTrackSolo`, the ABI
  opcodes both parsers ultimately encode into.
- `components/core/arrangrr/include/arrangrr/config.hpp:15-16` —
  `kMaxTracks=16`, `kMaxStepsPerTrack=64` — the real core capacity
  `step_pattern_model.hpp`'s `kStepPatternMaxTracks`/`kStepPatternMaxSteps`
  hand-copy.
- `apps/gui-sonotron/src/grid_panel.cpp:1107-1126`
  (`try_create_step_track_on_empty_cell`) — how a step-track is born (role,
  fixed port/channel, `track new` + `track mute ... on` (pre-muted) + `clip
  add ... track ...`).

I did not read `apps/gui-sonotron/CMakeLists.txt` line-by-line for the
`gui_sonotron_layout` vs `gui_sonotron_engine` target boundary; I take the
boundary as stated in the header comments of `preview.hpp`,
`step_pattern_model.hpp`, and `in_process_brain_session.cpp`, which agree with
each other and with `docs/roadmap.md:754-761`.

---

## 2. The architecture as built (what Phase-1 actually gives Phase-2)

**The wire grammar is already fully rich — not on/off-only.** `Shell::track_step`
(`shell_music_commands.cpp:873-950`) and its in-process mirror
(`in_process_brain_session.cpp:720-792`) both parse:

```
track step <idx> <step#> <note|clear> [vel] [gate] [prob=0..100] [ratchet=1..8] [micro=0..127] [tie=on|off]
```

`StepPatternModel::set_step` (`step_pattern_model.cpp:17-37`) mirrors the same
7 fields (`note, vel, gate, probability, ratchet, micro, tie`) and the same
validation (reject `note/vel > 127`, reject an audible step with `gate==0`).
`Timeline::set_step` (`timeline.hpp:103-127`) is the ABI-side twin. This
three-way mirror (core / host echo / two wire parsers) is exact today — every
field a piano-roll note editor needs (pitch, velocity, duration, plus the
Elektron-style locks) is already addressable per step index, with **zero new
ABI opcodes, zero new `Param` values, zero core changes**.

**The model is monophonic per step by construction, and this is inherited, not
new.** `arrangrr::Step` (`timeline.hpp:54-64`, `static_assert(sizeof(Step)==8)`)
carries one `note` field; `arrangrr::Track::steps[kMaxStepsPerTrack]` is one
`Step` array — one note per step index, full stop. `preview.hpp:168-171` says
this explicitly: *"a step track is monophonic per step by construction... the
remaining kMaxVoicesPerStep-1 slots always read -1."* The 4-voice capacity in
`neon::ClipPattern`/`preview::PreviewPattern` exists to show a **built-in
style's** occasional same-step chord/kit collision (`preview.hpp:54-64`), a
structurally different, non-editable data source (`arrangrr::StylePattern`).
Phase-2 inherits the monophonic constraint as a fact of the editable data
model, not a Phase-2 regression — a chord-capable step track would be a core
ABI change (widening `Step`, breaking the `sizeof(Step)==8` invariant), and I
am **not** proposing it (see §5, flagged for the owner, not designed).

**The read-only preview pipeline is a lossy, non-invertible compression — and
Phase-2 cannot reuse it for editing.** `neon::clip_pattern_from_pitches`
(`neon_widgets.hpp:83-96`, `neon_widgets.cpp`) linearly rescales the
*observed* pitch span of a pattern onto a fixed `kPitches=5` row band, so a
pitch value in `ClipPattern::pitch[step][voice]` is a **relative row index**,
not an absolute MIDI note — the mapping is content-dependent and not
invertible back to a real pitch from a click position. This is the right
design for a glanceable, always-fits, contour-only preview (which is
everything `draw_role_pattern`/`draw_piano_roll_lanes`/the Repeat-Zone
mini-cell need today), and the wrong one for "click here to place note 67."
Phase-2's interactive canvas must read/write `StepPatternModel` directly and
compute its own absolute-pitch row mapping (§6) — it must **not** route
through `preview::preview_for_track`/`neon::clip_pattern_from_pitches` the way
`render_step_grid` does today for its *visual* layer (`seqedit_panel.cpp:184-188`).
This is a genuine missing abstraction, not ceremony: the existing
`ClipPattern`/`pitch_grid_cell` seam is real and correctly scoped to read-only
preview; a second, absolute-pitch seam is needed for the editable canvas, and
inventing one is in-scope, not gold-plating.

**The two-lane-kind split is already real in the code, not aspirational.**
`try_render_step_canvas` (`seqedit_panel.cpp:260-278`) branches the whole
canvas into the interactive `kStep` grid; `draw_piano_roll_lanes`
(`seqedit_panel.cpp:112-157`) is the read-only `kPianoRoll` multi-lane
overlay, and its own header comment states outright: *"kPianoRoll ... keeps
today's overlay COMPLETELY UNCHANGED — that stays Phase-2's future editable
canvas."* Phase-2 is not inventing a new seam here; it is filling one the
codebase already named and left open.

**The `kMaxBars=2` cap is a pre-flagged, deliberate placeholder, not a
forgotten mismatch.** `preview.hpp:163-167` says verbatim: *"Capped at kMaxBars
(32 steps): a StepPatternModel can hold up to kStepPatternMaxSteps (64) steps,
wider than this preview widget supports today — widening the widget itself is
a Phase-2 (piano-roll) call, out of scope here."* §5 below is that call.

---

## 3. Interaction design → wire mapping

Every gesture below is a **host-only** GUI change (new ImGui hit-testing +
draw code in `gui_sonotron_layout`) that ends in one or more of the *existing*
`track step <idx> <n> ...` / `track step <idx> <n> clear` commands, sent
through `SeqEditModel::brain_session()` exactly as `render_step_grid` already
does (`seqedit_panel.cpp:196-213`). No new verb, no new `Param`.

| Gesture | Model op | Wire | Notes |
|---|---|---|---|
| **Add** note (click empty cell at step *i*, pitch *p*) | `StepPatternModel::set_step(i, p, defaultVel, defaultGate)` | `track step <idx> <i+1> <p> <vel> <gate>` | Mirrors `render_step_grid`'s existing add path exactly, generalized from a fixed `kDefaultNote=60` to the clicked pitch row. |
| **Delete** note (click/right-click an occupied note) | `StepPatternModel::clear_step(i)` | `track step <idx> <i+1> clear` | Identical to Phase-1's toggle-off path. |
| **Re-pitch** (drag note vertically, same step) | re-`set_step(i, newP, …)` | `track step <idx> <i+1> <newP> <vel> <gate> [prob=][ratchet=][micro=][tie=]` | **Must re-supply every non-default lock field** — see the gotcha below. |
| **Move** (drag note to a different step, same or different pitch) | `clear_step(oldI)` + `set_step(newI, p, …)` | two `track step` sends, old-then-new, in the SAME frame's mouse-release handler | Not atomic across the two wire sends, but the GUI is single-threaded against `BrainSession::send`, so there is no observable inconsistency window inside this process. If the target step already holds a note, decide overwrite-vs-reject before sending (I recommend overwrite, matching the step-grid's own toggle semantics). |
| **Resize / duration** (drag note's right edge) | see below | one-or-many `track step` sends | Two structurally different cases — see next paragraph. |
| **Velocity** ("if cheap") | re-`set_step(i, note, newVel, gate, …)` | one `track step` send | See recommendation below. |

**The full-restate gotcha (load-bearing, must be designed around, not
discovered during implementation).** Every `track step` command **restates the
whole step**, it does not patch one field. Both parsers default the four
param-lock fields to neutral (`probability=100, ratchet=1, micro=0, tie=false`)
whenever the caller omits every `key=value` token
(`shell_music_commands.cpp:899-904`, `in_process_brain_session.cpp:744-749`).
A re-pitch or move gesture that reads `StepPatternModel::step(i)`'s current
`vel/gate` but forgets to re-supply a non-default `probability`/`ratchet`/
`micro`/`tie` **silently strips those locks** the instant the user drags a
note. Every write-path in Phase-2 must read the FULL current
`StepPatternStep` (all 7 fields) before composing its `track step` command,
and always emit the `key=value` tokens for any field that differs from
neutral — not just vel/gate. This is not a wire gap; it is an interaction
implementation discipline the design must state explicitly, because Phase-1's
own `render_step_grid` never exercises it (it only ever writes the two
hardcoded defaults or clears).

**Resize — two structurally different cases, not one.** A note's audible
length in this model is `Step::gate` (raw scheduler ticks, `kTicksPerStep=240`
ticks/step, §`timeline.hpp:20-21`), which happily exceeds one step's own
width — but that does **not** shorten or silence a *different* note already
authored on a later step index; the two would simply overlap and both sound
(`Timeline::on_tick` schedules each step's on/off pair independently,
`timeline.hpp:149-187`). So:
- **Dragging into empty following steps** (no note authored at steps
  *i+1..i+k*): just grow `gate` on step *i* (bounded by the 16-bit `gate`
  field and by not visually overrunning the next *occupied* step, or the
  track's own `length`). One `track step` send.
- **Dragging to make one sustained note spanning several *editable* step
  slots that the user also wants to remain individually visible/steppable**
  (the DAW-idiom "half-note held over 8 sixteenths"): the correct model
  realization is the **tie-chain** `Timeline::suppressed_by_tie`/`emit_step`
  already implement (`timeline.hpp:196-266`) — every absorbed step in the run
  needs `note=X, vel>0 (nonzero — a zero `vel` breaks the chain, see
  `prev.vel == 0` in `suppressed_by_tie`), tie=true`, same note, and the LAST
  step of the run carries the actual release `gate`. This means **one drag
  gesture must emit N `track step` sends** (one per absorbed step index in
  the run), not one. This is real interaction complexity to hand to whoever
  implements (§7), not a design gap — the wire and the model both already
  support it exactly as specified; the piano-roll UI must compose the
  multi-step batch correctly, including the edge case of dragging across a
  run that is already partially tied (read-modify-write the whole run
  consistently, don't leave orphaned tie flags on discarded steps).

**Velocity ("if cheap").** Recommend hover + scroll-wheel (or a modifier-drag,
e.g. Alt+drag-vertical) adjusting the hovered note's `vel` by ±1-5 per notch,
committed via the same restate-with-full-locks `track step` send. A dedicated
horizontal velocity-lane strip (the common DAW affordance, a second band under
the pitch canvas) is real UI work (new layout region, new hit-testing, new
draw pass) — defer it to a follow-up increment inside Phase-2 rather than the
first cut; it adds no new wire/model requirement, only screen space and
render code.

**Confirmed**: no core change, no new ABI opcode, no new `Param` value is
required for add/delete/move/re-pitch/resize/velocity. The one real gap is
architectural (§2's absolute-pitch rendering seam, §6), not protocol-level.

---

## 4. Data-flow: is the model rich enough?

Yes, already, per §2/§3 — `StepPatternModel` is not on/off-only; it is a
field-for-field mirror of the core's own `arrangrr::Step` (note, velocity,
gate/duration, probability, ratchet, micro-timing, tie), which is everything a
monophonic-per-step piano-roll editor needs. **No enrichment of
`StepPatternModel`/`StepPatternStep` is required.** The write-through
direction (piano-roll edit → host model mutation → `track step ...` wire send
→ core `Timeline::set_step`) is the same one-writer, two-echo shape Phase-1
already proved end-to-end (`apps/gui-sonotron/tests/
test_step_track_end_to_end_contract.cpp`); Phase-2 exercises more call sites
into the same `StepPatternModel::set_step`/`clear_step` + the same
`BrainSession::send` path, nothing new in kind.

The one enrichment genuinely needed is **not to the data model** but to the
**pitch axis metadata carried alongside it for rendering** — see §6.

---

## 5. `ClipPattern::kMaxBars=2` vs the Timeline's 64-step/4-bar capacity

**Do not widen `neon::ClipPattern::kMaxBars`/`preview::kMaxBars` globally.**
That constant is correctly sized for its actual job: the longest built-in
style section observed in the corpus is 2 bars
(`preview.hpp:42-46`, `neon_widgets.hpp:46-51`, both citing the same
Wave-1-style-depth evidence). It is shared by `ClipPattern`/`PreviewPattern`,
which is ALSO the read-only preview shape for built-in style sections and
captured loops — those callers never need more than 2 bars, and widening the
shared `std::array<std::array<int,4>, kMaxSteps>` to 64 steps to satisfy the
editable step-track case would inflate every non-step-track preview call for
no reason. This is exactly the "don't invent an abstraction beyond what a seam
needs" side of the ledger.

**Recommendation: the editable piano-roll canvas reads `StepPatternModel`
directly, bypassing `PreviewPattern`/`ClipPattern` entirely, sized to the
model's own real capacity (`kStepPatternMaxSteps=64`, i.e. up to 4 bars of
16).** This falls out for free once §2/§6's absolute-pitch rendering path
exists — it was never going through the 2-bar-capped preview pipeline for its
*interactive* layer anyway (`render_step_grid`'s `total_steps` today is
already artificially clamped to `neon::ClipPattern::kMaxBars` via
`preview_for_track`, `seqedit_panel.cpp:186-188` — this is the SAME
pre-flagged clamp `preview.hpp:163-167` names, and it silently truncates a
64-step track's editable region to steps 1-32 today; Phase-2 removes this
clamp, not by widening the shared constant, but by not routing through it).
Bound the new canvas's step count by `StepPatternModel::length()` (already
settable up to 64 via the existing `track length` verb,
`shell_music_commands.cpp:969-982`), not by a hardcoded bar cap — a track
authored at length 64 becomes fully visible and fully editable for the first
time.

Read-only lanes for *other, non-open* roles keep going through
`resolve_track_cell_preview`/`ClipPattern` exactly as today — this
recommendation only detaches the ONE open/editable lane from that pipeline.

**Cost/complexity**: near-zero. No new struct beyond a small
`kPianoRollMaxSteps = kStepPatternMaxSteps` constant (or reuse the existing
one directly) local to the new rendering code; no ABI, no dependency.

---

## 6. Piano-roll rendering design

**Pitch axis**: abandon the 5-row relative-contour band
(`neon::ClipPattern::kPitches=5`) for the editable lane. Use an absolute MIDI
row axis: `row = note` (0-127), windowed to a scrollable/zoomable band (e.g.
default-fit to `[min(observed)-3, max(observed)+3]` clamped to a sane range
like `[36,96]` on first open, then user-pannable) so a screen Y position maps
1:1 and invertibly back to a MIDI note — this is the missing seam named in
§2. A thin piano-keyboard gutter on the left (black/white key coloring per
`note % 12`) is the standard, cheap affordance for "which row is which pitch"
and costs one small draw loop, no new dependency.

**Grid/snap**: keep the existing 16th-note column grid
(`preview::kSteps=16`/step, `SeqEditModel::grid_division()` already exists as
a `"1/N"` readout, currently fixed display-only) — snap add/move gestures to
the column boundaries the model already quantizes to (one `Step` per column,
no sub-step placement exists in the model, so there is nothing finer to snap
to without a core change). The existing vertical bar-guide draw loop
(`seqedit_panel.cpp:462-468`, `bars * 8` divisions) generalizes unchanged to
the new step count from §5.

**Playhead**: unchanged — the existing beat-synced sweep
(`seqedit_panel.cpp:526-530`, `neon::playhead_at`) already spans the full
canvas height and needs no Phase-2-specific change.

**Coexistence with the per-role lane view**: only the row(s) belonging to
`model.part_index()` (the opened role) become the new interactive
absolute-pitch canvas, and only when `model.open_step_track() >= 0` (a real
step-track cell is open — a built-in style section has no writable Timeline
track to target, so it stays read-only, matching `try_render_step_canvas`'s
existing guard). Every other visible role's lane keeps rendering through
`draw_role_pattern`/`ClipPattern` exactly as today, dimmed, non-interactive,
for glanceable context.

**Coexistence with the in-flight per-lane visibility rework (do not touch,
design around it)**: today `compute_piano_roll_lanes`/`draw_piano_roll_lanes`
do not special-case the opened role — if the user hides the OPEN track's own
lane via the per-lane toggle, that lane simply does not appear in
`lane_row_indices`, and (today) nothing is lost because the lane was
read-only anyway. Once that lane becomes the editable canvas, hiding it would
either silently remove the only way to edit the open track, or (if the
toggle-rework changes shape before Phase-2 lands) do something the toggle's
own author didn't anticipate. **Phase-2 must add one small rule, independent
of whatever the in-flight rework's final shape is**: the opened role's own
lane is always force-visible while it is the open, editable target,
regardless of the per-lane hide flag — or, if the owner prefers preserving
full hide-ability, the canvas shows an explicit "role hidden — click × again
to edit" placeholder instead of a silently absent editing surface. This is a
one-line policy decision for whoever lands Phase-2 to make with the owner; I
flag it here so it is not rediscovered as a bug after both features ship.

---

## 7. Phased implementation plan, file set, risks

**Phase 2a — absolute-pitch read/write canvas for the open track only**
(SHIPPABLE, HOST-ONLY, zero dependency):
- New rendering + hit-testing in `apps/gui-sonotron/src/seqedit_panel.cpp`
  (a sibling to `render_step_grid`, e.g. `render_piano_roll_canvas`), gated
  by `model.view() == SeqEditView::kPianoRoll && model.open_step_track() >= 0`.
- Add/delete/re-pitch/move gestures (§3), each ending in the correct
  full-restate `track step ...` send via `SeqEditModel::brain_session()`
  (unchanged API, same as Phase-1).
- Read the model directly (`StepPatternModel::step`/`length`), bypassing
  `preview::preview_for_track`/`neon::ClipPattern` for this one lane (§5/§6).
- Files touched: `apps/gui-sonotron/src/seqedit_panel.cpp` only (plus
  whatever small pitch-row/keyboard-gutter helper is factored out, either
  inline or as a new tiny header if `clementi`/`nazzareno` judge the function
  too large — that split is an implementation call, not an architecture one).
- Risk: coexistence with the in-flight bar-visibility rework — implement
  AFTER that lands, or coordinate file ownership explicitly; both touch the
  same function (`draw_piano_roll_lanes`).

**Phase 2b — resize/duration, including the tie-chain multi-step batch write**
(SHIPPABLE, HOST-ONLY): the more intricate gesture from §3; sequence after
2a so add/delete/move/re-pitch ship and are tested first. Risk: the
read-modify-write-a-whole-run correctness (§3's tie-chain paragraph) is the
single trickiest piece of this whole design — recommend a focused
Torquato-owned test (author a tied run, drag-resize it, assert the resulting
`StepPatternStep[]` run shape) before it ships, not just an end-to-end
click-test.

**Phase 2c — velocity edit + widened scope to the full 64-step/4-bar capacity**
(SHIPPABLE, HOST-ONLY): scroll/modifier-drag velocity (§3); detach the open
lane's step count from `preview::kMaxBars`'s 2-bar clamp (§5). These are
independent of each other and of 2a/2b's core interaction code, so they can
land in either order or in parallel once 2a exists.

**Deferred, not designed here (flag to the owner, not a silent omission)**:
- A dedicated velocity-lane strip (§3) — real screen-space/layout work,
  optional polish.
- Multi-note selection / box-select / copy-paste of a run of steps — not
  asked for in this task's scope, and would need its own interaction design.
- Anything touching `arrangrr::Step`'s shape (e.g. true polyphony per step) —
  a core ABI change, explicitly out of scope, see §8.

**Full file set for 2a-2c**: `apps/gui-sonotron/src/seqedit_panel.cpp`
(rendering + gestures), possibly `apps/gui-sonotron/src/step_pattern_model.hpp/.cpp`
only if a convenience read-helper is wanted (none is structurally required —
`step()`/`set_step()`/`clear_step()`/`length()` already suffice), no changes
to `preview.hpp/.cpp` or `neon_widgets.hpp/.cpp` (Phase-2's editable lane
deliberately does not route through them, §5/§6), no changes to
`in_process_brain_session.cpp` or `shell_music_commands.cpp` (the wire
grammar is already sufficient, §3), no changes to
`components/core/arrangrr/**` (zero ABI, confirmed).

---

## 8. Flagged for the owner / not decided here

- **Monophonic-per-step is a real ceiling, not a Phase-2 defect.** A
  chord-capable step (e.g. authoring a triad on one step of a "chord" role
  track) is impossible with the current `arrangrr::Step` shape
  (`sizeof(Step)==8`, one `note` field) and would require a core ABI change.
  I have not designed this — it is a genuine core-regime decision the owner
  must make explicitly if wanted; Phase-5's ABI-unfreeze (per project memory)
  makes it *feasible* to raise, not something I am proposing by default.
- **Lane force-visibility policy (§6's coexistence rule)** — a one-line
  product decision (force-visible vs. placeholder-when-hidden) that depends
  on how the concurrently in-flight visibility-toggle rework lands; needs the
  owner or whoever implements Phase-2 to pick one, informed by this
  document's framing.
- **Default pitch-window on first open** (§6: fit-to-content vs. a fixed
  default range like `[36,96]`) is a UX taste call, not an architecture one —
  I recommend fit-to-content-with-margin but this is not load-bearing either
  way.
- No new dependency is proposed anywhere in this design; none is needed.

---

## Summary (orchestrator-relayable)

**Design in brief**: Phase-2 turns the currently read-only `kPianoRoll`
overlay's *one open/editable lane* into a real absolute-pitch note editor —
add/delete/move/re-pitch/resize/velocity — all mapped onto the **already
sufficient** `track step <idx> <n> <note> <vel> <gate> [prob=][ratchet=]
[micro=][tie=]` / `track step ... clear` wire grammar Phase-1 shipped, into
the **already rich** (not on/off-only) `StepPatternModel`. The one real
architectural gap is that today's read-only preview pipeline
(`neon::ClipPattern`/`preview::PreviewPattern`) uses a **lossy, non-invertible
relative-pitch compression** (correct for glanceable preview, wrong for
click-to-place editing) and a **2-bar cap** that is a pre-flagged placeholder
(`preview.hpp:163-167` already names this exact Phase-2 call) — Phase-2's
fix is a **new, dedicated absolute-pitch rendering path** that reads
`StepPatternModel` directly (up to its real 64-step/4-bar capacity) instead of
routing through that pipeline, leaving the read-only preview pipeline
untouched for every other caller (Repeat-Zone mini-cells, other roles' lanes,
built-in style sections).

**Confirmed ZERO-ABI**: no new `Param`/ABI opcode, no `arrangrr::Step`/`Track`
shape change, no core file touched. Every gesture in §3 composes out of the
existing 5 wire verbs (`new/step/length/mute/solo`), including the trickiest
one (tie-chain resize) which needs *several* existing-verb sends per gesture,
never a new verb.

**One real design gotcha for whoever implements**: `track step` restates the
WHOLE step every time — a re-pitch/move/resize write that forgets to
re-supply a note's existing non-default `prob=/ratchet=/micro=/tie=` silently
strips those locks (evidence: `shell_music_commands.cpp:899-904`,
`in_process_brain_session.cpp:744-749`). Must read-full-then-write-full.

**Open questions for the owner**: (1) lane force-visibility vs. hidden
placeholder when the open track's own role is toggled off by the
concurrently-in-flight visibility rework; (2) whether true per-step polyphony
(a core ABI change) is ever wanted — not designed here, flagged only; (3)
default pitch-window behavior on first open (taste, not architecture).

Persisted at: `docs/proposals/seqedit-piano-roll-phase2-design.md`.

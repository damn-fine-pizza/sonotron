# Green Tests, Broken App — a Process Gate

**Author:** Guido (process analyst). **Scope:** gui-sonotron, generalizable to
any dual-target/host component in this repo. **Status:** PROPOSAL — no test
authored, no product code touched. This document specifies WHAT must be
proven and hands authoring to Torquato; it builds on mechanisms the project
already has rather than inventing new ones.

**Why this exists (owner ask, 2026-07-17):** the same failure shape —
`ctest` green, the running app broken — has bitten this branch at least
three times in three days. This is not "we need more tests"; it is a hole in
the PROCESS that decides what a test is allowed to assume. This document
names the hole precisely and proposes a gate that closes it, sequenced by
pain-relieved-per-unit-friction.

This proposal is a companion to, and does not duplicate,
`docs/proposals/flow-verification-matrix-2026-07.md` (Guido, 2026-07-16/17),
which already inventoried ~55 flows and made one recommendation (§1 there:
textual REAL/STUBBED convention over a new CTest label) and left one fork
open (mechanical label vs textual convention). That inventory is reused here
as the traceability substrate (§5); this document adds the piece it did not:
a DoD gate that would have CAUGHT each of the three incidents before they
shipped, expressed as something a PR author can run tomorrow.

---

## 1. The three incidents, read from the code and git history

### Incident A — auto-song arming (commit `7113a90`, root-caused same day)

Before the fix, `apps/gui-sonotron/src/v02_state.hpp:106` shipped
`bool auto_song = false;`. The ONLY production writer of `fx.auto_song` was
the header toggle, `apps/gui-sonotron/src/grid_panel.cpp:373-374`:

```cpp
if (ImGui::SmallButton(fx.auto_song ? "auto-song" : "song")) {
  fx.auto_song = !fx.auto_song;
```

a `SmallButton` with an alpha-0-at-rest fill (documented separately in
`flow-verification-matrix-2026-07.md` §1 as "the one widget the UI-automation
harness cannot yet click-locate"). Master Play
(`handle_master_play_launch`) and transport start never touched this field.
Meanwhile every test in `apps/gui-sonotron/tests/test_grid_panel_auto_song.cpp`
called a test-local helper, `click_arm_auto_song` (lines 88-91):

```cpp
void click_arm_auto_song(V02State& fx, const AppState& app_state) {
  fx.auto_song = true;
  ...
  fx.auto_song_last_bar = app_state.bar();
}
```

Despite its name, this is not a click — it is a direct field write. Every
pre-fix auto-song test proved "if `auto_song` is armed, the advance decision
is correct" while asserting nothing about whether pressing Play ever arms it.
The fix (`7113a90`) changed the default to `true` and added
`test_auto_song_armed_by_default_advances_without_manual_toggle` (same file,
line 376), whose entire point, per its own comment (line 366-373), is to
**never** call `click_arm_auto_song`.

### Incident B — seed_demo / ClipMatrix registration (commit `dff4e9e`)

Before the fix, `seed_demo()` (`apps/gui-sonotron/src/grid_panel.cpp`, prior
signature `seed_demo(GridModel&, SeqEditModel&, V02State&)`) only called
`GridModel::set_cell` — host-side display state. It never sent the
`clip add <role> <scene> style <section> id <n>` wire verb that the browser
drag-drop path (`grid_panel.cpp:561-573`-area `BeginDragDropTarget` block)
already sent for real drops. Consequence: the core's `ClipMatrix` never held
these clips, so `launch clip`/`launch scene` hit `m_clips.get(id)==nullptr`
in `engine.cpp` and silently warned — no audio, no `clip_state` flip, ever,
for demo content.

Why this stayed green: the auto-song decision tests
(`test_grid_panel_auto_song.cpp`) link against a `SpyBrainSession` (per that
file's own header comment) — a test double with no `ClipMatrix` inside it at
all. Asking a `SpyBrainSession` to accept `launch scene …` cannot fail the
way the real `InProcessBrainSession`/`ClipMatrix` can, because the double
does not implement the invariant ("a clip must be registered before it can
launch") that production enforces. The test and production were not
exercising the same wiring — this is the "test-double bypasses the
constraint the bug lives in" variant of the same root pattern, not a
hand-set boolean, but structurally identical: **the double stands in for
exactly the causal step under test.** The fix (`dff4e9e`) made `seed_demo`
send the real `clip add` verb through a real `BrainSession&`, and shipped the
UI-automation harness (`imgui_headless_harness.hpp`) plus a replay test
against an owner-recorded trace (`fixtures/owner_repro_2026_07_16.jsonl`)
specifically because, per that commit's own message, "all three were RED
before the fix and are GREEN after" only once real input + real backend were
in the loop.

### Incident C — advance verbs (commit `cfa0a78`)

Before the fix, `update_auto_song`'s advance branch sent only
`"style section <name>"`. It never sent `"launch scene <n> quantize <q>"` —
the second verb the manual scene-header ▶ click
(`render_scene_header_cell`) always sends alongside the first. Consequence:
`fx.active_scene` genuinely advanced (host-side bookkeeping), but the newly
active column's clips were never launched — audio stayed on scene 1 forever.
Per the commit message, this was root-caused by "a live
`SONOTRON_AUTOSONG_TRACE` stderr trace" specifically because **every headless
proxy test stayed green**: the existing tests asserted on
`fx.active_scene`/`next_scene_to_launch` (an internal decision variable) and
never asserted on the actual verb sequence sent to the session, so a bug in
"what gets sent" was invisible to them regardless of whether the precondition
was hand-armed or not. The fix added `test_grid_panel_auto_song_launches_next_scene.cpp`
(`cfa0a78`, `apps/gui-sonotron/tests/CMakeLists.txt:157`'s own comment: "This
test proves the fix stays fixed; do not weaken or remove this assertion").

### The common thread, verified, not assumed

All three are instances of one structural failure: **the test manufactures,
or lets a double stand in for, the very causal step the production bug lives
in**, and then asserts on an internal state variable rather than an
externally observable outcome. Concretely, three flavors of the same hole:

1. **Hand-armed precondition** (A): the test sets the flag production is
   supposed to set, so the setter itself is never exercised.
2. **Double that doesn't share the invariant** (B): the test's stand-in
   backend cannot enforce the constraint (`ClipMatrix` registration) that the
   real backend enforces, so a violation of that constraint is unobservable.
3. **Assertion on the wrong observable** (C): the test checks the internal
   decision (`active_scene` advanced) instead of the user-observable effect
   (the right verb reached the session / the right clip is audible).

None of the three is "not enough tests." Each incident had passing tests
specifically for the flow that broke. The hole is in what those tests were
allowed to assume, not in their quantity.

---

## 2. What already exists that this proposal builds on (verified)

- **CTest label convention** `unit` / `functional` (`apps/gui-sonotron/tests/CMakeLists.txt:1-17`),
  consistent with `components/core/arrangrr/tests` and others.
- **Three coverage metrics** (`scripts/coverage.sh:1-20`, memory
  `qa-bug-protocol` and `coverage-metrics` in the `arrangrr` project memory
  store): metric 1 unit (advisory 80% l/f/b, `coverage-gate-local-not-ci`
  memory confirms this is host-local, not CI-gated), metric 2 functional
  (report-only), **metric 3 regression — already implemented as a CENSUS
  with a monotonic-non-decreasing check against an optional
  `scripts/coverage-regression-baseline.txt`** (`scripts/coverage.sh:120-136`).
  This mechanism exists and works; it is simply unused by this app (see gap
  list below).
- **`ci.sh`** (`scripts/ci.sh`) runs the full untagged host suite
  (`ctest --preset host`) plus an ARM cross-build link gate plus `lint.sh`.
  **`.github/workflows/build-release.yml:142-150`** independently confirms
  Linux CI runs the FULL `ctest` suite (excluding only `tui_console`, which
  needs a real ALSA sequencer device) — this is a real, working outbound gate
  for "the labelled suites are green," already running on every push.
  Neither `ci.sh` nor the GitHub workflow runs `coverage.sh`, and neither
  drives a headless GUI boot (see gap 2 below).
- **The UI-automation harness** (`apps/gui-sonotron/tests/imgui_headless_harness.hpp`,
  memory `ui-automation-harness`): real ImGui input injection, widget
  location via draw-data color-cluster scanning, assertions on rendered
  vertices. This is the antidote already built. It closed incidents A, B and
  C's regression risk once applied, but nothing in the repo REQUIRES a new
  user-facing behavior to use it — a test can still be authored the old way.
- **Input record/replay** (`apps/gui-sonotron/src/input_trace.hpp/.cpp`,
  `--trace-input`/`--replay-input`) plus a working example,
  `test_repeat_zone_owner_trace_replay.cpp`
  (`apps/gui-sonotron/tests/CMakeLists.txt:205-220`), which replays an
  owner-recorded live session against a REAL `InProcessBrainSession`. This is
  the "trust a live trace over green proxies" lesson already made concrete
  once — but only for the one bug it was built to pin, not as a standing
  gate over other flows.
- **`qa-bug-protocol`** (project memory, `arrangrr`): red-before-green is the
  stated discipline for bug fixes. All three incidents above DID follow it
  (each fix commit message documents the red state and the trace/test that
  proved it) — the discipline is operating for the FIX step. What is missing
  is upstream of the fix: nothing stopped the ORIGINAL feature test from
  being written blind in the first place.
- **`flow-verification-matrix-2026-07.md`**: already a requirement→test→
  outcome table for ~55 flows, with an explicit "Gap: hand to Torquato" list
  and an open fork on label mechanism (§1 there). This proposal does not
  re-inventory; it names the GATE that would make new entries in that table
  arrive already fidelity-checked, instead of needing a forensic pass after
  the fact.

---

## 3. The gate — Behavior Capture Card + DoD

### 3.1 The core rule

> **No causal precondition armed by hand.** The trigger under test must be
> reached through the same entry point a real user reaches it through
> (a click via the UI-automation harness, a real wire verb via a real
> `BrainSession`/`InProcessBrainSession`, a real file on disk) — never by
> assigning the internal field/flag that the behavior under test reads.

This needs one explicit distinction, because "never set anything by hand" is
too strong and would ban ordinary test setup:

| | Armable by hand (fine) | Never armed by hand (the rule) |
|---|---|---|
| **What it is** | The ENVIRONMENT the flow runs in: scene count, active style, BPM, a fixture MIDI file, window size, `fx.seeded` latch bypassed for speed | The SPECIFIC causal step the behavior/bug under test is about: the flag/verb/registration that production itself is supposed to set as a CONSEQUENCE of the user action |
| **Incident A example** | Constructing a `V02State` with N scenes, a chosen style | `fx.auto_song = true` — THIS is what Play is supposed to arm; incident A's bug was that nothing armed it |
| **Incident B example** | Populating `GridModel::set_cell` demo content | `clip add … id <n>` reaching the real `ClipMatrix` — THIS is what registration is supposed to do; incident B's bug was that nothing sent it |
| **Incident C example** | `fx.active_scene`, `fx.auto_song_last_bar` bookkeeping used to compute WHEN to advance | The verb(s) actually sent on advance — THIS is what the bug was about; asserting only on `fx.active_scene` missed it |

The test says which is which: a test is allowed to construct any
environment, but the ONE field/verb that IS the reported behavior must be
reached by driving the real entry point, not written directly. If a
behavior's causal step is unreachable except via a stand-in (drag-and-drop
today has no click-injection seam — `flow-verification-matrix-2026-07.md`
§4 already lists this honestly as DEFERRED), the test must say so explicitly
(§3.3) rather than pretend the stand-in is equivalent.

### 3.2 Behavior Capture Card (inbound capture template)

For every NEW user-facing objective (a feature, a button, a flow), capture
it at intake with this card before work starts — a wish becomes a testable
objective only once every field is filled:

```
BEHAVIOR CAPTURE CARD
======================
Objective (one sentence, user-facing, imperative):
  e.g. "Pressing master Play must advance the song through its scenes
  unaided."

Entry point (file:line of the REAL production trigger a user reaches):
  e.g. grid_panel.cpp:397-421 render_scene_header_cell (▶ click)
  — if this does not exist yet, name where it WILL live; if you cannot
  name a file:line, the objective is not yet specifiable, go back a step.

Causal precondition (what must become true as a CONSEQUENCE of the entry
point firing, and WHERE in production code it becomes true):
  e.g. "fx.auto_song must be true (or default true) after Play is pressed."
  — if nothing in production sets it today, WRITE THAT DOWN. That line is
  the single most valuable line in this card; all three incidents in this
  document are exactly a precondition that had no production setter.

Observable outcome (externally observable — a readback the user or the
engine can see, NEVER an internal decision variable alone):
  e.g. "AppState::section()/clip_state(id) reflect the new scene within
  1 bar" — not "fx.active_scene changed" (that is internal bookkeeping,
  see incident C).

Acceptance test(s) (name at least one; state its fidelity per §3.3):
  e.g. test_grid_panel_auto_song_armed_by_default_advances_without_manual_toggle
  — END-TO-END (real render loop, no manual arm).

DoD (all four must hold to call the objective closed):
  [ ] Entry point reached by the test the way a user reaches it (§3.1)
  [ ] Causal precondition's production setter EXISTS and is exercised
      (not assumed, not hand-armed)
  [ ] Observable outcome asserted (not an internal variable alone)
  [ ] If this closes a reported bug: a `regression`-labeled test exists,
      red-before-green (qa-bug-protocol), and is added to the census
      (§4.5)
```

This card is deliberately short and text-only — no new tool, fillable in a
PR description or a commit message body (several of the three incident fixes
above already write 80% of this card informally in their commit messages;
this formalizes the shape so it is filled BEFORE the bug, not reconstructed
after).

### 3.3 Trigger-provenance tag (mandatory, one line, grep-able)

Every `functional`-labeled test file gets one line near its top, in a fixed,
grep-able shape (extends, rather than replaces, the header-comment
convention `flow-verification-matrix-2026-07.md` §1 already found and
endorsed):

```cpp
// TRIGGER: real   — apps/gui-sonotron/src/grid_panel.cpp:397 (scene-header click)
```
or, when a real trigger genuinely does not exist yet (drag-and-drop, the
alpha-0 SmallButton):
```cpp
// TRIGGER: stubbed — no click-injection seam for ImGui::SmallButton alpha-0
//   fill; this test proves DECISION LOGIC ONLY, see DESIGN-CAPTURE gap below.
```

`grep -rn "// TRIGGER: stubbed"` over `apps/*/tests` becomes, for free, the
running list of "tests that cannot yet prove the live path" — the exact
class of test that hid all three incidents. This is cheaper than the
mechanical `functional-stubbed` CTest label `flow-verification-matrix-2026-07.md`
§1 left as an open fork: a grep line costs nothing to introduce and is
reviewable in a diff; promoting it to a real CTest label later is still
available if the owner decides the textual convention isn't holding
(unchanged open fork, carried forward, not re-decided here).

### 3.4 The "kill-the-setter" check (entry-point mutation, PR discipline)

For every test tagged `// TRIGGER: real`, the author (or reviewer) performs
one manual check before merging:

1. Comment out (or temporarily revert) the ONE production line that arms
   the causal precondition named on the Behavior Capture Card (e.g.
   `fx.auto_song = true;` in `v02_state.hpp`, or the `clip add` send in
   `seed_demo`, or the `launch scene … quantize …` send in
   `update_auto_song`).
2. Rebuild just that test binary and run it (`cmake --build --preset host
   --target <test_name> && ./build/host/apps/gui-sonotron/tests/<test_name>`).
3. Confirm it goes RED. If it stays green, the test is not actually
   exercising the entry point it claims to — this is exactly how all three
   incidents would have been caught before the fix commit, not after.
4. Restore the line, rebuild, confirm green again.

This is manual today, deliberately — it is a PR-checklist step, not a CI
job, because a general "delete this line and see what goes red" automation
needs a maintained manifest mapping production lines to the tests that
should catch their absence (real mutation testing, `mull`, is already
flagged as owner-approval-needed future work in `coverage-metrics` memory —
this is the SAME idea scoped down to exactly the lines each incident already
proved matter, at zero tool cost). See §6 for the future-automation version
and its cost.

---

## 4. Making red-before-green and the 3 metrics operative, not aspirational

The mechanisms below already exist in `scripts/coverage.sh`; gui-sonotron
simply never engaged them. This is the cheapest fix in this whole document.

1. **Zero `regression`-labeled tests exist in gui-sonotron today** — verified
   by `grep -rln "LABELS regression" apps/gui-sonotron/tests/CMakeLists.txt`
   (no match), even though at least three real, owner-reported bugs were
   fixed on this branch this cycle with red-before-green tests already
   sitting in the tree, unlabeled:
   - `test_auto_song_armed_by_default_advances_without_manual_toggle`
     (incident A, `test_grid_panel_auto_song.cpp:376`)
   - `test_grid_panel_auto_song_launches_next_scene.cpp` (incident C,
     `apps/gui-sonotron/tests/CMakeLists.txt:157`, own comment: "proves the
     fix stays fixed")
   - the UI-automation + replay tests from `dff4e9e` (incident B) —
     `test_grid_cell_preview_vs_seqedit_ui_automation.cpp` and
     `test_repeat_zone_owner_trace_replay.cpp`
   **Handed to Torquato: relabel these from `functional` to `regression`**
   in `apps/gui-sonotron/tests/CMakeLists.txt` (a one-word category change
   per test, zero new authoring) — this alone takes the census from 0 to
   ≥4 and stops metric 3 silently reporting nothing while the discipline is
   in fact operating.
2. **No `scripts/coverage-regression-baseline.txt` exists yet** (confirmed:
   file absent). Once the relabel above lands, **handed to Torquato/owner:
   commit an initial baseline** (`echo 4 > scripts/coverage-regression-baseline.txt`
   or the real count `coverage.sh` reports) so the monotonic-non-decreasing
   check in `scripts/coverage.sh:130-136` actually engages instead of
   printing `(no baseline; report-only)` forever.
3. **Red-before-green itself is operating correctly at the FIX step** for
   all three incidents (each commit message documents the red state). The
   gap is upstream: nothing required the ORIGINAL feature test (before any
   bug was known) to already satisfy §3.1's rule. §3.2's Behavior Capture
   Card is the mechanism that closes that upstream gap — it turns
   red-before-green from "something we do once a bug is reported" into
   "something intake already set up correctly the first time."

---

## 5. Traceability: requirement → test → outcome

The project's requirement ledger is `docs/DESIGN.md` §22, a numeric WBS tree
(`0000`-`99999`) with a status glyph (✅ ◑ ▶ ○) per node
(`docs/DESIGN.md:621-664`). Verified: **no node carries acceptance criteria
or a DoD field** — `grep -n "Acceptance\|Definition of Done\|DoD"
docs/DESIGN.md` returns zero hits. A node's status is a glyph a human sets by
judgment; there is no mechanical link from a node to the test(s) that prove
it, which is exactly why an objective can be marked ✅ while its production
setter is missing (incident A's `auto_song` arming was never itself a WBS
leaf — it was an implicit assumption inside a larger "auto-song" feature that
WAS marked done).

Proposed shape, reusing what exists rather than building a new database:

- **`flow-verification-matrix-2026-07.md`'s own table IS the
  requirement→test→outcome traceability artifact** (`Flow | Trigger path |
  Verification today | Fidelity verdict | Gap`) — it should be treated as a
  living document, refreshed whenever a zone gets substantial new
  user-facing behavior (not on every commit — continuous refresh is not
  proposed here, see Open Decisions). Each row already cites file:line and a
  named test; this is the traceability shape to keep, not replace.
- **One-line `// REQUIREMENT: <short-name-or-WBS-node>` tag**, mandatory
  alongside the `// TRIGGER:` tag (§3.3), in every `functional`/`regression`
  test file, naming which flow-inventory row (or WBS node, once/if one
  exists for the behavior) the test closes. Several tests already do this
  informally and inconsistently (e.g. "Owner task #19", "docs/
  repeat-zone-real-contract.md §4/§8b decision 3" — both real, found while
  grounding this document) — this formalizes an existing habit into a
  mandatory, uniformly-shaped one-liner, not a new practice.
- **A behavior only counts as "captured" (able to enter a sprint/session of
  work) once it has a Behavior Capture Card (§3.2)**; it only counts as
  "verified" (able to be marked done) once the DoD checklist on that same
  card is fully checked. The card IS the requirement→test→outcome link for
  new work; the flow matrix is the same link retrofitted for existing work.

---

## 6. Adoption order — ranked by pain relieved vs friction added

1. **Relabel existing fix-pin tests to `regression` + commit an initial
   baseline (§4.1-4.2).** Zero new authoring, zero new tooling, immediately
   turns metric 3 from silently-zero to reporting the truth. Adopt THIS
   WEEK.
2. **`// TRIGGER: real|stubbed` + `// REQUIREMENT: …` tags (§3.3, §5) on
   every functional/regression test going forward, and retrofit onto the
   handful of tests already discussed in this document and in
   `flow-verification-matrix-2026-07.md`.** Pure text, no build/CI change,
   immediately reviewable in any PR diff. Adopt NEXT.
3. **Behavior Capture Card (§3.2) as the intake gate for every NEW
   user-facing objective.** Slightly more friction (a card to fill before
   work starts) but this is the mechanism that would have caught all three
   incidents at DESIGN time rather than at QA time. Retrofitting it onto
   PAST objectives is optional/backlog, not required — apply it going
   forward from adoption.
4. **The manual "kill-the-setter" check (§3.4) for every `TRIGGER: real`
   test touching transport/browser/grid panels.** Higher friction (a
   rebuild-and-run per PR) but the highest-signal check against exactly this
   failure class; scope it to the panels that have already burned days
   (grid_panel, transport_panel, browser_panel) before generalizing.
5. **Golden-flow replay smoke, one per zone, built on the existing replay
   harness (`test_repeat_zone_owner_trace_replay.cpp`'s shape).** Requires
   NEW test authoring — hand to Torquato using
   `flow-verification-matrix-2026-07.md`'s own "hand to Torquato" list as
   the starting inventory (transport Stop/Panic/continue, browser style
   click, the `NONE`-verification rows). Adopt incrementally, one zone per
   work cycle, not all at once.
6. **CI-wired headless GUI boot smoke
   (`SONOTRON_GUI_MAX_FRAMES`/`SONOTRON_GUI_SCREENSHOT`) and any future
   scripted entry-point-mutation job.** Both already flagged as open,
   feasibility-pending items in `flow-verification-matrix-2026-07.md` §4 —
   reaffirmed here, not re-decided: needs a GL/EGL-capable CI runner
   (unconfirmed) and, for the mutation job, a maintained manifest (new
   maintenance cost). Future work, owner-approval needed before scheduling.

---

## 7. What I flagged / what the owner decides

- **Carried-forward open fork (`flow-verification-matrix-2026-07.md` §4,
  unchanged, not re-decided here):** should `functional` stay one label with
  a textual REAL/STUBBED convention (this document's own §3.3 recommendation,
  now made mechanical via a grep-able tag), or should a mechanical
  `functional-stubbed` CTest label be introduced? Answerable only by the
  owner's appetite for CI/build-file churn versus a textual convention's
  reliance on review diligence.
- **New open decision, this document:** should the "kill-the-setter" check
  (§3.4) stay a manual PR-checklist step indefinitely, or be scheduled for
  scripted automation (a maintained production-line → test manifest,
  CI-run)? I recommend starting manual (cheap, immediately adoptable) and
  revisiting only if PR discipline proves unreliable in practice — but this
  is a genuine cost/trust tradeoff for the owner, not something I can settle
  by reading code.
- **New open decision, this document:** should the DoD checklist (§3.2)
  BLOCK merge (hard gate, like metric 1 briefly was) or ADVISE (soft,
  reviewer judgment, like metric 1 is now per `coverage-gate-local-not-ci`
  memory)? The project's own precedent went from hard-gate to advisory for
  metric 1 once its structural cost was understood (ABI-dispatch dilution).
  I did not assume the same resolution applies here; flagging for the owner.
- **New open decision, this document:** which flows count as "top-N" for the
  golden-flow replay smoke (§6 item 5)? I propose reusing
  `flow-verification-matrix-2026-07.md`'s own zone list and gap-ranking as
  the selection criterion rather than picking arbitrarily, but the final cut
  (how many zones, which cycle) is the owner's/Torquato's scheduling call.
- **Tool/dependency flag, carried forward, not new:** a scripted
  entry-point-mutation job (§6 item 6) is the SAME class of dependency
  `coverage-metrics` memory already flagged for mutation testing (`mull`),
  owner-approval needed; the dependency-free path (§3.4's manual check) is
  offered and is what this document recommends adopting first.
- **"Must be proven" items handed to Torquato** (specified here, not
  authored here, in priority order):
  1. Relabel the ≥4 identified fix-pin tests `functional` → `regression`
     (§4.1) and commit the initial baseline file (§4.2).
  2. Retrofit `// TRIGGER:`/`// REQUIREMENT:` tags (§3.3, §5) onto the
     tests discussed in this document and in the flow matrix's own
     "hand to Torquato" list.
  3. Author the zone-by-zone golden-flow replay smokes (§6 item 5), reusing
     `test_repeat_zone_owner_trace_replay.cpp`'s harness shape, starting with
     the flow-matrix's highest-ranked gaps (transport Stop/Panic/continue,
     browser style-click, filled-cell click, scene-header ▶, M/S latch —
     several of these are already closed per the flow matrix's §5 addendum;
     confirm current state before re-authoring).
  4. For every NEW user-facing objective from this point forward: at least
     one `TRIGGER: real` acceptance test satisfying the Behavior Capture
     Card's DoD (§3.2) before the objective is marked done.

---

**Doc path:** `docs/proposals/green-tests-broken-app-gate.md`

# Ritardando / tempo-curve fork — as-built analysis (Corelli, read-only)

Status: ANALYSIS DOC (architecture critic, read-only). No product code touched.
This is a fork analysis for the owner to decide between; not an implementation,
and not a scope lock for whichever arm gets picked.

## 0. The problem, grounded

`SectionDef{.type = SectionType::kEnding1, .bars = 2, ...}`
(`components/core/arrangrr/include/arrangrr/arranger/styles/basic.hpp:771`,
mirrored across all 16 style files) authors a 2-bar ending that cadences
harmonically but carries **no tempo dimension at all** — `bars` is a pure
tick-domain integer. Nothing downstream of it (`Transport`, `TickAccumulator`,
`Arranger::on_tick`) reads or interpolates a tempo trajectory. The owner is
right: there is no ritardando primitive anywhere in the tree today, host or
core. This doc analyzes the two ways to build one.

## 1. Cosa ho tracciato

Decisions read: `docs/DESIGN.md` D3/D4/D26/D27/D31-D33 (integer-only realtime
math, dependency-free core, no-heap/no-float-near-realtime, 960 PPQN, dual
target). ABI discipline read from the live banner in
`components/core/arrangrr/include/arrangrr/abi.hpp:14-45` — v1 is
**additive-only, UNFROZEN for Phase 5** (owner, 2026-07-13); a reshape is
permitted pre-release, a released wire format is not.

Graph traced, all with citations:

- **Tempo write path (host → core):** `Shell::cmd_bpm`
  (`components/platform/hostrt/shell_io_commands.cpp:374-405`) and
  `Shell::cmd_transport`'s `tempo` branch (`shell_io_commands.cpp:349-372`)
  both build a `Command{.op=kSet, .param=Param::kTransportTempo, .a=bpm_x100}`
  and push it through `Engine::push_command`. `Engine::cmd_transport`
  (`components/core/arrangrr/src/engine.cpp:125-131`) dispatches
  `Param::kTransportTempo` straight to `Transport::set_bpm`
  (`components/core/runtime/include/runtime/transport.hpp:39-45`), a clamped,
  `noexcept`, O(1) scalar store — no interpolation, no history.
- **The real-time clock, separate from Transport:** `TickAccumulator`
  (`components/core/common/include/common/time.hpp:83-101`) is a drift-free,
  integer-only (D32) accumulator: `advance_us(elapsed_us)` recomputes
  `ticks = (acc + elapsed_us*bpm*kPpqn) / kTickDenominator` **fresh, from the
  CURRENT `m_bpm`, on every call** — it holds no lookahead buffer of
  future tick times.
- **Three independent, byte-identical driver loops** each own their own
  `TickAccumulator` instance and re-sync it from `Transport::bpm()` on
  *every* iteration, immediately before calling `advance_us`:
  `apps/gui-sonotron/src/in_process_brain_session.cpp:1539` —
  `acc.set_bpm(shell.engine().transport().bpm());` followed by
  `acc.advance_us(now_us - last_us)` (line 1540);
  `apps/tools/cli-arrangrr/main.cpp:692-693`;
  `apps/sonotron-server/main.cpp:331-332`. All three are the identical
  idiom, unshared (a duplication finding, see §5).
- **The realtime scheduler is tick-indexed, not wall-clock-indexed:**
  `OutScheduler::schedule`/`before` (`components/core/runtime/include/
  runtime/out_scheduler.hpp:75-90, 193-201`) key every pending note-off on an
  absolute `Tick`, never a wall-clock deadline. `Runtime::advance_ticks`
  (`components/core/runtime/include/runtime/runtime.hpp:71-84`) is the only
  thing that steps `Tick`s forward, one at a time, driven by whatever tick
  count the host's `TickAccumulator` handed it that iteration.
- **Ending auto-stop is entirely tick/bar-count driven**, two call sites:
  `Arranger::on_tick`'s `section_end` branch
  (`components/core/arrangrr/include/arrangrr/arranger/arranger.hpp:544-554`)
  sets `result.stop_transport = true` the instant `section_is_ending(m_current)`
  and the section's own bar span (`m_section_start`/`transport_tick`, both
  `Tick`-domain, threaded via `m_transport.ticks_per_bar()`,
  `engine.hpp:816-829`) elapses — **the owner-locked comment at line 546-551
  is explicit that this one-shot is preserved unconditionally, checked FIRST,
  regardless of scene ownership; only the ending's LENGTH varies (style-authored
  `bars` vs. a SceneChain's own `hold_bars`), never whether it fires on
  wall-clock terms.** `Engine::fire_arranger` then converts `stop_transport`
  into a real `m_transport.stop()` + `kStop` MIDI + `kTransport` OutEvent
  (`engine.hpp:837-846`). The parallel SceneChain-driven path
  (`SceneChain::on_bar`, `components/core/arrangrr/include/arrangrr/scene/
  scene_chain.hpp:102-124`) counts `m_bars_elapsed < hold_bars` — again a pure
  bar count, no time. Golden proof: `test_scene_ending_stops_transport_at_its_
  own_hold_bars` (`components/core/arrangrr/tests/test_scene.cpp:291-322`) and
  `test_ending_stops_transport`
  (`components/core/arrangrr/tests/test_arranger.cpp:661-677`) both drive the
  stop purely via `b.advance(N * kTicksPerBar)`.
- **Host already mirrors per-section bar length:** `preview::section_bars(
  style_index, section)` (`apps/gui-sonotron/src/preview.hpp:119-148`,
  `preview.cpp:220`) is a "hand-copied-mirror of `arrangrr::StyleSection::
  bars`" already consumed by `grid_panel.cpp`'s `active_style_section_bars()`
  for the Repeat-Zone playhead sweep — i.e. the host **already knows** how
  many bars `kEnding1` spans for the active style, without any new ABI read.
  The host also already gets `OutEvent::kSection`
  (`abi.hpp:525`, fires on entry to the ending) and `OutEvent::kBeat`
  (`abi.hpp:531-539`, fires once per 24-PPQN pulse — 40 core ticks — with
  bar/beat/pulse fields) to track position inside it.
- **ABI struct budget:** `Command` is `op/boundary/param/idx/n_bars/a/b/c`,
  `static_assert(sizeof(Command) <= 20)` (`abi.hpp:466-482`); `OutEvent` is
  capped at 16 bytes (`abi.hpp:734`). `Param::kTransportTempo = 1` already
  spends one `int32 a` slot on `bpm_x100`; the enum has two more `int32`
  general slots (`b`, `c`) plus the already-shipped `Boundary` quantize field
  free for a ramp variant.
- **Dual-target gate:** `tests/arm-smoke/main.cpp:16-38` directly instantiates
  and exercises `TickAccumulator` (`acc.advance_us(1'000'000) == 1920`) inside
  the freestanding arm-none-eabi link/behavior gate — any core-side tempo
  primitive must keep this green and earn an equivalent assertion there.

## 2. L'architettura com'è costruita (per axis)

**Boundaries.** The tempo write path already crosses the ABI cleanly in one
direction (host command → core `Transport::set_bpm`); nothing about the
current design blocks either arm. The clock-generation duty, however, is
*not* a core responsibility today — `TickAccumulator` is a core-defined
*type* but every *instance* and every call site is host-owned, duplicated
three times verbatim. That is the load-bearing fact for both arms: the core
never drives its own realtime clock; it is always pulled forward by ticks a
host loop computes and hands it.

**Coupling/coesione.** The ending auto-stop mechanism (`Arranger::on_tick` +
`SceneChain::on_bar`) is cleanly decoupled from tempo by construction — both
only ever compare `Tick`/bar counts, never wall-clock or bpm. This is a
genuine structural strength for this fork specifically: whichever arm is
chosen, the stop-timing logic needs **zero changes** and cannot regress.

**Astrazioni.** `TickAccumulator` is a well-formed abstraction (drift-free,
integer, `constexpr`, freestanding-safe) but it is under-owned: there is no
single "the clock" component, only three copy-pasted call sites that happen
to stay byte-identical by discipline, not by a shared abstraction. A
HOST-NUDGE ritardando does not need to fix this, but a CORE ritardando that
wants the interpolation to live *inside* `TickAccumulator`/`Transport` would
be the first real pressure to consolidate the three driver loops — worth
flagging, not required.

**ABI stability.** Genuinely additive-friendly right now: v1 is explicitly
unfrozen for Phase 5, `Command` has spare `int32` capacity, and the existing
`kTransportTempo` opcode is a template to extend rather than replace. A new
opcode (e.g. `Param::kTransportTempoRamp`, next free id) reusing `a` =
target `bpm_x100` and `b` = ramp length (in ticks, or bars via `n_bars` +
`Boundary`) costs **zero bytes** of struct growth.

**Giunti dual-target.** Both arms are constrained by the same D32 rule (no
float near the realtime path) and the same arm-smoke gate. The two arms
differ sharply in *where* the interpolation math has to live and therefore
in dual-target blast radius — see §3.

**Layering/deriva.** No drift to report here: nothing in `docs/DESIGN.md`
locks a tempo-curve decision one way or the other. This is a fresh feature,
not a repaired boundary.

## 3. The two arms

### Arm A — HOST-NUDGE (cheap, host-only, zero-ABI)

**Mechanism.** The GUI already has every piece it needs, cited above:
`preview::section_bars(style, kEnding1)` gives the ending's length in bars;
`OutEvent::kSection`/`kBeat` give live position inside it; `Shell::cmd_bpm`
("`bpm <value>`") / `Shell::cmd_transport`'s `tempo` verb give a write path
that already exists and is exercised (`test_shell_bpm_command`,
`components/platform/hostrt/tests/test_host.cpp:155-184`). A host-side
ritardando authoring layer (new gui-sonotron logic, not core) would: detect
entry into `kEnding1` via the existing `kSection` OutEvent, read the ending's
bar length via the already-mirrored `section_bars()`, and on each subsequent
`kBeat` (or even each drained loop tick, `in_process_brain_session.cpp:1495`
onward runs every ~500 µs) push a fresh `transport tempo <bpm>` command that
steps the bpm down along a curve the GUI computes itself (linear, or an
easing curve — this is host-only arithmetic, float is fine here).

**Granularity.** `OutEvent::kBeat` fires once per 24-PPQN pulse — 40 core
ticks, `kMidiClockDivider` (`common/time.hpp:13`) — so up to 24 steps per
beat are available "for free" off an event the host already receives; the
engine-thread loop itself drains commands every ~500 µs
(`in_process_brain_session.cpp:1574`, the sleep_for cadence), so an even
finer step function is achievable by a host timer independent of `kBeat`.
For a 2-bar/4-beat-per-bar ending that is up to ~190 discrete tempo writes
across the ramp — comfortably smooth for a ritardando; commercial arranger
keyboards use far coarser steps than this.

**Smoothness ceiling / glitch risk.** None, structurally. `TickAccumulator::
advance_us` (`common/time.hpp:89-94`) recomputes ticks-per-microsecond fresh
from whatever `m_bpm` is current at call time — it is not a lookahead
scheduler and holds no baked future deadlines. `OutScheduler` (`out_scheduler.
hpp`) schedules every pending note-off by absolute `Tick`, never wall-clock,
so a live bpm write never needs to re-quantize anything already in flight —
those events simply arrive sooner or later in wall time, exactly as intended
for a tempo change. The `acc.set_bpm(shell.engine().transport().bpm())` line
already re-reads live every iteration
(`in_process_brain_session.cpp:1539`) — the plumbing this arm depends on is
**already proven in production**, today, for ordinary manual tempo changes;
a ritardando is just many small writes of the same verb instead of one.

**Cost.** New GUI-only code: a small ramp-authoring state machine (start bpm,
target bpm, bars remaining, curve shape) plus wiring it to the existing
`kSection`/`kBeat` events and the existing `bpm`/`transport tempo` write path.
No ABI change, no core change, no firmware/arm-smoke impact. Testable
entirely with the existing UI-automation harness
(`docs/proposals` conventions already used by e.g.
`test_transport_ending_button_ui_automation.cpp`) plus ordinary GUI unit
tests on the ramp math. **Cheapest and lowest-risk arm by a wide margin.**

**Structural caveat.** This arm is real-time-authored per host process — the
ramp only exists inside whichever GUI/host loop implements it. `cli-arrangrr`
and `sonotron-server` would each need the *same* authoring layer reimplemented
if the owner ever wants a ritardando there too (their `TickAccumulator`
re-sync is already present and would pick up the writes for free — only the
*authoring* logic is gui-sonotron-specific and would need duplicating, same
shape as the existing 3-way `acc.set_bpm(...)` duplication flagged in §1/§5).

### Arm B — CORE primitive (costly)

**Mechanism.** A per-section (or per-ending) ritardando descriptor —
target bpm + ramp length + curve shape — interpreted by `Transport` (or a new
small owned member) so that `bpm()` becomes a function of the current tick
rather than a stored scalar, and the host driver loops call that function
instead of the flat `bpm()` getter (a one-line change at each of the three
`acc.set_bpm(...)` call sites).

**ABI cost.** Cheap, contrary to first impression: additive-only fits.
`Param::kTransportTempo`'s pattern (`a` = target bpm_x100) extends directly
to a new `Param::kTransportTempoRamp` reusing `a` = target bpm_x100 and
`b` = ramp length (ticks, or bars via the already-shipped `n_bars`/`Boundary`
quantize fields, `abi.hpp:466-482`) — `sizeof(Command)` stays 20, no
`OutEvent` growth needed unless the host wants a live "ramp active" echo
(optional, and would ride the already-existing `kSection`-style echo pattern,
`abi.hpp:540-549`, not a new `Kind`). The real cost is NOT the wire format.

**Real cost — the interpolation engine.** `Transport`/`TickAccumulator` are
`constexpr`, `noexcept`, integer-only by house law (D32: "no float anywhere
near the realtime path", `common/time.hpp:6`). A ritardando needs a smooth
curve (typically exponential/S-shaped, not linear, for a musically credible
ritardando) computed in fixed-point integer arithmetic, driven per-tick or
per-call, with new state (`start_bpm`, `target_bpm`, `ramp_ticks_total`,
`ramp_ticks_elapsed` or an anchor tick) added to a type that today holds a
single `BpmX100` scalar. This is genuinely new core algorithm work, not
plumbing — and it must be proven byte-identical/deterministic (goldens) the
same way every other Phase-7 tick-domain change in this tree has been
(see the `Transport::advance_bar_tick`/`m_next_bar_tick` re-anchoring
precedent, `transport.hpp:64-96`, for the size of comment/reasoning a change
to this class typically requires here).

**Firmware/dual-target cost.** Must stay freestanding and pass
`tests/arm-smoke` (`main.cpp:16-38` already directly exercises
`TickAccumulator`); a ramp-aware `bpm()` needs its own arm-smoke assertion
alongside the existing `acc.advance_us(1'000'000) == 1920` check. Real but
bounded — the class already lives in the freestanding-verified `common/`
layer, so this is "extend an already-gated primitive," not "open a new
dual-target seam."

**Testing burden.** New unit coverage for the ramp math itself (start/mid/end
values, clamping, a ramp that's still active when `set_bpm` is called
manually mid-ramp — an ownership question the design must answer explicitly:
does a manual tempo override cancel an active ramp?), plus regression proof
that `Arranger::on_tick`'s existing bar-count stop logic is untouched (it
should be, since it reads `Tick`/bar counts, never bpm — but that
independence is exactly the kind of implicit invariant that deserves an
explicit test once a second bpm-mutating code path exists inside core).
Existing golden performance-wire tests (`test_performance_wire.cpp`,
`sizeof` assertions) would need a matching entry if the ramp state is
persisted in `Performance`/style data rather than transient.

**Where would the descriptor live?** Two sub-options, both NEEDS-DECISION:
(a) transient, engine-owned ramp state armed by a `kTransportTempoRamp`
command the SAME host authoring layer from Arm A would issue once instead of
per-step (moves Arm A's math into core, host now sends one command instead of
~190); or (b) authored per-style, a new field on `SectionDef`/`StylePattern`
(`style_model.hpp`) so an ending's ritardando ships as part of the style data
itself, alongside `.bars`. Option (b) is materially bigger: it touches the
style schema, `arrstyle-converter`'s model (`apps/tools/arrstyle-converter/
src/model.hpp`), all 16 in-tree style files, and the golden regeneration
pipeline that already exists for style changes (`benedetto-golden-hand`'s
mandate) — a genuinely cross-cutting change, not a contained one.

## 4. Interaction with the existing ending auto-stop

**Answer: none, by construction, for either arm**, and this is the single
strongest piece of evidence in this analysis. `Arranger::on_tick`'s
`section_end` branch and `SceneChain::on_bar` both fire purely off `Tick`/bar
counts (`arranger.hpp:544-554`, `scene_chain.hpp:102-124`) — never off bpm or
wall-clock elapsed time. A ritardando, by definition, only changes how many
*microseconds* a given number of *ticks* takes to elapse; it does not change
the tick count itself. The ending will still stop after exactly the same
number of bars it stops after today, just more slowly in wall-clock terms —
which is precisely the desired musical effect, and requires zero change to
the stop-timing code under either arm. The owner-locked comment at
`arranger.hpp:546-551` ("checked FIRST, unconditionally... only its LENGTH
changes") already establishes bar-count as the sole timing authority; a
ritardando does not compete with that authority, it operates one layer
below it (tick-to-wall-clock mapping), so there is no race, no double-stop,
and no need to touch the scene-hold suppression logic at all.

One second-order note, applying equally to both arms: **any host-side wall-
clock ETA/countdown UI** (if one exists or is ever added — none was found
wired to bar-remaining today) would need to account for a live bpm curve to
stay accurate; this is a cosmetic host concern, not a scheduling correctness
one, and does not change the recommendation.

## 5. Proposte strutturali

1. **Ship the ritardando as HOST-NUDGE, gui-sonotron-scoped, Phase 1.**
   Feasibility: **SHIPPABLE**. Dependency: **none new** — reuses
   `Shell::cmd_bpm`/`cmd_transport`'s existing `tempo` verb, `preview::
   section_bars`, and `OutEvent::kSection`/`kBeat`, all already in tree and
   already tested. Zero ABI change, zero core change, zero firmware impact.

2. **Defer the CORE ritardando primitive**, explicitly, as a follow-up ONLY
   if Arm A's step resolution or per-process duplication (cli-arrangrr,
   sonotron-server) becomes a real product need. Feasibility:
   **NEEDS-DECISION** — both the interpolation-math design (integer curve
   shape) and the descriptor placement (transient command-armed vs.
   per-style-authored, §3 Arm B) are genuine owner-level forks, not
   implementation details; do not let an implementor choose silently between
   them.

3. **Flag, don't fix, the three-way `TickAccumulator` re-sync duplication**
   (`in_process_brain_session.cpp:1539`, `cli-arrangrr/main.cpp:692`,
   `sonotron-server/main.cpp:331`) as a pre-existing structural note, not a
   blocker for this fork. It does not need to move for Arm A to ship. It
   becomes worth consolidating the day Arm B is chosen, since a ramp-aware
   `bpm()` is exactly the kind of logic three copy-pasted call sites would
   otherwise re-diverge on.

## 6. Cosa ho flaggato / cosa decide il proprietario

- **No new dependency required by either arm** — nothing flagged on the
  CLI-deps axis.
- **Owner decision, only if Arm B is ever chosen:** curve shape (linear vs.
  exponential/S-curve) and fixed-point representation for it under D32; and
  descriptor placement (transient command vs. per-style-authored field,
  §3's (a)/(b)) — both change the shape of the eventual core change
  materially and are not mine to pick.
- **Owner decision, only if Arm B is ever chosen:** does a manual `bpm`
  command mid-ramp cancel the ramp, override it, or queue after it? Today's
  `set_bpm` has no concept of "in progress," so this ownership question does
  not exist yet and must be answered explicitly before any core work starts.
- Nothing in `docs/DESIGN.md` locks a decision this fork would drift from —
  this is new ground, not a repair.

---

## Orchestrator-relayable summary

**Arm A — HOST-NUDGE.** The GUI already has everything it needs to fake a
ritardando cheaply: it already mirrors each style's ending length
(`preview::section_bars`), already receives per-pulse position events
(`OutEvent::kBeat`, 24 steps/beat), and already has a proven, live tempo-write
path (`Shell::cmd_bpm`/`transport tempo`) that a separate driver loop re-reads
every iteration with zero lookahead — so writing many small tempo steps
across the ending's bars is architecturally glitch-free (ticks are scheduled
in tick-space, not wall-clock, so nothing needs re-quantizing) and needs no
core or ABI change at all, only new gui-sonotron-side ramp-authoring logic.

**Arm B — CORE primitive.** The ABI side is cheap (additive `Param` +
existing spare `Command` int32 slots, sizeof unchanged), but the real cost is
a genuinely new interpolation engine inside `Transport`/`TickAccumulator`
under the no-float/no-heap/freestanding discipline, wired through all three
duplicated host driver loops, gated by `tests/arm-smoke`, and forced to
answer two un-made design calls (curve shape, and where the descriptor lives
— transient vs. per-style-authored, the latter touching all 16 style files
and the golden pipeline).

**Recommendation: ship Arm A (HOST-NUDGE) as Phase 1; defer Arm B.** The
ending auto-stop is bar-count-driven and provably indifferent to either arm
(no timing race, no code touched), so there is no correctness reason to
prefer the core route. Arm A is materially cheaper, zero-ABI, and already
proven-in-production plumbing; Arm B only earns its cost if the owner wants
the ritardando authored *in the style data itself* or shared across
cli-arrangrr/sonotron-server without reimplementing the authoring layer per
host.

**The decision only the owner can make:** does the ritardando need to be a
first-class, style-authorable musical primitive that ships identically
across every host front-end (→ Arm B, and then which curve shape and which
descriptor placement) — or is "gui-sonotron fakes it convincingly" the actual
bar for Phase 1 (→ Arm A, ship now)? Nothing in the traced code forces this
choice technically; it is a product-scope call.

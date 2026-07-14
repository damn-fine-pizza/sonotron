# The `Insert` interface fork + `5210`/`5220` as-built grounding (Phase-6 Theme 4, node `5210`/`5220`)

Corelli, DESIGN (2026-07-14). Pre-code architecture/design review, no product
code written or edited by this review. Companion piece to `docs/phase6-plan.md`
Theme 4's own scope note ("The biggest architectural fork — do it deliberately,
last"), kept as a new `docs/reflections/` file per this reviewer's
read-only-on-existing-docs discipline (mirrors `docs/reflections/phase6-theme3-
master-transpose-scope.md` and `docs/reflections/phase6-theme3-performance-
format-v2-review.md`'s own precedent). Feeds an owner decision, then an
implementation brief for `nazzareno-cpp-implementor`.

## What was asked

Decide the `Insert` interface fork (pure stream-transform vs a capability-
probed optional `on_tick` hook) — this fixes the signature for all eight
per-role chain slots and must be settled before `5210` (groove-as-insert) or
`5220` (arp-as-insert) can be implemented. Ground every claim in file:line;
this document does not propose product code, only the structural shape and
the fork table the owner must close.

## Cosa ho tracciato

**Decisions read:** `docs/DESIGN.md` node `0800`/D4 (dependency-free core,
line 706), node `0100`/D16 (determinism, line 678), `0200`/D32 (zero dynamic
allocation, compile-time-first, "virtual only at HAL boundaries", lines
579-580), `0400`/`12200`/D33 (≤512 KB envelope, `static_assert`-verified, line
587), node `3120`/D40 (resolution pipeline gather→expand→resolve→voice→groove,
line 804), node `2510`/`2520`/D53 (bar-boundary ordering precedent, lines
785-789), node `5000`/`5100`/`5200` (the chain framework's shipped/planned
state, lines 840-864; `5100` "core shipped… grafted ahead of `groove::apply`
with a per-fan-out-note grid recompute", line 852; `5200` "arp/groove/scale-
lock become INSTANCES of the chain, not disconnected modules", line 843; `5210`
`5220` `○ SHIPPABLE`, lines 857-858), node `7100`/`7130` (arp as a track
MIDI-FX, `= 5220`, line 881). Also the **prior** Corelli pre-code review of
this exact fork, `docs/phase5-design-reviews.md` lines 384-582 ("MIDI-FX /
Transform chain (#10, Item H)") — written before `5100` had any code, now
checked against what actually shipped.

**Code traced (include/call graph, not recalled), post-`5100` (shipped):**
- `components/arrangrr/include/arrangrr/fx/insert_chain.hpp` (full read):
  `InsertType` enum (32-37, 4 values), `Insert{type, enabled, params}` = 6 B
  (`static_assert`, line 104, "Insert RAM budget pin (D33)"); `Insert::Params`
  union (89-95) holds `ScaleLockParams`/`VelocityProcParams`/`EchoParams`/
  `NoteRepeatParams`, each exactly 4 B, flat, no internal padding; `Insert::
  process(in, out, max_out, ctx)` (240-253) — one call, synchronous, dispatch
  by `switch(type)`, no state carried between calls; `FxContext{key, chord,
  step, tick}` (120-125) — **no `role` field**; `FxNote{note, vel, gate,
  offset}` (108-113) — **no micro-timing field** distinct from `offset`;
  `InsertChain::apply` (410-451) — fixed `kMaxChainFan=8` fan buffer, runs
  every enabled slot in FIXED array order, no notion of "this slot fires on
  every tick regardless of input."
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp:366-560`
  (`Arranger::on_tick`, full read): the D40 pipeline is `gather (486-510) →
  m_voicing.voice() (515) → InsertChain::apply per resolved note (530-532) →
  per-FANNED-note groove::apply (549) → schedule() ×2 (551-555)` — **all
  inside** `for (const NoteReq& nr : group)` (518), itself inside `if (count ==
  0) continue;` (511) gated on `source` events whose `ev.step == step` (486-
  489, current tick's grid slot only). Groove is called with `step_j`/`tick_j`
  RECOMPUTED from `fn.offset` (545-548) — the graft comment (538-544) states
  the rule explicitly: "groove is computed at THIS note's OWN grid position…
  never the seed's." `groove::apply`'s `timing_offset` is added ONLY at the
  final `schedule()` call (550) — it is never folded back into `fn.offset` or
  re-read by anything downstream, because nothing runs downstream of it today.
- `components/arrangrr/include/arrangrr/arranger/groove.hpp` (full read):
  `GrooveParams{swing, humanize_timing, humanize_velocity, accent, swing_grid,
  quantize}` (6×`u8`) `+ seed(u32)` (21-29) — 6 B of `u8` then a 4-byte-
  aligned `u32`, i.e. **12 B with 2 B of implicit padding**; `groove::apply`
  (65-114) is a pure function of `(params, role, step, tick, base_vel)` — no
  history, no state, `constexpr`. `Arranger::m_groove` is a **single global
  field** (arranger.hpp:131/135), not per-role — "one global groove feel
  applied to every arranger part" (groove.hpp:19-20).
- `components/arrangrr/include/arrangrr/arp/arpeggiator.hpp` (full read):
  `ArpeggiatorParams{rate, direction, octaves, gate, latch, seed}` (57-64) —
  same shape as `GrooveParams`, also **12 B with padding** (4 `u8` + `bool` +
  `u32`); `ArpeggiatorEngine` holds LIVE SESSION state beyond params:
  `m_notes[8]`/`m_vels[8]`/`m_count`/`m_physical[2]` (`u64`×2)/`m_step` (312-
  317) — roughly 45-48 B of runtime, non-wire, held-note bookkeeping.
  `on_tick(transport_tick, emit)` (166-190) fires **unconditionally on every
  transport tick** that lands on its own rate grid (`transport_tick %
  step_ticks`), independent of any single input note — it emits **at most one
  note per call** (188, no fan-out).
- `components/arrangrr/include/arrangrr/engine.hpp`: `m_arp` is a **single,
  Engine-global** `ArpeggiatorEngine` (211-212), fed by `observe_arp_input`
  (645-651, called from `push_midi_in`'s live-keyboard capture, 160-192) and
  fired by `fire_arp` (655-664) — **outside** the Arranger entirely.
  `Engine::on_tick` (274-319) D53 order: `fire_timeline → fire_chord_seq →
  [bar boundary: commit_bar → fire_clips → apply_pending_performance_recall →
  apply_pending_pad_fires] → fire_arranger (317) → fire_arp (318)` — `fire_arp`
  runs once per tick, AFTER the whole arranger tick (chain+groove included),
  not interleaved with any per-role loop.
- `components/arrangrr/include/arrangrr/perf/performance.hpp`: `PerfInsert
  {type, enabled, params[4]}` = 6 B (`static_assert` line 70, "PerfInsert
  mirrors Insert's own 6 B budget pin (D33)") — its own header comment (53-63)
  is the load-bearing citation for this review: *"`params` is a RAW 4-byte
  copy of `Insert::Params`' union bytes… this is safe and portable:
  `Insert::Params` is itself a flat, padding-free union of 4-byte PODs
  (`insert_chain.hpp`'s own `static_assert(sizeof(Insert) == 6)` proves it)…
  **unlike `GrooveParams`' padding gap** that motivates every OTHER field's
  explicit put/get walk below."* `insert_chains[10][8]` (115) costs 480 B of
  `Performance`'s already-shipped 576 B record (`sizeof(Performance) == 576`,
  line 126, `format_version 2`, line 155) — landed via commit `d242348`
  (`docs/phase6-plan.md` line 83-87), **the same phase, same day** as this
  Theme 4 review. No live/session arp state is captured anywhere in
  `Performance` today (grep of `capture_performance`/`apply_performance` for
  `arp` returns nothing) — only `groove` (a config) is captured, never `m_arp`.
- `docs/phase5-design-reviews.md:384-582` — the PRE-`5100` review of this same
  fork: recommended per-ROLE addressing (adopted, confirmed in `arranger.hpp`
  above), predicted bit-identity is reachable "IF AND ONLY IF… the chain grafts
  at the same kernel point where `groove::apply()` lives today" (496-511,
  confirmed true of the shipped `5100` code), and left the `Insert` interface
  fork and arp instancing genuinely open (513-532) — this document is the
  as-built continuation of exactly those two open items.
- `tests/golden/*.acmd` — no golden combines an `fx set …` chain configuration
  with non-default `GrooveParams` (grep across `tests/golden/*.acmd` for
  `fx `/`groove ` in the same file: no hits); `feel_swing.acmd`/`feel_blues
  .acmd`/`feel_shuffle.acmd` pin non-trivial groove math with NO chain
  configured; `arranger_band.golden`/`arranger_gesture.golden`/`arranger_
  voicing.golden`/`mute_solo.golden`/`polymeter.golden`/`minor_v.golden`/
  `hello_chord.golden`/`chord_*.golden`/`smart_richness.golden`/`seq_*
  .golden`/`clip_launch.golden` all route through `Arranger::on_tick`'s
  unconditional `groove::apply` call with default (all-zero, provably
  identity per `test_groove_apply`'s first assertion, `test_arranger.cpp:264-
  267`) `GrooveParams`.

## L'architettura com'è costruita

**Confini/coupling.** `Insert`/`InsertChain`/`FxNote`/`FxContext` are core,
freestanding, `constexpr`-first, no heap — a clean example of D32 discipline.
`groove::apply` and `ArpeggiatorEngine` are equally core/dual-target, equally
clean in isolation. The coupling problem this review surfaces is not a leak
across the core/host seam; it is that **two independently-clean, pure
subsystems (groove, arp) sit at structurally incompatible POSITIONS relative
to the chain's own graft point**, and Theme 4 asks to fold both into ONE
interface anyway.

**Astrazione — the chain's contract is "ingest-and-emit-in-one-call," and
neither groove nor arp is symmetric with that contract.** `Insert::process`
is a pure function: one note in, N notes out, same call, same instant
(`insert_chain.hpp:240-253`). Groove (as it exists today) fits this shape
perfectly — `groove::apply` is equally a pure, single-call function of
`(params, role, step, tick, vel)` with **no session state**, so it is not
"stateful" in the sense the task brief worried about; it is merely called
**at the wrong granularity** for the chain today (once per fanned note,
outside the chain, not once per chain slot). Arp does NOT fit the contract at
all, for a reason distinct from statefulness: its `on_tick` fires on ticks
where **no resolved input note exists at all** (`ArpeggiatorEngine::on_tick`
runs unconditionally on its own rate grid; `InsertChain::apply` only runs
inside `Arranger::on_tick`'s `if (count == 0) continue`-gated, per-active-step
loop, `arranger.hpp:511`,`518`). An arp cannot be reached by ANY per-note
process()-shaped call, because the tick on which it must fire may not be a
tick on which any note was gathered.

**Stabilità dell'ABI — Theme 3 already spent the `Insert` size budget, TODAY.**
`static_assert(sizeof(Insert) == 6)` (`insert_chain.hpp:104`) and
`static_assert(sizeof(PerfInsert) == 6)` (`performance.hpp:70`) are two
independent, already-shipped pins on the SAME number, and `PerfInsert`'s own
header comment explicitly credits `Insert::Params`' "flat, padding-free
union of 4-byte PODs" as the reason the raw-byte-copy wire strategy is safe —
**contrasting it, by name, against `GrooveParams`' own padding gap**
(`performance.hpp:53-63`). `GrooveParams` (12 B) and `ArpeggiatorParams`
(12 B) both blow the 4 B `Insert::Params` union budget roughly 3×; adding
either as a literal union member grows `Insert` from 6 B to 16 B (alignment
forces the union to start at offset 4, not 2), which breaks BOTH
`static_assert`s and grows `Performance::insert_chains[10][8]` from 480 B to
1280 B — a real wire-format break on a record that bumped to `format_version
2` (commit `d242348`) in the SAME phase, arguably the same day, as this
review. This is not a hypothetical cost; it is the concrete number the owner
is deciding against.

**Giunto dual-target.** Nothing proposed anywhere in this fork crosses core
into host-only territory — `Insert`, `FxNote`, `FxContext`, `groove::apply`,
`ArpeggiatorEngine` all stay freestanding, no-heap, no exceptions/RTTI,
consistent with `components/arrangrr/CMakeLists.txt`'s PUBLIC compile flags
(cited in `docs/phase6-design-reviews.md` line 9-11). The RAM question above
is a D33 budget question, not a target-boundary question.

**Layering — `5200`'s "arp/groove/scale-lock become INSTANCES of the chain"
(`0600`, `DESIGN.md:843`) is truer for groove than for arp, and the plan does
not currently say so.** Groove genuinely IS one more pure per-note transform
misplaced one call-site away from the chain. Arp is not a transform at all —
it is a second, independent clock source that happens to also want to emit
notes into the same role's output stream. Treating both as "the same kind of
`5200` refactor" is the drift this review names below.

## Fork 1 — the `Insert` interface: option 1 vs option 2

**Option 1 (pure stream-transform, current shape unchanged).** `int
process(const FxNote&, FxNote*, int, const FxContext&) const noexcept` for
every slot, unchanged. Groove COULD be added as a 5th `InsertType` under this
option (it is pure — see above), but arp CANNOT: there is no way to express
"fire on ticks the chain's own gated loop never visits" inside a function
that is only ever called from inside that gated loop. Option 1 therefore
does not merely "not support" arp — it **structurally forecloses** `5220`
regardless of how `Insert::process`'s body is written.

**Option 2 (capability-probed optional `on_tick` hook).** The task frames
this via `pipeline.hpp`'s `if constexpr (requires {...})` SFINAE idiom
(cited approvingly by `docs/phase5-design-reviews.md:556-568`). That
citation needs a precision correction: `pipeline.hpp`'s capability probing
works because `Pipeline<StageTs...>` composes **heterogeneous, distinct C++
types** at compile time — SFINAE detects "does the concrete stage type NAME
this method." `Insert` is the opposite shape: **one concrete type**, `type`
is a **runtime** tag, dispatch is a `switch`. "Capability-probed" for `Insert`
cannot mean the same compile-time mechanism; it means a `switch`-dispatched
method that is a real no-op for the five stateless types and does real work
only for the arp-typed slot — the INTENT phase5's review wanted ("most
inserts stay pure, only the ones that need time implement it") is preservable,
the literal SFINAE mechanism is not, because the type is not heterogeneous.

**The concrete signature this locks, and the graft-point widening it forces.**
A slot that hosts a time-aware effect needs to separate INGEST from EMIT —
`process()` cannot do both for arp, because arp's emission is decoupled from
any one input note:

```cpp
// Existing, unchanged for the 5 stateless types (ScaleLock/VelocityProc/
// Echo/NoteRepeat/Groove-as-insert): one call, in -> N out, no history.
int process(const FxNote& in, FxNote* out, int max_out,
            const FxContext& ctx) const noexcept;

// NEW, optional: for a resolved note arriving THIS step, feed it into the
// slot's own held-state instead of emitting synchronously (arp's note_on
// equivalent). No-op (return without touching state) for the 5 stateless
// types.
void ingest(const FxNote& in, const FxContext& ctx) noexcept;

// NEW, optional: called ONCE PER TICK, PER ROLE, UNCONDITIONALLY --
// including ticks where the pattern grid produced no event at all -- from a
// NEW pass in Arranger::on_tick that is NOT nested inside the existing
// `if (count == 0) continue` gated per-step loop (arranger.hpp:511,518).
// No-op for the 5 stateless types (the switch short-circuits before doing
// any work). Returns 0..N notes ready to schedule, same FxNote/max_out/ctx
// shape as process() for symmetry.
int on_tick(Tick transport_tick, FxNote* out, int max_out,
            const FxContext& ctx) noexcept;
```

**Cost, quantified, dual-target.** The `Insert::Params` union (the WIRE-
persisted, 4 B, D33-pinned part) must NOT hold `ArpeggiatorParams`/
`GrooveParams` directly (Fork "Stabilità dell'ABI" above). The runtime
SESSION state (`m_notes[8]`/`m_vels[8]`/`m_count`/`m_physical[2]`/`m_step`,
~40 B) must live in a **separate, non-wire, per-role runtime array** — e.g.
`ArpeggiatorEngine m_role_arp[kRoleCount]` living beside (not inside)
`InsertChain m_chain[kRoleCount]` (`arranger.hpp:662`). At `kRoleCount=10`:
`10 × ~48 B ≈ 480 B` of NEW static/stack RAM (matches
`docs/phase5-design-reviews.md:544-554`'s own prior estimate) — negligible
against the `≤512 KB` D33 envelope, and it is fixed-capacity, no-heap,
freestanding by construction (`ArpeggiatorEngine` already is). This state is
**session-only** (mirrors the existing precedent that `Performance` does not
capture the live `m_arp`'s held notes either — only config would be
captured, if at all).

**Recommendation.** Adopt option 2, narrowly: add `ingest`/`on_tick` as
optional, `switch`-dispatched, no-op-by-default capabilities on `Insert`, NOT
because groove needs them (it does not — see Fork 2) but because arp
genuinely cannot be hosted any other way. Pay the graft-point widening in
`Arranger::on_tick` once (a new, ungated, once-per-role-per-tick pass), not
per-insert-type. Keep `Insert::Params`' wire shape at 4 B / `sizeof(Insert)
== 6` unchanged — route any arp-typed slot's CONFIG through a small,
explicit, ≤4 B subset (see Fork 3) and its SESSION through the separate
runtime array, never through the union. **NEEDS-DECISION**: the owner must
accept that arp-typed slots are a structurally different citizen of the
chain than the other five types (config in the union, session outside it,
emission on a second call site) — an honest asymmetry, not a uniform
interface, however the method names are spelled.

## Fork 2 — `5210` groove-as-insert: the bit-identity verdict

**Groove does not need `on_tick`; it needs `role` added to `FxContext` and a
carry field added to `FxNote`.** `groove::apply` is pure and single-call
(above); it fits `Insert::process` if two small, mechanical gaps are closed:
(1) `FxContext` (`insert_chain.hpp:120-125`) has no `role` field —
`groove::apply` needs it for its position hash (`groove.hpp:52-60,92`); (2)
`FxNote` (`insert_chain.hpp:108-113`) has no field to carry
`GrooveOut::timing_offset` forward to `schedule()` if Groove is not the
LAST-executed slot — today that value is added only at the final
`schedule()` call (`arranger.hpp:550`), never re-read. Both are additive,
zero-behavior-change-for-existing-types widenings (every existing `Insert`
type already copies `in` verbatim via `out[0] = in;`/`FxNote n = in;`, so a
new field free-rides through unchanged) — SHIPPABLE on their own.

**The real risk is not on/off correlation (the phase-5 review's original
worry) — it is FAN-ORDER dependency, and it is real.** The phase-5 review
flagged the pitch-overlap/on-off-correlation risk for a scenario that did NOT
end up shipping: a "true post-scheduling insert re-reading an event stream"
(`phase5-design-reviews.md:502-511`). `5100` shipped grafted INSIDE the same
loop instead, so that specific risk does not apply as originally framed. A
DIFFERENT, equally real risk takes its place: **today groove runs
unconditionally AFTER the entire chain has finished fanning** (`arranger.hpp:
532` chain, `545-549` groove, per FANNED note) — so an Echo/NoteRepeat copy N
steps out from its seed gets grooved at ITS OWN, correctly-offset grid
position. If Groove becomes a genuinely orderable, removable chain slot (the
actual product ask — "an instance of the chain," `DESIGN.md:843`), and a
role's chain places Groove BEFORE Echo/NoteRepeat, Echo's fanned copies would
never see groove applied to their own offset positions (groove already ran,
once, on the pre-fan seed) — a materially different musical result than
today for ANY role that combines groove with a fanning insert. No golden
today exercises this combination (`tests/golden/*.acmd` has zero files
combining `fx set` with non-default `GrooveParams` — confirmed above), so no
EXISTING golden fails on this specific edge; it is a real behavior change for
Theme 4's OWN target use case, and the goldens are silent on it because they
predate it.

**Two structurally honest resolutions, not a defect to quietly patch:**
1. **Pin Groove non-reorderable, always-effectively-last** (a real,
   documented exception to "the chain is a free ordering of instances") —
   preserves bit-identity for the feel-groove goldens by construction
   (unconditional default insertion, see below) AND preserves per-fanned-note
   grid correctness for free. Cost: `5210` becomes "groove is TOGGLEABLE and
   CONFIGURABLE as a slot, but not truly ORDERABLE relative to fanning
   inserts" — an honest, named asymmetry in the abstraction, not the free
   composability `0600`/`5200` promise.
2. **Make Groove genuinely orderable**, accept that Echo/NoteRepeat placed
   AFTER Groove in a chain get un-grooved (or re-groove-per-fan-copy via a
   second, explicit re-apply pass the chain would need to grow) — a real
   product/musical decision, not free, and NOT reachable without also solving
   how a fanning insert re-derives a "post-groove" grid position for each of
   ITS OWN copies (the exact machinery `RestyleStage::m_pending` needed for
   an unrelated reason, `phase5-design-reviews.md:403-409`).

**The zero-cost half of `5210`, spelled out plainly for the migration.**
Every existing golden's `GrooveParams` is either default (mathematically
identity, `test_groove_apply`'s first assertion) or non-default with NO chain
configured (`feel_swing`/`feel_blues`/`feel_shuffle`). Bit-identity for ALL
CURRENT goldens is reachable regardless of which resolution above is chosen,
PROVIDED the implementation auto-appends an enabled, default-parameterized
`kGroove` slot to every role's chain at construction time (mirroring how
`InsertChain::InsertChain()` already fills every slot to full capacity with
an inert default, `insert_chain.hpp:271-275`) rather than leaving groove
OPT-IN. An opt-in default would silently drop groove from every role that
does not explicitly configure it — breaking `feel_swing`/`feel_blues`/
`feel_shuffle` outright, not at an edge case. **Recommendation: resolution 1
(pin-last, auto-present) — SHIPPABLE, zero golden risk, zero owner review
burden on the existing corpus.** Resolution 2 is **NEEDS-DECISION** and
should not be adopted without an explicit, reviewed golden regeneration for
any new fixture that combines groove with a fanning insert.

## Fork 3 — `5220` arp-as-insert: instancing

**Per-role instances (one `ArpeggiatorEngine` per role), not per-slot, and
NOT literally inside `Insert::Params`.** `docs/phase5-design-reviews.md:516-
522`'s "path 1" (per-role array, engine-level, own `EmitFn` into the SAME
`OutScheduler`) is the shape this review confirms as-built evidence supports:
`kRoleCount=10` is already the ordinal space for `m_routes`/`m_chain`
(`arranger.hpp`), and one arp per role (not one per slot, of which there are
up to 8) matches product intent — a role plays ONE arpeggiated pattern, not
eight independently-clocked ones. A `kArp` `InsertType` still marks WHICH
slot activates the role's arp and carries its non-session config (rate/
direction/octaves/gate — 4 `u8`, exactly filling the existing 4 B `Params`
budget; `latch`/`seed` would need to be dropped from the PERSISTED,
wire-shape config or `Insert` accepts a size bump, an explicit owner
trade-off, not a default), while the actual `ArpeggiatorEngine` instance
lives in the new `m_role_arp[kRoleCount]` array (Fork 1's cost estimate).

**Data-flow inversion, not just an interface gap.** The live-keyboard arp's
"held notes" come from `note_on`/`note_off` MIDI pairs over an unbounded time
window (`observe_arp_input`, `engine.hpp:645-651`). A track-FX arp instead
needs its "held chord" derived from the ARRANGER'S OWN resolved
`NoteReq group` for that role/step (`arranger.hpp:459-513`) — which the
style-pattern model expresses as periodic, gated note EVENTS (a note-on with
a fixed `gate`), not as an open note-on/note-off session. Feeding a resolved
group into `Insert::ingest` (Fork 1) as a batched "these notes just started
sounding" signal is mechanically straightforward; there is no equally
natural "these notes just stopped" signal in the pattern model beyond the
event's own `gate` expiring — meaning the arp-insert's held-chord model would
most naturally be "replace the held set with whatever this step's resolved
group contains, hold until the next non-empty step for this role" rather than
a literal note_on/note_off mirror of the live-keyboard arp. **This is a
product-scope decision (how a style step's momentary chord becomes a
sustained arp source), not a structural one** — flagged, not resolved here.

**Ordering — no conflict, but two co-existing arp instances per role become
possible and untested.** `Engine::on_tick`'s D53 order keeps `fire_arp`
(global, live-keyboard) firing AFTER `fire_arranger` (`engine.hpp:317-318`)
unchanged; a role's new arp-insert fires FROM WITHIN `fire_arranger`
(the new ungated per-role-per-tick pass, Fork 1). These are two independent
`ArpeggiatorEngine` instances with independent note sources — no shared
state, no read/write race — but if BOTH are routed to the same output
port/channel, event ordering at a shared tick is a genuinely new combination
no golden exercises today. **NEEDS-DECISION/QA note for Torquato**, not a
structural defect.

**No re-opening of `5100`'s locked per-role addressing.** `kMaxInserts=8` is
the per-role SLOT cap (`abi.hpp:316`); adding `kGroove`/`kArp` as two more
selectable `InsertType` values is a pure append to a closed enum (matches
D26's append-only discipline, `abi.hpp:236-286`'s own `Param` numbering
precedent) — it does not touch `kRoleCount=10` or the "per-ROLE, not
per-Timeline-Track" decision (`DESIGN.md:846-850`). Confirmed non-drift on
that axis.

## Tabella dei fork per il proprietario

| # | Fork | Structural read | Recommendation | Label |
|---|---|---|---|---|
| 1 | `Insert` interface: option 1 (pure) vs option 2 (optional `ingest`/`on_tick`) | Option 1 structurally forecloses `5220`; groove alone would not need it | Option 2, narrow — no-op by default for 5/6 types | NEEDS-DECISION (fixes the signature for all 8 slots) |
| 2 | `Insert::Params` union growth (`GrooveParams`/`ArpeggiatorParams` = 12 B vs the 4 B, doubly-`static_assert`-pinned budget) | Breaks `sizeof(Insert)==6` AND `sizeof(PerfInsert)==6` (`insert_chain.hpp:104`, `performance.hpp:70`); forces a `Performance` v3 wire bump in the same phase v2 shipped | Do NOT put full config structs in the union — marker/slim-config only, session state lives outside the union | NEEDS-DECISION (real, dated cost if reopened) |
| 3 | Groove ordering: pinned-last (asymmetric but bit-identical) vs genuinely orderable (honest abstraction, golden risk) | No existing golden combines chain+groove; pinned-last preserves ALL current goldens for free | Pinned-last, auto-present default slot | SHIPPABLE (pinned-last) / NEEDS-DECISION (orderable) |
| 4 | Arp instancing: per-role `ArpeggiatorEngine` array vs literal per-slot union member | Session state (~48 B/role) cannot live in the 6 B wire slot regardless | Per-role array beside `m_chain`, config-only in the union | SHIPPABLE (structure) / NEEDS-DECISION (which config fields the union keeps: `latch`/`seed` cut or `Insert` grows) |
| 5 | Arp-insert's "held chord" source model (note_on/off mirror vs step-resolved-group replace) | Product-scope, not structural | — | NEEDS-DECISION (Ottorino/owner, product) |
| 6 | Dual-arp-instance output collision (global live arp + a role's arp-insert on the same port) | Untested combination, no structural conflict | Torquato QA coverage once implemented | NEEDS-DECISION (QA scope) |

## Proposte strutturali

**P1 — `Insert` gains `ingest`/`on_tick`, `switch`-dispatched, no-op default.
[NEEDS-DECISION, no new dependency]** Per Fork 1. Locks the signature for all
eight slots; the five existing stateless types are untouched (their `switch`
arms simply do not implement the new methods' bodies).

**P2 — `FxContext` gains `role`; `FxNote` gains a micro-timing carry field.
[SHIPPABLE, no new dependency]** Mechanical, additive, zero behavior change
for the four shipped insert types (all already copy `in` verbatim). Required
regardless of which Fork-2 resolution the owner picks.

**P3 — Groove-as-insert ships PINNED-LAST, auto-present. [SHIPPABLE, no new
dependency]** `InsertChain`'s constructor already fills every slot to
capacity with an inert default (`insert_chain.hpp:271-275`); extend that
precedent to guarantee a `kGroove` slot exists and stays logically last
regardless of the other 7 slots' configuration. Zero golden risk (traced
above).

**P4 — Arp-as-insert ships as `m_role_arp[kRoleCount]` beside `m_chain
[kRoleCount]`, config-only `kArp` marker in the union. [NEEDS-DECISION —
`latch`/`seed` scope, no new dependency]** Per Fork 3. The config-field cut
(`latch`/`seed` dropped from the wire-persisted slot, or `Insert` accepts a
size bump reopening P0 above) is an explicit owner call, not a default.

**P5 — `Arranger::on_tick` gains a new, ungated, once-per-role-per-tick pass
for `on_tick`-capable slots. [NEEDS-DECISION — widens the D40 kernel's own
shape, no new dependency]** Not a drop-in addition to `InsertChain::apply`'s
existing call site (`arranger.hpp:530-532`); a second traversal outside the
`if (count == 0) continue`-gated loop. Sizing/exact placement is
implementation work for `nazzareno-cpp-implementor`, but the STRUCTURAL fact
that `Arranger::on_tick` must grow a second per-role pass is itself an
owner-visible cost of adopting `5220`, not a hidden implementation detail.

## Cosa ho flaggato / cosa decide il proprietario

- **Fork 1 (the central ask).** Option 2, narrowly scoped, is my
  recommendation — but it is the owner's call because it fixes the signature
  for every slot, including the 4 shipped, currently-untouched types.
- **Fork 2's `Insert::Params` size question** is not a preference — it is a
  concrete, dated collision with a wire format (`Performance` v2) shipped the
  same phase as this review. I recommend AGAINST reopening it; if the owner
  wants full `GrooveParams`/`ArpeggiatorParams` fidelity inline in the
  per-slot config anyway, that is a real `Performance` v3 bump, costed
  honestly here, not something to fold in as a minor detail.
- **Fork 2's ordering resolution (pinned-last vs genuinely orderable)** is a
  product/musical call, not an engineering default — I recommend pinned-last
  for zero golden risk, but "groove is not really reorderable" is a real,
  visible asymmetry in the `5200` "instances of the chain" promise that the
  owner should accept explicitly, not discover later.
- **Fork 3's held-chord source model** (item 5 in the table) is Ottorino's
  and the owner's, not mine — I flag it because it changes what `Insert::
  ingest` receives, but the musical shape of "how a style step becomes an
  arp's held chord" is product scope.
- **No new dependency anywhere in this review.** Every proposal stays within
  already-present core vocabulary (`switch`-dispatched tagged unions,
  `StaticVector`, `constexpr` pure functions) and the already-established
  core/host split — no CLI-deps flag needed.
- **P5's kernel widening** is the one item I would want `nazzareno-cpp-
  implementor`'s brief to treat as real, scoped work, not a one-line
  addition — it changes `Arranger::on_tick`'s own shape, the single most
  heavily-commented, cognitive-complexity-suppressed function in the core
  (`arranger.hpp:360-365`).

## File toccati da questa review (solo lettura)

- `components/arrangrr/include/arrangrr/fx/insert_chain.hpp`
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp`
- `components/arrangrr/include/arrangrr/arranger/groove.hpp`
- `components/arrangrr/include/arrangrr/arp/arpeggiator.hpp`
- `components/arrangrr/include/arrangrr/engine.hpp`
- `components/arrangrr/include/arrangrr/perf/performance.hpp`
- `components/arrangrr/include/arrangrr/abi.hpp`
- `components/arrangrr/include/arrangrr/config.hpp`
- `components/arrangrr/src/engine.cpp`
- `components/arrangrr/tests/test_arranger.cpp`
- `tests/golden/*.acmd` / `tests/golden/*.golden`
- `docs/DESIGN.md`
- `docs/phase6-plan.md`
- `docs/phase6-design-reviews.md`
- `docs/phase5-design-reviews.md`

Nessun file di prodotto è stato modificato. Questo documento è l'unica
scrittura di questa review.

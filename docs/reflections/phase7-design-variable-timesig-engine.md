# Phase 7 design: the variable time-signature engine (node T0, prerequisite for `6000`/`8100`)

Status: DESIGN DOC (read-only architecture analysis). No product code touched.
Author: Corelli (architecture critic), structural design pass under an explicit
owner override.

**Owner override recorded for transparency.** The prior scope doc
(`docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md`, Fork D)
recommended DESCOPING variable time-signature for v1. The owner overrode that:
build the variable-time-signature engine now, as the prerequisite node "T0"
gating `6000` (looper records against bars) and `8100` (per-scene time-sig).
This doc designs T0 under the owner's locked constraints: ABI stability is
LIFTED for this scope (no size pins, no migrators); the freestanding/no-heap/
dual-target STM32H743 M7 realtime core stays HARD; the default-4/4 path must
stay golden byte-identical until a style/scene genuinely selects a non-4/4
signature.

---

## 1. Site inventory — the real production graph

Grepped tree-wide for `kTicksPerBar|kBeatsPerBar|kTicksPerBeat`, filtered to
non-test production files (`components/`, `apps/`). Fourteen matches remain
concentrated in **13 distinct production files**, matching the prior doc's
count exactly. They split cleanly along the dual-target boundary that
`CMakeLists.txt:38-45` (unconditional, cross-builds `arm-none-eabi` when
`CMAKE_SYSTEM_NAME=Generic`) vs. `CMakeLists.txt:59-83` (the `else()` branch —
host-only: `midisrc`, `orchestrator`, `hostrt`, `cli-arrangrr`,
`sonotron-server`, tests, tools, GUI) actually draws it. **Only
`components/common`, `components/runtime`, `components/chorddet`,
`components/arrangrr` cross-build the firmware target** — `components/hostrt`
itself is proven host-only by its own `CMakeLists.txt:44`:
`target_link_libraries(hostrt PUBLIC arrangrr runtime orchestrator
ALSA::ALSA)` — a hard host dependency (ALSA) that can never cross-build
`arm-none-eabi`. There is no `app/firmware/stub` directory in this tree; the
firmware side is `tests/arm-smoke` plus the four unconditional components
above.

### Definition site

- **`components/common/include/common/time.hpp:27-29`** — `kBeatsPerBar = 4`,
  `kTicksPerBeat = kPpqn` (960), `kTicksPerBar = kBeatsPerBar * kTicksPerBeat`
  (3840). The file's own comment (lines 22-26) is explicit that this was
  ALWAYS meant to be temporary: *"Minimal 4/4 metric for M0 (a real
  TimeSignature engine lands with M5/M6 ... Hoisted out of
  runtime/transport.hpp during the Phase-1 runtime extraction: arrangrr-side
  modules ... need these constants without depending on the runtime component
  that now owns Transport itself."* — **this is not a locked decision this
  design breaks; it is a stated, milestone-tagged placeholder** (node `1210`/
  D27, `docs/DESIGN.md:733`, scopes `1210` to "960 PPQN scheduler, 96 grid,
  bpm_x100" only — time signature was never inside that shipped scope). T0 is
  the fulfillment of an old, explicitly deferred promise, not a rebuild
  against a broken lock. Classification: **reads-in-hot-on_tick-path is N/A
  here** (this is the definition, consumed everywhere below).

### CORE sites (must cross-build arm-none-eabi — the hard constraint applies)

| # | File:line | What it reads | Classification |
|---|-----------|----------------|-----------------|
| 1 | `components/runtime/include/runtime/transport.hpp:28-29,66-68` | `Position::beat` comment + `Transport::position()` derives `bar`/`beat`/`tick` from `kTicksPerBar`/`kTicksPerBeat` | reads-only, called on-demand (not gated to every tick by callers seen), golden-affecting IF any golden asserts on `Position` |
| 2 | `components/arrangrr/include/arrangrr/engine.hpp:288` | `if (m_transport.tick() % kTicksPerBar == 0)` — **the** bar-boundary gate that promotes chord commits, clip fires, Performance recalls, pad fires | **reads-in-hot-on_tick-path** — evaluated every single tick, unconditionally, whether or not the transport plays a bar boundary this tick |
| 3 | `components/arrangrr/include/arrangrr/arranger/arranger.hpp:415-416` | `len = section->bars * kTicksPerBar`; `bar_boundary = pos % kTicksPerBar == 0` inside `Arranger::on_tick` | **reads-in-hot-on_tick-path**, golden-affecting (section-length/switch timing is core generative math) |
| 4 | `components/arrangrr/include/arrangrr/common/boundary_latch.hpp:39` | `BoundaryLatch::arm()`: `window = n_bars * kTicksPerBar` | reads-only, at arm-time (command-time), not per-tick; `due()` itself only does modulo against the already-computed `window`, not against `kTicksPerBar` directly |
| 5 | `components/arrangrr/include/arrangrr/chord/chord_sequence.hpp:81` | `quantize(Tick grid = kTicksPerBar)` default arg | reads-only, once per `stop_record`, not hot-tick, golden-affecting (quantize-after math) |
| 6 | `components/arrangrr/include/arrangrr/chord/chord_sequencer.hpp:84` | `stop_record(Tick now, Tick grid = kTicksPerBar)` default arg | reads-only, once per `stop_record`, golden-affecting |
| 7 | `components/arrangrr/include/arrangrr/restyle/restyle_stage.hpp:315` | `step = (snapped % kTicksPerBar) / kTicksPerStep` in `RestyleStage::note_on` | reads-only, per live note-on (not per tick), golden-affecting for restyle/groove goldens |
| 8 | `components/arrangrr/include/arrangrr/clip/clip_matrix.hpp:9,40,57,113,126` | `ClipMatrix::on_bar`: `window = c.n_bars * kTicksPerBar` | reads-only, evaluated only for armed/queued-stop clips inside the already-gated bar-boundary block (site #2), not every raw tick |
| 9 | `components/arrangrr/src/engine.cpp:473` | `m_seq.stop_record(m_now, cmd.a > 0 ? cmd.a : kTicksPerBar)` | reads-only, command-time (`kSeqStop` handler) |
| 10 | `components/arrangrr/include/arrangrr/abi.hpp:370,468` | comments only: *"beat (1-based, 1..kBeatsPerBar)"* documenting the `kBeat`/`kParamState` wire shape | **not code** — documents an ABI CONTRACT gap: the wire has no channel today for "what is the current beats-per-bar," so a client cannot correctly interpret `msg.status` (`beat`) once it varies. Genuine ABI delta, not a hot-path site. |

Only site #2 (`Engine::on_tick`'s own gate) and #3 (`Arranger::on_tick`) are
genuinely evaluated on **every** tick regardless of activity. Everything else
in the core fires at command-time or inside the already-existing bar-gated
block — this matters directly for the "near-zero hot-path cost" requirement
in §5.

### HOST-ONLY sites (no freestanding/no-heap constraint; still need the runtime value)

| # | File:line | Context |
|---|-----------|---------|
| 11 | `components/hostrt/shell_io_commands.cpp:535` | `mult = kTicksPerBar` — L1 command duration-unit parsing |
| 12 | `components/hostrt/shell_parse.cpp:234-235` | `strip("bars", kTicksPerBar)` / `strip("beats", kTicksPerBeat)` — the "bars"/"beats" duration-suffix grammar for CLI note/duration literals |
| 13 | `components/hostrt/shell_music_commands.cpp:677` | `dur = kTicksPerBar` — default duration for a music command |

`hostrt` is a static library linking `ALSA::ALSA` (`components/hostrt/CMakeLists.txt:44`)
and is only ever added under the CMake `else()` (non-firmware) branch —
confirmed host-only, no cross-build exposure. These three sites need the
**current runtime** ticks-per-bar (so `bars`/`beats` literals parse against
whatever time signature is actually loaded), but carry none of the
freestanding/no-heap weight.

---

## 2. The runtime carrier

**Decision: `Transport` (`components/runtime/include/runtime/transport.hpp`)
gains the runtime time-signature state, mirroring the EXACT pattern it
already uses for tempo.**

Transport already owns `BpmX100 m_bpm = kDefaultBpm;` (transport.hpp:74) — a
runtime, per-instance, mutable musical parameter — with a validated setter
(`set_bpm`, transport.hpp:39-45, rejects out-of-range) and bounds living
beside the default in `common/time.hpp` (`kMinBpm`/`kMaxBpm`,
`time.hpp:39-40`). Time signature is structurally the same shape of fact:
a small, bounded, runtime-mutable musical parameter that changes rarely (at
style-load / scene-change) and is read often. It should live in the exact
same place, by the exact same discipline:

```
// common/time.hpp, beside kBeatsPerBar/kTicksPerBar/kMinBpm/kMaxBpm
struct TimeSig {
  std::uint8_t beats_per_bar = kBeatsPerBar;    // numerator; runtime-variable, T0
  std::uint32_t ticks_per_beat = kTicksPerBeat; // denominator granularity; PINNED
                                                 // to kPpqn for v1 (Fork "denominator", §6)
  constexpr Tick ticks_per_bar() const noexcept {
    return static_cast<Tick>(beats_per_bar) * ticks_per_beat;
  }
};
inline constexpr std::uint8_t kMinBeatsPerBar = 1;
inline constexpr std::uint8_t kMaxBeatsPerBar = /* NEEDS-DECISION, §6 */;
```

`Transport` gains `TimeSig m_time_sig{};` plus `time_sig()`/`set_time_sig()`
(validated exactly like `set_bpm`) and a convenience `ticks_per_bar()`
forwarder. `Transport::position()` (transport.hpp:64-69) is rewritten to read
`m_time_sig.ticks_per_bar()`/`m_time_sig.ticks_per_beat` instead of the two
global constants — **no threading needed there**, it is already a member
function of the object that now owns the value.

**Why Transport, not a new type, not Engine:** `Transport& m_transport` is
already the SINGLE object every consumer that needs live transport state
holds by reference — `Engine` (`engine.hpp:709`: *"Runtime-owned, injected by
reference (§14.3/B3)"*) and, as of this trace, `RestyleStage` can trivially
join it (see below). Adding a second runtime carrier (e.g. a free-standing
`TimeSigStore` owned by `Runtime<Engine,N>`) would duplicate exactly the
ownership problem `Transport` already solves for tempo — a "fourth bespoke
copy" of the kind `BoundaryLatch`'s own header comment
(`boundary_latch.hpp:7-14`) was written specifically to stop happening.

**Threading to the 13 sites — pass-through, not a global, confirmed per site:**

- **Engine's own gate (site #2)**: `Engine` already holds `Transport&
  m_transport`. `engine.hpp:288` becomes
  `m_transport.tick() % m_transport.ticks_per_bar() == 0` — one inlined
  member call, zero allocation, zero new state in `Engine` itself.
- **Arranger, ChordSequence, ChordSequencer, ClipMatrix, BoundaryLatch (sites
  #3-#6, #8)**: none of these classes holds a `Transport&` today — `Engine`
  already unpacks the raw tick and threads it down explicitly as a parameter
  at every one of these call boundaries (`fire_arranger(tick, ...)`,
  `fire_clips(tick, ...)`, `Arranger::on_tick(Tick transport_tick, const
  Key&, const ChordState&, NoteScheduleFn)`). The non-global fix mirrors that
  EXACT existing shape: `Engine` reads `const Tick tpb =
  m_transport.ticks_per_bar();` once per `on_tick`/command dispatch and
  passes it down as one additional scalar parameter — the same shape `key`/
  `chord` already travel in. Concretely: `Arranger::on_tick(...,
  Tick ticks_per_bar = kTicksPerBar)`, `ClipMatrix::on_bar(Tick
  transport_tick, Tick ticks_per_bar, Fn&&)`, `BoundaryLatch::arm(std::uint8_t
  n_bars, Tick ticks_per_bar)`, `ChordSequence::quantize(Tick grid)` /
  `ChordSequencer::stop_record(Tick now, Tick grid)` keep their existing
  explicit-grid parameter — `engine.cpp:473`'s
  `cmd.a > 0 ? cmd.a : kTicksPerBar` fallback becomes `cmd.a > 0 ? cmd.a :
  m_transport.ticks_per_bar()`. Every one of these is a default-arg or
  explicit-arg change at an existing call boundary — additive, not a new
  abstraction.
- **RestyleStage (site #7) — the one site that is NOT an Engine sub-object.**
  `RestyleStage` is a peer `Stage` in `runtime::Pipeline`, driven directly by
  `runtime::Runtime` (`restyle_stage.hpp:275`: `on_tick(const
  runtime::StageContext& ctx, SinkT)`), not reached through `Engine`'s own
  parameter-threading. `runtime::stage.hpp:20-35` is explicit and deliberate
  that `StageContext` stays narrow (`{ Tick now; }` only) BY DESIGN — its own
  comment names the alternative directly: *"This Phase-1 implementation
  instead injects `arrangrr::Transport&` into the arrangrr Stage-adapter by
  reference ... because arrangrr's OWN command dispatch ... reads/writes
  Transport from OUTSIDE the on_tick path too."* This is the precedent to
  extend, not `StageContext`: give `RestyleStage` a `Transport&` the same way
  `Engine` already has one. The wiring is **already mechanically present and
  unused**: `components/hostrt/shell.cpp:128-129` builds `RestyleStage` from
  a lambda `[this](auto& sched, auto&, auto&, auto& chorddet)` whose SECOND
  positional parameter (named `transport` in the sibling `Engine` lambda
  right below it, line 131) is passed by `runtime::Runtime`'s composition
  machinery to every stage factory already — it is simply unnamed and unused
  in `RestyleStage`'s own lambda today. Naming it and forwarding it into
  `RestyleStage`'s constructor is a mechanical, already-plumbed change, not a
  new injection seam.
- **HOST sites #11-#13**: `hostrt` already links `arrangrr`/`runtime`
  (`components/hostrt/CMakeLists.txt:44`) and its `Shell` already owns the
  `Engine`/`Transport` composition (`shell.cpp:104-133`). The three CLI sites
  read the CURRENT runtime value via `shell.engine().transport().ticks_per_bar()`
  (or an equivalent accessor already reachable from `Shell`), replacing the
  compile-time constant with a live query — no freestanding constraint
  applies, so this is a straight read, no design tension.

---

## 3. Byte-identity guarantee — the acceptance gate

`TimeSig`'s default member initializers are `beats_per_bar = kBeatsPerBar`
(4) and `ticks_per_beat = kTicksPerBeat` (960) — **the literal two constants
whose product defined `kTicksPerBar` in the first place**
(`common/time.hpp:29`: `kTicksPerBar = kBeatsPerBar * kTicksPerBeat`). A
default-constructed `Transport` therefore has `m_time_sig.ticks_per_bar() ==
4 * 960 == 3840 == kTicksPerBar`, bit-for-bit, not by coincidence but by
construction — the runtime formula IS the compile-time formula, evaluated on
a variable instead of two named constants.

This makes the acceptance gate mechanical, not probabilistic: **as long as no
call site ever invokes `Transport::set_time_sig()` away from the default**,
every one of the 13 sites above computes the numerically IDENTICAL value it
computes today, because each one either (a) reads `m_transport.ticks_per_bar()`
where `m_transport` was never mutated away from its default `TimeSig{}`, or
(b) receives that same unmutated value threaded down as a parameter. No
existing style, no existing `Performance` slot, no existing test/golden
fixture calls a not-yet-existing `set_time_sig` — so every current golden
stays byte-identical for free, by the SAME discipline that made
groove-as-insert reproduce the old math exactly (a structural refactor whose
default value is definitionally equal to the prior constant, not
approximately equal). Goldens move ONLY the moment a style/scene/CLI command
actually calls `set_time_sig(n)` with `n != 4` — a deliberate, attributable
trigger, never an accidental side effect of the refactor itself.

---

## 4. The `8100` and `6000` hooks

### `8100` — per-scene time-sig override

Two existing precedents make this nearly free:

1. **`Style::tempo`** (`components/arrangrr/include/arrangrr/arranger/style_model.hpp:189`:
   `BpmX100 tempo = kDefaultBpm;`) is already a per-style default, applied at
   every style-load path through `Engine::apply_style_tempo()`
   (`engine.hpp:478-488`: *"Seeds the transport tempo from the loaded style's
   default ... Called on every path that (re)loads the arranger's style — an
   explicit load, an immediate live switch, or a deferred switch landing at
   the bar boundary."*). A sibling `Style::beats_per_bar` field (or a
   `TimeSig` member) plus a sibling `apply_style_time_sig()` call at the
   SAME three call sites (`apply_style_tempo`'s own call sites) gives every
   built-in style a genuine per-style time signature by construction, using a
   mechanism already tested for tempo.
2. **`Performance::tempo_x100`** (`components/arrangrr/include/arrangrr/perf/performance.hpp:90`,
   packed/unpacked at lines 293/336) is already the per-slot tempo carrier for
   Performance recall. With ABI stability lifted for this scope, a sibling
   `Performance::beats_per_bar` (u8) field, packed/unpacked the SAME way, is
   the honest per-scene time-sig carrier the prior scope doc's Fork D asked
   for — `SceneChain` (not yet built, §2 of the prior doc) references
   `Performance` slots; at each scene transition `Engine::apply_performance`
   (`engine.cpp:1283-1362` per the prior doc's citation) already recalls
   tempo via `set_bpm` — it gains one sibling call to
   `m_transport.set_time_sig(perf.beats_per_bar)` at the exact same point.

**ABI delta this genuinely opens (flagged, not assumed settled):** the
`kBeat` wire event (`abi.hpp:370-373,467-472`) packs `beat` as "1-based,
1..kBeatsPerBar" with NO channel today telling a client what the CURRENT
`beats_per_bar` actually is — a client cannot render a meter correctly once
it varies. This needs a new announce, mirroring the existing
`kParamState`/`kSection` echo-on-change shape (`abi.hpp:404-414`) — e.g. a
`kTimeSig` `OutEvent::Kind` emitted whenever `set_time_sig` actually changes
the value, exactly like `kSection` announces an arranger section change.
Free under the lifted-ABI directive; still a genuine new wire surface, not a
struct-field-only change — flagged for the owner's SHIPPABLE sign-off in §6.

### `6000` — the looper records/loops against the current time-sig

The not-yet-built `LoopBuffer` (prior doc, §2/§3, node `6100`-`6400`) must, on
day one, follow the SAME discipline as every site in §2 above: loop length
"in bars" is `n_bars * m_transport.ticks_per_bar()`, threaded exactly like
`ClipMatrix::on_bar`'s own window and `BoundaryLatch::arm`'s own window — it
must NEVER reference `kTicksPerBar` directly, or it re-introduces the exact
compile-time coupling this design closes, on day one of a brand-new class.
This closes the prior doc's own open item (§1: *"giving `8100` a genuine
per-scene time-signature is NOT a struct-field addition, it is a prerequisite
variable-time-signature engine"*) for `6000` too — `LoopBuffer`'s quantize-
after grid (mirroring `ChordSequence::quantize`) and its loop-length-in-bars
field (mirroring `ClipMatrix::Clip::n_bars`) both take the SAME threaded
`ticks_per_bar` parameter this design already plumbs everywhere else.

---

## 5. No-heap / arm confirmation

**Bounded, static, confirmed.** `TimeSig` is a 5-6 byte POD (`std::uint8_t` +
`std::uint32_t`, likely 8 bytes with alignment padding) added as a VALUE
member of `Transport` — no pointer, no container, no allocation, identical in
kind to the `BpmX100 m_bpm` field already there. `Transport` itself already
cross-builds `arm-none-eabi` unconditionally (`CMakeLists.txt:38-45`); adding
one more scalar-sized value member changes `sizeof(Transport)` by a handful
of bytes, nothing that threatens the STM32H743 512 KB envelope (D33,
`docs/DESIGN.md:587`). Every downstream threading point (§2) is a parameter
addition or a member-function change — no new heap, no new container, no new
virtual dispatch (`Arranger`/`ClipMatrix`/`BoundaryLatch`/`ChordSequence` stay
concrete, non-virtual classes exactly as today).

**Hot-path cost, honestly priced.** Only two sites run on every raw tick
regardless of activity (§1's table): `Engine::on_tick`'s own gate
(`engine.hpp:288`) and `Arranger::on_tick`'s bar-boundary check
(`arranger.hpp:415-416`) — both go from "modulo against a compile-time
constant" to "modulo against a runtime value read from a reference/passed as
a parameter." On a Cortex-M7 this is, at most, trading a constant-folded
`AND`/shift (if `kTicksPerBar` were a power of two — it currently is NOT:
3840 = 4×960, not a power of two, so today's compile-time modulo already
compiles to a real integer division or a `constexpr`-precomputed reciprocal,
not a bit-mask) for a runtime `udiv`/modulo against a value held in a
register — the SAME instruction class, operand now a register read from an
already-resident cache-line (`Transport` is tiny and already touched every
tick for `.tick()`/`.playing()`). This is not a new instruction class on the
hot path, it is the same division with a variable divisor — negligible
against the existing 192 KB DTCM zero-wait budget and the <1 ms tick-jitter
target (D33). **No site in the core requires a container, a lookup table, or
a heap-backed structure to become runtime.**

**One site flagged as NOT cheap to generalize (denominator, see Fork below):
`kTicksPerBeat` itself.** If a future iteration wants genuine compound
meters (6/8, 12/8 — where "beat" is an eighth note, not a quarter note),
`ticks_per_beat` would ALSO need to vary, which touches `kPpqn`/`kGridPpqn`'s
own fixed relationship (`time.hpp:10-20`, asserted at compile time,
`kPpqn % kGridPpqn == 0`) — that is a materially larger change than this
design, correctly out of scope for T0, and named explicitly as Fork
"denominator" below rather than silently assumed away.

---

## 6. Residual forks for the owner

**Fork — denominator scope (numerator-only vs. full N/D meter).**
Recommendation: **v1 varies `beats_per_bar` (numerator) only; `ticks_per_beat`
stays pinned to `kTicksPerBeat` (== `kPpqn`, quarter-note beat) for every time
signature.** This covers 3/4, 5/4, 7/4, 9/8-as-a-beat-count-of-9 (treated as 9
quarter-note-equivalent beats, not true 9/8 compound feel) — i.e. every
"simple meter" and any meter whose feel the owner is willing to approximate
as N quarter-note-equivalent beats per bar. TRUE compound meters (6/8 felt as
2 dotted-quarter beats, not 6) need `ticks_per_beat` to vary too, which
interacts with the `kPpqn`/`kGridPpqn` compile-time relationship
(`time.hpp:17-20`) and is materially bigger. NEEDS-DECISION: is
numerator-only sufficient for the styles/scenes the owner has in mind for
`8100`, or does the first cut already need true compound-meter feel?

**Fork — `kMaxBeatsPerBar` bound.** No existing constant bounds this anywhere
in the tree. Recommendation: a small, honest cap (e.g. 16 — covers every
meter in common practice with generous headroom) living in `common/time.hpp`
beside `kMinBpm`/`kMaxBpm` (NOT in `arrangrr/config.hpp` — see the layering
note below). NEEDS-DECISION: exact number is a product call, not a technical
one; any bound `<= 255` is free against the `kBeat` wire event's existing
single-byte `beat` field (`abi.hpp:370`).

**Fork — the `kBeat`/`kTimeSig` ABI delta (§4).** Recommendation: add a new
`OutEvent::Kind::kTimeSig`, emitted on genuine change, mirroring `kSection`'s
existing echo-on-change shape. SHIPPABLE under the lifted-ABI directive; the
owner should explicitly bless a NEW wire kind (not implied by "reshape freely"
— a new Kind is additive, not a reshape, but it is still new wire surface a
future client must handle).

**Fork — `Style`/`Performance` field naming and whether BOTH need a
time-sig, or just `Performance` (the `8100` carrier).** Recommendation: give
BOTH the field (mirroring how BOTH already carry independent tempo state
today) — a style author's authored default feel (e.g. a waltz style
genuinely wants 3/4 by default) is a different concern from a scene's
override, exactly as `Style::tempo` and `Performance::tempo_x100` already
coexist without conflict (`apply_performance`'s recall simply overwrites
whatever the style seeded, same as tempo does today). NEEDS-DECISION: purely
confirms the existing tempo precedent extends unchanged; low-risk, but the
owner should see it named rather than inherited silently.

**Layering note, not a fork — where `kMaxBeatsPerBar` may NOT live.**
`arrangrr/config.hpp` is the wrong file for this bound: `components/runtime`
(which owns `Transport`, the carrier) depends only on `components/common`,
never on `components/arrangrr` — the dependency runs `arrangrr -> runtime ->
common`, confirmed by `CMakeLists.txt`'s unconditional `add_subdirectory`
order (`components/common`, then `components/runtime`, then
`components/chorddet`, then `components/arrangrr`, `CMakeLists.txt:38-42`)
and by `runtime/transport.hpp:5`'s own `#include "common/time.hpp"` (never
the reverse). A bound constant for the carrier belongs beside the carrier's
own default/definition, in `common/time.hpp` — putting it in
`arrangrr/config.hpp` would be a dependency pointing the wrong way (a lower
layer's own validated setter reaching upward into a higher layer's config for
its bound), the exact "confini e proprietà" defect this review's method
exists to catch.

---

## What I flagged / dependency stance

No new dependency anywhere in this design: `TimeSig` is a POD value type
using only `std::uint8_t`/`std::uint32_t`, added to an existing class
(`Transport`) already dependency-free per D4 (`docs/DESIGN.md:0800`'s node).
Every threading change is a parameter or member addition at an EXISTING call
boundary, never a new container, template, or virtual seam. The three genuine
owner decisions are named above as Forks (denominator scope, the bound
value, the ABI `kTimeSig` announce, and the `Style` vs. `Performance`
field-pair) — none assumed settled here.

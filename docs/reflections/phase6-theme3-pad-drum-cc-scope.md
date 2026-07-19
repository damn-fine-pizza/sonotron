# Pad types Drum/CC — musical + design scope (Phase-6 Theme 3, Item #2)

Ottorino, DESIGN (2026-07-14). Pre-code musical + design scoping, no product
code written or edited by this review. Companion piece to
`docs/reflections/phase6-theme3-master-transpose-scope.md` (same house style,
same reviewer, same reason for a separate `docs/reflections/` file rather than
touching `docs/phase6-plan.md` in place). Traces `docs/phase6-plan.md:72-73`'s
own framing: *"Pad types Drum/CC (need new note/CC emission in the pad layer —
the piece #9 deferred). NoteRepeat likely folds into the FX note-repeat
insert."* This doc is that scoping pass, laid out as options for the owner —
no code follows from it yet.

## What Item #2 is

Phase-5 Item #9 shipped the pad bank as WRAPPER-ONLY: every `PadType` fans out
to an EXISTING verb (a `ClipMatrix` clip launch, an `Arranger` section
request, or a `PerformanceStore` recall) — `components/arrangrr/include/
arrangrr/pad/pad_bank.hpp:11-16` states this as a locked decision, and
`components/arrangrr/include/arrangrr/abi.hpp:208-213` repeats it verbatim
next to `kPadAssign`'s own doc comment: *"there is no new note/CC emission
engine."* Item #2 is the one piece #9 deliberately deferred: two new
`PadType`s, **Drum** and **CC**, that emit a MIDI message directly instead of
wrapping a subsystem. This is the one place in the pad system that is
expected to touch that rule.

## Traced ground

- **The `Pad` POD, 12 bytes, pinned.** `pad_bank.hpp:50-69`:
  ```cpp
  struct Pad {
    PadType type; PadMode mode; Boundary sync; PadPitch pitch;
    std::uint8_t n_bars;        // meaningful only when sync == kNextNBars
    std::uint8_t dest_port;     // reserved destination (comment: "not yet
    std::uint8_t dest_channel;  //  consumed by fire_pad's wrapper-only dispatch")
    std::uint16_t source_idx;   // meaning depends on `type`
    std::uint8_t source_aux;    // reserved per-type auxiliary selector (unused v1)
  };
  static_assert(sizeof(Pad) == 12, "Pad RAM/wire budget pin (D33)");
  ```
  `dest_port`/`dest_channel`/`source_aux` are ALREADY reserved for exactly a
  direct-emission `PadType` — the header comment names the wrapper-only
  dispatch as the reason they are unconsumed today, not a structural gap.
  There is no spare byte beyond these fields: `type`+`mode`+`sync`+`pitch`+
  `n_bars`+`dest_port`+`dest_channel` = 7 bytes, `source_idx` (u16, needs
  2-byte alignment) + `source_aux` (u8) = 3 more, and the 12-byte total
  leaves at most 1-2 bytes of compiler-inserted tail padding — not a field,
  not safe to rely on. **A gate/duration field is not obtainable without
  growing `Pad` past 12 bytes.** This review does not recommend that; see
  Decision 3.

- **`kPadAssign`'s packing is already 100% type-agnostic.**
  `abi.hpp:214-226`: `a = type|(mode<<8)|(sync<<16)|(pitch<<24)`, `b =
  dest_port|(dest_channel<<8)|(n_bars<<16)`, `c = source_idx|(source_aux<<24)`.
  `Engine::pad_assign` (`components/arrangrr/src/engine.cpp:915-950`) unpacks
  and bounds-checks ALL of these fields for EVERY `PadType` today — including
  `dest_port < kMaxPorts` and `dest_channel <= 15`
  (`engine.cpp:932`) — regardless of whether the current dispatch reads them.
  Drum/CC pads are therefore already validly constructible via `kPadAssign`
  with no change to that command's own shape. The ONE line that gates which
  `PadType` values are accepted is `engine.cpp:928`: `type_v <=
  static_cast<std::uint32_t>(PadType::kPerformance)` — this is the single
  edit `pad_assign` needs to accept two new enum values.

- **`pad_trigger`/`pad_release` are already fully generic over `PadMode`.**
  `engine.cpp:955-969` (`pad_trigger`): `kToggle` flips `PadRuntime::on` and
  calls `fire_pad(pad, on ? kPlaying : kStopped, sink)`; every other mode
  calls `fire_pad(pad, kPlaying, sink)`. `engine.cpp:971-983`
  (`pad_release`): a no-op unless `mode == kHold`, in which case it calls
  `fire_pad(pad, kStopped, sink)`. **This dispatch needs zero changes** for
  Drum/CC — it already turns "trigger"/"release" into a `LaunchState`
  target per `PadMode`, for every `PadType` uniformly.

- **`fire_pad`'s switch is the one place a case must be added.**
  `engine.cpp:986-1041`: each existing case calls exactly one already-tested
  subsystem verb (`clip_request`, `clip_scene_launch`, `m_arranger.request`,
  `perf_recall`). A `kDrum`/`kCC` case would instead construct a
  `MidiMessage` and call `schedule_or_warn` directly — see Decision 5.

- **`schedule_or_warn` is the SAME choke point every emission path already
  uses.** `components/arrangrr/include/arrangrr/engine.hpp:447-451`:
  ```cpp
  void schedule_or_warn(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    if (!m_scheduler.schedule(port, tick, msg)) sink(OutEvent::warn(WarnCode::kSchedulerFull, m_now));
  }
  ```
  Every existing note-emitting path — `Timeline`, `Arranger::on_tick`,
  `ChordEngine`, the live arpeggiator, `restyle_stage` — funnels through this
  one function (confirmed by grep across `engine.cpp`/`engine.hpp`: 9 call
  sites, none bespoke). A Drum/CC pad that also calls it is not a new
  emission mechanism, it is one more caller of the one that exists.

- **`flush()` gives Panic-safety and host echo for free, with zero new
  plumbing.** `engine.hpp:305-310`:
  ```cpp
  void flush(EventSink sink) {
    m_scheduler.pop_due(m_now, [&](const ScheduledEvent& ev) {
      m_tracker.observe(ev.port, ev.msg);       // NoteTracker: panic bookkeeping
      sink(OutEvent::midi(ev.port, ev.msg, ev.tick));  // host echo
    });
  }
  ```
  `NoteTracker::observe`/`panic` (`components/arrangrr/include/arrangrr/
  routing/note_tracker.hpp:14-77`) tracks every NoteOn/NoteOff/CC64-sustain
  that passes through `flush()` and can silence it with explicit NoteOffs +
  CC120/121/123. Anything scheduled via `schedule_or_warn` is automatically
  Panic-safe and automatically echoed to the host as an `OutEvent::midi` —
  no bespoke tracking needed for Drum/CC.

- **`MidiMessage` factories already cover exactly what's needed, nothing
  more.** `components/common/include/common/midi/message.hpp:92-107`:
  `note_on(ch, note, vel)`, `note_off(ch, note, vel=64)`, `cc(ch, controller,
  value)`. No new wire primitive is required.

- **No existing GM-percussion-channel default anywhere in the engine.**
  Grepped `components/arrangrr` and `components/common` for a hardcoded
  drum/percussion channel constant (`channel = 9`, `kDrumChannel`,
  `kGmPercChannel`, etc.) — **zero hits**. `Arranger::set_route`
  (`arranger.hpp:80-89`) is the only channel-assignment mechanism and it is
  fully host-driven, no default per role. The ONLY place the GM-perc
  convention is even mentioned in the repo is a UI-layer comment,
  `components/hostrt/parts_view.hpp:31-33`: *"`is_percussion` selects the
  '(kit)' voice label instead of a GM program name (GM percussion lives on
  channel 10 regardless of program)"* — i.e. the convention is known to the
  mixer display, never enforced or defaulted by the engine. **A Drum pad's
  default channel is therefore an open decision, not a lookup** (Decision 4).

- **`TrackRole::kCc` already exists, purpose-built and currently 100% dormant
  — a real alternative destination path.** `components/arrangrr/include/
  arrangrr/timeline/timeline.hpp:26-37`: `enum class TrackRole { kDrums=0,
  kPerc=1, ..., kCc=9 }`. `arranger.hpp:635`: the per-role register-anchor
  table comments it `60, // kCc (unused)`. `components/hostrt/
  parts_view.hpp:26-27`: *"the eight automatic style parts... kLead/kCc are
  excluded — they are not arranger band parts."* Grepping all 16 style
  tables for `TrackRole::kCc` (`components/arrangrr/include/arrangrr/
  arranger/styles/*.hpp`) → **0 hits** anywhere in the shipped corpus. `kCc`
  is a routing slot that was reserved and named for "raw MIDI CC
  destination" and never wired to anything — which makes it a serious
  candidate for the CC pad's OWN destination, via the ALREADY-SHIPPED
  `kStyleRoute` command (`abi.hpp:131`: `set: a = TrackRole, b = port |
  (channel << 8)`) and `Arranger`'s public `PartInfo part_info(TrackRole)`
  accessor (`arranger.hpp:210-239`, returns `routed`/`port`/`channel`/
  `muted`/`soloed`). See Decision 2.

- **Real, corpus-measured drum-hit gate lengths exist, for grounding a
  one-shot gate constant if one is needed.** `components/arrangrr/include/
  arrangrr/arranger/styles/basic.hpp:87-88` (`kVarBDrums`, real GM
  percussion notes): kick/snare (`tone=36`/`38`) use `gate=120`; closed
  hi-hat (`tone=42`) uses `gate=50`. PPQN is 960 (`components/common/
  include/common/time.hpp:12`), so a 16th-step is 240 ticks
  (`timeline.hpp:21`) — these authored drum gates are a 32nd-note or shorter,
  confirming percussion hits are conventionally very short relative to the
  grid. `StyleEvent::gate` itself is `std::uint16_t` (`style_model.hpp:94`),
  a 10-byte pinned struct — this is the shape a gate field would need if it
  existed on `Pad`, which it structurally cannot (see above).

- **`NoteSource::kFixed`'s "drums never transpose" convention.**
  `style_model.hpp:47`: `kFixed = 0, // literal MIDI notes (drums/percussion
  — never transposed)`. `PadPitch` (`pad_bank.hpp:43-46`) is itself
  documented as "CAPTURED but NOT YET WIRED to any dispatch behavior" for
  every existing `PadType`. Consistent with both precedents, Drum/CC pads
  should simply ignore `pitch` too — a raw note number and a raw CC value
  have no meaningful "transpose with chord" reading, and doing nothing here
  costs zero code.

## Decision 1 — field mapping

| `Pad` field | Drum (`kDrum`) | CC (`kCC`) |
|---|---|---|
| `source_idx` (u16, low byte read) | MIDI note number, 0..127 | CC controller number, 0..127 |
| `source_aux` (u8) | velocity, 0..127 | on-value / toggled value, 0..127 |
| `dest_port` | output port (already bounds-checked `< kMaxPorts` at assign time) | same |
| `dest_channel` | output channel, 0..15 (already bounds-checked at assign time) | same |
| `pitch` | ignored (matches `kFixed`'s "drums never transpose") | ignored (no pitch concept for a CC) |
| `mode`/`sync`/`n_bars` | drives `LaunchState` via the EXISTING generic `pad_trigger`/`pad_release` dispatch (no change there) | same |

Both new types need semantic-range checks (note/CC/value `> 127` → reject)
done AT FIRE TIME inside the new `fire_pad` cases, mirroring the existing
`kVariation`/`kFill` precedent (`engine.cpp:1016-1019`,
`sink(OutEvent::warn(WarnCode::kBadArgument, m_now))` for an out-of-range
`source_idx`) — `pad_assign` itself only validates STRUCTURAL bounds
(enum ranges, port/channel width) for every `PadType` uniformly today, never
per-type semantics; Drum/CC should follow that same division of labor rather
than inventing a new one.

## Decision 2 — destination: pad-owned vs role-route reuse

Two real, evidence-backed options; this is a genuine product fork.

**Option A — pad owns its destination (`dest_port`/`dest_channel` consumed
directly).** Matches the reserved-field comment's own stated intent
(`pad_bank.hpp:58-59`) literally: the pad is a self-contained physical
trigger, independently addressable to ANY port/channel regardless of what
the current style routes to `kDrums`/`kCc`. Musically this matches how
physical drum-pad controllers and CC pads behave on real gear — free
routing, not tied to "the band." Con: does not inherit `Arranger`'s
mute/solo (`m_muted`/`m_solo`, `arranger.hpp:552-555`) or route-enable state
— a Drum/CC pad sounds even if the corresponding role is muted, which may
be surprising or may be exactly the point (an independent tool, not a band
member).

**Option B — reuse the route table via `TrackRole::kDrums`/`kCc`.**
`fire_pad` ignores the pad's own `dest_port`/`dest_channel` and instead
reads `m_arranger.part_info(TrackRole::kDrums)` / `part_info(TrackRole::kCc)`
for `port`/`channel`, pre-configured by the ALREADY-SHIPPED `kStyleRoute`
verb. This is closer to the wrapper-only rule's original spirit ("fans out
to an existing verb") and `kCc`'s own dormant, purpose-named existence is a
strong hint this is what it was reserved for. Gains `muted` for free via the
public `PartInfo` (`arranger.hpp:222-239`); does NOT reproduce the
"`solo_active` on some OTHER role" half of `part_silenced`
(`arranger.hpp:552-555`, PRIVATE) without one small new public accessor — a
real, disclosed, small cost, not free. Con: a Drum pad on `TrackRole::kDrums`
necessarily shares the SAME port/channel as the arranger's own drum pattern
— no independent destination for an "extra one-shot on its own synth" use.

**This reviewer's read:** Option A is the better fit for a *pad* specifically
(the reserved fields exist for exactly this, and free per-pad routing is the
more useful primitive for a performance surface), but Option B deserves a
real hearing given how purpose-built `TrackRole::kCc`'s dormancy looks.
Recommend the owner decide once, since it also determines whether "mute the
drums" (`kPartMute`, role `kDrums`) should silence a Drum pad — a musical
question as much as an architectural one.

## Decision 3 — Drum pad: mode semantics, and the gate-length gap

`PadMode` dispatch needs no new plumbing (Decision 1 above), but the
`kOneShot`/`kLoop` case exposes a real gap: **`Pad` has no gate/duration
field**, and cannot gain one without breaking the 12-byte pin
(`pad_bank.hpp:69`, D33).

- **`kHold`**: trigger → `note_on(channel, note, vel)`; release →
  `note_off(channel, note)`. Clean — `LaunchState::kPlaying`/`kStopped`
  already map directly to on/off, no constant needed.
- **`kToggle`**: same on/off pair, flipped by `PadRuntime::on` exactly like
  every other `PadType`'s `kToggle` today. Clean.
- **`kOneShot`** (and `kLoop`, folded to the same behavior — "loops on its
  own terms" has no meaning for a single note, and `pad_assign` validates
  `mode` structurally for every type today, so silently degrading `kLoop` to
  `kOneShot` avoids a validation change): `pad_release` is ALREADY a no-op
  for every mode but `kHold` (`engine.cpp:978-979`), so nothing calls the
  matching note-off. Two defensible resolutions:
  1. **Same-tick paired on/off** (`schedule_or_warn` the `note_on` then the
     `note_off` at the same `m_now`, no gate constant to choose). Correct
     for GM percussion voices (they self-envelope; a note-off is largely
     ignored by drum samples per GM convention) but risky if Option A/B lets
     a Drum pad target a non-percussion, envelope-sensitive synth voice —
     an instantaneous off could clip it.
  2. **Fixed short engine constant gate**, grounded in the corpus's own
     authored drum-hit gates (`basic.hpp:87-88`: 50-120 ticks for
     kick/snare/hi-hat) rather than an invented number — e.g. ~120 ticks,
     matching the kick/snare gate already shipped. Costs one named constant
     in engine code, no `Pad` field growth, safer for the non-percussion
     one-shot case.
  Recommend (2) as the more defensible default, but the exact tick value is
  a product/musical call for the owner or Giotto to fix, not this review.
- **Stuck-note risk under option (1) if chosen**: `NoteTracker` marks the
  note "on" until an explicit off or `panic()`; a same-tick paired off
  avoids this entirely, so (1) has no real stuck-note risk either — the gap
  only matters for the "no note-off scheduled at all" reading, which this
  review does NOT recommend either way.

## Decision 4 — CC pad: mode semantics, and the off-value gap

- **`kHold`**: trigger → `cc(channel, source_idx, source_aux)` (the
  on-value); release → `cc(channel, source_idx, 0)`. The off-value has no
  field to carry a custom number — `source_aux` is a single byte, already
  spent on the on-value. Recommend **hardcoding the off-value to 0**: this
  matches the codebase's own established "0 = off/no-effect" convention
  (`message.hpp:31`, `kCcSustain` threshold is `>=64` down / `<64` up;
  `NoteTracker::panic` zeroes CC120/121/123, `note_tracker.hpp:71-73`) and
  covers the common momentary-CC use (a pedal-style filter/effect toggle)
  without inventing a second value field. A CC pad that genuinely needs a
  non-zero "released" value is out of scope for the 12-byte `Pad`
  budget as it stands — flagged, not solved here.
- **`kToggle`**: `cc(channel, source_idx, source_aux)` when `rt->on` flips
  true, `cc(channel, source_idx, 0)` when it flips false — same convention,
  reuses `PadRuntime::on` exactly like every other toggle pad.
- **`kOneShot`/`kLoop`** (folded together, same reasoning as Drum): fire the
  on-value once, no release action needed or expected — a CC pad firing a
  single, permanent-until-changed value (e.g. "switch to the bright patch")
  is a musically NORMAL, common pad use, unlike Drum's stuck-note concern.
  No gap here at all.

## Decision 5 — the tripwire: what breaks, how much, and where it stops

The LETTER of the wrapper-only rule (*"there is no new note/CC emission
engine"*, `pad_bank.hpp:14-16`, `abi.hpp:211-212`) is broken by these two
`PadType`s — for the first time, `fire_pad` constructs a `MidiMessage`
itself instead of delegating to `clip_request`/`m_arranger.request`/
`perf_recall`.

The SPIRIT of the rule — "no new machinery" — is NOT broken:
- No new scheduler: uses the existing `schedule_or_warn`
  (`engine.hpp:447-451`), the SAME choke point 9 other call sites already
  share.
- No new note-tracking/panic mechanism: `flush()`'s existing
  `m_tracker.observe` (`engine.hpp:305-310`) covers it automatically.
- No new host-echo mechanism: the existing `OutEvent::midi` emitted by the
  same `flush()` covers it automatically.
- No new ABI verb, no new `Command` shape, no `Pad`/`PadRuntime` size
  change, no new `PadMode` dispatch logic (Decisions 3/4 reuse the fully
  generic `pad_trigger`/`pad_release` as-is).
- The new surface is exactly 2 `case` blocks inside `fire_pad`'s EXISTING
  switch (`engine.cpp:997-1041`) plus a widened bound check on ONE line
  (`engine.cpp:928`).

Net: a surgical, minimal, honest break of the rule's letter, in exactly the
one place its own comment predicted ("reserved... not yet consumed by
fire_pad's wrapper-only dispatch") — not a reopening of the wrapper-only
scope for the other 6 `PadType`s, which stay untouched.

## Decision 6 — ABI surface: no new Param

Reuse `kPadAssign`(48)/`kPadTrigger`(49)/`kPadRelease`(50) verbatim — their
`a`/`b`/`c` packing (`abi.hpp:214-226`) is already fully type-agnostic and
validates `dest_port`/`dest_channel`/`source_idx`/`source_aux` for every
`PadType` today, Drum/CC included, with zero shape change. The only edits:

1. Two new `PadType` enum values in `pad_bank.hpp:20-28` (`kDrum = 7`,
   `kCC = 8`).
2. Widen `pad_assign`'s bound check at `engine.cpp:928` from `type_v <=
   PadType::kPerformance` to include the new max (suggest introducing a
   `kPadTypeCount` constant, mirroring `kSectionTypeCount`,
   `style_model.hpp:35` — a naming nicety, not a scope decision).
3. If Option B (Decision 2) is chosen for destinations, no ABI change at
   all beyond the above — `kStyleRoute` (`abi.hpp:131`) already exists.

**No new `Param` value is needed**, so nothing from 59+ is consumed by this
item — confirmed no collision with `kMasterTranspose`(57)/`kPadBankSelect`
(58), both landing in parallel per Theme 3 Items #1/#4.

## NoteRepeat — confirmed excluded

`docs/phase6-plan.md:72-73`'s own text: *"NoteRepeat likely folds into the
FX note-repeat insert"* — Theme 4 (`5210`/`5220`, the MIDI-FX insert chain),
not a `PadType` here. This review does not reserve a `PadType` enum value
for it; `kDrum = 7`/`kCC = 8` are the only two new values in scope.

## What needs deciding / what I flagged

| # | Question | This review's read | Status |
|---|---|---|---|
| 1 | Field mapping (note/vel, CC#/value) | As tabled in Decision 1 — no ambiguity, follows existing per-field byte widths | Not really open |
| 2 | Destination: pad-owned `dest_port`/`dest_channel` (A) vs role-route reuse via `kCc`/`kDrums` + `kStyleRoute` (B) | A fits a "pad" primitive better; B fits the wrapper-only letter better and `kCc`'s dormancy is a real hint. Genuine fork | **Open, owner call** |
| 3 | `kOneShot` Drum gate: same-tick paired off (no constant) vs a fixed short constant grounded in corpus values (~120 ticks) | Recommend the fixed constant (safer for non-percussion one-shot use) | Open on the exact tick value only |
| 4 | CC `kHold`/`kToggle` off-value: hardcode 0 vs a second value field (not obtainable in 12 bytes) | Hardcode 0, matches existing 0-is-off convention | Not really open — the alternative isn't buildable without breaking D33 |
| 5 | Should a Drum pad muted-role interaction exist (only relevant under Option B) | Falls out of Decision 2 | Open only if B is chosen |
| 6 | `kLoop` folded to `kOneShot` behavior for both new types | Recommend yes, avoids a validation change | Not really open |
| 7 | New `PadType` enum values only, no new `Param` | Confirmed feasible, minimal | Not open |

## Feasibility summary

| Item | Label | Dependency |
|---|---|---|
| `kDrum`/`kCC` `PadType` enum values + widened `pad_assign` bound check | SHIPPABLE | none — one enum addition, one comparison-bound edit |
| `fire_pad` new cases emitting via `schedule_or_warn` (Option A, pad-owned dest) | SHIPPABLE | none — no heap, no new scheduler, reuses existing choke point; realtime cost is one more `MidiMessage` construction + schedule call per trigger, identical order of magnitude to every existing pad type |
| `fire_pad` new cases routing via `TrackRole::kCc`/`kDrums` + `part_info` (Option B) | SHIPPABLE | none — reuses `kStyleRoute` + existing public accessor; needs one small new public accessor if full `solo_active` parity with `part_silenced` is wanted (currently private, `arranger.hpp:552-555`) |
| `kOneShot` fixed-gate constant for Drum (Decision 3) | SHIPPABLE | none — a compile-time constant, no field growth, no dependency |
| CC off-value hardcoded to 0 (Decision 4) | SHIPPABLE | none |
| A `Pad`-carried gate/duration field, or a second CC "off value" field | INSTRUCTIVE-BUT-INFEASIBLE | breaks the pinned 12-byte `Pad`/D33 RAM budget; would need an explicit owner decision to grow the struct and re-pin, out of scope for this item |

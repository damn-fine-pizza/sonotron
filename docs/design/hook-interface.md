# The Hook Interface — sonotron's uniform component language (proposal, NOT frozen)

Status: **design proposal for owner review.** Architecture and interface shape only — no
implementation. Written under an explicit, owner-granted lift of the v1 command/event ABI freeze
(node 11720): this proposal is free to replace `Op`/`Param`/`Command`/`OutEvent`
(`components/arrangrr/include/arrangrr/abi.hpp`) wholesale where that yields the cleaner design.
It supersedes the "additive/shippable" framing of `docs/design/hooks-design-notes.md` — that
document's factual excavation (the bifurcated channels, the two Param lineages, `Op::kGet` dead
code, the `kLivePriority` pattern) is still the evidence base; only its "must stay additive"
constraint is lifted. Companions: `docs/design/workstation-vision.md` (the product's own words:
*"[arrangrr] exposes hooks on EVERY component ... with two faces — observability ... and
interaction"*), `docs/design/project-structure.md` (D43 name-blind components / POD ports),
`docs/design/gui-contract-map.md` (the concrete gaps this closes).

## 0. Grounding — what exists today, cited

- **The binary ABI.** `components/arrangrr/include/arrangrr/abi.hpp`: `Op{kSet,kDo,kGet}`,
  a flat `Param` enum (43 leaves, two colliding lineages — see below), `Command{op, param, idx,
  a, b, c}` (≤20 bytes), `OutEvent{kind, port, msg, tick, code}` (≤16 bytes) with exactly 5
  `Kind`s. `kProtocolVersion = 1` (`version.hpp`).
- **Two colliding addressing lineages inside `Param` itself:** `kGroove`/`kArp` already do
  `(family, field-in-`a`, value-in-`b`)`; everything else (`kChordHold`, `kPartMute`,
  `kPartSolo`, `kTransportTempo`, …) is one enumerator per leaf. Both lineages are live in the
  same enum today.
- **`Op::kGet` is dead.** `Engine::push_command` (`components/arrangrr/include/arrangrr/engine.hpp:175`
  dispatches to `engine.cpp`'s `cmd_*` handlers) switches on `cmd.param`, never on `cmd.op`; no
  handler ever constructs or consumes `kGet`.
- **Observability is bifurcated, traced in the tree, not asserted:**
  - Channel A (crosses the process): 5 `OutEvent::Kind`s → `arrangrr::host::to_jsonl`
    (`components/hostrt/jsonl.cpp`) → `UdsServer::broadcast` (`components/hostrt/uds_server.hpp`).
    Per `docs/design/gui-contract-map.md` §3, only `kMidi/kChord/kSection/kTransport/kWarn` cross;
    `kChord` fires **only** from the recorded-sequencer path (`fire_chord_seq`,
    `components/arrangrr/include/arrangrr/engine.hpp:311-354`), never from `chord play` nor from
    live detection.
  - Channel B (never crosses): direct const accessors on `Engine` —
    `chords()/arranger()/sequences()/arp()/transport()`
    (`components/arrangrr/include/arrangrr/engine.hpp:60-65,123-124`) — read in-process by
    `components/hostrt/shell.cpp`'s `refresh_*_content()` family (e.g. `shell.cpp:134-143`
    `refresh_piano_content`, `:172-197` `refresh_styles_content`, `:206-211`
    `refresh_groove_content`, `:213-219` `refresh_arp_content`) and by `shell_input.cpp`
    (`:314,318,338,377,395,445`). The TUI's truth and the wire's truth are two different reads of
    two different surfaces — they can (and today do, per `gui-contract-map.md` §5) disagree.
- **The one arbitration mechanism that already exists** — `ChordFollow::kLivePriority` — lives
  split across a static gate (`FollowedContext::may_follow`,
  `components/arrangrr/include/arrangrr/chord/followed_context.hpp:132-149`) and a dynamic
  decision at the `fire_chord_seq` call site (held-vs-released state,
  `components/arrangrr/include/arrangrr/engine.hpp:311-323`). `FollowedContext` also carries the
  pattern this proposal generalizes: an **explicit-origin latch** (`m_explicit`) that makes a
  passive default (`establish_default`) a no-op once any real producer has committed, while a true
  reset (`reset`) can still overwrite it — see `followed_context.hpp:63-161`.
- **Quantization-to-boundary already exists, inconsistently spelled.** `kChordPlay` overloads
  `idx` as an immediate/next-bar flag; `kStyleSwitch` overloads `c` for the same concept
  (`abi.hpp` comments on both enumerators). `Engine::advance_ticks`
  (`engine.hpp:185-211`) is the single sequential point where bar boundaries are recognized
  (`m_transport.tick() % kTicksPerBar == 0`) and where `m_chords.commit_bar()` already runs before
  `fire_arranger` — this is the one true "commit point" in the whole engine.
- **Concurrency reality.** Everything is single-threaded and sequential today (UDS `poll()`,
  `exec_line`, `push_command`, `advance_ticks`); `RingBuffer` is declared but unused in
  production. There is no thread barrier to design against yet.
- **D43 / the orchestrator direction** (`docs/design/project-structure.md` §"The orchestrator +
  the composable pipeline", DESIGN.md `0910`): future peer components (`melodd`, `samplrr`,
  `orchestrator`, and eventually `sequencrr`) are **name-blind**, wired only through a small POD
  port. `components/orchestrator`, `components/melodd`, `components/samplrr` are today empty
  slots (`README.md` stubs only, verified — no code exists to trace).
- **Doctrine that binds this proposal regardless of the ABI freeze:** `0200`/`0300`/`0700`/`0800`
  in `docs/DESIGN.md` (no heap, no RTTI/exceptions/`std::string` in the core, dual-target
  freestanding, append-only-or-versioned binary ABI **as a discipline**, dependency-free core) —
  these are identity, not the frozen *shape*, and this proposal does not touch them.

### 0.1 Genesis / evidence base (rescued from `hooks-design-notes.md`, retired 2026-07-11)

The following grounding items are preserved verbatim in intent from the retired excavation doc, so
the genealogy and the still-open threads survive its deletion:

- **This is not a new principle — it is the unpaid debt of D17b / DESIGN.md §24.** The owner's
  "every component exposes uniform hooks (observability + interaction)" was already designed in the
  docs as a *uniform, addressable L1 param-space with declared get/set/do access + MIDI-learn*; it
  was simply never made real in the binary. Everything §0 above excavates (`kGroove`/`kArp` as the
  one lineage that already generalizes, `Op::kGet` dead, the bifurcated channels) is that debt
  showing through. This proposal is the payment.
- **Traceability — `kChordFollowed` is NOT in `main`.** It lives on the `spikes/kchordfollowed`
  branch (the first instance of the "promote a channel-B fact to a channel-A event" move).
  **Reconciliation (2026-07-11):** this spike pointer is now *superseded* by `ux-workstation.md`
  §11, which promotes `kChordFollowed {current, pending, valid, source}` to a **front-of-line
  [P0] CORE** deliverable, fired on every commit (manual/detect/sequencer/bar-promote). Treat that
  plan — not the spike branch — as the live source of truth for this fact.
- **Proposal F — the VST / hosting seam.** VST hosting has zero trace in the core, correctly. The
  seam is exactly where the binary `OutEvent`/`Command` ends: the outer product receives events
  (observability hooks) and sends overrides (interaction hooks) from *outside* the core process.
  The core rule: **never `#include` a VST header inside `components/arrangrr`.** Where the Director
  outer-product physically lives on disk is a file-layout call — "a Palladio call".
- **Open item — MIDI-learn is unverified.** It is *not verified* whether MIDI-learn exists anywhere
  in the tree (only a targeted grep was ever run). D17b/§24 name it as part of the same param-space
  vision; whether any of it is wired remains an open excavation, not an assertion.

## 1. The core idea

**One POD language.** Every hookable fact in every component — Director, Arranger, sequencers,
harmony, groove, arp, looper, MIDI-FX, and (later) orchestrator/melodd/samplrr — is addressed by
the same triple:

```
(ComponentFamily family, InstanceId instance, FieldId field)  →  Value{a, b, c}
```

- `family` replaces `Param`'s dual duty (today `Param` is *sometimes* a leaf, *sometimes* a
  family-with-a-field-in-`a`). After this proposal it is **only ever a family** — `kTransport`,
  `kRouting`, `kHarmony` (the `FollowedContext` + `ChordEngine` surface), `kChordDetector`,
  `kChordSequence` (collection), `kArranger`, `kGroove`, `kArp`, `kTrack` (collection), `kPart`
  (collection over roles), `kMidiFx` (reserved, collection over track×slot), and forward slots for
  cross-process peers (`kOrchestrator`, `kMelodd`, `kSamplrr`, `kDirector`) — see §5.
- `instance` generalizes today's `idx` (already used for tracks/sequences) into the **uniform**
  collection selector for every family, including singletons (`instance = 0`).
- `field` is the family-local leaf — exactly what `GrooveField`/`ArpField` already are, now the
  **only** lineage; the `kChordHold`/`kPartMute`-style one-enum-per-leaf lineage is retired.
- `Value{a, b, c}` is the **unchanged payload shape** `Command` already carries — three
  `int32_t`s. No new union, no tag, no third payload kind: every existing packing convention
  (`kTrackStep`'s bit-packed param-locks, `kChordPlay`'s packed note bytes) rides unmodified.

This is deliberately **not** a new abstraction bolted beside the old one — it is the two existing
lineages honestly merged into the one that already generalizes (`kGroove`/`kArp`), with `idx`
promoted from a track-only convenience into the interface's uniform instance axis.

### 1.1 The two faces

- **Observability** = `Op::kGet` (now real) + a delta-on-change push (`FieldEvent`, §3). A caller
  can ask "what is `(family, instance, field)` right now" and get a synchronous answer, and/or
  passively receive every change as it commits.
- **Interaction** = `Op::kSet` / `Op::kDo`, each carrying **who is asking** (`Origin`, §4) and
  **when it should land** (`Boundary`, §4). This is the same two ops that exist today
  (`kSet`/`kDo`); nothing new is invented here — they gain two small, uniform envelope fields.

### 1.2 The commit primitive

```
struct HookCommand {
  Op              op;        // kGet | kSet | kDo   (unchanged set of ops)
  ComponentFamily family;    // replaces Param's dual role — a family, never a leaf
  std::uint16_t   field;     // family-local leaf id (was GrooveField/ArpField, now universal)
  std::uint16_t   instance;  // was `idx`; 0 for singleton families
  Origin          origin;    // kDefault | kDirector | kHuman  (§4 — the arbitration currency)
  Boundary        boundary;  // kImmediate | kNextTick | kNextBar (§4 — replaces the ad hoc bits)
  std::int32_t    a, b, c;   // unchanged payload convention
};
```

Sizing note (not a commitment, an existence proof): `1+1+2+2+1+1+12 = 20` bytes — the **same**
budget `Command` already has today (`static_assert(sizeof(Command) <= 20)`). The uniform
interface does not need to grow the envelope, only reinterpret it; final packing is an
implementation call (Fabrizio/Nazzareno), not an architectural one.

## 2. Snapshot-on-connect (`Op::kGet`, given a body)

A caller sends `HookCommand{op = kGet, family, instance, field}`. Two shapes:

- **Point read:** `instance`/`field` concrete → the owning component emits exactly one
  `FieldEvent` for that leaf, synchronously, on the same call stack (no queue, no "next tick" —
  a `kGet` is never staged; it answers with the value *right now*, even if a write is pending for
  a future boundary — the pending value is a separate, inspectable fact, not a lie by omission).
- **Wildcard read:** `instance = kAllInstances` and/or `field = kAllFields` → the component walks
  its own declared field table (each component already knows its fields — this is exactly what
  `GrooveField`/`ArpField`/`FollowedContext`'s `state()/pending()/explicit_set()` already expose,
  just not yet through one gate) and emits one `FieldEvent` per leaf, in one synchronous burst,
  through the same sink already threaded through `push_command`.

This closes gap #3 in `gui-contract-map.md` (*"No state-on-connect sync ... `Op::kGet` appears
unwired"*) with **no second read mechanism**: the wildcard `kGet` *is* the snapshot. A golden
session can emit `get *` at `@0` and diff the resulting FieldEvent burst exactly like any other
event block — determinism is free, not bolted on.

## 3. Observability: one channel, not two

**`FieldEvent` replaces Channel A's `kChord`/`kSection`/`kTransport` and Channel B's direct
accessors, at the same source.**

```
struct FieldEvent {
  ComponentFamily family;
  std::uint16_t   field;
  std::uint16_t   instance;
  Origin          origin;   // who committed this value — lets a GUI show "you" vs "the Director"
  Tick            tick;
  std::int32_t    a, b, c;  // same payload convention as HookCommand
};
```

Emitted **delta-on-change**, at the sites that already exist and already know when something
changed: `fire_chord_seq`'s `sink(OutEvent::chord(...))` call
(`engine.hpp:345-347`) becomes `sink(FieldEvent{kHarmony, ...})`, and — this is the actual gap fix
— the SAME emission now also fires from `chord play` (`chord_play`, engine.cpp) and from live
detection (`observe_chord_input`, `engine.hpp:398-420`), because all three are now producers
writing through the same `(kHarmony, field=kCurrent)` slot instead of two of them writing into
`FollowedContext` silently and only the third telling anyone. `fire_arranger`'s section-change
(`engine.hpp:362-364`) and `advance_ticks`'s transport-state emission become `FieldEvent{kArranger,
kSection, ...}` and `FieldEvent{kTransport, kState, ...}` the same way.

**What does *not* fold into `FieldEvent`:**
- `OutEvent::kMidi` stays its own kind, unchanged shape. It is not a hook — it is the sounding
  stream a hardware synth or the MIDI monitor consumes; folding it into "component state" would be
  a category error (a note-on is an occurrence, not a value at rest).
- `WarnCode`/`kWarn` stays its own lightweight kind for the same reason: a warning is an event
  that *happened*, not a field with a resting value. (Flagged as an open question in §7 — a
  "diagnostics" pseudo-family is possible but not obviously cleaner.)

So the wire/in-process envelope shrinks from 5 `Kind`s to **3**: `kMidi` (unchanged), `kField`
(new, universal, replaces `kChord`+`kSection`+`kTransport` and everything Channel B used to leak),
`kWarn` (unchanged). The 3rd kind is now infinitely extensible by adding a `(family, field)` pair
— never again a wire-shape change to add one more observable fact.

### 3.1 The TUI reads the same channel a remote client would

This is owner decision 1, made literal. Today `shell.cpp`'s `refresh_*_content()` functions call
`m_engine.arranger()/chords()/arp()/transport()` directly (cited in §0). The proposal introduces
a `HookMirror` — host-only, a bounded table keyed by `(family, instance, field) → FieldEvent`,
populated **exclusively** by the `FieldEvent` stream (the same stream a socket client receives,
JSONL-encoded, via `to_jsonl`) plus one wildcard `kGet` issued at shell startup to seed it. Every
`refresh_*_content()` is rewritten to read the mirror, never `m_engine` directly. Writes
(`push_command`) stay direct in-process calls — interaction was never bifurcated, only
observation was — but every write now carries `Origin::kHuman` (§4) so the TUI's own input is
indistinguishable, at the interface, from a remote GUI's or a MIDI-learn binding's.

Once this lands, `Engine`'s public const accessors (`chords()/arranger()/sequences()/arp()/
transport()`) have no product caller left outside the components that build `describe()`/`kGet`
responses from them — they should be demoted to a private or test-only surface, not deleted
immediately (the mirror's correctness is proven *against* them first). This is the concrete,
non-trivial host refactor the owner already priced in.

## 4. Human always wins — the uniform arbitration

`Origin` is carried on **every** `Set`/`Do` `HookCommand`, not only chord commands:

```
enum class Origin : std::uint8_t { kDefault = 0, kDirector = 1, kHuman = 2 };
```

Ordering is the rule: **kHuman > kDirector > kDefault**, always, for every family — this is
`kLivePriority` generalized past harmony, exactly per owner decision 2. The mechanism a family
uses to enforce it is the pattern `FollowedContext` already proves out (§0):

1. **An explicit-origin latch per field.** A field remembers whether any non-default origin has
   ever committed it (`FollowedContext::m_explicit`, generalized). A `kDefault` write
   (`establish_default`-equivalent) is a no-op once the latch is set — a passive default can never
   clobber a real decision, human or Director.
2. **At the commit boundary, `kHuman` beats `kDirector`.** If both a Director-origin and a
   Human-origin write target the same `(family, instance, field)` in the same boundary window,
   the Human one lands and the Director one is dropped for that window (not corrupted, not
   silently merged — a component may choose to re-emit its own `FieldEvent` confirming what
   actually landed, so a Director-side caller can see it was overridden).
3. **Boundary, not thread.** `Boundary::kImmediate` lands on the same call (matches most of
   today's `kDo` commands); `kNextTick`/`kNextBar` stage into the same kind of pending slot
   `FollowedContext::stage()`/`commit_bar()` already implements, consumed at the one real
   sequential commit point in `Engine::advance_ticks` (`engine.hpp:199-206`) — never mid-tick, by
   construction, because there is exactly one place ticks advance. This collapses today's two
   overloaded ad hoc bits (`kChordPlay`'s `idx`, `kStyleSwitch`'s `c`) into one envelope field
   used identically by every family (closes proposal D — the quantization-bit consolidation
   excavated in §0's grounding, the "Quantization-to-boundary already exists" bullet — for real).

**Where the Director lives, architecturally: nowhere inside this core.** The core does not know
what a Director *is* — it only ever sees `Origin::kDirector` on an inbound `HookCommand`, arriving
through the exact same entry point a human keystroke or a remote GUI click uses. This is what
keeps `0800`/D4 (core stays dependency-free) and the STM32-capable identity intact: a firmware
build that never links a Director still compiles and behaves correctly, because "Director" is a
*caller role*, not a core dependency. It is also what makes this the D43 component port, not a
GUI-specific convenience: `melodd`/`samplrr`/`orchestrator`, whenever they exist, speak the exact
same `(family, instance, field, origin, boundary) → value` language to arrangrr and to each other
— the hook interface IS the name-blind POD port `project-structure.md` asks for, not a second
thing built beside it.

## 5. What survives, what changes, what is discarded (breaking, explicitly)

| Surface | Disposition |
|---|---|
| `Op{kSet,kDo,kGet}` | **Kept**, unchanged values. `kGet` stops being dead code. |
| `Param` (43-leaf flat enum, two lineages) | **Discarded.** Replaced by `ComponentFamily` (family only) + per-family `FieldId` (the `GrooveField`/`ArpField` pattern, generalized to every family). No leaf keeps its old *enumerator identity* — every existing `Param` value gets re-expressed as `(family, field)`, most 1:1 (e.g. `kTransportTempo` → `(kTransport, kTempo)`), a few genuinely merge (`kPartMute`+`kPartSolo` → one `kPart` family, `field ∈ {kMute,kSolo}`, `instance = role` — today two Params, tomorrow one family). |
| `Command{op,param,idx,a,b,c}` | **Discarded**, replaced by `HookCommand` (§1.2) — same payload convention, generalized envelope, +`origin`+`boundary`. |
| `OutEvent{kind,port,msg,tick,code}`, 5 `Kind`s | **Narrowed to 3 kinds** (`kMidi` unchanged, `kField` new/universal, `kWarn` unchanged). `kChord`/`kSection`/`kTransport` **discarded** as distinct kinds, subsumed into `kField`. |
| `WarnCode` | **Kept**, unchanged. |
| `kMaxInserts` / MIDI-FX reservation | **Kept** as a slot; the eventual `kFx*` verbs are born directly as a `kMidiFx` family (`instance = track*kMaxInserts+slot`) — cheap, since nothing has shipped there yet. |
| `kProtocolVersion` | **Bumped to 2.** The "FROZEN v1" banner and `test_abi_frozen.cpp`'s pins describe a surface this proposal replaces; carrying a comment that claims permanence over a shape we are about to break would be a structural lie the moment this lands (flagged in §6, not decided here). |
| L1 text grammar (§23/§24 DESIGN.md) | **Reshaped, not deleted.** `groove swing 40` still expands L2→L1→wire; the wire target is now `(kGroove, field=kSwing, instance=0)` instead of `Param::kGroove{a=field}` — a host-side expansion-table change (`shell_parse.cpp`), invisible to the musician-facing grammar. |

## 6. Independent implementation slices

Sized to hand to separate implementors/agents; sequencing noted where real (mostly: the envelope
lands first, everything else can fan out). Every slice is CORE (freestanding, no-heap) unless
marked HOST-ONLY.

- **Slice 0 — the envelope.** `ComponentFamily`, `Origin`, `Boundary`, `HookCommand`, `FieldEvent`
  in `components/arrangrr/include/arrangrr/abi.hpp` (or a sibling `hooks.hpp`); bump
  `kProtocolVersion = 2`; rewrite the "FROZEN v1" banner comment to say what it now is (superseded,
  not perpetual). **CORE. Blocking for everything else. No new dependency.**
- **Slice 1 — per-family `describe()`/dispatch, split by family (parallelizable once Slice 0
  lands, each touches disjoint files):**
  - 1a Transport + Routing (`transport/transport.hpp`, `routing/router.hpp`)
  - 1b Harmony (`chord/followed_context.hpp`, `chord/chord_engine.hpp`, `chord/chord_detector.hpp`,
    `chord/chord_sequencer.hpp`) — the reference implementation; promote `FollowedContext`'s
    existing latch pattern to the general algorithm rather than rewriting it.
  - 1c Arranger (`arranger/arranger.hpp`, `arranger/groove.hpp`, style/section/parts)
  - 1d Arpeggiator (`arp/arpeggiator.hpp`)
  - 1e Timeline/Track (`timeline/timeline.hpp`)
  Each sub-slice: add a `describe(EventSink)` (wildcard `kGet` body) + accept `HookCommand`-shaped
  set/do, own unit tests. **CORE. No new dependency.**
- **Slice 2 — `Engine` dispatch rewrite.** `engine.hpp`/`engine.cpp`: replace the `Param`-keyed
  `cmd_*` switch with a `family`-keyed dispatch to the owning component; rewire
  `fire_chord_seq`/`fire_arranger`/`advance_ticks`'s event emission to `FieldEvent`. **CORE.
  Single high-fan-in file — one owner, not parallelized. Depends on Slice 0; can scaffold against
  family stubs ahead of Slice 1 completing everywhere.**
- **Slice 3 — host wire encoding.** `components/hostrt/jsonl.cpp`/`.hpp`: table-driven
  `to_jsonl`/`to_human` for `FieldEvent` (a `(family,field)` → JSON-key/name table) replacing the
  current per-`Kind` switch. **HOST-ONLY. Depends only on Slice 0's enum values — can start in
  parallel with Slice 1/2.**
- **Slice 4 — snapshot-on-connect wiring.** New `get`/`get *` L1 verb
  (`shell_io_commands.cpp`/`uds_server.cpp`) issuing the wildcard `kGet`. **HOST-ONLY. Depends on
  Slices 0+2.**
- **Slice 5 — the TUI `HookMirror` + panel refactor.**
  - 5a the mirror itself (new `components/hostrt/hook_mirror.hpp/.cpp`) — fixes the read API first.
  - 5b..5f one per panel-refresh function (`refresh_piano_content`, `refresh_styles_content`,
    `refresh_parts_content`, `refresh_groove_content`, `refresh_arp_content`, `refresh_chords_
    content`) — genuinely disjoint code once 5a's API is frozen, good parallel-agent split.
  **HOST-ONLY. Depends on Slices 0, 2, 3, 5a. No new dependency — `hostrt` already uses
  `std::unordered_map`/`std::vector` (`uds_server.hpp`), so the mirror needs nothing new; §7 item 5
  flags the storage-shape choice.**
- **Slice 6 — `Origin` at the input edges + L1 parser rewrite.** Tag every human-keystroke
  `push_command` call site (`shell_input.cpp`) with `Origin::kHuman`; rewrite `shell_parse.cpp`'s
  L1→wire expansion table from `Param` to `(family, field)`. **HOST-ONLY. Depends on Slice 0;
  parallelizable by file against Slice 1/2/3 once the enum values are pinned.**
- **Slice 7 — ABI-pin test replacement.** Retire/relabel `test_abi_frozen.cpp`'s v1 pins; add the
  v2 equivalent pinning `HookCommand`/`FieldEvent` shapes and enumerator values. **CORE test.
  Land in the same change as Slice 0 — there must never be a window where the ABI is unpinned.**

Nothing in this list requires a new dependency, host or core; every slice stays inside doctrine
`0800`/D4. Flagged for the owner if that ever stops being true (it currently is not).

## 7. Open questions / forks for the owner

1. **Position/beat heartbeat cadence (closes gui-contract-map gap #2).** `FieldEvent` doctrine is
   delta-on-change; tick position changes *every* tick, so naively wiring `(kTransport, kPosition)`
   the same way would flood the channel (and violate the "never per-tick" clause proposal B
   already established). Two real shapes: (a) declare a coarser, per-family emission cadence for
   this one field (e.g. once per beat, not per tick), or (b) make position **poll-only** —
   observable exclusively via a repeated point `kGet`, never pushed. These have different cost/UX
   trade-offs (a playhead needs (a); a static inspector is fine with (b)). **Needs an owner call**
   — it also decides whether the interface stays purely synchronous-request-response or gains its
   first genuinely periodic push.
2. **`kProtocolVersion` bump to 2 + retiring the "frozen v1" banner/test now, even though no real
   external GUI client ships yet.** Nothing currently depends on v1 outside the codebase's own
   spike/stub (`node 11600` is unbuilt). Bumping is the honest move (§5), but it is a doctrine
   statement — the owner should sign off explicitly rather than have it happen as a side effect of
   Slice 0.
3. **Arbitration richness: flat priority vs. release/expiry.** The proposal's default (§4) is the
   simplest uniform rule — Human beats Director, permanently, once latched. But `kLivePriority`'s
   *actual* behavior today is dynamic (held-vs-released), not a permanent latch: a sequencer
   resumes the instant keys are released. A pure "human wins forever once touched" rule would
   starve a future Director on any field a human has ever nudged. The richer alternative — a
   per-field "human wins while held / for N ticks after last touch, then reopens" — is exactly
   `kLivePriority`'s dynamic half, generalized, and is a real design fork, not a triviality.
   **Needs an owner decision** before Slice 1b (harmony) is implemented, since harmony is the one
   family that must express this correctly on day one.
4. **How is `Origin::kHuman` vs `Origin::kDirector` actually verified before a Director exists?**
   Today there is exactly one producer of `HookCommand`s (`Shell`/`UdsServer`), always
   human-or-default. The arbitration logic in §4 is therefore architecturally sound but
   *empirically unverifiable* until a second producer exists. Options: (a) build a minimal,
   test-only synthetic Director-origin producer now (a scripted golden-session stub) purely to
   exercise the arbitration path before the real Director lands, or (b) defer verification to when
   the Director component is actually built. This is an architecture decision (whether the seam
   needs a stand-in to be provable) that should be routed to the owner now and, once decided, to
   Torquato for the actual test design.
5. **Storage shape of the host-side `HookMirror` (Slice 5a).** A bounded POD table indexed by
   `(family, instance, field)` (consistent with the "everything is a flat indexable table" spirit
   the core already uses, e.g. `kMaxTracks`-sized arrays) vs. a `std::unordered_map` keyed the
   same way (simpler, more idiomatic host C++, already the pattern `UdsServer` uses for its client
   table). Host-only, no doctrine violation either way — a style/consistency call, not a structural
   one, but worth pinning once so the five panel-refresh slices (5b..5f) don't disagree on the
   read API's shape.
6. **`kWarn`/diagnostics: distinct kind, or a `(family, field)` pseudo-family?** §3 keeps `kWarn`
   as its own lightweight kind — a warning is an event that *happened*, not a field at rest. Folding
   diagnostics into a `kDiagnostics` pseudo-family is possible but not obviously cleaner. Left as an
   owner call, not decided here.

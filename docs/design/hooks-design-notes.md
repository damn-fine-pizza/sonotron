# Hooks — design notes (exploration, NOT frozen)

Status: **exploration to shape the "hooks everywhere" principle** (Corelli, grounded in the real
tree). Nothing here is committed to the ABI. Companion: `docs/design/workstation-vision.md`
(the principle), `docs/design/gui-contract-map.md` (the Phase-0 gaps).

## The reframe

The owner's "every component exposes uniform hooks (observability + interaction)" is **not a new
principle — it is the unpaid debt of D17b / DESIGN.md §24** (a uniform, addressable L1 param-space
with declared get/set/do access + MIDI-learn), designed in the docs but never made real in the
binary. Building blocks already half-exist:
- **Uniform "component + field + value" addressing** already exists *in nucleus*: `kGroove`
  (a=`GrooveField`, b=value) and `kArp` (a=`ArpField`, b=value) do exactly this. It was never
  generalized, and a second lineage ("one Param per leaf": `kChordHold`, `kPartMute`/`kPartSolo`)
  contradicts it.
- **`Op::kGet` exists but is DEAD** — dispatch is on `cmd.param`, never `cmd.op`; `kGet` is only a
  frozen `static_assert`, never constructed/consumed. It is the snapshot-on-connect mechanism
  waiting for a body.

## The structural truth: observability is BIFURCATED today

- Channel A (poor, crosses the process): the 5 `OutEvent::Kind` (`kMidi/kTransport/kWarn/kChord/
  kSection`) via `EventSink` → JSONL over UDS. A remote client sees only these.
- Channel B (rich, crosses nothing): direct C++ accessors `Engine::chords()/arranger()/
  sequences()/arp()/transport()`, read in-process by the TUI (`shell.cpp`, `shell_input.cpp`,
  `shell_chooser.cpp`).
The Phase-0 gaps (no followed-chord, no position/beat, no snapshot-on-connect, no ack, no detector
state) ARE exactly the difference between A and B. NOTE: `kChordFollowed` is NOT in main — it lives
in `spikes/kchordfollowed` (the first instance of "promote a channel-B fact to a channel-A event").

## Proposals (all SHIPPABLE, additive, no new deps, binary/POD — no JSON/std::string in core)

- **A. One addressing lineage:** generalize kGroove/kArp → `(component_family, component_id,
  field_id) → value`; `param` becomes a component FAMILY, `a` always the field-id. No id-per-field
  explosion; fits the 43-id budget + additive freeze. Cost = rewriting the L1→binary surface in the
  HOST (where text verbs resolve), not the core.
- **B. Observability = one source of truth:** a generic `kComponentState{component_id, field_id,
  value, tick}` emitted **delta-on-change** (like kChord/kSection, never per-tick) at the existing
  `sink(...)` sites. For true uniformity the TUI must read from this too, not from direct accessors
  — a real, non-trivial HOST refactor.
- **C. Snapshot-on-connect:** give `Op::kGet` a body — client sends `kGet` on a component_id, gets
  an immediate `kComponentState`. Closes gap #3, no second read mechanism. Must stay SYNCHRONOUS
  (answer current state now; never "queue and answer at next bar").
- **D. Interaction = one quantization bit:** today `kChordPlay` uses `idx` and `kStyleSwitch` uses
  `c` for the same "immediate vs next-bar" bit — consolidate into one convention per family.
  Additive if a new field; reinterpreting an existing field would need `kProtocolVersion=2` (don't).
- **E. Human-vs-Director arbitration:** copy the `kLivePriority` pattern (static gate in
  `FollowedContext::may_follow` + runtime decision in `fire_chord_seq`, resolved before the
  bar-boundary, never mid-tick). Generalizing `ChordFollow`/`Producer` into a generic priority for
  EVERY component is a non-trivial ABI commitment. **NEEDS-DECISION.**
- **F. VST / hosting:** zero trace in core (correct). The seam is exactly where the binary
  `OutEvent`/`Command` ends: the outer product receives events (observability hooks) and sends
  overrides (interaction hooks) from OUTSIDE the core process — never `#include` VST in `components/arrangrr`.
  Where the Director outer-product physically lives is a file-layout call (Palladio).

## Concurrency note

Today everything runs single-threaded (UDS poll, `exec_line`, `push_command`, `advance_ticks`,
`broadcast`); `RingBuffer` is unused in production. So there is NO thread barrier for a hook — the
"boundary" is purely sequential/logical (bar tick). A future STM32 IRQ + main-loop split MIGHT need
a real ring buffer; assuming it now is premature.

## Owner decisions — resolved 2026-07-07 as DIRECTION (still not frozen/built)

1. **"Uniform" includes the TUI — YES.** The TUI reads via the same hooks a remote client does,
   not via privileged direct C++ accessors. One source of truth; TUI and GUI can never disagree;
   it forces the observability hooks to be genuinely complete. Cost: a real TUI host refactor,
   accepted. (Proposal B, full form.)
2. **Priority: the USER always wins, everywhere — DECIDED.** Generalize the chord-only
   `kLivePriority` ("the human hand beats the machine") into a uniform rule for EVERY component:
   **human intent > Director intent > default.** The Director proposes; the human commands.
   (Proposal E — now a committed direction, not just an option.)
3. **Give `Op::kGet` a body (snapshot-on-connect), synchronous-always — DECIDED.** A client asks
   for a component's current state and gets an immediate answer instead of being blind until the
   next change. (Proposal C.)
- Not verified: whether MIDI-learn exists anywhere in the tree (only a targeted grep was run).

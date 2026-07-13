# Clip / launch primitive (#2) — architecture design review (Corelli)

Status: DESIGN (Corelli, 2026-07-13). Phase-5 Item D (`docs/design/phase5-execution-plan.md`).
Captured from the review; feeds the #2 implementation. Coherent with the Pad/Scene
review (`pad-scene-design.md`) and the MIDI-FX review (`midifx-chain-design.md`).

## What #2 is
Give the GUI's inert hero zone (Live-Loops/Repeat grid, `ux-workstation.md` §4.4–§5)
a real launchable-cell primitive: `launch`/`stop`/`scene-quantize` verbs + a `clip`
state event, quantized to the bar/beat boundary. LIGHTER than the full Looper
(node 6000) — arm a PRE-EXISTING pattern/section to start on the next boundary,
NOT record/overdub.

## Key structural finding — three incoherent "arm-at-boundary" mechanisms today
For what is conceptually ONE primitive, the tree currently has three inconsistent
spellings:
1. `FollowedContext::stage()/commit_bar()` (chord) — real stage/commit at bar boundary.
2. `Arranger::m_pending/m_pending_style` (arranger.hpp) — a second staging, spelled differently.
3. `ChordSequencer::play()` — NO staging: starts immediately at the current tick.

And the third content a clip must encapsulate (a step track) has NO play/stop state
in `Timeline` at all (`Track` has only mute/solo). This is exactly the incoherence
`hook-interface.md` §0 diagnoses (`kChordPlay`'s idx overloads immediate/next-bar;
`kStyleSwitch`'s `c` overloads the same, "inconsistently spelled").

## Decisions
1. **Placement — `ClipMatrix` as an Engine-owned value member** (mirrors
   `ArpeggiatorEngine`), NOT a Pipeline peer (Clip orchestrates subsystems Engine
   already owns — Arranger/ChordSequencer/Timeline — it shares no raw cross-stage
   data). SHIPPABLE core, bounded POD pool `StaticVector<Clip, kMaxClips>`;
   `Clip = {TrackRole part_role, scene_index, ContentKind(kStyleSection|kChordSequence|
   kStepTrack), content_index, LaunchState}` — enum-tag + index, never a pointer/variant.
2. **Boundary hook** — inside `Engine::on_tick`'s EXISTING `tick % kTicksPerBar == 0`
   block, BEFORE `fire_arranger` (same reason the chord commit precedes the arranger).
   No new clock; reuse `m_transport.tick()`. Quantize "N bars" generalizes the check
   to `% (N * kTicksPerBar)` per-clip/per-request — same Transport, no new infra.
3. **ABI — spend the reshape budget here** (freeze lifted, F3 accepted). Corelli's
   recommendation: do NOT add a 4th ad-hoc "quantize-at-boundary" spelling. Introduce
   a real `Boundary` field on `Command` (kImmediate/kNextBar/kNextNBars) SHARED by
   `kChordPlay`, `kStyleSwitch`, and the new clip verbs — closing `hook-interface.md`
   item #3, consolidating the three mechanisms. (Additive fallback exists — new
   `Param::kClipLaunch/kClipStop/kSceneQuantize` + `OutEvent::Kind::kClip` mirroring
   chord_followed/beat — but it perpetuates the drift.)
4. **Correct the stale `FROZEN v1` banner** in `abi.hpp` (lines ~14–31) — it now
   contradicts the 2026-07-13 unfreeze. #2 is the natural first item to fix it.
5. **Scope line vs Looper (6000)** — `ClipMatrix` owns ONLY {content ref, launch
   state, pending boundary}. Tripwire: if a `record` field appears on `Clip` or a
   `capture()` method on `ClipMatrix`, that is node-6000 territory. Grep-enforced.
6. **Dual-target** — new budget static_assert in `config.hpp` for `kMaxClips`
   (DESIGN.md notes the STM32H743 budget is already tight on `ScheduledEvent`); no
   heap, no variant.
7. **GUI wiring (HOST-only, already staged)** — `render_grid_panel` takes a
   `BrainSession&` (like `render_styles_branch`); the disabled branch becomes
   `brain_session.send("launch clip " + id + " quantize " + n)`; `brain_event`
   decodes a new `kClip` at the already-empty case (brain_event.cpp ~340-341);
   `grid_model.hpp`'s `kGridLaunchWired` flips true once the verb + jsonl exist.

## Open decisions for the owner (flagged, not blocking)
- Minimal `Boundary`-reshape (recommended) vs additive-only vs the full
  `hook-interface.md` `HookCommand` reshape — three cost/benefit points.
- Per-grid vs per-launch quantize (does `Command` need a persistent quantize Param?).
- Read `runtime/pipeline.hpp` before final sign-off (confirm a new `m_clips` value
  member doesn't perturb Engine's construction order).

## Coherence with #9 / #10
- #9 Pad/Scene reuses this boundary primitive (`BoundaryLatch` there); distinct
  primitives — Clip = "what plays now" (ephemeral), Performance = "how the rig is
  configured" (rare). Only Phrase/Chord pads call the clip-launch verb.
- #10 MIDI-FX's RESERVED `Param` block (kFx*) is separate and non-conflicting.

# Song mode via SceneChain adoption

Status: PROPOSED (2026-07-18). Owner directive: adopt the core `SceneChain`
primitive as the GUI's song engine ("Song-mode completo") — each grid scene
column becomes a real `Performance`, the chain is driven by the core, and the
hand-rolled auto-song is retired.

## Context — why

The GUI's current auto-song is hand-rolled on top of the ClipMatrix clip-arm
layer and it fights the core Arranger:

- `activate_scene_column` sends BOTH `style section <name>` AND
  `launch scene <n> quantize 1` (double section-trigger). The immediate clip
  promotion (`Engine::apply_clip_content` → `Arranger::request(section,
  immediate=true)`) clobbers the queued bar-quantized section, which is why
  the `ending_clip_arms_cancelled` latch had to be added as a band-aid.
- Advance is a per-frame host state machine (`update_auto_song`,
  `next_scene_to_launch`) that re-anchors `active_scene_start_bar` on every
  launch, so a scene never accumulates repeats cleanly.
- The core already ships a real linear-song primitive, `SceneChain` (node
  8100), that is **own-transport (Fork B) — it bypasses ClipMatrix entirely**,
  counts bars itself, and applies a `Performance` per step. The GUI ignores it
  and re-implements a worse version by hand.

The CLI "felt better" because it leaned on the clean core Arranger with no
clip layer in between. Adopting `SceneChain` gives the GUI that same clean,
core-owned advance — and more, because each scene is a full `Performance`
(per-scene style / section / groove / key / tempo / routing).

## Core facts (verified)

- `SceneStep { TimeSig time_sig; u16 performance_slot; u8 n_bars;
  SceneTransitionKind transition; }` — references a `PerformanceStore` slot,
  holds `n_bars` bars, then advances. Last step holds, no implicit loop
  (`scene_chain.hpp:51-127`).
- Driven by ABI commands (no shell/`.acmd` verb needed): `kSceneAdd`
  (`a=performance_slot`, `b=n_bars | (beats_per_bar<<8)`, `c=SceneTransitionKind`),
  `kScenePlay`, `kSceneStop`, `kSceneClear` (`engine.cpp:948-1021`).
- `kScenePlay` fires step 0 synchronously; `fire_scene` drives `on_bar` at the
  engine's existing bar-boundary gate. Each transition calls
  `apply_scene_transition` → `apply_performance(perf)` + per-step `set_time_sig`
  override (`engine.cpp:1034-1054`).
- `apply_performance` (`engine.cpp:1751`) applies the WHOLE snapshot: style
  (if `style_id != 0xFFFF`) + `variation`/section, all 10 roles' routing +
  mute/solo + FX, groove, tempo, time-sig, master-transpose, key, chord-mode/
  follow, and — only if `chord_sequence_id != 0xFFFF` — `m_seq.use(id)` +
  `play(tick)` (which RESTARTS the harmony loop). It then emits a full
  confirmation dump (`emit_performance_confirmation`, ~20 events).
- Host reach: `Engine::performances()` and `Engine::scenes()` have public
  mutable accessors (`engine.hpp:152,164`). The in-process GUI host owns the
  `Shell`/`Engine`, so it can populate `PerformanceStore` slots directly and
  push the scene commands. `capture_performance()` is private → the base
  snapshot is obtained via one `kPerformanceStore` capture read back through
  `performances().get(slot)`.

## Design decisions (defaults; refine in impl)

1. **Harmony keeps flowing across transitions.** Every per-scene Performance
   sets `chord_sequence_id = 0xFFFF` so `apply_performance` never touches the
   ChordSequencer. The looping progression established at style-load
   (`append_default_progression_commands`, `seq loop on`) keeps its phase
   untouched across scene changes. (Per-scene harmony is a later concern.)

2. **Per-scene Performance = base capture + per-scene overrides.** At Play:
   capture the current live state once into a scratch slot, read it back as the
   base, then for each scene `i` clone the base and override the fields the
   grid stores for that scene (Phase 1: `variation`; Phase 2: also `style_id`,
   `groove`, `key_root/key_mode`, `tempo_x100`, `beats_per_bar`), force
   `chord_sequence_id = 0xFFFF`, and `performances().store(i, perf)`. Then
   `kSceneClear`, `kSceneAdd × N` (with `n_bars` from the per-scene stepper),
   `kScenePlay`.

3. **Ending stays host-cued on chain exhaustion.** `SceneChain` holds its last
   step (`playing()` flips true→false when the chain ends). The host watches
   that edge and cues `style section ending1` (Arranger ending → stops the
   transport), preserving the shipped #27 "last column plays out to an ending"
   behavior — without the clip-arm latch.

4. **Retire the hand-rolled machinery.** Remove the double section-trigger
   (`activate_scene_column` sends only the scene-chain build, not
   `style section` + `launch scene`), `next_scene_to_launch`/`update_auto_song`
   per-frame advance, and the `cancel_active_scene_clip_arms` /
   `ending_clip_arms_cancelled` latch. The per-scene length stepper (#6) stays
   — it now feeds `SceneStep.n_bars`.

## Phasing

- **Phase 1 — engine foundation (this workstream).** Scenes differ only by
  section (as today). Build per-scene Performances (base capture + variation
  override), drive `SceneChain`, host-cued ending, rip out the old auto-song.
  Delivers the owner's actual ask (clean core-driven looping) and establishes
  the per-scene Performance model. GridModel gains no new stored fields yet.

- **Phase 2 — per-scene editors.** GridModel grows a `ScenePerformance`
  per-scene record (style / groove / key / tempo). Per-scene editor UI. The
  Phase-1 capture-and-override picks the fields up automatically.

## Observable contract (for tests)

- With auto-song on and a multi-column grid, pressing Play issues exactly:
  `kSceneClear`, one `kSceneAdd` per populated scene (n_bars = that scene's
  stepper value), then `kScenePlay` — and NO `launch scene`/`style section`
  double-send per advance.
- The active section changes once per scene, on the bar boundary, with no
  oscillation.
- Emitted `chord` activity keeps flowing (harmony loop unbroken) across scene
  boundaries.
- When the last scene's bars elapse, exactly one `style section ending1` is
  cued and the transport stops; the song does not wrap.
- Per-scene length stepper (#6) still bounds each scene's hold to [1,8] bars.

## Test-surface impact

The existing auto-song UI-automation tests pin the OLD mechanism
(double-trigger, per-frame advance, clip-arm cancel). They must be migrated to
the new observable contract or retired. Golden tests are NOT expected to move
(this is host-side wiring over unchanged core primitives); if any move, that
needs explicit owner sign-off before regeneration.

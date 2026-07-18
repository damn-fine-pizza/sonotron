# GUI live-harmony architecture — as-built assessment (Corelli)

Status: AS-BUILT ASSESSMENT, not a from-scratch design. The mechanism this
task was framed around ("the GUI emits ZERO chord events, loops a static
default key forever") is **already fixed and shipped** on this branch
(`gui-sonotron`). This document (a) traces the live-harmony seam end to end
with citations, (b) confirms it satisfies the locked constraints (host-owned
`ChordSequence`, zero ABI, reuse of CLI wire verbs), and (c) names the one
real structural gap left for a future song-mode phase, evidenced from the
core's own `Performance`/`SceneChain` primitives rather than asserted.

## 0. The premise is stale — read this first

The task brief's root-cause diagnosis matches a real, historical bug, pinned
by `apps/gui-sonotron/tests/test_default_style_progression_functional.cpp`'s
own header comment: *"PATH-B captures ... showed 0 'chord' events and pitch-
classes pinned at {0,4,7} ... for the whole session, no matter which style was
loaded"* (docs/reflections/cli-vs-gui-ab-2026-07.md, cited there). That bug
was fixed by commit `7f07a6f` ("feat(gui-sonotron): default per-style
harmonic progression, host-only"), preceded by a design doc at
`docs/proposals/per-style-default-progressions.md` (commit `5aed414`,
attributed to Ottorino), and it sits **behind** four later commits on this
same branch (`465bb48` song-mode Phase 1, `ec5b90d`, `b4eda51`, `7784dea`).
`MEMORY.md`'s "CLI-vs-GUI divergence" entry, which frames the "0 chord
events" finding, predates the fix and is stale on this specific point (the
double-section-trigger / 8× hold / VarA-reset items it also lists are a
separate matter, out of this doc's scope — this doc covers harmony only).

This is a **vasari-status-steward** — reconciliation matter, not an
architecture defect; I flag it here because it changes what this document
needs to be (a critique of what shipped, not a blueprint for what to build).

## 1. Cosa ho tracciato — the CLI's live-harmony path, end to end

**Wire verbs** (`components/core/arrangrr/include/arrangrr/abi.hpp:91-432`,
`Param` enum — all pre-existing, none added by the fix):
- `kKeySet=14` — set: `a`=root pitch class, `b`=Mode.
- `kChordPlay=15` / `kChordStop=16` / `kChordHold=17` / `kChordOut=18` /
  `kChordMode=29` / `kChordDetect=34` / `kChordFollow=41` — the *live*
  (piano-driven) chord surface.
- `kSeqNew=19` / `kSeqUse=20` / `kSeqRec=21` / `kSeqAdd=22` / `kSeqLoop=23` /
  `kSeqPlay=24` / `kSeqStop=25` / `kSeqTranspose=26` / `kSeqDel=27` /
  `kSeqClear=28` — the *sequenced* (`ChordSequence`) surface, D28's
  `StaticVector<ChordStep,N>` model (docs/DESIGN.md:476).
- `kSceneAdd=65` / `kSceneClear=66` / `kScenePlay=67` / `kSceneStop=68` — the
  Phase-7 `SceneChain` song-mode surface (abi.hpp:377-405), a peer, not a
  competitor, to the harmony verbs above.

**CLI L1 grammar → `Command`**
(`components/platform/hostrt/shell_music_commands.cpp`):
- `Shell::cmd_key` (:151-170) → `kKeySet`.
- `Shell::cmd_chord` (:284-377) → `kChordMode`/`kChordStop`/`kChordHold`/
  `kChordDetect`/`kChordFollow`/`kChordOut`; `Shell::cmd_play` (:238-282) →
  `kChordPlay`.
- `Shell::cmd_seq` (:773-838) dispatches `seq_add`/`seq_del`/`seq_transpose`
  (:702-771) → `kSeqNew/Use/Rec/Add/Loop/Play/Transpose/Del/Clear`.

**Core dispatch** (`components/core/arrangrr/src/engine.cpp`):
- `Engine::cmd_seq` (:446-502) is the single switch over every `kSeq*` verb;
  `kSeqNew` calls `m_seq.add_sequence(m_chords.key())` (:449) which becomes
  the pool's *current* sequence implicitly — no `kSeqUse` follow-up is
  needed, matching `Shell::cmd_seq`'s own "new" grammar (shell_music_
  commands.cpp:777-788, which also never follows a `new` with a `use`).
- `Engine::seq_add`/`seq_transpose` (:520-554) mutate the current
  `ChordSequence` (append/transpose-and-re-derive, D28).
- **The forward drive is core-owned, not host-re-emitted per bar**:
  `Engine::fire_chord_seq` (`components/core/arrangrr/include/arrangrr/
  engine.hpp:767-814`) is called from `Engine::on_tick` every tick while the
  transport plays; `m_seq.on_tick(...)` advances the sequence's own step
  clock and fires `m_chords.sound(...)` on each new step, which both plays
  the chord's voicing (`Producer::kSequencer`) and — through the D47
  `ChordFollow` arbitration (`live_priority`/`comp_on_live`, engine.hpp:
  777-800) — decides whether this step is allowed to *steer* the rest of the
  band's harmonic context (`emit_chord_followed`, :807). `fire_arranger`
  (engine.hpp:816+) reads `m_chords.state()` right after, so a fired chord
  step genuinely re-harmonizes the arranger, not just a lone sequencer track.
  **Consequence for the host-side design**: once a `ChordSequence` is seeded
  and `kSeqPlay`'d, nothing needs to re-poke it bar-by-bar — the loop is
  self-sustaining inside the core.

## 2. L'architettura com'è costruita — the GUI's now-shipped mechanism

**Where the `ChordSequence` content is owned**: HOST data, not core —
`apps/gui-sonotron/src/default_style_progressions.hpp`, a `constexpr
std::array<DefaultProgression,16>` indexed 1:1 with `arrangrr::styles::
kBuiltins` (16 real progressions: blues 12-bar all-dominant, bossa ii-V-I
with smart 7ths, rock I-bVII-IV in mixolydian, funk a static I7 vamp,
motown/disco/swing/ballad turnarounds, etc. — file lines 48-146). It depends
only on `chorddet/theory.hpp` (a core *public* header for `Mode`/
`ChordQuality`) — the correct dependency direction, host reaching into core's
published vocabulary, never the reverse.

**Where it is injected**
(`apps/gui-sonotron/src/in_process_brain_session.cpp`):
- `append_default_progression_commands` (:1077-1116) turns one
  `DefaultProgression` into the exact `kKeySet`/`kSeqNew-or-Clear`/`kSeqAdd`*
  /`kSeqLoop`/`kSeqPlay` `Command` sequence a human would type at the CLI —
  same encoding (`c.b = (quality+1) | (100<<8)` on :1101-1102, byte-identical
  to `shell_music_commands.cpp:723`'s own `seq_add`).
- `DefaultProgressionState` (:1122-1134) tracks "has `kSeqNew` ever fired"
  (so a repeat style pick sends `kSeqClear`, never a second `kSeqNew` —
  avoiding the 16-slot pool exhaustion the comment at :1068-1071 names by
  name) and holds a **single pending slot** for a live `style switch`.
- `handle_style_change_progression` (:1157-1178) and
  `release_pending_progression_if_due` (:1340-1349) implement the
  phase-alignment fix: `style load` (always immediate) fires the progression
  synchronously; `style switch` while playing (which the core itself
  quantizes to the next bar, `Boundary::kNextBar`) holds the progression
  commands and releases them the instant `Transport::bar_index()` — "the
  SAME monotonic bar counter the core itself advances at every bar boundary"
  (comment, :1149-1151) — moves past the value observed at arm time. This is
  a real, evidenced fix for exactly the kind of stale-anchor class of bug
  the owner has hit before (m_section_start) — here avoided by keying off
  the core's own bar counter instead of a host-local clock.
- Wiring: `drain_command_ring` (:1322-1338) calls
  `handle_style_change_progression` right after any host-authored
  `kStyleLoad`/`kStyleSwitch` `Command` is pushed (:1334); `run_engine`'s
  main loop (:1396+) calls `release_pending_progression_if_due` every
  iteration (:1497) and owns one `DefaultProgressionState` per engine-thread
  lifetime (:1472).

**Song-mode (`SceneChain`) interaction is explicit and correct, not
accidental**: `apply_song_build` (:1254-1308) captures the live rig into
`PerformanceStore` slot 0 via `kPerformanceStore` (:1258-1266), then builds
each scene's `Performance` from that same captured base with **only**
`.variation` overridden and `.chord_sequence_id` **forced to `0xFFFF`**
(:1277) — the sentinel `Engine::apply_performance` (engine.cpp:1846-1850)
reads as "do not touch the chord sequencer on this recall". The design
comment at :1196-1197 names this explicitly ("forcing `chord_sequence_id =
0xFFFF` so `apply_performance` never restarts the harmony loop on a scene
transition"). Verified structurally sound: the running progression is never
double-triggered by a scene transition.

**Test evidence, not just code reading**: `apps/gui-sonotron/tests/
test_default_style_progression_functional.cpp`, CTest label `functional`
(`apps/gui-sonotron/tests/CMakeLists.txt:102`), drives the **real** path —
`InProcessBrainSession::send("style load <name>")` then `"transport
start"`, through the real `command_line_to_command()` translator, never a
hand-written `seq`/`key` line — and asserts, per style: `chord_event_count >
0`, more than one distinct chord label (harmony genuinely moves), and a
pitch-class-union popcount `>3` (escapes the bare `{0,4,7}` tonic triad the
old bug was pinned at). Extended to all 16 built-in styles by a documented QA
pass (file header, "TORQUATO QA PASS (2026-07-17 ... commit 7f07a6f / issue
#29)"), with one deliberate, documented exception for funk's static I7 vamp.

## 3. Confirmed constraints

- **Zero ABI, confirmed by inspection, not assertion**: every `Param` value
  the mechanism uses (`kKeySet`, `kSeqNew/Add/Loop/Play/Clear`) predates the
  fix by many phases (abi.hpp's own numbering places them at M0/early
  milestones, 14 and 19-28); the fix added **zero** new `Param` values, zero
  new `OutEvent::Kind` values, and zero core file changes (`git show 7f07a6f
  --stat` touches only `apps/gui-sonotron/src/*` and its own tests — verify
  independently if you want the raw diff, I did not re-run it here beyond
  the trace above since the code itself proves the claim: every symbol this
  mechanism calls is `Shell`/`Engine`/`abi.hpp` API that already existed).
- **`ChordSequence` is host-supplied per style, never a core `StyleDef`
  field** — matches the owner-locked decision recorded in `MEMORY.md`
  ("Harmony progression architecture", 2026-07-18) and D28's model
  (docs/DESIGN.md:476): the progression table lives in `apps/gui-sonotron/
  src/default_style_progressions.hpp`, `arrangrr::styles::kBuiltins`
  (`components/core/arrangrr/.../style.hpp`) is untouched.
- **Dependency direction is correct**: host (`apps/gui-sonotron`) depends on
  core (`arrangrr::`/`chorddet::` public headers and the `Engine`/`Shell`
  API); nothing in `components/core` or `components/platform/hostrt` was
  touched or needed to be.

## 4. Deriva dalle decisioni — none found in the harmony seam itself

No drift from a locked D-decision was found in the shipped mechanism. The one
prior drift (D-adjacent: the harmony-progression-architecture lock demands a
host-supplied `ChordSequence`; the GUI previously supplied none at all,
silently defaulting to a bare tonic triad) has been closed by this
mechanism, in the direction the lock requires (code moved to match the
decision, not the reverse).

## 5. Proposte strutturali — what is genuinely still open

### 5a. The progression-injection hook is wired to *command origin*, not to
*the fact of a style change* — a latent gap for song-mode Phase 2

**Evidence**: `drain_command_ring` (in_process_brain_session.cpp:1334) fires
`handle_style_change_progression` only when the `Command` **the host itself
just constructed** has `param == kStyleLoad || param == kStyleSwitch` — i.e.
only for the two hardcoded call sites inside `command_line_to_command`
(style.hpp `"style load"`/`"style switch"` text lines, :414-474). But
`Engine::apply_performance` (engine.cpp:1760-1854) **already supports a
style change that originates entirely inside the core**: when
`perf.style_id != 0xFFFF` it calls `m_arranger.load(perf.style_id)`
(:1767-1781), and separately, when `perf.chord_sequence_id != 0xFFFF`, it
switches the active chord-sequence pool slot and replays it (:1846-1850).
`Performance.style_id` is a real, independently-tested field
(`components/core/arrangrr/tests/test_performance_style_id_regression.cpp`,
`test_performance_validate.cpp`'s `test_style_id_sentinel_0xffff_is_valid`)
— this is not a hypothetical extension point, it is a load-bearing field the
core already validates and restores.

Today this is **dormant, not live**: `apply_song_build` (in_process_brain_
session.cpp:1254-1308) builds every scene's `Performance` from one captured
`base` and only ever overrides `.variation`, never `.style_id`, and always
forces `.chord_sequence_id = 0xFFFF` (:1276-1277) — so `SceneChain`'s
bar-driven advance (core-internal, `Engine::on_tick` → the scene step's own
`apply_performance` call) never actually changes style or chord-sequence
under the current Phase-1 scope. The gap is real but currently closed by
upstream host discipline, not by anything the architecture itself enforces
— exactly the kind of seam that looks solid until the next feature leans on
it.

**Where it breaks**: the moment a future song-mode phase lets a GUI user
assign a **different style per scene column** (a plausible next step —
`docs/reflections/gui-features-queued-behind-song-mode.md`-class backlog,
and the per-scene UI surface for section/repeat already exists in
`grid_panel.cpp`'s scene-header verbs), `apply_song_build` would naturally
start setting `perf.style_id` per scene. `Engine::apply_performance` would
then change style **from inside the core's own bar-boundary advance**,
completely bypassing `command_line_to_command`'s two hardcoded entry points
— so `handle_style_change_progression` would never fire, and the OLD
style's progression (or whatever the sentinel-`0xFFFF` discipline is
relaxed to) would keep looping under the NEW style, silently.

**Structural recommendation (HOST-ONLY, feasibility SHIPPABLE, zero ABI)**:
`Engine::apply_performance` already calls `emit_performance_confirmation`
(:1851, engine.cpp:1856+) which re-emits the **same** `kParamState(kStyleLoad)`
/`kParamState(kKeySet)` echoes the on-connect state dump uses — this is
already a clean, existing OutEvent-level signal of "the style (and key) the
engine is now on". The robust fix, when/if per-scene style ships, is to move
(or add) the progression-injection trigger from "I am the host and I just
authored this Command" to "I observed a `kParamState(kStyleLoad)` OutEvent
with a value different from what I last injected for" — i.e. react to the
**effect**, not the **origin**, of a style change. This closes the seam for
ANY future style-changing path (a Performance recall, a pad-triggered
`kPerformanceRecall`, a SceneChain step — not just the two text verbs known
today), at zero ABI cost (the echo already exists) and no core touch. I am
not implementing this now — Phase-2 per-scene style is not decided yet, and
this recommendation is scoped to "when it is."

### 5b. The mechanism's own documented expiry — relay only, not my call

`in_process_brain_session.cpp:1454-1471` already flags, in its own words, a
real future core/ABI-adjacent decision: today the progression is
unconditionally re-applied on every style load/switch, which is safe only
because "nothing in gui-sonotron today ever plays a live chord." The comment
states its own expiry condition precisely: once a live chord/detect/pad path
ships, this unconditional re-apply would silently clobber a user-steered
chord, and the fix would need `ChordEngine::explicit_set()` readback from
the GUI thread — which the comment itself calls "a real, if small,
core/ABI change requiring its own sign-off, not done here." I relay this
verbatim rather than re-deciding it: **NEEDS-DECISION**, owner-only, not
before a live-chord GUI path is scoped.

## 6. Cosa ho flaggato / cosa decide il proprietario

1. **Status-log staleness** (not architecture): `MEMORY.md`'s "CLI-vs-GUI
   divergence" entry states the GUI "emits 0 chord events" — this is no
   longer true on `gui-sonotron` as of commit `7f07a6f`, regression-tested
   by `test_default_style_progression_functional.cpp`. This is
   vasari-status-steward's reconciliation lane, not mine to fix, but the
   orchestrator should not keep re-diagnosing a closed bug.
2. **5a above (song-mode Phase 2 per-scene style)**: NEEDS-DECISION only in
   the sense of "not yet needed" — no dependency, no ABI change, purely a
   sequencing question of *when* to move the injection trigger from
   command-origin to OutEvent-observation. Flagged so it is not forgotten
   when per-scene style actually gets scoped.
3. **5b above (live-chord-vs-default-progression clobber)**: a genuine,
   self-flagged, dated core/ABI-adjacent decision already on record in the
   code itself. I did not re-litigate it; I confirm it is real and still
   open, and that no core file has been touched to work around it silently.
4. **No dependency was added, proposed, or needed** anywhere in this trace.

## Orchestrator-relayable summary

- **Recommended seam**: already built and shipped — host-owned
  `default_style_progressions.hpp` (16 real per-style progressions) injected
  through the existing `kKeySet`/`kSeqNew`/`kSeqAdd`/`kSeqLoop`/`kSeqPlay`
  wire verbs right after `kStyleLoad`/`kStyleSwitch`, bar-phase-aligned via
  `Transport::bar_index()` for live switches (`apps/gui-sonotron/src/
  in_process_brain_session.cpp`, commit `7f07a6f`). No further design work
  needed to close the originally-diagnosed "0 chord events" bug.
- **Zero-ABI: CONFIRMED**, not merely claimed — every wire verb used
  predates this fix; zero new `Param`/`OutEvent::Kind` values, zero core
  file touched.
- **Open questions for the owner**:
  (a) is the `MEMORY.md` CLI-vs-GUI-divergence entry due for a Vasari pass
  now that this item is closed;
  (b) when song-mode Phase 2 (per-scene style, if it ships) lands, route the
  progression-injection trigger off the `kParamState(kStyleLoad)` OutEvent
  echo instead of the two hardcoded command-origin sites (§5a) — no
  dependency, no ABI, just a sequencing note for that future work;
  (c) the code's own self-flagged live-chord-vs-default-progression clobber
  (§5b) remains a real, open, owner-only core/ABI-adjacent decision, not
  something to silently design around.

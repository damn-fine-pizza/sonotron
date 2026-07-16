# arrangrr — Canonical Numbered Roadmap (WBS) — PROPOSAL

Status: PROPOSAL (Verdi, 2026-07-06). This is the strategist's proposal for **the
single canonical roadmap AND the single decision record** for arrangrr. One
nomenclature only: **the hierarchical number is the identity.** Every former
milestone and every former decision becomes a NODE in this tree, with its rationale
folded into that node. There is no parallel milestone list and no parallel decisions
log; both collapse here.

> **Unresolved cross-document flag (saverio-doc-steward, recorded not resolved by
> this pass):** this file still self-describes as `Status: PROPOSAL` awaiting the
> orchestrator's merge into `docs/DESIGN.md` (see the next paragraph), yet
> `docs/DESIGN.md` §22 already carries a fully-formed, near-duplicate "canonical
> numbered roadmap (WBS) — single roadmap & decision record" with its own status
> tracking for the same node IDs. Which of the two is actually canonical, and
> whether this file should be retired/merged/kept as the working proposal, is a
> scope/ownership decision for `verdi-roadmap-strategist` or the owner — out of
> this reconciliation pass's mandate (status glyphs only, not document structure).

History is preserved in ONE place: the migration appendix at the end maps every old
identifier to its new numeric ID, so existing references in code and docs still
resolve. The BODY uses numbers only.

The orchestrator applies the accepted parts of this proposal into `docs/DESIGN.md`
later. Nothing here is code; nothing here edits DESIGN.md.

Ground-truth as read on 2026-07-06 (branch `harmony-global-steer`, HEAD `acad2c9`),
against the real core tree and the full decision history — not against the plan's
optimism.

---

## How to read and maintain this tree

**Numbering.** Hierarchical numeric IDs, 0000–99999:
- **thousands** = major area (band), e.g. `5000` MIDI-FX chain;
- **hundreds** = sub-area, e.g. `5100` insert-chain framework;
- **tens/units** = leaf item, e.g. `5310` echo/MIDI-delay insert;
- **decimals** = sub-item, e.g. `9130.1` triplet grid pass.

**The number is the only ID.** No prefixes. A node is cited as `5100`, never as a
milestone or decision code. IDs are stable for life: reordering the WORK does not
renumber the TREE — sequence lives in the "Recommended sequence" section, not in the
IDs. A new item takes the next free number in its sub-area (tens are left sparse);
a new sub-area the next free hundred; a new band the next free thousand.

**The 0000 band is different: invariants, not work.** Cross-cutting principles
(determinism, no-heap, dual-target, the STM32 budget, scope-honesty, the open
protocol, ABI discipline, dependency policy, product identity) are not schedulable
leaves — they are constraints every work-node must obey. They live in band `0000` as
declared invariants with stable numbers, so any work-node can be checked against them
("does `6000` honor `0400`?"). They have no status and no sequence; they simply hold.

**Each work leaf carries:** a stable ID, a short title, a STATUS, a feasibility
regime if open, and a folded one-line rationale (the WHY, formerly the decision text).
Where the work came from is recoverable via the migration appendix, not via inline
codes.

**Status legend.** ✅ done · ◑ partial · ▶ in-flight (this branch) · ○ planned.

**Feasibility regime (from invariants `0200`/`0300`/`0400`).** Every OPEN leaf is
labelled **SHIPPABLE** (dual-target core, no heap, bounded, cross-builds), **HOST-ONLY**
(`platform/host`/`tools`, never on device), or **NEEDS-DECISION** (an ABI shape, scope,
or dependency the owner must settle first).

---

## 0000 — Invariants & product identity (declared constraints; not schedulable work)

These bind every node below. They are the "why the schedule is honest" layer.

- **0100 Determinism as an instrument-property, where needed — not a cage.** The core
  is reproducible given identical inputs (enables golden tests, replay, debug);
  randomness only via seeded PRNG, so humanize/probability vary in live play but
  reproduce on demand. Determinism must never stiffen musical life.
- **0200 Zero dynamic allocation, compile-time-first.** No heap anywhere in the core;
  bounded static/stack/arena storage; `constexpr`/`consteval` tables; no
  RTTI/exceptions/iostream/std::string in the core. Allocation and non-determinism
  only in host tools.
- **0300 Dual-target, green from day one.** The core cross-builds host GCC +
  arm-none-eabi (freestanding subset); a feature enters the core only if it compiles
  on both. Linux is a dev/sim environment only; the platform layer stays OS-generic.
- **0400 STM32 budget envelope (falsifiable).** Anchor STM32H743 (Cortex-M7, 1 MB RAM,
  2 MB flash); all pools fit ≤512 KB with `static_assert`; hot path in DTCM;
  read-only content memory-mapped from flash; `Event` = 8-byte POD;
  ≤40k RAM-resident events + unlimited read-only in flash. Any plan that assumes
  device capacity beyond this is a fantasy.
- **0500 Scope honesty / vertical-first.** The foundational spine and the two WOWs are
  done VERTICALLY, not by opening everything horizontally. The dream list is the
  horizon; the solo-developer risk #1 is starting horizontally and closing nothing —
  so in-flight work finishes before a new band opens.
- **0600 Open / hackable, first-class.** Headless core with a stable versioned text
  protocol (commands→/events←), a uniformly addressable parameter space (stable
  ID/path per parameter), MIDI-learn/automation as first-class, state dump/inspect,
  golden/replay as the central dev workflow.
- **0700 ABI discipline.** The core contract is typed BINARY commands/events on a ring
  buffer (POD `Command`/`OutEvent`); string-path resolution and JSONL live host-side.
  Param IDs are append-only, never reused; collections are fixed-capacity `u16`-indexed
  arrays; user names live only on the host.
- **0800 Dependency policy.** The core stays dependency-free. Host-layer deps are
  allowed only if lightweight/self-contained AND evaluated with the owner first. Any
  move implying a dependency is flagged, never assumed.
- **0910 Captured direction — `melodd` audio companion — ◑ SCHEDULED (Phase-5 Item F,
  program position #6, owner-ordered 2026-07-13; `docs/phase5-plan.md`).**
  arrangrr (the MIDI brain) and a future host-only audio engine are peer modules wired by an
  orchestrator, name-blind, talking only through the POD interface; audio never
  crosses the interface; the core stays audio-ignorant (identity `0110` intact). Still
  zero code (`components/melodd/README.md`: "SLOT — no code yet") and gated on an
  explicit owner `0800` dependency approval (softsynth/DSP lib, license, weight) before
  its slot opens — an ORDERED direction now, not yet started work.
- **Product identity (what the tree is building toward):** `0110` MIDI-only, never
  audio (drives external gear, syncs by clock); `0120` live + studio use, 50/50;
  `0130` the signature is the COMBINATION — chord intelligence + unified
  write/generate/capture timeline + perfect MIDI glue + open/hackable — none dominant.

---

## The canonical tree

### 1000 — Foundations, build, harness + Timeline×Track primitive — ✅ done

- **1100 Build & toolchain**
  - `1110` Repo scaffold + CMake, two toolchains (host GCC + arm-none-eabi) — ✅
  - `1120` Freestanding containers (StaticVector/Span/RingBuffer/fixed/crc) — ✅
  - `1130` Dual-build green-from-day-one CI gate — ✅
- **1200 Realtime MIDI backbone**
  - `1210` Transport/Clock (960 PPQN scheduler, 96 grid, bpm_x100) — ✅
  - `1220` MIDI-In parser (running status, v0→Off, merge) — ✅
  - `1230` Out scheduler (4096 bounded queue, total-order emission) — ✅
  - `1240` Router + thru (in→out matrix, filters) — ✅
  - `1250` Note/Voice tracker + Panic — ✅
  - `1260` Multi-port HAL (DIN+USB abstract, 4×16) — ✅
  - `1270` Realtime hardening — sustained-play crackle defect — ▶ under
    investigation, SHIPPABLE (working regime; root cause not yet isolated)
    *reported by the owner as an active defect, not yet reproduced or committed on
    this branch as of HEAD `acad2c9`; DISTINCT from the already-fixed, HOST-ONLY,
    demo-launcher "first FluidSynth note after PipeWire stream-open" crackle
    (`apps/demo/lib/launch.sh`, commit `1bea22a`, warm-note workaround — that one is
    cosmetic demo tooling and solved). THIS defect is on the core/output realtime
    path and gates the GUI freeze line (`11700`): a crackling instrument is not
    shippable as something you play.*
- **1300 Test & protocol harness**
  - `1310` Headless golden runner (virtual clock, total order) — ✅
  - `1320` CLI shell, 3-layer protocol (Surface/Model/Wire, thin client) — ✅
  - `1330` Core binary ABI (Command/OutEvent POD ring) — ✅
  - `1340` Three-metric coverage (unit ≥80 / functional / regression) — ✅
- **1400 Timeline × Track primitive**
  - `1410` Timeline of bounded deterministic events — ✅
  - `1420` Track = role+destination+length; mute/solo; polymeter — ✅

### 2000 — Harmony core: key/scale, chord modes, detector, chord-sequencer (1st WOW) — ✅ done + ▶ in-flight consolidation

- **2100 Key/Scale engine**
  - `2110` Key/scale/mode + theory tables (diatonic default, explicit modifiers) — ✅
- **2200 Chord intelligence (modes)**
  - `2210` Diatonic mode (key-aware: one note → diatonic chord of the degree) — ✅
  - `2220` Single-finger mode (scale-aware, Casio lineage: one key → diatonic triad) — ✅
  - `2230` Shell / partial → completion mode — ✅
  - `2240` Smart quality per degree/context (V→dom7, I/IV→maj7, ii/iii/vi→min7…) — ✅
- **2300 Live harmonizer & detector**
  - `2310` ChordDetector (freestanding, chord-memory hold, no double-voice) — ✅
  - `2320` Live piano→chord steer of the running band — ✅
  - `2330` Two-zone harmony input (melody vs harmony per port) — ✅
  - `2340` Chord-follow source selector (auto / detect / sequencer / manual) — ✅
- **2400 Chord sequencer (1st WOW)**
  - `2410` ChordSequence functional storage (degree + overrides, free durations) — ✅
  - `2420` Record / loop / transpose / re-harmonize — ✅
  - `2430` Dual output (live harmonizer + editable ChordSequence) — ✅
- **2500 Followed-context ownership (▶ in-flight)**
  - `2510` Quantized next-bar chord entry (stage → commit at bar) — ◑ SHIPPABLE
    *half-built: only the detect producer stages; sequencer/manual still write the
    followed chord immediately*
  - `2520` original / current / next key readout (panel top, clip-safe) — ◑ HOST-ONLY
  - `2530` Single-owner FollowedContext consolidation — ▶ SHIPPABLE
    *one owner of the followed chord + pending; folds the follow-gate and the
    reset-vs-persist policy inside; kills three bugs by construction (transport-start
    clobber, style-load reset, self-drift). No ABI, no dep. Source:
    `docs/architecture.md`*
  - `2540` Delete dead `set_context` seam — ○ SHIPPABLE
  - `2590` *(reserved: shared-voicing split — separate "who plays the pad" from "who
    steers"; specified, not yet scheduled)* — ○ SHIPPABLE

### 3000 — Arranger & style engine (2nd WOW) — ✅ done

- **3100 Arranger resolution**
  - `3110` NTT resolver (degree/root map, wrong-note-proof) — ✅
  - `3120` Resolution pipeline (gather→expand→resolve→voice→groove) — ✅
  - `3130` Pattern-relative NoteSource (chord-tone / scale-degree / interval) — ✅
  - `3140` Voice-leading (nearest-octave, opt-in per pattern) — ✅
  - `3150` ChordGesture (strum / roll fan-out over the live chord) — ✅
  - `3160` Section model (13 section types, fills, endings, break) — ✅
- **3200 Style engine**
  - `3210` Style format (sections, per-role patterns) — ✅
  - `3220` 8 style parts (drums/bass/chord1/2/pad/perc/arp/lead) — ✅
  - `3230` Per-role register anchor + default GM voice — ✅
  - `3240` Program change per role (thin ABI slice of the external-sound model) — ✅
  - `3250` Groove engine (swing/accent/humanize/quantize-strength, seeded) — ✅
  - `3260` 16 built-ins using the modern vocab (15/16; basic kept as baseline) — ✅
- **3300 Arranger refinements (○ planned)**
  - `3310` Harmonic spillover + open voicing — ○ SHIPPABLE
  - `3320` Slash-chord / on-bass resolver — ○ SHIPPABLE
  - `3330` Per-part groove + ghost-note amount — ○ SHIPPABLE
  - `3340` Voicing applied to gesture output — ○ SHIPPABLE

### 4000 — Deep sequencer — ◑ partial

- **4100 Step parameter-locks (first increment ✅)**
  - `4110` probability (seeded position hash) — ✅
  - `4120` ratchet (evenly-spaced retriggers) — ✅
  - `4130` micro (forward-only lay-back) — ✅
  - `4140` tie (real sustain chain) — ✅
  - `4150` fire-order invariant (mutation-tested: chord-seq before arranger) — ✅
- **4200 Remaining step params (○ planned, SHIPPABLE)**
  - `4210` rest / conditional-trig — ○
  - `4220` euclidean / rotation — ○
  - `4230` bidirectional micro (needs step look-ahead) — ○
  - `4240` tie loop-seam (carry across loop restart) — ○
- **4300 Track record / overdub** — ○ SHIPPABLE *(also gates 6000)*
- **4400 CC / pitchbend / aftertouch lanes in patterns** — ○ SHIPPABLE
- **4500 External clock-in (slave sync)** — ○ SHIPPABLE
  *the one pure-interop MIDI-engine/sequencer gap; master-out only today*

### 5000 — MIDI-FX / Transform chain — ◑ partial (`5100` core shipped, rest behind the GUI freeze line, `11700`)

*A composable bounded chain (fixed max inserts) of MIDI transforms per track/zone —
the open/hackable north-star (`0600`) made concrete. Arp/groove/scale-lock become
INSTANCES of the chain, not disconnected modules.*
- **5100 Insert-chain framework** (bounded, POD) — ✅ core shipped (Phase-5 Item
  #10, commit `95f542b`, 2026-07-14): `kMaxInserts=8`, UI exposes 4.
  **Addressing re-decided by the owner from per-track to per-ROLE**
  (`kRoleCount=10`, the same ordinal space as `Arranger::m_routes` — the graft
  point is `Arranger::on_tick`, which is per-role, not per-Timeline-Track); the
  `kFx…` ABI verbs (`kFxSet/kFxParam/kFxEnable/kFxClear`, Param 53–56) and the
  reserved block are now written to that decision. v1 ships four stateless
  per-note stream-transform inserts (scale-lock, velocity-proc, echo,
  note-repeat, `components/arrangrr/include/arrangrr/fx/insert_chain.hpp`)
  grafted ahead of `groove::apply` with a per-fan-out-note grid recompute
  (existing goldens byte-identical: an empty chain is passthrough). The chain
  is live config only — **not** persisted in the `Performance` v1 format yet.
  (Mirrors the DESIGN.md §22 `5100` entry, corrected in the same commit.)
- **5200 Refactor existing modules into chain instances**
  - `5210` groove as a chain instance — ○ SHIPPABLE
  - `5220` arp as a track MIDI-FX instance — ○ SHIPPABLE *(= 7130)*
  - `5230` scale-lock / scale-filter as a chain instance — ○ SHIPPABLE
- **5300 New inserts (SHIPPABLE)**
  - `5310` echo / MIDI-delay — ○
  - `5320` note-repeat / ratchet insert — ○
  - `5330` velocity-proc / probability / randomize — ○
  - `5340` harmonize / chord-memory-expand — ○

### 6000 — Looper (the missing "capture" gesture) — ◑ partial (Phase 7 SLICE 1 shipped
ahead of the informal `11700` ordering; ABI-additive, all pre-existing goldens
byte-identical)

*Completes the write/generate/capture triad of the unified timeline (`0130`). Budget
pre-sized by `0400` (8×3072 ev = 192 KB).*
- `6100` Record / overdub / replace / erase / undo — ✅ done: `LoopBuffer`
  (`components/core/arrangrr/include/arrangrr/loop/loop_buffer.hpp`), commit `eaf9d50`
  "add node 6000 Looper primitive (Phase 7 SLICE 1)" — `kLoopNew`/`kLoopRecordStart`
  (record/overdub/replace)/`kLoopRecordStop`/`kLoopErase`/`kLoopUndo` = ABI Param
  59–63 (`abi.hpp:330-350`). Tests: `test_loop.cpp`, `test_loop_ops.cpp`,
  `test_loop_wrap_and_idempotency_regression.cpp`. Playback-capacity hardening in
  `bc693d1`.
- `6200` Quantize-after (non-destructive) — ✅ done: `kLoopRecordStop`'s quantize-grid
  argument, same commit `eaf9d50` (`abi.hpp:342-345`, "non-destructive to event
  count/positions").
- `6300` Retroactive capture (always-on ring, "grab last N bars") — ✅ done:
  `RetroCaptureRing` (`components/core/arrangrr/include/arrangrr/loop/retro_capture.hpp`),
  commit `95a3a7d` "add node 6300 retroactive capture ('grab last N bars')" —
  `kRetroCaptureArm`/`Disarm`/`Grab` = ABI Param 69–71 (`abi.hpp:391-399`); hardened by
  QA commit `6e89bda` "keep the most-recent tail on a dense retro-capture grab (6300
  QA)" + `test_retro_capture.cpp`/`test_retro_capture_ring.cpp`.
- `6400` Loop length (fixed/auto/quantized), per-track/global — ✅ done: `kLoopLength`
  = ABI Param 64 (`abi.hpp:351-354`), `LoopLengthMode` 0=auto (content-derived) /
  1=fixed (explicit tick length) / 2=quantized (snap to grid), same commit `eaf9d50`.
- `6500` Sync + follow-chord capture (re-harmonize on chord change) — ◑ partial: the
  RE-HARMONIZE half is proven end-to-end (`test_loop_reharmonize.cpp` — record → stop
  → [context changes] → launch through the real ABI, all three `LoopNoteSource`
  branches) — chord-tone-relative storage + resolve-at-playback makes re-harmonize a
  byproduct of `6100`'s own shape (`loop_event.hpp:9-19`), not new machinery. The SYNC
  half stays explicitly unbuilt: `loop_buffer.hpp:146-152`'s own comment calls today's
  overdub alignment "a SLICE-1 simplification: true bar-aligned overdub-while-playing
  synchronization is 6300/6500 territory (deferred)." Not ✅.

### 7000 — Expression — ◑ partial

- **7100 Arpeggiator engine**
  - `7110` Live-keyboard arp (rate/dir/octaves/gate/latch/seed) — ✅
  - `7120` Arp as a style part (replace hand-written kArp patterns) — ○ SHIPPABLE
  - `7130` Arp as a track MIDI-FX — ○ SHIPPABLE *(= 5220)*
- **7200 Phrase/Pad engine** (banks of 4; one-shot/loop/hold/toggle) — ◑ partial
  (Phase-5 Item #9, commit `e980665`, 2026-07-14): `PadEngine`
  (`components/arrangrr/include/arrangrr/pad/pad_bank.hpp`) ships 8 banks × 4 =
  32 flat POD pad slots, all four trigger modes (OneShot/Loop/Hold/Toggle), and
  six pad types (Phrase/Chord/SceneColumn/Variation/Fill/Performance), each a
  wrapper fanning out to an EXISTING verb (clip launch / Arranger section
  request / Performance recall) — wrapper-only by design, no new emission
  engine. Still open: `Drum`/`CC`/`NoteRepeat` pad types (deferred — they would
  need new note/CC emission, per the commit message) and
  `PadPitch::kTransposeWithChord` (captured in the struct, not yet wired to any
  dispatch behavior). Tests: `test_pad.cpp`, `test_pad_bank.cpp`,
  `test_pad_perf.cpp`.
- **7300 Groove/Humanize** — ✅ *(shared with 3250)*
- **7400 Metronome/click, tap-tempo, tempo-nudge** — ○ SHIPPABLE

### 8000 — Structure & recall + persistence — ◑ partial (`8100`/`8200`/`8500` done,
`8300`/`8400`/`8600` remain, behind the GUI freeze line, `11700`)

- `8100` Scenes / song mode (snapshot + chain + tempo/time-sig) — ✅ done (Phase 7,
  commit `67fdd13` "add SceneChain (node 8100, Scenes/song mode) + re-anchor the
  bar-boundary gate"): `SceneChain`
  (`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`) is a bounded,
  ordered chain of steps, each holding a `PerformanceStore` slot index (the
  "snapshot") + its own `TimeSig` ("tempo/time-sig" — tempo rides the referenced
  Performance), advancing at bar boundaries via `Engine::fire_scene`/`Engine::scenes()`
  (`engine.hpp:151-152,608-611`). ABI `Param::kSceneAdd`/`kScenePlay`/`kSceneStop`/
  `kSceneClear` = 65–68 (`abi.hpp:364-383`). Tests: `test_scene.cpp` (220 lines),
  `test_scene_hardening.cpp` (341), `test_scene_meter_gate_regression.cpp` (283) — 844
  lines combined. **Supersedes the former "explicitly NOT built by Phase-5 Item #9"
  note**: that note correctly described `Performance`'s own deliberately narrower scope
  AT THAT TIME (commit `e980665`, 2026-07-14); a later, separate Phase-7 pass (commit
  `67fdd13`, 2026-07-15) built node `8100` itself, exactly as its own header comment
  says. `SceneTransitionKind` ships one value (`kCut`, a hard switch) today — a future
  crossfade kind is reserved as metadata, not required by this node's own description
  (snapshot + chain + tempo/time-sig).
- `8200` Performance/Registration (recall live state) — ✅ done (Phase-5 Item
  #9, commit `e980665`, 2026-07-14): `Performance` POD (96 B,
  `components/arrangrr/include/arrangrr/perf/performance.hpp`) snapshots
  style/variation/per-role routes/mute+solo/groove/tempo/key/chord-mode/
  chord-follow/chord-sequence; `apply_performance` validates every referenced
  id before applying anything (atomic recall, no half-applied rig). ABI
  `Param::kPerformanceStore`/`kPerformanceRecall` (Param 51–52, `abi.hpp`);
  host `perf store|recall|save|load` verbs
  (`components/hostrt/shell_pad_commands.cpp`). Tests: `test_performance.cpp`,
  `test_performance_validate.cpp`, `test_performance_style_id_regression.cpp`
  (91/91 host green per the commit message). Scoped narrower than DESIGN.md
  §17's full sketch (Corelli-reviewed choice): `master_transpose` is reserved
  with no engine backing yet, routing is per-role Arranger routes only (not
  the general Router thru-matrix/Zones), and `scene_refs[]` (song mode) stays
  under `8100`, not built.
- `8300` SetList — ○ SHIPPABLE
- `8400` Project/Preset manager (slots, defaults) — ○ SHIPPABLE
- `8500` Versioned binary storage + CRC (save/load round-trip) — ✅ done
  (Phase-5 Item #9, commit `e980665`, 2026-07-14):
  `serialize_performance`/`deserialize_performance`
  (`components/arrangrr/include/arrangrr/perf/performance.hpp`,
  `components/arrangrr/src/performance.cpp`) write/read an explicit
  field-by-field little-endian wire format with `magic`/`format_version`/
  CRC32 trailer (not a struct memcpy, so `GrooveParams` padding and host/arm
  layout can't desync) — "the first real exercise of [Architectural Principle
  #8]" per the header comment. Round-trip proven by
  `test_performance_wire.cpp`; file I/O is host-only (`perf save`/`perf load`
  verbs). Scoped to the `Performance` object only — the broader §21
  multi-table Project binary format (styles/patterns/programs/device
  profiles/songs) that `8400` would need remains unbuilt.
- `8600` Diagnostics/MIDI monitor + fault-injection suite — ○ HOST-ONLY

### 9000 — Style content & tooling — ◑ partial / decided

- **9100 Per-style feel — ✅ done (model + tempo + feel-genre swing; only 9130 refinement left)**
  - `9110` Style owns its default GrooveParams — ✅ done (`Style::groove`, ABI/struct-additive,
    seeded into the arranger on every style load/switch)
  - `9120` Style owns its tempo — ✅ done (`Style::tempo`, wired through `Engine::apply_style_tempo`
    → `Transport::set_bpm` on load/switch, no new ABI; Ottorino's per-style tempos applied to all
    16 builtins; `latin` held at 120 BPM behind an owner TODO)
  - **Feel-genre swing (swing/shuffle/blues)** — ✅ done. Each of the three now seeds its `.groove`
    (swing 62/12, shuffle 72/10, blues 75/10, `swing_grid=8`) and its note table was re-authored
    (Rule R: swung eighths moved from steps 3/7/11/15 onto the even off-8ths 2/6/10/14 that
    `groove::apply` actually swings; fills/pickups/chromatic-approach exceptions preserved). The
    other 13 styles stay byte-identical. Three regression goldens (`feel_swing/shuffle/blues`) lock
    the swung ticks. Spec in `docs/style-corpus-and-generation.md` §5 (Swing re-authoring spec).
  - `9130` Triplet / shuffle grid (feel expressible in note placement) — ○ SHIPPABLE, deferred
    *(analysis: only `blues` needs a true 3-equal-subdivision grid to refine its 12/8 beyond the
    shipped 2-note shuffle; swing/shuffle are complete as 2-note swing — this is a blues-only
    quality upgrade, not a blocker)*
  *ranked the single biggest lever against style sameness (corpus measurement,
  `docs/style-corpus-and-generation.md`); feel values in
  `docs/style-corpus-and-generation.md`*
- **9200 Generative style — ◑ partial (`9210` shipped, `9220` remains)**
  - `9210` Motif + transforms (diatonic transpose/retrograde/displacement, seeded) — ✅
    done. Engine: `components/core/arrangrr/include/arrangrr/arranger/motif.hpp`, commit
    `6fe5869` "add generative motif engine first slice (9210)"; `test_motif.cpp` (487
    lines). Owner directive recorded in
    `docs/reflections/phase7-9210-motif-authoring-all16.md`: "9210 is 'done' only when
    the motif engine is wired into all 16 built-in styles, not just a blues
    demonstrator." Now true: commit `f850ea2` "wire MotifSpec into 12 built-in styles
    (Phase 7, node 9210, Option-1 batch)" attached existing per-style content as motif
    seeds across 12 styles; commit `417a674` "wire 4 Option-2 motif::generate leads
    (bossa/samba/funk/ballad)" added the last 4 (generated, not authored, seeds),
    "closes the last 4 of the 16-style kLead model gap"; commit `f46d758` un-held
    samba's Finding-B-flagged bass motif; golden regression `690b35c` locks the 4 new
    lead fixtures (26/26 goldens green, per its own commit message). Verified against
    the tree: every one of the 16 `arranger/styles/*.hpp` headers carries at least one
    `.motif=&...Spec` attachment; 15/16 also carry a `TrackRole::kLead` motif —
    `basic` is the deliberate exception (no genre convention to generate toward, per
    the authoring plan's own §3.16), matching `3260`'s already-established "basic kept
    as baseline" precedent, not a residual gap.
  - `9220` Offline-trained Markov/grammar on scale degrees, baked constexpr — ○ runtime SHIPPABLE / training HOST-ONLY
- **9300 MIDI stylizer — ◑ partial (host-only)**
  - `9310` Accompany (keep the melody, play the genre band under detected chords) — ✅ done
    HOST-ONLY. End-to-end: `components/orchestrator`'s `AccompanyPipeline` (MIDI-source →
    chorddet → arrangrr, `components/orchestrator/include/orchestrator/accompany.hpp`)
    drives the band from chords detected FROM the imported melody, not a scripted steer
    — all five sub-phases committed: 4a `Pipeline<StageT...>` composite in
    `components/runtime` (`8a0f701`), 4b `ChordDetector`+`FollowedContext` promoted to
    `components/chorddet` (`65f16bc`), 4c the SMF parser + MIDI-source stage extracted
    to `components/midisrc` (`85cd454`), Phase 4d stands up the 3-stage pipeline
    (`17f8f43`), Phase 4e wires melody-driven detection (`2e55d9b`). Proven by
    `tests/golden/accompany_basic.golden` +
    `tests/golden/accompany_melody_detect.golden` (both green, `ctest -R accompany`).
    No ABI break: `test_abi_frozen` untouched, `sizeof(OutEvent)==16` unchanged — the
    ABI waiver this pipeline could have spent (a stage/source tag) stays UNSPENT, per
    the ABI-fork analysis in `docs/architecture.md` §16.2.
  - `9320` Restyle (transform the input's own parts into the genre idiom) — ○ HOST-ONLY *(depends on 9100)*
- **9400 Style data format + generator — ◑ partial**
  - `9410` Style inspector + serialize/deserialize (offset/index-based) — ○ HOST-ONLY
  - `9420` Style compiler (data → .cpp constexpr for the device path) — ◑ partial,
    first slice IN-FLIGHT: `apps/tools/arrstyle-converter/src/style_lower.{hpp,cpp}` +
    a new `compile-style` CLI subcommand lower a `StyleModel` to a compilable
    `arrangrr::Style` header (section-slot mapping, role rename, tick/gate rescale,
    exact chord-tone reduction against each lane's own reference chord); anything
    needing a musical judgment call is dropped with a diagnostic, never guessed.
    Verified end-to-end against one real corpus `.sty` (compiles clean); deliberately
    never wired into the built-in style list or `ci.sh`. Commit `f6611e5`, currently on
    sibling worktree branch `worktree-agent-a3f3c787996a2023e`, **not yet merged onto
    this branch** — pending cherry-pick. Stays ◑, not ✅: a first slice, not full
    corpus coverage.
  - `9430` CASM→NTT importer body (arrstyle-converter; 1010-style corpus) — ◑ partial,
    real decode (not "inspect-only" as stale docs elsewhere still claim — see below):
    `apps/tools/arrstyle-converter/src/casm.cpp` is a bounds-checked CASM/CSEG/Ctab/Ctb2
    decoder and `sff_import.cpp`'s `import_sff()` (`:365-417`) actually calls it, builds
    real per-role `PhraseLane`s, and applies the bass-register filter — SFF1 fully
    decoded, SFF2 partially (`Ctb2`'s richer per-chord-group sub-structure not fully
    unpacked, `casm.cpp:132-134`). Proven by
    `test_sff_import.cpp::test_real_corpus_samples` against two real files under
    `../resources/styles/extra/**`: `ctest --test-dir build/host -R sff_import` green
    (live run recorded in `docs/phase5-design-reviews.md` §0). **Known stale spot,
    code-level not just docs:** `cmd_inspect` still routes SFF files through
    `inspect_sff()` (`sff_import.cpp:336-363`), which prints the old "unsupported
    subset — inspect-only" / "not decoded" text even though `import-sff` on the same
    file now decodes it fully — a one-line fix flagged for whoever next touches this
    tool, not blocking. Not ✅: the SFF2 sub-structure gap + the stale `inspect` message
    keep this partial. *(Correction still owed elsewhere:
    `docs/style-corpus-and-generation.md`, and
    `apps/tools/arrstyle-converter/DESIGN.md` still say CASM decode is not implemented
    — those are outside this reconciliation's file scope; flagged, not fixed here.)*

### 10000 — Generative Director — ○ planned, CAPSTONE (last; also behind `11700`)

*A top layer that PILOTS (never bypasses) the parameters of the modules below,
morphing them gradually bar-by-bar from a current expressive state toward a target.
Deterministic trajectory (`0100`), no heap (`0200`), cheap on device (`0400`).*
- `10100` DirectorState / DirectorTarget (energy/density/tension/brightness/complexity) — ○ SHIPPABLE
- `10200` Bar-by-bar interpolation engine (seeded, bounded per-bar step) — ○ SHIPPABLE
- `10300` Parameter-delta drive into arranger/style/sequencer/arp inputs — ○ SHIPPABLE
  *hard-gated: needs 3250 ✅, 7110 ✅, 4100 ✅, the voicing/scene tunable surface (3140
  done; per-part exposure open) and ideally 5000 — see sequence*

### 11000 — Host UI / clients — ◑ partial

- `11100` TUI foundation — ✅
- `11200` Piano/monitor MVP — ✅
- `11300` Presentation layer (colors/themes/unicode, richer views) — ○ HOST-ONLY (deferred)
- **11400 Piano source-coloured visualizer — ◑ partial (11410 done, 11420 deferred)**
  - `11410` GREEN (current bar) + AMBER (next/pending chord) — ✅ done (green note-on now bold;
    new `kMidiNotePending` amber role; `PianoChordOverlay` from existing host chord state, no core
    ABI; committed-green gated OFF at rest — lit only when transport plays or the chord is
    explicitly steered)
  - `11420` WHITE (direct play) — ○ HOST-ONLY *(deferred: needs a sounding melody surface)*
- `11500` UDS-JSONL control adapter (one protocol, three consumers) — ✅ done, and
  extended (Phase 3): `kParamState` given a wire shape (`param_state_wire.{hpp,cpp}` +
  `param_state_mirror.{hpp,cpp}`, commit `807c140`) and `Param::kNoteRaw` + the `note`
  L1 verb added so `apps/tools/cli-arrangrr --connect` runs as a genuinely
  arrangrr-free pure socket client (`TuiClient`+`UdsClient`+`client_event`,
  `apps/tools/cli-arrangrr/client_mode.{hpp,cpp}`, commit `b98dbda`) — both additive,
  `test_abi_frozen`-safe (`sizeof` unchanged). **Deferred, not owed** (per `b98dbda`'s
  own commit message): "deliberately does NOT split `hostrt::Shell` or retire
  cli-arrangrr's embedded live mode — that larger, riskier cutover is left for a
  follow-up"; `--connect` is a flat REPL, not full TUI parity with the embedded mode.
  `components/hostrt/shell.hpp`'s `Shell` is still one class (unsplit, verified). Scoped
  as a DEFERRED OPTIONAL follow-up (owner-decided 2026-07-13), not outstanding work
  against this node.
- `11600` Host GUI client — the TARGET of the GUI freeze line (`11700`). Tech stack —
  **DECIDED & vendored: Dear ImGui (upstream `ocornut/imgui`, pinned v1.92.8) + GLFW3,
  backends `imgui_impl_glfw` / `imgui_impl_opengl3`, under `third_party/imgui` +
  `third_party/glfw` (each with an `ARRGRR_VENDOR.md` pin), built and linked by
  `apps/gui-sonotron/`** (dependency fork resolved under `0800` and executed in code;
  rationale as-built in `docs/gui-and-ux.md` §1). — ◑ partial HOST-ONLY
  (**mechanical GUI strand COMPLETE — G0 concept demolition `38b5826`, G1 workstation
  layout `c49f8c6`, G2 brain session + G3 zone panels `473ab60` (66/66 host tests
  green); `docs/gui-and-ux.md` records "mechanical strand
  COMPLETE — G0, G1, G2 and G3 all DONE". Core-dependent strand (§11): `kChordFollowed`
  (P0-1, `52008e4`) and `kBeat`/position (P0-2, `ba568ca` + `f4c6188`) are now DONE and
  wired end-to-end (`apps/gui-sonotron/src/brain_event.cpp`, `app_state.cpp`,
  `transport_panel.cpp`; `test_chord_followed_event`, `test_brain_event`,
  `test_app_state`, `golden_chord_followed` all green) — the harmony visualizer and the
  live playhead are real, not placeholders. **Update (Repeat Zone, 2026-07-16):** the
  clip/scene launch primitive gap this bullet used to name is now CLOSED for slices
  1–3 of the `docs/proposals/repeat-zone-real-contract.md` workstream — the stale
  "`apps/gui-sonotron/src/grid_panel.cpp`'s 'awaits the core clip primitive' tooltip"
  citation this note used to carry no longer matches the tree (that tooltip string is
  gone). Commit `3398f04` "Repeat Zone real — readback + Shape-A clip binding" wires
  `AppState` to a real per-clip `{id -> LaunchState}` map reduced from the existing
  `clip` `OutEvent` (replacing a local click-time echo with honest core readback) and
  gives `kClipAdd` an explicit-`id` form (`ClipMatrix::add_at`) so the GUI's grid cells
  register real content instead of addressing an empty pool slot; commit `f531d8f`
  "renamable scene columns with scenes.json persistence" adds slice 3 (host-only scene
  naming + persistence, zero ABI). Tests: `test_app_state.cpp`,
  `test_in_process_brain_session.cpp`, `test_grid_model.cpp`, `test_scenes_json.cpp`.
  **Still open (why the node stays ◑, not ✅):** slice 4, "auto-song" — a new
  grid-column active-scene cursor auto-advancing at bar/section boundaries
  (`repeat-zone-real-contract.md` §5/§8b decision 4) — is not yet built. (Separately,
  by explicit owner decision and NOT a gap, §8b decision 2: in-app step/chord/loop
  authoring of a cell's own content from inside the GUI stays out of scope for this
  workstream — a cell's content is real only for a style dropped from the Browser;
  every other content kind is still CLI/script-only.) Toolkit dependency flag:
  RESOLVED / vendored. **Architecture
  fact (Phase 2a/2b, owner-decided):** the GUI now hosts the engine IN-PROCESS by
  default — a dedicated thread driven by lock-free SPSC Command/OutEvent rings;
  `apps/gui-sonotron/CMakeLists.txt`'s `gui_sonotron_engine` library links
  `hostrt`/`runtime`/`arrangrr` directly (commits `bf2c4b2` Phase 2a `sonotron-server`,
  `8c9e54d` Phase 2b in-process integration), with `--control <path>` kept as an
  alternative pure-client mode against an external `sonotron-server`. D38 is retired
  for that one library only — every other GUI library (`gui_sonotron_models`/`brain`/
  `layout`/`screenshot`) stays core-free — see the D38 record below. The core ABI
  itself is UNCHANGED by this: `abi.hpp` stays FROZEN v1, `sizeof(OutEvent)==16`,
  `test_abi_frozen` intact.**)

- **11700 GUI freeze line — pivot from core-feature work to the host GUI**
  (owner-decided). STATUS: ✅ CROSSED (2026-07-06). The pre-GUI batch `11710` is all ✅
  and `11720` (freeze) has executed — the ABI is frozen v1 and the `5100` shape is
  reserved. **Update (owner directive, 2026-07-13): the freeze itself is LIFTED for
  Phase-5 work forward** — resolves `docs/architecture.md` fork F3 (the
  freeze-lift is ACCEPTED; the specific ABI-reshape proposal in that document remains
  its own separate open design review), recorded in
  `docs/phase5-plan.md`: `Op`/`Param`/`Command`/`OutEvent` may be
  reshaped/rewritten for Phase-5 items, and `test_abi_frozen` may be rewritten or
  retired for those changes. This does NOT retroactively reopen the Phase-3 extraction
  wire-work recorded under `11500` (`807c140`/`b98dbda`), which shipped under the OLD
  additive-only discipline and stays byte-identical/frozen — its identity is "same
  behaviour, restructured." Core-feature work is now BEHIND the line; the next action
  is `11600` (build the host GUI). This node still governs sequencing of everything
  below it.
  *Through-line: **validate feel in the hands, then grow on a living instrument.** The
  product is a MIDI arranger — a live instrument whose value is in the hands. The TUI
  structurally cannot validate FEEL (timing, the chord-steer sensation, the piano
  visualizer's readability): feel lives in the hands, not in a text panel. So
  core-feature work STOPS at a small, well-defined batch, and the product PIVOTS to
  building the host GUI (`11600`). Every remaining musical feature (`5000`, `6000`,
  `8000`, `10000`, and the rest of `9000`) is grown AFTER, on an instrument that
  already exists and already sounds — reprioritized by real feel-in-the-hands testing,
  not by this document's current ranked order.*
  - `11710` Pre-GUI gating batch — must be ✅ before the line is crossed
    (invariant `0500`, vertical-first: the GUI is built on solid, verified ground, not
    raced onto half-built cells). **ALL FOUR cleared: `2530` ✅, `1270` ✅, `11410` ✅,
    `9100` ✅ — batch complete; the line has been CROSSED (`11720` freeze done). The next
    action is `11600` (build the GUI). (Owner-directed: freeze this code, then GUI.)**
    1. `2530` Single-owner FollowedContext consolidation — ✅ done (merged, band
       `2500`). The GUI's central interaction — steer/follow — is now correct and
       un-raced, so a visual surface can be built on top of it. **Batch item cleared.**
    2. `11410` Piano source-coloured visualizer, GREEN (current bar) / AMBER
       (pending) — ✅ done. The GUI's central *readable* surface now exists and is
       verified standalone (bold green committed, amber pending, off at rest), so the
       GUI can be built around it rather than inventing it inside the GUI build.
       **Batch item cleared.**
    3. `9100` family Per-style feel / anti-sameness — ✅ done. `9110`/`9120` (per-style
       default groove + tempo) and the feel-genre swing (swing/shuffle/blues re-authored
       for engine-driven swing, regression goldens locked) all landed; the GUI's style
       picker now presents genuinely different styles, not 16 clones. Only `9130` (a
       blues-only true-triplet refinement) remains, and it is a post-freeze nicety.
       **Batch item cleared.**
    4. `1270` Realtime hardening — sustained-play crackle — ✅ resolved (downstream
       PipeWire buffer, not a core defect; core path proven clean). **Batch item
       cleared** — no longer gates the line.
  - `11720` At-the-freeze-line actions — ✅ DONE (merged):
    - ✅ Froze the current ABI command/event surface (`0700`): `abi.hpp` carries a
      FROZEN-v1 banner with the additive-only invariant, and `test_abi_frozen.cpp`
      compile-time-pins every id value, `kWarnCodeCount`, `sizeof(Command)==20`,
      `sizeof(OutEvent)==16` and `kProtocolVersion==1` — a breaking change now fails the
      build (fix = append, or bump to v2). Corelli's verdict held: the ABI is
      additive/healthy, so later features (`5000`/`6000`/`8000`/`10000`) extend it.
    - ✅ Reserved the `5100` MIDI-FX shape as ABI-none: `constexpr kMaxInserts=8` +
      a RESERVED block documenting the future per-track `kFx…` verbs and their
      `(idx,a,b,c)` packing, no live enum values — the GUI is born aware, appended when
      `5000` lands.
    - **Phase-5 update:** this freeze is LIFTED for Phase-5 items going forward — see
      the `11700` note above (owner directive 2026-07-13).
  - `11730` Behind the line — reprioritized on a living instrument, no longer ordered
    by this document alone:
    - `11600` itself (the GUI build) is the FIRST thing behind the line — it is what
      the line pivots TO, not one of the deferred items.
    - `5000` MIDI-FX / Transform chain (framework body + inserts; shape pre-fixed).
    - `6000` Looper.
    - `8000` Structure & recall / song mode.
    - `10000` Generative Director (capstone, unchanged: still last).
    - the remainder of `9000` not in the gating batch (`9200` generative style,
      `9300` stylizer, `9400` format/tooling), `7000` remainder, `12000` device/HW.
    - **Phase-5 program (owner-ordered, 2026-07-13) — supersedes the informal order
      above.** The owner selected and ORDERED eight of Verdi's ten Phase-5 candidates
      (`docs/phase5-plan.md`): `1→7→8→2→9→6→4→10` — Restyle (`9320`) ·
      Motif (`9210`) · Corpus import (`9400`/`9430`) · Clip/launch primitive (no
      canonical node assigned yet) · Pad/Scene (`7200`/`8100`–`8200`) · `melodd`
      (`0910`) · Fuzzing harness (no canonical node assigned yet) · MIDI-FX chain
      (`5000`). Full detail: `docs/phase5-plan.md`. **Deferred out of
      this program:** #3 STM32 bring-up (`12100`), #5 external clock-in (`4500`).
      **Shipped:** the Fuzzing harness — `components/midisrc/fuzz/`,
      `option(SONOTRON_FUZZ)`, commit `c2251f2`. Pad/Scene (`7200` partial,
      `8200`/`8500` done at the time of commit `e980665`; `8100` itself shipped
      separately in a later Phase-7 pass — commit `67fdd13`, see the `8100` entry
      above). Motif (`9210`) — commits `f850ea2`/`417a674`/`f46d758`/`690b35c`, see
      the `9210` entry above. MIDI-FX chain core (`5100`) — commit `95f542b`.
      **In-flight, not yet on
      this branch:** the corpus-import lowering first slice — see `9420` above
      (commit `f6611e5`, on sibling worktree branch
      `worktree-agent-a3f3c787996a2023e`, pending cherry-pick). **Also
      resolved by this program:** the ABI freeze LIFTED for Phase-5 (see the
      `11700`/`11720` update above) and Verdi's fork #2 (the
      `sonotron`/workstation audio destination) answered by including `melodd`
      (see `0910`).
  *Dual-target note: this whole node is HOST-ONLY by construction — the GUI is a
  desktop client. The STM32 target (`12000`) keeps its OWN separate physical UI
  (`12400`); `11600` is never the device front-end — do not conflate the two
  (`0300` dual-target discipline).*

### 12000 — Device / STM32 / HW + outward interop — ○ planned / ◑

- `12100` STM32H743 HAL (USB-MIDI + UART DIN + timer + storage) — ○ device
- `12200` Real budget validation (≤512 KB envelope, static_assert) — ◑ *(asserts exist; no HW run)*
- `12300` Watchdog / crash-recovery (all-notes-off at boot) — ○ device
- `12400` Physical UI (pads/encoders/display) if/when in scope — ○ device
- `12500` External interop: DeviceProfile + ExternalSound, ordered Bank/PC/CC init, RPN/NRPN/14-bit/aftertouch, MIDI-Learn/ControllerMap — ○ SHIPPABLE
- `12600` Laptop tools: SMF import/export, style/device editor — ○ HOST-ONLY

---

## Recommended sequence for the open work (the through-line)

**Superseded in part by the GUI freeze line (`11700`), owner-decided.** The arc now
reads: **sparse input → rich harmony (done) → a band that follows correctly and no
longer sounds the same twice, hardened against realtime defects (the pre-GUI batch,
`11710`) → FREEZE (`11700`) → a living, playable GUI (`11600`) → every remaining
musical feature grown on that instrument, reprioritized by real feel-in-the-hands
testing, not by this document's ranking.** Steps 1–4 below are the pre-GUI gating
batch verbatim; everything from step 6 onward now sits BEHIND the freeze line and its
mutual order is advisory only — `11700`/`11730` is the binding word on what's deferred,
this list is kept for continuity and for ordering WITHIN the behind-the-line set.

### NEXT — the pre-GUI gating batch, in order (invariant `0500`: don't open a front over a half-built one; all four gate `11700`)
1. **`2530` — Single-owner FollowedContext consolidation.** ✅ DONE (merged, no ABI, no
   dep). Finished `2510` (immediate-commit + SHIFT-quantize) and removed the three
   reported bugs by construction. The GUI's central interaction (`11410`, and everything
   downstream: `9320`, `10000`) reads through this cell. `2540` (delete dead seam) folded
   in. **Cleared — the next open batch item is step 2.**
2. **`11410` — Piano visualizer GREEN+AMBER.** ✅ DONE (merged, HOST-ONLY, no core ABI).
   Bold green committed chord + amber pending, gated off at rest; sourced from existing
   host chord state. It makes the harmony visible, not just correct — the GUI's central
   readable surface, now verified standalone before `11600` is built around it.
   **Cleared.**
3. **`1270` — Realtime hardening: sustained-play crackle.** ✅ RESOLVED — it was a
   downstream integrated-audio (PipeWire) buffer underrun, not a core defect (the core
   output path is proven clean); fixed by sizing FluidSynth's period in the demo
   launcher. **Cleared as a `11700` precondition.**
4. **`9110`/`9120` + feel-genre swing — Per-style feel.** ✅ DONE (merged). Per-style default
   `GrooveParams` + tempo (flash-resident, dual-target, ABI-additive) and the feel-genre swing:
   swing/shuffle/blues re-authored for engine-driven swing (Rule R) with their `.groove` seeded,
   three regression goldens locking the swung ticks, the other 13 styles byte-identical. The GUI's
   style picker (`11700`/`11710`.3) now presents genuinely different styles. Only `9130` (a
   blues-only true-triplet refinement) remains, deferred as a post-freeze nicety.

### AT THE FREEZE LINE — `11700`
5. **Freeze `0700` (current ABI) + lock the `5100` shape** (`kMaxInserts=8`, UI-limited
   4, per-track first, ABI-none for the data-model increment — already locked, see the
   updated `5100` entry). **Build `11600`, the host GUI.** Tech stack (ImGui or
   alternative) is a **NEEDS-DECISION / dependency flag** for the owner under `0800` —
   evaluate it now, at this exact point, not before (nothing to build it for yet) and
   not after (the batch is closed, nothing left gating it).

### BEHIND THE LINE — grown on a living instrument (`11730`), reprioritize on real feel-in-the-hands
6. **`9310` — Stylizer: Accompany.** HOST-ONLY, **zero new core, zero dependencies**,
   reuses the resolver + detector. Cheapest possible demo of the engine's worth inside
   the new GUI: feed a plain MIDI, get the band under it. Good first candidate to
   exercise the GUI itself. Ships before `9320` (Restyle), which depends on `9100`.
7. **`5100`+`5200` — MIDI-FX / Transform chain, first increment.** SHIPPABLE core,
   shape now locked (step 5). Owner's stated strong current interest AND the `0600`
   north-star made concrete. Strategic payoff beyond the inserts: refactoring groove
   (`5210`) and arp (`5220`=`7130`) into instances gives `10000` a wider, uniform
   tunable surface. Start with the framework + 2–3 inserts (`5310`, `5320`, `5230`);
   defer `5340`.
8. **`9210` (generative style) + `9320` (Restyle).** The anti-sameness arc's second
   half. Both unblocked once step 4 lands.
9. **`4300` → `6000` — track record/overdub → Looper.** The "capture" leg of the
   unified-timeline triad (`0130`), the one gesture still entirely missing. Heavy but
   budget-sized (`0400`). Pair `4500` (external clock-in) — a looper that can't slave
   to the DAW clock is half a looper.
10. **`8100`–`8400` — Scenes / song / performance / setlist.** Structure & recall;
    prerequisite for the Director's scene targets. Storage+CRC (`8500`) rides here.
11. **`10000` — Generative Director.** Capstone, unchanged in position: only real once
    `3250`/`7110`/`4100` (done), `5000` (step 7) and the voicing/scene surface (steps
    7, 10) exist, AND it now has a living GUI to be felt through.

### DEFER (specified, not now)
- **`9400` style data format + CASM importer** — HOST-ONLY tooling; the compiled-C++
  style path works today (`0400`). Defer until authoring pain is real or the corpus
  import becomes the priority. Smallest first step: teach `arrstyle-converter` to emit
  today's constexpr header from its model. **Update: no longer deferred** — the corpus
  import became the priority (Phase-5 program item #8, position 3 of 8, owner-ordered
  2026-07-13; see the `9400`/`9420`/`9430` entries above and the Phase-5 program note
  under `11730`) and the "smallest first step" is now IN-FLIGHT (`f6611e5`, pending
  cherry-pick).
- **`11300` presentation layer; `4200` remaining step params; `3300` arranger
  refinements** — incremental polish; interleave opportunistically, none on the
  critical arc.

### CUT / hold at the horizon (no schedule)
- MIDI 2.0 / MIDI-CI, full SysEx, SMF (`12600` tail) — remain horizon per `0500`;
  nothing depends on them.
- ML style-transfer — stays OUT (`9300` scope): on-device infeasible, host-only would
  need a dependency flag.

---

## Constraints & flags that gate ordering

- **The GUI freeze line (`11700`) is the binding gate** on this whole sequence: nothing
  in "BEHIND THE LINE" schedules before `11710` is all ✅ and `11720` has executed.
- **ABI / struct changes:** `9110` is owner-approved. `5100`'s shape is now LOCKED at
  the freeze line (`11720`) — the former NEEDS-DECISION is resolved. All other open
  leaves are additive or internal.
- **No new core dependency** is introduced by any SHIPPABLE item. The former open
  dependency flag — the **GUI toolkit** for `11600` (HOST-ONLY, policy `0800`) — is now
  RESOLVED: Dear ImGui + GLFW3 vendored under `third_party/` and driven by
  `apps/gui-sonotron/` (see the `11600` entry; rationale `docs/gui-and-ux.md` §1).
- **Dual-target / no-heap reality (`0200`/`0300`/`0400`):** every SHIPPABLE leaf is
  bounded and flash/static-resident; `6000` is pre-budgeted; all ML training (`9220`)
  is HOST-ONLY, only the baked table ships. `11700`/`11600` are HOST-ONLY by
  construction — the STM32 target (`12000`) has its own separate physical UI (`12400`)
  and is never this front-end. No open leaf assumes device capacity that isn't there.
- **In-flight-first (`0500`):** the whole pre-GUI batch is closed — `2530`, `1270`,
  `11410` and `9100` (model + tempo + feel-genre swing) all landed and verified. The
  readable surface exists and the styles genuinely differ (tempo + swing). Nothing that
  `0500` guards against (opening `11600`/`5000`/`6000` over a racing followed-context
  cell, an open crackle defect, an un-readable harmony surface, or 16 same-feel styles)
  remains — the freeze line `11700` is ready to cross.

---

## Owner decisions this proposal surfaces

1. **Style-load semantics** (`2530`) — **DECIDED: keep the chord.** A style-load means
   "change the band under the same chord", not "new song" — the followed chord survives
   a style change. Shipped in the consolidation.
1b. **Live-vs-sequencer arbitration** (`2340`) — **DECIDED: live-priority.** When a
   ChordSequencer is running and the player also plays live, the live chord WINS while
   held: the band follows it and the sequencer's comp follows it too (no clash); on
   release the sequencer resumes its own progression. With no sequencer a live chord
   latches. `kAuto` (last-writer race) is retained only as an explicit legacy mode.
   **Shipped** (`ChordFollow::kLivePriority`, ABI-additive, engine default); supersedes
   the former "deferred / `kAuto`-race" note.
2. **MIDI-FX chain scope & ABI** (`5100`) — **RESOLVED by the GUI freeze line
   (`11720`):** `kMaxInserts=8`, UI-limited 4, per-track first, ABI-none for the
   data-model increment. Locked; no longer open.
3. **Sequence fork — MIDI-FX vs Looper** — **SUPERSEDED by the freeze line (`11700`):**
   neither `5000` nor `6000` runs before the GUI now; both sit behind it (`11730`).
   Their relative order (this proposal's steps 7 vs 9) is advisory only — it should be
   re-decided by real feel-in-the-hands testing once `11600` exists and sounds, not by
   this document.
4. **GUI tech stack** (`11600`, surfaced by `11700`/`11720`) — **DECIDED & vendored:
   Dear ImGui (`ocornut/imgui` v1.92.8) + GLFW3.** The dependency fork under policy `0800`
   is resolved and EXECUTED in code: vendored under `third_party/imgui` + `third_party/glfw`
   (each with an `ARRGRR_VENDOR.md` pin) and built/linked by `apps/gui-sonotron/`
   (`target_link_libraries(... imgui)`). Rationale as-built: `docs/gui-and-ux.md` §1.
5. **D38 ("the GUI never links/#includes the core")** (`11600`, surfaced by Phase 2) —
   **RELAXED / RETIRED, scoped to one library (owner-decided).**
   `docs/architecture.md`'s Corelli §15 review resolves the collision:
   the GUI binary now hosts the engine in-process by default, so `gui_sonotron_engine`
   (`apps/gui-sonotron/CMakeLists.txt`) links `hostrt`/`runtime`/`arrangrr` directly
   and names `OutEvent` — EXECUTED in commits `bf2c4b2` (Phase 2a, `sonotron-server`)
   and `8c9e54d` (Phase 2b, in-process integration). The relaxation is scoped: every
   other GUI library (`gui_sonotron_models`/`brain`/`layout`/`screenshot`) still never
   includes the core, and `--control <path>` still runs as a pure client of an external
   `sonotron-server`. Not a reshape of the ABI itself — `abi.hpp` stays FROZEN v1
   (`sizeof(OutEvent)==16`, `test_abi_frozen` intact); the ABI waiver this could have
   spent stays UNSPENT.

---

## Appendix — migration cross-reference (old identifier → new numeric ID)

The old milestone and decision codes are retired from the canonical text. This table
exists only so existing references in code/docs still resolve; the owner may drop it
once references are migrated. A single old code may map to several nodes (it was
realized across them) and several old codes may share a node.

### Former milestones

| Old | New node(s) |
|-----|-------------|
| M0  | 1000 (1100/1200/1300) |
| M1  | 1400 |
| M2  | 2100, 2210, 2300 |
| M3  | 2400 (1st WOW) |
| M4  | 2220, 2230 |
| M5  | 3000 (2nd WOW) |
| M6  | 4000 |
| M7  | 6000 |
| M8  | 7000 |
| M9  | 8000 (8100–8400) |
| M10 | 12500 |
| M11 | 8500, 8600 |
| M12 | 12600 |
| M13 | 12100–12400 |
| H1  | 11100 |
| H2  | 11200 |
| H3  | 11300 |
| Phased #1..#9 | 3220 · 3250 · 7110 · 8100 · 4100 · 3260 · 4500 · 4400/12500 · 10000 |

### Former decisions

| Old | New node(s) |
|-----|-------------|
| D1  | 0110 |
| D2  | 0300 |
| D3  | 0300, 1110, 1130 |
| D4  | 0800 |
| D5  | *(historical context — dropped)* |
| D6  | 1260, 1220 |
| D7  | 0500, 1320 |
| D8  | 0120 |
| D9  | 0500 |
| D10 | 1400, 0130 |
| D11 | 2000 (band identity) |
| D12 | 2210, 2220, 2230 |
| D13 | 2430 |
| D14 | 2410 |
| D15 | 0130 |
| D16 | 0100 |
| D17 | 0600, 1320, 1330 |
| D18 | 0500 |
| D19 | 2240 |
| D20 | 2110, 2240 |
| D21 | 8400, 8500 |
| D22 | 1320 |
| D23 | 1320 |
| D24 | 3110 |
| D25 | 5000 |
| D26 | 1330, 0700 |
| D27 | 1210 |
| D28 | 2410 |
| D29 | 1310, 1230 |
| D30 | 0500 |
| D31 | 1110, 1130 |
| D32 | 0200 |
| D33 | 0400, 12200 |
| D34 | 2310, 2320 |
| D35 | 3240, 12500 |
| D36 | 3230 |
| D37 | 10000 |
| D38 | 11500, 11600 |
| D39 | 3130 |
| D40 | 3120 |
| D41 | 3140 |
| D42 | 3150 |
| D43 | 0910 |
| D44 | 9400 |
| D45 | 2220 |
| D46 | 4100 (4110–4150) |
| D47 | 2340 |
| D48 | 2590 (reserved: shared-voicing split) |
| D49 | 2330, 2220 |
| D50 | 9100 |
| D51 | 9200 |
| D52 | 9300 |
| D53 | 2510, 2520 |
| D54 | 11400 |

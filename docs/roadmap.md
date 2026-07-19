# arrangrr / sonotron — Canonical Numbered Roadmap (WBS) — single roadmap & decision record

Status: **CANONICAL.** This is the ONE roadmap and decision record for the
project. One nomenclature only: **the hierarchical number is the identity.**
Every milestone and every decision is a node in this tree, with its
rationale folded in. There is no parallel milestone list and no parallel
decisions log; both live here. `docs/DESIGN.md` §22 is now a short pointer
to this file — see its own text for what that supersedes.

Ground-truth as read on 2026-07-18 (branch `gui-sonotron`, HEAD `a0517af`),
against the real tree (code, tests, commit history), not against optimism.
This is a full rewrite (Vasari status-reconciliation pass) that:

1. replaced this file's previous frozen 2026-07-06 (`acad2c9`) content —
   which had drifted stale and self-contradictory on several nodes (`1270`,
   the `2500` band, `5210`/`5220`/`7130`) — with the tree-true status that
   `docs/DESIGN.md` §22 had already reconciled to;
2. applied four owner-locked decisions dated 2026-07-18 (the `9100`
   anti-sameness reframe, the `11600` split into `11610`–`11650`, the new
   `11640` UI-animation node, and the new `12510` MIDI Implementation Chart
   node + `12500` split into `12520`–`12550`) — see each node's own entry
   and the "Owner decisions" section below for the citation trail;
3. archived the long forensic evidence for several already-✅ nodes to
   `docs/roadmaps/completed-nodes-archive.md`, keeping a one-line status +
   key citation live here so nothing reads as un-done. Every status change
   made in this pass is cited by commit hash or exact path; where the tree
   did not settle a question, the field was left as found and the fork is
   named, not guessed.

History is preserved in ONE place: the migration appendix at the end maps
every old identifier (D-codes, M-codes) to its new numeric ID, so existing
references in code and docs still resolve. The BODY uses numbers only.

---

## How to read and maintain this tree

**Numbering.** Hierarchical numeric IDs, 0000–99999:
- **thousands** = major area (band), e.g. `5000` MIDI-FX chain;
- **hundreds** = sub-area, e.g. `5100` insert-chain framework;
- **tens/units** = leaf item, e.g. `5310` echo/MIDI-delay insert;
- **decimals** = sub-item, e.g. `9130.1` triplet grid pass.

**The number is the only ID.** No prefixes. A node is cited as `5100`, never
as a milestone or decision code. IDs are stable for life: reordering the
WORK does not renumber the TREE — sequence lives in the "Recommended
sequence" section, not in the IDs. A new item takes the next free number in
its sub-area (tens are left sparse); a new sub-area the next free hundred; a
new band the next free thousand.

**The 0000 band is different: invariants, not work.** Cross-cutting
principles (determinism, no-heap, dual-target, the STM32 budget,
scope-honesty, the open protocol, ABI discipline, dependency policy, product
identity) are not schedulable leaves — they are constraints every work-node
must obey. They live in band `0000` as declared invariants with stable
numbers, so any work-node can be checked against them ("does `6000` honor
`0400`?"). They have no status and no sequence; they simply hold.

**Each work leaf carries:** a stable ID, a short title, a STATUS, a
feasibility regime if open, and a folded one-line rationale (the WHY,
formerly the decision text). Where the work came from is recoverable via
the migration appendix, not via inline codes. Where a node's full evidence
trail is long, the live entry here is a citation-backed one-liner and the
full paragraph lives in `docs/roadmaps/completed-nodes-archive.md`.

**Status legend.** ✅ done · ◑ partial · ▶ in-flight (this branch) · ○
planned.

**Feasibility regime (from invariants `0200`/`0300`/`0400`).** Every OPEN
leaf is labelled **SHIPPABLE** (dual-target core, no heap, bounded,
cross-builds), **HOST-ONLY** (`platform/host`/`tools`, never on device), or
**NEEDS-DECISION** (an ABI shape, scope, or dependency the owner must settle
first).

---

## 0000 — Invariants & product identity (declared constraints; not schedulable work)

These bind every node below. They are the "why the schedule is honest"
layer.

- **0100 Determinism as an instrument-property, where needed — not a cage.**
  The core is reproducible given identical inputs (enables golden tests,
  replay, debug); randomness only via seeded PRNG, so humanize/probability
  vary in live play but reproduce on demand. Determinism must never stiffen
  musical life.
- **0200 Zero dynamic allocation, compile-time-first.** No heap anywhere in
  the core; bounded static/stack/arena storage; `constexpr`/`consteval`
  tables; no RTTI/exceptions/iostream/std::string in the core. Allocation
  and non-determinism only in host tools.
- **0300 Dual-target, green from day one.** The core cross-builds host GCC +
  arm-none-eabi (freestanding subset); a feature enters the core only if it
  compiles on both. Linux is a dev/sim environment only; the platform layer
  stays OS-generic.
- **0400 STM32 budget envelope (falsifiable).** Anchor STM32H743 (Cortex-M7,
  1 MB RAM, 2 MB flash); all pools fit ≤512 KB with `static_assert`; hot
  path in DTCM; read-only content memory-mapped from flash; `Event` = 8-byte
  POD; ≤40k RAM-resident events + unlimited read-only in flash. Any plan
  that assumes device capacity beyond this is a fantasy.
- **0500 Scope honesty / vertical-first.** The foundational spine and the
  two WOWs are done VERTICALLY, not by opening everything horizontally. The
  dream list is the horizon; the solo-developer risk #1 is starting
  horizontally and closing nothing — so in-flight work finishes before a new
  band opens.
- **0600 Open / hackable, first-class.** Headless core with a stable
  versioned text protocol (commands→/events←), a uniformly addressable
  parameter space (stable ID/path per parameter), MIDI-learn/automation as
  first-class, state dump/inspect, golden/replay as the central dev
  workflow.
- **0700 ABI discipline.** The core contract is typed BINARY commands/events
  on a ring buffer (POD `Command`/`OutEvent`); string-path resolution and
  JSONL live host-side. Param IDs are append-only, never reused; collections
  are fixed-capacity `u16`-indexed arrays; user names live only on the host.
- **0800 Dependency policy.** The core stays dependency-free. Host-layer
  deps are allowed only if lightweight/self-contained AND evaluated with the
  owner first. Any move implying a dependency is flagged, never assumed.
- **0910 Captured direction — `melodd` audio companion — ◑ partial, first
  slice SHIPPED and wired into the GUI.** `components/platform/engines/melodd/`
  ships `melodd::Synth` (TinySoundFont-backed GM realizer, commits
  `fb1e2e8`/`3ea901b` "feat(melodd): first slice — built-in GM synth
  realizer + standalone binary") plus a reference standalone binary
  (`apps/tools/melodd/main.cpp`). `apps/gui-sonotron`'s `gui_sonotron_audio`
  library wires the same engine in-process (Phase-6 Theme 2, commit
  `4f97af5` "feat(gui-sonotron): make the standalone GUI audible", recorded
  as "✅ SHIPPED" in `docs/phase6-plan.md`), making the GUI audible on its
  own — audio never crosses the arrangrr POD interface (`0110` intact,
  `melodd` never links `arrangrr`/`hostrt`). A shared engine seam now exists
  at `components/core/audio_engine/include/audio_engine/i_sound_engine.hpp`
  (the host/core-regime split, commits `65aace9`/`0ac2aee`). Still open: the
  broader multi-method "audio-engines family" (physical-models/analog
  engines behind the same contract) remains a PROPOSAL only
  (`docs/proposals/audio-engines-family-layout.md`, "Status: PROPOSAL —
  read-only move-plan, not executed"), not the "SLOT — no code yet" state
  this entry used to describe.
- **Product identity (what the tree is building toward):** `0110` MIDI-only,
  never audio (drives external gear, syncs by clock); `0120` live + studio
  use, 50/50; `0130` the signature is the COMBINATION — chord intelligence +
  unified write/generate/capture timeline + perfect MIDI glue + open/
  hackable — none dominant.

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
  - `1270` Realtime hardening — sustained-play crackle — ✅ resolved
    (downstream). Investigated: the core/output path is clean — 0 scheduler
    drops over a 300-bar sustained run. The crackle was a DOWNSTREAM
    integrated-audio (PipeWire) buffer underrun, NOT a core defect — fixed
    by sizing FluidSynth's period (`apps/demo/lib/launch.sh`,
    `audio.period-size=2048`, rate-matched to PipeWire). No longer gates
    the GUI freeze line (`11700`).
- **1300 Test & protocol harness**
  - `1310` Headless golden runner (virtual clock, total order) — ✅
  - `1320` CLI shell, 3-layer protocol (Surface/Model/Wire, thin client) — ✅
  - `1330` Core binary ABI (Command/OutEvent POD ring) — ✅
  - `1340` Three-metric coverage (unit ≥80 / functional / regression) — ✅
- **1400 Timeline × Track primitive**
  - `1410` Timeline of bounded deterministic events — ✅
  - `1420` Track = role+destination+length; mute/solo; polymeter — ✅

### 2000 — Harmony core: key/scale, chord modes, detector, chord-sequencer (1st WOW) — ✅ done

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
  - `2340` Chord-follow source selector + live-priority arbitration — ✅
    Selector (detect / sequencer / manual / auto) + owner-decided default
    `ChordFollow::kLivePriority` (ABI-additive value 4, engine constructor
    default; host `chord follow live`). LIVE-PRIORITY: live input overrides
    the sequencer while a chord is HELD — the band follows it and the
    sequencer comps its rhythm on the live root+quality without publishing
    its own chord; on RELEASE the sequencer's next step resumes committing.
    With no sequencer running a live chord LATCHES (chord memory) —
    momentary with a sequencer, latching without. `kAuto` kept as the
    explicit legacy last-writer mode. The held-vs-released decision is made
    at the `fire_chord_seq` call site (dynamic engine state), not in a
    static gate.
- **2400 Chord sequencer (1st WOW)**
  - `2410` ChordSequence functional storage (degree + overrides, free durations) — ✅
  - `2420` Record / loop / transpose / re-harmonize — ✅
  - `2430` Dual output (live harmonizer + editable ChordSequence) — ✅
- **2500 Followed-context ownership — ✅ landed**
  - `2510` Immediate-commit + SHIFT-quantize chord entry — ✅ Shipped model: a
    lowercase note-letter commits the followed chord IMMEDIATELY; an
    uppercase/SHIFT letter stages it to the next bar boundary.
    Transport-start seeds the home-key tonic so the band starts in the home
    key.
  - `2520` original / current / next key readout (panel top, clip-safe) — ✅ HOST-ONLY
  - `2530` Single-owner FollowedContext consolidation — ✅ done (merged). One
    owner of the followed chord + pending; folds the follow-gate and the
    reset-vs-persist policy inside; killed three bugs by construction
    (transport-start clobber, style-load reset, self-drift). No ABI, no
    dep. Merged to `main` (`96dbb72` "harmony: single-owner followed chord
    + single-finger steer, suite green" / merge `6667d6c`). Source:
    `docs/architecture.md`.
  - `2540` Delete dead `set_context` seam — ○ SHIPPABLE
  - `2590` *(reserved: shared-voicing split — separate "who plays the pad"
    from "who steers"; specified, not yet scheduled)* — ○ SHIPPABLE

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

### 5000 — MIDI-FX / Transform chain — ◑ partial (`5100` core + `5210`/`5220` shipped, rest behind the GUI freeze line, `11700`)

*A composable bounded chain (fixed max inserts) of MIDI transforms per
track/zone — the open/hackable north-star (`0600`) made concrete. Arp/groove/
scale-lock become INSTANCES of the chain, not disconnected modules.*
- **5100 Insert-chain framework** (bounded, POD) — ✅ core shipped (Phase-5
  Item #10, commit `95f542b`, 2026-07-14): `kMaxInserts=8`, UI exposes 4.
  Addressing re-decided by the owner from per-track to per-ROLE
  (`kRoleCount=10`, the same ordinal space as `Arranger::m_routes` — the
  graft point is `Arranger::on_tick`, which is per-role, not
  per-Timeline-Track); the `kFx…` ABI verbs (`kFxSet/kFxParam/kFxEnable/
  kFxClear`, Param 53–56) and the reserved block are now written to that
  decision. v1 ships four stateless per-note stream-transform inserts
  (scale-lock, velocity-proc, echo, note-repeat,
  `components/core/arrangrr/include/arrangrr/fx/insert_chain.hpp`) grafted
  ahead of `groove::apply` with a per-fan-out-note grid recompute (existing
  goldens byte-identical: an empty chain is passthrough). The chain is live
  config only — **not** persisted in the `Performance` v1 format yet.
- **5200 Refactor existing modules into chain instances**
  - `5210` groove as a chain instance — ✅ DONE (`8428a4d` "feat(arrangrr):
    groove-as-insert and arp-as-insert (Phase 6 Theme 4, 5210/5220)").
    `InsertType::kGroove` reproduces `groove::apply`'s math inside the
    chain; pinned effectively last by `InsertChain`'s constructor so every
    existing golden stays byte-identical while groove becomes a genuinely
    reorderable slot.
  - `5220` arp as a track MIDI-FX instance — ✅ DONE (`8428a4d`, same
    commit) *(= 7130)*. `InsertType::kArp` carries a 4-byte config in the
    union; its session state lives in a new per-role
    `ArpeggiatorEngine m_role_arp[kRoleCount]`, never captured by
    `Performance` (mirrors the global live arp).
  - `5230` scale-lock / scale-filter as a chain instance — ○ SHIPPABLE
- **5300 New inserts (SHIPPABLE)**
  - `5310` echo / MIDI-delay — ○
  - `5320` note-repeat / ratchet insert — ○
  - `5330` velocity-proc / probability / randomize — ○
  - `5340` harmonize / chord-memory-expand — ○

### 6000 — Looper (the missing "capture" gesture) — ◑ partial

*Completes the write/generate/capture triad of the unified timeline
(`0130`). Budget pre-sized by `0400` (8×3072 ev = 192 KB). Full commit/test
evidence for the four ✅ leaves:
`docs/roadmaps/completed-nodes-archive.md#6000--looper-full-evidence-for-the-four--leaves`.*
- `6100` Record / overdub / replace / erase / undo — ✅ done: `LoopBuffer`,
  commit `eaf9d50`.
- `6200` Quantize-after (non-destructive) — ✅ done: same commit `eaf9d50`.
- `6300` Retroactive capture ("grab last N bars") — ✅ done:
  `RetroCaptureRing`, commit `95a3a7d`.
- `6400` Loop length (fixed/auto/quantized) — ✅ done: `kLoopLength` = ABI
  Param 64, commit `eaf9d50`.
- `6500` Sync + follow-chord capture (re-harmonize on chord change) — ◑
  partial: the RE-HARMONIZE half is proven end-to-end
  (`test_loop_reharmonize.cpp` — record → stop → [context changes] → launch
  through the real ABI, all three `LoopNoteSource` branches) — chord-tone-
  relative storage + resolve-at-playback makes re-harmonize a byproduct of
  `6100`'s own shape (`loop_event.hpp:9-19`), not new machinery. The SYNC
  half stays explicitly unbuilt: `loop_buffer.hpp:146-152`'s own comment
  calls today's overdub alignment "a SLICE-1 simplification: true
  bar-aligned overdub-while-playing synchronization is 6300/6500 territory
  (deferred)." Not ✅.

### 7000 — Expression — ◑ partial

- **7100 Arpeggiator engine**
  - `7110` Live-keyboard arp (rate/dir/octaves/gate/latch/seed) — ✅
  - `7120` Arp as a style part (replace hand-written kArp patterns) — ○ SHIPPABLE
  - `7130` Arp as a track MIDI-FX — ✅ DONE (`8428a4d`, Phase-6 Theme 4) *(= 5220)*
- **7200 Phrase/Pad engine** (banks of 4; one-shot/loop/hold/toggle) — ◑
  partial (Phase-5 Item #9, commit `e980665`, 2026-07-14): `PadEngine`
  (`components/arrangrr/include/arrangrr/pad/pad_bank.hpp`) ships 8 banks ×
  4 = 32 flat POD pad slots, all four trigger modes
  (OneShot/Loop/Hold/Toggle), and six pad types (Phrase/Chord/SceneColumn/
  Variation/Fill/Performance), each a wrapper fanning out to an EXISTING
  verb — wrapper-only by design, no new emission engine. Still open:
  `Drum`/`CC`/`NoteRepeat` pad types (deferred — need new note/CC emission)
  and `PadPitch::kTransposeWithChord` (captured in the struct, not yet
  wired). Tests: `test_pad.cpp`, `test_pad_bank.cpp`, `test_pad_perf.cpp`.
- **7300 Groove/Humanize** — ✅ *(shared with 3250)*
- **7400 Metronome/click, tap-tempo, tempo-nudge** — ○ SHIPPABLE

### 8000 — Structure & recall + persistence — ◑ partial (`8100`/`8200`/`8500` done, `8300`/`8400`/`8600` remain, behind the GUI freeze line, `11700`)

*Full commit/test evidence for the three ✅ leaves:
`docs/roadmaps/completed-nodes-archive.md#8000--structure--recall-full-evidence-for-810082008500`.*
- `8100` Scenes / song mode (snapshot + chain + tempo/time-sig) — ✅ done
  (Phase 7, commit `67fdd13`): `SceneChain`
  (`components/core/arrangrr/include/arrangrr/scene/scene_chain.hpp`),
  ABI `Param::kSceneAdd`/`kScenePlay`/`kSceneStop`/`kSceneClear` = 65–68.
  Tests: `test_scene.cpp`, `test_scene_hardening.cpp`,
  `test_scene_meter_gate_regression.cpp` (844 lines combined).
- `8200` Performance/Registration (recall live state) — ✅ done (Phase-5
  Item #9, commit `e980665`): `Performance` POD (96 B). ABI
  `Param::kPerformanceStore`/`kPerformanceRecall`. Tests:
  `test_performance.cpp`, `test_performance_validate.cpp`,
  `test_performance_style_id_regression.cpp` (91/91 host green).
- `8300` SetList — ○ SHIPPABLE
- `8400` Project/Preset manager (slots, defaults) — ○ SHIPPABLE
- `8500` Versioned binary storage + CRC (save/load round-trip) — ✅ done
  (Phase-5 Item #9, commit `e980665`):
  `serialize_performance`/`deserialize_performance` write/read an explicit
  field-by-field little-endian wire format with `magic`/`format_version`/
  CRC32 trailer. Round-trip proven by `test_performance_wire.cpp`. Scoped
  to the `Performance` object only — the broader §21 multi-table Project
  binary format remains unbuilt (`8400`'s dependency).
- `8600` Diagnostics/MIDI monitor + fault-injection suite — ○ HOST-ONLY

### 9000 — Style content & tooling — ◑ partial / decided

**Anti-sameness scope (owner-locked, 2026-07-18 pass).** `9100` is ✅ done
ONLY as *inter-style feel / anti-clone differentiation* — the 16 built-in
styles genuinely differ from each other (tempo, groove, swing). It is
explicitly NOT a claim that a single style never repeats within itself:
**short-term repetition is WANTED** — grooves and riffs repeating is how a
band sounds like a band. The real open goal is that a style must not loop
byte-identically to infinity — it must **evolve over time** (fills at
phrase boundaries, variation, motif development, humanize drift), never
"never repeat." `9210` (motif + transforms, ✅ shipped) is the first slice
of that evolution; the rest of that property — the never-loops-forever
guarantee — is OPEN generative work living behind the line at `9200`
(generative style, `9220` specifically) and `10000` (Director). See the
corrected through-line in "Recommended sequence" below, which previously
conflated the two.

- **9100 Per-style feel (inter-style differentiation) — ✅ done (model +
  tempo + feel-genre swing; only 9130 refinement left).** Full commit
  evidence for `9110`/`9120`/swing:
  `docs/roadmaps/completed-nodes-archive.md#9100--per-style-feel-full-evidence-for-91109120feel-genre-swing`.
  - `9110` Style owns its default GrooveParams — ✅ done
  - `9120` Style owns its tempo — ✅ done
  - Feel-genre swing (swing/shuffle/blues) — ✅ done (three regression
    goldens `feel_swing/shuffle/blues` lock the swung ticks; spec in
    `docs/style-corpus-and-generation.md` §5)
  - `9130` Triplet / shuffle grid (feel expressible in note placement) — ○
    SHIPPABLE, deferred *(only `blues` needs a true 3-equal-subdivision
    grid to refine its 12/8 beyond the shipped 2-note shuffle — a blues-only
    quality upgrade, not a blocker)*
  *ranked the single biggest lever against inter-style sameness (corpus
  measurement, `docs/style-corpus-and-generation.md`); feel values in
  `docs/style-corpus-and-generation.md`*
- **9200 Generative style (intra-style evolution) — ◑ partial (`9210`
  shipped, `9220` remains — `9220` is where the never-loops-forever
  property actually gets built)**
  - `9210` Motif + transforms (diatonic transpose/retrograde/displacement,
    seeded) — ✅ done. Engine:
    `components/core/arrangrr/include/arrangrr/arranger/motif.hpp`, commit
    `6fe5869`. Owner directive ("done only when wired into all 16
    built-in styles") satisfied: commits `f850ea2`/`417a674`/`f46d758`,
    golden regression `690b35c` (26/26 goldens green). Every one of the 16
    `arranger/styles/*.hpp` headers carries at least one `.motif=&...Spec`
    attachment; `basic` is the deliberate exception (no genre convention to
    generate toward). Full evidence:
    `docs/roadmaps/completed-nodes-archive.md#9210--motif--transforms-full-evidence`.
  - `9220` Offline-trained Markov/grammar on scale degrees, baked constexpr — ○ runtime SHIPPABLE / training HOST-ONLY
- **9300 MIDI stylizer — ◑ partial (host-only)**
  - `9310` Accompany (keep the melody, play the genre band under detected
    chords) — ✅ done HOST-ONLY. End-to-end via `components/orchestrator`'s
    `AccompanyPipeline`. Proven by `tests/golden/accompany_basic.golden` +
    `tests/golden/accompany_melody_detect.golden`. Full evidence:
    `docs/roadmaps/completed-nodes-archive.md#9310--accompany-stylizer-full-evidence`.
  - `9320` Restyle (transform the input's own parts into the genre idiom) — ○ HOST-ONLY *(depends on 9100)*
- **9400 Style data format + generator — ◑ partial**
  - `9410` Style inspector + serialize/deserialize (offset/index-based) — ○ HOST-ONLY
  - `9420` Style compiler (data → .cpp constexpr for the device path) — ◑
    partial, first slice IN-FLIGHT:
    `apps/tools/arrstyle-converter/src/style_lower.{hpp,cpp}` + a new
    `compile-style` CLI subcommand lower a `StyleModel` to a compilable
    `arrangrr::Style` header. Verified end-to-end against one real corpus
    `.sty` (compiles clean); deliberately never wired into the built-in
    style list or `ci.sh`. Commit `f6611e5`, on sibling worktree branch
    `worktree-agent-a3f3c787996a2023e`, **still not merged onto this
    branch as of this pass** (`git merge-base --is-ancestor f6611e5 HEAD`
    re-verified false, 2026-07-18) — pending cherry-pick. Stays ◑, not ✅.
  - `9430` CASM→NTT importer body (arrstyle-converter; 1010-style corpus) — ◑
    partial, real decode (not "inspect-only" as stale docs elsewhere still
    claim): `apps/tools/arrstyle-converter/src/casm.cpp` is a
    bounds-checked CASM/CSEG/Ctab/Ctb2 decoder and `sff_import.cpp`'s
    `import_sff()` actually calls it, builds real per-role `PhraseLane`s —
    SFF1 fully decoded, SFF2 partially (`Ctb2`'s richer per-chord-group
    sub-structure not fully unpacked). Proven by
    `test_sff_import.cpp::test_real_corpus_samples`. Known stale spot,
    code-level not just docs: `cmd_inspect` still routes SFF files through
    `inspect_sff()`, which prints the old "unsupported subset —
    inspect-only" text even though `import-sff` on the same file now
    decodes it fully — a one-line fix flagged for whoever next touches this
    tool, not blocking. *(Correction still owed elsewhere:
    `docs/style-corpus-and-generation.md` and
    `apps/tools/arrstyle-converter/DESIGN.md` still say CASM decode is not
    implemented — outside this reconciliation's file scope; flagged, not
    fixed here.)*

### 10000 — Generative Director — ○ planned, CAPSTONE (last; also behind `11700`)

*A top layer that PILOTS (never bypasses) the parameters of the modules
below, morphing them gradually bar-by-bar from a current expressive state
toward a target. Deterministic trajectory (`0100`), no heap (`0200`), cheap
on device (`0400`). This is the OTHER half of the intra-style-evolution
property named in the `9000` band header — the Director is what makes an
arrangement demonstrably different an hour into a set, not just
tempo/swing-different from another style.*
- `10100` DirectorState / DirectorTarget (energy/density/tension/brightness/complexity) — ○ SHIPPABLE
- `10200` Bar-by-bar interpolation engine (seeded, bounded per-bar step) — ○ SHIPPABLE
- `10300` Parameter-delta drive into arranger/style/sequencer/arp inputs — ○ SHIPPABLE
  *hard-gated: needs 3250 ✅, 7110 ✅, 4100 ✅, the voicing/scene tunable
  surface (3140 done; per-part exposure open) and ideally 5000 — see
  sequence*

### 11000 — Host UI / clients — ◑ partial

- `11100` TUI foundation — ✅
- `11200` Piano/monitor MVP — ✅
- `11300` Presentation layer (colors/themes/unicode, richer views) — ○ HOST-ONLY (deferred)
- **11400 Piano source-coloured visualizer — ◑ partial (11410 done, 11420
  deferred)**
  - `11410` GREEN (current bar) + AMBER (next/pending chord) — ✅ done. Full
    evidence:
    `docs/roadmaps/completed-nodes-archive.md#1140011500-full-evidence`.
  - `11420` WHITE (direct play) — ○ HOST-ONLY *(deferred: needs a sounding melody surface)*
- `11500` UDS-JSONL control adapter (one protocol, three consumers) — ✅
  done, and extended (Phase 3, commits `807c140`/`b98dbda`). Deferred, not
  owed: does NOT split `hostrt::Shell` or retire cli-arrangrr's embedded
  live mode (owner-decided 2026-07-13). Full evidence:
  `docs/roadmaps/completed-nodes-archive.md#1140011500-full-evidence`.
- `11600` Host GUI client — **OPEN (parent node; the instrument is never
  "done").** The GUI freeze line's (`11700`) target. Toolkit dependency
  flag: RESOLVED / vendored — Dear ImGui (upstream `ocornut/imgui`, pinned
  v1.92.8) + GLFW3 under `third_party/imgui` + `third_party/glfw` (each
  with an `ARRGRR_VENDOR.md` pin), built and linked by
  `apps/gui-sonotron/` (dependency fork resolved under `0800` and executed
  in code; rationale as-built in `docs/gui-and-ux.md` §1). **Split into
  sub-nodes `11610`–`11650` (owner-locked, 2026-07-18 pass)** — the parent
  stays open indefinitely; each sub-node below carries its own status.

  **DONE criterion for a GUI sub-node (owner-locked, verbatim):** *a
  sub-node is DONE when its claimed capability is wired end-to-end with a
  contract test pinning it AND has no known NO-OP path in the default run
  mode.*

  - `11610` GUI foundation — ✅ done. Mechanical strand COMPLETE (G0 concept
    demolition `38b5826`, G1 workstation layout `c49f8c6`, G2 brain session
    + G3 zone panels `473ab60`, 66/66 host tests green). Core-dependent
    strand: `kChordFollowed` (P0-1, `52008e4`) and `kBeat`/position (P0-2,
    `ba568ca` + `f4c6188`), wired end-to-end and pinned by
    `test_chord_followed_event`/`test_brain_event`/`test_app_state`/
    `golden_chord_followed`. Repeat Zone slices 1–3 (commits `3398f04`,
    `f531d8f`), pinned by `test_app_state.cpp`/
    `test_in_process_brain_session.cpp`/`test_grid_model.cpp`/
    `test_scenes_json.cpp`. Song-mode Phase 1 (commit `465bb48` "adopt core
    SceneChain as the song engine"), pinned by
    `test_song_mode_scenechain_contract.cpp`. In-process engine hosting by
    default (Phase 2a/2b, commits `bf2c4b2`/`8c9e54d`; D38 retired for the
    single `gui_sonotron_engine` library only, core ABI unchanged). Every
    capability here meets the DONE criterion above (contract-test-pinned,
    no known no-op path). Full evidence:
    `docs/roadmaps/completed-nodes-archive.md#11610-formerly-the-pre-split-11600--full-mechanicalgui-foundation-history`.
  - `11620` In-process `program`-verb no-op fix — ✅ done (commit
    `ec5b90d`, "fix(gui-sonotron): honor 'program' verb in in-process
    backend"). `command_line_to_command`
    (`apps/gui-sonotron/src/in_process_brain_session.cpp`) — the
    translator the DEFAULT in-process GUI backend uses — now carries a
    `t[0] == "program"` arm mirroring `hostrt`'s own `cmd_program` grammar
    (voice by GM name or bare number, `out0`/numeric port, optional
    `:channel`) and dispatches `Op::kSet`/`Param::kProgram`, so BOTH the F1
    Voices picker and the F2 Kits picker (see `11630`) now reach the engine
    when the GUI runs in-process (the default), not only over
    `--control`/UDS as before. Meets the `11610` DONE criterion
    end-to-end, not merely at the translator: pinned by
    `test_program_voice_by_name_reaches_program_command` and
    `test_program_kit_by_number_on_channel_ten_reaches_program_command`
    (`apps/gui-sonotron/tests/test_in_process_brain_session.cpp`), each of
    which polls for the resulting real MIDI program-change `BrainEvent`
    (`kMidiOut`/`"program"`) — i.e. proof through `Engine`'s real
    voice-change dispatch, not just a successfully-parsed `Command`. The
    unresolvable-voice and unresolvable-port error paths are pinned too
    (`test_program_unknown_voice_name_surfaces_clean_error`,
    `test_program_unknown_port_surfaces_clean_error`). No known NO-OP path
    remains for `program` in the default run mode.
  - `11630` Browser redesign — remainder — ○ HOST-ONLY. F1 (commit
    `bce71ab`: category selector + Sections/Variations click→apply +
    Voices/GM-program picker) and F2 (commit `6a8799e`: real GM
    percussion-Kit category on the `program` verb) are ✅ shipped and
    counted under `11610`'s Browser-redesign contribution. What remains is
    proposal Phase 3 of `docs/proposals/browser-redesign-taxonomy.md`:
    Songs/scene-chains and Performances tabs (previously blocked on
    song-mode Phase 2, `11650`, per that proposal's own §4 — "earliest sane
    start... is after song-mode Phase 2"; `11650` shipped `76a8774`, so
    that block is LIFTED — Phase 3 itself is not scheduled, only no longer
    gated), Chord-progressions/Loops/Pad-FX-Groove-preset tabs
    (each needs its own host-side naming registry, none scheduled), and
    Controller-maps/Routing-profiles (recommended to route to a future
    Settings surface instead, per that proposal's fork 4). Not started.
  - `11640` UI-animation — ○ not started. Scope: beat/bar-synchronized
    motion for Repeat Zone / Sequence Edit / Transport (Steps 1–3 of
    `docs/proposals/ui-animation-roadmap.md`); Step 4 (real-data animation)
    is gated on new core/audio/GPU data not yet built. *Pointer content
    (carried over from the former unnumbered placeholder in DESIGN.md
    §22): the plan is phased Step 1 Foundation (`motion.hpp/.cpp`,
    beat/bar-driven `MotionFrame`) → Step 2 Hero (Repeat Zone lifecycle,
    Sequence Edit playhead) → Step 3 Polish (Transport/Intention/Browser/
    Parts) → Step 4 Real data (gated). It acts on the same P1 finding as
    the earlier `docs/proposals/ui-motion-extreme-2026-07.md` audit (no
    motion is beat-synchronised) — see that proposal's own reconciliation
    note for how the two relate.*
  - `11650` Song-mode Phase 2 — ✅ done (commit `76a8774`,
    "feat(gui-sonotron): Song-mode Phase 2 — per-scene style/groove/
    key/tempo overrides", PR #4, merged to `main`; HEAD `c863d5c` sits
    directly on top of it). Scope (per
    `docs/proposals/song-mode-scenechain-adoption.md` §"Phasing", Phase
    2): `GridModel` grows a per-scene style/groove/key/tempo override
    record (`apps/gui-sonotron/src/grid_model.hpp`/`.cpp`:
    `scene_style_id`/`set_scene_style_id`, `scene_groove`/
    `set_scene_groove`, `scene_key_root`/`scene_key_mode`/`set_scene_key`,
    `scene_tempo_x100`/`set_scene_tempo_x100`, each independently gated by
    its own sentinel/override-flag) + a per-scene editor UI
    (`render_scene_editor_popup`, `apps/gui-sonotron/src/grid_panel.cpp`,
    right-click on the scene-header cell); the `song build` wire line
    carries four trailing per-scene override tokens
    (`apps/gui-sonotron/src/in_process_brain_session.cpp`), folded into
    each scene's baked `Performance` before `apply_song_build` stores
    it — the Phase-1 capture-and-override mechanism (already shipped,
    `11610`) picks the new fields up automatically, no new engine
    mechanism, zero core-ABI change. The tempo override also carries a
    live-readback path (`OutEvent::kParamState`/`kTransportTempo`
    confirmation echo, `components/core/arrangrr/src/engine.cpp`,
    `apps/gui-sonotron/src/brain_event_from_outevent.cpp`), so a recalled
    tempo round-trips to the GUI the same way style/groove/key already
    did. Pinned by `apps/gui-sonotron/tests/test_grid_model.cpp` and
    `test_song_mode_scene_performance_overrides_contract.cpp` (both
    re-verified green in this pass: `ctest --test-dir build/host -R
    "test_grid_model|test_song_mode_scene_performance_overrides_contract"`
    — 2/2 passed). Meets the `11610` DONE criterion end-to-end:
    contract-test-pinned, no known NO-OP path. No longer blocks `11630`'s
    Songs/Performances Browser tabs (see `11630` below — the block is
    lifted; that sub-node's own scheduling is unchanged).

  *Dual-target note: the whole `11600` band is HOST-ONLY by construction —
  the GUI is a desktop client. The STM32 target (`12000`) keeps its OWN
  separate physical UI (`12400`); `11600` is never the device front-end —
  do not conflate the two (`0300` dual-target discipline).*

- **11700 GUI freeze line — pivot from core-feature work to the host GUI**
  (owner-decided). STATUS: ✅ CROSSED (2026-07-06). The pre-GUI batch
  `11710` is all ✅ and `11720` (freeze) has executed — the ABI is frozen v1
  and the `5100` shape is reserved. **Update (owner directive,
  2026-07-13): the freeze itself is LIFTED for Phase-5 work forward** —
  resolves `docs/architecture.md` fork F3 (the freeze-lift is ACCEPTED;
  the specific ABI-reshape proposal in that document remains its own
  separate open design review), recorded in `docs/phase5-plan.md`.
  Core-feature work is now BEHIND the line; the next action was `11600`
  (build the host GUI) — now split into `11610`–`11650` above. This node
  still governs sequencing of everything below it.

  *Through-line: **validate feel in the hands, then grow on a living
  instrument.** The product is a MIDI arranger — a live instrument whose
  value is in the hands. The TUI structurally cannot validate FEEL (timing,
  the chord-steer sensation, the piano visualizer's readability): feel
  lives in the hands, not in a text panel. So core-feature work STOPS at a
  small, well-defined batch, and the product PIVOTS to building the host
  GUI (`11600`). Every remaining musical feature (`5000`, `6000`, `8000`,
  `10000`, and the rest of `9000`) is grown AFTER, on an instrument that
  already exists and already sounds — reprioritized by real
  feel-in-the-hands testing, not by this document's ranked order.*

  - `11710` Pre-GUI gating batch — must be ✅ before the line is crossed.
    **ALL FOUR cleared: `2530` ✅, `1270` ✅, `11410` ✅, `9100` ✅ — batch
    complete; the line has been CROSSED (`11720` freeze done).**
  - `11720` At-the-freeze-line actions — ✅ DONE (merged): froze the ABI
    (`0700`, `test_abi_frozen.cpp` pins `sizeof(Command)==20`,
    `sizeof(OutEvent)==16`, `kProtocolVersion==1`); reserved the `5100`
    MIDI-FX shape as ABI-none. **Phase-5 update:** this freeze is LIFTED
    for Phase-5 items going forward. Full text of both actions:
    `docs/roadmaps/completed-nodes-archive.md#11700117101172011730-full-phase-5-program-inventory`.
  - `11730` Behind the line — reprioritized on a living instrument, no
    longer ordered by this document alone. `11600` itself (now
    `11610`–`11650`) is the FIRST thing behind the line. Then, in the
    owner-ordered Phase-5 program (`1→7→8→2→9→6→4→10`, full inventory
    archived): Restyle (`9320`) · Motif (`9210`, ✅ shipped) · Corpus
    import (`9400`/`9430`, ◑ in-flight) · Clip/launch primitive (delivered
    under `11610`) · Pad/Scene (`7200`/`8100`–`8200`, mostly ✅) ·
    `melodd` (`0910`, ◑ shipped-first-slice) · Fuzzing harness (✅ shipped,
    `components/midisrc/fuzz/`, commit `c2251f2`) · MIDI-FX chain (`5000`,
    core ✅). Deferred out of this program: STM32 bring-up (`12100`),
    external clock-in (`4500`). Full inventory:
    `docs/roadmaps/completed-nodes-archive.md#11700117101172011730-full-phase-5-program-inventory`.

### 12000 — Device / STM32 / HW + outward interop — ○ planned / ◑

- `12100` STM32H743 HAL (USB-MIDI + UART DIN + timer + storage) — ○ device
- `12200` Real budget validation (≤512 KB envelope, static_assert) — ◑ *(asserts exist; no HW run)*
- `12300` Watchdog / crash-recovery (all-notes-off at boot) — ○ device
- `12400` Physical UI (pads/encoders/display) if/when in scope — ○ device
- `12500` External interop — **parent node, split into `12510`–`12550`
  (owner-locked, 2026-07-18 pass).**
  - `12510` MIDI Implementation Chart — ◑ partial (the underlying mapping
    is real, the formal chart document is not yet written). 72-opcode →
    MIDI wire mapping spec: PC/Bank↔style/voice/kit, NRPN↔param,
    SysEx↔multi-operand, F8/FA/FB/FC↔transport. HOST-ONLY doc, zero code
    dependency. Row 1 (PC/Bank↔style/voice/kit) is already real via the
    `program` verb (`3240`; `components/platform/hostrt/shell.cpp:673`,
    `shell_io_commands.cpp:89`) — the chart's job is to document the
    mapping that already exists and specify the rest, not to build new
    wire mechanism. `12510` PRECEDES and constrains `12520`–`12550`: no
    row below should be built before the chart states its wire shape.
  - `12520` DeviceProfile + ExternalSound — ○ SHIPPABLE
  - `12530` Ordered Bank/PC/CC init — ○ SHIPPABLE
  - `12540` RPN/NRPN/14-bit/aftertouch — ○ SHIPPABLE
  - `12550` MIDI-Learn/ControllerMap — ○ SHIPPABLE
- `12600` Laptop tools: SMF import/export, style/device editor — ○ HOST-ONLY

### 13000 — Desktop feature expansion (proposed) — ○ not started, PROPOSED only (not yet DECIDED, prioritized, or scheduled)

*Twelve desktop-feature evolutions proposed 2026-07-19, translated in full
and filed at `docs/proposals/desktop-feature-expansion-2026-07.md`. Filed
as a new top-level band per this file's own numbering rule (a new band
takes "the next free thousand") — `13000` is the first unused thousand-band
as of this pass (verified against every existing 5-digit node number in
this file). The owner asked only that the proposal be placed at the
correct spot in the tree; nothing here is committed to being built. Every
leaf below therefore carries **○ not started** and no status stronger than
PROPOSED — prioritization and sequencing judgment is explicitly deferred to
whoever schedules this band later (`verdi-roadmap-strategist`'s domain, not
this filing pass).*

*The source document's own closing section groups the twelve features into
four architectural stabilization blocks plus a set of cross-cutting
principles; both are preserved here as the band's organizing hundreds
(`13100`–`13400`) rather than flattened, because the source explicitly
frames the twelve as one coherent system rather than independent panels,
with load-bearing ordering between blocks: Block B (the Transaction Engine)
is meant to land before Block A's live edits touch too many subsystems at
once, and Block C's three resource nodes are "designed together, even if
implemented incrementally."*

- **13100 Block A — Immediate value on the current code.** Four features
  that exploit primitives already shipped and produce host-visible value
  without waiting on the other blocks.
  - `13110` Looper in the Repeat Zone — ○ not started, HOST-ONLY (GUI/host
    integration work; the core primitives already ship). Builds DIRECTLY on
    shipped infrastructure, not greenfield: `LoopBuffer` record/overdub/
    replace/erase/undo/quantize (`6100`/`6200`/`6400`, ✅ done, part of the
    `6000` Looper band which stays ◑ partial pending `6500`'s sync half),
    `RetroCaptureRing` retroactive capture (`6300`, ✅ done), and
    `ContentKind::kLoopBuffer`/`Engine::apply_clip_content()`'s existing
    loop branch plus the Repeat Zone's `kArmed`/`kPlaying`/`kQueuedStop`
    core launch-state model. Cross-reference, not duplication: see `6000`
    (core Looper) and `11610` (Repeat Zone GUI foundation, ✅ done,
    "Repeat Zone slices 1–3") — this node is the remaining HOST/GUI
    integration work (real hardware MIDI capture, virtual-surface wiring,
    recording-lifecycle UI, cell↔slot persistence) that those two nodes do
    not yet cover. See also `docs/proposals/repeat-zone-real-contract.md`
    and `docs/proposals/repeat-count-phase2-abi.md` for adjacent as-built/
    ABI analysis of the same Repeat Zone surface.
  - `13120` Complete, live Sequence Edit — ○ not started, HOST-ONLY. Extends
    the already-shipped host-side `SeqEditModel`/piano-roll/step-view/
    `StepPatternStore` surface to more content types (LoopBuffer, imported
    clips, Style Sections, ChordSequence) and a current/draft/pending
    editing model. Not currently itemized as its own roadmap node; see
    `docs/proposals/seqedit-piano-roll-phase2-design.md` and
    `docs/proposals/seqedit-column-view-and-zoom.md` for the existing
    design work this node extends.
  - `13130` Performance Transformation Surface — ○ not started, HOST-ONLY.
    Explicitly proposed to ORCHESTRATE, not duplicate, already-shipped
    `InsertChain` (`5100`, ✅ core shipped), groove/swing/humanize/accent/
    quantize (`3250`/`7300`, ✅ done), and quantized launch/change
    machinery.
  - `13140` Visualized Musical Causality — ○ not started, HOST-ONLY.
    Overlaps in spirit with `11640` (UI-animation, ○ not started): both are
    about beat/bar-synchronized, causally-meaningful GUI motion. Distinct
    focus: `11640` is the motion/animation MECHANISM
    (`docs/proposals/ui-animation-roadmap.md`, its own Steps 1–4); `13140`
    is the causal DATA MODEL that motion would express (current/pending/
    predicted/candidate/historical state, a Causal Event Model layered on
    `BrainEvent`/`AppState`) — `13140` would consume `11640`'s motion
    primitives once both exist, not replace them. `11640`'s own entry
    already cross-references `docs/proposals/ui-motion-extreme-2026-07.md`;
    the same finding that motion is not yet beat-synchronized bears on
    `13140` too.
- **13200 Block B — Coherence of live operations.**
  - `13210` Live Musical Transaction Engine — ○ not started, HOST-ONLY,
    NEEDS-DECISION (the bundle/ABI shape for multi-subsystem atomic commits
    is unspecified). Proposed to coordinate, then eventually reduce
    duplication among, already-shipped pending/boundary primitives:
    `BoundaryLatch`, `ClipMatrix::arm()`, `due_bar_index`, SceneChain
    transitions (`8100`, ✅ done), Performance recall (`8200`, ✅ done), and
    Style changes. Per the source document's own dependency framing, this
    node is meant to land before `13120`/`13130`/`13140` (Block A) start
    producing live edits that touch more than one subsystem at once — a
    sequencing note carried over from the source, not a hard gate decided
    by this filing pass.
- **13300 Block C — Resource model.** The source document is explicit that
  these three must be designed together even if implemented incrementally.
  - `13310` Musical Resource Graph — ○ not started, mostly HOST-ONLY (the
    core would only ever see bounded handles/compiled descriptors derived
    from it, never the graph itself). Cross-reference, not duplication:
    overlaps the Browser-taxonomy remainder at `11630` (Songs/Performances/
    Chord-progressions/Loops/Pad-FX tabs, per-type naming registries) —
    `11630`'s scope is the BROWSER PANEL surfacing a taxonomy; `13310`
    would be the underlying shared RESOURCE MODEL (Resource ID, descriptor,
    relationship edges, capability queries) that `11630`'s per-type naming
    registries could sit on top of instead of each growing its own ad hoc
    list. This does not expand `11630`'s already-written scope — it is a
    separate, unscheduled node that a future `11630` implementation could
    build on to avoid duplicating taxonomies.
  - `13320` Universal Resource Runtime — ○ not started, split HOST-ONLY
    (registry/adapters/plugins) + SHIPPABLE core-side compiled layer
    (bounded handles, no realtime allocation — explicitly must NOT become
    dynamic polymorphism in the core). Proposed Phase 1 scope adapts the
    existing four `ContentKind` values (`kStyleSection`/`kChordSequence`/
    `kStepTrack`/`kLoopBuffer`) and `Engine::apply_clip_content()`'s
    existing switch; does not propose replacing that switch.
  - `13330` Semantic Resource Browser — ○ not started, HOST-ONLY. Overlaps
    directly with `11630`'s remaining scope (Songs/Performances/Chord-
    progressions/Loops/Pad-FX tabs, per-type naming registries — see
    `docs/proposals/browser-redesign-taxonomy.md`). `11630` already ships
    (as of `11610`'s F1/F2) the category-selector, click-to-apply, and
    drag-and-drop mechanics this node's Phase 1 ("unified browser") would
    build on; `13330` proposes generalizing `BrowserModel` onto `13310`'s
    resource model plus adding compatibility ranking and similarity
    search, which is not part of `11630`'s existing scope text. Cross-
    reference only — `11630`'s existing scope description is unchanged by
    this filing.
- **13400 Block D — Intelligent composition.**
  - `13410` Lead Sheet and intelligent Chord Map — ○ not started,
    HOST-ONLY. Proposed to expose the already-shipped `ChordSequence`/
    chord-follow/live-priority/detector machinery (`2300`–`2500`, ✅ done)
    through a new visual/editable model; ChordPro import already exists in
    the converter tool. See also
    `docs/proposals/gui-live-harmony-musical-design.md` and
    `docs/proposals/per-style-default-progressions.md` for adjacent
    ChordSequence/default-progression design this would need to align
    with.
  - `13420` Seeded Variation Lab — ○ not started, HOST-ONLY. Proposed to
    elevate the determinism/seed infrastructure already used by groove,
    humanization, probability, and motif generation (`9210`, ✅ done) into
    a first-class, genealogy-tracked product surface.
  - `13430` Smart Form Builder — ○ not started, HOST-ONLY. Builds on
    `SceneChain`/`SceneStep` (`11650`, ✅ done) and `PerformanceStore`,
    proposing structure generation on top of them, not a replacement.
    Genuine overlap found beyond the task's own list: this is the same
    problem space already analyzed in
    `docs/proposals/song-form-autoarrange.md` and
    `docs/proposals/song-form-option-a-wiring-plan.md` (auto-song form,
    energy-curve-driven section choice, archetype templates) — those two
    documents predate the SceneChain adoption (`11650`) and were written
    against the OLD hand-rolled auto-song mechanism; neither is marked
    superseded anywhere in this tree. Whoever schedules `13430` should read
    those two documents first rather than re-deriving the same analysis.
  - `13440` Nonlinear Arrangement Graph — ○ not started, HOST-ONLY (the
    Graph is proposed to live host-side and compile bounded transactions
    into the core, the same boundary discipline as `13210`). Builds on
    `SceneChain`/`SceneStep` (`11650`, ✅ done) the same way `13430` does —
    `SceneTransitionKind` today supports only `kCut`; this node's node/edge
    model is explicitly scoped as sitting ABOVE `SceneChain`, compiling
    paths into it, not replacing it.

*Cross-cutting principles carried over from the source document (transport
never stops for creative ops; live changes must be quantizable; determinism
given identical seed/input; generative proposals must stay inspectable and
editable; the UI must distinguish current/draft/pending/preview; persistent
resources are never identified by runtime index; the realtime core stays
bounded with no dynamic allocation while the desktop host may keep richer
dynamic models; every feature must produce observable events/state;
important behavior must be reproducible via tests/trace; initial simplicity
must not block progressive depth; animations must communicate state/time/
causality, never be decoration) restate invariants `0100`/`0200`/`0600`/
`0700` already declared for this whole tree — they are this band's own
confirmation that none of the twelve features asks for an exception to
`0000`, not new obligations.*

---

### 14000 — BDD testing architecture (Gherkin + BabyBehave) — cross-cutting QA infrastructure for `13000` — ○ not started, DECIDED (accept-with-changes, 2026-07-19)

*Reviewed 2026-07-19 by the orchestrator plus six read-only research passes
(`torquato-qa-lead`, a general-purpose WebFetch pass, `corelli-architecture-
critic`, `guido-process-analyst`, two `Explore` passes) against a proposal
to cover the twelve `13000` features with a Gherkin+BabyBehave BDD layer.
Verdict: **ACCEPT WITH CHANGES.** Filed here per that BDD-architecture
review session, 2026-07-19 — no proposal document exists for this pass (it
was a chat-session decision, not a translated source document like `13000`
itself); every finding below was independently verified against the real
tree by that review, cited by exact path. New top-level band per this
file's own numbering rule (`14000` is the first unused thousand-band, verified
against every 5-digit node in this file as of this pass) — the closest
existing testing-infrastructure band, `1300` (`1310`–`1340`), is the
core/protocol harness and already fully `✅ done`; this is a distinct,
GUI-facing testing SYSTEM for the still-unscheduled `13000` band, not an
extension of `1300`'s closed scope.*

*Dependency and scope calls already settled by the review, not re-opened
here: **`BabyBehave`** (`github.com/crsnplusplus/BabyBehave`) is real, MIT,
header-only C++23 (C++17 fallback), with a real CMake/vcpkg/Conan/Bazel
packaging story — but it is NOT a Gherkin/Cucumber engine (no `.feature`
parsing, scenario auto-discovery, CTest auto-integration, tag filtering, or
"World" concept): it is a fluent Given/With/When/Then C++ chaining DSL with
JUnit-XML/TAP reporters, so a semantic Gherkin-parsing/step-registry layer
is still needed on top of it. Pre-1.0, no tagged releases — pin an exact
commit SHA via `FetchContent`, never `GIT_TAG main` (policy `0800`
dependency-fork evaluation, satisfied by this review). Existing test
infrastructure is stronger than the proposal assumed:
`apps/gui-sonotron/tests/imgui_headless_harness.hpp` already does real ImGui
IO-event injection (`AddMousePosEvent`/`AddMouseButtonEvent`) with real
`ImDrawData` read-back assertions; `apps/gui-sonotron/src/input_trace.hpp`
already provides real `--trace-input`/`--replay-input` record/replay; ImGui
is vendored at v1.92.8 (`third_party/imgui`) with the full IO-event-queue
API; real drag-and-drop payloads already exist
(`kStyleDragPayloadId`/`kVariationDragPayloadId`, `browser_model.hpp`); the
project-wide harness is a custom `CHECK`-macro pattern (`test.hpp`), no
gtest/catch2/doctest. **Feature-ID decision:** reuse the existing `13xxx`
node numbers directly as Gherkin tags (`@feature:13110` etc.) — no separate
Feature Registry file. **Interaction Plan / `.flow.yaml` — rejected** as a
separate artifact (would duplicate Gherkin scenario text plus the C++
step-registry binding; no YAML dependency introduced).*

*Owner-ranked MVP priority (refined 2026-07-19, supersedes the flatter
"precise things + several wows + UI matters" framing this band opened
with): **(1) NO CRASH** — stability/robustness, non-negotiable, above
either wow tier below (`14100.1`); **(2) WOW** — the functional milestone
sequence Browser → Sequence Edit → Looper (`14110`–`14150`, unchanged from
this band's original filing); **(3) SUPERWOW** — fluid, animated,
innovative UI as its own goal, sequenced strictly AFTER (2) (`14400`).*

- **14100 Priority #2 (wow) — Foundational seam fix + the three-milestone
  BDD program, in order.**
  - `14100.1` No-crash / stability gate — ◑ partial (commit `3eb005d`,
    "feat(gui-sonotron): sanitizer preset + clock-injection seam
    (14100.1/14110)"), **PRIORITY #1, non-negotiable, supersedes both wow
    tiers below.** No feature milestone (`14120`/`14130`/`14140`) counts as
    done if it introduces or leaves in place a crash/UB risk. Requires
    ASan/UBSan sanitizer runs on the new BDD test binary and on any GUI
    code each milestone touches. **Infra gap CLOSED:** `CMakePresets.json`
    now carries a `sanitize` configurePreset+buildPreset+testPreset triad
    (inherits `host`, `-fsanitize=address,undefined
    -fno-omit-frame-pointer -g -O1`, own `build/sanitize` dir) —
    previously verified absent (`host`/`host-release`/`coverage`/`tidy`/
    `arm`/`arm-release` only). Verified by a full rebuild under the
    `sanitize` preset + `ctest --test-dir build/sanitize -E
    "live_alsa|live_tracks"` → 163/163 tests passed, zero ASan/UBSan
    diagnostics; the `host` preset was independently reverified at
    163/163 alongside this change. **Still open (why this is `◑`, not
    `✅`):** the gate has not yet been exercised against "the new BDD test
    binary" or against `14120`/`14130`/`14140`'s own GUI code, because
    none of those three milestones has landed yet (each remains `○ not
    started` below) — this node closes fully only once each landed
    milestone has itself been verified clean under the `sanitize` preset.
    *(Discrepancy flagged, not written as fact: an earlier
    framing of this gate cited "zero regression-labeled CTest tests" and an
    aspirational "metric-3 bug registry" — re-checked against the tree and
    found FALSE. `1340` (this same file) already records the three-metric
    unit/functional/regression convention as `✅ done`, and
    `components/core/arrangrr/tests/CMakeLists.txt` carries real,
    passing, `LABELS regression` tests today — e.g. `test_step_locks`,
    `test_performance_style_id_regression`,
    `test_insert_chain_fan_overflow_regression`,
    `test_master_transpose_voicing_regression`,
    `test_clip_matrix_live_meter_change_regression` (10+ such tests,
    `ctest -L regression` finds 11 in the current `build/host`). No
    "bug registry"/"metric-3" term appears anywhere in this repo. This
    gate is filed on the verified sanitizer gap alone; the regression-count
    claim is not repeated here.)* Closing `14110`'s clock-injection seam and
    `14130`'s `StepPatternModel` read-back gap are THIS priority's work, not
    merely test-writing convenience: a hardcoded wall-clock scheduler and a
    silent core-write rejection are both correctness/no-crash-adjacent gaps
    independent of BDD — the fact that fixing them also unblocks trustworthy
    `@ui` scenarios is a side effect, not the reason to do them.
  - `14110` GUI↔backend clock-injection seam — ✅ done (commit `3eb005d`,
    "feat(gui-sonotron): sanitizer preset + clock-injection seam
    (14100.1/14110)"), HOST-ONLY. `InProcessBrainSession::run_engine()`
    (`apps/gui-sonotron/src/in_process_brain_session.cpp`) previously
    hardcoded real `steady_clock`/`sleep_for` with no injectable
    clock/scheduler hook, even though core (`Transport`,
    `TestEngine::advance_ticks`) already had a fully deterministic
    tick-injection model — root cause of the 50+ wall-clock `sleep_for`
    polling loops in the GUI test suite, and of
    `test_in_process_brain_session_bar_advance.cpp`'s own admitted weak
    assertion ("did it advance at all", to dodge CI flakiness). Now closed:
    a `ClockHooks{now_us, wait}` seam
    (`apps/gui-sonotron/src/in_process_brain_session.hpp`), settable via
    `set_clock_hooks_for_test()` before `start()`, production defaults
    reproducing the old real-time behavior byte-for-byte
    (`Impl::now_us_hook`/`wait_hook` in
    `in_process_brain_session.cpp`; `run_engine()` now reads the hooks
    instead of calling `monotonic_us()`/`std::this_thread::sleep_for`
    directly). `test_in_process_brain_session_bar_advance.cpp` was
    rewritten to drive a virtual clock and assert the exact expected bar
    instead of the old weak "did it advance at all" check — runtime
    dropped from ~6s real wall-clock to ~0.02–0.07s. Full `host` preset
    reverified at 163/163 tests passed alongside this change.
  - `14120` Browser (`13330`) BDD milestone — ○ not started, HOST-ONLY,
    gated on `14110` (CLOSED — see `14110` above, commit `3eb005d`). First
    milestone: the most demo-ready "wow" today (real
    search + real drag-and-drop already ship, per `13330`'s own
    cross-reference to `11630`'s shipped F1/F2 category-selector/
    click-to-apply/drag-and-drop). Bundles a REAL fix to
    `click_arm_auto_song()`
    (`apps/gui-sonotron/tests/test_grid_panel_auto_song_stop_restart_ui_automation.cpp:100,163`
    — currently writes the `UiState` field directly instead of clicking the
    real button, its own comment admitting "documented gap: direct field
    write, not a click") — landed WITH this milestone, never filed as
    tolerated legacy debt. Requires a semantic wrapper reading
    `ImGui::GetItemRectMin()`/`GetItemRectMax()` right after the real widget
    call (works even for a `SmallButton` at rest-alpha-0, which the current
    color-vertex-scanning harness cannot locate) — a second, related
    harness gap closed by the same milestone.
  - `14130` Sequence Edit (`13120`) BDD milestone — ○ not started,
    HOST-ONLY, gated on closing `StepPatternModel`'s core read-back gap.
    `StepPatternModel` is explicitly "the GUI's own local echo of core
    state, not a replacement for it" (its own header comment,
    `step_pattern_model.hpp:23`); every edit op mutates the local echo and
    fire-and-forgets a command to core with NO read-back query confirming
    core accepted it — a BDD scenario asserting only against
    `StepPatternModel` would pass even if core silently rejected the edit.
    This is a blocking architectural gap (a real core read-back query must
    be added), not a test-writing task, and must close before `13120` gets
    any DoD-level `@ui` coverage. Second milestone: "precise" must come
    before "wow" is trustworthy. Serves priority #1 (`14100.1`) first: a
    silent core-write rejection is a correctness gap independent of
    testing, not merely a precision-for-its-own-sake nicety.
  - `14140` Looper (`13110`) BDD milestone — ○ not started, HOST-ONLY,
    gated on new GUI gesture work (press-and-hold record) not yet built.
    Third milestone: a real third wow, but there is nothing to demo until
    the gesture exists.
  - `14150` MVP cutline — governing marker (does not itself schedule work,
    the same role `11700` plays for the GUI freeze line). **MVP = through
    `14140` landing the Looper milestone with a trustworthy `@ui`
    scenario, with `14100.1`'s no-crash gate held throughout.** `14200`
    (legacy-debt baseline / CI-gate hardening) is explicitly POST-MVP
    process hardening, filed as a distinct later step, not part of the MVP
    itself. `14400` (SUPERWOW, priority #3) sits strictly after this
    cutline too — it is not part of the functional MVP either.
- **14200 Post-MVP process hardening.**
  - `14210` Legacy-debt baseline + CI-gate hardening — ○ not started,
    HOST-ONLY, gated on `14150` (comes AFTER the MVP cutline, never
    before).
- **14300 Deferred — not yet scheduled ("finché non serve").**
  - `14310` `IMidiHal` fake/test-double for injected MIDI input — ○ not
    started, DEFERRED, **NEEDS-DECISION** (connection class confirmed by
    owner as USB MIDI; the specific device/model to characterize against
    remains an open owner decision, left open for now — do not invent a
    device). No fake exists today (only three real hardware
    backends: ALSA/CoreMIDI/WinMM). Do NOT write the fake from reading the
    code alone: the risk is a plausible-but-wrong fake silently encoding
    incorrect assumptions about real MIDI hardware, inherited invisibly by
    both product code and any `@engine`/`@ui` scenario built on it.
    Required sequencing when this is scheduled: (a) FIRST, a small set of
    `@live_smoke` characterization scenarios against REAL MIDI hardware,
    measuring concrete numbers (timing jitter note-on/off, running-status
    behavior, SysEx chunking/timeout, multi-port event ordering,
    connect/disconnect events); (b) THEN build the `IMidiHal` fake to
    satisfy that measured contract, not to "seem reasonable"; (c) KEEP the
    `@live_smoke` scenarios in the tree permanently as drift detectors,
    never delete them once the fake exists.
- **14400 Priority #3 (SUPERWOW) — fluid, animated, innovative UI as its own
  goal.** ○ not started, sequenced strictly AFTER the three functional-wow
  milestones (`14120`–`14140`) land, not alongside them: there must be real,
  working, boundary-synchronized functionality on screen before animating
  it means anything causally.
  - `14410` UI-animation + causal data model, combined — ○ not started.
    Cross-reference only, no new sub-nodes invented under either parent:
    `11640` (UI-animation MECHANISM — its own entry already finds motion
    "not yet beat-synchronized") together with `13140` (Visualized Musical
    Causality — the CAUSAL DATA MODEL, current/pending/predicted/candidate/
    historical state layered on `BrainEvent`/`AppState`, that motion would
    express; `13140`'s own text already says it "would consume `11640`'s
    motion primitives once both exist"). This band's SUPERWOW tier IS
    `11640` + `13140` together, once whoever schedules that work takes it
    up — their own scheduling and scope are unchanged by this filing. BDD
    scenario coverage for this tier is explicitly OUT OF SCOPE here: to be
    designed once `11640`/`13140` are scheduled, not invented in this pass.

*Test-authoring order vs. product-build order — do not conflate.*
`14120`→`14130`→`14140` is a TEST-AUTHORING order (which of the three
already-partially-built `13000` features gets BDD coverage first). This is
distinct from `13210`'s own existing product-BUILD sequencing note ("this
node is meant to land before `13120`/`13130`/`13140` (Block A) start
producing live edits that touch more than one subsystem at once — a
sequencing note carried over from the source, not a hard gate decided by
this filing pass"): that note is about construction order for NOT-YET-BUILT
features; this band's order is about which of the three already-real
features earns BDD scenarios first. The two do not contradict each other —
they answer different questions.

---

## Recommended sequence for the open work (the through-line)

**Superseded in part by the GUI freeze line (`11700`), owner-decided.** The
arc now reads: **sparse input → rich harmony (done) → a band that follows
correctly and whose styles genuinely differ from each other, hardened
against realtime defects (the pre-GUI batch, `11710`) → FREEZE (`11700`) →
a living, playable GUI (`11600`) → every remaining musical feature grown on
that instrument, reprioritized by real feel-in-the-hands testing, not by
this document's ranking.** *(Corrected 2026-07-18: the former wording here
— "a band that follows correctly and no longer sounds the same twice" —
conflated the shipped inter-style differentiation with the still-OPEN
intra-style evolve-over-time property; see the `9000` band header for the
corrected distinction.)* Steps 1–4 below are the pre-GUI gating batch
verbatim; everything from step 6 onward now sits BEHIND the freeze line and
its mutual order is advisory only — `11700`/`11730` is the binding word on
what's deferred, this list is kept for continuity and for ordering WITHIN
the behind-the-line set.

### NEXT — the pre-GUI gating batch, in order (invariant `0500`)
1. **`2530` — Single-owner FollowedContext consolidation.** ✅ DONE.
2. **`11410` — Piano visualizer GREEN+AMBER.** ✅ DONE.
3. **`1270` — Realtime hardening: sustained-play crackle.** ✅ RESOLVED.
4. **`9110`/`9120` + feel-genre swing — Per-style feel (inter-style).** ✅
   DONE. Only `9130` (a blues-only true-triplet refinement) remains,
   deferred as a post-freeze nicety.

### AT THE FREEZE LINE — `11700`
5. **Freeze `0700` (current ABI) + lock the `5100` shape.** ✅ DONE. **Build
   `11600`, the host GUI.** Tech stack DECIDED & vendored (ImGui + GLFW3,
   see `11600`).

### BEHIND THE LINE — grown on a living instrument (`11730`), reprioritize on real feel-in-the-hands
6. **`9310` — Stylizer: Accompany.** ✅ DONE, HOST-ONLY.
7. **`5100`+`5210`+`5220` — MIDI-FX / Transform chain, first increment.** ✅
   DONE (core + groove/arp-as-insert). Remaining: `5230` scale-lock,
   `5310`–`5340` new inserts.
8. **`9210` (generative style, motif slice) — ✅ DONE (shipped, wired into
   all 16 styles).** `9320` (Restyle) remains open, depends on `9100`
   (done). The rest of the intra-style-evolution arc (`9220`
   Markov/grammar, `10000` Director) remains OPEN — see the `9000` band
   header note; this is genuinely unfinished work, not a stale bullet.
9. **`4300` → `6000` — track record/overdub → Looper.** `6000` core
   (`6100`–`6400`) ✅ shipped; `4300` (track record/overdub) and `6500`'s
   sync half remain open. Pair `4500` (external clock-in).
10. **`8100`–`8400` — Scenes / song / performance / setlist.** `8100`/
    `8200`/`8500` ✅ shipped. `8300`/`8400` remain open.
11. **`10000` — Generative Director.** Capstone, unchanged in position:
    only real once `3250`/`7110`/`4100` (done), `5000` (mostly done) and
    the voicing/scene surface exist, AND it now has a living GUI (`11610`,
    shipped) to be felt through.

### DEFER (specified, not now)
- **`9400` style data format + CASM importer** — no longer deferred in
  priority (Phase-5 program item #8, position 3 of 8); the "smallest first
  step" is IN-FLIGHT (`f6611e5`, pending cherry-pick, re-verified still
  unmerged 2026-07-18).
- **`11300` presentation layer; `4200` remaining step params; `3300`
  arranger refinements** — incremental polish; interleave
  opportunistically, none on the critical arc.

### CUT / hold at the horizon (no schedule)
- MIDI 2.0 / MIDI-CI, full SysEx, SMF (`12600` tail) — remain horizon per
  `0500`; nothing depends on them.
- ML style-transfer — stays OUT (`9300` scope): on-device infeasible,
  host-only would need a dependency flag.

---

## Constraints & flags that gate ordering

- **The GUI freeze line (`11700`) is the binding gate** on this whole
  sequence: nothing in "BEHIND THE LINE" schedules before `11710` is all ✅
  and `11720` has executed.
- **ABI / struct changes:** `9110` is owner-approved. `5100`'s shape is now
  LOCKED at the freeze line (`11720`) — the former NEEDS-DECISION is
  resolved. All other open leaves are additive or internal.
- **No new core dependency** is introduced by any SHIPPABLE item. The
  former open dependency flag — the **GUI toolkit** for `11600`
  (HOST-ONLY, policy `0800`) — is RESOLVED: Dear ImGui + GLFW3 vendored
  under `third_party/` and driven by `apps/gui-sonotron/`.
- **Dual-target / no-heap reality (`0200`/`0300`/`0400`):** every
  SHIPPABLE leaf is bounded and flash/static-resident; `6000` is
  pre-budgeted; all ML training (`9220`) is HOST-ONLY, only the baked
  table ships. `11700`/`11600` are HOST-ONLY by construction — the STM32
  target (`12000`) has its own separate physical UI (`12400`) and is
  never this front-end. No open leaf assumes device capacity that isn't
  there.
- **In-flight-first (`0500`):** the whole pre-GUI batch is closed. The
  readable surface exists and the styles genuinely differ (tempo +
  swing). The freeze line `11700` has been crossed and stays crossed.

---

## Owner decisions this document records

1. **Style-load semantics** (`2530`) — **DECIDED: keep the chord.** A
   style-load means "change the band under the same chord", not "new
   song" — the followed chord survives a style change. Shipped in the
   consolidation.
1b. **Live-vs-sequencer arbitration** (`2340`) — **DECIDED: live-priority.**
   When a ChordSequencer is running and the player also plays live, the
   live chord WINS while held; on release the sequencer resumes its own
   progression. With no sequencer a live chord latches. `kAuto`
   (last-writer race) retained only as an explicit legacy mode. **Shipped**
   (`ChordFollow::kLivePriority`, ABI-additive, engine default).
2. **MIDI-FX chain scope & ABI** (`5100`) — **RESOLVED by the GUI freeze
   line (`11720`):** `kMaxInserts=8`, UI-limited 4, per-role, ABI-none for
   the data-model increment. Locked; no longer open.
3. **Sequence fork — MIDI-FX vs Looper** — **SUPERSEDED by the freeze line
   (`11700`):** both `5000` and `6000` sit behind it (`11730`); their
   relative order was advisory only. `5100`/`5210`/`5220` and `6100`–`6400`
   both shipped since.
4. **GUI tech stack** (`11600`) — **DECIDED & vendored: Dear ImGui
   (`ocornut/imgui` v1.92.8) + GLFW3.** The dependency fork under policy
   `0800` is resolved and EXECUTED in code: vendored under
   `third_party/imgui` + `third_party/glfw` and built/linked by
   `apps/gui-sonotron/`. Rationale as-built: `docs/gui-and-ux.md` §1.
5. **D38 ("the GUI never links/#includes the core")** (`11600`) —
   **RELAXED / RETIRED, scoped to one library (owner-decided).** The GUI
   binary now hosts the engine in-process by default, so
   `gui_sonotron_engine` links `hostrt`/`runtime`/`arrangrr` directly
   (commits `bf2c4b2`/`8c9e54d`). The relaxation is scoped: every other GUI
   library (`gui_sonotron_models`/`brain`/`layout`/`screenshot`) still
   never includes the core, and `--control <path>` still runs as a pure
   client. Not a reshape of the ABI itself.
6. **Anti-sameness reframe** (`9100`/`9200`/`10000`, 2026-07-18) —
   **DECIDED:** `9100` records inter-style feel differentiation only.
   Short-term repetition within a style is WANTED; the open goal is that a
   style must "evolve over time" (fills/variation/motif development), not
   "never repeat." That property lives at `9220`/`10000`, not `9100`. See
   the `9000` band header note and the corrected "Recommended sequence"
   intro.
7. **`11600` split into `11610`–`11650`** (2026-07-18) — **DECIDED:**
   `11610` "GUI foundation" is ✅ done (mechanical strand + core-dependent
   strand + Repeat Zone 1–3 + song-mode Phase 1, all contract-test-pinned).
   At the time of this split, `11620` (in-process `program`-verb no-op),
   `11630` (Browser redesign remainder), `11640` (UI-animation), `11650`
   (song-mode Phase 2) were open sub-nodes; `11620` has since shipped
   ✅ done (commit `ec5b90d`, see `11600` above) and `11650` has since
   shipped ✅ done (commit `76a8774`, see `11650` above) — `11630`/`11640`
   remain open. Parent `11600` stays OPEN — the instrument is never
   "done." The DONE criterion for a GUI sub-node is recorded verbatim
   under `11600` above.
8. **New node `11640` — UI-animation** (2026-07-18) — **DECIDED:** carries
   the beat/bar-synchronized motion program
   (`docs/proposals/ui-animation-roadmap.md`), previously an unnumbered
   pointer under the old single `11600` entry. STATUS: ○ not started.
9. **New node `12510` — MIDI Implementation Chart + `12500` split**
   (2026-07-18) — **DECIDED:** `12510` (the 72-opcode → MIDI wire mapping
   spec) precedes and constrains the former single `12500` leaf, now split
   into `12520` DeviceProfile+ExternalSound, `12530` ordered Bank/PC/CC
   init, `12540` RPN/NRPN/14-bit/aftertouch, `12550`
   MIDI-Learn/ControllerMap.
10. **New band `13000` — Desktop feature expansion (twelve features)**
    (2026-07-19) — **PROPOSED, not decided.** Filed from a strategic
    proposal document translated in full to English
    (`docs/proposals/desktop-feature-expansion-2026-07.md`), describing
    twelve desktop feature evolutions — Looper in the Repeat Zone,
    Performance Transformation Surface, complete live Sequence Edit, Smart
    Form Builder, Seeded Variation Lab, Visualized Musical Causality, Live
    Musical Transaction Engine, Semantic Resource Browser, Musical Resource
    Graph, Lead Sheet and intelligent Chord Map, Nonlinear Arrangement
    Graph, Universal Resource Runtime — grouped into four architectural
    blocks (`13100`–`13400`, mirroring the source document's own Block
    A/B/C/D framing) with twelve leaf nodes (`13110`–`13440`). STATUS:
    every leaf is **○ not started / proposed only** — the owner asked only
    to have the proposal filed at the correct place in the tree, not to
    commit to building it, prioritize it, or schedule it; no existing
    node's status or wording was changed by this pass. Cross-references
    filed against `6000`/`11610` (Looper), `11630` (Browser/Resource
    Graph/Semantic Browser), `11640` (UI-animation/Causality), `11650`
    (SceneChain — Smart Form Builder/Arrangement Graph), plus two
    additional overlaps found during filing that are not tracked
    elsewhere in this tree: `docs/proposals/song-form-autoarrange.md` and
    `docs/proposals/song-form-option-a-wiring-plan.md` (pre-existing,
    unsuperseded auto-song-form analysis overlapping Smart Form Builder,
    `13430`).
11. **New band `14000` — BDD testing architecture (Gherkin + BabyBehave)**
    (2026-07-19) — **DECIDED: ACCEPT WITH CHANGES.** Filed from a
    BDD-testing-architecture review session (orchestrator +
    `torquato-qa-lead` + a general-purpose WebFetch pass +
    `corelli-architecture-critic` + `guido-process-analyst` + two `Explore`
    passes, 2026-07-19; no proposal document — this was a chat-session
    decision, its findings independently verified against the tree).
    Reuses `13xxx` node numbers directly as Gherkin tags (no separate
    Feature Registry); rejects a separate Interaction Plan / `.flow.yaml`
    artifact. Records the foundational clock-injection seam fix (`14110`)
    as blocking any timed `@ui` scenario at scale, the owner-set BDD
    milestone order Browser (`13330`) → Sequence Edit (`13120`) → Looper
    (`13110`) (`14120`–`14140`) — distinct from `13210`'s own product-build
    sequencing note, not a contradiction of it — the MVP cutline at
    `14150`, legacy-debt-baseline/CI-gate hardening as a distinct POST-MVP
    step (`14210`), and the `IMidiHal` fake/test-double (`14310`) filed as
    explicitly DEFERRED / NEEDS-DECISION (which real hardware to
    characterize against is still an open owner decision, not answered by
    this pass). No existing `13000` leaf's status or wording was changed by
    this filing.
11b. **Refined MVP priority ranking for band `14000`** (2026-07-19,
    same-day follow-up) — **DECIDED: three explicit priority tiers,
    ranked.** (1) NO CRASH — a non-negotiable stability gate (`14100.1`)
    above either wow tier, requiring ASan/UBSan sanitizer runs on the new
    BDD test binary and touched GUI code (verified absent today:
    `CMakePresets.json` has no sanitizer preset); closing `14110`'s
    clock-injection seam and `14130`'s read-back gap now explicitly serve
    this priority, not merely BDD convenience. (2) WOW — the existing
    Browser → Sequence Edit → Looper milestone sequence (`14110`–`14150`),
    unchanged. (3) SUPERWOW — a new tier, fluid/animated/innovative UI as
    its own goal, filed at `14400`/`14410`, cross-referencing `11640`
    (motion mechanism) + `13140` (causal data model) without adding new
    sub-nodes under either, sequenced strictly after tier (2). *Discrepancy
    found and NOT recorded as fact: the relayed brief for this follow-up
    also claimed "grepping for regression-labeled CTest tests project-wide
    returns zero hits" and an aspirational "metric-3 bug registry" —
    re-checked against the tree and found FALSE (`1340` already records the
    three-metric convention as `✅ done`; real `LABELS regression` tests
    exist and pass, e.g. `test_step_locks`,
    `test_performance_style_id_regression`; "bug registry"/"metric-3"
    appears nowhere in the repo). That specific claim is omitted from
    `14100.1`; only the verified sanitizer-preset gap is recorded there.

---

## Appendix — migration cross-reference (old identifier → new numeric ID)

The old milestone and decision codes are retired from the canonical text.
This table exists only so existing references in code/docs still resolve; a
single old code may map to several nodes (it was realized across them) and
several old codes may share a node.

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
| M10 | 12520–12550 *(formerly 12500, split 2026-07-18)* |
| M11 | 8500, 8600 |
| M12 | 12600 |
| M13 | 12100–12400 |
| H1  | 11100 |
| H2  | 11200 |
| H3  | 11300 |
| Phased #1..#9 | 3220 · 3250 · 7110 · 8100 · 4100 · 3260 · 4500 · 4400/12520–12550 · 10000 |

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
| D35 | 3240, 12520–12550 *(formerly 3240, 12500, split 2026-07-18)* |
| D36 | 3230 |
| D37 | 10000 |
| D38 | 11500, 11610 |
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

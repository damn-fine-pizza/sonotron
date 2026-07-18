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
  - `11620` In-process `program`-verb no-op fix — ○ HOST-ONLY. KNOWN GAP:
    `apps/gui-sonotron/src/in_process_brain_session.cpp`'s
    `command_line_to_command` — the translator the DEFAULT in-process GUI
    backend uses — has no `"program"` case among its `if` branches and
    falls through to `TranslateOutcome::kUnknownCommand` (re-verified
    2026-07-18: no `"program"` branch present), so BOTH the F1 Voices
    picker and the F2 Kits picker (see `11630`) silently no-op when the GUI
    runs in-process (the default). `program` only reaches the engine today
    via an external server (`--control`/UDS,
    `components/platform/hostrt/shell_music_commands.cpp`'s `cmd_program`,
    reached through `UdsBrainSession`). This node fails the `11610` DONE
    criterion by construction (a known NO-OP path in the default run mode)
    — that is exactly why it is its own open sub-node rather than folded
    into `11610`.
  - `11630` Browser redesign — remainder — ○ HOST-ONLY. F1 (commit
    `bce71ab`: category selector + Sections/Variations click→apply +
    Voices/GM-program picker) and F2 (commit `6a8799e`: real GM
    percussion-Kit category on the `program` verb) are ✅ shipped and
    counted under `11610`'s Browser-redesign contribution. What remains is
    proposal Phase 3 of `docs/proposals/browser-redesign-taxonomy.md`:
    Songs/scene-chains and Performances tabs (blocked on song-mode Phase 2,
    `11650`, per that proposal's own §4 — "earliest sane start... is after
    song-mode Phase 2"), Chord-progressions/Loops/Pad-FX-Groove-preset tabs
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
  - `11650` Song-mode Phase 2 — ○ not started. Scope (per
    `docs/proposals/song-mode-scenechain-adoption.md` §"Phasing", Phase
    2): `GridModel` grows a `ScenePerformance` per-scene record
    (style/groove/key/tempo) + a per-scene editor UI; the Phase-1
    capture-and-override mechanism (already shipped, `11610`) picks the
    new fields up automatically once they exist. Blocks `11630`'s
    Songs/Performances Browser tabs.

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
   `11620` (in-process `program`-verb no-op), `11630` (Browser redesign
   remainder), `11640` (UI-animation), `11650` (song-mode Phase 2) are
   open sub-nodes. Parent `11600` stays OPEN — the instrument is never
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

# arrangrr — Canonical Numbered Roadmap (WBS) — PROPOSAL

Status: PROPOSAL (Verdi, 2026-07-06). This is the strategist's proposal for **the
single canonical roadmap AND the single decision record** for arrangrr. One
nomenclature only: **the hierarchical number is the identity.** Every former
milestone and every former decision becomes a NODE in this tree, with its rationale
folded into that node. There is no parallel milestone list and no parallel decisions
log; both collapse here.

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
- **0910 Captured direction — `melodd` audio companion (not scheduled).** arrangrr (the
  MIDI brain) and a future host-only audio engine are peer modules wired by an
  orchestrator, name-blind, talking only through the POD interface; audio never
  crosses the interface; the core stays audio-ignorant (identity `0110` intact). A
  recorded direction, not work.
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
    `docs/reviews/followed-chord-context-ownership.md`*
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

### 5000 — MIDI-FX / Transform chain — ○ planned (behind the GUI freeze line, `11700`)

*A composable bounded chain (fixed max inserts) of MIDI transforms per track/zone —
the open/hackable north-star (`0600`) made concrete. Arp/groove/scale-lock become
INSTANCES of the chain, not disconnected modules.*
- **5100 Insert-chain framework** (bounded, per-track/zone, POD) — ○ SHIPPABLE,
  **shape pre-fixed at the GUI freeze line (`11700`)**: `kMaxInserts=8` in the
  on-disk/ABI format, UI exposes 4; per-track first (per-zone deferred); ABI-none
  for this data-model increment — locked so the GUI (`11600`) is born aware of
  this surface and is not rebuilt when `5000` lands. The earlier
  **NEEDS-DECISION is RESOLVED** by that lock; implementing the framework body +
  inserts remains ○ planned, behind the freeze line.
- **5200 Refactor existing modules into chain instances**
  - `5210` groove as a chain instance — ○ SHIPPABLE
  - `5220` arp as a track MIDI-FX instance — ○ SHIPPABLE *(= 7130)*
  - `5230` scale-lock / scale-filter as a chain instance — ○ SHIPPABLE
- **5300 New inserts (SHIPPABLE)**
  - `5310` echo / MIDI-delay — ○
  - `5320` note-repeat / ratchet insert — ○
  - `5330` velocity-proc / probability / randomize — ○
  - `5340` harmonize / chord-memory-expand — ○

### 6000 — Looper (the missing "capture" gesture) — ○ planned (behind the GUI freeze line, `11700`)

*Completes the write/generate/capture triad of the unified timeline (`0130`). Budget
pre-sized by `0400` (8×3072 ev = 192 KB).*
- `6100` Record / overdub / replace / erase / undo — ○ SHIPPABLE
- `6200` Quantize-after (non-destructive) — ○ SHIPPABLE
- `6300` Retroactive capture (always-on ring, "grab last N bars") — ○ SHIPPABLE
- `6400` Loop length (fixed/auto/quantized), per-track/global — ○ SHIPPABLE
- `6500` Sync + follow-chord capture (re-harmonize on chord change) — ○ SHIPPABLE

### 7000 — Expression — ◑ partial

- **7100 Arpeggiator engine**
  - `7110` Live-keyboard arp (rate/dir/octaves/gate/latch/seed) — ✅
  - `7120` Arp as a style part (replace hand-written kArp patterns) — ○ SHIPPABLE
  - `7130` Arp as a track MIDI-FX — ○ SHIPPABLE *(= 5220)*
- **7200 Phrase/Pad engine** (banks of 4; one-shot/loop/hold/toggle) — ○ SHIPPABLE
- **7300 Groove/Humanize** — ✅ *(shared with 3250)*
- **7400 Metronome/click, tap-tempo, tempo-nudge** — ○ SHIPPABLE

### 8000 — Structure & recall + persistence — ○ planned (behind the GUI freeze line, `11700`)

- `8100` Scenes / song mode (snapshot + chain + tempo/time-sig) — ○ SHIPPABLE
- `8200` Performance/Registration (recall live state) — ○ SHIPPABLE
- `8300` SetList — ○ SHIPPABLE
- `8400` Project/Preset manager (slots, defaults) — ○ SHIPPABLE
- `8500` Versioned binary storage + CRC (save/load round-trip) — ○ SHIPPABLE
- `8600` Diagnostics/MIDI monitor + fault-injection suite — ○ HOST-ONLY

### 9000 — Style content & tooling — ◑ partial / decided

- **9100 Per-style feel — ○ planned (next musical lever, owner-approved ASAP)**
  - `9110` Style owns its default GrooveParams — ○ SHIPPABLE (**ABI/struct change, approved**)
  - `9120` Style owns its tempo — ○ SHIPPABLE
  - `9130` Triplet / shuffle grid (feel expressible in note placement) — ○ SHIPPABLE
  *ranked the single biggest lever against style sameness (corpus measurement,
  `docs/reflections/style-differentiation-and-generation.md`)*
- **9200 Generative style — ○ planned**
  - `9210` Motif + transforms (diatonic transpose/retrograde/displacement, seeded) — ○ SHIPPABLE
  - `9220` Offline-trained Markov/grammar on scale degrees, baked constexpr — ○ runtime SHIPPABLE / training HOST-ONLY
- **9300 MIDI stylizer — ○ planned (host-only)**
  - `9310` Accompany (keep the melody, play the genre band under detected chords) — ○ HOST-ONLY *(zero new core, ships first)*
  - `9320` Restyle (transform the input's own parts into the genre idiom) — ○ HOST-ONLY *(depends on 9100)*
- **9400 Style data format + generator — ○ direction**
  - `9410` Style inspector + serialize/deserialize (offset/index-based) — ○ HOST-ONLY
  - `9420` Style compiler (data → .cpp constexpr for the device path) — ○ HOST-ONLY
  - `9430` CASM→NTT importer body (arrstyle-converter; 1010-style corpus) — ○ HOST-ONLY

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
- **11400 Piano source-coloured visualizer**
  - `11410` GREEN (current bar) + ORANGE (next/pending chord) — ○ HOST-ONLY
  - `11420` WHITE (direct play) — ○ HOST-ONLY *(deferred: needs a sounding melody surface)*
- `11500` UDS-JSONL control adapter (one protocol, three consumers) — ✅
- `11600` Host GUI client — the TARGET of the GUI freeze line (`11700`): separate
  process, pure client, never links core. Tech stack (Dear ImGui or an alternative)
  is an OPEN dependency decision, evaluated WITH the owner under `0800` when the
  freeze line's pre-GUI batch (`11710`) closes — NOT picked here. — ○ HOST-ONLY
  (**dependency flag: GUI toolkit — decision deferred to freeze-line crossing**)

- **11700 GUI freeze line — pivot from core-feature work to the host GUI**
  (owner-decided). STATUS: DECIDED (gate, not a schedulable work item — see the `0000`
  numbering note on invariants vs. work; this node governs sequencing of everything
  below it, the way `0000` governs everything in the tree).
  *Through-line: **validate feel in the hands, then grow on a living instrument.** The
  product is a MIDI arranger — a live instrument whose value is in the hands. The TUI
  structurally cannot validate FEEL (timing, the chord-steer sensation, the piano
  visualizer's readability): feel lives in the hands, not in a text panel. So
  core-feature work STOPS at a small, well-defined batch, and the product PIVOTS to
  building the host GUI (`11600`). Every remaining musical feature (`5000`, `6000`,
  `8000`, `10000`, and the rest of `9000`) is grown AFTER, on an instrument that
  already exists and already sounds — reprioritized by real feel-in-the-hands testing,
  not by this document's current ranked order.*
  - `11710` Pre-GUI gating batch — ALL FOUR must be ✅ before the line is crossed
    (invariant `0500`, vertical-first: the GUI is built on solid, verified ground, not
    raced onto half-built cells):
    1. `2530` Single-owner FollowedContext consolidation — ▶ SHIPPABLE, **in flight
       now** (band `2500`). The GUI's central interaction — steer/follow — must be
       correct and un-raced before a visual surface is built on top of it.
    2. `11410` Piano source-coloured visualizer, GREEN (current bar) / ORANGE
       (pending) — ○ HOST-ONLY, small. The GUI's central *readable* surface; must
       exist and be verified standalone before the GUI is built around it, not
       invented inside the GUI build itself.
    3. `9100` family (`9110`/`9120`/`9130`) Per-style feel / anti-sameness — ○
       SHIPPABLE, **owner-approved**. Gates the GUI's style picker: it must present
       genuinely different styles, not 16 clones of one feel.
    4. `1270` Realtime hardening — sustained-play crackle defect — ▶ under
       investigation. **Non-negotiable precondition, not a nice-to-have**: a
       crackling instrument is not shippable as a GUI you actually play.
  - `11720` At-the-freeze-line actions (executed once `11710` is all ✅):
    - Freeze the CURRENT ABI command/event surface (`0700`) as of this point. Corelli's
      architecture verdict: the ABI is additive/healthy, so freezing now is safe —
      later features (`5000`, `6000`, `8000`, `10000`) extend it, they do not break it.
    - Pre-fix the SHAPE of the MIDI-FX ABI verbs (`5100`, updated above) even though
      unimplemented, so the GUI is born aware of that surface and is never rebuilt
      when `5000` lands.
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
1. **`2530` — Single-owner FollowedContext consolidation.** ▶ SHIPPABLE, no ABI, no
   dep. FINISHES `2510` correctly and removes the three reported bugs by construction.
   The GUI's central interaction (`11410`, and everything downstream: `9320`, `10000`)
   reads through this cell; landing it half-built is compounding debt. Fold in `2540`
   (delete dead seam). **Do this first — it is already in flight.**
2. **`11410` — Piano visualizer GREEN+ORANGE.** HOST-ONLY, small, rides the pending
   state that `2530` stabilizes. Cheap, high perceived value: it makes the harmony
   visible, not just correct — and it is the GUI's central readable surface, so it
   must be verified standalone before `11600` is built around it.
3. **`1270` — Realtime hardening: close the sustained-play crackle defect.** ▶ under
   investigation. **Non-negotiable precondition of `11700`**, not a nice-to-have — run
   this in parallel with or immediately after 1–2; a crackling instrument is not
   shippable as a GUI you actually play.
4. **`9110`/`9120`/`9130` — Per-style feel.** SHIPPABLE, ABI/struct change **already
   owner-approved**, ranked the single biggest lever against style sameness. Gates the
   GUI's style picker (`11700`/`11710`.3): it must present genuinely different styles,
   not 16 clones. GrooveParams+tempo per style, flash-resident, dual-target clean;
   `9130` triplet grid follows as a second pass.

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
  today's constexpr header from its model.
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
- **No new core dependency** is introduced by any SHIPPABLE item. The dependency flag
  in the open set is the **GUI toolkit** for `11600` (HOST-ONLY, policy `0800`,
  decided AT the freeze line, not before).
- **Dual-target / no-heap reality (`0200`/`0300`/`0400`):** every SHIPPABLE leaf is
  bounded and flash/static-resident; `6000` is pre-budgeted; all ML training (`9220`)
  is HOST-ONLY, only the baked table ships. `11700`/`11600` are HOST-ONLY by
  construction — the STM32 target (`12000`) has its own separate physical UI (`12400`)
  and is never this front-end. No open leaf assumes device capacity that isn't there.
- **In-flight-first (`0500`):** `2530` and `1270` must both close before the freeze
  line is crossed. Opening `11600` (or `5000`/`6000`) over a racing followed-context
  cell or an open crackle defect is exactly the failure `0500` names.

---

## Owner decisions this proposal surfaces

1. **Style-load semantics** (`2530`): does a style-load mean "new song" (drop the
   chord) or "change the band under the same chord" (keep it)? The consolidation
   defaults to *keep*; confirm — one line either way.
2. **MIDI-FX chain scope & ABI** (`5100`) — **RESOLVED by the GUI freeze line
   (`11720`):** `kMaxInserts=8`, UI-limited 4, per-track first, ABI-none for the
   data-model increment. Locked; no longer open.
3. **Sequence fork — MIDI-FX vs Looper** — **SUPERSEDED by the freeze line (`11700`):**
   neither `5000` nor `6000` runs before the GUI now; both sit behind it (`11730`).
   Their relative order (this proposal's steps 7 vs 9) is advisory only — it should be
   re-decided by real feel-in-the-hands testing once `11600` exists and sounds, not by
   this document.
4. **GUI tech stack** (`11600`, surfaced by `11700`/`11720`): ImGui or an alternative —
   a genuine dependency fork under policy `0800`, to be evaluated WITH the owner when
   the pre-GUI batch (`11710`) closes. Not picked here; flagged.

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

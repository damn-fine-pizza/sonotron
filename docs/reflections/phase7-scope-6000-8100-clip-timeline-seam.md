# Phase 7 scope: node 6000 (Looper) / node 8100 (Scenes/song mode) — the clip/timeline seam

Status: SCOPE DOC (read-only architecture analysis). No product code touched.
Author: Corelli (architecture critic), delta+seam analysis against the as-built tree.
Scope trigger: docs/DESIGN.md `6000` (lines 866-874) and `8100` (lines 897-919).

**Mid-session directive applied to this doc (recorded for transparency):** the
launching agent relayed an owner instruction that, for THIS design pass, ABI
stability is NOT a limiting constraint — the `sizeof` pins
(`Insert==6`/`PerfInsert==6`/`Command<=20`/`OutEvent<=16`), the `Performance`
wire format, and migrator discipline are all fair game to reshape. Section 4
below is written under that license: it proposes the structurally cleanest
shape, not the one that preserves the current wire. Still hard, per the same
directive: the freestanding/no-heap/dual-target STM32H743 M7 realtime core
(bounded static state, the 192 KB event budget is real money) and
golden-regression correctness where no deliberate musical-output change is
intended.

---

## 1. As-built inventory

**`ClipMatrix` is a launch grid only — it holds NO recorded content.**
`components/arrangrr/include/arrangrr/clip/clip_matrix.hpp:17-19` states the
tripwire explicitly in its own header comment: *"ClipMatrix owns ONLY {content
ref, launch state, pending boundary}. NO record field on Clip, NO capture()
method here — that is node 6000 (the Looper) territory, out of scope."* The
`Clip` struct (`clip_matrix.hpp:50-63`) is an 8-byte POD: `part_role`,
`scene_index`, `ContentKind` (`kStyleSection`/`kChordSequence`/`kStepTrack`),
`content_index`, `LaunchState`, `n_bars`. `ClipMatrix` never touches
Arranger/ChordSequencer/Timeline itself (`clip_matrix.hpp:21-24`); the one
place that translates a fired clip into a real effect is
`Engine::apply_clip_content` (`components/arrangrr/src/engine.cpp:842-880`),
which dispatches on `ContentKind` to three EXISTING, pre-authored content
stores:
- `kStyleSection` → `Arranger::request(section, immediate)` (a hand-authored style section, generative playback).
- `kChordSequence` → `ChordSequencer::use()/play()` (see below).
- `kStepTrack` → mutes/unmutes a `Timeline::Track` (a hand-authored step grid).

**`Timeline` (`components/arrangrr/include/arrangrr/timeline/timeline.hpp`) is
a fixed-length, hand-authored GRID, not a recorded event buffer.** `Track`
(`timeline.hpp:66-74`) holds `Step steps[kMaxStepsPerTrack]` — one `Step`
(8 bytes, `timeline.hpp:54-64`, `static_assert(sizeof(Step)==8)`) per quantized
16th-grid position, `kMaxTracks=16 x kMaxStepsPerTrack=64` (`config.hpp:15-16`)
= 1024 slots x 8 B = 8 KB total. Steps are written by `set_step()`
(`timeline.hpp:103-127`), a direct authoring call, not a live-capture path.
There is no free timing, no undo, no overdub-into-existing-notes semantic —
this is the "write" gesture of the unified timeline (D10), explicitly distinct
from the still-missing "record"/"capture" gesture DESIGN.md assigns to node
`6000` (`docs/DESIGN.md:868`: *"Completes the write/generate/capture triad of
the unified timeline (0130)"*).

**The one genuine "recorded, free-duration event timeline" primitive that
already exists is `ChordSequencer`/`ChordSequence`** — but it is scoped
strictly to functional CHORD data, never raw MIDI notes:
`components/arrangrr/include/arrangrr/chord/chord_sequencer.hpp:52-98` ships
`start_record(Tick)` / `capture(Tick, degree, quality_ovr, velocity)` (closes
the previous open step, opens a new one) / `stop_record(Tick, grid)` (closes
the tail step, then calls `quantize()` — "quantize-after", D14). The payload,
`ChordStep` (`chord_sequence.hpp:19-26`, 12 bytes, `static_assert(sizeof==12)`)
is `{start, duration, degree, quality_ovr, velocity}` — a **functional,
key-relative** representation: transposition moves only the reference `Key`
(`chord_sequence.hpp:107-115`, `transpose_to`/`transpose_by`) and playback
re-derives the concrete chord every time (`chord_sequencer.hpp:141-153`), per
D28. Capacity: `kMaxChordSequences=16 x kMaxChordSteps=128 x 12B = 24 KB`
(`config.hpp:17-18`, matching the `static_assert` comment in
`chord_sequence.hpp:26`). This is architecturally the closest existing sibling
to what `6100`/`6200` need, but it records CHORDS, not NOTES, and it has no
overdub (record always `clear()`s first, `chord_sequencer.hpp:59`), no erase,
no undo, and no "always-on ring" mode — it is record-once/stop/quantize, full
stop.

**The `6000` 192 KB budget (`8×3072 ev = 192 KB`, `docs/DESIGN.md:869`, tied to
invariant `0400`, `docs/DESIGN.md:689-693`: `Event = 8-byte POD`) is
UNALLOCATED.** No constant for it exists in `config.hpp` (I read the file in
full — the only pool constants there are `kMaxTracks`/`kMaxStepsPerTrack`,
`kMaxChordSequences`/`kMaxChordSteps`, `kMaxClips`, `kMaxPads`,
`kMaxPerformances`, `kMaxChainFan`). `Timeline`'s own 8 KB step-grid pool is a
DIFFERENT, smaller, pre-authored allocation, not this one. `192 KB / (8
slots × 3072 events) = 8 bytes/event` — the intended per-captured-event shape
is meant to be as compact as `Step` (8 B), which is a real constraint on how
much functional/NTT metadata (role, chord-tone index, octave, source-kind) a
captured event can carry alongside its free-duration timing fields; see §3/§5.

**No record/capture/overdub/undo path exists anywhere in the tree for raw note
events.** `search_code`/`grep` across the whole repo for
`Looper|overdub|retroactive capture` outside `docs/DESIGN.md` and the
`ClipMatrix` tripwire comment returns nothing in `components/`. `abi.hpp` has
verbs for CHORD-sequence record only: `kSeqRec = 21` / `kSeqLoop = 23` /
`kSeqStop = 25` (`abi.hpp:114-121`) — these are the wire surface for
`ChordSequencer`'s record path, not a note-level Looper. Node `6000` is
genuinely greenfield inside an otherwise-populated neighborhood: the closest
kin (`ChordSequence`) is a proven SHAPE, not reusable CODE, because its
payload is chord-functional, not note-level.

**`Performance` (`components/arrangrr/include/arrangrr/perf/performance.hpp`)
explicitly excludes node `8100`.** Its own header comment
(`performance.hpp:12-18`) states: *"Deliberately NOT ... DESIGN.md section 7's
Song-anchored timed Scene (node 8100, a separate, later concern)."* `8100`'s
own DESIGN.md entry (`docs/DESIGN.md:899-904`) confirms: *"explicitly NOT
built by Phase-5 Item #9... Still planned, unchanged."* `Performance` is a
576-byte flat POD SNAPSHOT of live rig state (routes, mute/solo, groove,
style/variation, transpose, chord mode/follow/sequence, FX insert-chains,
`format_version 2`) with NO ordering/chaining concept of its own — one slot,
one recall, atomic (`Engine::apply_performance`,
`components/arrangrr/src/engine.cpp:1283-1362`). There is no `SceneChain`,
no ordered sequence of Performances, anywhere in the tree.

**Time signature does not exist as a runtime concept anywhere in the core.**
`components/common/include/common/time.hpp:22-29`: `kBeatsPerBar = 4` is a
`constexpr` GLOBAL, with the comment *"Minimal 4/4 metric for M0 (a real
TimeSignature engine lands with M5/M6"*). `kTicksPerBar` (derived from it) is
consumed as a COMPILE-TIME constant across the tree — I traced 13 production
call sites: `arranger.hpp`, `chord_sequence.hpp`, `chord_sequencer.hpp`,
`boundary_latch.hpp`, `restyle_stage.hpp`, `clip_matrix.hpp`, `engine.hpp`,
`engine.cpp`, `runtime/transport.hpp`, plus `hostrt`'s `shell_parse.cpp`/
`shell_io_commands.cpp`/`shell_music_commands.cpp` (full path list gathered
via `grep -rl kTicksPerBar`). `8100`'s own DESIGN.md wording — *"snapshot +
chain + tempo/time-sig"* (`docs/DESIGN.md:899`) — therefore needs a per-scene
TIME SIGNATURE field that has NO backing engine to read from today; this is
not a struct-field addition, it is a prerequisite feature (§3/§5, Fork D).

---

## 2. The seam decision — ONE shared clip/timeline primitive, or two?

**Recommendation: NOT one shared class-level primitive. ONE shared
STRUCTURAL PATTERN, instantiated twice, plus a genuine (small) touch-point at
the `ClipMatrix` launch layer.**

### Option A — force both into ONE generic timeline/clip class

A single template `EventSequence<Payload>` (bounded `StaticVector` of
`{start, duration, Payload}`, `record()`/`quantize()`/`clear()`/loop flag)
used BOTH for `6000`'s note-loop payload and `8100`'s scene-chain payload,
possibly even folded into `ClipMatrix` itself as a 4th `ContentKind`.

Rejected. Evidence against:
- `ClipMatrix`'s own scope tripwire (`clip_matrix.hpp:17-19`) is a LOCKED
  decision that a launch-grid cell never owns recorded content — folding a
  note-event ring or a scene-chain INTO `Clip`/`ClipMatrix` directly
  contradicts a decision already written into the file, not a stale one.
- The two payloads have irreconcilable UPDATE CADENCE and WRITE PRESSURE: a
  loop buffer is written on every live note during active recording (up to
  the tick rate) and read every tick during playback (a true realtime-path
  citizen, grafted beside `Timeline::on_tick`/`ChordSequencer::on_tick`,
  called from `Engine::on_tick`); a scene chain advances only at BAR/SCENE
  boundaries (an occasional macro-sequencer, structurally a sibling of
  `ChordSequencer`'s OWN `play()`/`on_tick()` transport-like driver, not the
  hot per-note path). One class serving both is a "swiss-army-knife"
  abstraction — exactly the kind of invented-ceremony Corelli's method
  flags: forcing two orthogonal concerns to share one type buys nothing and
  costs a leaky abstraction (the class would need to know which cadence it
  is in).
- Payload shape mismatch: a captured note event needs `{start, note-ish
  payload}` at ~8 B/event (the `0400`-derived budget, §1); a scene-chain step
  needs `{start_in_bars, performance_slot_id, transition_kind}` — the units
  of `start` themselves differ (raw ticks vs. bars), which a shared class
  would have to abstract away for no shared benefit.

### Option B — two fully independent, unrelated primitives

Build `LoopBuffer` (6000) and `SceneChain` (8100) with no acknowledged
kinship at all.

Partially rejected: it would silently re-derive `ChordSequence`'s own proven
shape (record/quantize-after/loop, free-duration `{start, duration}` steps)
twice from scratch, without naming the reuse, which is how a codebase ends up
with three near-identical hand-rolled sequencers that drift from each other
over time (a coupling/cohesion smell even without any shared TYPE).

### Recommended — Option C: shared PATTERN, separate INSTANCES, one small launch-layer touch-point

1. **`LoopBuffer` (node 6000)** — a new, note-payload sequencer living beside
   `ChordSequencer` as an `Engine`-owned peer (NOT inside `Arranger`; see §3),
   following `ChordSequence`'s own proven record/quantize-after/loop shape
   (`record()`, `quantize(grid)`, `loop` flag) but reshaped around a
   note-level payload. This is a NEW class — reusing the SHAPE, not the CODE
   (the payload and the record semantics — overdub/erase/undo — genuinely
   differ from `ChordSequence`'s record-once model; see §3).
2. **`SceneChain` (node 8100)** — a new, thin ordered sequence over EXISTING
   `PerformanceStore` slot indices: `{start_bar, performance_slot, bars,
   transition}`. Structurally this is the SAME shape again
   (`StaticVector` of `{start, duration, functional-reference}` steps,
   quantize-at-record-boundary optional) but reused for scene ordering, not
   chord degrees or notes. `Performance` ITSELF needs no reshape to become
   the snapshot half of `8100` — it was deliberately factored out for exactly
   this reuse (`performance.hpp:12-18`) — `8100`'s true delta is almost
   entirely this new chain class plus the missing tempo/time-sig field
   (§3/§5, Fork D).
3. **`ClipMatrix` gets ONE small, additive touch-point, not a merge**: once
   `LoopBuffer` exists, a captured loop is a natural 4th launchable
   `ContentKind` (`kLoopBuffer`) beside `kStyleSection`/`kChordSequence`/
   `kStepTrack` — "launch loop N from the grid" is exactly the same shape as
   the three existing kinds. `ContentKind` is a `std::uint8_t` enum with 3 of
   256 values used and `content_index` is already `std::uint16_t`
   (`clip_matrix.hpp:31-35,54`) — this is a textbook additive enum extension
   that needs no reshape even without the ABI-freedom directive.
   `SceneChain`/`8100` is DIFFERENT: `Performance`'s own header comment
   already distinguishes it from *"the GUI's own Repeat-Zone 'Scene', a
   clip-column"* (`performance.hpp:14-15`) — song-mode scene playback is not
   naturally a grid-launchable cell, it is its own transport-like driver
   (mirroring `ChordSequencer`'s `play()`/`on_tick()`), so it should NOT be
   forced through `ClipMatrix` (Fork B, §5).

**Why this decides serial vs. parallel implementation:** `6000` and `8100` do
NOT share a load-bearing class, so they are NOT serially blocking at the
IMPLEMENTATION level — `SceneChain` does not need `LoopBuffer` to exist, and
`LoopBuffer` does not need `SceneChain`. They CAN be built in parallel. But
they DO share a PATTERN and both graft near `ChordSequencer`'s own precedent,
so the owner should lock the shared shape (record/quantize-after semantics,
`{start, duration, payload}` discipline) ONCE, reviewed against both use
cases at the same time, so the second implementation does not quietly diverge
from the first's proven idiom. Recommendation: **design the shared pattern
in one pass (a short internal note, not a shipped abstraction — do not build
a template class prematurely per D4/anti-ceremony), then implement 6000 and
8100 as two independent workstreams.**

---

## 3. Node-by-node delta

**`6100` Record/overdub/replace/erase/undo** — grafts a NEW `LoopBuffer`
(Engine-owned peer of `ChordSequencer`/`Timeline`, referenced from
`Engine::on_tick`, NOT from `Arranger::on_tick`'s generative kernel — mirrors
how `ClipMatrix`'s own header comment already documents the Engine-owns-all-
three-subsystems discipline, `clip_matrix.hpp:21-24`). `record`/`replace`
mirror `ChordSequencer::start_record` (clear-then-record). **`overdub` is
genuinely NEW** — `ChordSequence::record()` (`chord_sequence.hpp:71-73`) only
ever appends to an already-cleared buffer; there is no existing merge-into-
existing-content code path anywhere in the tree to reuse. **`erase`/`undo`
are also genuinely new** — nothing in `ChordSequence` supports removing a
single captured region or reverting to a prior generation; `undo` in
particular needs at least one retained prior state (a shadow copy or a
bounded diff-log), which is a REAL memory cost against the (already tight,
§1) 192 KB event budget and needs its own explicit sizing decision (Fork A).
Touches the realtime path only at the PLAYBACK side (a new
`LoopBuffer::on_tick`-shaped method called from `Engine::on_tick`, sibling to
`Timeline::on_tick`); recording itself is I/O-bound by the live player, not a
hot-path concern, UNLESS 6300 is enabled (see below).

**`6200` Quantize-after (non-destructive)** — directly reuses the ALGORITHM
already proven in `ChordSequence::quantize()`
(`chord_sequence.hpp:81-103`: snap starts to grid, rebuild durations from
consecutive starts, keep tail duration ≥ one grid unit), re-applied to
note-level `{start, duration}` steps. Lowest-risk delta in the whole band —
this is copy-the-proven-algorithm, not new design. No realtime-path touch
(runs once, at `stop_record`, exactly like today's chord path).

**`6300` Retroactive capture (always-on ring, "grab last N bars")** — the
ONE node in this band that DOES touch `on_tick`'s hot path directly, and
should be flagged as such. "Always-on" means the ring must be fed on EVERY
tick regardless of whether the user has pressed record — the natural tap
point is wherever `Engine` already calls `OutScheduler::schedule` from
`Arranger`'s `NoteScheduleFn` callback (`arranger.hpp:683-694`,
`schedule_fanned_note`) — i.e. a passive tee on the SAME note stream the
scheduler already consumes, not a new emission path. This is a permanent tax
on the realtime kernel for every tick the transport runs, whether or not
6300 is ever used by the player — a genuine cost/architecture question for
the owner (Fork C): gate the tee behind a live "arm retroactive capture"
flag (skips the write when not armed, cheap: one branch) vs. truly always-on
(simpler code, permanent branch+write cost on every scheduled note even when
no one will ever use it on a given track).

**`6400` Loop length (fixed/auto/quantized), per-track/global** — a scalar
config mirroring two EXISTING precedents in the same neighborhood:
`Track::length` (`timeline.hpp:70`, per-track polymeter) and
`ChordSequence::loop` (`chord_sequence.hpp:31`) + `length()`
(`chord_sequence.hpp:38-44`, derived from the last step's own end). Low risk,
no new realtime-path touch beyond what 6100/6300 already add.

**`6500` Sync + follow-chord capture (re-harmonize on chord change)** — the
node that actually DECIDES the storage shape for the whole 6000 band, and
should be settled FIRST, before any 6100 struct is cut in stone (changing
storage representation later is a rewrite, not a migration). The entire rest
of the arranger core stores musical content FUNCTIONALLY, never as raw
absolute pitch (D24 NTT: `Arranger::resolve()`,
`arranger.hpp:721-757`, always derives an absolute note from
`{chord-tone-index, octave, role-anchor}` at the moment of play;
`ChordSequence` stores `degree`, never a pitch, precisely so `transpose_to`/
D28 re-harmonizes for free, `chord_sequence.hpp:105-115`). A shared
"followed chord" context (`ChordEngine`'s `m_followed`,
`components/arrangrr/include/arrangrr/chord/chord_engine.hpp:192-213`,
`set_follow`/`follow()`/`state()`) is ALREADY the one place every
chord-aware consumer reads current harmonic context from. If `6000`'s
captured events store a RAW absolute MIDI note (the "obvious" looper
design), `6500`'s re-harmonize-on-chord-change is architecturally expensive
and ad hoc (heuristic re-transposition after the fact, no functional ground
truth to re-derive from) — and it would be the ONE storage in the whole
core that breaks the NTT discipline every sibling around it observes. If
captured events instead store a CHORD-TONE-RELATIVE representation (role +
tone-index + octave + source-kind, resolved backward at capture time,
mirroring `StyleEvent`'s own shape, `arranger.hpp:721` neighbourhood),
`6500` becomes close to free: playback just calls the SAME `resolve()`
pipeline against the CURRENT `chord`/`key`, exactly like every generative
pattern already does. This is a genuine, load-bearing fork the owner must
settle before 6100 (Fork A/E below) — it also interacts directly with the
8-byte/event budget from `0400` (§1): a chord-tone-relative payload needs to
fit role+tone+octave+source+velocity+gate in the same 8 bytes `Step` already
uses, which is tight but not impossible (`Step` itself already packs 6 useful
fields into 8 B).

**`8100` Scenes / song mode (snapshot + chain + tempo/time-sig)** — the
snapshot half is DONE and deliberately reusable: `Performance`
(`performance.hpp`) already IS the "one full rig state" unit `8100` wants per
scene (`performance.hpp:12-18` names this reuse explicitly by exclusion). The
real delta is narrow: (a) the NEW `SceneChain` ordering primitive (§2,
Option C) referencing `PerformanceStore` slot indices, not duplicating their
content; (b) the time-signature gap (§1) — `kBeatsPerBar`/`kTicksPerBar` are
GLOBAL COMPILE-TIME constants consumed at 13+ call sites across
`Arranger`/`ChordSequence`/`ChordSequencer`/`ClipMatrix`/`Engine`/
`BoundaryLatch`/`RestyleStage`/`Transport` — giving `8100` a genuine
per-scene time-signature is NOT a struct-field addition, it is a
prerequisite variable-time-signature engine touching every one of those call
sites (Fork D). Realtime-path touch: `SceneChain::on_tick` (bar-boundary
gated, mirrors `ChordSequencer::on_tick`'s own cadence) calls
`Engine::apply_performance` at each scene transition — same cost class as an
existing `Performance` recall, not a new hot-path burden.

---

## 4. Structural (not ABI-weighted) impact — persistence, sizes, no-heap budget

*(Re-framed per the mid-session directive: propose the cleanest shape; ABI
back-compat and the current `Performance` wire are not limiters here. The
STM32 no-heap/bounded/dual-target constraint and golden-regression
correctness remain hard.)*

- **`LoopBuffer` payload struct** — cleanest shape, not wire-constrained: I
  would NOT force it into 8 bytes if the chord-tone-relative representation
  (Fork A/E) needs more room to stay honest (e.g. 10-12 B, matching
  `ChordStep`'s own 12 B precedent). The `0400`/`8×3072` note in DESIGN.md
  is a PLANNING estimate, not a shipped `static_assert` — nothing in
  `config.hpp` pins it yet (§1), so this is free to size correctly for the
  chosen representation before it is ever committed to a constant. Still
  HARD: total pool size is a bounded, static, compile-time-sized array (no
  heap, D32) sitized against the real STM32H743 512 KB envelope (D33) — the
  event COUNT × slot-count × struct-size must be re-derived and
  `static_assert`-pinned once the payload shape is chosen, not assumed to be
  192 KB just because DESIGN.md said so under an 8-B assumption that may no
  longer hold.
- **`undo`'s shadow-copy cost** — with ABI weight off the table, the honest
  move is to size it explicitly (e.g. one extra generation per loop slot,
  doubling that slot's footprint) and `static_assert` the doubled total,
  rather than retrofitting undo into an already-sized single-generation
  buffer later.
- **`Command`/`OutEvent` additions** — freely extendable now: 6000 needs new
  `Param` verbs (record/overdub/replace/erase/undo/loop-length, mirroring
  `kSeqRec`/`kSeqLoop`/`kSeqStop`'s existing shape at
  `abi.hpp:114-121`) and likely a new `OutEvent::Kind` for loop
  state/position feedback (mirroring `kSection`/`kChordFollowed`'s existing
  shape, `abi.hpp:359-364`). With the `sizeof(Command)<=20`/
  `sizeof(OutEvent)<=16` pins explicitly OFF as a constraint for this pass,
  there is no forced bit-packing contortion — but I would still recommend
  KEEPING both structs small by habit (cheap on a bounded scheduler/command
  queue) rather than using the freedom to bloat them; that is a style
  recommendation, not a structural requirement.
- **`Performance`/`8100`** — with the migrator requirement lifted, the
  cleanest move is a NEW, separate binary table for `SceneChain` data
  (mirrors §21's own multi-table vision, `docs/DESIGN.md:613`: "styles,
  patterns, programs, device profiles, performances, songs" as separate
  sections) rather than growing `Performance` itself or forcing a
  `format_version` bump with migration machinery `Performance`'s own header
  discipline (`performance.hpp:143-155`) would otherwise demand. A `SceneChain`
  record referencing `Performance` slot INDICES (not embedding `Performance`
  copies) keeps the two tables decoupled and avoids duplicating the 576-byte
  snapshot per chain step.
- **Time signature (Fork D)** — if the owner locks a real per-scene
  time-signature, this is the one item in this whole scope that is NOT
  contained to `8100`: it is a cross-cutting change to `common/time.hpp`'s
  `kBeatsPerBar`/`kTicksPerBar` and every downstream consumer (§1, §3). This
  should be scoped and reviewed as its OWN item, not folded silently into
  `8100`'s "small" delta.

---

## 5. Decision forks for the owner

**Fork A — 6000 payload representation (settle before any 6100 code).**
Raw absolute MIDI note (simple, fast to build, breaks D24's NTT discipline,
makes 6500 re-harmonize ad hoc/heuristic) vs. chord-tone-relative functional
payload (matches every sibling in the core, makes 6500 close to free via the
existing `resolve()`/`ChordEngine::m_followed` machinery, costs a few more
bytes/event and a capture-time "reverse resolve" step). **Recommendation:
functional/chord-relative** — the architecture around it (D24, `ChordSequence`'s
own D28 precedent, `ChordEngine`'s shared followed-context) is built
end-to-end for this discipline; storing raw pitch would be the one
foreign body in an otherwise-consistent design. NEEDS-DECISION (musical/UX
tradeoff — a functional loop reharmonizes with the song, a raw-pitch loop
stays literal; both are legitimate LOOPER behaviors in the wild, so this is
a product-identity call, not a purely technical one).

**Fork B — does `8100`'s SceneChain get wired through `ClipMatrix`, or is it
its own transport?** `Performance`'s header already distinguishes `8100`
from the GUI's `ClipMatrix`-driven "Scene" (clip-column,
`performance.hpp:14-15`). Recommendation: own transport, mirroring
`ChordSequencer`'s `play()`/`on_tick()` shape — song-mode advances at
scene/bar boundaries independent of any grid-launch gesture, and forcing it
through `ClipMatrix::ContentKind` would resurrect the very "Scene" ambiguity
`Performance`'s own comment was written to kill. SHIPPABLE either way,
structurally; this is a genuine design-identity fork, not a technical
blocker.

**Fork C — 6300's always-on ring: truly always-on, or armed?**
Recommendation: gate behind an explicit "retroactive capture armed"
per-track/global flag, checked once per scheduled note (one branch) —
cheaper than a permanent unconditional tee on every tick for tracks the
player never intends to grab, and avoids growing the realtime kernel's
unconditional workload for a feature that may often be off. NEEDS-DECISION
(a UX call: does the player expect "always instantly available" — no arm
step — as the whole point of "retroactive"? If yes, always-on is the correct
answer and the flag is wrong). SHIPPABLE either way.

**Fork D — 8100's time-signature: build the prerequisite engine, or descope
it from v1 (like `Performance` already descoped several `8100`-adjacent
things)?** Recommendation: **descope for v1**, exactly matching the
precedent `Performance` itself already set (`performance.hpp`'s own header:
narrower-than-the-full-sketch scoping is an established, Corelli-reviewed
pattern in this codebase, per `docs/DESIGN.md:915-919`). Ship `SceneChain`
with per-scene `tempo_x100` (already exists, reusable) and fixed global
time-signature; open a SEPARATE, explicitly-scoped item for a real
variable-time-signature engine if/when a style genuinely needs it.
NEEDS-DECISION (owner call on whether `8100` v1 without time-sig is
acceptable against the DESIGN.md wording, or whether the wording itself
should be edited to match).

**Fork E — undo depth for 6100.** One generation (double-buffer, 2x a loop
slot's footprint) vs. none (replace is destructive, no undo — cheaper, but
"undo" is explicitly named as SHIPPABLE in DESIGN.md's own `6100` line)
vs. a bounded multi-level journal (most flexible, most RAM). Recommendation:
one generation — matches what "undo" colloquially means for a live looper
(undo the last overdub), avoids open-ended journal sizing, and keeps the
budget math in §4 tractable. NEEDS-DECISION (RAM budget call once Fork A's
payload size is locked).

**Fork F — loop slot cardinality.** DESIGN.md's own `8×3072` phrasing implies
8 loop slots, which matches NEITHER `kRoleCount` (10) NOR any existing pool
constant in `config.hpp`. Recommendation: decide explicitly whether loop
slots are per-`TrackRole` (10, one loop per role, simplest mental model,
matches how `Arranger::m_chain`/`m_role_arp` are already indexed) or a free
pool (a `ClipMatrix`-style grid of N independent loop slots, more flexible,
more bookkeeping). NEEDS-DECISION — this number drives the final budget
`static_assert` in §4 and should not be inherited silently from a DESIGN.md
estimate that predates this analysis.

---

## What I flagged / dependency stance

No new dependency is proposed anywhere in this scope — every primitive above
(`LoopBuffer`, `SceneChain`) is a bounded, dependency-free, header-only class
in the same style as `ChordSequence`/`ClipMatrix`, using only
`StaticVector`/`Span`/existing common types. If a future implementation pass
finds it needs anything beyond that (e.g. a richer container), that is a
NEW flag for the owner at that time, not assumed here.

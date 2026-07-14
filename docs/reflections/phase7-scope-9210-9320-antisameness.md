# Phase 7 scope: closing 9210 (Motif) and 9320 (Restyle)

Ottorino, DELTA analysis (read-only). Both nodes already have real, tested,
committed code — Phase 5 Items A/B (`docs/phase5-plan.md`,
`docs/phase5-design-reviews.md`) shipped design reviews AND an implementation
that follows them closely. This is not a greenfield scope; it is a measurement
of what exists, what is genuinely missing, and the forks the owner must still
close. **Owner directive received mid-scope: ABI stability is NOT a
constraint for this design — proposals below spend that freedom freely where
useful. The hard constraints that remain are platform/correctness: 9210 must
run freestanding/no-heap/deterministic on the STM32 M7, and existing golden
regressions must stay meaningful.**

---

## 1. As-built inventory

### 9210 Motif (`components/arrangrr/include/arrangrr/arranger/motif.hpp`, 369 lines)

**Fully implemented, not a stub.** Three transforms
(`transpose_diatonic`/`retrograde`/`displace`, lines 236–275), a constrained
generator (`motif::generate`, lines 165–226: bounded-leap contour, cadence
to root/fifth, idiom-onset-mask-respecting placement), an anti-triviality
retry guard (`apply_transform`, lines 327–348, bounded `kMaxRetries=4`), and
the call-and-response repeat policy (`apply_repeat`, lines 358–366: even
repeat = statement verbatim, odd repeat = transformed answer). All of it is
`constexpr`, seeded via the shared `arrangrr::seeded_hash` (D16), no heap, no
PRNG object.

**Wired live into `Arranger::on_tick`**, not an orphan library —
`components/arrangrr/include/arrangrr/arranger/arranger.hpp:509–531`: inside
the per-role/per-step gather loop, when `pattern.motif != nullptr` the seed
motif is produced (`motif::from_span` if `pattern.events` is authored,
`motif::generate` if empty) and `motif::apply_repeat` is applied before the
result flows unchanged through `gesture::expand`/`resolve()`/`voice()`/
`groove::apply()`. A single `std::uint32_t m_motif_repeat` member
(`arranger.hpp:815`) tracks the repeat count; it is reset on `load_style()`
(line 88) and `on_transport_start()` (line 363), and incremented **only**
when a plain variation section loops back onto itself
(`arranger.hpp:440–452`) — moving from `VarA`→`VarB` does **not** advance it.
This is a real scope characteristic, not a bug: call-and-response variety is
only audible on a variation that repeats itself, not across a progression
through different variations.

**Tested at three levels**: `components/arrangrr/tests/test_motif.cpp` (487
lines) — pure-function unit tests, property tests over 100–200 seeds for the
leap bound/cadence/anti-triviality guard, and two end-to-end goldens through
the real `Arranger` fire loop (`test_motif_end_to_end_golden`,
`test_motif_authored_seed_through_arranger`, both Option-1-authored-seed and
Option-2-generated-seed paths). It is also cross-build-proven:
`tests/arm-smoke/link_gate.cpp:111–160` (`motif_link_gate()`) constructs a
motif-driven style and asserts lead notes actually fire through `Arranger`
alone on the `arm-none-eabi` freestanding build.

**The one thing NOT built: zero of the 16 built-in styles use it.**
`grep -rn "MotifSpec\|\.motif =" components/arrangrr/include/arrangrr/arranger/styles/*.hpp`
returns **zero hits** across all 16 files. Every `StylePattern.motif` pointer
in the shipping styles is the default `nullptr`. The engine is a real,
tested, arm-proven library that today produces **no audible difference in
any built-in style** — its only two "customers" are `test_motif.cpp`'s
synthetic fixtures and `link_gate.cpp`'s synthetic fixture. This is the
single most important finding for "definition of done" below.

### 9320 Restyle (`components/arrangrr/include/arrangrr/restyle/restyle_stage.hpp`, 361 lines)

**Fully implemented and live end-to-end, not an orphan.** The one genuinely
new piece — `restyle::classify()` (lines 99–116), an inverse-NTT classifier
(absolute pitch → chord-tone index or scale degree against the live
`Key`/`ChordState`) — plus `snap_to_grid` (16th-grid rounding, lines 124–128)
and register-anchor helpers (`role_anchor`/`nearest_octave_to`, lines
136–169). `RestyleStage` (lines 176–359) is a real `Pipeline`-shaped stage:
inert until `load_style()` succeeds, tracks pending note-on/off pairs in a
fixed `Pending m_pending[128]`, classifies each input note, anchor-snaps +
re-voices chord tones through its **own** `VoicingState` (never sharing the
Arranger's), rhythmically snaps+grooves every note (chord tone or not)
through the target style's own `GrooveParams`, and schedules through the
shared `OutScheduler`.

**Wired into the live pipeline, not just a header.**
`components/orchestrator/include/orchestrator/accompany.hpp:42–53` grows
`AccompanyPipeline` from 3 to 4 stages —
`Pipeline<MidiSourceStage<N>, ChorddetStage<kMaxPorts>, RestyleStage<kMaxPorts>, Engine>`
— inserted between `ChorddetStage` and `Engine`, inert by default (same
"byte-identical when unused" convention `MidiSourceStage` established).
`components/hostrt/shell_music_commands.cpp:114–127` implements a live L1
verb, `restyle <style>`, that loads the target style into the stage AND
flips `MidiSourceStage::set_thru_enabled(false)` (the double-note guard —
`midisrc/midi_source_stage.hpp:117–159`, `m_thru_enabled` defaults `true`,
so nothing changes for pipelines that never call `restyle`).

**Proven with a real end-to-end golden**, not just unit tests:
`tests/golden/accompany_restyle.acmd` + `.golden` — `key C major` → `style
load basic` → `restyle blues` → `midi-source load tiny.mid` → `advance
3000`. `components/arrangrr/tests/test_restyle.cpp` (245 lines) additionally
asserts the core correctness property directly: re-classifying the OUTPUT
note against the same chord/key yields the SAME tone-kind/index as the INPUT
note (`test_restyle_stage_preserves_harmony_of_every_note`, lines 194–228) —
harmony is preserved by assertion, not just by golden byte-stability.
Cross-build-proven the same way as Motif: `link_gate.cpp:207ff`
(`restyle_link_gate()`).

**Two documented, deliberate first-slice gaps, both still open:**
1. **Channel-blind.** The golden's own comment
   (`accompany_restyle.acmd:20–26`) states it plainly: the shared fixture's
   drum hit (channel 9, note 36) shares pitch class 0 with the chord root
   and gets **restyled exactly like a melodic chord tone** — there is no
   channel filter distinguishing "the melody channel" from anything else
   arriving on the same port. Documented as "not a bug… a future slice may
   add channel-aware filtering," but it is a real defect against any
   multi-track SMF import today.
2. **Reharmonization is out of scope by design**, not by omission — a
   non-chord-tone note passes through at its ORIGINAL pitch (only
   requantized), for the reasons the design review states at length (no
   reverse-NTT precedent to build a reharmonizer from without compounding
   two unproven pieces; a wrong reharmonized note is "the single worst
   failure class the entire NTT design exists to make impossible").

### A doc-hygiene note (small, worth one line to the owner)

Both headers cite `docs/design/motif-engine-{scope,placement}.md` and
`docs/design/restyle-{musical-scope,placement}.md` as the source design
docs. **Those files do not exist in the tree** (`find docs -iname
"*motif*" -o -iname "*restyle*"` returns nothing under `docs/design/`). The
actual content lives in `docs/phase5-design-reviews.md` (§"Restyle (#1, Item
A, node 9320)" and §"Generative motif engine (#7, Item B, node 9210)") —
apparently folded there instead of split into separate files, with the
in-code comments never updated to point at the real location. Also,
`docs/DESIGN.md` lines 958–976 still mark both `9210` and `9320` as
`○ planned`/`○ HOST-ONLY` — stale against the tree exactly the way a prior
Vasari pass already reconciled other roadmap drift; this scope doc is a
second instance of the same staleness, not a new problem.

---

## 2. The anti-sameness thesis: what remains after 9100

`docs/phase5-design-reviews.md`'s own re-measurement of the corpus (§Restyle,
"The corpus is more differentiated than the stale premise assumes") is the
current ground truth, superseding the older blanket "all 16 styles are the
same skeleton" framing:

- Drums/perc and FEEL (9100, done) are genuinely idiomatic today: reggae's
  real one-drop + off-beat skank, samba's real surdo/tamborim/agogo pattern,
  blues's genuine blue-note harp line (`kInterval`) over a boogie
  root-fifth-seventh bass, with real per-style `GrooveParams` (swing/shuffle/
  blues carry an actual engine-driven swung feel).
- What is **still** flat, precisely: **bass FUNCTION** (root/fifth/seventh
  only across the whole corpus — zero walking bass, zero chromatic
  approach) and **melodic variation** (no generator; where a melodic line
  exists — 35 `kScaleDegree` + 14 `kInterval` events total across 16 files —
  it is one fixed, hand-typed four-note phrase, never a generated one).

**What 9210 attacks, and does not yet:** the melodic/bass-variation gap
directly — a generator existing is exactly what "no generator" describes as
missing. But since zero built-in styles reference `MotifSpec` (§1 above),
9210 attacks this gap **in principle, not yet in product**: the corpus today
sounds exactly as flat on this axis as it did before 9210 was built. The
concrete, already-measured target the design review names is `blues.hpp`'s
`kBoogieBass` (6 events) referenced byte-identical across 8 separate
`StylePattern` tables (`kIn2P`/`kAP`/`kBP`/`kFAP`/`kFBP`/`kFCP`/`kFDP`/`kDP`)
— the bass literally never varies across Intro/VarA–D/every Fill, while the
same file hand-authors 4 genuinely different comping tables. That is the
single sharpest, already-quantified example of exactly the redundancy 9210
exists to replace with a seed + transform.

**What 9320 attacks, and does today:** a different axis entirely — not "is
a built-in style varied enough" but "does an imported, arbitrary melody
sound like it belongs in a target genre." This is orthogonal to the bass/
melody-generation gap above: Restyle transforms one existing input part's
rhythm and register into a target idiom; it does not derive a genre band
underneath it (that is Accompany, 9310, done) and it does not generate new
melodic content (that is 9210). It is shipped and functionally proven today
(§1), gated only by the two documented first-slice limits (channel-blindness,
no-reharmonization-by-design).

---

## 3. Definition of "done" per node

### 9210 — minimal musically-meaningful deliverable

The generator/transform/repeat machinery, the seeding discipline, and the
device-cross-build proof are **already done** to a genuinely defensible
standard (tested at unit, property, end-to-end-golden, and arm-smoke
levels). What is **not** done, and is the actual remaining work to call
9210 "shipped" rather than "built": **at least one built-in style must
reference `MotifSpec`** — closing the exact `blues.hpp` `kBoogieBass`×8
redundancy the design review already measured and named as the target. This
is Option 1 from the design review ("author-seeded": one hand-written motif
per role, B/C/D variations derived via transform), already the recommended
first slice, already fully supported by the existing `MotifSpec`/
`Arranger::on_tick` wiring — no new engine code needed, only style-data
authorship + a deliberate, reviewed golden change (since it changes
`blues`'s emitted notes). Until this lands, 9210 is a complete, correct,
on-device-provable **library** with zero shipped musical effect.

Musically-nice-but-infeasible-on-device, for completeness: none identified.
Every generative technique in motif.hpp (constrained walk, bounded retries,
cadence snap) is integer arithmetic already proven to cross-build; nothing
here needed a heap, float, or ML step. The INSTRUCTIVE-BUT-INFEASIBLE line
for this node lies entirely in `9220` (statistically-trained Markov/n-gram),
a separate roadmap item by design, not something 9210 should absorb.

### 9320 — minimal musically-meaningful deliverable

**Already done to its documented first-slice scope**: rhythm requantization
+ register/voicing transfer for chord tones, harmony preserved by
assertion, live via a real CLI verb, proven with a real golden. Calling it
genuinely "done" (not just "first-slice done") requires closing the two
gaps named in §1:
1. A channel filter — "which channel(s) count as the melody to restyle,"
   distinct from any other content sharing the same port (e.g. drum hits).
   Host-only, no dependency, small (a channel mask parameter on
   `RestyleStage`/the `restyle` verb).
2. Nothing else is required for "done" — reharmonization is a deliberate,
   well-argued scope EXCLUSION, not a gap; it should stay out of this
   closure, per the design review's own reasoning (§1).

Musically-nice-but-infeasible-today: a live-toggleable restyle target
(currently load-time-only, host L1 verb) is fine as-is; a full melodic
reharmonizer needs a validated reverse-NTT-plus-harmony-rules pass this
program has not scoped and should not absorb here.

---

## 4. On-device cost for 9210 (host + STM32 M7, no heap)

Measured, not estimated, where the tree lets me measure directly:

- **`sizeof(Motif)` = 162 bytes** (compiled and printed against the real
  header: `kMaxMotifLen(16) × sizeof(StyleEvent)(10, pinned by
  `static_assert`) + 1-byte count`, plus alignment). This is a **stack
  local**, not persistent RAM — `Arranger::on_tick` declares
  `Motif generated_motif;` fresh inside the per-pattern loop
  (`arranger.hpp:519`), exactly like the existing `NoteReq
  group[kMaxVoiceNotes]` it sits beside. No heap, no static pool.
- **`sizeof(MotifSpec)` = 16 bytes**, one `const MotifSpec*` per
  `StylePattern` (a pointer, 4 or 8 bytes depending on target) — flash-
  resident, read-only, zero RAM cost per D32/D33.
- **Persistent RAM cost of the whole feature: one `std::uint32_t
  m_motif_repeat`** — 4 bytes, a single counter shared by every
  motif-driven pattern in the current section (not per-role, not
  per-pattern — see §1's repeat-counter note). Smaller than
  `VoicingState` already carries per role.
- **Determinism/seeding**: pure `constexpr` function of `(seed, position)`
  via the shared `arrangrr::seeded_hash` — no PRNG object, no carried
  state beyond the one bounded counter above. Same seed always reproduces
  the same motif (asserted directly, `test_motif_generate_deterministic`,
  and over 100–200 seeds in the property tests) — D16-clean.
- **Tick budget — a real but minor inefficiency worth flagging.** The
  motif-generation branch sits inside `if (rel % kTicksPerStep == 0)`
  (`arranger.hpp:490`), i.e. it re-runs `motif::generate` (an O(length≤16)
  loop, each iteration one `seeded_hash` call, ~5 integer ops) **and**
  `motif::apply_repeat`/`apply_transform` **once per 16th-grid STEP**, not
  once per bar/repeat — 16× more often than the result actually changes
  within one repeat. On an M7 this is unambiguously negligible (order of
  a few hundred integer ops, worst case, 16 times across a ~2-second bar
  at a typical tempo) — not a budget risk — but it is genuine redundant
  work a future pass could cache once per repeat instead of recomputing
  per step, if a much denser motif-driven arrangement (many roles, many
  patterns) ever made the constant factor matter.
- **What is NOT verified: execution on real M7 silicon.**
  `tests/arm-smoke/CMakeLists.txt` builds `firmware_stub` (a bare `.elf`,
  `SUFFIX ".elf"`) with **no `add_test`** — the cross-build/link gate
  proves the freestanding toolchain accepts the code (no accidental heap/
  RTTI/exceptions/libc dependency), and `motif_link_gate()`'s logic is
  the SAME logic `test_motif.cpp` already runs and asserts correct on
  host — but nothing in CI actually EXECUTES `firmware_stub` on hardware
  or in an emulator. `main.cpp`'s own header comment says this plainly:
  "Real board bring-up is M13," and `docs/phase5-plan.md` lists "#3 STM32
  hardware bring-up (`12100`)" as an explicitly **deferred** platform bet,
  separate from this program. **Honest label: cross-build-clean and
  logically host-verified, not silicon-verified** — that gap is a known,
  separately-tracked, deferred item, not something 9210 introduced or
  should be blocked on closing itself.

**Feasibility label for 9210 as a whole: SHIPPABLE** (dual-target,
no-heap, deterministic, cross-build-proven) for everything that exists
today; the missing piece (§3, wiring one built-in style) is equally
SHIPPABLE — it is pure style-data authorship, not new engine code.

---

## 5. Decision forks for the owner

For each fork: options, my recommendation, why. ABI freedom (owner
directive) is spent where it genuinely helps; flagged where a new
dependency would be needed (none is, anywhere below).

### Fork A — Does closing 9210 require wiring a built-in style, or is the engine itself the deliverable?
- **Option 1: ship the engine as-is, call 9210 done.** Defensible if the
  goal was purely "prove the mechanism is buildable and correct." Leaves
  zero audible anti-sameness payoff shipped.
- **Option 2 (recommended): wire `blues.hpp`'s `kBoogieBass`×8 into a
  `MotifSpec` (author-seeded, Option 1 from the design review) before
  calling 9210 closed.** Small, scoped, uses only existing code, and is
  the exact example the design review already measured and named. Ships
  a real, reviewed golden change and a real anti-sameness payoff for the
  first time. This is the one recommendation in this doc I'd call
  close to non-negotiable — the alternative is closing a roadmap item
  that changes nothing a listener can hear.

### Fork B — Whole-part generation vs. varying an existing lane (already resolved once; confirm it still holds)
- The Phase-5 review already ranked **Option 1 (author-seeded, transform
  varies B/C/D)** as the safest first slice, **Option 2 (constrained-random
  seed generation)** as the natural fast-follow, and **Option 3
  (offline-trained Markov/n-gram, `9220`)** as a distinct, separately
  dependency-flagged roadmap item. **Recommendation: reaffirm this
  ordering** — nothing measured in this pass argues for revisiting it, and
  revisiting it now would re-litigate a review that already reasoned
  through the tradeoffs (musical risk MEDIUM-HIGH for Option 2's "hard to
  make interesting" surface; `9220`'s corpus/provenance dependency is
  shared with the Yamaha-corpus-import item and should not be absorbed
  here).

### Fork C — Single shared `m_motif_repeat` counter vs. per-role/per-pattern counters
- **Current state**: one global counter for the whole `Arranger`, shared
  by every simultaneously motif-driven pattern (§1, §4). This keeps
  call-and-response ALIGNED across parts (bass and lead "answer" on the
  same repeat together, like a real arranger's phrasing) but forbids a
  bass motif cycling on an independent period from a lead motif.
- **Option 1 (recommended, keep as-is):** the shared counter is musically
  coherent (band-wide phrasing) and costs nothing extra. No evidence any
  built-in style needs independent per-role cycles.
- **Option 2:** widen to a per-role array (`std::uint32_t[kRoleCount]`,
  40 bytes) — cheap if the ABI-freedom-driven wiring below is spent
  anyway, but adds a real design question (does looping VarA vs VarB
  count as "the same cycle" per role independently?) with no concrete
  musical case demanding it yet. Flag for later if a specific style
  authors two independently-repeating motif lanes.

### Fork D — Live ABI exposure for Motif (owner has now lifted the ABI-stability constraint)
- **Option 1: none — stays load-time/compile-time only** (today's state:
  `MotifSpec` is baked into the compiled `Style` table, changed only by
  loading a different style). Zero new surface, zero risk.
- **Option 2 (recommended as the target shape, not urgent for closing
  9210 itself): add `Command::kMotif` + a `MotifField` selector**
  (`kEnabled`/`kTransformKind`/`kAmount`/`kSeed`), mirroring the existing
  `GrooveField`/`ArpField` idiom exactly — same `set_field` clamp pattern,
  same cost class. With ABI stability no longer a constraint, this is
  cheap to add NOW rather than later, and is explicitly the shape the
  eventual Generative Director (D37/`10000`) will need to pilot Motif
  live, the same way it will pilot groove/arp. **Recommendation: scope
  this as a fast-follow to Fork A, not a blocker on it** — Fork A ships
  the musical payoff; Fork D ships the controllability, a genuinely
  separable increment.
- No new dependency either way.

### Fork E — `kChordTone`-motif transpose semantics
- For `kScaleDegree`/`kInterval` motifs, "transpose" is unambiguous
  (`tone += amount` on a melodic pitch). For `kChordTone` motifs, `tone`
  is a chord-relative index (0=root, 1=third…), so the SAME operation
  would restack which functional tone plays — a different musical
  operation (an inversion/voicing choice), not a melodic transposition.
- **Recommendation: stay out of scope**, exactly as implemented today
  (`transpose_diatonic` passes `kChordTone` events through unmodified,
  `arranger/motif.hpp:230–246`). No measured demand for chord-tone
  restacking via this mechanism; if it is wanted later it deserves its
  own named transform, not an overload of "transpose."

### Fork F — Is Restyle a new host stage or an extension of AccompanyPipeline? (CLOSED, not open)
- **Already resolved by the shipped code**, both at once:
  `RestyleStage` is a genuinely new class/type, inserted as the third
  stage of the (grown, same-alias) `orchestrator::AccompanyPipeline`.
  There is no live fork here to decide — stating this explicitly so it
  is not re-litigated in a future pass.

### Fork G — Channel filtering for Restyle's input (real gap, needs a decision)
- **Option 1: leave channel-blind**, document it more prominently as a
  known limitation. Cheapest, but a real defect on any multi-track SMF
  a user actually imports (drum-channel content restyled as melody, per
  the golden's own comment).
- **Option 2 (recommended): add a simple channel allowlist** — a
  bitmask parameter on `RestyleStage`/the `restyle <style>` verb (or a
  sibling verb, `restyle-channel <n>`), host-only, no dependency, no ABI
  needed (another load-time L1-verb parameter, same shape as `restyle
  <style>` itself). Small, directly closes the one concretely observed
  first-slice defect.

### Fork H — Target role assignment (`kTargetRole = TrackRole::kLead`, hardcoded)
- **Option 1: leave hardcoded to `kLead`.** Works today; `kLead` already
  has a plausible register/idiom in every style that authors one.
- **Option 2 (recommended if the owner wants this closed alongside Fork
  G): make it a second optional CLI argument** (`restyle <style>
  [role]`, defaulting to `kLead`), reusing the existing `parse_role`
  helper `Shell` already has for other commands. Trivial, host-only, no
  ABI needed (same load-time-verb reasoning as `restyle <style>`
  itself) — unlocks the design review's own flagged alternative
  (`kPhrase`, "a genuinely free slot used by zero built-in styles").

### Fork I — Reharmonization (confirm it stays deferred)
- **Recommendation: leave deferred**, per the design review's own
  reasoning (§1, §2) — no reverse-NTT-plus-harmony-rules precedent
  exists yet, the corpus has almost no melodic-generation precedent to
  validate against, and a wrong reharmonized note is the specific failure
  class NTT (D24) exists to make impossible. Not part of "done" for this
  pass; flag only that if it is ever picked up, it should NOT be folded
  quietly into either 9210 or 9320 — it needs its own scoping pass
  against whichever of the two has matured further by then.

**No new dependency is proposed anywhere in this document.** Every fork
above is either already-existing-vocabulary reuse (`GrooveField`/
`ArpField`-shaped selectors, `parse_role`, a channel bitmask) or pure
style-data authorship.

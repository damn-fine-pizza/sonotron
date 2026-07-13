# Generative motif engine (`9210`) — musical transform scope

Status: **analysis + design scope, not code.** Author: Ottorino (style-and-
arrangement, dual axis). Read-only measurement of `components/arrangrr`,
`components/chorddet`. Written for Phase-5 Item B (`docs/design/
phase5-execution-plan.md`, item #7 / node `9210`), the procedural half of the
anti-sameness arc — Restyle (`9320`, item #1, `docs/design/
restyle-musical-scope.md`) RE-CLOTHES existing material; this item GENERATES
new melodic material from a seed. Per the plan: **"SHIPPABLE core, dual-
target"** — unlike Restyle (HOST-only) and unlike a corpus importer or synth,
this is the one generative Phase-5 item the plan itself flags as living on
the `arm-none-eabi` realtime path, not just the host tool tier. Nothing
proposed here needs a new dependency.

---

## 0 — What was measured (evidence, not memory)

### 0.1 The D40 resolution pipeline — the reusable spine, and where a motif
### stage would sit

`components/arrangrr/include/arrangrr/arranger/arranger.hpp:300-361`, the
per-step fire loop, is gather → `gesture::expand` (`gesture.hpp:47-116`) →
`resolve()` (`arranger.hpp:402-438`, the forward NTT kernel) →
`m_voicing.voice()` (`voicing.hpp:65-98`) → `groove::apply()`
(`groove.hpp:65-114`) → schedule. Every stage is freestanding, no-heap,
deterministic. A motif engine's natural seam is **upstream of `gesture::
expand`**, at the point where the loop currently reads `pattern.events`
(authored `Span<const StyleEvent>`, `arranger.hpp:317`): a motif+transform
stage would produce the SAME `StyleEvent` value type — `{step, tone, octave,
vel, gate, NoteSource src, ChordGesture gesture}`, pinned at 10 bytes
(`style_model.hpp:88-105`, `static_assert(sizeof(StyleEvent) == 10, ...)`) —
so it plugs into the existing pipeline as a *source* of specs, unchanged
downstream. This is the load-bearing fact for feasibility: **generation
output is representationally identical to authored content**, so `resolve()`,
`gesture::expand`, `VoicingState`, `groove::apply` need ZERO new code to
consume it. The only new code is what PRODUCES the `StyleEvent` sequence.

### 0.2 The seeded-PRNG discipline (D16, node `0100`) — reused three times
### already, independently

Confirmed the SAME deterministic 32-bit position-hash formula exists in
**three separate places**, each a verbatim reimplementation, not a shared
call:
- `groove.hpp:52-60` (`groove::hash`, feeds swing/humanize)
- `arp/arpeggiator.hpp:195-201` (`ArpeggiatorEngine::hash`, feeds
  `ArpDirection::kRandom`)
- `timeline/timeline.hpp:190-196` (`Timeline::hash`, feeds the D16
  probability-lock gate, `on_tick:180-183`)

All three: `h = seed*2654435761u + pos + 0x9E3779B9u; h ^= h>>15; h *=
2246822519u; h ^= h>>13;` — a splitmix-family integer hash, pure, `constexpr`,
zero state to carry (the "seed" IS the state). `test_arp_random_deterministic`
(`components/arrangrr/tests/test_arp.cpp:135-163`) is the existing test
PATTERN this doctrine is validated by: two independently-constructed engines
with the same seed produce byte-identical output over 8 steps, AND the
output is checked to actually vary (`CHECK(any_off_root)`) — reproducibility
and non-triviality asserted together, not just one or the other. **Flag: a
motif engine is the natural FOURTH consumer of this formula — it should reuse
one shared `hash()` (probably promoted to a small `common` header) rather
than adding a fourth independent copy; this is a real, cheap, evidence-based
cleanup opportunity worth doing as part of `9210`'s implementation, not
scope creep.**

### 0.3 `theory.hpp` primitives available for diatonic transforms

`components/chorddet/include/chorddet/theory.hpp`:
- `degree_to_semitones(Mode, int degree)` (`theory.hpp:105-117`) is already
  UNBOUNDED and wraps correctly for any signed degree via floor-division
  (tested: `degree_to_semitones(kMajor, -7) == -12`, `theory.hpp:337`) — this
  is exactly the primitive a "shift by N diatonic scale-steps" transpose
  needs, already proven, already `constexpr`.
- `shape_of(ChordQuality)` (`theory.hpp:132-158`) returns up to 4 chord-tone
  offsets; `Arranger::resolve()`'s `kChordTone` branch (`arranger.hpp:423-436`)
  already wraps a tone-index past `shape.count` an octave up
  (`ev.tone / shape.count`, `arranger.hpp:432`) — so an out-of-range
  chord-tone index from a transform is NOT an error, it is absorbed by
  existing, tested wrap logic. **Every transform output stays inside the
  NTT wrong-note-proof envelope (D24) by construction** — this is the single
  strongest feasibility argument for shipping this item on-device: there is
  no new "is this a valid note" logic to write or verify.

### 0.4 The corpus's own redundancy — the concrete manual-labor gap

Grepped `components/arrangrr/include/arrangrr/arranger/styles/blues.hpp`:
the SAME literal array `kBoogieBass` (`blues.hpp:52`, 6 events) is referenced
byte-identical in **8 separate `StylePattern` tables** — `kIn2P`, `kAP`,
`kBP`, `kFAP`, `kFBP`, `kFCP`, `kFDP`, `kDP` (`blues.hpp:57-93`) — i.e. the
bass NEVER varies across Intro/VarA/VarB/VarC/VarD/every Fill. By contrast
the SAME file hand-authors 4 genuinely DIFFERENT comping tables (`kAC`,
`kBC`, `kCC`, `kDC`) for Chord1 across the 4 variations. `styles/funk.hpp`
shows the same pattern at the drum layer: `kAD`/`kBD`/`kCD`/`kDD` are each
fully, independently hand-typed 16-step tables (`funk.hpp:63-141`) with no
shared derivation — a human author manually re-wrote a variant instead of
transforming a seed. **This is the precise gap `9210` targets**: a motif +
transform engine could derive B/C/D bass/lead variation from ONE authored
seed via transpose/retrograde/displacement, at flash cost of the seed alone
plus a few bytes of transform parameters, instead of N fully-duplicated
tables — a flash SAVINGS opportunity, not just a variety one.

### 0.5 Bounds already in the codebase (grounding the on-device cost claims)

`components/arrangrr/include/arrangrr/config.hpp`: `kMaxTracks = 16`
(line 14), `kMaxStepsPerTrack = 64` (line 15), `kSchedulerCapacity = 4096`
out-queue entries (line 13). `Arranger`'s own per-step voice group is capped
at `kMaxVoiceNotes = 16` (`arranger.hpp:371`). `VoicingState`'s per-role
memory is `int m_last[10][8]` + `int m_count[10]` (`voicing.hpp:117-118`) —
80 ints + 10 ints, a known-good precedent for "small fixed per-role memory,
no heap." These are the order-of-magnitude the motif engine's own state
should target.

### 0.6 No SMF writer exists (verified independently, not just cited)

`components/midisrc/include/midisrc/smf.hpp` (read in full) exposes only
`parse_smf(...)` — a reader. `grep -rn "class Smf\|write_smf\|SmfWriter"` over
`components/midisrc/include/midisrc/*.hpp` returned nothing. This
independently confirms the same gap `docs/design/restyle-musical-scope.md`
§4.5 already flagged for Restyle: **there is no committed SMF-write or
audio-render path in this tree** for either generative feature to be
ear-validated outside a live MIDI session.

---

## 1 — The three named transforms, defined against the existing model

The plan (`phase5-execution-plan.md` Item B) names diatonic transpose,
retrograde, displacement. Defined precisely against `StyleEvent`/NTT:

**A "motif" is a small, fixed-length `StyleEvent[kMaxMotifLen]` array** — the
existing 10-byte struct, nothing new. `kMaxMotifLen` should be one bar
(16 steps, matching the existing 16th-grid convention every style already
uses) or a half-bar (8) for a shorter cell; both are small enough to live on
the stack exactly like `arranger.hpp`'s own `NoteReq group[kMaxVoiceNotes]`.

**(a) Diatonic transpose — needs a source motif; clean ONLY for
`NoteSource::kScaleDegree` (and, differently, `kInterval`).** For a
`kScaleDegree` event, `tone += N` shifts the whole contour by N diatonic
scale-steps while the SAME wrap/floor-division logic `degree_to_semitones`
already implements (§0.3) keeps it correct at any N, including negative or
octave-crossing shifts. For `kInterval` events, adding a constant to `tone`
is a **chromatic** transpose (semitones from the chord root), a related but
musically distinct operation — worth naming separately in an implementation,
not conflating. **For `kChordTone` events, "transpose" does NOT mean the
same thing**: `tone` there is already a chord-relative index (0=root,
1=third, 2=fifth...); adding a constant reassigns which FUNCTIONAL chord tone
plays (root becomes third), which is closer to a "chord-tone step shift" or
a restacking than a melodic transposition in the classical sense. A first
slice should scope diatonic transpose to `kScaleDegree`/`kInterval` motifs
and treat `kChordTone` motifs as a distinct (arguably lower-priority) case,
rather than applying one transform uniformly across all three `NoteSource`
kinds and calling the `kChordTone` result "transposed" when it is musically
something else.

**(b) Retrograde — needs a source motif; purely a temporal reordering, fully
expressible today.** `new_events[i] = old_events[N-1-i]` with each event's
`step` remapped to `(motif_length - 1 - old_step)` (or the nearest occupied
slot after remapping, if gate lengths must not overlap once reversed) — pure
integer arithmetic over the existing `step`/`gate` fields, no new resolve()
path. Retrograde is well-defined and equally cheap for ALL three
`NoteSource` kinds (it never touches `tone`), including drum/perc `kFixed`
patterns — a genuinely under-explored option, since retrograding a DRUM
pattern (reversing its onset order within the bar) is a real, cheap
technique with no wrong-note risk at all (kFixed never resolves through NTT).

**(c) Displacement — needs a source motif; a rhythmic phase shift, fully
expressible today.** `step = (step + shift) mod kStepsPerBar` (or per-motif
length) — again pure arithmetic on the existing field, applicable to every
`NoteSource`/`RolePolicy` combination including drums. This is mechanically
the SAME kind of operation that already differentiates onset patterns across
the corpus's authored variations (e.g. `blues.hpp`'s hand-typed `kAD`/`kBD`
tables shift kick/snare onsets by a few steps between variations) — a
displacement transform automates a pattern a human author is ALREADY doing
by hand today, evidenced directly by the corpus.

**What needs a source motif vs. generate-from-scratch.** All three named
transforms are, by definition, operations ON an existing ordered
`StyleEvent` sequence — none of them can originate content. **A fourth,
un-named-by-the-plan piece is therefore required before any transform can
run: a seed-motif generator** (the actual "generate new melodic material"
half of the item's own description). This seed generator is the genuinely
open design surface — see §2 for what constrains it musically, and §3 for
concrete seed-generation options with cost/feasibility labels.

---

## 2 — Keeping generated motifs musical (the plan's own named risk)

The plan states the risk precisely: "easy to make CORRECT (no wrong notes
via NTT) and hard to make INTERESTING." §0.3 shows the CORRECT half is
already solved structurally (NTT wrap absorbs any tone-index). The
INTERESTING half needs concrete, not hand-wavy, constraints:

1. **Contour bounding, not free random walk.** An unconstrained seeded
   random walk over `tone` (even NTT-safe) produces the exact "technically
   valid, musically inert" failure the plan names — e.g. wide, directionless
   leaps read as noise, not melody. Constrain consecutive-event `tone` deltas
   to a small bounded range (stepwise motion dominant, an occasional larger
   leap for interest, mirroring standard melodic-writing practice), computed
   as a simple `|delta| <= kMaxLeap` gate at generation time over the SAME
   seeded hash already in use elsewhere (§0.2) — no floating point, no new
   PRNG family needed.
2. **Phrase shape, not a flat sequence.** The corpus's own section vocabulary
   (`SectionType::kVarA..kVarD`, `style_model.hpp:20-34`) is ALREADY a
   statement/variation/return form at the section level; a motif generator
   should exploit the SAME logic one level down: generate ONE seed motif as
   the "statement," then apply retrograde or displacement to produce an
   "answering" phrase, rather than four independently-random motifs with no
   relationship — call-and-response is a real, cheap, well-understood
   generative-music technique (Schoenberg's "developing variation" is the
   classical-theory name for exactly this: vary a motif's RHYTHM or
   CONTOUR while keeping its identity recognizable, rather than inventing
   fresh unrelated material every time).
3. **Rhythmic onsets anchored to the STYLE's own idiomatic grid, not any of
   16 uniform slots.** `docs/design/restyle-musical-scope.md` §0.1 already
   measured that the corpus's drum/perc layer IS genuinely idiomatic (a real
   reggae one-drop, a real samba surdo/tamborim clave) — a motif generator
   for a comping/lead role should draw candidate onset steps from a
   per-style-characteristic SUBSET of the 16th-grid (e.g. the same steps the
   style's OWN authored drum pattern accents) rather than choosing uniformly
   at random across all 16 positions, so a generated bassline in reggae
   still implies the one-drop's accent structure even though its PITCHES are
   generated. This constraint needs no new data: the style's existing
   `kFixed` drum `StyleEvent.step` values are already in flash and directly
   readable as an "allowed onset mask" for a comping-role generator in the
   SAME style.
4. **Cadential resolution.** A generated phrase that does not resolve reads
   as unfinished; a cheap, testable rule is: the LAST strong-beat tone-index
   of a generated motif should resolve to a stable scale-degree/chord-tone
   (root or fifth) more often than chance — assertable exactly like Restyle's
   own harmony-preservation check (§0.1 of `restyle-musical-scope.md`): "does
   the last classified tone of this generated phrase equal root or fifth,"
   a property test over many seeds, not a listening-only claim.
5. **Non-triviality / anti-repetition across variations.** Because retrograde
   and displacement are combinatorially limited transforms of ONE seed, a
   trivial parameter choice (e.g. `shift=0`, or a shift equal to the motif's
   own length) reproduces the original — an accidental "generated" variation
   that is byte-identical to its seed. A cheap, deterministic guard: reject
   (re-hash to another seed/shift) any transform whose OUTPUT step-onset set
   is identical to the input's, bounding the search to a handful of tries
   (same cost class as `groove::hash`'s single evaluation, no iteration risk
   on a realtime path since the check runs once at generation time, not per
   tick).

None of the above needs statistics, ML, or floating point — every mechanism
is integer arithmetic and comparisons over the existing `StyleEvent`/`theory`
vocabulary, decidable at the SAME cost class as `groove::apply`.

---

## 3 — Seed-motif generation: the genuinely open design fork

Three real alternatives for producing the FIRST motif a transform then
varies, each with a distinct feasibility/cost profile — this is the fork the
plan does not resolve and Ottorino should not bluff a single answer for:

**Option 1 — Author-seeded (rule: one hand-written motif per style/role,
transforms do the rest).** A style still authors ONE motif per comping/lead
role (as today), but B/C/D variations are DERIVED via transpose/retrograde/
displacement instead of hand-typed separately. **SHIPPABLE, dual-target,
no-heap.** Musical payoff: directly fixes the §0.4 measured redundancy
(`kBoogieBass` reused 8x unchanged) with almost no new runtime code — the
"generation" is really "guided variation of curated material," the safest
and most easily-validated option. Cost: flash SAVINGS (fewer full tables);
runtime cost is transform application, O(motif length) ~16-64 events, once
per section entry or once per loop, negligible against the tick budget.
Engineering risk: LOW. Musical risk: LOW-MEDIUM — this option does not
generate anything NEW, it recombines authored material, so its "interesting"
ceiling is bounded by the seed's own quality (garbage in, garbage out, but
also good-in, good-out — no risk of inventing bad melody from nothing).

**Option 2 — Constrained-random seed (rule/grammar: generate the FIRST
motif itself from a seeded walk under §2's contour/onset/cadence
constraints).** **SHIPPABLE, dual-target, no-heap** — same cost class as
Option 1's transforms (the generation step is itself a bounded loop over
`kMaxMotifLen` positions, each a single hash evaluation + a few integer
comparisons). Musical payoff: HIGHER ceiling than Option 1 — can produce
motifs no author wrote, still inside every §2 guardrail. Engineering risk:
LOW (no new data structures, pure functions). **Musical risk: MEDIUM-HIGH —
this is exactly the "hard to make interesting" surface the plan flags**; a
constrained random walk can still sound generic/characterless even with
contour and cadence rules, because "not wrong" and "not aimless" are
necessary but not sufficient for "sounds like this genre." Mitigating this
needs real ear-testing against genre reference (see §4), not just passing
the property tests.

**Option 3 — Statistically-trained seed (Markov chain / n-gram over scale
degrees, trained OFFLINE on a corpus, baked to a `constexpr` transition
table).** This is explicitly named in the roadmap as a SEPARATE sibling item:
`9220` "Offline-trained Markov/grammar on scale degrees, baked constexpr —
○ runtime SHIPPABLE / training HOST-ONLY" (confirmed,
`docs/strategy/roadmap-numbered.md:284`). Musical payoff: potentially the
most idiomatic-sounding seed generator of the three, IF trained on a genre-
labeled corpus (the Yamaha 1010-style corpus, `docs/backlog/
yamaha-style-corpus-and-rules.md`, is the obvious training data, still
subject to Item C/`9400`'s OWN unresolved provenance/licensing flag — not
re-litigated here, only noted as a shared dependency). Cost/dependency: the
RUNTIME lookup (a small baked transition table + a seeded roll) is
SHIPPABLE and dual-target — no different in kind from a baked constexpr
`Style` table already in flash (D32/D33). The TRAINING step (computing
transition probabilities from a corpus) is HOST-ONLY tooling, comparable to
`arrstyle-converter`'s existing offline pipeline — no NEW dependency is
implied by training itself (a frequency-count Markov model needs no ML
library), but this is explicitly `9220`, not `9210` — **flagging that a `9210`
scope should NOT quietly absorb `9220`'s scope**, exactly the discipline
`restyle-musical-scope.md` §2 already applied to keep Restyle from
duplicating THIS item's scope.

**Ranking, honestly:** Option 1 is the safest, cheapest, most immediately
shippable slice and directly fixes a measured, cited redundancy — recommend
it as the actual FIRST slice. Option 2 is the natural second step once
Option 1's transform machinery exists (the transforms are shared code either
way). Option 3 is a related but DISTINCT roadmap item (`9220`) with its own
corpus/provenance dependency and should stay out of `9210`'s first-slice
scope, exactly as the plan's own numbering already implies.

---

## 4 — Testing musical acceptability beyond golden byte-equality

Mirrors the exact gap Restyle hit (`restyle-musical-scope.md` §4, and §0.6
above, independently reverified): a byte-identical golden proves "same seed
⇒ same output" (D16), not "the first output was musically good." Concrete,
automatable checks beyond ctest-green:

1. **Property tests over MANY seeds, not one golden seed.** `§2.4`'s
   cadence-resolution rule and `§2.1`'s leap-bound rule are both directly
   assertable as loop-over-N-seeds property tests (`test_arp_random_
   deterministic`'s own pattern — `test_arp.cpp:135-163` — is the existing
   precedent: assert reproducibility AND assert the output is not
   degenerate, e.g. `CHECK(any_off_root)`). A motif-engine test suite should
   assert BOTH "same seed reproduces" and "over 100+ seeds, no generated
   motif violates the leap bound / fails to cadence," which a single golden
   file cannot catch.
2. **Anti-repetition assertion (§2.5).** Directly testable: generate B/C/D
   from A under the SAME seed policy and assert no two variations share an
   identical onset-step set — a concrete, cheap regression that catches the
   "trivial-parameter" degenerate case before it ships.
3. **A real listening path is still missing.** Re-confirmed independently
   (§0.6): no SMF writer, no audio render in-tree. Whoever implements `9210`
   will hit the SAME infrastructure gap Restyle already flagged — recommend
   this be scoped ONCE (a small host-only SMF writer, or routing generated
   output through `hostrt::Shell`'s existing live MIDI-out path) rather than
   re-discovered per generative item. Not `9210`'s job to build, but its
   implementation should not proceed to "sounds right" sign-off without one.
4. **Genre-reference check, by ear, against known convention — NOT invented
   by the property tests.** The property tests (leap bound, cadence,
   anti-repetition) can only prove "not obviously broken." Whether a
   generated bossa comping motif actually sounds like idiomatic bossa
   phrasing (not just "correct and non-repeating") is a musician's ear
   judgment against real genre reference recordings/transcriptions — the
   same category of check `per-style-feel-values.md`'s swing-ratio table
   already applies to the groove layer, extended to melodic phrasing. This
   is qualitative and NOT reducible to a unit test; flagging it honestly
   rather than pretending the property tests cover it.

---

## 5 — Relationship to Restyle's inverse classifier (`9320`) — genuinely
## mostly independent, evidence-checked

Read `components/arrangrr/include/arrangrr/restyle/restyle_stage.hpp` in
full (the ALREADY-IMPLEMENTED first slice of `9320`, not just its design
doc). The load-bearing fact: **Restyle's one genuinely new piece is an
INVERSE classifier** — `restyle::classify()` (`restyle_stage.hpp:99-116`),
absolute input pitch → NTT tone-index/scale-degree, the reverse of
`Arranger::resolve()`. **A motif engine's core direction is FORWARD** — it
authors or transforms `StyleEvent{tone, octave, NoteSource}` specs that flow
through the EXISTING, unchanged `resolve()` exactly like authored content
does today (§0.1). These are opposite-direction operations on the theory
vocabulary, not the same problem twice.

**What they DO share, concretely, evidenced by reading both:**
- `theory.hpp`'s primitives (`shape_of`, `degree_of`/`degree_to_semitones`) —
  Restyle uses `shape_of`+`degree_of` (classification direction);
  a motif engine would use `degree_to_semitones` (generation direction,
  §0.3) — same file, complementary halves, not overlapping code.
- The seeded position-hash discipline (§0.2) — Restyle's first slice does
  NOT currently use it (its rhythmic snap, `restyle_stage.hpp:124-128`
  `snap_to_grid`, is pure deterministic rounding, no hash), but a motif
  engine will, for the SAME reason `groove`/`arp`/`timeline` already do:
  reproducible pseudo-variety.
- `VoicingState`/`groove::apply` reuse — Restyle instantiates its OWN
  `VoicingState` (`restyle_stage.hpp:355`, "a distinct voice-leading lineage
  from the generative Arranger's own — they are different musical streams
  and must not share memory," per that file's own comment) rather than
  sharing the Arranger's. A motif engine, if it generates content that then
  flows through the EXISTING Arranger fire loop (§0.1's "same seam"
  argument), would use the Arranger's OWN `m_voicing`/`m_groove` — it does
  NOT need its own separate lineage the way Restyle does, because unlike
  Restyle (a parallel pipeline stage reading a DIFFERENT input source),
  a motif engine's output IS an Arranger pattern's content.
- **No shared new code is required between the two items.** They are
  independently implementable and independently testable; the only
  coordination point is the shared `hash()` cleanup flagged in §0.2, which
  benefits Restyle not at all (Restyle doesn't currently call it) but would
  benefit a THIRD/FOURTH caller consistency argument if `9210` adds a
  fourth copy instead of reusing one.

---

## Open forks / flags (owner's or the implementor's call, not Ottorino's)

1. **Seed-generation option (§3) — Option 1 vs Option 2 as the actual first
   slice.** Recommended: Option 1 first (transforms over an authored seed),
   Option 2 as a fast-follow once the transform machinery is proven. This is
   a product-scope call the owner or Nazzareno should confirm before
   implementation, not a technical constraint either way.
2. **`kChordTone`-motif transpose semantics (§1a).** Whether a first slice
   should even ATTEMPT "transpose" on `kChordTone` motifs (where it is
   musically a chord-tone restack, not a melodic transposition) or scope it
   OUT and keep diatonic transpose to `kScaleDegree`/`kInterval` motifs only
   — a naming/scope precision question, not an engineering blocker.
3. **The shared `hash()` cleanup (§0.2).** Three independent copies of the
   same formula exist today (`groove.hpp`, `arpeggiator.hpp`, `timeline.hpp`);
   `9210` would be a natural fourth. Flagging this as a small, low-risk,
   evidence-based refactor opportunity worth doing alongside `9210`'s
   implementation — not this document's call to schedule, but worth
   Corelli's placement review noting.
4. **The missing SMF-writer / audio-render path (§0.6, §4.3).** Same gap
   Restyle already flagged; still unresolved, now confirmed to block BOTH
   Phase-5 anti-sameness items' ear-validation equally. Worth scoping ONCE
   (Torquato or a small preceding chore) rather than re-flagging per item.
5. **`9220` boundary (§3, Option 3).** Confirmed as a distinct roadmap item
   with its own corpus/provenance dependency (shared with `9400`/Item C).
   `9210`'s first slice should not absorb it — flagged so an implementor
   does not accidentally widen scope mid-work.
6. **No new dependency proposed anywhere in this scope.** Every mechanism
   above (transform arithmetic, contour/onset/cadence constraints, seeded
   hash reuse) is host-and-device C++ over existing `theory::`/`groove::`/
   `StyleEvent` primitives. The one adjacent temptation (an ML-trained
   motif generator, as opposed to `9220`'s frequency-count Markov model) is
   explicitly OUT per `roadmap-numbered.md:505-506`'s existing "ML style-
   transfer — stays OUT" line — not revisited here, only reaffirmed for
   the same reason Restyle's scope reaffirmed it.

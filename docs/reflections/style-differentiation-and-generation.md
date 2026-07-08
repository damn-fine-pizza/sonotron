# Reflection — Why the 16 styles feel like "one song rearranged", and how to make them genuinely different

Status: analysis + design DIRECTION (not code). Read-only measurement of the
built-in corpus. Author: Ottorino (style-and-arrangement, dual axis: arrangement
musicology + computational/embedded).
Scope: diagnosis of style flatness + generative design OPTIONS. No product code.

All counts below were MEASURED on `components/arrangrr/include/arrangrr/arranger/styles/*.hpp`
(16 files) and the model in `components/arrangrr/include/arrangrr/arranger/style_model.hpp`,
`arranger/groove.hpp`, `timeline/timeline.hpp`. Grounded against DESIGN.md
(D24/D37/D39/D40/D41/D42/D44) and `docs/research/yamaha-style-corpus-and-rules.md`.

---

## Part 0 — What was measured (evidence)

Per-style aggregates (event = one `{.step=...}` record):

| style | events | melodic(scaleDeg+interval) | gestures | kLead patterns | drums signature (VarA kick / snare) |
|---|---:|---:|---:|---:|---|
| basic | 255 | 0 + 0 | 0 | 0 | — |
| ballad | 159 | 0 + 0 | 0 | 13 | — |
| blues | 196 | 0 + 5 | 0 | 19 | kick 0,8 / snare 4,12 |
| bossa | 231 | 0 + 0 | 1 | 6 | kick 0,8 / (side-stick clave) |
| country | 202 | 4 + 1 | 8 | 0 | kick 0,8 / snare 4,12 |
| disco | 294 | 4 + 0 | 1 | 8 | kick 0,4,8,12 (four-on-floor) |
| funk | 283 | 0 + 0 | 0 | 0 | kick 0,3,6,10 / snare 4,7,11,12 |
| house | 273 | 4 + 0 | 0 | 15 | kick 0,4,8,12 |
| latin | 312 | 4 + 1 | 0 | 7 | (perc-driven) |
| motown | 233 | 4 + 0 | 0 | 19 | kick 0,8 / snare 4,12 |
| pop | 260 | 4 + 0 | 0 | 11 | backbeat |
| reggae | 202 | 4 + 0 | 23 | 11 | kick 8 + snare 8 (one-drop) |
| rock | 227 | 3 + 1 | 8 | 7 | hard backbeat |
| samba | 244 | 0 + 0 | 18 | 7 | kick 0,4,8,12 |
| shuffle | 200 | 1 + 4 | 0 | 18 | kick 0,8 / snare 4,12 |
| swing | 195 | 3 + 2 | 0 | 18 | kick 0,8 |

Corpus totals:

- **3766 note events. 35 are `kScaleDegree` (0.9%), 14 are `kInterval` (0.4%).
  The remaining 3717 (98.7%) are chord-tone comps or fixed drum hits.**
- **`RolePolicy` has exactly two values in the enum** (`kFixed`, `kChordTone`,
  `style_model.hpp:44`). Every pitched part in every style is `kChordTone`
  ("play this chord's tones"); every drum/perc part is `kFixed`. There is no
  third policy — riff, walking, ostinato, melody are not policies.
- **No bassline uses `kScaleDegree` or `kInterval`.** Every `*Bass*` array is
  built from chord-tone indices only — root/fifth dominant, a little
  third/seventh in motown/swing. There is no walking or chromatic-approach bass
  anywhere in the corpus, because the notes that make one (`kInterval` ±1/±2
  into the next root) are never used on `kBass`.
- **Gestures (D42) total 59** across the corpus: `kStrumUp` 36, `kStrumDown` 21,
  `kRollUp` 2 — concentrated in reggae (23), samba (18), country (8), rock (8);
  bossa and disco 1 each; **the other 10 styles use zero gestures.** (This
  corrects a stale earlier scan that read "most have ≤1 gesture" — several
  styles are now gesture-rich; the distribution is UNEVEN, not uniformly thin.)
- **The `Style` struct carries only `name` + `sections`** (`style_model.hpp:125`).
  There is **no per-style tempo, no per-style swing/feel**. `GrooveParams`
  (`groove.hpp:21`) is ONE GLOBAL runtime state (swing/humanize/accent/quantize),
  not an attribute a style can declare. The grid is 16th-note only
  (`step` is a 16th slot, `groove.hpp` comment). So a style cannot say "I am
  triplet-swung at 60%" — swing/shuffle/bossa/jazz lose their defining feel to a
  global knob the style does not own, and triplet placement cannot be authored on
  a 16th grid.
- **Shared skeleton.** All 16 use the same role roster
  {Drums, Perc, Bass, Chord1, Chord2, Pad, Arp, Lead} and the same section set
  (Intro1/2, VarA–D, FillA–D, Ending1/2); 6 styles add a real `kBreak`
  (funk, disco, rock, motown, latin, blues). The `kPhrase` role (index 7,
  `timeline.hpp:33`) — an available second melodic slot — **is used by 0 styles.**
- Caps for any generative fan-out: `kMaxVoiceNotes=16` (`arranger.hpp:369`),
  `kMaxGestureFan=8` (`gesture.hpp:24`), `StyleEvent` pinned at 10 bytes
  (`static_assert`, `style_model.hpp:103`).

---

## Part 1 — Diagnosis, per genre axis (MODEL GAP vs UNDERUSE)

A genre is defined by its signatures, not by which GM program comps the same
chord. Axis by axis:

- **Rhythmic signature (drums).** EXPRESSIBLE and genuinely USED. The `kFixed`
  drum layer carries real idiom: reggae one-drop (kick+snare both on beat 3 =
  step 8), disco/house/samba four-on-the-floor (kick 0,4,8,12), funk syncopated
  16th kick (0,3,6,10) with ghost snares, backbeat (snare 4,12). This is the ONE
  axis where the corpus is actually differentiated. Not the problem.

- **Bass function.** UNDERUSE bordering on MODEL-adjacent. Real genres are
  separated by what the bass DOES: root-fifth two-beat (country), octave-jump 8ths
  (disco), syncopated 16ths (funk), anticipated tumbao (latin), walking quarter
  line with chromatic approach (swing/jazz), sparse melodic root line (reggae).
  The corpus expresses ONE of these — root/fifth on strong beats — everywhere,
  because bass is `kChordTone`-only and never uses `kInterval`. Walking and
  octave-jump and chromatic-approach basses are describable in TODAY's model
  (`NoteSource::kInterval`, D39) but are used on the bass by **zero** styles. So
  bass sameness is mostly UNDERUSE; the residual gap is that a true walking line
  also wants a per-beat grid and a groove feel the model can't declare.

- **Comping / voicing.** PARTIAL. `VoicingPolicy::kLead` (D41) is used (6–19
  patterns in most styles) and gives smooth voice-leading; register is set via
  octave and `gm_program`. But every comp is the same operation — stack the
  chord's tones at step X. There is no open voicing, no inversion policy, no
  drop-2/rootless-jazz voicing, no power-chord-only (root+fifth, omit third)
  policy — DESIGN itself lists "no open voicing, no inversion policy" as an
  Arranger gap (line 893). The distinction between a rootless jazz comp and a
  country block triad is not expressible; both are "chord tones." MODEL GAP for
  voicing shape; UNDERUSE for the rhythm/register that IS available.

- **Melodic content.** MODEL-thin + UNDERUSED. Only 0.9% of events are diatonic
  (`kScaleDegree`) and 0.4% intervallic; 4 styles (basic, ballad, funk, samba)
  have **no** melodic content at all. Where a melody exists (reggae melodica,
  blues/shuffle blue notes) it is a FIXED authored line — the same four notes
  every bar. There is no melody GENERATION or variation: a `kScaleDegree` event
  resolves to exactly one note, and gestures only fan CHORD tones, never a line.
  So "the melody never changes" is literally true: nothing in the model produces
  a varying melodic line. This is the axis with both the least authored content
  AND a real generative gap.

- **Groove / microtiming / feel.** MODEL GAP. Swing, shuffle, bossa and swing-jazz
  are FEEL genres — their identity is triplet subdivision and microtiming. The
  style cannot declare any of it: no per-style tempo, no per-style swing, a 16th
  grid that can't place triplets. `swing.hpp` and `shuffle.hpp` differ from
  `rock.hpp` only in their note tables and rely on a global `GrooveParams.swing`
  the user must set. Yamaha rule U11 ("groove feel is per-genre") has no home in
  the current `Style` struct. This is the single most consequential gap for
  "these feel like the same song."

- **Instrumentation.** EXPRESSIBLE and used — each style sets distinct `gm_program`
  voices per role (strings/organ/clean-guitar/harmonica in reggae, etc.). Not the
  problem, but also the shallowest kind of difference: a different GM patch playing
  the identical chord-tone comp is exactly the "same song, different sound"
  complaint.

- **Form.** UNIFORM. Same 12-section skeleton, almost all 1-bar loops; the density
  ladder VarA→VarD is present but modest. Song FORM does not differentiate genres
  here; it is a shared template.

**Root cause, stated once.** The pitched half of every style is the SAME
operation — "resolve this chord's tones at these steps" (98.7% of events). The
axes that actually separate genres in real music — bass FUNCTION, melodic LINE,
comp VOICING shape, and FEEL — are collapsed into "chord-tone at step X" (a data
model with two role policies) plus a global groove knob the style does not own.
The drum layer and the GM voices vary; everything harmonic/melodic is one band
playing one functional arrangement. That is precisely "the same song, slightly
rearranged", and it is an aesthetic amputation encoded in the fact that
`RolePolicy` has two values and `Style` has no feel.

---

## Part 2 — The five questions

### Q1 — Can we have MORE styles that are genuinely different? What makes two accompaniments feel like different music, and how much is expressible today?

Yes — and most of the missing difference is UNDERUSE, recoverable TODAY, with a
short list of true model gaps.

What makes two accompaniments feel like different music, ranked by how much the
ear weights it, with today's expressibility:

1. Drum idiom + groove/feel — **half here**. Idiom EXPRESSIBLE and used; feel
   (swing/tempo) a MODEL GAP.
2. Bass function — **EXPRESSIBLE today, UNUSED**. `kInterval` gives octave-jump,
   chromatic approach, anticipation; no style uses it on bass.
3. Melodic line + its variation — **thin + generative GAP**. `kScaleDegree`
   exists; generation does not.
4. Comp rhythm/register — **EXPRESSIBLE, under-varied**; voicing SHAPE a gap.
5. Instrumentation — **fully expressible, already used** (weakest differentiator).

Verdict: a large step up in genuine difference is **SHIPPABLE now** purely by
AUTHORING what the model already allows — `kInterval` basslines per genre
(walking/octave/anticipated), `kScaleDegree` signature phrases on the unused
`kPhrase`/`kLead` roles, genre-appropriate gestures on the 10 gesture-less styles,
distinct comp rhythms per genre rule (Yamaha Part 5 is a ready rulebook). The
CEILING — making swing FEEL swung and a melody that CHANGES — needs the two model
additions in Q4/Q5 (per-style feel; a melodic generator).

### Q2 — Why are the current ones so similar? (root cause, cited)

Four causes, in order of impact:

1. **Two-policy pitched model (MODEL).** `RolePolicy` = {`kFixed`, `kChordTone`}
   only. Every pitched part is "comp the chord." Measured: 98.7% chord-tone/fixed.
   Bass, comp and (absent) melody are the same operation, so they cannot sound
   like different genres.
2. **No per-style feel (MODEL).** `Style` has no tempo/swing; `GrooveParams` is
   global. Feel genres (swing/shuffle/bossa/jazz) collapse into straight-16
   variants of each other.
3. **Uneven, thin use of the vocabulary that EXISTS (UNDERUSE).** D39
   scale-degree/interval (49 events total) and D42 gestures (in 6 of 16 styles)
   are a sprinkle; 4 styles have no melodic content and 10 have no gestures.
4. **Identical skeleton (template).** Same 8 roles, same 12 sections, mostly
   1-bar loops, `kPhrase` unused. Every style is "drums + root-fifth bass +
   chord-tone comp ×2 + held pad + arp." The frame is the same band.

### Q3 — A standalone "stylizer": plain MIDI → pop/rock/samba/…

The enabling fact (Yamaha research Part 3): **apply-on-the-fly and
generate-statically are the SAME `resolve()` with a different chord argument.** A
stylizer is that operation run offline over an analyzed input. Offline is fine.
Options, with tradeoffs:

- **Option A — Host offline "re-accompanier" (rule-based). SHIPPABLE (host tool).**
  Analyze the input MIDI → chord track (the core-portable `ChordDetector`, D34,
  already exists) + section segmentation + key; keep the input's own melody on
  `kLead`/`kPhrase`; run a CHOSEN `Style`'s patterns through `resolve()` against
  the detected chord progression; emit MIDI. This is "play the genre's band over
  YOUR song's chords and form." Reuses the arranger engine wholesale, no new core.
  Effort: moderate (analysis + a host render loop in an `arrstyle-converter`
  sibling). Limit: it re-accompanies; it does not restyle the input's OWN drum/bass
  rhythm — it replaces them with the style's.
- **Option B — True part-restyler (rule-based, HOST-ONLY, more effort).** Map the
  INPUT's parts onto genre behavior: input drums → genre kit/pattern, input bass →
  genre bass FUNCTION, requantize to the genre's groove. Needs (a) the per-style
  FEEL/groove data that is today a model gap, and (b) a genre mapping ruleset —
  the 79 Yamaha rules (research Part 5) ARE that ruleset. Musically the richer
  "restyle", but gated on the feel-model addition (Q5).
- **Option C — Statistical/ML groove & melody transfer. INSTRUCTIVE-BUT-INFEASIBLE
  on device; HOST-ONLY with a flagged dependency.** A learned groove/melody model
  could transfer feel convincingly, but needs model weights + a runtime/toolchain.
  DEPENDENCY FLAG: any ML runtime or weights is a new dependency requiring owner
  approval (CLI-deps policy: core stays dependency-free; host deps evaluated
  jointly). Not shippable to the no-heap core.
- **On-device stylizer of an arbitrary file: not the core's job.** File-level
  analysis + I/O is host work; the DEVICE already does the live equivalent (play
  the chosen style over the live chord). Keep the stylizer host-only.
- **Mapping onto constructs:** input → `ChordSequence` (D28) + `Key`; target →
  `Style`; feel → `GrooveParams`; gestures/voicing per D40/D41/D42; output via the
  existing MIDI writer; the D44 blob lets the target style be swapped without a
  recompile. Recommended first increment: **Option A**, because it is the same
  engine with a different sink and adds no core code.

### Q4 — Change the MELODY, not just re-comp the chord (open brainstorm)

The point is a melodic LINE that varies, not the same authored four notes. Credible
directions, each costed against the no-heap dual-target core:

- **Direction 1 — Motif + transformation grammar. SHIPPABLE.** Author a short
  per-style motif as `kScaleDegree`/`kInterval` events; generate variations with
  deterministic, bounded operators — diatonic transposition, retrograde/inversion,
  rhythmic displacement, ornament/passing-tone insertion, contour-preserving
  reshape — constrained to the Key/NTT envelope so output stays wrong-note-proof.
  Seed-deterministic (same seed ⇒ same line, D16), no heap, bounded fan — exactly
  the D37 Director philosophy, applied to pitch instead of parameters. Needs a new
  small stateful "melodic generator" module and a motif store; runtime is
  device-safe. Musical payoff: real new lines, on the unused `kPhrase`/`kLead`
  roles, that still can't play a wrong note.
- **Direction 2 — Baked Markov/grammar over scale degrees + rhythm. SHIPPABLE
  runtime, HOST-ONLY training.** A per-style order-1/2 transition table over scale
  degrees and durations, TRAINED offline (host) and baked `constexpr` into flash
  (the `canon-builder` codegen pattern already proves "host program emits a
  freestanding artifact"). On device: a tiny deterministic sampler, no heap, table
  in flash. Payoff: stylistically colored melodic variation per genre. Cost:
  authoring/training pipeline (host, offline); the table adds flash. FLAG: training
  is host-only; nothing new on the device except a sampler + `.rodata` table.
- **Direction 3 — Neural melody model (RNN/transformer). INSTRUCTIVE-BUT-INFEASIBLE
  on device.** Musically the strongest, but weights (100s KB–MB) + a runtime blow
  the flash/RAM/realtime budget and add a dependency. HOST-ONLY tool at best, with
  an explicit DEPENDENCY FLAG (model runtime + weights). Name it as the ceiling,
  not a shippable plan.

Cross-cutting requirement for ALL three: melody generation is a NEW module ABOVE
the arranger (the Director's cousin), not a style-table change. The model already
has the melodic SOURCE (`kScaleDegree`/`kInterval`, D39) and empty melodic ROLES
(`kPhrase` unused, `kLead` thinly used) to receive a generated line; what is
missing is the GENERATOR and its state. A richer rhythmic grid (below) would let
the generated line phrase against triplet/16th feels.

### Q5 — How to organize the styles/corpus (taxonomy, families, D44)

- **Family taxonomy by (feel × rhythmic engine), not by name.** Measured drum
  signatures cluster naturally into: straight-8 backbeat (pop, rock, country,
  motown, ballad), four-on-the-floor (disco, house), 16th-funk (funk),
  triplet/shuffle (blues, shuffle, swing), latin-clave (bossa, samba, latin,
  reggae). Within a family, styles share the drum/bass skeleton and differ by
  voice, density, gesture and motif. This makes reuse explicit and forces each new
  style to justify what it adds to its family.
- **Factor shared skeleton from unique payload.** The shared part (role roster,
  section set, density ladder) becomes a reusable template; the UNIQUE payload
  (drum idiom, bass FUNCTION, groove/feel, signature gesture + motif) is the
  per-style data. This is exactly what D44's data format enables: author as data,
  share templates, mechanically lower to `constexpr` for the device.
- **The one struct change worth proposing: give `Style` a feel.** Add a per-style
  default `GrooveParams` + tempo (and consider an optional triplet/12-tick grid
  flag) so swing/shuffle/bossa/jazz can DECLARE their feel instead of borrowing a
  global knob. Cost is flash-only (data), consistent with D39's precedent, but it
  is an ABI/struct change → OWNER DECISION. It is the highest-leverage single fix
  for "same song".
- **D44 corpus plan is sound; the real payoff is scale.** The layered format
  (JSON authoring on host → `constexpr` bake for the device; a flat offset-based
  blob for runtime-load; built-ins stay baked) from `style-data-format.md` holds.
  The order-of-magnitude prize is ingesting the validated 1010-style Yamaha corpus
  via the `arrstyle-converter` CASM→NTT decode (research Part 6) — no new
  dependency, host-only work. Recommended sequence: Step 0 (teach the converter to
  emit today's constexpr header from its `StyleModel`) → author far more, richer
  styles as DATA → then the blob + loader for no-recompile.

---

## Part 3 — What needs an owner decision / flagged tensions

1. **Per-style feel in the `Style` struct** (default `GrooveParams` + tempo,
   maybe a triplet grid). Flash-only cost, but an ABI/struct change and the single
   biggest lever for genre feel. OWNER DECISION.
2. **Is generative melody in scope as a NEW module** (Director-adjacent,
   Direction 1/2), or do we stay with authored `kScaleDegree` lines? Product fork.
3. **Stylizer scope** — Option A (host re-accompanier, shippable, no new code) vs
   B (part-restyler, needs the feel model) vs C (ML, dependency). Any ML path =
   DEPENDENCY FLAG (model runtime + weights), owner approval per CLI-deps policy;
   the core stays dependency-free.
4. **Corpus expansion** via finishing the SFF/CASM→NTT importer — no new
   dependency, host-only — is the cheapest path to real variety and is largely
   an execution decision, not a design one.

Nothing above changes the core doctrine: every SHIPPABLE item is no-heap,
dual-target, dependency-free; every HOST-ONLY and INSTRUCTIVE item is labelled as
such, and every dependency is flagged, not smuggled.

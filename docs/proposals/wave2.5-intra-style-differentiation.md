# Wave-2.5: intra-style section differentiation in a moving-harmony world

Ottorino, static corpus analysis (read-only on product code; the sole write is this
document). Task: measure how DISTINCT `Intro1/VarA/VarB/VarC/VarD/Fill*/Ending*` are
from each other, INSIDE each of the 16 built-in styles, and design a prioritized,
per-genre Wave-2.5 program — reframed for the fact that live harmony is landing
host-side (a parallel, non-conflicting lane) and will soon give every `kChordTone`
role real pitch movement it does not have today. Corpus measured:
`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (16 files,
current `gui-sonotron` HEAD `5a88d95`).

## 0. Why the target axis changes, and what state the corpus is actually in today

**The corpus already includes Wave-1 AND Wave-2C — this is not the state either
prior Ottorino pass measured.** Verified directly (`git log --oneline -- .../styles/`):
`59660fd` (Wave-1: per-genre humanize + VarD motif, already covered by
`docs/proposals/intra-style-section-differentiation.md`) is followed by `f117b0f`
("Wave-2C multi-bar Intro1/VarA across all 16 styles + downbeat-honest humanize")
and `5a88d95` ("reconcile basic-style Intro redesign onto post-Wave-2C main"), both
landed on the main tree, not left in a worktree as the `parked-workstreams` memory
note (written mid-session, now stale on this point) suggested. Confirmed by re-grep:
`.bars=2` now appears **exactly twice per style, 32 times total across the 16
files** — always `Intro1` and `VarA`, `.bars=1` everywhere else (`grep -o
'\.bars=[0-9]*' styles/*.hpp`). Wave-2.5 must build on top of this, not repeat it.

**Live harmony is landing separately (Giotto, `apps/gui-sonotron/`, a different
lane, no conflict with this read-only pass).** `docs/reflections/cli-vs-gui-ab-2026-07.md`
measured the concrete effect: with live harmony driving a real ii–V–I–vi turnaround,
the same 8-bar capture visits **7 of 12 pitch-classes** on bass+chord1+chord2;
without it (today's GUI default), it is frozen on **3** — the tonic triad, for the
whole session. `harmony-progression-architecture` (owner decision, 2026-07-17)
settles WHERE the fix lives: a host-supplied default `ChordSequence` per style, fed
through the *existing* core `seq` verb — **zero new core surface**, and every
`kChordTone` role will start moving in pitch, automatically, style-load onward.

**Consequence for this task, stated precisely so it isn't lost:** the two prior
Ottorino passes measured similarity on `(step, tone, octave)` — a key that *includes*
pitch. Once harmony moves, that key's `tone`/`octave` component becomes noisy: two
bars of the same `kChordTone` pattern will differ in pitch by construction (different
chord underneath), even if the *rhythm* is the identical, unvaried skeleton it always
was. Measuring differentiation on that key would soon report "fixed" sections that
never actually changed anything except which chord happened to be current. This pass
therefore uses a **pitch-blind** metric — onset position only (`step % 16`, ignoring
`tone`/`octave` entirely) — precisely so the finding survives the harmony landing
instead of being invalidated by it.

## 1. Method

A purpose-built parser (`measure_wave25.py`, written by me, spec'd exactly, then
delegated for the formal run + summarization to `celestino-corpus-hand`, who
executed it verbatim and returned raw tables with zero interpretation — I verified
his numbers against direct reads of `rock.hpp`, `reggae.hpp`, `shuffle.hpp`,
`latin.hpp`, `blues.hpp` before trusting them) extracts every `StyleEvent[]` /
`StylePattern[]` / `StyleSection[]` literal into a role→section model, then computes,
per style × section × role:

- **density**: event count;
- **rhythm signature**: the SET of onset `step % 16` positions (pitch-blind by
  construction — this is the new axis, replacing the old `(step,tone,octave)` key);
- **register**: `octave` field spread (still meaningful post-harmony: it is the
  authored octave OFFSET, orthogonal to which chord tone NTT resolves);
- **instrumentation**: role-set per section;
- **motif presence**: per role-pattern, whether `.motif=` is wired.

Then, pairwise, a **rhythm Jaccard** on the onset-position sets for every
`VarA/B/C/D` pair and for the perceptually-loadbearing transition pairs
(`Intro1→VarA`, `VarX→FillX`, `VarD→Ending1/2`). No cmake/ctest run (header-text
analysis only, consistent with the two prior passes).

## 2. What I measured

### 2.1 A real, already-shipped strength: instrumentation ramps everywhere

Role count Intro1→VarD, all 16 styles (Celestino's Table 3, unedited):

| style | Intro1 | VarA | VarB | VarC | VarD | FillA | FillD | Ending1 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| ballad | 4 | 5 | 5 | 7 | 7 | 2 | 2 | 4 |
| basic | 3 | 5 | 6 | 6 | 7 | 2 | 3 | 4 |
| blues/bossa/country/disco/funk/house/reggae/rock/samba/shuffle/swing | 2 | 5–6 | 6 | 6–7 | 7–8 | 2–3 | 3 | 4 |
| latin | 2 | 6 | 6 | 7 | 8 | 3 | 3 | 4 |
| motown/pop | 2 | 5 | 6 | 7 | 8 | 2–3 | 3 | 4 |

Every style ramps role count roughly 2→5→6→7→8 across Intro1→VarD. This is not
flat and I am not manufacturing a complaint here: **do not spend Wave-2.5 budget
re-proving instrumentation growth that already exists.**

Density also already ramps (dips at VarC, rebounds at VarD) in most styles — e.g.
`blues` drums 24→14→4→14, `samba` drums 53→26→8→28 (Celestino's Table 2). A sparse
VarC before a dense VarD is a real, idiomatic "half-time breakdown before the peak"
arc, already present corpus-wide. Good baseline; not a target either.

### 2.2 The headline finding: pitch-blind rhythm sameness across VarA–D, ranked

Mean rhythm-Jaccard (onset positions only) over all 6 `VarA/B/C/D` pairs, Bass and
Drums (the two roles present in all 16 styles), combined and ranked worst→best:

| style | bass mean J | drums mean J | combined | reading |
|---|:-:|:-:|:-:|---|
| **shuffle** | 1.00 | 1.00 | **1.00** | worst |
| **reggae** | 1.00 | 0.938 | **0.969** | |
| **latin** | 1.00 | 0.80 | **0.90** | |
| **disco** | 1.00 | 0.75 | **0.875** | |
| **samba** | 1.00 | 0.75 | **0.875** | |
| funk | 0.917 | 0.80 | 0.858 | |
| motown | 1.00 | 0.667 | 0.833 | |
| swing | 1.00 | 0.651 | 0.825 | |
| blues | 1.00 | 0.625 | 0.813 | |
| bossa | 0.778 | 0.812 | 0.795 | |
| house | 0.867 | 0.682 | 0.775 | |
| pop | 0.604 | 0.767 | 0.686 | |
| rock | 0.606 | 0.65 | 0.628 | |
| ballad | 0.542 | 0.695 | 0.618 | |
| country | 0.667 | 0.50 | 0.583 | |
| **basic** | 0.50 | 0.50 | **0.50** | best |

`basic` — the neutral scaffold style — is again (as it was before Wave-2C, per the
prior doc) the LEAST self-similar style in the corpus, on the pitch-blind metric too.
It is the corpus's own working proof that four genuinely distinct drum/bass ideas
per `Var` is achievable inside the existing model; I use it as the internal
reference example below rather than inventing one.

### 2.3 The specific, confirmed mechanism: VarB↔VarD is where the flatness concentrates

Drums `VarB–VarD` rhythm-Jaccard = **1.0 in 12 of 16 styles** (basic, blues, bossa,
country, funk, latin, motown, reggae, samba, shuffle, swing, + rock at 0.8); only
`ballad` (0.5), `disco` (0.5), `house` (0.688), `pop` (0.533) genuinely rework the
drum skeleton between the second variation and the peak. Bass `VarB–VarD` is
literally the **same source array** (`same_array=1`, not just equal onsets) in
**12 of 16 styles**.

I read the actual source to see WHY, in two concrete, re-confirmed cases:

- **`latin.hpp`**: `kBD` (`VarB` drums, lines 149–153) and `kDD` (`VarD` drums, line
  177) are a byte-for-byte literal duplicate — same 20 cowbell/conga/timbale tuples,
  same steps, same velocities (`88,68,78,68,88,68,78,68` for cowbell on both).
  "Son montuno" (VarB) and "mambo peak" (VarD) differ ONLY in Chord1/Arp/Lead/Perc —
  the drum pattern that carries the genre's actual clave/cascara identity never
  changes at all between the two.
- **`shuffle.hpp`**: `kBD` and `kDD` are NOT byte-identical, but read side by side
  (lines 121–124 vs 149–152) they fire on the exact same 16th-grid positions
  (kick 0/6/8, snare 4/12/14, hat every 2 steps) — the ONLY difference is that
  `kBD`'s off-beats are closed-hat and `kDD`'s are open-hat. VarD is VarB with the
  hi-hat re-articulated, not a different groove.

### 2.4 Bass function: a 3-way split, confirmed and re-quantified

Bass array identity across `VarA/B/C/D`, all 16 styles (my own reduction of
Celestino's raw table):

| n unique bass arrays | styles | what it means |
|:-:|---|---|
| **1** (all 4 vars share one array) | blues, latin, motown, reggae, samba, shuffle, swing | 7 styles — matches (and reconfirms unchanged since) the prior doc's finding |
| **2**, wired `VarA=VarC`, `VarB=VarD` | bossa, country, disco, funk, house | 5 styles — a NEW, more precise finding: the "peak" variation does not get its own bass idea, it borrows VarB's verbatim (e.g. `funk.hpp`/`disco.hpp`/`house.hpp` literally name it `kBB`, wired into both `VarB` and `VarD`) |
| **4** (genuinely distinct per var) | ballad, basic, pop, rock | 4 styles — the healthy end |

### 2.5 Motif coverage: unchanged since Wave-1 (Wave-2C did not touch it)

| section | motif-wired / total role-patterns | % |
|---|:-:|:-:|
| Intro1 | 0/35 | 0% |
| Intro2 | 10/65 | 15.4% |
| VarA | 15/81 | 18.5% |
| VarB | 19/95 | 20.0% |
| VarC | 35/100 | 35.0% |
| **VarD** | **83/123** | **67.5%** |
| FillA–C | 14/34 each | 41.2% |
| FillD | 14/47 | 29.8% |
| Break | 0/19 | 0% |
| Ending1/2 | 2/64 each | 3.1% |

Identical shape to the pre-Wave-2C measurement (expected: Wave-2C only added bar-2
content to `Intro1`/`VarA`, it did not rewire any `.motif=`). Concentration on VarD
stands, unchanged.

### 2.6 Two findings that are NOT flaws — flagging explicitly so I don't manufacture problems

- **`Intro1→VarA` rhythm-Jaccard is high (0.5–1.0) almost everywhere, and that is
  BY DESIGN, not a residual flatness.** Wave-2C's own code comments say so directly
  — `rock.hpp:187-188`'s bar-2 Intro drums are commented "the arrival... landing
  right where VarA starts"; the point of that bar IS to preview the destination
  groove as an anacrusis. A high Intro→VarA Jaccard here is the intended effect, and
  I am not recommending it be "fixed."
- **`VarX→FillX` bass Jaccard is 1.0 (same array) in most styles, and that is
  idiomatic, not underuse.** A drum fill conventionally rides over a HELD or
  unchanging bass while the kit does the work — real arranger-workstation practice,
  matching `docs/style-corpus-and-generation.md`'s own researched convention that
  fills are a drums-forward event. I am not proposing new bass content under fills.
- **`VarD→Ending1/2` rhythm-Jaccard is uniformly low (0.06–0.17)** — endings
  genuinely differ from the peak variation, confirming (again) the prior audit's
  verdict that endings are NOT the corpus's problem.

### 2.7 The model gap that has not moved: `GrooveParams` is still per-`Style`, not per-section

Re-verified directly: `GrooveParams` (`arrangrr/arranger/groove.hpp:21-29`) and
`Style::groove` (`style_model.hpp:184`) are unchanged since the prior pass — one
`GrooveParams` per style, applied identically to every section. There is still no
way to make VarD swing/humanize harder than VarA. Unchanged verdict: **real model
work, not a Wave-2.5-shaped data edit** (see §4, item flagged INSTRUCTIVE-BUT-
INFEASIBLE-FOR-THIS-WAVE).

## 3. Diagnosis — model gap vs underuse, qualified by genuine genre idiom

Before prescribing fixes, I separate three things the raw numbers alone cannot:
(a) **accidental underuse** — a section pair is flat because nobody authored a
difference, and the model already has everything needed to; (b) **deliberate,
genre-correct invariance** — some roles in some genres ARE supposed to stay locked,
and the corpus's own code comments already say so; (c) **model gap** — the
differentiation the axis wants cannot be expressed in `StyleEvent`/`StylePattern`/
`GrooveParams` today at all.

- **Rhythm/onset skeleton (Drums, mostly Chord1)**: **UNDERUSE**, 12/16 on VarB↔VarD.
  `StyleEvent`/`StylePattern` already support two genuinely different onset grids;
  the model does not need to change, someone has to author the second idea.
- **Bass function**: split. The 7 single-array + 5 alternating-array styles are
  **UNDERUSE** for "author one more idea for the peak tier" (no core change). Going
  further — an actual WALKING bass, or a chromatic-approach idiom, rather than a
  second hand-picked ostinato — is a **MODEL GAP**, unchanged since the prior pass:
  `MotifTransform` still only offers `kDiatonicTranspose`/`kRetrograde`/
  `kDisplacement` (`style_model.hpp:112-117`), none of which IS "walk," so any fix
  within Wave-2.5 is authored data, not a new bass-generation idiom.
- **Register**: **UNDERUSE, zero model gap.** The `octave` field is free and
  already expressive (`StyleEvent::octave`, `style_model.hpp:92`); no style
  currently ramps register across the `VarA→VarD` arc as a deliberate device (an
  octave push reserved for the peak, distinct from an incidental one-off like
  `rock.hpp`'s existing off-beat octave bounce in VarA bass, which is already
  present in EVERY var, not reserved for the climax).
- **Instrumentation**: **STRENGTH, not a target** (§2.1) — already ramping
  everywhere; where a style is role-saturated by VarB already, Wave-2.5 budget
  should go to density/register instead of adding an 8th role nobody will notice.
- **Groove/feel across sections**: **MODEL GAP**, unchanged (§2.7).
- **Motif distribution**: **UNDERUSE**, concentrated at VarD/peak (§2.5) — though
  I explicitly do NOT flag Endings' near-zero motif coverage as underuse: an ending
  plays once and stops (`section_is_ending`, engine fires `stop_transport`), so
  there is no "repeat" for call-and-response to vary across; 3% motif coverage there
  is correct, not a gap.
- **Fill/Ending sameness**: **NOT A FLAW** (§2.6) — already well-differentiated by
  the corpus's own content; I refuse to invent a target here.

**The idiom-vs-underuse split, concretely, changes which of the worst-ranked 8
styles get a rhythm-skeleton fix and which do not:**

| style | drums/bass VarB≈VarD is... | why |
|---|---|---|
| latin | **underuse** | drums (cowbell/conga skeleton) — no stated idiomatic reason for the drums to be frozen; the code's own rationale (`latin.hpp`) only ever defends the TUMBAO bass's repetition, never the drum pattern |
| motown | **underuse, and arguably ANTI-idiomatic** | the Motown/Jamerson soul-bass idiom is famous for being the MOST active, syncopated, chromatically-decorated bass in popular music — a single static array for all 4 variations is a genuine gap against the genre's own defining feature, not a stylistic choice |
| swing | **underuse, and definitionally anti-idiomatic** | a WALKING bass is, by definition, supposed to keep moving; freezing it to one array for the whole arc contradicts the idiom's own name |
| blues | **underuse** | 12-bar boogie convention idiomatically intensifies (turnaround lick, chromatic descent) toward a chorus's end; nothing in the code defends the current total staticity as deliberate |
| disco | **partial underuse** | the "octave-bounce" disco bass IS conventionally repetitive (a real, legitimate genre trait — "four-on-the-floor" bass patterns are supposed to be hypnotic), but total invariance across the WHOLE arc, including the peak, still leaves the peak with nothing new; a register push at VarD is idiomatic, a whole new idea is not required |
| reggae | **DELIBERATE, genuinely idiomatic — do not touch** | `reggae.hpp`'s own comments say so explicitly ("Finding B": the one-drop's exact beat-3 kick+snare and the root-bass anchor DEFINE the groove; motif variation there "risks decoherence") |
| shuffle | **DELIBERATE for bass, underuse for drums** | the shuffle bass is explicitly motif-locked by design comment ("the densest single-idiom bass in the corpus"); but the drums-only-differ-by-hat-articulation finding (§2.3) has no such stated rationale — that one IS a gap |
| samba | **partial DELIBERATE, partial underuse** | a samba surdo-locked bass (following, not leading, the drum pattern) is a real, defensible convention; but VarB≈VarD on the caixa/tamborim SUBDIVISION has no such defense — samba variations conventionally differentiate by caixa/repique density, not by freezing it |

## 4. Wave-2.5 moves — prioritized, per style, idiomatic, harmony-agnostic

Every move below changes **density, register, onset position, or instrumentation**
— never pitch content — consistent with §0's reframe: pitch variety is about to
arrive "for free" from live harmony; rhythm/density/register variety will not, and
is the one axis Wave-2.5 should spend its whole budget on. All moves are
**constexpr style-table edits only**: no ABI change, no new `arranger.hpp`/
`motif.hpp`/`style_model.hpp` surface, `StyleEvent` stays pinned at 10 bytes,
no new dependency anywhere. Every move is **SHIPPABLE** under that constraint
unless stated otherwise.

### Tier 1 — worst combined score, real gap (not idiom), highest priority

1. **`motown`** (combined 0.833, bass 1.0/1.0/1.0/1.0 across ALL pairs) — the
   single most genre-mismatched flatness in the corpus. **Move**: author a second
   `kSoulBass`-family array (`kSoulBassPeak` or similar) for the `VarC`/`VarD` tier
   with a chromatic passing 16th between root and fifth (an `NoteSource::kInterval`
   event — already-proven vocabulary, `rock.hpp:69`) and a syncopated anticipation
   push ahead of beat 3 — the two idioms that define Jamerson-style bass playing.
   **Touch**: `motown.hpp`, the `Bass` entry of `kVarCPatterns`/`kVarDPatterns`
   (currently both point at the shared `kSoulBass`).
2. **`swing`** (combined 0.825, bass 1.0 on all 6 pairs) — walking bass that never
   walks differently. **Move**: give `VarD` a genuine turnaround/passing-tone
   variant of `kWalkBass` — a chromatic approach note (`kInterval`) into the repeat's
   downbeat, the standard "walk into the turnaround" bebop device; this is
   harmony-agnostic (a passing tone works under any live chord). **Touch**:
   `swing.hpp`, add `kWalkBassPeak[]`, wire into `kVarDPatterns`' `Bass` entry.
3. **`blues`** (combined 0.813, bass 1.0 all 6 pairs, `kBoogieBass` shared) —
   **Move**: a second boogie-bass idea for `VarD` with the classic descending
   chromatic turnaround lick on the last beat (a real 12-bar-blues device); also
   rework `VarD` drums (`kDD`) so its kick pattern gains 1-2 genuinely new onset
   positions rather than the current uniform velocity-only difference from `VarB`'s
   `kBD` (confirmed still: same 14-event grid). **Touch**: `blues.hpp`,
   `kVarDPatterns`' `Bass` entry (new array) + `kDD`'s onset positions.
4. **`latin`** (combined 0.90) — the starkest single case: `VarB`/`VarD` drums are
   byte-identical (§2.3). **Move**: give `VarD`'s cowbell/bell pattern a genuine
   "mambo bell" — an 8th-note-denser campana, not the son-montuno cowbell placement
   of `VarB` — same instrument choice (cowbell/timbale), different onset density;
   also add an octave-doubled anticipation note to the `VarD` tumbao on the
   syncopated "and of 2" (a real mambo-peak convention). **Touch**: `latin.hpp`,
   replace `VarD`'s `kDD` reference with a new, denser cowbell/conga array; the
   `Bass` entry of `kDPatterns` (currently `kTumbao`, shared with all 4 vars) gets
   one new octave-push event.
5. **`shuffle`** (combined 1.00, worst overall) — bass is deliberately locked
   (§3, do not touch); drums is the real gap. **Move**: `VarD`'s kick pattern gets
   a genuine extra onset (a "Jimmy Reed" pickup push on the & of 2), so the
   kick/snare skeleton itself differs from `VarB`, not just hat articulation
   (closed→open). This also sidesteps a real, separate mechanism gap: `m_motif_repeat`
   only advances on same-section loop-back (`arranger.hpp:480`), never on a section
   switch, so `kPeakDrumsMotif`'s displacement will not even have applied on the
   FIRST time a listener switches into `VarD` — authoring the base array distinctly
   is the fix that works regardless of that mechanism's timing. **Touch**:
   `shuffle.hpp`, `kDD`'s kick onset positions.
6. **`disco`** (combined 0.875) — the repetitive octave-bounce bass IS the genre
   (§3); the fix is a register push, not a new idea. **Move**: add one grace octave
   note on the & of 4 in `VarD`'s bass (an intensification of the SAME idiom, not a
   new one), plus confirm `VarD` drums keep their already-present extra density
   (Jaccard here is already the best-behaved of the tier-1 group on drums, 0.75 mean
   — do not over-fix what already works). **Touch**: `disco.hpp`, `kBB`'s `VarD`
   reference (currently identical to `VarB`) needs its own small variant, e.g.
   `kBBPeak[]`.
7. **`samba`** (combined 0.875) — bass-follows-surdo is a defensible convention
   (§3); the caixa/tamborim subdivision is the real gap. **Move**: `VarD` gets 2-4
   extra caixa ghost-strokes (a real samba "virada" push, denser 16th subdivision),
   distinct from `VarB`'s onset grid, while leaving the surdo-locked bass alone.
   **Touch**: `samba.hpp`, `VarD`'s drums array — new ghost-note onsets, same
   instrument (`kSnare`/caixa voice), no new role.

### Tier 2 — alternating-2-bass-idea styles (VarD silently borrows VarB's bass)

8. **`funk`** (bass 0.917 mean — the "2 ideas" are themselves too similar) —
   **Move**: `VarD`'s bass (currently `kBB`, byte-shared with `VarB`) gets a real
   16th-note syncopated ghost-note addition — the JBs/P-Funk convention of the
   peak groove getting MORE percussive muting, not just more velocity. **Touch**:
   `funk.hpp`, add `kBBPeak[]`, wire into `kVarDPatterns`' `Bass`.
9. **`house`** — **Move**: `VarD` bass gets an octave-doubled offbeat push (four-
   on-the-floor house convention: the peak intensifies via a fuller low end, not a
   new note choice). **Touch**: `house.hpp`, `VarD`'s `Bass` entry.
10. **`bossa`** — Chord1 (nylon-guitar comp) is already reasonably varied (§2, mean
    0.348 — one of the better-behaved styles on THAT role); the gap is drums (mean
    0.812, second-worst in the corpus after shuffle). **Move**: `VarD` drums gets a
    genuine extra rim/shaker subdivision distinct from `VarB`'s brushes pattern — a
    real "more forward" bossa-nova peak convention (denser ride pattern), not a
    louder copy. **Touch**: `bossa.hpp`, `VarD` drums array.
11. **`country`** — combined score is mid-table (0.583) but bass is still the
    alternating-2-idea pattern (`kBoomBass`/`kBB`). **Move**: lowest priority in
    this tier — a single walking-bass-style passing tone on `VarD`'s turnaround, in
    the same spirit as item 2 (swing), reusing the same `kInterval` idiom.
    **Touch**: `country.hpp`, `VarD`'s `Bass` entry.

### Tier 3 — already the healthiest 4; minor, targeted fixes only

12. **`rock`** — the corpus's own comment already flags a real, un-fixed asymmetry:
    `rock.hpp:140-146` notes that `Chord2`'s motif-generated events never carry
    `step>=16`, so in the now-2-bar `VarA`, `Chord2` sounds in bar 1 and goes silent
    in bar 2 — an audible dropout, not a stylistic choice, and the comment itself
    says "flagged, not fixed." **Move**: give `VarA`'s `Chord2` its own (non-motif)
    bar-2 event at `step>=16` so it does not vanish for half the section — a small,
    contained data-only fix, distinct from anything Wave-2C touched. **Touch**:
    `rock.hpp`, `kChord2Off` or a bar-2-only variant wired into `kVarAPatterns`.
13. **`ballad`/`basic`/`pop`** — already the best 3 on the combined metric (0.618,
    0.50, 0.686). No structural move proposed; if any effort is spent here, it
    should be the smallest, most surgical one (e.g. `ballad`'s bass mean 0.542 is
    still non-trivial — a single passing tone on `VarD`, same shape as item 2/11,
    would close most of the remaining gap) rather than a new idiom.

### Explicitly NOT recommended (idiom-preserving, do not touch)

- **`reggae`**: one-drop drums + root-bass anchor — deliberately, correctly locked
  by the style's own design rationale (§3). If any Wave-2.5 budget goes to reggae,
  spend it on `Chord2`'s organ-bubble register (an octave lift reserved for `VarD`,
  `kBubble7` currently shared identically across `VarC`/`VarD`) — a register move on
  a NON-anchor role, never the drums/bass.
- **`shuffle` bass**, **`samba` bass**: as above, leave alone; the drums-side moves
  (items 5, 7) already close the measured gap without touching the locked anchor.

## 5. The per-section groove idea, re-flagged (unchanged verdict)

A genuine per-section/per-pattern `GrooveParams` override (so `VarD` could
swing/humanize harder than `VarA`, the real arranger-workstation "peak variation
pushes the time" idiom) remains **INSTRUCTIVE-BUT-INFEASIBLE for Wave-2.5**: it
needs a new field on `StyleSection`/`StylePattern` plus an `Arranger::on_tick` read
path, which is core/ABI-adjacent work, not a `constexpr`-table edit. Unchanged from
the prior pass's verdict; still a candidate for its own scoped follow-up, not this
wave.

## 6. What Giotto needs to touch (clean-slice list)

Every item below is a `constexpr` array addition/edit inside the named file, no
`style_model.hpp`/`arranger.hpp`/`motif.hpp` change, no new role, no ABI surface:

| style file | what changes |
|---|---|
| `motown.hpp` | new `Bass` array for `VarC`/`VarD` tier (chromatic passing + anticipation) |
| `swing.hpp` | new `kWalkBassPeak[]` for `VarD` `Bass` |
| `blues.hpp` | new `VarD` `Bass` array (turnaround lick) + edited `kDD` onsets |
| `latin.hpp` | new `VarD` drums array (denser cowbell/bell) + one added `Bass` event |
| `shuffle.hpp` | edited `kDD` kick onsets (leave `kShufBass` alone) |
| `disco.hpp` | new `kBBPeak[]` for `VarD` `Bass` (leave drums as-is) |
| `samba.hpp` | edited `VarD` drums onsets (leave bass alone) |
| `funk.hpp` | new `kBBPeak[]` for `VarD` `Bass` |
| `house.hpp` | edited `VarD` `Bass` (octave-doubled push) |
| `bossa.hpp` | edited `VarD` drums (denser ride/shaker) |
| `country.hpp` | edited `VarD` `Bass` (passing tone) |
| `rock.hpp` | new bar-2 `Chord2` event in `VarA` (closes the flagged silent-dropout) |
| `ballad.hpp` | optional, smallest: one `VarD` `Bass` passing tone |
| `reggae.hpp`, `shuffle.hpp` (bass), `samba.hpp` (bass) | **do not touch** — deliberate idiom |

No file needs a new role, a new `RolePolicy`, a new `NoteSource`, or a new
`MotifTransform` — every move is expressible in the current model. Golden tests
WILL move for every style touched; that sign-off is the owner's, consistent with
the standing discipline already established for Wave-1/Wave-2C.

## 7. What needs deciding / what I flag

- **§5 (per-section groove override)** is a real, owner-schedulable model-gap fork,
  unchanged from the prior pass — I have not re-sized it here, it needs its own
  scoping pass if picked up.
- **The `MotifTransform` vocabulary gap** (no "walking bass"/"chromatic approach"
  transform) is real and still open (`phase7-scope-9210-9320-antisameness.md` §2,
  restated here) — items 2/3/8/11 in §4 work around it with hand-authored data
  rather than closing it; closing it for real is a separate, larger decision.
- **Once live harmony lands, re-measure whether motif's `kDiatonicTranspose` on
  lead/arp roles is still earning its keep.** Several Wave-1 motif choices (e.g.
  `rock.hpp`'s `kLeadLickMotif`, `MotifTransform::kDiatonicTranspose`) exist partly
  to inject pitch variety against a static chord; once the chord itself moves, that
  specific transform's marginal value may shrink relative to the rhythm-only
  transforms (`kDisplacement`/`kRetrograde`). I do not recommend undoing anything
  today — the wiring is harmless either way — but flag this as a real question for
  whoever next tunes motif choices, once the harmony landing is measured live.
- **Zero new dependency, zero product-code write** in this document or in the
  measurement pipeline — confirmed: the only write is this file; the parser script
  lives in the session scratchpad, not the repo.

Files read (all absolute paths):
`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (all 16),
`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`,
`components/core/arrangrr/include/arrangrr/arranger/groove.hpp`,
`components/core/arrangrr/include/arrangrr/timeline/timeline.hpp`,
`docs/DESIGN.md` (D24/3110, D37/10000, D39/3130, D40/3120, D41/3140, D42/3150),
`docs/reflections/cli-vs-gui-ab-2026-07.md`,
`docs/proposals/intra-style-section-differentiation.md`,
`docs/proposals/style-section-depth-and-length.md`,
auto-memory `harmony-progression-architecture`, `parked-workstreams`,
`wave1-style-depth-shipped`.

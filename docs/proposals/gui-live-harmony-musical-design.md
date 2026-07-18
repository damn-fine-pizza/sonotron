# GUI live-harmony musical design — event shape, per-style defaults, section interaction, SFF-import fork

Status: proposal, 2026-07-18. Author: Ottorino (style/arrangement analyst).

## 0. What this document is, and — load-bearing — what it is NOT

Before anything else: **the specific gap this task describes (GUI emits zero
`key`/`chord`/`seq` events, static tonic drone forever) is already fixed,
shipped, and functionally tested on this branch.** I built and reasoned about
that fix in a prior pass; it is not a re-litigation, it is the ground truth I
verified fresh for this document:

- `docs/proposals/per-style-default-progressions.md` (my own prior proposal,
  2026-07-17) designed the mechanism and the 16 progressions.
- `apps/gui-sonotron/src/default_style_progressions.hpp` is the shipped host
  data table (16 `DefaultProgression` entries, `kDefaultProgressions`).
- `apps/gui-sonotron/src/in_process_brain_session.cpp:1077-1178`
  (`append_default_progression_commands`, `handle_style_change_progression`)
  is the shipped wiring: it builds `kKeySet`/`kSeqNew`/`kSeqClear`/`kSeqAdd`
  ×N/`kSeqLoop`/`kSeqPlay` `Command` PODs and pushes them right after a
  successful `kStyleLoad`/`kStyleSwitch`. `release_pending_progression_
  if_due` (same file, lines 1340-1349) bar-phase-aligns a LIVE switch — see
  §3.0, the one place I found the shipped design needed more citation than
  my first pass gave it.
- `apps/gui-sonotron/tests/test_default_style_progression_functional.cpp`
  drives the REAL `InProcessBrainSession` translator path (never a
  hand-written `seq` line) and asserts, for **all 16** built-in styles per
  its own 2026-07-17 QA-extension comment (lines 24-38): chord events fire at
  all, harmony actually moves (>1 distinct chord label), and pitch-class
  coverage escapes the bare `{0,4,7}` tonic triad (`popcount > 3`).
- Shipped in commits `5aed414` (design doc), `7f07a6f` (implementation +
  first-pass test), and Torquato's 2026-07-17 QA extension to all 16 styles
  referenced inside the test file itself (issue #29).

So items 1 and 2 of the brief this task restates are **already answered by
code that compiles, runs, and is tested** — I re-verified the claim, I did
not re-derive it, and I am NOT inventing a fresh set of 16 progressions.
What genuinely remains as this document's own content is an AUDIT of what
shipped, plus the one area (section interaction) that is a real, measured
gap rather than an already-closed one:

- **§1** restates the "what must a live-harmony path produce" contract
  precisely, because the shipped mechanism embodies an answer but nowhere
  states it as a portable design contract on its own — useful for whoever
  designs the NEXT harmony source (a live chord/detect panel, D47's other
  producers).
- **§2** audits the shipped 16 progressions, style by style, against the
  `StyleFamily` taxonomy and real genre convention — judging each as
  idiomatic or deficient, proposing a concrete replacement ONLY where one is
  actually found deficient (none were, at the "needs a rewrite" bar — see
  §2's own verdict for the one residual soft spot that does NOT clear that
  bar).
- **§3 is the actually new work**: how the chord progression interacts with
  `SectionType` (VarA-D / Fill / Break / Ending) AND with a LIVE style
  switch's own bar-phase alignment. §3.0 audits a mechanism I under-cited in
  my own first pass (the bar-phase alignment is shipped and correct); §3.4
  (Ending) is the one place a real, unclosed gap remains.
- **§4** sharpens the SFF-import collision beyond what the prior proposal
  flagged, with one new observation (index-keying vs. identity-keying) and
  an explicit answer to "which styles collide with an imported style's own
  harmony" (§4.1: today, ALL of them, uniformly, because none are wired to
  identity-keyed import data yet).

## 1. What a live-harmony path must produce, as a portable contract

Grounded in the core's own functional chord model
(`components/core/arrangrr/include/arrangrr/chord/chord_sequence.hpp:10-15,
19-26`, D28): a `ChordStep` is `{start, duration, degree (0-6 relative to a
per-sequence reference `Key`), quality_ovr (-1 = smart D19, else explicit
`ChordQuality`), velocity}`. A live-harmony path — whether the shipped
default-progression sequencer, a future live chord/detect producer, or an
imported style's own `SongModel.chords` — must therefore produce, at
minimum:

1. **One reference key** (`Param::kKeySet`, root pitch-class + `Mode`).
   Everything else is relative to it; changing it re-derives, it does not
   blindly transpose (`ChordSequence::transpose_to`, chord_sequence.hpp:107).
2. **An ordered list of chord events at BAR granularity.** `ChordStep`
   durations are free-form ticks, but every host-side producer measured so
   far (the shipped table, the `.acmd` demo scripts) quantizes to whole bars
   — and that is not arbitrary: I measured (§3 below) that `SectionType`
   bar-lengths are themselves fixed per style at `{Intro1=2, Intro2≤1,
   VarA=2, VarB-D=1, FillA-D=1, Break=1, Ending1=2, Ending2=2}` bars,
   identical across **all 16** headers (verified this pass, see §3.1). A
   harmony producer that changes chords faster than one bar risks landing
   mid-section-pattern; one that changes slower than the shortest sections
   (1 bar) simply holds through them, which is correct (§3.3).
3. **Quality resolution, explicit or smart.** Every `ChordStep` either states
   its own `ChordQuality` or defers to `theory::smart_quality` (scale-degree
   -> seventh chord, D19) — there is no third state. This is a genre lever,
   not a neutral default: bare-root steps ALWAYS resolve to sevenths, so a
   style wanting clean triads (pop/rock/country/reggae/latin/basic) MUST
   state quality explicitly on every step (verified in the shipped table:
   compare `pop`'s `kMaj`-tagged steps, `default_style_progressions.hpp:56-
   60`, against `disco`'s bare `quality_ovr = -1` steps, lines 77-81).
4. **A loop or a terminal state**, and this is where the live-harmony
   contract genuinely stops being just "data": `ChordSequencer::on_tick`
   (chord_sequencer.hpp:121-155) drives playback from a fixed `m_base` tick
   captured at `kSeqPlay` time; nothing in the core ties this tick base back
   to `SectionType` transitions. **The living progression this fix delivers
   is a free-running loop, phase-independent of which style section is
   currently selected — with ONE exception the host layer adds on top: a
   LIVE style switch's own `kSeqPlay` is deliberately held back until the
   next real bar boundary (§3.0), so the loop's phase-zero lands in sync
   with the audible style morph even though `ChordSequencer` itself has no
   concept of that alignment.** That is correct for VarA-D (§3.2) and
   incomplete for Ending (§3.4) — the entire subject of §3.

Why this matters musically, restated precisely: `NoteSource::kInterval`
events (91 occurrences measured across the 16 headers this pass — e.g.
`country.hpp:149`, `"// blue b3 off the chord root"`; `bossa.hpp:72,436,476`)
encode a FIXED semitone offset from whatever `ChordState.root_pc`
`FollowedContext` is currently publishing. With zero chord events (the
pre-fix state), `root_pc` sat pinned at the static default key's tonic for
the entire session — so every `kInterval` voice in the corpus, no matter how
idiomatically placed by its style author, played the SAME fixed pitches
forever. A moving `ChordSequence` is not a decoration on top of these
devices; it is the literal input they are defined against. This is the
precise mechanism by which "0 chord events" flattens the corpus, independent
of any other style-differentiation question Ottorino has raised elsewhere.

## 2. The 16 shipped default progressions, audited style by style

`StyleFamily` (`apps/gui-sonotron/src/browser_model.hpp:234-243`) and its
16-style assignment (`kBuiltinStyleFamilies`, lines 276-293) are the exact
"pop/rock/ballad, funk/groove, dance, swing/shuffle/jazz, latin/clave, other"
grouping this task asked me to organize the audit by — it already exists,
hand-classified per `docs/proposals/style-browser-corpus-scale.md §2.1`, and
the shipped 16 progressions (`default_style_progressions.hpp`) already sit
inside it one-to-one. Audited cross-check, family by family — I looked for
any progression I would call generic/weak enough to need a concrete
replacement; I found none that clear that bar, and I say so explicitly per
style rather than only in aggregate, so the audit is falsifiable:

**kPopRockBallad** (pop, rock, ballad, country, motown — 5 styles):
- pop: I-V-vi-IV, plain triads (the "Axis"/four-chord-song shape, the
  single most-cited contemporary pop convention). **Idiomatic, no change.**
- rock: I-bVII-IV-I in **G mixolydian**, plain triads — the flat-VII vamp,
  reachable only via a mode change because `Engine::seq_add`
  (`engine.cpp:520-537`) rejects any chord whose ROOT is non-diatonic to the
  active key/mode (`WarnCode::kNotInKey`); this is a real, measured grammar
  constraint, not a taste choice (§4.3 restates it). **Idiomatic — this is
  the textbook Skynyrd/Bowie modal-rock move, genuinely well-chosen given
  the grammar ceiling, no change.**
- ballad: I-vi-ii-V, smart sevenths, 2 bars/chord (8-bar loop) — same
  turnaround family as motown but with a jazzier ii substituting for IV, and
  the doubled harmonic rhythm matching the style's own slow tempo
  (`ballad.hpp`'s `.tempo=7200` = 72 BPM). **Idiomatic, no change.**
- country: I-IV-I-V, plain triads — the Nashville three-chord shape,
  revisiting I before V. **Idiomatic, no change.**
- motown: I-vi-IV-V, smart sevenths — the doo-wop/"Stand by Me" turnaround.
  **Idiomatic, no change.**
All 5 are genuinely distinct shapes or distinct quality treatments; this
family differentiates well and needs no rework.

**kDanceFourOnFloor** (disco, house — 2 styles):
- disco: I-vi-ii-V, smart sevenths (Chic-style lush turnaround).
  **Idiomatic — see the cross-family note below for its one caveat.**
- house: i-VII in **A minor**, 2 bars/chord, static 2-chord loop — the
  documented deep-house "tonic-to-backdoor-bVII" device, not I-IV-V motion
  at all. **Idiomatic, strongly differentiated from disco by mode, chord
  count, and harmonic rhythm — no change.**

**kSwingShuffleJazz** (swing, blues, shuffle — 3 styles):
- swing: I-vi-ii-V, smart sevenths — the rhythm-changes/"I Got Rhythm"
  turnaround family. **Idiomatic on its own terms; see the cross-family
  caveat immediately below — this is the one spot my audit flags without
  recommending a rewrite.**
- blues: full 12-bar blues, ALL dominant 7ths (I7 even on the tonic — the
  single most identifying blues harmonic marker). **Idiomatic, no change.**
- shuffle: V7-IV7-I7-I7, all explicit dominant 7ths — the closing 4 bars of
  a 12-bar turnaround, looped standalone; related to blues by chord family
  but audibly distinct from it (not the full 12-bar form). **Idiomatic, no
  change.**

**The one residual soft spot my audit found, named in the shipped code's own
comment, but NOT a rewrite-worthy defect**: swing's progression is
byte-for-byte the same shape as disco's
(`default_style_progressions.hpp:86-91`, comment: *"same shape as disco, per
spec"*) — despite disco sitting in a DIFFERENT family (`kDanceFourOnFloor`
vs `kSwingShuffleJazz`). I-vi-ii-V is genuinely idiomatic to BOTH genres
independently (it is the Gershwin-era turnaround AND a very common
Motown/disco-adjacent shape) — so this is not a musicological error, and I
am NOT proposing a replacement for it: any alternate shape I could substitute
for swing (say, a bIII7-VI7-ii-V tritone-flavored turnaround) would be
LESS idiomatically swing than I-vi-ii-V is, trading a real, correct
convention for a more "different-looking" one purely for the sake of
looking different. What this DOES mean, stated plainly: the harmony layer
alone cannot distinguish disco from swing; their identity in this system
rests entirely on groove, tempo, and instrumentation (drum pattern, GM
program choice) — consistent with how these two genres actually differ in
practice (four-on-the-floor disco pulse vs. swung eighths). **Verdict: no
rewrite; the fix, if one is ever wanted, belongs to the rhythm/groove layer,
not harmony — noted for the record, not actioned here.**

**kLatinClave** (bossa, samba, reggae, latin — 4 styles):
- bossa: ii-V-I-vi, smart sevenths — Jobim's own harmonic backbone.
  **Idiomatic, no change.**
- samba: I-VI7-ii-V7 (VI7 forced dominant, a genuine secondary-dominant
  device) — forward-driving, deliberately distinct from bossa's circular
  ii-V-I even though both are commonly labeled "Brazilian". **Idiomatic and
  well-differentiated from its family sibling bossa, no change.**
- reggae: I-IV, plain triads — "two chords, infinite groove", the documented
  roots-reggae convention (harmonic interest deliberately minimal; skank +
  bassline carry identity, not chord color). **Idiomatic, no change.**
- latin: I-IV-V-IV, plain triads — a son-montuno-style 2-bar cell.
  **Idiomatic; I note with LOWER confidence than the other three that real
  montuno vamps are often even sparser (a 2-chord I-IV or I-V cell) — this
  4-chord version is a plausible, slightly richer variant, not a
  mischaracterization, so I do not recommend changing it, but I flag the
  lower-confidence genre citation honestly rather than asserting it as
  settled.**

**kFunkGroove** (funk — 1 style): static I7 vamp, one chord for the whole
2-bar loop — funk's own idiom is a static dominant/extended vamp (James
Brown "the one"), not chord motion; the groove carries the interest.
**Idiomatic and, I'd argue, the single best-judged entry in the whole
table**: it is the one style where "no progression" IS the idiomatic answer,
and the shipped functional test
(`test_default_style_progression_functional.cpp`, its own "ONE deliberate,
documented exception" comment) correctly excludes funk from the "harmony
must MOVE" assertion rather than forcing an inauthentic chord change on it
just to pass a generic invariant. No change.

**kOther** (basic — 1 style): I-IV-V-I, plain triads — deliberately the most
genre-neutral shape, matching `basic`'s own role as the unmarked/tutorial
style (not force-fit into a family it doesn't belong to,
`browser_model.hpp:270-275`'s own reasoning, which I concur with).
**Idiomatic for its own (deliberately neutral) purpose, no change.**

**Overall audit verdict**: 16 of 16 shipped progressions are genuinely
genre-grounded, not generic placeholders — I found zero that clear the bar
for "needs a concrete rewrite." The one item worth flagging (disco/swing
shape-sharing) is a real, honestly-named limitation of harmony-alone
differentiation across family boundaries, not a mistake, and I am
deliberately NOT proposing a replacement for it because every alternative I
considered would trade genre-correctness for cosmetic distinctness.

## 3. Section interaction — audited mechanism, and the one genuinely open gap

### 3.0 The live-switch bar-phase alignment mechanism — shipped, and musically correct

Worth stating explicitly, since it is easy to miss and directly answers "is
the shipped section-interaction design musically correct": a LIVE `style
switch` (while playing) does NOT re-fire the new progression immediately.
`handle_style_change_progression` (`in_process_brain_session.cpp:1157-
1178`) builds the full `key`/`seq new-or-clear`/`seq add`×N/`seq loop`/
`seq play` command batch, and — unless the switch is immediate (a fresh
`style load`, an explicit `Boundary::kImmediate`, or the transport is
stopped) — HOLDS it in `DefaultProgressionState::pending_commands` rather
than pushing it, recording the transport's current `bar_index()` as the arm
point. `release_pending_progression_if_due`
(`in_process_brain_session.cpp:1340-1349`) is polled every engine-thread
iteration (called from the drain loop, line 1497) and releases the held
batch — including its own `kSeqPlay`, which is what actually captures
`ChordSequencer`'s tick-zero `m_base` — the FIRST time `bar_index()` differs
from the armed value.

This resolves, by a host-side deferral rather than a core change, exactly
the "quantize-boundary mismatch" the prior proposal (`per-style-default-
progressions.md §3.3` item 3) flagged as an open trap: a live `style switch`
itself quantizes to the next bar (`Boundary::kNextBar`,
`in_process_brain_session.cpp:358`), and `Engine::cmd_seq` never reads
`Command::boundary` for any `seq` verb (confirmed again this pass,
`engine.cpp:446-501` has no `cmd.boundary` read) — so the ONLY way to land
the new harmony loop's phase on the same downbeat the style morph itself
targets is to delay handing `kSeqPlay` to the engine at all until that
downbeat arrives, which is precisely what `release_pending_progression_
if_due` does. **I judge this musically correct**: a live style switch
changes band AND harmony on the exact same beat, rather than the harmony
jumping early (up to a bar ahead of the audible style morph, which would
have sounded like a harmonic hiccup preceding the arrangement change rather
than arriving with it). This specific trap is closed, not merely mitigated
— my first pass at this document under-cited it; corrected here.

### 3.1 What I measured

`Engine::cmd_seq` (`engine.cpp:446-501`) handles `kSeqNew/kSeqUse/kSeqRec/
kSeqStop/kSeqAdd/kSeqLoop/kSeqPlay/kSeqTranspose/kSeqDel/kSeqClear` — **no
case reads or reacts to `SectionType` or `Param::kStyleSection` at all**, and
`Engine::cmd_style`'s own `kStyleSection` handler (confirmed via the GUI's
own comment, `in_process_brain_session.cpp` around the `kStyleSection`
encoding site) only ever changes which style-pattern variation is currently
selected — it never touches the `ChordSequencer`. I verified the bar length
of every `SectionType` across **all 16** style headers this pass (`grep -o
"SectionType::k.*\.bars = [0-9]*"` over `components/core/arrangrr/include/
arrangrr/arranger/styles/*.hpp`): every one of the 16 files carries the
IDENTICAL bar table — `Intro1=2, Intro2=1 (absent on basic/pop/rock/
ballad — 4 of 16 styles skip Intro2), VarA=2, VarB=1, VarC=1, VarD=1,
FillA=1, FillB=1, FillC=1, FillD=1, Break=1, Ending1=2, Ending2=2`. This is
uniform enough across the corpus to reason about generically, not
style-by-style.

So today: the `ChordSequencer` runs a free-running loop from the tick it was
told to `kSeqPlay` (bar-phase-aligned to a live switch per §3.0), entirely
independent of `SectionType` thereafter. A `style section ending1` command
(already used by the shipped Song-mode Ending-cue watcher,
`in_process_brain_session.cpp:1547-1564`) changes which StylePattern plays;
it does nothing to which chord is currently "up" in the progression.

### 3.2 VarA-D — hold as-is, and that is CORRECT, not an oversight

The chord track should NOT be re-triggered or reset by a VarA<->VarB<->VarC
<->VarD switch. This matches real arranger-keyboard practice (the harmony/
chord track and the currently-selected accompaniment variation are
independent axes on every arranger keyboard I know of — a performer changes
"Variation B" for a rhythmic lift without restarting the chord progression)
and matches the shipped design's own stated reasoning
(`per-style-default-progressions.md §3.4`: *"a real arranger's chord track
is independent of which style-section variation is currently playing"*),
which I re-affirm here rather than re-deriving. **No change recommended.**

### 3.3 Fill and Break — hold as-is, recommended, with lower confidence

Fills (`FillA-D`, 1 bar each) and the Break (1 bar) are short, rhythm-focused
transitional patterns — they exist to accent a section boundary or to strip
the texture down, not to carry harmonic content of their own. Two supporting
observations, both measured this pass rather than assumed:

- Every Fill/Break section is exactly 1 bar — shorter than or equal to the
  shortest chord-step duration used anywhere in the 16 progressions (also 1
  bar) — so a Fill/Break can span at most one chord of the underlying
  progression; there is no "multi-chord fill" case to design for.
- A Break's own `StylePattern` (`kBrkPatterns`) is, by genre convention,
  typically sparse or silent — if few or no notes sound during the Break,
  there is nothing audible for the underlying chord state to color one way
  or the other, making the interaction moot in practice even before any
  design decision.

I recommend leaving Fill/Break untouched by the harmony layer (status quo),
but I flag explicitly that I did NOT verify the actual note density of every
`kBrkPatterns`/`kFillXPatterns` table across all 16 styles for this pass —
that specific claim ("Break is typically sparse") is corpus-general
reasoning I hold with lower confidence than the bar-count measurement above,
which I did verify directly.

### 3.4 Ending — the one place the status-quo gap is real, three options

This is the genuine musical problem, and the one place §0's "already fixed"
claim does NOT apply. `Ending1`/`Ending2` (2 bars each, 4 bars if chained) is
meant to sound CONCLUSIVE — and the strongest way a chord progression
signals conclusion is landing on I via a real cadential approach (Perfect
Authentic Cadence, V-I or a comparable strong resolution, is the textbook
strongest way to end a musical section — general cadence theory, not
sonotron-specific). Today, whichever chord happens to be "current" in the
free-running `ChordSequencer` loop at the tick `Ending1` fires is whatever
plays under the Ending pattern — this is a matter of timing luck, not
design; unlike §3.0's live-switch case, NOTHING bar-phase-aligns Ending's own
trigger to the harmony loop.

Checking the shipped 16 progressions' own loop points (the chord immediately
before the loop wraps back to bar 1 = the tonic, in every case where the
tonic opens the loop): most already approach the tonic by IV or V (pop:
IV->I, country: V->I, disco/swing: V->I, motown: V->I, samba: V->I, reggae:
IV->I, latin: IV->I, blues/shuffle: V->I, ballad: V->I) — genuinely
cadential IF Ending happens to land exactly there. `bossa`'s own loop point
is vi->ii (its own turnaround continues past I, by design — Jobim's
ii-V-I-vi never really "stops" on I), so even lucky timing would not
resolve it. In general, Ending can be cued at ANY tick, not just the loop's
last bar (the shipped Song-mode Ending-cue watcher fires the instant a song
build finishes, `in_process_brain_session.cpp:1547-1564`, with no attempt to
phase-align to the harmony loop, unlike the style-switch case §3.0 covers)
— so relying on timing luck is not a real design, it is an unexamined
default.

Three considered options, none of which I am implementing (host plumbing is
Corelli/Nazzareno's domain per my own remit — this is the musical intent
each option buys, and its rough feasibility):

**Option A — status quo, no interaction.** Ending's own `StylePattern`
(`kEndPatterns`/`kEnd2Patterns`) plays over whatever chord is currently
active via the existing NTT `kChordTone` resolution, same as every other
section. Musical payoff: none beyond what already exists — the "resolved"
feeling depends entirely on the Ending pattern's own composed voicing (e.g.
if it lands on the chord root/octave in a low register, it reads as
conclusive-ish regardless of WHICH chord that is) and on timing luck for the
progressions that do approach I at their loop point. Cost: zero — this is
what ships today. **Feasibility: SHIPPABLE (it already is).**

**Option B — a dedicated cadence sequence, swapped in on Ending.** On
`kStyleSection` -> `Ending1`/`Ending2`, the host additionally issues
`kSeqUse` to a SECOND pre-built `ChordSequence` slot holding a short,
style-appropriate cadential tag (e.g. V-I or IV-I, sized to exactly the
Ending's own 2 or 4 bars, `loop=false` so it holds I after resolving), then
`kSeqUse`s back to the main progression's slot the next time a fresh
`VarA`/style load returns. This is the musically correct answer — it
guarantees a real cadence regardless of when Ending was cued, matching the
Perfect-Authentic-Cadence principle above, and mirrors §3.0's own proof that
"hold/defer a Command batch host-side until the right musical moment" is
already a working idiom in this codebase. Cost/mechanism: uses the EXISTING
`kSeqNew`/`kSeqUse` Command surface (`Engine::cmd_seq`, `engine.cpp:446-501`)
— no new `Param`, no ABI change — but consumes a SECOND slot out of the
bounded 16-sequence pool (`kMaxChordSequences = 16`,
`components/core/arrangrr/include/arrangrr/config.hpp:17`, "16 x 128 x 12B =
24KB") per style, and I did NOT verify whether `ChordSequencer::on_tick`'s
phase/`m_base` handling supports a clean handoff back to the main
progression's own original tick phase after the swap (a genuine open
engineering question, not something I resolved — flag for Nazzareno/
Corelli). **Feasibility: HOST-ONLY, with an unverified core-behavior
dependency that needs an engineering spike before it is safe to build.**

**Option C — author every progression's own loop point to already resolve.**
Reshape each of the 16 progressions so EVERY bar in the loop, not just the
last one, is one that would sound acceptable if Ending fired there (e.g.
collapse everything to alternating I/V). Rejected: this would flatten
exactly the harmonic variety §2 audited as already well-differentiated —
bossa's own circular turnaround, blues' 12-bar form, house's static i-VII —
none of which HAVE a "safe to stop anywhere" shape without becoming a
different, less idiomatic progression. **Feasibility: INSTRUCTIVE-BUT-
INFEASIBLE — solves the wrong problem at the cost of undoing §2's own
result.**

**My ranking**: B is the musically correct answer and I recommend it as the
target; A is an acceptable, already-shipped interim (Ending sounds
"probably fine, sometimes exactly right, sometimes not" rather than
"wrong"); C is not worth pursuing. This is a genuine product-priority call
for the owner, not something I resolve unilaterally — see §5.

## 4. The SFF-import collision, sharpened

The prior proposal already flagged that `Style`/`StyleSection`
(`style_model.hpp:150-197`) carry no harmony field by design, and that the
16-entry `kDefaultProgressions` table is HOST data, not core — I re-confirm
both facts, unchanged. Two sharper points, new to this pass:

**4.1 Index-keying, not identity-keying — which styles collide, answered
precisely: today, ALL of them, uniformly.** `default_progression_for()`
(`default_style_progressions.hpp:153-158`) is a bounds-checked lookup keyed
by the BUILT-IN style's integer index (`arrangrr::styles::kBuiltins`
position, 0-15) — any index outside that range (which is exactly what an
imported SFF/Yamaha style will produce, since it has no slot in a fixed
16-entry compile-time table) falls back to entry 0, `basic`'s I-IV-V-I. This
means, TODAY, every single one of the 1010-style validated corpus
(`docs/backlog/yamaha-style-corpus-and-rules.md`) would receive the
IDENTICAL generic fallback progression regardless of its own genre — a
cha-cha, a tango, and a K-pop dance style would all sound the same neutral
I-IV-V-I loop; there is no per-genre collision to enumerate because there is
no per-imported-style DIFFERENTIATION yet for the collision to happen
against. This is not a defect in what shipped (the import path that would
supply a REAL per-style progression, `SongModel.chords` via
`PhraseSourceHarmony::kChordSequence`, `apps/tools/arrstyle-converter/src/
model.hpp:140-167`, is itself still "partial" per `docs/DESIGN.md §9400`, so
there is currently nothing to key against) — but it is worth stating
precisely: **the real fix for imported styles is wiring `SongModel.chords`
through to the SAME `seq` verbs the built-in table already uses, keyed by
style IDENTITY (name/hash) not by a fixed built-in index** — extending
`kDefaultProgressions` itself will never scale past the 16 built-ins no
matter how large the table grows, by construction.

**4.2 The "keeps HARMONY" doctrine tension remains fully open, unchanged.**
`Engine::cmd_style`'s `kStyleLoad` handler carries the verbatim comment
(`engine.cpp:614-634` area, cited in the prior proposal): *"loading a style
changes the BAND, keeps the HARMONY"* — `establish_default()` only seeds the
home key when nothing explicit is already in force. Auto re-injecting a
progression on every style load/switch (what ships today) is, literally,
the opposite of "keeps the HARMONY". I re-verified this pass that this is
STILL a live tension, not resolved by anything shipped since: I checked
every panel file under `apps/gui-sonotron/src/*panel*.cpp` and confirmed
none of them send `kKeySet` or any `kSeq*` command — there is still no live
chord/key GUI surface today, so the auto-injection is currently harmless
(nothing genuine to "keep" yet), but the fork is real and dated: the day a
live chord panel ships, blind re-injection on every style switch will
silently clobber a user's live-played chord. This requires either (a)
accepting that regression as a known cost, (b) gating on
`ChordEngine::explicit_set()` (which needs a new engine-thread-reachable
readback — a real, small, core/ABI change), or (c) applying the default
progression only once per session. Same three-way fork the prior proposal
named; still unresolved; still not mine to decide.

## 5. What needs deciding / what I flagged and refuse to decide alone

1. **Ending's harmonic resolution (§3.4)** — Option A (status quo, shipped)
   vs. Option B (dedicated cadence sequence, musically correct, needs an
   engineering spike on `ChordSequencer` phase handoff before Nazzareno can
   safely scope it) vs. Option C (rejected). I recommend targeting B but
   this is a genuine priority call: is an occasionally-unresolved Ending
   worth fixing now, or is it a fine deferred polish item behind other
   Phase-7 work?
2. **The re-apply-on-switch / "keeps HARMONY" fork (§4.2)** — unchanged from
   the prior proposal, restated because it is still open: always re-apply
   (current), apply once per session, or gate on `explicit_set()` (needs a
   small core/ABI change). Still the owner's call, still contingent on
   whether/when a live chord panel ships.
3. **Whether to invest in identity-keyed (not index-keyed) default
   progressions ahead of the SFF import path actually being wired (§4.1)** —
   building this now would be speculative (the import path that would
   consume it is itself still partial); I flag it as the eventual right
   answer, not a near-term ask.
4. **Nothing in this document requires a new host or core dependency.** Both
   options considered in §3.4 reuse the existing `seq`/`key` Command surface;
   no new `Param`, no new ABI entry, no third-party library.

## Appendix: files read/measured for this pass

`docs/proposals/per-style-default-progressions.md`,
`apps/gui-sonotron/src/default_style_progressions.hpp`,
`apps/gui-sonotron/src/in_process_brain_session.cpp` (lines 1000-1360,
1440-1570 read in full this pass, including `handle_style_change_
progression` and `release_pending_progression_if_due` in full),
`apps/gui-sonotron/tests/test_default_style_progression_functional.cpp`
(header comment read in full),
`apps/gui-sonotron/src/browser_model.hpp:225-294`,
`components/core/arrangrr/include/arrangrr/chord/{chord_sequence,
chord_sequencer,chord_engine}.hpp` (read in full),
`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:1-50`
(`SectionType` and helpers),
`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (all 16 —
grepped for `SectionType::k*.bars` and `NoteSource::kInterval` this pass),
`components/core/arrangrr/include/arrangrr/config.hpp:17`,
`components/core/arrangrr/src/engine.cpp` (lines 240-560 read in full this
pass), `docs/DESIGN.md` (D28 area, §9400 area), git log for
`apps/gui-sonotron/src/in_process_brain_session.cpp` and the proposal doc
(commits `5aed414`, `7f07a6f` inspected via `git show --stat`).

# Style section depth: why arranged sections feel short/thin, and options to fix it

Owner-observed problem (2026-07-16, gui-sonotron Repeat Zone): playing a style, each
scene/section is only ~2-3 seconds and feels far too short for e.g. an Intro. This
document is the DEPTH track (owner task #8): why that happens, root-caused against
the actual style data and the arranger's playback model, and a set of options —
ranked, costed against the no-heap dual-target core, honestly labelled — to give
sections real musical length and development.

**Relationship to the shallow fix.** A separate, already-in-progress GUI change
assigns a distinct `SectionType` to each of the 5 Repeat-Zone columns (col0=Intro1,
col1=VarA, col2=VarB, col3=VarC, col4=VarD), so switching columns at least changes
*which* pattern plays. That fix is necessary but insufficient: it changes *which*
1-bar loop you hear, not the fact that every one of those loops is a single bar that
repeats itself note-for-note, forever, with no development. This document is about
that second problem — depth, not just variety of pattern.

---

## 1. What I measured

### 1.1 Every section in the corpus is authored as exactly 1 bar

```
components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp
```

Every `StyleSection` literal in all 16 built-in style headers sets `.bars=1`.
Grepping `\.bars=[0-9]*` across all 16 files: **198 occurrences, 0 of them not equal
to 1.** Representative citations: `basic.hpp:249`, `ballad.hpp:252`, `funk.hpp:200`,
`reggae.hpp:95`, `latin.hpp:147`, `blues.hpp:122` — and the same pattern in every
other file. This is not a sampling claim; it is the full corpus.

### 1.2 What "1 bar" means in seconds, per style (this IS the "2-3 seconds" complaint)

`Style::tempo` (`BpmX100`) and the default `beats_per_bar=4` (no built-in overrides
it — confirmed by grep; `kBeatsPerBar=4` in `components/core/common/include/common/
time.hpp:27`) give an exact bar duration = `4 * 60 / bpm` seconds:

| style | tempo (bpm) | seconds/bar (= seconds/section as authored) |
|---|---:|---:|
| latin | 180.00 | **1.33** |
| swing | 140.00 | 1.71 |
| house | 128.00 | 1.88 |
| shuffle / rock / bossa | 130.00 | 1.85 |
| disco | 122.00 | 1.97 |
| motown | 124.00 | 1.94 |
| basic / pop / country | 120.00 | 2.00 |
| funk | 108.00 | 2.22 |
| samba | 104.00 | 2.31 |
| reggae | 75.00 | 3.20 |
| ballad | 72.00 | 3.33 |
| blues | 66.00 | 3.64 |

Every single built-in section, as authored, is 1.3–3.6 seconds long. The owner's
"~2-3 seconds" is not an impression — it is the exact, measured loop length of every
section in the corpus. `latin` is the extreme case at 1.33s.

### 1.3 The arranger ALREADY supports genuinely multi-bar sections, correctly, and this is proven by a regression test — the gap is 100% authoring, 0% engine

`StyleSection.bars` (`style_model.hpp:171`) already drives the section length:
`Arranger::on_tick` computes `len = section->bars * ticks_per_bar`
(`arranger.hpp:444`), and — critically — a **mid-section** bar boundary does *not*
reset the section clock; only true `section_end` (or a section/style change) resets
`m_section_start` (`arranger.hpp:487-489`). `StyleEvent.step` is a `uint16_t`
documented as "16th-grid position **within the section**" (`style_model.hpp:89`,
verbatim), not within the bar — and `on_tick` computes `step` as
`rel / kTicksPerStep` where `rel` grows across the *whole* multi-bar section
(`arranger.hpp:516-517`), so an author can already place bar-2 content at
`step=16..31`, bar-3 at `step=32..47`, etc., inside the *existing* `StyleEvent`
struct — no ABI change, no new field.

This is not a theoretical reading of the code: it is pinned by
`test_multibar_section_plays_bar_two` (`components/core/arrangrr/tests/
test_arranger.cpp:668-710`), whose own comment records that this used to be broken
("the review found bars 2..N were unreachable because the section clock restarted
at every bar boundary") and is now a fixed, tested invariant — a 2-bar section with
one event at `step=0` and a distinct one at `step=16` correctly fires both, on both
loop passes, over a 4-bar run. **The mechanism a "genuinely multi-bar Intro/Main"
proposal needs already exists, already works, and is already regression-tested.**
The only reason no built-in uses it is that all 16 were hand-authored at `bars=1`.

### 1.4 The corpus already has a per-repeat variation engine (roadmap 9210) — and it touches almost nothing

`arrangrr/arranger/motif.hpp` implements a full call-and-response engine: a
`StylePattern` can carry a `MotifSpec*` (`style_model.hpp:136-167`); on each section
repeat, the arranger's own repeat counter (`m_motif_repeat`, incremented once per
bar-loop-back in `arranger.hpp:480`) makes **even repeats play the seed pattern
verbatim and odd repeats apply a deterministic transform** (diatonic transpose,
retrograde, or displacement — `motif.hpp:350-366`). This is exactly the "arranger
plays a 1-bar pattern for N bars applying per-bar variation" idea from the task
brief — **it is not a proposal, it is shipped code.**

But its adoption is narrow. Counting `.motif=&...` (non-null) assignments across all
16 style headers: **112 of 798 total `StylePattern` entries (14%)** carry a
`MotifSpec`, and every single one is on `TrackRole::kBass` (89), `kArp` (8), or
`kLead` (15). **Zero** are on `kDrums`, `kChord1`, `kChord2`, `kPad`, or `kPerc` —
even though `motif::retrograde`/`motif::displace` are explicitly valid for `kFixed`
(drum) events too (`motif.hpp:253,264`, doc comment: "Valid for every NoteSource,
including kFixed"). The roles that carry the most events and the most identity of a
section — the drum groove and the chordal comp — are the roles that **never** vary,
bar to bar, in the entire corpus.

### 1.5 Zero humanization anywhere — every un-motif'd repeat is byte-for-byte identical

`GrooveParams` (`arrangrr/arranger/groove.hpp:21-29`) has `humanize_timing` and
`humanize_velocity` fields (deterministic, seeded wobble) exactly for this purpose.
Grepping the 16 style headers for `humanize`: **0 matches.** Every style — including
the 3 that do set `.groove={...}` for swing (blues/shuffle/swing) — leaves both
humanize fields at their struct default of 0. Combined with §1.4: for 686 of 798
`StylePattern`s (86%), the exact same MIDI bytes fire on loop 1, loop 2, loop 8, loop
80 of a section. There is no ghost-note jitter, no velocity breathing, nothing a real
arranger-workstation groove engine layers on top of a short loop to keep it alive.

### 1.6 The GUI already has an orthogonal "scene length" knob, and it does not fix this

`apps/gui-sonotron/src/grid_model.hpp:129-149`: `GridModel::scene_bars` is a
**host-side, GUI-only** per-scene length (`kDefaultSceneBars = 8`, line 147) used by
`next_scene_to_launch`/auto-song to decide when to advance to the next column. Its
own doc comment explains the split precisely: "the advance decision ... must use ...
the SAME per-scene length, **not the style's own section length**, which is what the
pre-fix code used and why the sprint was locked to 1 bar per scene for every
built-in style." This is a real, already-shipped fix for a DIFFERENT problem (how
long a *column* holds the floor before auto-song advances). It does not touch what
happens musically *inside* those 8 bars: with §1.4/§1.5 as measured, 8 bars of
`scene_bars` on a drum/comp-only section is the same 1-bar loop played 8 times,
identically. Scene length (GUI) and section depth (engine/data) are two different
axes; only the second is this proposal's subject.

### 1.7 The tooling to build genuinely multi-bar sections from real source material already exists

`apps/tools/arrstyle-converter` computes `bars` from real content, not a hardcoded
1: `midi_import.cpp:134-141` derives `bars` from `total_ticks / ticks_per_bar` of an
imported MIDI file; `sff_import.cpp:173-181` (`bars_for_span`) does the same from a
decoded Yamaha SFF section span; `validate.cpp:97-99` rejects a non-positive `bars`.
The corpus's 1-bar-everywhere state is a hand-authoring choice for the 16 built-ins,
not a limit of the model or the toolchain — the importer path (inspect-only for SFF
decode today, per `docs/style-corpus-and-generation.md` §6) is the route to
multi-bar content at scale without hand-writing every bar in C++.

### 1.8 Real arranger-workstation convention (researched, not assumed)

Cross-checked against Yamaha style-file documentation and `docs/style-corpus-and-
generation.md`'s own measured 1010-file SFF corpus (§6.1/§6.4, U9/U14): **Fill-Ins
are conventionally 1 bar** (the corpus is correct here — not a bug); **Main
sections commonly loop a 1-2 bar unit**, so "make every Main 8 bars" is not itself
the genre-correct fix; **Intros are conventionally ~2 bars**, sometimes over a fixed
(non-chord-follow) progression. So the length gap is real but narrower than "every
section is too short": it is concentrated in (a) Intro being 1 bar against a ~2-bar
convention, and, more importantly per §1.4/§1.5, (b) the *absence of any bar-to-bar
articulation change* that real hardware supplies (ghost notes, humanize, the
Yamaha-native call-and-response the SFF format itself encodes) even when the loop
unit stays short.

Sources: [JJazzLab — Yamaha styles](https://jjazzlab.gitbook.io/user-guide/rhythm-engines/yamjjazz-rhythm-engine/yamaha-styles), [YamahaMusicians.com — How to make your own style?](https://yamahamusicians.com/forum/viewtopic.php?t=15123), [Style Files — Introduction and Details, Wierzba/Bedesem](https://wierzba.hier-im-netz.de/stylefiles_v101.pdf)

---

## 2. Diagnosis

The "too short/thin" perception has three separate, additive causes, in order of
how much of the effect each explains:

1. **No bar-to-bar development on the roles that carry the section's identity
   (§1.4/§1.5).** Drums and comping — 86% of all patterns — are dead-identical on
   every repeat, forever, in every one of the 16 styles. This is the dominant cause:
   even at the GUI's own 8-bar `scene_bars` default (§1.6), the listener hears the
   same 1.3-3.6-second cell 8 times with zero variation. UNDERUSE, not a model gap:
   the motif engine (§1.4) and the humanize fields (§1.5) already exist and already
   apply to these roles; they are simply never authored onto them.
2. **The loop unit itself is short (§1.2), and for one section type — Intro — that
   is genuinely shorter than genre convention (§1.8).** This is a real, if smaller,
   gap: about 2 bars is idiomatic for an Intro; the corpus's Intro1/Intro2 are 1 bar.
   Main/Var sections being 1-2 bars is, by contrast, NOT itself off-convention.
3. **The engine change needed for (2) is zero — it already shipped and is
   regression-tested (§1.3).** The only real cost of authoring longer sections is
   hand-writing (or importing) the additional bars of content; there is no
   engineering unknown here.

The model itself is not the amputation this time (contrast with the general
same-y-ness diagnosis in `docs/style-corpus-and-generation.md` §2, which is about
comping/melodic VOCABULARY poverty). Here the gap is almost entirely **authoring
underuse of infrastructure that already exists**: `StyleSection.bars` beyond 1,
`MotifSpec` beyond bass/arp/lead, and `GrooveParams.humanize_*` beyond 0.

---

## 3. Options (ranked, with tradeoffs)

### Option A — Extend the 9210 motif engine to drums and comping roles
**SHIPPABLE — zero new engine code, pure authoring.**
Add `.motif=&someDrumMotif` (using `motif::from_span` on the pattern's own existing
`events`, i.e. Ottorino's already-built "Option 1") with `MotifTransform::
kDisplacement` or `kRetrograde` to the `kDrums`/`kChord1`/`kChord2`/`kPad` patterns
of the Main/Var sections. `kDisplacement`/`kRetrograde` are valid for `kFixed`
events today (§1.4). Every OTHER loop of a section then plays a rhythmically shifted
or reflected variant of the same idiom instead of the identical bytes — genuinely
audible, deterministic (D16 seeded), costs a `MotifSpec` constant (a handful of
bytes) per pattern, no ABI change. **Payoff:** the single cheapest, highest
leverage-per-byte fix; directly answers "why does it feel thin" without touching
`bars` or flash budget at all. **Cost:** authoring effort only — someone has to
choose a musically sound transform + amount per pattern per style (~16 styles × a
handful of Main/Var patterns each); no code review risk beyond data.
**Caveat:** call-and-response is a *binary* alternation (statement/answer), not an
evolving arc — see Option E for the richer version.

### Option B — Turn on `humanize_timing`/`humanize_velocity` per style
**SHIPPABLE — zero new engine code, pure data (one `GrooveParams` field each).**
`groove::apply` already reads these fields (`groove.hpp`); every built-in currently
sets them to 0 (§1.5). Setting small, genre-appropriate values (2-6% per U11's own
convention in `docs/style-corpus-and-generation.md` §6.4) makes every repeat
audibly non-identical — the deterministic "same seed ⇒ same feel" property is kept
(D16), it just stops being "same seed ⇒ same BYTES." **Payoff:** cheap, orthogonal
to Option A, covers every role including ones no motif ever will (Chord2 stabs,
Pad long tones). **Cost:** near zero — a few struct-literal edits; golden tests that
pin exact tick/velocity output on these styles WILL churn (the same flag already
raised for the swing/shuffle/blues re-authoring in the corpus doc) and must be
regenerated deliberately, not silently. **Flag:** confirm with the golden-test owner
before landing — this is intentional, bounded churn, not a regression.

### Option C — Author genuinely multi-bar Main/Var sections with per-bar-distinct content
**SHIPPABLE — the engine mechanism is proven (§1.3); the cost is authoring + a small,
honestly bounded flash increase.**
Use the existing, tested `bars > 1` + `step >= 16` mechanism to write 2-4 bar Main
sections where each bar is a hand-distinct idea (a real arc: sparser bar 1, denser
bar 2, a turnaround into bar 3/4) instead of a repeated 1-bar cell. **Payoff:**
the most "musically resembles a real arranger workstation" option — real
development inside a Main, not just alternation. **Cost — quantified, not
hand-waved:** the corpus today is ~3.7-3.8K note events total (`≈37-38 KB` at the
pinned 10 bytes/`StyleEvent`, `style_model.hpp:105`) against an STM32H743 target
with **2 MB flash** (D33, `DESIGN.md:587`). Even a 4x growth of every Main/Var
section across all 16 styles (leaving Fills/Endings at their genre-correct 1 bar,
§1.8) lands around 150 KB — under 10% of flash, not a binding constraint. The real
cost is **authoring effort**: hand-writing musically distinct bars for ~4 Var
sections × 16 styles is a large task; the `arrstyle-converter` import path (§1.7)
is the way to do this at scale from real MIDI/SFF source rather than by hand.
**Recommendation within C:** prioritize Intro1/Intro2 to ~2 bars first (the one
place §1.8 shows a real convention gap), then VarA (the most-heard section), rather
than doing all 16 styles × all sections in one pass.

### Option D — A song-form / arrangement sequencing layer
**PARTIALLY SHIPPED already (GUI-side), and this is the one place I disagree with
treating it as a fresh proposal.**
`GridModel::scene_bars`/`next_scene_to_launch` (§1.6) already implements "play this
scene for K bars, then auto-advance" at the host layer, with no core/ABI change,
defaulting to 8 bars. It is a real, useful lever, but — per §1.6 and the diagnosis
in §2 — it does not by itself fix the depth problem, because it operates on TOP of
the same 1-bar loop underneath. **Recommendation:** do not build a second song-form
mechanism; it exists. If it is not yet wired into the currently-shipping Repeat Zone
build, that is a wiring/UI task (HOST-ONLY, GUI layer), not a new design — combine
it with A/B/C above rather than treating length as solved by scene count alone.

### Option E — A richer, N-state per-repeat evolution (beyond binary call-and-response)
**INSTRUCTIVE-BUT-INFEASIBLE as a drop-in today; SHIPPABLE in spirit, needs a small,
real design decision first.**
The 9210 motif engine's `apply_repeat` is binary (even=seed, odd=transform,
`motif.hpp:358-366`). A genuine "density ladder across N repeats" (bar 1 sparse →
bar N dense, mirroring U9's Main A→D energy convention) would need either (a)
authoring that ladder directly as a real multi-bar section (Option C — already
sufficient, no new mechanism), or (b) extending `m_motif_repeat`'s consumer to pick
from more than one transform/amount per repeat count (e.g. `repeat % 4` instead of
`repeat % 2`). (b) is a small, bounded, no-heap, dual-target-safe change (a few
lines in `motif::apply_repeat` plus richer `MotifSpec` fields) — genuinely
SHIPPABLE, not infeasible — but it IS a core-touching change, unlike A/B/C which are
pure data. I flag it as a separate, smaller follow-on engine task, not a
day-one fix, because Option C already reaches the same musical outcome with zero
engine risk.

### Option F — ML/statistical groove or arrangement generation
**INSTRUCTIVE-BUT-INFEASIBLE on device; HOST-ONLY with a flagged dependency, if ever.**
Not needed here — nothing in this specific complaint (short/thin sections) requires
a learned model; A-C already close the gap with existing, dependency-free
mechanisms. Not recommending it for this problem; noting it only so it isn't
silently reached for. Any ML runtime/weights would be a new core/host dependency
requiring explicit owner approval (never assumed).

---

## 4. Recommendation, in priority order

1. **B then A first** — turn on small `humanize_timing/velocity` values (§B) and
   extend `MotifSpec` coverage onto drum/comp roles for the Main/Var sections of a
   few styles (§A). Zero engine code, zero ABI change, immediately audible, and the
   two together directly attack the dominant cause (§2.1) — 86% of patterns being
   byte-identical on every repeat. Expect bounded, deliberate golden-test churn;
   flag it to whoever owns the golden harness before landing, do not surprise them.
2. **C, scoped to Intro (→2 bars) and VarA first**, using the proven `bars>1` +
   `step≥16` mechanism (§1.3) — the one place genre convention (§1.8) shows a real
   length gap, and the most-heard section. Treat "all 16 styles × all sections" as
   a later, larger pass, ideally via the `arrstyle-converter` import path (§1.7)
   once the SFF CASM decode (already scoped in `docs/style-corpus-and-generation.md`
   §6.3) lands, rather than by further hand-authoring C++ tables.
3. **Confirm D is already wired** into the shipping Repeat Zone rather than
   re-designing it; it solves a real but different problem (scene-hold length) and
   should be combined with, not substituted for, 1-2 above.
4. **E as a deliberate, small, later core change** if the owner wants a genuine
   density-ladder arc beyond binary call-and-response — scoped separately because
   it touches `motif.hpp`, unlike 1-3 which are pure `constexpr` data edits.

## 5. What needs an owner decision / what I flag

- **Golden-test churn from B and C is real and must be pre-approved**, not
  discovered as a diff — same discipline already established for the 9100 swing
  re-authoring in `docs/style-corpus-and-generation.md` §4.
- **How much hand-authoring effort to spend on Option C vs waiting for the SFF
  importer (§1.7)** is a scheduling call: hand-authoring 2-4 bar Mains for 16 styles
  now vs finishing the CASM→NTT decode (scoped, no new dependency, host-only per the
  existing corpus doc) and importing real multi-bar Yamaha content at scale later.
  I do not have enough information to make that scheduling tradeoff for the owner.
- **Option E is a real, if small, core change** (touches `arrangrr/arranger/
  motif.hpp`) — flagging it explicitly as NOT a pure-data option like A-C, so it
  gets the review a core change deserves rather than being nodded through as
  "just more style data."
- No option here requires a new dependency; nothing is flagged as needing
  external approval beyond the golden-test-churn sign-off above.

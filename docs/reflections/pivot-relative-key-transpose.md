# Reflection — Pivot: live chord press as a relative transpose of a running chord loop (PARKED)

Feature name: **Pivot** (a played gesture that pivots a running progression to a
new key — the pivot-chord modulation device, made live). Working name; the owner
may veto.

Status: **PARKED**. The owner chose Path 1 (plain-arranger literal chord-follow)
for the JAM that ships now. This document is the proper, cited treatment of the
intellectual question he is still asking, for when he revisits the feature. It
judges the model as a musician, situates it against how commercial arrangers
actually behave (researched and cited, not asserted), and — because he fully
specified it — records the implementable shape so a future build starts from a
settled design rather than a fresh argument.

Filename follows the `docs/reflections/` convention (kebab-case topic slug).

## Sul tavolo (the reflection as understood — final form)

A running chord loop plays one chord per bar. On a live chord press, the pressed
root defines an ABSOLUTE global transpose measured against the ORIGINAL chord at
the currently-playing bar position — never cumulative:

`offset = pressed_root - original_root[P] (mod 12)`, recomputed from the
untouched original progression on every press.

The owner's locked answers:
1. **Absolute, from the original**, recomputed each press (example: original
   `C A F G`; at the C bar press D -> offset +2 -> `D B G A`; later at the bar
   whose original is C press C -> offset 0 -> back to `C A F G`).
2. **Root-only**: each original chord keeps its stored quality
   (`C A(min) F G` +2 -> `D B(min) G A`). The played chord's quality does not
   propagate. Open sub-case (deferred): a press whose quality differs from the
   scheduled chord uses only its root; its quality is ignored or localized.
3. **Persists** across loop wraps forever, until the next press.
4. **Reset** = press the original chord of the current bar (offset 0 falls out of
   rule 1 arithmetically).

**Fact correction that reframes everything** (confirmed against the code): the
`C A F G` loop is NOT the style. The `basic` style has no progression of its own —
its patterns re-root to whatever is in the followed context. The loop is a
SEPARATE, OPTIONAL `ChordSequencer` (the jam's `setup.acmd` runs `seq new / seq
add / seq loop on / seq play`). Torquato measured that WITHOUT a sequencer running,
a live chord already persists across every bar — the plain-arranger "press a chord
and the band holds it" behavior already works. A live chord is clobbered ONLY when
the competing `ChordSequencer` fires under the default `kAuto` gate. So this
feature is not a bug fix and not the style's job; it is an OPTIONAL performance
layer: a backing loop you can live-modulate by interval.

**The owner's unification insight to test**: "no progression" == a one-chord
progression `[X]`; pressing D sets an offset and the band holds D. So live steering
with and without a sequence would be ONE mechanism — a global transpose offset over
the current chord source.

---

## How commercial arrangers actually do it (researched)

The working belief is **confirmed**. Across Yamaha, Korg, and Roland the live
harmonic controls are three SEPARATE mechanisms; none fuses them the way this idea
does.

1. **Literal chord detection.** Every fingering mode is WYSIWYG: the chord you
   play is the chord the band plays. Yamaha offers Single Finger, Fingered, Full
   Keyboard, AI Fingered, AI Full Keyboard and Multi Finger — all detect a chord
   and the accompaniment plays *that* chord; AI modes only improve anticipation of
   the *next* chord, they do not transpose a stored loop
   ([psrtutorial](https://psrtutorial.com/lessons/start/s50_fingering.html),
   [Yamaha FAQ](https://faq.yamaha.com/usa/s/article/U0002033),
   [ePianos Tyros 5 guide](https://www.epianos.co.uk/tyros-5-fingering-styles-guide/)).
   Roland's E-A7 is identical: "the accompaniment changes according to the chords
   that you play"
   ([Roland E-A7 manual](https://www.manualslib.com/manual/1062762/Roland-E-A7.html?page=15)).
   Korg's Pa chord recognition ranges from single-finger to an Expert mode modelled
   on jazz pianists — still literal detection
   ([Korg Pa4X](https://www.korg.com/us/products/synthesizers/pa4x/page_2.php)).

2. **A separate recorded chord loop for hands-free playback.** Yamaha's **Chord
   Looper** records your left-hand chords and loops them "for the Style engine to
   loop, freeing you up to experiment with two hands or soloing without worrying
   about playing the changes"
   ([Sand, software and sound](https://sandsoftwaresound.net/step-edit-chord-looper/),
   [Yamaha Genos2](https://usa.yamaha.com/products/musical_instruments/keyboards/arranger_workstations/genos2/index.html)).
   Korg's **Chord Sequencer** does the same: "record and loop your chord
   progressions on the fly... freeing up your left hand while performing live"
   ([Korg Pa1000 guide](https://manualzz.com/doc/o/12vopf/korg-pa1000-guide-the-chord-sequencer)).
   While the loop plays, the loop drives the harmony — it is not a target you
   re-key by pressing chords over it.

3. **A separate, context-free global Transpose.** Yamaha's TRANSPOSE buttons shift
   overall pitch in semitone steps — keyboard, Style, and Song together — as an
   independent function from the Chord Looper
   ([Yamaha Genos reference, transpose](https://www.manualslib.com/manual/1333828/Yamaha-Genos.html?page=36)).

**Conclusion: no mainstream arranger lets you press a chord to RELATIVELY
transpose a running recorded progression.** They give you literal chord control,
OR a hands-free recorded loop, OR a numeric transpose button — three distinct
gestures. The owner's model is a genuine **novel hybrid**: it fuses the Chord
Looper (mechanism 2) and the relative Transpose (mechanism 3) into a single
gesture, using the chord press to supply the transpose interval.

---

## Is the model musically sensible? (a musician's judgment)

Mostly yes, with one soft spot.

- **Absolute-from-original is the musically correct choice.** "Put the song in D"
  yields the same key regardless of how you got there; there is no accumulating
  error, no path dependence. A performer thinks in absolute keys, not in a running
  sum of intervals. Correct.
- **Root-only, quality-preserving is correct for what this is — a key change**
  (elaborated below). It keeps the progression's identity intact. Correct.
- **Persist-until-next-press is correct** for a sustained modulation (lift the last
  chorus a tone and stay there). Correct.
- **Reset-by-pressing-the-original-chord is arithmetically sound but ergonomically
  thin.** It falls out of rule 1 (press `original_root[P]` -> offset 0), but it
  asks the player to remember the ORIGINAL chord of the CURRENT bar after the
  display already shows the transposed chord — and with a moving loop that original
  is not necessarily the tonic. Original `C A F G` transposed +2 shows `D B G A`; to
  come home while sitting on the second bar you must press A (its original), not C.
  Musicians intuit "play the tonic to come home," not "play whatever chord was
  originally in this specific bar." A context-free home (a Transpose-reset, like the
  commercial button) is clearer. Keep the gesture; add a backstop.

The deeper musical caveat is the gesture itself. A chord press is the most
overloaded action on any arranger, and everywhere else in the world it means "play
THIS chord, literally." Repurposing it to mean "compute an interval and silently
re-key four bars" is heavier: one press, global consequence you cannot see coming.
It is a valid MODULATION tool, but it collides with deep muscle memory, so it
belongs behind an explicitly-armed "modulate" mode, never as the default meaning of
a press.

---

## Situating the hybrid: the UX trade

Novel, and internally sound — but it trades away the one thing the commercial model
guarantees.

- **Commercial literal-chord model:** maximal predictability and immediacy —
  WYSIWYG, no hidden global state, the chord you play is the chord you hear. To
  re-key the whole song you reach for a separate, context-free Transpose (+/-
  semitone) button. Cost: your hands leave the keys for a mode control, and the
  transpose is a number, not a musical target.
- **Owner's hybrid:** hands-free, in-tempo modulation played from the keys by ear —
  press the chord you want to hear and the running loop follows in that key. Cost:
  hidden global consequence; the "what am I controlling" ambiguity; and it CANNOT
  simultaneously be your literal chord-change gesture (you can't press Dm to mean
  both "a passing Dm here" and "re-key everything to D").
- **A subtle asymmetry:** because the interval is measured against the CURRENT
  bar's original chord, the SAME press produces a different modulation depending on
  where in the loop you press it. To reliably go "up a tone" you must press a chord
  a tone above whatever the current bar is — which means knowing the current chord.
  A Transpose +2 button is context-free and needs no such knowledge. So the hybrid
  swaps the button's context-free reliability for the expressiveness of playing the
  target chord directly.

**When each serves the player.** The literal model wins for comping/jamming, where
direct chord control matters and key changes are occasional and deliberate. The
hybrid wins for a solo performer running a fixed backing loop who wants to modulate
live, in time, without stopping — essentially making the Transpose button *playable
as a chord*. That is a real but niche gesture. It earns its place as an option, not
as a default.

---

## Root-only vs quality-propagation (in musical terms)

This is the cleanest ruling in the whole design.

- **Root-only = a KEY CHANGE.** It preserves the progression's HARMONIC FUNCTION:
  I-vi-IV-V stays I-vi-IV-V, only re-keyed. The vi is still a minor sixth, the V
  still a dominant, the voice-leading contour is intact. This is exactly what
  "transpose the song up a tone" MEANS. It is the right and only choice for a
  feature whose identity is "same song, new key."
- **Quality-propagation = a MOOD-MORPH that destroys identity.** If the played
  chord's quality overrode the whole loop, C-Am-F-G (a bright I-vi-IV-V turnaround)
  under a minor press becomes Cm-Am-Fm-Gm — no longer the same progression, no
  longer a coherent diatonic set, its harmonic contour flattened to a single color.
  That is not a transposition; it is an erasure. It would be a *different* feature,
  and a musically incoherent one.

The only defensible use of the played quality is LOCAL: color the CURRENT bar only
(a one-off substitution), which is a separate gesture and must not leak into the
transpose. Root-only is right.

---

## Tenere / Rilavorare / Buttare

### Engineering axis (Asse ingegneristico)

**Tenere**
- *Absolute-from-original is the superior formulation.* Referencing the immutable
  stored `ChordSequence` (offset = pressed − original_root[P]) is stateless with
  respect to history: no cumulative term to read, no drift, reset falls out
  arithmetically. It reduces to a single `std::int8_t` applied `% 12` — bounded,
  heap-free, one add+modulo folded into `ChordSequencer::on_tick`, byte-identical
  on host and `arm-none-eabi`. A textbook fit for D32/D33 and for D28's existing
  functional transpose.
- *It dissolves the `kAuto` last-writer race structurally.* Route the press into
  the offset instead of into the followed context, and the `ChordSequencer` stays
  the SOLE steering writer of `current` each bar (now deriving root from
  original[P]+offset). One writer per bar -> no race to arbitrate. This is a
  cleaner shape than the D47 gate deciding a winner.

**Rilavorare**
- *The delta reference must be the ACTIVE step at position P, not the step whose
  start == pos.* Mid-bar presses land between step starts; `on_tick` only matches
  exact starts, so a separate `current_step_at(pos)` lookup (bounded over
  `<= kMaxChordSteps`) is needed. And the boundary tie-break — a press exactly on a
  downbeat reads the step active AFTER that bar's fire — must be specified. A
  scheduler tie-break IS groove; leave it undefined and the feel is nondeterministic.
- *Lifecycle of the offset must be defined.* `seq new` / `seq use` / `key set` /
  transport restart should reset offset to 0 (a new song is in its written key).
  "Persists forever" is about loop WRAPS, not session boundaries — flag the
  distinction to the owner.

**Buttare**
- Nothing. The engineering is sound; the faults above are reworks, not deletions.

### Musical axis (Asse musicale)

**Tenere**
- *Absolute-from-original, root-only, persist-until-press* — each is the musically
  right choice, justified above.

**Rilavorare**
- *Reset-by-press-original* — sound as math, thin as ergonomics (you must recall the
  current bar's original chord after the display has changed). Add a context-free
  home (a `seq home` / Transpose-reset) and, ideally, a UI readout of the original
  progression. Do not remove the owner's gesture; back it up.
- *The overloaded-gesture problem* — put transpose behind an explicitly-armed mode,
  never the default meaning of a press.

**Buttare**
- *The premise that a chord press should be the default whole-song re-key.* Every
  researched arranger keeps literal chord-follow as the default and puts whole-song
  transposition on a separate control. The owner already conceded this by choosing
  Path 1 for the jam; the transpose belongs as an opt-in layer, not the default.

---

## The unification insight: real on root, seam on quality

The owner's "no progression == one-chord progression" is **genuinely true on the
root axis**. With a one-cell source, `original_root[P]` is constant, so
`offset = pressed − constant` makes the band hold exactly the pressed root — which
IS the plain latched hold. One offset scalar, one code path, one steering writer.
Adopt it.

It is **NOT clean on quality**, and this is the seam to name explicitly. In the
degenerate one-cell case the player expects their PLAYED quality to sound and
persist (press Dm7 -> band holds Dm7 — the literal-chord-follow the jam ships). But
root-only-preserve-original would transpose the *home chord's* quality and give
D-major, not Dm7. So quality does not unify:

- **Single-cell source:** the press defines root AND quality of the held chord
  (literal follow).
- **Multi-cell recorded source:** the press supplies root only (transpose);
  each recorded cell keeps its own quality.

The reconciliation that keeps ONE mechanism: the live press always SOUNDS the
pressed chord in full (root+quality) for the current bar at the instant of press
(this is just the normal live-sound path), and sets the global root offset for
subsequent bars. With a one-cell source every bar is "the current bar," so the
played quality persists — latched follow recovered — with no persistent per-bar
quality state. With a recorded loop, subsequent bars (and the next wrap of P) fire
their own recorded qualities on transposed roots. Same code, one documented
quality-locality rule.

---

## Verdetto

A novel, internally coherent hybrid that no commercial arranger offers: it fuses
the Chord Looper and the Transpose button into one played gesture. Judged as a
musician it is sensible where it counts — absolute-from-original, root-only, and
persistent are the right calls, and root-only is unarguable because the feature IS
a key change and quality-propagation would erase the progression's identity. Its
two soft spots are ergonomic, not conceptual: reset-by-press-original is arithmetic
that a performer will not intuit (give it a context-free home), and a bare chord
press is too overloaded to carry a silent whole-song re-key by default (arm it
explicitly). The unification with the plain hold is real on the root axis and is
the design's best idea — ship ONE offset mechanism — but it has an honest quality
seam that must be ruled, not glossed. Parked correctly: Path 1 ships the literal
follow players expect; this returns as an opt-in "play the Transpose button as a
chord" performance mode.

---

## SPEC (parked — buildable shape for when it returns)

No product code here; this is the settled design.

**State (bounded, no heap, dual-target)**
- `std::int8_t m_root_offset = 0;` — global root transpose, applied `% 12`,
  normalized to `[-6, +6]` for display (direction is cosmetic: `+2 == -10 mod 12`).
  Lives on `ChordSequencer`/`Engine`. The stored `ChordSequence` is the immutable
  original reference and is NEVER mutated.
- No new mode enum for the root math (behavior is unified). One explicit arm flag
  to enable "chord-press = transpose" vs the default literal follow.

**When the offset is captured / applied**
- On a live steering press (`Producer::kDetect` or `kManual`) while armed:
  - Determine the source cell: if `m_seq.playing()` with `length>0`, `P` = current
    playback position and cell = `current_step_at(P)`, `original_root =
    resolve_root(seq, cell, /*offset*/0)` (stored key, no offset); else the single
    home/followed cell.
  - `offset := (pressed_root - original_root) mod 12`, normalized — absolute from
    the original, recomputed every press (never reads the current offset -> no
    cumulative drift).
  - Immediate (default): set `m_root_offset`; SOUND the pressed chord now
    (root+quality) for the current bar; subsequent bars fire from the sequencer at
    their own downbeats using recorded qualities on transposed roots.
  - Quantized (opt-in, existing quantize flag): stage the offset to the next bar
    via the existing `stage`/`commit_bar` path and expose it as `pending()` — this
    also fills the "next key" readout the owner reported as always empty.
- Boundary tie-break: a press exactly on a bar boundary reads the step active AFTER
  that boundary's fire.

**Root delta semantics**: mod-12; store normalized `[-6, +6]`; direction cosmetic.

**Quality rule (the seam)**: root offset is global; quality is per-source-cell.
Multi-cell recorded source keeps each cell's recorded quality (root-only);
single-cell degenerate source adopts the pressed quality; the current bar always
audibly sounds the pressed chord at the instant of press. Deferred sub-case: a
press whose quality differs from a MULTI-cell scheduled chord uses root only for
the delta and may localize its quality to the current bar's one-shot sounding only.

**Reset**
- Press the original chord of the current bar -> offset 0 (falls out; no special
  case).
- Lifecycle: `seq new` / `seq use` / `key set` / transport restart -> offset := 0.
- Add a context-free `seq home` safety (and a UI readout of the original
  progression) — backstop for the discoverability gap.

**Single-finger vs fingered**: transparent — both yield a `root_pc` (and a
quality); the offset reads `root_pc` only; quality follows the rule above.

**D47 gate / `kAuto` clobber — how this REPLACES the race**: the live press no
longer writes the followed context as a competing producer. The `ChordSequencer`
remains the SOLE steering writer (`Producer::kSequencer` `commit_now` each bar),
now deriving its root from `original[P] + offset`. One writer per bar -> the
last-writer race is structurally gone; `kAuto` is safe because only one producer of
`current` exists while a sequence plays. In the degenerate case only detect writes
(no sequence), also single-writer. The D47 gate remains for the genuinely different
detect-vs-manual selection but no longer has to arbitrate seq-vs-live.

**Determinism / no-heap / dual-target**: one `int8` offset; `current_step_at` is a
bounded loop; one add+modulo on the hot path; `fire` callback signature unchanged;
constexpr-friendly; identical host/arm; no allocation, no dynamic dispatch.

**ABI surface (minimal)**: one arm flag (transpose vs literal follow); reuse the
existing quantize flag for the staged variant; reset needs no new command (falls
out) plus an optional `seq home` safety; one read query exposing `m_root_offset`
and the staged offset for the UI "current key / next key" readout.

---

## Sources

- [Playing Chords on Yamaha Keyboards — PSR Tutorial](https://psrtutorial.com/lessons/start/s50_fingering.html)
- [Single Finger and Fingered methods — Yamaha FAQ](https://faq.yamaha.com/usa/s/article/U0002033)
- [Tyros 5 fingering styles guide — ePianos](https://www.epianos.co.uk/tyros-5-fingering-styles-guide/)
- [Mastering AI Fingered Mode — Yamaha Hub](https://hub.yamaha.com/keyboards/workstations/genos-power-playing-mastering-ai-fingered-mode/)
- [Step Edit / Chord Looper — Sand, software and sound](https://sandsoftwaresound.net/step-edit-chord-looper/)
- [Genos2 overview — Yamaha USA](https://usa.yamaha.com/products/musical_instruments/keyboards/arranger_workstations/genos2/index.html)
- [Genos reference, Transpose in semitones — ManualsLib](https://www.manualslib.com/manual/1333828/Yamaha-Genos.html?page=36)
- [Chord Sequencer — Korg Pa1000 guide](https://manualzz.com/doc/o/12vopf/korg-pa1000-guide-the-chord-sequencer)
- [Chord recognition / Chord Sequencer — Korg Pa4X](https://www.korg.com/us/products/synthesizers/pa4x/page_2.php)
- [Chord recognition / accompaniment — Roland E-A7 manual](https://www.manualslib.com/manual/1062762/Roland-E-A7.html?page=15)

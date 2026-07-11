# Chord Following — design & competitor analysis

How arrangrr turns your live playing into the chords the band follows, how the
mainstream arrangers do the same thing, and where the two differ.

Two related but **separate** things live here:

- **Literal chord follow** — the standard arranger behaviour arrangrr ships: you
  play a chord, the whole band plays that chord, held until you change it. This
  is "Path 1", and it is what every commercial arranger does.
- **Pivot** — a novel, *parked* feature: playing a chord relatively **transposes**
  a running chord progression. Optional, opt-in, future. Full cited analysis and
  implementable spec in
  [`docs/reflections/pivot-relative-key-transpose.md`](../reflections/pivot-relative-key-transpose.md).

## 1. How commercial arrangers do it (researched)

Yamaha (Genos/PSR), Korg (Pa-series) and Roland (E-A7) keep **three orthogonal
mechanisms**, never fused:

1. **Literal chord detection.** You play a chord in the left hand (or the whole
   keyboard); the arranger detects the *literal* chord and re-harmonises the
   accompaniment to it in real time. Play `Dm7` → the band plays Dm7. You **are**
   the chord track, live. (Yamaha Single Finger / Fingered / Full Keyboard / AI
   modes; Korg chord scanner; Roland chord recognition.)
2. **Chord looper / sequencer.** A separately *recorded* chord progression that
   loops and drives the band hands-free (Yamaha **Chord Looper**, Korg **Chord
   Sequencer**). Playback, not live steering.
3. **Global Transpose.** A dedicated button that shifts everything by semitones —
   a key change — context-free.

**No mainstream arranger lets you play a chord to *relatively transpose a running
progression*.** That fusion (chord looper + transpose, as one played gesture) is
arrangrr's **Pivot** — see §3 and the reflection.

Sources (full list in the reflection): PSR Tutorial (fingering); Yamaha FAQ
(Single Finger / Fingered); Yamaha Genos manual (Transpose in semitones); Korg
Pa1000 Chord Sequencer; Korg Pa4X chord recognition; Roland E-A7 chord
recognition.

## 2. Literal chord follow — what arrangrr ships (Path 1)

The behaviour competitors call "chord detection": the **style has no progression
of its own**; every part re-roots to a single *followed chord*. Play a chord →
the band follows it and **holds** it until you play another. This is what a
player expects, and it is the default.

As built:

- **`FollowedContext`** (`components/arrangrr/include/arrangrr/chord/followed_context.hpp`)
  is the single owner of the followed chord. Live detection commits it
  immediately; it persists (nothing overwrites it) until the next live chord.
- **Single-finger vs fingered** (`ChordDetector`): single-finger = one key names a
  scale-aware chord on that root; fingered = spell the chord by holding its
  notes. In single-finger, a **new key REPLACES** the previous one
  (`Shell::toggle_surface_key`) — one key == one chord. (This is the fix for the
  "A then S give the same chord" / "G then H resolve to Em" reports: without it
  the plain-TTY toggle accumulated the held set and the detector rooted on the
  lowest note.)
- **The `ChordSequencer` is a separate, optional backing track**, *not* the
  style. The engine default is now **`ChordFollow::kLivePriority`** (ABI-additive):
  while you HOLD a live chord it beats the sequencer — the band follows your finger
  and the sequencer comps its rhythm on your chord (no clash); when you release, the
  sequencer's next step resumes its own progression. With no sequencer a live chord
  latches (chord memory). `kAuto` (last-writer race) is kept only as an explicit
  legacy mode. Path 1's demo (`apps/demo/jam/setup.acmd`) still ships with the sequencer
  off, but live-priority means a running sequencer no longer clobbers live steering.

Status of the shipped standard:

- *(done)* **Live-vs-sequencer arbitration — live-priority.** Live input wins over
  the sequencer while held; the sequencer follows the live chord and resumes on
  release (`kLivePriority`, decided at the `fire_chord_seq` call site, not the static
  gate). This is the crossroads Pivot answers differently — Pivot re-*keys* the whole
  running progression instead of momentarily overriding it.
- *(done)* **Chord-context tests re-ratified** to the immediate-commit model and the
  panel-label rename (`original` / `current` / `next` key).
- *(done)* **Single-finger replace** — A→S and G→H no longer collapse to one root.

### 2.1 Two-zone harmony input (melody vs harmony per port)

As built (node `2330`, decision `D49`): the PIANO panel and the CHORDS panel drive
two distinct playable surfaces on the host, selected by panel focus, not by pitch
split. The PIANO panel feeds `kPianoInputPort` (in0), zone `kMelody`: its keys
sound and steer nobody. The CHORDS panel feeds `kHarmonyInputPort` (in1), zone
`kHarmony`: the *same* musical key bindings are output-suppressed (silent) but
OBSERVED by the `ChordDetector`, so playing there re-harmonizes the band without
sounding a note (`components/hostrt/shell_internal.hpp:44-60`). On the wire the
zone is set via `kInputZone` (ABI value 42): `a` = input port, `b` = `InputZone`
(0 = melody, 1 = harmony); `kHarmony` suppresses the port's note output, `kMelody`
routes/sounds and is the default (`components/arrangrr/include/arrangrr/abi.hpp:126-131`).
This is the two-zone split that un-defers the piano-as-sole-chord-input question:
chord recognition has its own silent surface and no longer competes with the
piano's melodic role. *(Absorbed from `harmony-input-chord-zone.md`, retired
2026-07-11.)*

### 2.2 Scale-aware single-finger rule (triads only)

As built (node `2220`, decision `D45`): single-finger mode maps one pressed key to
the diatonic MAJOR-or-MINOR triad of that root — never diminished or augmented.
The rule (`single_finger_quality`, `components/arrangrr/include/arrangrr/chord/theory.hpp:203`,
self-tested at `theory.hpp:342-351`): root = the pressed key; quality = the
diatonic degree's quality; where the diatonic degree is diminished or augmented it
SNAPS by fifth-restoration — a diminished triad (minor third, lowered fifth) snaps
to MINOR (keep the minor third, restore the perfect fifth), an augmented triad
(major third, raised fifth) snaps to MAJOR (keep the major third, restore the
perfect fifth); a chromatic (out-of-scale) root defaults to MAJOR. Single-finger
is triads only — it never adds the richness (7ths, extensions) that diatonic
mode's `smart_quality` (`2240`) applies, and unlike diatonic mode it never rejects
a chromatic root (diatonic mode is D20-strict: out-of-key is silent/rejected;
single-finger is non-rejecting by design, so a performance shortcut never blocks
mid-phrase). This is arrangrr's own scale-aware single-finger (Casio-Chord/"smart"
lineage), not the key-independent classic Yamaha Single Finger convention
(root+left = 7th, root+white = minor). *(Absorbed from
`harmony-input-chord-zone.md`, retired 2026-07-11.)*

## 3. Pivot — the novel feature (PARKED)

Playing a chord relatively transposes a running chord progression by the interval
between the chord you play and the **current bar's original chord**: absolute
from the original (never cumulative), **root-only** (each chord keeps its
quality), persistent across loop wraps, reset by playing the current bar's
original chord. It fuses the Chord Looper and the Transpose button into **one
played gesture** — something no mainstream arranger offers.

Verdict (from the reflection): musically **sound** — it is a live key change that
preserves the progression's function (`I-vi-IV-V` stays `I-vi-IV-V`), and
root-only is inarguable because the feature *is* a key change. Its weak points are
**ergonomic, not conceptual** (reset-by-original-chord is arithmetic a performer
won't intuit → prefer an explicit `seq home`; a bare press is too overloaded to
carry a silent re-key by default → ships as an **opt-in mode**, "play the
Transpose button as a chord").

Full model, edge cases, cited competitor research, and the implementable spec
(bounded `int8` offset, capture/apply, reset, quality-locality rule, `kAuto`-race
replacement, determinism/ABI):
[`docs/reflections/pivot-relative-key-transpose.md`](../reflections/pivot-relative-key-transpose.md).

## Status

| Feature | State |
|---|---|
| Literal chord follow (Path 1) | **Shipping.** Single-finger replace fixed; tests re-ratified; live-vs-sequencer arbitration decided + shipped (`kLivePriority`). |
| Two-zone harmony input (§2.1) | **Shipping** (`2330`/`D49`). Piano = melody (sounds, no steer), Chords = harmony (silent, steers). |
| Scale-aware single-finger (§2.2) | **Shipping** (`2220`/`D45`). Triads only; dim→minor, aug→major, chromatic→major. |
| Pivot | **Parked**, fully specified, name assigned. Returns as an opt-in mode on a running sequencer. |

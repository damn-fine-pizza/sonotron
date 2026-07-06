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

- **`FollowedContext`** (`app/core/include/arrangrr/chord/followed_context.hpp`)
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
  legacy mode. Path 1's demo (`demo/jam/setup.acmd`) still ships with the sequencer
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
| Pivot | **Parked**, fully specified, name assigned. Returns as an opt-in mode on a running sequencer. |

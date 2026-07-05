# Style KB — Aggregate Report & Generator Design Recommendations

Synthesis over 1184 analysed style files (1109 ok / 21 partial / 54 failed, avg
confidence 0.93, 63,564 abstract patterns). Numbers from `aggregate/*.json` and
`discovery-summary.json`. Companion: `knowledge-log.md`, `discovery-report.md`.

## Phase 7 — Aggregate analysis (the ten questions)

1. **Most represented genres:** pop 84, rock 51, "beat" 50, ballad 44, swing 35,
   country 34, bossa 28, jazz 26, waltz 23; tail disco/blues/funk/bigband/samba/latin/
   dance/march. 546 filenames carry no recognised genre token (needs content inference).
2. **Recurring arranger structures:** the Yamaha grid — ~4 Mains + matched Fills + 2–3
   Intros/Endings per style (main 4013, fill 5092, intro 2765, ending 2750). Section
   markers + fixed channel→role map are the two most reliable structural facts.
3. **Most common rhythm cells:** `0` (downbeat); `0,2,4,6,8,10,12,14` (straight 8ths);
   `0,4,8,12` (quarters); full 16ths; `0,3,4,7,8,11,12,15` (syncopated pop); `0,8` (half);
   `2,6,10,14` (pure offbeat). ~10 cells dominate 63k patterns.
4. **Most common bass templates (degrees):** `1-1-1-1` (root pulse), `1-5`/`1-5-1-5`
   (alternating root–fifth), `1-1-5-5`, `1-1-1-5`, `1-1-3-5` (root-third-fifth walk).
5. **Most common chord comping:** stab `0`; 8th comp; quarter block `0,4,8,12`; offbeat
   `2,6,10,14`; sustained `0,8`; syncopated. Two chord parts differ by rhythm+register.
6. **Recurring fill strategies:** 1-bar, highest drum density, one-shot back to the
   returning Main; crash on the next downbeat. 5092 fills ≈ one per Main.
7. **Most useful for a generator:** bass degree-movement, the ~10 rhythm cells, drum
   kick/snare/hat grids, chord voicing-width + rhythm, per-style energy curve.
8. **Too specific — do NOT copy:** absolute pitches, exact velocities, ornament runs, the
   `b5-b5-b5` bass artefacts, one-off long-tail cells, proprietary voicings verbatim.
9. **Abstractions to implement in the engine:** rhythm cell (16-bit mask) + mutation ops;
   degree-movement bass; drum grid + percussion layer; voicing-width chord realiser
   (`kLead`); energy/density steering.
10. **Missing → compensate with theory/procedural:** chord quality per beat (arrangrr
    theory), voice-leading (D41), swing ratio (parameterise), humanization (generate),
    genre for 546 unknowns (content inference), triplet/1-32 detail (procedural).

## Phase 8 — Generator Design Recommendations

A dynamic MIDI generator on this KB + arrangrr's existing pipeline. It composes from
abstractions; it does not replay styles.

**Architecture (host-first, device-portable):**
- **Style profiles** — per-genre bundles of canonical cells, bass templates, drum grids,
  tempo/feel/energy defaults, distilled from `aggregate/*.json`.
- **Section engine** — reuse `SectionType` + `request()`; Intro→Main(A..D)→Fill→Ending
  with the observed one-shot/return semantics.
- **Role engine** — DRUMS/PERCUSSION/BASS/CHORD_COMP/PAD/RIFF/MELODY; each role a
  generator keyed by its abstraction type.
- **Pattern selector** — pick cell + template per role/section from the profile, seeded.
- **Variation engine** — Main A→D = density/energy deltas from the per-style curve.
- **Mutation engine** — apply each pattern's `mutation_axes` (octave, syncopation,
  ghost_density, voicing_width, rests), bounded + seeded.
- **Humanization** — `groove::apply` (swing/accent/humanize), seeded (D16).
- **Energy/intent steering** — the Generative Director (D37) consumes per-style energy
  curves as targets; intent maps to (density, register, tension, swing).
- **Chord-following** — render through `Arranger::resolve()` vs the live `ChordState`:
  **generate and apply are the same call** (D24).
- **Scale-aware transforms** — `NoteSource::kScaleDegree`/`kInterval` (D39) for melody +
  approaches; `ChordGesture` (D40) strum/roll; `VoicingPolicy::kLead` (D41) comp.
- **Fill/transition managers** — fill = 1-bar high-density insert returning to Main;
  transitions bar-quantised (arrangrr already does this).
- **Deterministic seed system** — one seed per performance → reproducible (D16/D29).

**Data tiering:**
- **Keep in KB (host):** full per-file JSON + aggregates — training/lookup, regenerable.
- **Compile to flash (constexpr):** the curated canon — ~32 rhythm cells (`uint16_t`
  masks), ~16 bass degree templates (nibbles), drum grids, per-genre defaults (D33).
- **Generate at runtime:** cell/template selection + mutation + `resolve()` expansion —
  no heap, bounded, seeded.
- **Derive procedurally:** voice-leading, chromatic approaches, chord quality per beat,
  humanization, any genre/feel not in the data.
- **Compress:** rhythm cell = 16-bit onset mask; degrees = 4-bit nibbles; velocity
  profile = 4 bytes/bar.

**STM32 compatibility:** the device half touches only flash-resident POD tables + the
existing no-heap `resolve()`/`groove`/gesture/voicing pipeline. KB, extractor and JSON
stay on the host. Preserves D1/D2/D32/D33.

## First implementation plan (off-device first)

1. **Canon builder** (host): read `aggregate/*.json` → compact per-genre canon (cells as
   masks, bass/chord templates, drum grids, defaults).
2. **CASM decoder** in `arrstyle-converter` to replace the CMaj7 source-chord assumption
   and finish `import-sff` (feeds truer degrees into the KB).
3. **Generator prototype** (host): profile + seed → `StyleEvent`s → existing
   `resolve()`/`groove`/gesture/voicing → MIDI out. Prove same-seed-same-bytes.
4. **Flash port**: compile the canon to `constexpr`; run on the no-heap pipeline; measure
   vs the D33 budget.

*KB files: discovery-report.md, discovery-index.json, discovery-summary.json,
schema/*.json, extracted/<id>.json (1184), aggregate/*.json, knowledge-log.md, report.md.
Tool: app/tools/arrstyle-extractor/extract_kb.py.*

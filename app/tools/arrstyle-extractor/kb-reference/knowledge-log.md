# Styles Knowledge Log

Primary document for the arranger-style Knowledge Base. Cumulative record of what
was found, what is uncertain, what is reusable, what must NOT be copied literally,
what the future dynamic MIDI generator needs, and what to implement next.

Built by `app/tools/arrstyle-extractor/extract_kb.py` (deterministic, dependency-free)
over `projects/resources/`. Regenerate with:
`python3 app/tools/arrstyle-extractor/extract_kb.py --corpus <resources> --out <resources>/kb/styles`.

---

## Executive Summary

- **1184 style files analysed**, every one produced an entry: **1109 ok · 21 partial ·
  54 failed**, mean confidence **0.93**. **63,564 abstract patterns** extracted.
- The corpus is overwhelmingly **Yamaha SFF1/SFF2** (1109) — Standard MIDI Files with a
  proprietary `CASM` chord-transposition block — plus a few plain SMF (21). 54 files
  are non-SMF junk/unsupported (discard or write a decoder later).
- The style format maps **cleanly onto arrangrr's existing model**: fixed channels →
  roles, CASM NTR/NTT → arrangrr NTT (D24), section markers → `SectionType`. The KB is
  therefore not a foreign artefact — it is arrangrr's own concepts observed at scale.
- The generator should store **abstractions, not notes**: chord-relative degree
  movement (bass), 1/16 rhythm cells (all roles), drum instrument grids, voicing width,
  contour + density + energy curves. All are present and aggregated in `aggregate/`.

---

## Dataset Overview

**Evidence:** `discovery-summary.json`, `aggregate/*.json`. **Confidence:** high (measured).

- Files: 1184 (`.sty` 1010, `.prs` 120 Korg, `.sst` 22, `.mid` 32; some overlap by scan).
- Formats detected: sff 1109, smf 21, unknown 54.
- Roles observed (files containing the role): CHORD_COMP 1066, BASS 1059, PERCUSSION
  1035, DRUMS 966, PAD 957, RIFF 931, MELODY 895, UNKNOWN 872.
- Sections observed (total instances): fill 5092, main 4013, intro 2765, ending 2750.
  → the average style has ~4 Mains, ~5 fills, ~2–3 intros/endings: the classic
  Yamaha section grid.
- Genres (by filename token; 546 "unknown" = no recognisable token): pop 84, rock 51,
  "beat" 50, ballad 44, swing 35, country 34, bossa 28, jazz 26, waltz 23, disco 16,
  blues 16, funk 15, bigband 14, samba 14, latin 10, jive 10, dance 8, boogie 8, march 7.
- PPQN: mostly 1920, some 96. Time signature: mostly 4/4, waltz 3/4.

---

## Musical Findings

### Bassline movement is idiomatic and chord-relative
**Evidence:** `aggregate/bass-templates.json` (degree movement over the source root C).
**Confidence:** high. Top templates: `1-1-1-1` (1515, root pulse), `1` (1131),
`1-1-5-5` (458), `1-1-1-5` (452), `1-5` (423), `1-5-1-5` (402, alternating root–fifth =
the country/pop two-beat), `1-1-3-5` (165, root–third–fifth walk). `b5-b5-b5-b5` (271)
is an artefact (percussion/atonal content on the bass channel — flag, do not treat as
harmony). **Implication:** store bass as degree-sequence + rhythm; realise via NTT.

### Rhythm cells cluster into a small vocabulary
**Evidence:** `aggregate/rhythm-cells.json`. **Confidence:** high. Dominant cells:
`0` (single downbeat, 7936), `0,2,4,6,8,10,12,14` (straight 8ths, 3638), full 16ths
(2623), `0,4,8,12` (quarters, 2184), `0,3,4,7,8,11,12,15` (a syncopated pop cell, 1263),
`0,8` (half-note, 968), and — telling — `2,6,10,14` (the pure OFFBEAT "and" cell = the
reggae/ska skank and funk stab). **Implication:** a generator needs only a few dozen
canonical cells + a mutation operator, not thousands of copies.

### Chord comping = a handful of rhythm archetypes
**Evidence:** `aggregate/chord-comping-templates.json`. **Confidence:** high. `0` (stab),
`0,2,4,6,8,10,12,14` (8th comp), `0,4,8,12` (quarter block), 16ths, `0,3,4,7,8,11,12,15`
(syncopated), `2,6,10,14` (offbeat), `0,8` (sustained). **Implication:** chord parts =
(rhythm cell × voicing width × register), realise as `kChordTone` stacks with `kLead`.

### Drum backbone is stable across genres
**Evidence:** `aggregate/drum-grooves.json`. **Confidence:** medium-high. Recurring sets:
`hat_closed+kick+snare`, `hat_closed+hat_open+kick+snare`, `crash+hat*+kick` (section
starts), `tamb`/percussion layers. **Implication:** kick/snare/hat as three independent
grids (kFixed) + a percussion layer covers most genres; genre lives in the *placement*,
not the *instruments*.

### Energy ladder across Main A→D is real but not universal
**Evidence:** per-file `energy_curve_main`; e.g. Sweet Bossa A 5.16 > B 4.52 > D 3.85 —
here density DECREASES A→D (that style front-loads energy). **Confidence:** medium: the
"A<B<C<D" convention holds for many but NOT all styles; some invert or vary texture
rather than density. **Implication:** treat the energy curve as *data per style*, not a
hard-coded rule.

---

## Arranger Findings

**Evidence:** section markers per file, `aggregate/section-index.json`. **Confidence:** high.

- Section set is the Yamaha standard: `Intro A/B/C`, `Main A/B/C/D`, `Fill In AA/BB/CC/DD`
  (+BA), `Ending A/B/C`, occasional `Break`. Marker meta-events (`FF 06`) delimit them
  inside a single MTrk.
- Fills are ~1 bar; Mains 1–4 bars (Sweet Bossa Mains = 4 bars each). Intros/Endings vary.
- Fixed channel→role map is honoured across the corpus (parts on MIDI ch 9–16). This is
  the single most reliable structural fact and the backbone of the role engine.
- Each Main has a matching Fill (AA↔A, BB↔B). Intro leads in; Ending stops. → maps 1:1 to
  arrangrr `request()`/one-shot/`stop_transport` semantics.

---

## Composition Findings

- Two chord channels differ by **role, not chord**: one on-beat block, one offbeat/upper
  syncopation. **Confidence:** medium (inferred from paired CHORD_COMP cells `0,4,8,12`
  vs `2,6,10,14`). **Implication:** generate Chord 1 and Chord 2 from the same harmony
  with different rhythm cells + registers.
- Bass and kick are rhythmically coupled in most straight genres. **Confidence:** medium.
- Melodic riff/phrase channels carry contour, not chords — degree/contour abstraction is
  the right store. **Confidence:** medium.

---

## Programming Findings

- The embedded SMF is trivially parseable with a ~120-line stdlib parser (no deps);
  running status + meta handling is all that's needed. **Confidence:** high.
- The proprietary chunks (`CASM`/`CSEG`/`OTS`) sit AFTER the MTrk tracks; stopping after
  `ntrks` MTrk chunks cleanly ignores them (the extractor does this). **Confidence:** high.
- CASM is NOT decoded here (Note Transposition Rules/Tables). It is the one remaining
  binary to reverse for full fidelity; the note data + our C-source-root degree inference
  already gives generation-ready harmony without it. **Confidence:** medium (degree
  inference assumes source chord = CMaj7, the Yamaha default).

---

## Parsing Findings

**Evidence:** run stats. 1109 ok, 21 partial, 54 failed. **Confidence:** high.
- "failed" (54) = files with no `MThd` (junk, partial downloads, or non-Yamaha `.prs`).
- "partial" (21) = plain SMF with notes but no SFF section markers → treated as one block.
- Zero crashes over 1184 files; every file produced an entry (hard requirement met).

---

## Format Limitations

- **CASM not decoded** → transposition rules (which channels transpose, note limits,
  retrigger) are inferred from convention, not read. Bass/melody degree inference assumes
  source root C.
- **Korg `.prs` / `.pcg`** carry native chunks we do not parse; only their embedded SMF
  (if any) is read. 120 `.prs` mostly land in failed/partial.
- **Swing estimate is weak** (drum-channel offbeat position heuristic); treat as a hint,
  not a measurement. Most entries report swing 0.0 because the heuristic needs dense hat
  data it did not always find.
- **Key signature** almost never present in style files (they are chord-relative), so
  `key_signature` is usually null — expected, not a defect.

---

## Unsupported / Partially Supported Cases

- 54 non-SMF files: document a Korg/`.pcg` decoder as a future task; for now they carry a
  `parse_status: failed` entry with a limitation note (traceable, not silently dropped).
- Multi-bar Main sections with tempo changes mid-section: handled (first tempo taken);
  mid-section tempo automation is not modelled.

---

## Pattern Extraction Strategy

Chosen abstraction per role (all present in `extracted/*.json`):
- **BASS** → degree sequence (rel. to source root) + 1/16 rhythm cell + contour.
- **CHORD_COMP** → rhythm cell + average voice count (voicing width).
- **DRUMS** → per-instrument onset grid (kick/snare/hat/...), a `drum_map`.
- **PAD** → sustained; contour + very low onset density.
- **RIFF/MELODY** → scale-degree contour over 1–2 bars.
Folding is done modulo the bar (1/16 grid), so a 2-bar pattern collapses to its
repeating cell. Section-relative ticks (bug fixed during the run) make degree extraction
correct for sections that don't start at tick 0.

---

## Abstraction Decisions

### Insight: store movement as chord/scale degrees, never absolute MIDI notes
**Evidence:** bass/chord templates are only reusable once expressed as degrees; the top
bass template `1-5-1-5` is meaningless as absolute pitches but universal as degrees.
**Confidence:** high. **Reasoning:** degrees transpose to any chord/key via NTT; absolute
notes do not. **Implication:** the generator's currency is (degree, rhythmic slot, accent).
**Limitations:** depends on source-root inference (CMaj7 default) — reliable for Yamaha.

### Insight: rhythm is a small alphabet + mutation, not a corpus
**Evidence:** ~10 cells cover the majority of onsets across 63k patterns.
**Confidence:** high. **Implication:** ship ~32 canonical cells in flash; generate the
rest by mutation (shift/thin/thicken/swing). **Limitations:** loses rare idiosyncratic
cells — acceptable (those are the "do not copy" tail).

---

## Generator-Relevant Insights

### Insight: bass templates as chord-relative movement
**Evidence:** observed across pop, rock, dance, country, jazz. **Confidence:** high.
**Reasoning:** absolute notes aren't reusable across chords/keys; degrees are.
**Implication:** represent bass as degree movement + rhythmic grid; realise via
`resolve()` (`kChordTone` + `NoteSource::kInterval` for approaches). **Limitations:**
needs chord/root inference, incomplete for non-Yamaha files.

### Insight: the same resolve() applies AND generates
**Evidence:** arrangrr already resolves chord-relative events live (D24). **Confidence:**
high. **Reasoning:** "apply on the fly" = resolve vs live chord; "generate static" =
resolve vs a chosen chord, emit to a table. **Implication:** one function, two call
sites — no separate generator harmony engine. **Limitations:** gestures/voicing (D40/D41)
must be applied in the same pipeline to match arranger output.

### Insight: energy and density are per-style curves, steer them, don't hardcode
**Evidence:** `energy_curve_main` varies (some A→D rising, some falling).
**Confidence:** medium. **Implication:** feed the Director (D37) real per-style curves as
targets rather than an assumed ladder. **Limitations:** density is a proxy for energy;
velocity/timbre also matter and are only partly captured.

### Insight: offbeat cell `2,6,10,14` is a genre signature
**Evidence:** appears as a top chord/percussion cell. **Confidence:** medium-high.
**Implication:** expose "offbeat placement" as a first-class mutation axis (reggae skank,
funk stab, ska). **Limitations:** placement alone doesn't make a genre — pair with feel.

---

## Embedded / STM32 Constraints

- The KB (JSON) is a BUILD-TIME asset. For the device, compile the chosen canonical
  cells + degree templates into `constexpr` tables (D32/D33 flash), exactly like today's
  `StyleEvent[]` — the extractor's abstractions are already POD-shaped.
- Runtime generation must stay no-heap/bounded: pick a cell + degrees + seed → expand via
  `resolve()` + `groove::apply` into the scheduler. No JSON on the device.
- Data tiers: **KB (host, full JSON)** → **flash (curated cells/templates, constexpr)** →
  **runtime (seed-driven selection + mutation)** → **derived (voicing/approach via
  procedural rules)**. Compress by storing cells as 16-bit onset masks (a `uint16_t` per
  bar) and degrees as nibble arrays.

---

## Data Model Decisions

- Per-file entry schema: `schema/style-kb.schema.json`; abstract pattern:
  `schema/pattern.schema.json`.
- Rhythm cell canonical form = sorted 1/16 slot list (equivalently a 16-bit mask).
- Degrees canonical form = strings relative to source root C (`1, b3, 5, b7, ...`).
- Traceability: every entry keeps `source_file` + `sha1`.

---

## Assumptions

- Source chord = **CMaj7** (Yamaha default) for degree inference. (High confidence for
  Yamaha; unknown for Korg.)
- Channel→role map = Yamaha canonical (ch 9–16). (High confidence; a few packs re-use
  channels differently → UNKNOWN role.)
- 1/16 grid is sufficient resolution. (Loses 1/32 and triplet detail — acceptable v1.)

## Risks

- Degree inference wrong if a style used a non-C source chord (rare). Mitigation: read
  CASM source-chord byte when the decoder lands.
- Genre-by-filename is noisy (546 unknown). Mitigation: add content-based genre inference
  (tempo + feel + drum map) later.
- Treating percussion-on-bass-channel as harmony (`b5-b5-b5-b5`). Mitigation: filter bass
  notes to a plausible bass register before degree extraction.

## Open Questions

1. Do we decode CASM for exact transposition rules, or keep convention-based inference?
2. Which ~32 cells and ~16 bass templates become the flash "canon"?
3. Should the generator consume the KB JSON directly (host) or only compiled tables?
4. Is 1/16 enough, or do jazz/latin need triplet/1/32 grids?

## Recommendations

1. Keep the extractor as the KB's single source of truth (regenerable, deterministic).
2. Decode CASM next (source chord + NTR/NTT) to remove the CMaj7 assumption and finish
   `arrstyle-converter import-sff`.
3. Curate `aggregate/*.json` into a small flash "canon" of cells + templates per genre.
4. Add content-based genre/feel inference to cut the 546 "unknown".
5. Filter bass/melody notes by register before degree extraction (kills `b5-b5` noise).

## Next Steps

1. **CASM decoder** in `app/tools/arrstyle-converter` (finishes `import-sff`; supplies
   real source chord + transposition rules to the KB).
2. **Canon builder**: a second pass that selects the top-N cells/templates per genre into
   `constexpr`-ready tables (host) — the bridge from KB to flash.
3. **Generator prototype** (host, off-device first) that consumes the canon + a seed and
   emits `StyleEvent`s through the existing `resolve()`/`groove`/gesture/voicing pipeline
   (D40/D41), proving "one function generates and applies".

---

*See `report.md` for the aggregate synthesis and the full Generator Design
Recommendations (Phase 8), and `discovery-report.md` for the raw discovery pass.*

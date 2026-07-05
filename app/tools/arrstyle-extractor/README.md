# arrstyle-extractor

Deterministic, dependency-free (Python stdlib only) **Knowledge Base extractor** for the
arranger-style corpus. It reads Yamaha SFF (`.sty/.sst`), Korg (`.prs`, best-effort) and
plain SMF (`.mid`) files and derives an **abstract, generation-oriented** KB — chord/scale
degrees, 1/16 rhythm cells, drum grids, voicing width, contour, density and energy curves
— never raw note copies. It is the offline companion to `arrstyle-converter` (the C++
runtime importer): this tool *learns from* the corpus so a future dynamic MIDI generator
can compose, not replay.

## Run

```
python3 extract_kb.py --corpus <corpus-root> --out <kb-out-dir>
```

Example (from the repo, against the downloaded corpus):

```
python3 app/tools/arrstyle-extractor/extract_kb.py \
  --corpus ../resources \
  --out    ../resources/kb/styles
```

## Guarantees

- **Every input file produces exactly one entry** with a `parse_status`
  (`ok | partial | failed`) — never stops at the first error, never crashes.
- **Deterministic**: no randomness, sorted iteration, integer math → same corpus, same KB.
- **Original files are never modified.**

## Output layout (`--out`)

```
discovery-report.md      discovery-index.json      discovery-summary.json
schema/style-kb.schema.json  schema/pattern.schema.json
extracted/<style_id>.json    one abstracted entry per source file
aggregate/                   genre/role/section indexes, rhythm-cells, bass/chord
                             templates, drum-grooves, fill/transition rules
knowledge-log.md             primary cumulative findings + generator design
report.md                    aggregate synthesis + Generator Design Recommendations
```

`summaries/` is intentionally left to `extracted/<id>.json` (the per-file JSON *is* each
style's summary; 1184 parallel markdowns would be redundant).

## Abstractions (see `schema/pattern.schema.json`)

| role | stored as |
|---|---|
| BASS | degree sequence over source root + 1/16 rhythm cell + contour |
| CHORD_COMP | rhythm cell + average voice count (voicing width) |
| DRUMS | per-instrument onset grid (kick/snare/hat/...) |
| PAD | sustained; contour + low onset density |
| RIFF / MELODY | scale-degree contour over 1–2 bars |

These map onto arrangrr's model: `kFixed` drums, `kChordTone`/`NoteSource::kScaleDegree`/
`kInterval` harmony (D39), `ChordGesture` (D40), `VoicingPolicy::kLead` (D41) — so the
same `Arranger::resolve()` both applies patterns live and generates them statically.

## Known limitations (see `knowledge-log.md` → Format Limitations)

- CASM (Yamaha transposition rules) is **not decoded**; bass/melody degrees assume the
  Yamaha default source chord CMaj7. Decoding CASM is the top follow-up (also finishes
  `arrstyle-converter import-sff`).
- Korg `.prs` native chunks are not parsed (embedded SMF only).
- Swing estimate is a weak heuristic; treat as a hint.

# Discovery Report — arranger style corpus

Phase 1 discovery over `projects/resources/`. Produced with
`app/tools/arrstyle-extractor/extract_kb.py`; machine index in `discovery-index.json`,
counts in `discovery-summary.json`.

## 1. How many style files
1184 analysed (recursive scan of `.sty/.STY/.sst/.prs/.mid`). Core set: **1010 Yamaha
`.sty`**, plus 120 Korg `.prs`, 22 `.sst`, 32 `.mid`.

## 2. Formats present
- **Yamaha SFF1/SFF2** (`.sty/.sst`) — the bulk (1109 parsed as sff). SMF + `CASM` chunk.
- **Plain SMF** (`.mid`) — 21 parsed as smf.
- **Korg** (`.prs`, `.pcg`, `.pcs`) — native chunks; embedded SMF read best-effort.
- Non-interpretable: 54 files (no `MThd`), flagged `failed` with a limitation note.

## 3. MIDI-like files
All `.sty/.sst/.mid` that begin with `MThd` (≈99% of `.sty`) are Standard MIDI Files and
fully parseable by the stdlib parser in the extractor.

## 4. Yamaha style / SFF / SFF2
1109 files carry SFF markers; 98% contain a `CASM` chord-transposition block. SFF2 and
SFF1 both present. Sections: Intro A-C / Main A-D / Fill In / Ending A-C (+Break).

## 5. Parsing-friendly
The 1109 SFF + 21 SMF (1130 total, 95%) parse cleanly: tempo, time-sig, markers, notes,
programs, CC, channels all recovered. Zero crashes across 1184 files.

## 6. Binary / not immediately interpretable
54 files (no SMF header) + the un-decoded proprietary chunks (`CASM/CSEG/OTS`) inside
otherwise-parseable files. The note data is read; the transposition tables are not (yet).

## 7. Metadata already available
- Per file: PPQN, tempo, time signature, section markers, channel map, program changes.
- Corpus: `manifest.sha256`, `styles/styles-found.txt`, pack directory names (device
  provenance: PSR-S950/S910/1700, PA600/PA800/pa3x, JJazzLab).

## 8. Filename-suggested genre / tempo / feel / era
Filenames encode genre reliably for ~54% (546/1184 have no recognised token): pop, rock,
country, ballad, swing, jazz, bossa, waltz, disco, blues, funk, bigband, samba, latin,
march, tango, reggae, boogie, jive, etc. Tempo/feel are recovered from the SMF, not the
name. Era hints appear ("50s", "60s", "70s") but were not parsed into fields.

## Formats to support in future
- **CASM decode** (Yamaha) → source chord + Note Transposition Rules/Tables. Highest value.
- **Korg `.prs`/`.pcg`** native chunk decode → +120 files.
- **Content-based genre inference** → recover the 546 "unknown" filenames.

Outputs: `discovery-index.json` (per-file), `discovery-summary.json` (counts),
`extracted/<id>.json` (full per-file KB), `aggregate/*.json`, `knowledge-log.md`, `report.md`.

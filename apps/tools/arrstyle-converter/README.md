# arrstyle-converter

Host-side CLI that imports **alien** music/style/song formats into arrangrr's
own native style/song JSON. It is a compiler frontend: *parse foreign format →
clean canonical model → validate → emit deterministic native JSON*. A later
stage (the style compiler) lowers that JSON onto the constexpr device format in
`app/core/include/arrangrr/arranger/style.hpp`.

**Host-only.** Built by the `host` preset, excluded from the ARM/firmware build.
Dependency-free: hand-rolled JSON writer/reader and SMF parser, no third-party
libraries, does not link `arrangrr_core`. See [`DESIGN.md`](DESIGN.md) for the
full design, model, and scope.

## Build & test

```console
cmake --preset host
cmake --build --preset host
ctest --preset host -R 'test_json|test_midi_import|test_chordpro_import|test_validate|test_cli'
```

The binary is `build/host/app/tools/arrstyle-converter/arrstyle-converter`.

## Commands

| Command | Status | Result |
|---|---|---|
| `inspect <file>` | real | auto-detects format, prints a summary |
| `import-midi <in.mid> --out <f.arrstyle.json>` | real | SMF subset → StyleModel JSON |
| `import-chordpro <in.chopro> --out <f.arrsong.json>` | real | ChordPro subset → SongModel JSON |
| `import-sff <in.sty>` | **stub** | honest "inspect-only", exits non-zero |
| `validate <file.json>` | real | validates an `.arrstyle`/`.arrsong` document |
| `help` / `version` | real | usage / version |

Exit codes: `0` ok, `1` runtime/import/validation failure, `2` usage error.

## What is real vs stubbed

- **SMF import (real, subset):** header + tracks, running status, tempo, time
  signature, track names, Note On/Off → per-channel lanes with conservative role
  inference. CC / program change / pitch bend / aftertouch / SysEx are **counted
  and reported**, never dropped silently. SMPTE division is rejected.
- **ChordPro import (real, subset):** title/key/tempo, section markers, inline
  `[Chord]` tokens (incl. `/bass`). Lyrics are ignored by design and reported.
- **SFF (stub):** inspect-only extraction of the embedded public SMF facts;
  `import-sff` refuses with a clear "not implemented". No proprietary internals,
  no shipped style content.

Output is **byte-deterministic** (integer-only JSON, fixed key order): re-running
an import yields identical bytes.

# arrstyle-converter — design

Host-side CLI that imports **alien** music/style/song formats into arrangrr's
own **native** style/song representation. It is a tool in the classic
compiler-frontend sense: *parse an untrusted foreign format → build a clean
canonical model → validate → emit deterministic native JSON*. A later stage (the
**style compiler**, out of scope for this MVP) lowers that JSON onto the
constexpr device format in `app/core/include/arrangrr/arranger/style.hpp`.

This document is the concrete, critical design. Where the originally-proposed
shape was heavier than an MVP needs, it is called out and simplified.

---

## 1. Why this tool exists, and where it must NOT live

arrangrr's runtime plays **only deterministic, precompiled arrangrr data**
(DESIGN.md D32/D33: no heap, no `<iostream>`, freestanding ARM,
`-fno-exceptions`). Parsing SMF / ChordPro / Yamaha SFF on the device is a
non-goal forever: those parsers are unbounded, allocation-hungry, and format-
churn-prone. So the alien formats are a **host import concern**, never a runtime
contract.

Consequences that shape everything below:

- The tool uses `std::string`/`std::vector`/`<iostream>` freely — it is a laptop
  program, wired into the **host build only**, excluded from the ARM preset.
- The tool does **not** link `arrangrr_core`. Its canonical model is a separate,
  tool-local set of value types (§4). This keeps the importer decoupled from the
  realtime ABI and lets the model carry rich, lossy-import metadata (source
  channel, original chord text, provenance) that has no place on the device.
- The bridge to the device is **one-directional and deferred**: the canonical
  model is designed to lower cleanly onto the runtime `Style`, but that lowering
  is a future compiler pass, not part of importing.

### Explicit "do NOT do" list

- **No parser zoo on STM32.** No format parser ever compiles into core/firmware.
- **No alien format as a runtime contract.** SFF/SMF/ChordPro are import inputs,
  not persisted arrangrr formats. The persisted native format is arrangrr JSON
  (host) → compiled binary (device).
- **No premature full SFF2/CASM engine.** SFF is stubbed inspect-only. Building a
  faithful CASM/CSEG/NTR/NTT importer is a large, low-certainty reverse-
  engineering effort; it is deliberately not attempted in the MVP.
- **No third-party dependencies** (project policy). JSON writer/reader and the
  SMF parser are hand-rolled and self-contained. If a dependency ever looks
  warranted, it is flagged for the user to decide — never added unilaterally.
- **No silent data loss.** Every musical element we cannot represent is reported
  as a diagnostic (warning), with a count and a location where possible.
- **No shipped proprietary content.** See §11 (legal).

---

## 2. Architecture

A straight pipeline, each stage a pure function over value types:

```
 bytes/text ──► Importer ──► StyleModel / SongModel ──► Validator ──► Writer ──► native JSON
   (alien)        │  ▲             (canonical)              │            │
                  │  └──────────── Diagnostics ◄────────────┴────────────┘
                  └─ format-specific parsers (SMF / ChordPro / SFF-stub)
```

- **Importers** own the alien-format knowledge. Each maps its format onto the
  canonical model and pushes diagnostics for anything dropped or guessed.
- **Canonical model** (`StyleModel` / `SongModel`) is the single interchange
  type. Nothing downstream knows which alien format produced it.
- **Validator** checks the model (via its JSON form) for structural + semantic
  invariants, independent of importer.
- **Writer** serializes deterministically. **Reader** parses native JSON back
  (used by `validate`).
- **Diagnostics** is threaded through every stage; the CLI turns
  `has_errors()` into a non-zero exit.

### Shared model vs tool-only

| Concern | Where | Rationale |
|---|---|---|
| Alien parsers (SMF/ChordPro/SFF) | tool-only | never portable; format churn |
| JSON writer/reader | tool-only | no heap/`<iostream>` allowed in core |
| Diagnostics | tool-only | host reporting concept |
| Canonical `StyleModel`/`SongModel` | tool-only *today* | rich lossy-import metadata; std containers |
| Native names (Section/Role/…) | shared *vocabulary* | the model mirrors the runtime concepts so lowering is mechanical |

The canonical model intentionally is **not** the runtime `Style`. It is a
superset (absolute source ticks instead of quantized 16th steps, per-lane
provenance, original chord text). The future compiler quantizes/prunes it down
to the constexpr `StyleEvent` grid.

---

## 3. Directory layout

```
app/tools/arrstyle-converter/
  DESIGN.md              this document
  README.md              quick usage
  CMakeLists.txt         host-only lib + exe + tests
  src/
    model.{hpp,cpp}      canonical StyleModel/SongModel + enum<->name tables
    diagnostics.{hpp,cpp} severity + message + location, collected
    json.{hpp,cpp}       self-contained deterministic writer + minimal reader
    serialize.{hpp,cpp}  model -> Json (fixed field order)
    smf.{hpp,cpp}        hand-rolled, bounds-checked SMF reader
    midi_import.{hpp,cpp}   SMF -> StyleModel
    chordpro_import.{hpp,cpp} ChordPro -> SongModel
    sff_import.{hpp,cpp}    SFF inspect-only stub + honest import refusal
    validate.{hpp,cpp}   structural/semantic validation over native JSON
    cli.{hpp,cpp}        subcommand dispatch; run() returns the exit code
    main.cpp             thin argv shim over run()
  tests/
    CMakeLists.txt
    test.hpp             tiny CHECK harness (mirrors app/core/tests/test.hpp)
    test_json.cpp
    test_midi_import.cpp
    test_chordpro_import.cpp
    test_validate.cpp
    test_cli.cpp
    fixtures/
      tiny.mid           hand-built SMF bytes (committed)
      sample.chopro      ChordPro fixture (committed)
```

---

## 4. Data model

Value types, strong enums, no global mutable state. Native vocabulary only —
even where SFF concepts map in, the names are arrangrr's
(`SectionKind`/`SectionVariation`/`Role`/`TranspositionPolicy`/
`RetriggerPolicy`/`PhraseSourceHarmony`/`PhraseLane`), never Yamaha's.

### StyleModel (target: runtime `Style`)

```cpp
struct PhraseEvent {                 // one note, section-relative
  uint32_t tick;                     // in source_ppqn units (compiler requantizes)
  uint8_t  note;                     // fixed: MIDI note; chord-tone: source note
  uint8_t  velocity;                 // 1..127
  uint32_t gate_ticks;
};
struct PhraseLane {
  Role role;                         // Drums/Bass/Chord1/... (runtime TrackRole)
  uint8_t source_channel;            // provenance (0-based MIDI channel)
  TranspositionPolicy transposition; // fixed | chord_tone  (runtime RolePolicy)
  RetriggerPolicy retrigger;         // sustain | retrigger (future NTT input)
  vector<PhraseEvent> events;
};
struct StyleSection {
  SectionKind kind;                  // intro/main/fill/break/ending
  SectionVariation variation;        // a/b/c/d/none  (kind+variation -> SectionType)
  uint16_t bars;
  vector<PhraseLane> lanes;
};
struct StyleModel {
  string name;
  SourceFormat source_format;
  uint16_t source_ppqn;              // source ticks-per-quarter (SMF division)
  uint32_t tempo_milli_bpm;          // integer => byte-stable JSON
  uint8_t  time_sig_num, time_sig_den;
  vector<StyleSection> sections;
};
```

**Lowering sketch (future).** `(kind, variation)` selects a runtime
`SectionType`; each `PhraseLane` becomes a `StylePattern` with
`RolePolicy = fixed|chordTone`; each `PhraseEvent.tick` is quantized to the
16th-grid `step = tick * 4 / source_ppqn`; chord-tone lanes have their absolute
MIDI notes reduced to NTT chord-tone indices + octave against a reference chord.
That reduction is the compiler's job and its hardest musical decision — see §10
risks.

### SongModel (target: runtime ChordSequence + Song)

```cpp
struct ChordEvent { uint16_t position, bar; int8_t root_pc; ChordQuality quality;
                    int8_t bass_pc; string source_text; };
struct SongSection { string label; uint16_t position; };
struct SongModel {
  string name; int8_t key_root_pc; uint8_t key_mode;   // 0 major, 1 minor
  uint32_t tempo_milli_bpm; uint8_t time_sig_num, time_sig_den;
  PhraseSourceHarmony harmony_source;                  // chord_sequence | live_chord
  vector<SongSection> sections; vector<ChordEvent> chords;
};
```

**Bank.** The originally-proposed "Bank" (a set of styles/songs in one file) is
**deferred**. For an MVP one document = one style or one song; a bank is a future
array wrapper and buys nothing yet. Called out as over-engineering for slice 1.

### Numbers are integers on purpose

Tempo is `tempo_milli_bpm` (120000 = 120.000 BPM), not a float. The JSON writer
emits integers only, so output is **byte-identical** run to run — a hard
requirement for golden tests and diffable review.

---

## 5. Importer interface

Uniform free functions (no inheritance needed for three importers; a virtual
`Importer` base is premature):

```cpp
bool import_midi   (const vector<uint8_t>& bytes, const string& source, StyleModel& out, Diagnostics&);
bool import_chordpro(const string& text,          const string& source, SongModel&  out, Diagnostics&);
bool import_sff    (const vector<uint8_t>& bytes, const string& source, Diagnostics&);       // refuses
void inspect_sff   (const vector<uint8_t>& bytes, const string& source, ostream&, Diagnostics&);
```

Return `false` (with an error diagnostic) on unusable input; on success the model
is populated and any lossy decisions are warnings/infos in `Diagnostics`.

### SMF subset (real)

Parsed: `MThd` (format 0/1/2, ticks-per-quarter division only — SMPTE rejected),
`MTrk` chunks with running status, VLQ deltas, Note On/Off (velocity-0 = Off),
`FF 51` tempo, `FF 58` time signature, `FF 03` track name. One `Main/VarA`
section is produced; notes group into lanes by MIDI channel; roles are inferred
conservatively (GM drum channel 10 → Drums/fixed; else a track-name keyword hint;
else Phrase/chord_tone). **Counted and reported, never dropped silently:** CC,
program change, pitch bend, aftertouch, SysEx, and any Note On with no matching
Off (gate clamped to end of track). Every buffer read is bounds-checked.

### ChordPro subset (real)

Recognized directives: `title/t`, `key`, `tempo/bpm`, section markers
(`start_of_verse|sov`, `start_of_chorus|soc`, `start_of_bridge|sob`,
`comment|c`), and a set of recognized-but-unmodelled directives (`subtitle`,
`define`, `capo`, `end_of_*`). Inline `[Chord]` tokens parse into
root+quality+optional `/bass`; `|` advances a best-effort bar counter. Lyrics are
**ignored by design** (ChordPro carries no reliable timing) and reported as a
single info line — no musical data is being dropped, only un-timed text.

### SFF stub (honest)

A Yamaha `.sty` is a Standard MIDI File with appended `CASM`/`CSEG`/`OTS` chunks.
`inspect` prints `SFF: unsupported subset — inspect-only`, extracts the *public*
embedded SMF facts (division, tempo, time sig, track count) via the SMF reader
(which stops after `ntrks` tracks and ignores trailing chunks), notes whether a
`CASM` block is present (undecoded), and exits 0. `import-sff` refuses with a
clear "not implemented" and exits non-zero. No proprietary chunk internals are
implemented.

---

## 6. Validator

Works off the native JSON (parse → check), so it validates *files*, not just
freshly-imported models. Checks: `format` ∈ {arrstyle, arrsong}; `version`
present; required fields present and correctly typed; enum strings recognized via
the same `model.cpp` name tables the writer uses (single source of truth);
`source_ppqn`/`tempo`/`time_sig`/`bars` positive; `note`/`velocity` in MIDI
range; `root_pc`/`bass_pc` in `-1..11`; per-lane events non-decreasing in tick
(warning if not). Errors accumulate; `validate` exits non-zero iff any error.

---

## 7. Normalizer

For the MVP, normalization is folded into import (sort lane events by
`(tick, note)`; clamp gates; default missing tempo/time-sig with a warning) so
that output is already canonical and deterministic. A standalone normalize pass
(merge duplicate lanes across tracks sharing a channel, snap near-grid ticks,
collapse redundant sections) is a **future** stage — listed, not built, to avoid
speculative complexity.

---

## 8. Error-reporting model

`Diagnostic{severity, message, source, line, column}` with
`Severity ∈ {info, warning, error}`. Rules:

- **info** — a benign decision the user should know (lyrics ignored, channel→role
  mapping, unsupported directive skipped).
- **warning** — a lossy decision that keeps musical data honest (dropped CC,
  unmatched Note Off, assumed tempo, unparseable chord kept as `unknown`).
- **error** — unusable input; the command fails non-zero.

Diagnostics print one stable line each: `severity: source:line:col: message`.
Import/validate fail non-zero when any error is present; the CLI also fails a
successful-parse-but-error case (e.g. an importer that produced errors while
still returning a partial model).

---

## 9. CLI design

`run(args, out, err) -> int` is the whole tool; `main` is a shim. Exit codes:
`0` ok, `1` runtime/import/validation failure, `2` usage error.

```
arrstyle-converter inspect <file>
arrstyle-converter import-midi <in.mid> --out <f.arrstyle.json>
arrstyle-converter import-chordpro <in.chopro> --out <f.arrsong.json>
arrstyle-converter import-sff <in.sty>          # refuses (inspect-only); non-zero
arrstyle-converter validate <file.json>
arrstyle-converter help | version
```

`inspect` auto-detects by extension + magic (`MThd`, `CASM`, braces/brackets).
Keeping `run` stream-injected makes whole subcommands unit-testable (exit codes,
byte-identical output) without spawning a process.

---

## 10. MVP / MVP+ / future

**MVP (this slice, all green):** CLI skeleton; canonical model + diagnostics;
deterministic JSON writer + minimal reader; **real** SMF-subset and
ChordPro-subset importers; SFF inspect-only stub + honest import refusal;
validator; unit tests + committed fixtures; host-only CMake wiring.

**MVP+:** richer role inference (per-track program-change → GM family → role);
multi-section SMF via marker/region conventions; ChordPro bar-timing from
measure bars; a `compile` subcommand that lowers `StyleModel` → a C++ `constexpr`
`Style` table or the device binary; a Bank wrapper.

**Future / aspiration:** faithful SFF CASM/CSEG/NTR/NTT import; MusicXML/iReal
song import; round-trip export (native → SMF) for A/B listening; a fuzz harness
over the SMF parser.

**Biggest design risks.**
1. *Chord-tone reduction is the whole ballgame.* Mapping absolute MIDI notes back
   to NTT chord-tone indices requires knowing the phrase's underlying chord.
   SMF does not encode it; SFF does (that is what CASM is). Until the compiler
   solves this, imported non-drum lanes are honest but naive `chord_tone` note
   dumps — musically usable only after the reduction pass. This is the reason SFF
   (which *has* the harmony metadata) is the eventual high-value importer, and
   also why it is the hardest.
2. *Role inference is a guess.* Channel/name heuristics will mislabel; the model
   records `source_channel` so a later interactive remap is possible.
3. *SFF reverse-engineering scope creep.* Easy to sink weeks into CASM. Held
   firmly at inspect-only until there is a concrete musical payoff.

---

## 11. Legal / licensing

- SFF is a **reverse-engineered** format. We ship **no** proprietary format
  internals copied verbatim and **no** copyrighted factory-style content. The
  tool only parses **user-provided** files at the user's direction.
- Committed fixtures are **hand-built** minimal files authored for this repo
  (a tiny SMF and a plain-text ChordPro), not extracted from any product.
- Imported output derives from the user's own input files; the tool asserts no
  ownership over it.

---

## 12. CMake integration

Wired into the top-level `CMakeLists.txt` **inside the non-firmware branch only**:

```cmake
else()  # host
  enable_testing()
  ...
  add_subdirectory(app/tools/arrstyle-converter)
endif()
```

The tool never appears in the ARM/firmware branch, links no third-party library,
and does not link `arrangrr_core`. Tests use `add_test` + CTest like the rest of
the project.

---

## 13. Acceptance criteria

- `cmake --preset host && cmake --build --preset host` builds the tool + tests.
- `ctest` green for `test_json`, `test_midi_import`, `test_chordpro_import`,
  `test_validate`, `test_cli`.
- `scripts/lint.sh` clean; clang-format stable (100-col, braces, `m_` members).
- ARM preset still configures/builds and contains **no** reference to the tool.
- Re-running an import produces **byte-identical** output.
- Import/validate exit non-zero on error; unsupported features are reported.

---

## Appendix A — example native JSON

`example.arrstyle.json` (abridged):

```json
{
  "format": "arrstyle",
  "version": 1,
  "name": "tiny",
  "source_format": "midi",
  "source_ppqn": 96,
  "tempo_milli_bpm": 120000,
  "time_sig_num": 4,
  "time_sig_den": 4,
  "sections": [
    {
      "kind": "main",
      "variation": "a",
      "bars": 1,
      "lanes": [
        {
          "role": "drums",
          "source_channel": 9,
          "transposition": "fixed",
          "retrigger": "sustain",
          "events": [
            { "tick": 0, "note": 36, "velocity": 110, "gate_ticks": 1 }
          ]
        }
      ]
    }
  ]
}
```

`example.arrsong.json` (abridged):

```json
{
  "format": "arrsong",
  "version": 1,
  "name": "Twelve Bar Demo",
  "key_root_pc": 7,
  "key_mode": 0,
  "tempo_milli_bpm": 128000,
  "time_sig_num": 0,
  "time_sig_den": 0,
  "harmony_source": "chord_sequence",
  "sections": [ { "label": "Verse", "position": 0 } ],
  "chords": [
    { "position": 0, "bar": 0, "root_pc": 7, "quality": "maj",  "bass_pc": -1, "source_text": "G" },
    { "position": 1, "bar": 0, "root_pc": 7, "quality": "dom7", "bass_pc": -1, "source_text": "G7" }
  ]
}
```

## Appendix B — example CLI usage

```console
$ arrstyle-converter inspect song.mid
format: midi
ppqn:   96
tempo_milli_bpm: 120000
time_sig: 4/4
tracks: 2

$ arrstyle-converter import-midi song.mid --out song.arrstyle.json
wrote song.arrstyle.json (1 section(s))

$ arrstyle-converter import-chordpro tune.chopro --out tune.arrsong.json
wrote tune.arrsong.json (12 chord(s))

$ arrstyle-converter validate song.arrstyle.json
valid: song.arrstyle.json

$ arrstyle-converter import-sff yamaha.sty ; echo $?
SFF: unsupported subset — inspect-only (use `inspect` to view the embedded SMF)
error: yamaha.sty: SFF import is not implemented (inspect-only in this MVP)
1
```

## Appendix C — example C++ API

```cpp
#include "midi_import.hpp"
#include "serialize.hpp"

arrstyle::Diagnostics diag;
arrstyle::StyleModel style;
if (arrstyle::import_midi(bytes, "song.mid", style, diag)) {
  const std::string json = arrstyle::to_json(style).dump() + "\n";
  // ... write json ...
}
diag.print(std::cerr);          // info/warning/error lines
return diag.has_errors() ? 1 : 0;
```

# Style corpus import (`9400`/`9430`) — extraction scoping pass

> Status: **scoping analysis, read-only.** Ottorino, Phase-5 Item C
> (`docs/design/phase5-execution-plan.md`). Feeds Nazzareno's `9420`/`9430`
> implementation slot. No product code touched.

## 0. Headline correction — the premise in the roadmap docs is stale

Every document that currently describes this work (`docs/DESIGN.md` `9430`,
`docs/backlog/yamaha-style-corpus-and-rules.md`, `docs/backlog/style-data-format.md`,
and `apps/tools/arrstyle-converter/DESIGN.md` itself) says the SFF importer is
**inspect-only** and CASM decode is **not implemented**. Measured against the tree,
that is no longer true:

- `apps/tools/arrstyle-converter/src/casm.cpp` (468 lines) is a real, bounds-checked
  big-endian CASM/CSEG/Ctab/Ctb2 decoder: it extracts per-channel source chord
  root/type, `NoteTranspositionRule`, `NoteTranspositionTable`, note-register limits,
  retrigger threshold, and section markers (`Intro/Main/Fill In/Ending/Break` +
  variation letter) — see `to_ntt`/`to_ntr` (`casm.cpp:63-93`),
  `role_from_destination_channel` (`casm.cpp:438-459`, the canonical Yamaha
  channel-9..15 → `Role` map), `parse_style_section` (`casm.cpp:387-436`).
- `apps/tools/arrstyle-converter/src/sff_import.cpp` `import_sff()` (`:365-417`)
  actually calls `decode_casm`, builds real `SectionSpan`s, groups notes into
  per-role `PhraseLane`s with `policy_from_channel`/`retrigger_from_channel`
  (`:72-88`), applies a bass-register fold/drop filter (`:297-314`), and reports
  every lossy decision through `Diagnostics`.
- `apps/tools/arrstyle-converter/tests/test_sff_import.cpp::test_real_corpus_samples`
  (`:183-208`) runs this against two files physically present under
  `../resources/styles/extra/**`, asserts `import_sff()` succeeds, the model
  validates, and CI is green: I ran it myself —
  `ctest --test-dir build/host -R sff_import` → **`Passed 0.07s`**.
- I ran the CLI directly on a real corpus file:
  ```
  $ arrstyle-converter import-sff "../resources/styles/extra/PSR-S950-06/PSR-S950-06/50's Pop.sty" --out /tmp/…json
  wrote …json (15 section(s))
  info: …: decoded 15 section(s) from CASM (SFF1)
  ```
  15 real sections, real per-role lanes (`drums/fixed`, `bass/chord_tone`,
  `chord1/chord_tone`, `phrase/chord_tone`, `percussion/fixed`), real event counts
  (20–56 events/lane), real `source_root_pc=0`/`source_quality=maj7` provenance.

**One inconsistency survives inside the code itself, not just the docs:** `cmd_inspect`
still routes SFF files to `inspect_sff()` (`sff_import.cpp:336-363`), which prints the
stale `"SFF: unsupported subset — inspect-only"` / `"casm: present (not decoded)"`
even though `import-sff` on the *same file* now decodes it fully. `inspect` was never
updated when `import_sff` grew real CASM decoding. Flagging this for Nazzareno as a
one-line fix opportunity, not asking him to do it here.

**Net effect on scope:** the "least-predictable-effort" reverse-engineering slice —
byte-level CASM/CSEG parsing — is **already done and tested** for SFF1 (and partially
for SFF2, see §2). The actual gap is downstream of it: nothing lowers the resulting
`StyleModel` into the constexpr `Style`/`StyleSection`/`StylePattern` shape the device
consumes. That lowering function does not exist anywhere in the tree — confirmed by
`search_code`/`grep` across `apps/tools/arrstyle-converter`: zero hits for any function
producing `arrangrr::Style` from `StyleModel`. `docs/DESIGN.md` names this precisely as
`9420` *"Style compiler (data → .cpp constexpr for the device path) — ○ HOST-ONLY"*,
still unbuilt, and `docs/backlog/style-data-format.md` independently converges on the
same conclusion ("Smallest first step: teach `arrstyle-converter` to emit the existing
constexpr header shape from its `StyleModel`"). This scope doc targets exactly that gap.

---

## 1. What is concretely extractable today, and the field-by-field mapping

### 1.1 Two independent existing pipelines (do not conflate them)

| Tool | Language | Input | Output | State |
|---|---|---|---|---|
| `arrstyle-converter` (`apps/tools/arrstyle-converter`) | C++, in the CMake host build | one `.sty`/`.sst`/`.prs` at a time | `StyleModel` → `.arrstyle.json` (`model.hpp:129-137`) | CASM decode real (§0); no device emitter |
| `arrstyle-extractor` (`apps/tools/arrstyle-extractor/extract_kb.py`, 534 lines) | Python 3 stdlib-only, **not wired into CMake/CI**, offline research tool | whole corpus tree | per-file abstracted JSON + genre/role/section aggregates | Already run over the full corpus (see below) |

The extractor's own README (`apps/tools/arrstyle-extractor/README.md:63-65`) says
*"CASM (Yamaha transposition rules) is not decoded [...]. Decoding CASM is the top
follow-up (also finishes `arrstyle-converter import-sff`)"* — i.e. it predates the C++
CASM decoder above and its bass/melody degree extraction still **assumes** the Yamaha
default CMaj7 source chord rather than reading it from CASM. This is consistent
evidence (Python tool's own self-description vs. the newer C++ decoder) that
`casm.cpp` was built after the extractor and the roadmap prose was simply never
reconciled.

The extractor's output already exists on disk and is large:
`../resources/kb/styles/discovery-summary.json` — measured directly:
`files_total: 1184`, `parse_status: {ok: 1109, partial: 21, failed: 54}`,
`formats: {sff: 1109, smf: 21, unknown: 54}`, `avg_confidence: 0.932`,
`patterns_extracted: 63564`. Per-file entries
(`../resources/kb/styles/extracted/*.json`, 1184 files) carry `musical_profile`
(genre, tempo, swing, time-sig, feel, energy) and a `patterns[]` array per
role/section already tagged `pattern_type` (`drum_grid`/`bass_movement`/
`chord_rhythm`/…), `rhythm_cell_16` (onset steps on a 16-grid), `syncopation`,
`density`, plus `generation_notes`/`mutation_axes` — i.e. genre-tagged, abstracted,
almost directly generator-ready data for **1109 SFF files**, already reduced.
`../resources/kb/styles/aggregate/{rhythm-cells,bass-templates,chord-comping-templates,
drum-grooves,fill-rules,transition-rules}.json` roll these up per genre — and this is
exactly the input `arrstyle-converter`'s existing `canon.cpp`/`build-canon` command
already consumes (`canon.hpp:44-51`, `GenreAggregate`) to emit a **different**,
coarser constexpr artifact: top-N rhythm cells + bass templates per genre, NOT a
full per-style `Style` table (see `canon.hpp` doc comment: *"a compact, freestanding
constexpr table"* of statistical aggregates). **`build-canon` is a cousin, not the
missing tool** — it answers "what does funk statistically do" for a future
rule-based generator, not "reproduce this specific Yamaha style."

So there are, in effect, **three** candidate sources feeding `9420`, not one:
1. Raw SFF bytes → `arrstyle-converter import-sff` → `StyleModel` (per-file, exact,
   CASM-grounded, but un-reduced/un-quantized — §1.3).
2. The extractor's `extracted/*.json` (per-file, abstracted, genre-tagged, but
   **not CASM-validated** for the bass/melody degree assumption — its own admitted
   limitation).
3. The extractor's `aggregate/*.json` (cross-corpus statistical rules per genre —
   feeds `build-canon`, already wired, already emits constexpr).

**Recommendation for scope:** `9420`/`9430` should build on source (1) —
`arrstyle-converter`'s `StyleModel`, because it alone carries verified
`source_root_pc`/`source_quality`/NTT provenance per lane, which is the one thing
that makes chord-tone reduction (§1.3) honest rather than assumed.

### 1.2 SFF/CASM → compiled-`.cpp` field mapping

Target shape confirmed by reading a real built-in
(`components/arrangrr/include/arrangrr/arranger/styles/bossa.hpp`): a `Style` is a
`Span` of `StyleSection`s (`type`, `bars`, `Span<StylePattern>`); each `StylePattern`
is `{role, policy, Span<StyleEvent>, gm_program, voicing}`; each `StyleEvent` is
`{step: 0..15 (16th-grid), tone, octave, vel, gate, src, gesture}`, pinned to 10 bytes
(`style_model.hpp:105`).

| `StyleModel` (already decoded) | Device `Style` field | Status |
|---|---|---|
| `StyleModel.name` | `Style::name` | clean, 1:1 |
| `StyleModel.tempo_milli_bpm` | `Style::tempo` (`BpmX100`) | clean, unit conversion only |
| `SectionKind`+`SectionVariation` | `SectionType` (13-way enum) | clean, already isomorphic — `model.hpp:29-45` designed to match |
| `StyleSection.bars` | `StyleSection.bars` | clean, direct |
| `PhraseLane.role` (`Role`, 10-way) | `TrackRole` | clean, near-1:1 (`arrstyle::Role` mirrors `TrackRole`; `kArp`/`kLead` naming needs a 1-line reconciliation — extra `arrstyle::Role::kLead` vs core `TrackRole::kLead`/`kArp` split to re-check against `TrackRole`'s actual enumerators) |
| `TranspositionPolicy` (`kFixed`/`kChordTone`) | `RolePolicy` | clean, literally the same two values |
| `PhraseLane.note_low/note_high` | not modeled on `StylePattern`/`StyleEvent` today | **model gap** — the device format clamps register only implicitly via authored `tone`+`octave`; CASM's explicit register limits have nowhere to land without a new field or a compiler-side clamp-and-drop |
| `PhraseEvent.tick` (absolute, `source_ppqn` units, e.g. 1920) | `StyleEvent.step` (0..15, 16th-grid) | **NOT built** — needs `step = (tick mod ticks_per_bar) / (ticks_per_bar/16)`; mechanically simple ONLY when the source event already sits on a 16th boundary (see §2) |
| `PhraseEvent.note` (absolute MIDI, e.g. 59/64/67/71 verified live over a decoded `source_root_pc=0`,`quality=maj7`) | `StyleEvent.tone` (chord-tone index 0..3 when `RolePolicy::kChordTone`) | **NOT built** — the reduction function itself is a tractable, deterministic, unit-testable `(note - source_root_pc) mod 12` lookup against `{0,4,7,11}` for maj7 (verified by hand on the real sample: 59→11→7th, 64→4→3rd, 67→7→5th, 71→11→7th, bass 36→0→root, 31→7→5th) **for notes that ARE one of the source chord's tones**; extensions/passing tones/chromatic approaches (9ths, blue notes, walking-bass connective tones — exactly what the genre rules in `docs/backlog/yamaha-style-corpus-and-rules.md` Part 5 call out for jazz/funk/blues) do NOT fit the 4-slot `{root,3rd,5th,7th}` table and need a policy: fall back to `NoteSource::kInterval` (semitone offset from root — already in the vocabulary, D39, `style_model.hpp:57-61`, just unused by any emitter) or `NoteSource::kScaleDegree` for melodic/phrase roles. This is a real musical judgment call per role, not a pure mechanical step. |
| `RetriggerPolicy` | not modeled on the device `StyleEvent`/`StylePattern` today | **model gap** — CASM's retrigger rule (stop/retrig/pitch-shift on a mid-note chord change) has no home; the arranger already re-resolves chord-tone notes every tick, which happens to approximate "retrigger," but "sustain" (a genuinely held note through a chord change) has no way to be expressed |
| Yamaha guitar NTR / strum offset | `ChordGesture::kStrumUp/Down` | model exists (D40) but nothing in the importer/compiler infers it from CASM's NTR=Guitar; would need a heuristic (fast near-simultaneous onsets on the same chord across adjacent MIDI channel notes) |
| `OTS` voice presets | `StylePattern.gm_program` | not read at all today — `decode_casm` skips `OTS` entirely (`casm.cpp` only parses `CSEG`/`Sdec`/`Ctab`/`Ctb2`); would need a small additional chunk parser |
| Two chord channels (Chord1 on-beat / Chord2 offbeat, per U16 in the backlog rules) | `TrackRole::kChord1`/`kChord2` distinctness | **model is fine; the source data carries it via `destination_channel` 11 vs 12** (`casm.cpp:447-448`) — this one is genuinely easy, already resolved by the existing channel map |

**Cleanly-mapping fields (no new engineering):** name, tempo, section
kind/variation, bars, role, transposition policy, Chord1/Chord2 distinction.
**Fields needing a genuinely new function (mechanical, low-risk):** tick→16th-step
quantization, chord-tone-index reduction for the common case.
**Fields needing a judgment call (musical, not just technical):** non-chord-tone
reduction fallback, register-limit handling, retrigger semantics, guitar-strum
gesture inference, OTS voice mapping.

---

## 2. Least-predictable-effort parts, and the minimal first slice

### 2.1 Where the mess actually is (evidence, not vibes)

1. **SFF2 `Ctb2` is only partially decoded.** `casm.cpp:132-134`: *"The SFF2 Ctb2
   stores NTT / limits in a richer per-chord-group sub-structure that is NOT fully
   decoded here; the role-derived policy covers those files."* Most of the corpus by
   PPQN (1920, per the backlog doc's own measurement) is Tyros/PSR-S-series, i.e.
   likely SFF2-heavy — meaning a large fraction of files fall back to the
   role-inferred `kFixed`/`kChordTone` split rather than the true per-channel NTT
   (losing `kBass`/`kMelodicMinor`/`kHarmonicMinor` distinctions the format actually
   encodes). This degrades §1.2's reduction quality silently for those files (the
   importer already reports it via `diag.info`, so it's not silent to the operator,
   but it is a real ceiling on fidelity for a large corpus slice).
2. **Swing/microtiming vs. a fixed 16-step grid.** The device grid is exactly 16
   steps/bar; genre feel for swing/shuffle/funk/samba is currently expressed via
   `GrooveParams.swing` applied AT PLAYBACK, not via extra grid resolution. Real SFF
   recordings already have swing/humanization baked into absolute tick positions
   (that's what a human played). Naively snapping those ticks to the nearest of 16
   steps either (a) destroys the feel if the recorded swing gets flattened to
   straight steps, or (b) double-swings if the device's own `GrooveParams.swing` is
   then applied on top of already-swung quantized data. Deciding this per style
   (snap-and-let-GrooveParams-restore vs. bake exact micro-offsets some other way)
   is a genuine per-genre call, not resolved by any code today.
3. **Chord-tone reduction beyond the 4-slot table**, per §1.2 — dominant for
   walking-bass (jazz/blues), funk ghost-note/slap runs, and bossa/samba syncopated
   comping (the backlog's own Part-5 rules J3/F3/BN2 all describe exactly this).
4. **The 1% (≈10 of 1010) non-`MThd` files and the ≈2% CASM-absent files** (measured
   in `docs/backlog/yamaha-style-corpus-and-rules.md`: *"293/300 (98%) carry a CASM
   [...] 287/300 expose a decodable embedded SMF"*) fall to `import_raw_fallback`
   (`sff_import.cpp:283-293`) — single flat section, role inferred from channel only,
   no real harmony provenance. Fine as an honest degrade; not a source for a
   showcase first slice.
5. **OTS-driven voice choice** is currently unread (§1.2) — imported styles would
   ship without any `gm_program`, silently using the arranger's current voice. Minor,
   but will look wrong on first listen if not flagged as a known gap.

### 2.2 Minimal first slice — proving the path end-to-end

**Recommendation: one genre, one file, full section set, SFF1-preferred.**

- **Genre:** pick a genre the corpus already tags cleanly and where the built-in
  16 have a direct point of comparison — **bossa** or **samba** are strong choices:
  the backlog's discovery-summary shows `bossa: 28` tagged files (measured, `genre-
  index.json`/`discovery-summary.json`), the built-in `bossa.hpp`/`samba.hpp` already
  encode the idiom by hand (side-stick clave, two-feel root-fifth bass, 7th comping —
  `bossa.hpp:12-86`), so a fresh import is directly A/B-listenable against a known-good
  reference, and the genre's rhythmic signature (clave, two-feel bass) is a clear
  pass/fail signal for validation (§4) — much sharper than, say, "pop," where a bland
  import is hard to distinguish from a correct one by ear.
- **File selection:** prefer an **SFF1** file if one exists for the chosen genre
  (`decode_casm` fully resolves NTT/register/retrigger for SFF1 — `casm.cpp:125-131`
  — sidestepping risk #1 above for the very first slice). If the genre's available
  files are SFF2-only, that is itself useful signal: it means the very first slice
  immediately exercises the SFF2 gap and forces an early decision on it rather than
  deferring the hardest case to file #50.
- **Section scope:** import the FULL section set from that one file (Intro/Main
  A-D/Fill/Ending — whatever the file actually has; do not hand-pick a subset), since
  section-kind/variation mapping is already clean (§1.2) and a partial-section
  first slice would understate how much is already free.
- **What "proves the path end to end" concretely means:** a single new
  `apps/tools/arrstyle-converter` command/mode that takes one `.arrstyle.json` (the
  already-working `import-sff` output) and emits ONE new `.hpp` in the exact shape of
  `bossa.hpp` — same `namespace styles::<name>`, same `kStyle` constant, wired into
  `arrangrr/arranger/style.hpp`'s `kBuiltins[]` as entry 17. There is **no loader/cap
  infrastructure to build** for this slice: today's 16 built-ins are plain compiled-in
  constexpr data with no `kMaxSections`/`kMaxPatterns` runtime caps at all (confirmed:
  no such symbols exist in `components/arrangrr/include/arrangrr/`) — Layer B (the
  no-heap runtime-loadable `.arrsty` blob, `docs/backlog/style-data-format.md` §"Layer
  B") is future work, correctly out of scope for a first slice.
- **Acceptance for the first slice:** the new style builds on both host AND
  `arm-none-eabi` presets (it's plain constexpr data, no new dependency), the 18-golden
  suite is unaffected (a new, ADDITIONAL 17th style is additive — no existing golden
  should move), and the style is manually auditioned against the genre rules in
  `docs/backlog/yamaha-style-corpus-and-rules.md` Part 5 (§4 below) before calling it
  done.

---

## 3. License / provenance assessment of `../resources/` — SHIP-GATE, not build-gate

This is explicitly **not** a blocker on building the importer for private/dev use
against `../resources/` (which sits OUTSIDE the repo, at `projects/resources/`, is
never committed, and the tool's own `DESIGN.md` §11 already states *"the tool only
parses user-provided files at the user's direction"* and *"no shipped proprietary
format internals [...] no copyrighted factory-style content"*). It IS a hard gate on
distributing ANY style produced from this corpus in a public build.

**Provenance, measured directly from the resource tree, not assumed:**

- `../resources/extra-sources/archive-urls.txt` and `../resources/download-styles.sh`
  show the corpus was scraped from third-party fan/community hosts:
  `sandsoftwaresound.net` (a personal blog distributing PSR style zips, including
  `E-pop-v1.zip` dated 2026-03 in the URL — actively maintained fan content),
  `a-mc.biz/makemusic` (another fan-hosted style archive), and `psrtutorial.com`
  (`../resources/psrtutorial/`, `../resources/pages/psrtutorial.com_*.html` — a
  long-running Yamaha-arranger community hub whose entire business is aggregating
  and redistributing style files, official and fan-made, with no visible per-file
  licensing metadata in the pages harvested here).
- `../resources/styles/extra/E-pop-v1/E-Pop/README.TXT`, read directly: *"Mark and I
  (pj) did quick and dirty conversions to SFF2 style format [...] Please feel free to
  create new styles of your own"* — this is fan-derivative content (community
  recreations of Yamaha-style factory patches modeled on **copyrighted commercial
  songs**: the README literally maps each style to a specific pop song — "Every
  Breath You Take," "Levitating," "As It Was" — i.e. these are patterns intentionally
  built to imitate identifiable, currently-copyrighted commercial recordings).
  "Feel free to create your own" is a permission to REMIX, not a redistribution
  license for the files themselves, and says nothing about the underlying songs
  they imitate.
- `../resources/www.jjazzlab.org/pkg/*.zip` (JJazzLab-Jazz-1460, JJazzLab-Pop-400,
  JJazzLab-Beatles-71, JJazzLab-PinkFloyd-60, JJazzLab-StevieWonder-30, …) are
  official downloadable packs from JJazzLab, an open-source arranger project — but
  several pack NAMES directly reference specific copyrighted artists/albums
  (Beatles, Pink Floyd, Stevie Wonder), which is itself a signal these are
  artist-idiom-style packs, not necessarily packs JJazzLab holds unrestricted
  redistribution rights to onward. I did not fetch JJazzLab's own license terms for
  these specific packs (would require a live web check outside this scoping pass) —
  flagging as **unverified, not cleared**.
- Bulk factory-adjacent Yamaha `.sty` content (PSR-S910/S950/1700, Tyros —
  `../resources/styles/extra/PSR-S950-*`) is Yamaha's own proprietary style-file
  format and, where sourced from factory presets rather than user-authored content,
  is Yamaha IP redistributed without an apparent license by the fan sites above.

**Risk statement, plainly:** this corpus is **proprietary-adjacent at best and
copyright-uncleared at worst** across every one of its sources. None of the three
source categories (Yamaha-factory-adjacent `.sty`, fan song-imitation packs, JJazzLab
artist-named packs) has verified redistribution rights checked in this pass. **No
style compiled from `../resources/` content may ship in a public build until the
owner explicitly clears its specific source file** — this needs to be a per-file or
per-pack gate, not a blanket "the importer is fine so the output is fine" assumption,
precisely because a compiled `.hpp` derived from a copyrighted style pattern is
still a derivative of that pattern regardless of how it got there. The private/dev
use case (an engineer importing one file locally to test the pipeline, never
committing the corpus or its derivatives) is unaffected and is what the first slice
in §2.2 should be — but that first-slice `.hpp`, if it comes from any file in the
categories above, **must not be the one committed to the public tree** without an
explicit owner sign-off naming the specific source and its clearance.

---

## 4. Validating an imported style musically ("does it play as the genre it claims")

Three layers, cheapest/mechanical first:

1. **Structural validation (already exists, reuse it).** `validate.cpp`/
   `validate_style()` (`:55-156`) already checks the JSON invariants (enum names,
   ranges, non-decreasing ticks). Free, already wired, run it first — it catches
   "broken," not "wrong genre."
2. **Rule-based genre audit (mechanical, could be a small NEW checker, HOST-only,
   no new dependency).** `docs/backlog/yamaha-style-corpus-and-rules.md` Part 5 gives
   79 numbered, per-genre, checkable rules — several are literally assertable against
   the emitted `StyleEvent` tables without listening: e.g. **RG1/RG2** (reggae: chord
   stabs ONLY on offbeats, kick+snare together on beat 3 not 1 — checkable as "no
   chord-tone onset at step 0/8, a drum onset cluster at step 8"), **D1** (disco:
   kick on every quarter — checkable as "kick onset at steps 0/4/8/12"), **C1**
   (country train bass: alternating root/fifth on 1 and 3 — checkable against the
   emitted `tone` sequence), **F1/F2** (funk: ghost-note velocities 30-50 between
   accented hits — checkable as a velocity-histogram bimodality test). This turns
   "sounds right" into a small, auditable, deterministic test that could sit in
   `arrstyle-converter`'s own test suite (HOST-only, no device/audio dependency) —
   genuinely SHIPPABLE as tooling, cheap, and it is exactly the kind of check that
   should exist BEFORE any imported style is trusted, not after.
3. **Actual listening (human, non-negotiable, cannot be automated away).** Load the
   compiled style through the existing host player path (`components/hostrt`/
   `gui-sonotron`), play it against a chord progression the genre would plausibly
   carry, and judge by ear against the genre's real convention (rhythmic signature,
   bass function, comping density — the same axes Ottorino uses to judge the 16
   built-ins). Rule-checking (layer 2) can prove a style is NOT obviously wrong; only
   listening proves it IS musically right. Neither layer replaces the other.

**What I did NOT do in this pass:** I did not attempt to build or run any of the
rule-checks above — that is implementation, Nazzareno's or Torquato's job depending
on whether it lands as an importer self-check or a QA gate. I am scoping it as an
available, cheap, SHIPPABLE option, not delivering it.

---

## Open decisions for the owner / things I refuse to decide alone

1. **Which genre for the first slice** — I recommend bossa or samba for the
   A/B-listenable signature (§2.2), but this is a product taste call, not mine alone.
2. **SFF2 `Ctb2` full decode** — is it worth the reverse-engineering effort now
   (unlocks the majority-PPQN=1920 slice of the corpus at full NTT fidelity) or does
   the role-inferred fallback suffice for a first wave of imports? Real effort/value
   tradeoff, owner call.
3. **Chord-tone-reduction fallback policy** for non-chord-tone notes (§1.2) —
   `NoteSource::kInterval` semitone-offset vs. `kScaleDegree` vs. simple drop —
   affects fidelity vs. implementation cost per role; a musical-engineering
   judgment call best made with a concrete file in hand (the first slice), not in
   the abstract.
4. **License clearance of the first-slice source file(s)**, and every subsequent
   one, before any imported style is committed to the public tree (§3) — this is
   the hard ship-gate and is explicitly the owner's call per source/pack, not
   something I or Nazzareno can wave through.
5. **`inspect_sff`'s stale message** (§0) — flagging, not fixing; a one-line
   correction whoever picks up `9420`/`9430` should make in passing.

## Files read/measured for this pass (for traceability)

- `apps/tools/arrstyle-converter/src/{sff_import,casm,cli,model,canon}.{hpp,cpp}`
- `apps/tools/arrstyle-converter/tests/test_sff_import.cpp`
- `apps/tools/arrstyle-converter/DESIGN.md`
- `apps/tools/arrstyle-extractor/{README.md,extract_kb.py}`
- `../resources/kb/styles/{discovery-summary.json,extracted/*.json,aggregate/*.json}`
  (measured directly, not recalled)
- `../resources/{download-styles.sh,extra-sources/archive-urls.txt,styles/extra/**/README*.TXT,
  www.jjazzlab.org/pkg}`
- `components/arrangrr/include/arrangrr/arranger/{style_model.hpp,style.hpp,styles/bossa.hpp}`
- `docs/DESIGN.md` (§`9400`/`9430`/DEFER section), `docs/design/phase5-execution-plan.md`,
  `docs/backlog/yamaha-style-corpus-and-rules.md`, `docs/backlog/style-data-format.md`
- Live run: `ctest --test-dir build/host -R sff_import` (green) and
  `arrstyle-converter import-sff` against a real corpus file (§0).

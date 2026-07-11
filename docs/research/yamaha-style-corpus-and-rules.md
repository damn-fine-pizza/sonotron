# Alien style corpus (`projects/resources/`) — validity, structure, and 70+ genre pattern rules

> **Status — research reference / future phase.** Feeds the deferred `9400` CASM→NTT importer
> (DESIGN.md D44); not consulted by the current host-GUI / P0 work. The per-genre rules (Part 5)
> stay a durable asset for when `9400` resumes.

Study of the downloaded arranger-style corpus: are the files valid, how are they
structured, do they work with `arrstyle-converter`, and — the real payload — a
large set of genre-separated rules usable BOTH to apply patterns on the fly and to
generate patterns statically. Read-only analysis; all counts are measured on the
actual files (paths under `/var/home/crsn/Condos/fedora-strudel/projects/resources/`).

---

## Part 0 — Verdict

**The files are VALID.** 1010 `.sty` files, plus 120 `.prs` (Korg), 74 `.syx`,
22 `.sst`, 32 `.mid`. The `.sty` are Yamaha **Style File Format** (SFF1 and SFF2)
— Standard MIDI Files (Format 0, one `MTrk`) carrying the pattern data, followed by
proprietary chunks (`CASM`, `OTS`, `MDB`, `MH`). Evidence:

- **99%** of the 1010 `.sty` start with a valid SMF header `MThd` (20 files, 1%, do
  not — junk/misnamed/partial downloads; discard those).
- Random `inspect` sweep of **300** files: **100%** parsed (`rc==0`), **100%**
  detected as SFF, **293/300 (98%)** carry a `CASM` chord-transposition block,
  **287/300** expose a decodable embedded SMF, **0 crashes**.
- Sections present per file are the full Yamaha set: `Intro A/B/C`, `Main A/B/C/D`,
  `Fill In AA/BB/CC/DD` (+`BA`), `Ending A/B/C`, sometimes `Break` — 8–15 named
  sections each.

**Do they work with `arrstyle-converter`? Partly — by design.**
- `arrstyle-converter inspect <f.sty>` → **works on 100% of the sample, 0 crashes.**
  It detects `format: sff`, reports CASM present/absent, embedded SMF yes/no, PPQN,
  tempo, time signature, track count. So the tool *validates and characterises* the
  corpus correctly today.
- `arrstyle-converter import-sff <f.sty>` → **inspect-only MVP**: returns a graceful
  `"SFF import is not implemented (inspect-only in this MVP)"` (rc=1) on every file.
  The CASM/CSEG/OTS decode — the "runtime-compiler pass" — is the known gap
  (`apps/tools/arrstyle-converter/src/sff_import.cpp:70-76`). The recogniser routes
  `.sty/.sst/.prs` to `kYamahaSff` (`cli.cpp:71`) and `looks_like_sff()` keys on the
  `CASM/Sff1/Sff2/CSEG` markers (`sff_import.cpp:38-41`), so the front door is built;
  only the decoder body is missing.

**Bottom line:** the corpus is a valid, rich, genre-diverse training/reference set;
the converter already ingests and validates it; finishing SFF decode (CASM→NTT) is
the one piece of work that turns it into an import pipeline. The rules below are what
that decoder — and static generation — should encode.

---

## Part 1 — Corpus facts (measured)

- **1010 `.sty`** across packs: PSR-S950/S910/1700 (Yamaha PSR/Tyros), PA600/PA800/
  pa3x (Korg, some re-saved as `.sty`), plus Indian/Indonesian, Greek, Italian sets.
- **Genre distribution** (filename tokens, top): pop 77, country 76, rock 39,
  ballad 38, "beat" 37, swing 29, waltz 21, jazz 21, dance 20, bossa 20, shuffle 18,
  slow 17, funk 16, disco 14, blues 13, latin 12, samba 10; long tail: tango,
  foxtrot, bolero, rumba, cha-cha, beguine, reggae, march, soul, r&b, salsa,
  merengue, mambo, calypso, arabic, ska, schlager, musette.
- **PPQN**: mostly **1920** (Tyros/S-series), some **96** (older PSR-1700). Any
  importer must read `MThd` division and rescale to arrangrr's PPQN=960.
- **Time signature**: overwhelmingly **4/4**; **3/4** for waltz/musette; 6/8 for some
  marches/ballads.
- **Measured tempi (per genre, one representative each)**: Bossa 120, Samba 114,
  Funk 105, Disco 140, Swing 202 (fast/double-time), Waltz 90 (3/4), Country 92,
  Rock 121, Pop 98, Reggae 93, Blues 172 (shuffle-counted), Tango 123, Ballad 76,
  March 126, Latin 130.

---

## Part 2 — Yamaha SFF structure (what the decoder must read)

1. **Container**: SMF Format 0. `MThd` → division (PPQN). One `MTrk` holds ALL
   sections back-to-back, delimited by **marker meta-events** (`FF 06 len text`):
   `"Intro A"`, `"Main A"`, `"Fill In AA"`, `"Ending B"`, … Each section is a loop of
   `bars` measures on the style grid.
2. **Channel map (the 8 style parts)** — Yamaha fixes parts to MIDI channels
   (1-based): **9** Rhythm Sub, **10** Rhythm Main (drums), **11** Bass, **12**
   Chord 1, **13** Chord 2, **14** Pad, **15** Phrase 1, **16** Phrase 2. Confirmed
   in the data (parts cluster on 0-based ch 8–15). This maps 1:1 onto arrangrr's
   roles: drums/perc → kDrums/kPerc, bass → kBass, chord1/chord2 → kChord1/kChord2,
   pad → kPad, phrase1/phrase2 → kPhrase/kLead.
3. **Source chord**: the recorded notes are written **over a fixed source chord**
   (almost always **CMaj7**, source root = C, source type = Maj7). At playback the
   engine transposes them to the played chord.
4. **`CASM`** (Chord Arrangement Style Music) — the transposition brain, one `Ctab`/
   `Ctb2` entry per channel: source-chord root+type, **NTR** (Note Transposition
   Rule: `Root Fixed` for drums/no-transpose, `Root Transpose` for bass, `Guitar`
   for strummed chords), **NTT** (Note Transposition Table: `Bypass`, `Melody`,
   `Chord`, `Bass`, `Melodic Minor`, `Harmonic Minor`, …), note low/high limits, and
   a **retrigger rule** (what a held note does when the chord changes: stop, retrig,
   pitch-shift). **This IS arrangrr's NTT (D24) in another dialect.**
5. `OTS` (One Touch Setting) = 4 voice/effect presets — maps to arrangrr per-role
   `gm_program`, not to pattern shape. `MDB` (Music Finder), `MH` (Multi Pad) =
   peripheral, ignore for patterns.

---

## Part 3 — Mapping to arrangrr + the ONE dual-use function

The whole point: **"apply a pattern on the fly" and "generate a pattern statically"
are the same operation run in two places.** In arrangrr that operation already
exists — `Arranger::resolve(pattern, ev, key, chord)` — and the CASM NTR/NTT is its
Yamaha equivalent. So:

- **Apply on the fly** = feed a `StylePattern` of chord-relative `StyleEvent`s through
  `resolve()` against the *live* `ChordState`. (What the arranger does every tick.)
- **Generate statically** = feed the SAME chord-relative events through the SAME
  `resolve()` against a *chosen* `ChordState` (e.g. bake a CMaj7 or a target chord),
  emit the concrete notes to a table/MIDI. Identical function, different `chord`
  argument and different sink.

Translation table (Yamaha → arrangrr), the decoder/generator rulebook:

| Yamaha SFF | arrangrr |
|---|---|
| Rhythm Main/Sub channel, NTR=Root Fixed | role kDrums/kPerc, `RolePolicy::kFixed` |
| Bass channel, NTR=Root Transpose, NTT=Bass | role kBass, `kChordTone` (root/fifth) + `NoteSource::kInterval` for approach notes |
| Chord 1/2, NTT=Chord | kChord1/kChord2, `kChordTone` indices 0..n; `VoicingPolicy::kLead` (D41) for guitar/piano parts |
| Pad, NTT=Chord, long gates | kPad, `kChordTone`, held gates |
| Phrase 1/2, NTT=Melody/Melodic Minor | kPhrase/kLead, `NoteSource::kScaleDegree` (D39) |
| Guitar NTR + strum offset | `ChordGesture::kStrumUp/Down` (D40) |
| Source chord CMaj7 | the `chord` passed to `resolve()` when baking |
| Section marker "Main A"/"Fill In AA"/"Ending B" | `SectionType::kVarA/kFillA/kEnding2` |
| CASM note low/high limit | `kRoleAnchor` + octave clamp in `resolve()` |

---

## Part 4 — Universal rules (all genres)

**U1.** Rescale time: read `MThd` division, map every tick to arrangrr PPQN=960
(`tick_960 = tick_src * 960 / div_src`). 1920→÷2, 96→×10.
**U2.** One style = up to 13 `SectionType`s; always populate at least `kVarA` (Main A),
one `kFill`, one `kIntro`, one `kEnding`. Main A→D = increasing energy/density.
**U3.** Drums/perc are **`kFixed`** (literal GM notes), NEVER transposed — mirror
CASM NTR=Root Fixed. Everything melodic/harmonic is chord-relative.
**U4.** Bass is **chord-relative but bass-anchored**: root on beat 1, fifth/octave on
strong beats; approach notes as `NoteSource::kInterval` (±1, ±2 semitones into the
next root). Anchor low (kRoleAnchor kBass=36).
**U5.** Chord parts are chord-tone stacks (`kChordTone` indices 0,1,2[,3]); apply
`VoicingPolicy::kLead` so voicings don't jump octaves between chords (D41).
**U6.** Pad = the chord held with long gates (kGateHeld/kGateHalfBar), few onsets,
lowest activity — fills register under the comp (kRoleAnchor kPad=48).
**U7.** Phrase/lead = melodic, key-diatonic: `NoteSource::kScaleDegree`, NOT chord
tones, so lines walk the scale (D39). Sparse; motif on section starts.
**U8.** Bake the source chord as **CMaj7** when generating a reference table, then
re-resolve to any target chord with the same `resolve()` — no separate code path.
**U9.** Density ladder per section: Intro < Main A < Main B < Main C < Main D;
Fill = 1 bar, highest drum density, resolves to the returning Main (arrangrr one-shot).
**U10.** Ending decrescendos: fewer onsets, a held final tonic chord, then
`stop_transport` (arrangrr kEnding semantics).
**U11.** Groove feel is per-genre: set `GrooveParams.swing` for shuffle/jazz/bossa,
`accent` on beat 1 always, `humanize_timing/velocity` small (2–6%) for realism.
**U12.** Velocity shape: downbeat > backbeat > offbeat; ghost notes 30–50 vel on
hats/snare for funk/shuffle. Encode in `StyleEvent.vel`, not post-hoc.
**U13.** Chord gestures (D40): use `kStrumUp/Down` for guitar/harp parts (fixed
micro-stagger), `kRollUp/Down` for piano/harp flourishes (spread across the gate).
**U14.** Section length: most Mains are 1–2 bars looped; multi-bar Mains exist —
respect `StyleSection.bars`, don't assume 1.
**U15.** Retrigger policy: when the live chord changes mid-note, chord/pad parts
re-resolve on the next grid step (arrangrr already re-resolves each tick) — mirror
CASM "retrigger" vs "stop".
**U16.** Two chord channels differ by register/rhythm, not notes: Chord 1 =
on-beat block/stab, Chord 2 = offbeat/syncopated upper voicing. Keep them distinct.
**U17.** Note limits: clamp each role to its CASM low/high (arrangrr: `kRoleAnchor` +
`resolve()` 0..127 clamp) so transposition never runs off the register.
**U18.** Discard the 1% non-`MThd` files and any with 0 sections; validate every
import with `arrstyle-converter inspect` first (it is the cheap gate).

---

## Part 5 — Per-genre rules (apply on the fly AND generate)

Each genre: tempo/feel, then numbered pattern rules. Tempi from measured data +
convention (marked ~ when conventional).

### Pop (measured ~98 BPM, 4/4, straight)
**P1.** Straight 8th feel, swing=0. Kick on 1 & 3 (+ "and" of 2), snare backbeat 2 & 4.
**P2.** Closed hats every 8th, open hat on the "and" of 4 into the bar.
**P3.** Bass: root on 1, root/fifth on 3, an 8th-note approach into bar 2. Simple.
**P4.** Chord 1 = quarter-note block triads (`kChordTone` 0,1,2), Chord 2 = offbeat
"and" stabs; Pad holds. Voice-lead (kLead).

### Ballad (measured ~76 BPM, 4/4, often 6/8)
**B1.** Low density, long gates; hats on quarters or brushed; snare soft on 2 & 4.
**B2.** Bass: root on 1, fifth on 3, whole-note feel; no busy runs.
**B3.** Pad dominates: sustained chord across the bar, `VoicingPolicy::kLead` essential.
**B4.** Phrase/lead: sparse `kScaleDegree` motif entering on bar starts; use `kRollUp`
gesture for a harp/piano chord bloom on section changes.

### Rock (measured ~121 BPM, 4/4, straight, hard)
**R1.** Kick 1 & 3 (double-kick "and-3" for driving rock), snare 2 & 4 hard (vel 110+).
**R2.** Hats/ride straight 8ths; crash on section starts (kFixed kCrash).
**R3.** Bass locks to kick, root-driven, eighth-note pulse; octave jumps on Main C/D.
**R4.** Power-chord comp = root+fifth only (`kChordTone` 0,2) — omit the third; hold
with medium gate; `kStrumUp` for guitar attack.

### Country (measured ~92 BPM, 4/4; also shuffle & waltz)
**C1.** Train/two-beat bass: root on 1, fifth on 3 (alternating bass) — the country
signature; encode as `kChordTone` 0 then 2.
**C2.** Brushed/rim snare on 2 & 4, hats straight or light shuffle.
**C3.** Chord 2 = offbeat guitar "chank" (`kStrumUp`, short gate, on the "ands").
**C4.** Pedal-steel/lead phrase = `kScaleDegree` with bends emulated by adjacent
degrees; sparse.

### Swing / Jazz / Big Band (measured up to ~202, 4/4, TRIPLET swing)
**J1.** Set `GrooveParams.swing` high (triplet feel): offbeats pushed ~2/3.
**J2.** Ride cymbal spang-a-lang pattern (kFixed kRide), hats on 2 & 4 (foot).
**J3.** Walking bass: one note per beat, `kChordTone` root/third/fifth + `kInterval`
chromatic approach to the next root — quarter-note continuous line.
**J4.** Comp = syncopated upper-structure stabs (Chord 2), NOT on every beat;
`VoicingPolicy::kLead`, seventh chords (`kChordTone` up to index 3).
**J5.** Big-band: brass "hits" as `kChordGesture`-free block stabs on anticipations
(the "and" before 1 and 3); Phrase 1/2 carry the shout-chorus line.

### Bossa Nova (measured ~120, 4/4, straight-16 with clave)
**BN1.** No backbeat: rim/side-stick clave pattern (kSideStick) — the bossa signature.
**BN2.** Bass: root on 1, fifth on the "and of 2" (the bossa two-feel), tie into 3.
**BN3.** Guitar comp = syncopated chord jabs off the clave, `kChordTone` with 7ths/9ths;
`kStrumUp` with a very small stagger.
**BN4.** Light swing (subtle), soft velocities; pad optional and quiet.

### Samba (measured ~114, 4/4, fast-16 surdo)
**SM1.** Surdo bass-drum on beat 2-emphasis (kFixed kKick low), agogô/shaker 16ths.
**SM2.** Bass: root–fifth ostinato with the surdo, syncopated pushes.
**SM3.** Percussion-dense: cabasa/tamborim/agogô 16ths (kFixed), highest perc density
of any genre — populate kPerc heavily.
**SM4.** Chord jabs short and syncopated; `kScaleDegree` phrase optional.

### Latin ballroom — ChaCha / Rumba / Mambo / Beguine / Bolero (measured Latin ~130)
**L1.** Cha-cha: cowbell + the "cha-cha-cha" on 4-&-1; bass root on 1 & the "and of 2".
**L2.** Rumba/Bolero (slower ~100): clave + conga tumbao; bass anticipated (plays the
"and of 2" and 4) — encode with `kChordTone` + `kInterval` push.
**L3.** Mambo (faster): montuno piano — a repeating syncopated 2-bar `kChordTone`
arpeggio figure; this is a prime `kRollUp`/arpeggiated-gesture candidate.
**L4.** Congas/timbales/bongos define the genre — dense kFixed percussion; keep the
clave direction (2-3 vs 3-2) consistent across a style.
**L5.** Bass never on every beat — the anticipated (syncopated) bass IS the Latin feel.

### Funk (measured ~105, 4/4, straight-16, syncopated)
**F1.** Ghost notes everywhere: snare/hat 16ths at vel 30–50 between accents (F-critical).
**F2.** Kick syncopated 16ths ("the one" heavily accented, vel 120); tight hats.
**F3.** Bass is the star: syncopated 16th `kChordTone` root + `kInterval` octaves,
slap accents (high vel) — highest bass density of any genre.
**F4.** Chord = short 16th stabs on the offbeats (Chord 2), muted-guitar `kStrumUp`
with tiny gate.

### Disco / Dance / House (measured Disco ~140, 4/4, four-on-floor)
**D1.** Four-on-the-floor kick every beat (kFixed), open hat on every "and" (the
disco off-beat hat), clap/snare on 2 & 4.
**D2.** Bass: octave-jumping 8th ostinato (root then root+octave via `kInterval` +12) —
the disco "octave bass".
**D3.** Strings/pad = sustained `kChordTone` with `kLead`; string stabs on offbeats.
**D4.** House: add syncopated piano chord stabs (`kChordTone` 7th/9th), sparser than
disco; keep the four-on-floor.

### Blues / Shuffle (measured Blues ~172 shuffle-counted, 4/4 triplet)
**BL1.** Triplet shuffle: `GrooveParams.swing` high; hats/ride play the shuffle
(1-and-a with the middle triplet dropped).
**BL2.** Shuffle bass: the 1-5-6-b7 boogie walk as `kChordTone`+`kInterval`, one note
per shuffle-eighth.
**BL3.** Dominant-7 comp (`kChordTone` up to index 3 = b7); stabs on the shuffle grid.

### Reggae (measured ~93, 4/4, offbeat)
**RG1.** The "skank": chord ONLY on the offbeats (the "ands"), silent on downbeats —
Chord 1 as short `kChordTone` stabs on beats 2-&, 3-&, 4-& (or all "ands").
**RG2.** One-drop: kick + snare together on beat 3 (not 1) — the reggae signature.
**RG3.** Bass = deep, sparse, melodic root/fifth line with rests; long gaps.

### Waltz / Musette (measured ~90, 3/4)
**W1.** OOM-pah-pah: bass root on 1, chord stabs on 2 & 3 (`kChordTone` block).
**W2.** 3/4 grid — set section bar math to 3 beats; snare/brush on 2 & 3.
**W3.** Musette: fast 3/4, accordion `kScaleDegree` phrase + tremolo feel.

### Tango (measured ~123, 4/4, marcato)
**T1.** Marcato 4: staccato accented chord/bass on all four beats (short gates, high vel).
**T2.** Habanera bass option: dotted-8th + 16th + 2 quarters (`kChordTone`+`kInterval`).
**T3.** Dramatic `kScaleDegree` phrase with the characteristic 3-3-2 rhythmic grouping.

### March / Polka (measured March ~126, 2/4 or 4/4)
**M1.** Bass on 1 & 3 (oom), chords on 2 & 4 (pah) — the two-beat march engine.
**M2.** Snare rolls/flams in fills (kFixed toms + snare), crash on downbeats.
**M3.** Polka: fast 2/4 version of M1 with an offbeat `kStrumUp` chord bounce.

---

## Part 6 — How to use this (both directions, one engine)

- **Static generation of arrangrr styles**: pick a genre block above → author
  `StylePattern`s per role following its rules → the drums as `kFixed`, harmony as
  `kChordTone`/`kScaleDegree`/`kInterval`, gestures/voicing per D40/D41 → this is a
  pure `constexpr` table, STM32-safe (D33).
- **On-the-fly application**: the exact same `StylePattern`s run through `resolve()`
  against the live chord each tick — no second code path, which is the whole design
  win (D24 NTT). A genre rule that generates well also applies well, because it is
  the same function with a different `chord` argument.
- **Finishing the importer**: implement `import_sff` (`sff_import.cpp`) to decode
  CASM → per-role NTR/NTT → arrangrr `RolePolicy`/`NoteSource`, read section markers →
  `SectionType`, rescale PPQN (U1). The recogniser and `inspect` path already work on
  100% of the corpus; only the CASM→NTT decode body is missing.

*All measurements from `projects/resources/` on this machine; `arrstyle-converter`
behaviour from `build/host/.../arrstyle-converter` (inspect 300/300 ok, import-sff
inspect-only by design).*

# Phase 7 / 9210 — Musical authoring spec: wiring MotifSpec into all 16 built-in styles

Ottorino, musical authoring pass (read-only on product code; this document is the
only artifact written). Owner directive: 9210 is "done" only when the motif
engine is wired into **all 16** built-in styles, not just a blues demonstrator.
This document is the WHAT — Nazzareno decides the HOW (where `.motif = &Spec`
gets set in each `StylePattern` initializer, how the golden regen is staged).

No new engine code is proposed. Every recommendation below uses the existing
`MotifSpec`/`MotifTransform` shape exactly as it exists today in
`components/arrangrr/include/arrangrr/arranger/style_model.hpp` (lines 112–166)
and `components/arrangrr/include/arrangrr/arranger/motif.hpp`. Two cross-cutting
findings below (§2) are close to a scope fork and are flagged, not assumed.

---

## 1. MotifSpec as-built (re-derived from the header, not recalled)

`MotifSpec` (`style_model.hpp:136–148`):

```cpp
struct MotifSpec {
  MotifTransform transform = MotifTransform::kNone;
  std::uint32_t seed = 1;         // D16 determinism: same seed -> same motif
  std::uint8_t length = 8;        // onsets in a GENERATED seed motif (authored: ignored)
  std::int8_t center_degree = 0;  // GENERATOR contour center (kScaleDegree only)
  std::uint8_t vel = 90;          // GENERATOR event velocity (authored: ignored)
  std::uint16_t gate = 200;       // GENERATOR event gate, ticks (authored: ignored)
  TrackRole idiom_role = TrackRole::kDrums;  // GENERATOR onset-mask source; ignored when authored
};
```

It attaches via one new field on `StylePattern` (`style_model.hpp:150–166`):
`.motif` (a `const MotifSpec*`, default `nullptr` = today's historical behavior,
byte-identical). **Any role can carry a motif** — the field lives on
`StylePattern`, not restricted to any `TrackRole`. When non-null, `Arranger::on_tick`
(`arranger.hpp:509–531`) generates that step's events instead of reading `.events`
literally.

Two seeding paths coexist on the SAME `MotifSpec`, selected by whether `.events`
is empty:
- **Option 1 — authored seed** (`motif::from_span`): the `StylePattern`'s own
  existing hand-written `events` span becomes the seed motif verbatim. This is
  the ONLY path that needs zero new musical authoring — it reuses content
  already in the tree. `length`/`center_degree`/`vel`/`gate`/`idiom_role` are
  ignored.
- **Option 2 — generated seed** (`motif::generate`): when `.events` is empty,
  a constrained-random seed motif is built fresh from `length`/`center_degree`/
  `vel`/`gate`/`idiom_role` — bounded-leap contour, cadence to root/fifth on the
  last onset, always `NoteSource::kScaleDegree`. This is real NEW authorship
  (choosing `center_degree` musically per chord/section), used below only where
  a style currently has **no** lead content to seed from.

Three transforms (`MotifTransform`, `style_model.hpp:112–118`), applied on ODD
repeats only (even repeats replay the seed verbatim — `apply_repeat`,
`motif.hpp:358–366`, the call-and-response/statement-answer idiom):

- **`kDiatonicTranspose`** — `tone += amount`, **only** for
  `NoteSource::kScaleDegree`/`kInterval` events. **A confirmed no-op on
  `NoteSource::kChordTone` events** (`motif.hpp:236–246`) — `tone` there is a
  chord-relative index (root/third/fifth/seventh), and the engine deliberately
  refuses to reinterpret "transpose" as a restack (Fork E, already named and
  deferred in `docs/reflections/phase7-scope-9210-9320-antisameness.md`).
- **`kRetrograde`** — `step -> (16 - 1 - step)`, a reflection about the bar
  center. Valid for every `NoteSource` including `kFixed`. Takes **no amount**
  (`apply_transform_once`, `motif.hpp:279–291`, ignores its `amount` parameter
  for this case) — its output is fixed once the seed motif is fixed; it never
  varies again across further odd repeats.
- **`kDisplacement`** — `step += amount (mod 16)`, a rhythmic phase shift.
  Valid for every `NoteSource` including `kFixed`. The amount IS repeat-keyed
  (`transform_amount`, `motif.hpp:299–312`: `seeded_hash(seed, repeat+0x1000) % 15
  + 1`), so successive odd repeats of the SAME section get a **different** shift
  each time — a genuinely evolving "answer," not a fixed alternation.

**A per-style authoring lever this creates, worth stating explicitly**: a style
author cannot tune the transpose/displacement AMOUNT at all (no field for it on
`MotifSpec` — the ranges are engine constants baked into `transform_amount`).
The only authorial control is WHICH transform kind and WHICH seed. That makes
the choice of transform *kind* per lane the real musical decision below, not a
numeric tuning exercise.

Repeat counting is a single shared `Arranger::m_motif_repeat` (one counter for
the whole band, incremented only when a variation section loops onto itself —
`arranger.hpp:440–452`), so every motif-driven pattern in the current section
answers on the SAME repeat, together — call-and-response is band-wide phrasing,
not per-role independent cycling (Fork C in the scope doc; reaffirmed here, no
style below needs independent per-role cycles).

---

## 2. Two cross-cutting findings that shape every recommendation below

**Finding A — `kRetrograde` is measurably NOT trivial for this corpus, but for a
structural reason worth naming.** Every bass ostinato in the built-in corpus is
authored on EVEN 16th-grid steps (beats and 8th-note subdivisions — the
idiomatic convention this whole corpus uses for "root-fifth-seventh" riffs).
`step -> 15 - step` maps every even step to an ODD one. I checked this
concretely against each candidate bass table (§3): `blues.kBoogieBass`
`{0,4,6,8,12,14}` reflects to `{1,3,7,9,11,15}` — completely disjoint from the
original set, same for `motown.kSoulBass`, `swing.kWalkBass`,
`country.kBoomBass`, `latin.kTumbao`, `samba.kSyncBass`, `bossa.kTwoBass`,
`reggae.kRootBass`. So the anti-triviality guard never fires for these — but
the MUSICAL character of `kRetrograde` on this corpus is not "a subtle mirror,"
it is **a wholesale flip from downbeat/8th-note-anchored bass to a fully
off-beat shadow of the same line**, identical every time a section repeats
(retrograde needs no amount, so it never evolves further — see §1). That is a
stable, recognizable "answer" shape (good for a signature riff whose identity
must stay legible — the boogie bass IS blues' identity) but it is a big jump,
not a nuance.

**Finding B — bass/comp lanes locked to a genre-defining fixed placement are a
real risk for either transform, independent of triviality.** Reggae's one-drop
(kick+bass emphasis on beat 3, silence elsewhere) and Latin's tumbao (clave-
locked syncopation) are DEFINED by exact placement relative to the (motif-free,
`kFixed`) drum pattern. Because `kRetrograde`/`kDisplacement` only ever touch
`step` — and the drums in the SAME section never move, since no built-in style
puts a `MotifSpec` on `TrackRole::kDrums` below — a bass motif in these two
styles risks producing a bass line that no longer coheres with the anchor it is
defined against. This is not a per-triviality risk, it is a genre-idiom risk,
and it is the reason reggae's and (secondarily) latin's plans below route the
motif payoff to a freer lane (melodica/mambo horn) instead of the load-bearing
bass, with bass offered only as a flagged, lower-confidence option.

---

## 3. Per-style plan

Corpus enumerated from `components/arrangrr/include/arrangrr/arranger/styles/*.hpp`
(16 files, confirmed by `ls`): ballad, basic, blues, bossa, country, disco,
funk, house, latin, motown, pop, reggae, rock, samba, shuffle, swing.

Seeds below are placeholders (`ILLUSTRATIVE` — any fixed uint32 reproduces
identically per D16; Nazzareno/owner may pick final values) written as small
distinct decimals purely so no two patterns in the same style accidentally
share a seed.

### 3.1 blues — RANK 1 (the worked example; owner-named target)

- **Lane**: `TrackRole::kBass`, Option 1 (authored seed = existing
  `kBoogieBass`, `blues.hpp:52`).
- **Pattern instances to wire**: `kAP`,`kBP`,`kCP`,`kDP` (VarA–D) and
  `kFAP`,`kFBP`,`kFCP`,`kFDP` (FillA–D) — **8 of the 9** `StylePattern`
  entries that reference `kBoogieBass` (`blues.hpp:57,63,69,74–77,89,93`).
  `kIn2P` (`blues.hpp:57`), the 9th reference, is deliberately left un-wired:
  the intro should state the riff once, plainly, before any call-and-response
  variation begins — the motif engine's own vocabulary (§1) only answers a
  section that LOOPS, so leaving the intro's single playthrough alone is
  musically correct, not an oversight.
- **Transform**: `kRetrograde` (Finding A: a stable, always-recognizable
  off-beat "answer" to the boogie riff — legible even to a listener who has
  never heard the tune, because the riff's own contour is what returns,
  reflected).
- **Seed**: 101 (illustrative).
- **Secondary lane (optional, same rank)**: `TrackRole::kLead`
  (`kLeadLick`, kInterval blue-note harp line, `blues.hpp:44–50`, wired only
  into `kDP`). `kDiatonicTranspose`, seed 102 — bending the blue-note figure up
  or down a diatonic step on the answer repeat is exactly the harp-idiom
  "lean on it differently the second time" gesture.
- **Golden impact**: `accompany_blues`-family goldens WILL move on any repeat
  of VarA–D/FillA–D beyond the first: bass notes shift from even 16th-grid
  positions (`0,4,6,8,12,14`) to odd ones (`1,3,7,9,11,15`) on every odd
  repeat. First playthrough of each section (repeat 0) is byte-identical to
  today.

### 3.2 motown — RANK 2

- **Primary lane**: `TrackRole::kLead` (`kHornAnswer`, `motown.hpp:49–52`,
  wired only into the peak variation). The name is not incidental — a horn
  "answer" phrase that never varies is the single most on-the-nose example in
  the whole corpus of the exact gap 9210 exists to close.
  **Transform**: `kDiatonicTranspose`, seed 201. Diatonic-step transposition of
  a call phrase on the answer repeat is literally how a Motown horn section
  varies its own riff bar to bar.
- **Secondary lane**: `TrackRole::kBass` (`kSoulBass`, `motown.hpp` — reused
  9x across VarA–D/Fills/E1/E2, the sharpest single-idiom bass redundancy in
  the corpus after blues). `kRetrograde`, seed 202 (Finding A: soul bass is
  also a signature riff — root-third-fifth pattern with a big skip to octave
  root at step 10 — whose reflection stays legible as "the same line,
  answered").
- **Golden impact**: Var/Fill sections' bass and the peak-variation horn
  answer both move on odd repeats; the horn phrase's diatonic degree shifts
  are audible even in a short 4-bar hold.

### 3.3 swing — RANK 3

- **Primary lane**: `TrackRole::kBass` (`kWalkBass`, root/3rd/5th/7th on
  each quarter, `swing.hpp` — reused 9x). `kRetrograde`, seed 301. Reflection
  maps the four on-the-beat quarters to the four "and" 16ths just before the
  next beat — a legitimate walking-bass anticipation figure, and because
  retrograde needs no amount, the "walk direction" answer stays a stable,
  recognizable alternation (a real jazz-bass device: alternate ascending and
  a mirrored descending walk).
- **Secondary lane**: `TrackRole::kLead` (`kLeadLick`, mixed kScaleDegree +
  kInterval, `swing.hpp:59–62`). `kDiatonicTranspose`, seed 302.
- **Golden impact**: bass moves from strict on-the-beat to consistently
  off-beat on odd repeats of any Var/Fill; lead lick degree-shifts on its one
  wired section.

### 3.4 shuffle — RANK 4

- **Primary lane**: `TrackRole::kBass` (`kShufBass`, 8 onsets on every even
  16th, `shuffle.hpp` — reused 9x, the densest single-idiom bass in the
  corpus). `kDisplacement`, seed 401 — unlike the sparser signature riffs
  above, this is a busy groove-filler, not a 4–6 note "hook"; it tolerates (and
  benefits from) an EVOLVING push that changes shape on repeat 3 vs repeat 5,
  rather than one fixed mirrored answer.
- **Secondary lane**: `TrackRole::kLead` (`kLeadLick`, `shuffle.hpp:44–47`,
  the densest blue-note lick in the corpus: 1 kScaleDegree + 4 kInterval
  events). `kDiatonicTranspose`, seed 402.
- **Golden impact**: bass shifts to a genuinely different phase each odd
  repeat (not the same shift every time); lead lick transposes.

### 3.5 country — RANK 5

- **Primary lane**: `TrackRole::kBass` (`kBoomBass`, classic root-fifth
  "boom-chick" alternation on the beat, `country.hpp` — reused 7x + 2x
  `kBB`). `kRetrograde`, seed 501 — reflection of a boom-chick figure is
  literally "chick-boom," a real idiomatic reordering that stays legible as
  the same pattern.
- **Secondary lanes**: `TrackRole::kLead` (`kLeadLick`, mixed
  kScaleDegree/kInterval, `country.hpp:49–52`) `kDiatonicTranspose`, seed 502;
  `TrackRole::kArp` (`kArpRoll`, a banjo/pedal-steel roll figure reused
  identically across its two instances) `kRetrograde`, seed 503 — an
  ascending roll answered by its own mirrored/descending shape.
- **Golden impact**: bass alternation flips (boom-chick to chick-boom) on odd
  repeats; lead lick and arp roll each move on their wired sections.

### 3.6 latin — RANK 6

- **Primary lane**: `TrackRole::kLead` (`kMamboHorn`, `latin.hpp:25–28`, a
  genuine mambo horn call phrase, all kScaleDegree). `kDiatonicTranspose`,
  seed 601 — the real payoff; horn-call variation is idiomatic and carries no
  clave risk.
- **Secondary lane, FLAGGED (Finding B)**: `TrackRole::kBass` (`kTumbao`,
  `latin.hpp` — reused 9x, a genuine son-montuno tumbao). Tumbao's identity
  IS its exact syncopated placement against the clave; `kRetrograde`/
  `kDisplacement` both risk decoherence against the (motif-free) drum/clave
  pattern in the same section. **Recommendation: wire the lead first, hold
  the tumbao motif for a follow-up pass with a listen-through per repeat**,
  not skip it outright — but do not ship it in the same batch as the safe
  lanes without a specific listening check.
- **Golden impact**: mambo horn moves on its wired section; bass unchanged
  unless/until the flagged secondary is separately approved.

### 3.7 reggae — RANK 7

- **Primary lane**: `TrackRole::kLead` (`kMelodica`, `reggae.hpp:25–28`, a
  genuine dub/melodica solo-line idiom). `kDiatonicTranspose`, seed 701.
- **Secondary lane**: `TrackRole::kArp` (`kArpSkank`, the offbeat guitar/organ
  skank, reused identically across its two instances). `kDisplacement`, seed
  702 — the skank IS an offbeat-anticipation device already, so an evolving
  displacement fits its own idiom rather than fighting it.
- **Bass EXCLUDED from this pass, FLAGGED (Finding B)**: `kRootBass` is
  reggae's one-drop anchor; motif variation risks breaking the exact
  beat-3 placement that defines the groove against the (unmoved) drum
  pattern. Not recommended in this batch.
- **Golden impact**: melodica and skank move on their wired sections; bass
  untouched.

### 3.8 disco — RANK 8

- **Primary lane**: `TrackRole::kLead` (`kLeadLine`, `disco.hpp:72–75`, mixed
  kScaleDegree). `kDiatonicTranspose`, seed 801.
- **Secondary lane**: `TrackRole::kBass`, the flat FILL bass only
  (`kFillBass`, 2 onsets `{0,8}`, reused identically across FillA–D).
  `kRetrograde`, seed 802 — low onset count means low expressiveness either
  way, but it is real, measured, currently-100%-identical redundancy across
  4 fills.
- **Not touched**: the VarA/VarC vs VarB/VarD bass alternation (`kAB`/`kBB`)
  and the Arp lane (`kArp8`→`kArp16`, already hand-varied, disco/funk/house
  are the only 3 styles where Arp already differs between its two
  instances) — both already carry real hand-authored variety; motif
  investment here has lower marginal payoff than the flat lanes above.
- **Golden impact**: lead line moves on its section; fill bass moves
  identically across FillA–D (same seed/transform, so the fills stay
  internally consistent with each other, just no longer frozen against the
  Var sections' own bass).

### 3.9 house — RANK 9

Same shape as disco (`kAB`/`kBB` alternation already hand-varied,
`kArp8`→`kArp16` already hand-varied, `kFillBass` flat across 4 fills).

- **Primary lane**: `TrackRole::kLead` (`kLeadHook`, `house.hpp:64–67`).
  `kDiatonicTranspose`, seed 901.
- **Secondary lane**: `TrackRole::kBass`, fill only. `kRetrograde`, seed 902.
- **Golden impact**: same shape as disco.

### 3.10 rock — RANK 10

- **Primary lane**: `TrackRole::kLead` (`kLeadLick`, mixed
  kScaleDegree/kInterval, blue-note rock lick, `rock.hpp:62–65`).
  `kDiatonicTranspose`, seed 1001 — a genuine "call-response guitar lick"
  idiom (blues-rock solo phrasing).
- **Secondary lane**: `TrackRole::kBass`, fill only (`kFillBass`, reused 4x
  identically; `kVarABass`..`kVarDBass` are ALREADY 4 distinct hand-authored
  ideas, so Var bass is not a target). `kRetrograde`, seed 1002.
- **Rationale for the rank**: rock's genre identity leans more on drums/power-
  chord comp (already hand-varied per section, `kVarAChord`..`kVarDChord` all
  distinct) than on bass function; the lead-lick payoff is real but the
  overall redundancy this style carries is milder than tiers above.
- **Golden impact**: lead lick moves on its wired section; fill bass moves
  identically across fills.

### 3.11 pop — RANK 11

Same bass shape as rock (`kVarABass`..`kVarDBass` already distinct, `kFillBass`
flat across 4 fills).

- **Primary lane**: `TrackRole::kLead` (`kLeadHook`, `pop.hpp:58–61`).
  `kDiatonicTranspose`, seed 1101.
- **Secondary lane**: `TrackRole::kBass`, fill only. `kRetrograde`, seed 1102.
- **Rationale for the rank**: pop is, by design, the most genre-generic style
  in the corpus (its own comping/bass carry the least idiom-specific
  signature of the 16) — the anti-sameness payoff here is real but the
  smallest per unit of authoring effort among the "safe, Option-1-only"
  styles.
- **Golden impact**: same shape as rock.

### 3.12 bossa — RANK 12

- **Bass**: `kTwoBass` (`bossa.hpp` — reused 7x + 2x `kBB`), `kRetrograde`,
  seed 1201 — bossa's smooth, unaccented 2-feel makes a stable mirrored
  answer (not an evolving displacement) the safer, more idiomatic choice.
- **MODEL GAP, not just underuse**: bossa is one of 5 styles with **zero**
  `TrackRole::kLead` instances at all (`grep -c "TrackRole::kLead" bossa.hpp`
  = 0) — there is no melodic line to seed a motif from. Closing this fully
  needs **Option 2** (`motif::generate`): a fresh `kScaleDegree` nylon-guitar/
  flute-style line, `center_degree` set per section to the chord's own
  register anchor, `idiom_role = TrackRole::kDrums` (bossa's own kFixed ride/
  rim pattern supplies the onset mask so the generated line breathes with the
  actual groove). This is real new musical authorship, not mechanical wiring
  — flagged here as the item, not assumed as free.
- **Golden impact**: bass moves on odd repeats of its wired sections; if
  Option 2 is taken up, a wholly new lead line appears where none existed
  (the largest single golden delta of any recommendation in this document,
  because it is new content, not a transform of existing content).

### 3.13 samba — RANK 13

- **Bass**: `kSyncBass` (reused 9x, samba's ONLY bass idiom in the file).
  `kRetrograde`, seed 1301 — flagged at moderate risk (like reggae/latin,
  §Finding B) since samba's surdo/tamborim pattern (already genuinely
  idiomatic, per the phase-5 review) stays fixed while bass alone would move;
  recommend a listening pass before shipping this specific lane.
- **MODEL GAP**: samba is the 2nd of the 5 zero-`kLead` styles. Same Option 2
  recommendation as bossa — a generated `kScaleDegree` line (cavaquinho/
  flute-style), `idiom_role = TrackRole::kPerc` (samba's own rich
  surdo/tamborim/agogo pattern — `latin.hpp`/`samba.hpp`'s 10-event kPerc
  tables are the richest onset source in the corpus for this purpose).
- **Golden impact**: same shape as bossa.

### 3.14 funk — RANK 14

- **Bass**: only the flat FILL bass (`kFillBass`, reused 4x; `kAB`/`kBB`
  already alternate A/C vs B/D). `kRetrograde`, seed 1401.
- **MODEL GAP, and a genre-signature gap, not just a motif gap**: funk is the
  3rd zero-`kLead` style, AND its Chord1 comping (already hand-varied per
  section — real, not flat) is the only melodic-adjacent content in the file.
  Funk's actual signature move — a syncopated clav/horn stab answering the
  groove — is simply ABSENT as authored content, not merely un-varied. Option
  2 generation here is the highest-value fix in the corpus for a genuine
  content gap, not a redundancy: a short, syncopated `kScaleDegree` stab line,
  `idiom_role = TrackRole::kDrums` (funk's kFixed drum pattern is the
  richest, most syncopated onset source to draw candidate steps from).
- **Golden impact**: fill bass moves on odd repeats; Option 2, if taken up,
  introduces an entirely new stab line where none exists today.

### 3.15 ballad — RANK 15

- **Bass**: `kVarABass`..`kVarDBass` are ALREADY 4 distinct hand-authored
  ideas — the mildest bass redundancy in the corpus. Only the fill bass
  (`kFillBass`, reused 4x) is flat. `kRetrograde`, seed 1501.
- **Arp**: `kArpHarp` reused identically across its two instances.
  `kDisplacement`, seed 1502 — a harp/pad texture invites a soft, evolving
  push rather than a hard mirrored flip.
- **MODEL GAP — the most conspicuous one in the whole corpus**: ballad is the
  4th zero-`kLead` style. A ballad's entire genre identity IS "a memorable
  vocal-style melody over a simple backing" — and the built-in ballad style
  has ZERO melodic lead content today. Option 2 generation (a slow,
  `kGateHeld`-style `kScaleDegree` line, low `center_degree` movement per
  §2.1's leap bound, `idiom_role = TrackRole::kDrums`) is the single most
  genre-appropriate use of the generator in this document, precisely because
  ballad already has the least OTHER redundancy to fix.
- **Golden impact**: mild (fill bass, arp) from the transform side; large if
  Option 2 is taken up (new content, not a variation of existing content).

### 3.16 basic — RANK 16

- Same shape as ballad (`kVarABass`..`kVarDBass` distinct, `kFillBass` flat,
  `kArp8` reused identically, zero `kLead`).
- `kFillBass`: `kRetrograde`, seed 1601. `kArp8`: `kDisplacement`, seed 1602.
- **Rationale for last place**: `basic` is the neutral scaffold style, not a
  real genre with a signature to protect or complete — there is no
  genre-specific payoff analogous to "the ballad has no melody" or "the
  boogie riff never varies." Wiring it is for consistency (every built-in
  style should demonstrate the engine once 9210 is called closed), not
  because it carries a measured musical urgency. Could reasonably be
  deferred past the other 15 if staging time is limited.
- **Golden impact**: same shape as ballad, transform side only (no
  Option-2 recommendation — `basic` has no genre convention to generate
  toward).

---

## 4. Ranking summary (wiring order for Nazzareno; also the golden-regen staging order)

| Rank | Style | Primary lane | Transform | Effort | Payoff driver |
|---|---|---|---|---|---|
| 1 | blues | Bass (kBoogieBass ×8) | kRetrograde | Option 1 | owner-named worked example |
| 2 | motown | Lead (kHornAnswer) + Bass | kDiatonicTranspose / kRetrograde | Option 1 | horn "answer" idiom is literal |
| 3 | swing | Bass (kWalkBass) + Lead | kRetrograde / kDiatonicTranspose | Option 1 | walking-bass alternation |
| 4 | shuffle | Bass (kShufBass) + Lead | kDisplacement / kDiatonicTranspose | Option 1 | dense groove tolerates evolving push |
| 5 | country | Bass (kBoomBass) + Lead + Arp | kRetrograde ×2 / kDiatonicTranspose | Option 1 | boom-chick reflection is idiomatic |
| 6 | latin | Lead (kMamboHorn); bass FLAGGED | kDiatonicTranspose | Option 1 | tumbao/clave risk (Finding B) |
| 7 | reggae | Lead (kMelodica) + Arp; bass EXCLUDED | kDiatonicTranspose / kDisplacement | Option 1 | one-drop risk (Finding B) |
| 8 | disco | Lead + fill bass | kDiatonicTranspose / kRetrograde | Option 1 | Var bass/Arp already hand-varied |
| 9 | house | Lead + fill bass | kDiatonicTranspose / kRetrograde | Option 1 | same shape as disco |
| 10 | rock | Lead + fill bass | kDiatonicTranspose / kRetrograde | Option 1 | comp already hand-varied |
| 11 | pop | Lead + fill bass | kDiatonicTranspose / kRetrograde | Option 1 | most genre-generic style |
| 12 | bossa | Bass; Lead is a MODEL GAP | kRetrograde; Option 2 for lead | Option 1 + flagged Option 2 | no lead content exists |
| 13 | samba | Bass (flagged); Lead is a MODEL GAP | kRetrograde; Option 2 for lead | Option 1 + flagged Option 2 | surdo-lock risk + no lead |
| 14 | funk | Fill bass; stab line is a MODEL GAP | kRetrograde; Option 2 for stab | Option 1 + flagged Option 2 | signature content missing, not just unvaried |
| 15 | ballad | Fill bass + Arp; melody is a MODEL GAP | kRetrograde / kDisplacement; Option 2 for lead | Option 1 + flagged Option 2 | genre IS melody, has none |
| 16 | basic | Fill bass + Arp | kRetrograde / kDisplacement | Option 1 | neutral scaffold, no genre to protect |

**Which styles need a transform the engine may lack: none, for the SHIPPABLE
Option-1 plan above.** All 16 close with only the three existing transforms.
The one place a REAL transform gap exists is named, not assumed: none of the
16 plans ask for "chord-tone function variety" (e.g., alternating which chord
tone the bass plays, a restack rather than a rhythmic reflection/shift) —
that would need a new transform kind operating on `NoteSource::kChordTone`'s
`tone` field, which is exactly Fork E in the prior scope doc, already
deliberately deferred. I reaffirm that deferral; it is not needed for any of
the 16 plans above, all of which get their bass payoff from `step`-only
transforms (`kRetrograde`/`kDisplacement`).

---

## 5. What needs deciding / what I flagged (not decided alone)

1. **Finding B bass exclusions (reggae excluded outright; latin, samba
   flagged as lower-confidence) are a musical judgment call, not a
   mechanical rule** — the owner or Nazzareno may reasonably choose to wire
   them anyway and listen, rather than hold them. I recommend NOT shipping
   them in the same golden-regen batch as the 13 unflagged lanes, so a
   regression there is isolated and reviewable on its own.
2. **The five Option-2 (generate) recommendations — bossa, samba, funk,
   ballad — are new musical authorship** (picking `center_degree`,
   `idiom_role`, and implicitly a register/velocity/gate feel per style),
   not mechanical wiring of existing content. They are real, and in ballad's
   case arguably the single highest-value item in this whole document (a
   ballad style with no melody), but they cost more author-decision time
   than the 12 Option-1-only styles and carry the largest golden delta since
   they introduce content that does not exist today. Recommend staging them
   as a distinct, later batch from the Option-1 wiring.
3. **The `kMaxRetries`/no-amount-control constraint (§1)** is a real,
   already-existing engine property, not something I am asking to change —
   flagged only so the owner knows every choice above is "pick a transform
   kind and a seed," never "pick how far it shifts."
4. **No new dependency anywhere in this document.** Every recommendation
   reuses `MotifSpec`/`MotifTransform` exactly as shipped; the two Option-2
   parameters per style (`center_degree`, `idiom_role`) are existing struct
   fields, not new surface.

# Phase 7 / 9210 — Option-2 generated leads: concrete parameters for bossa, samba, funk, ballad

Ottorino, musical authoring proposal (read-only on product code; this document is
the only artifact written). Scope: the FOUR generated leads that the Option-1
batch (commit `f850ea2`, `docs/reflections/phase7-9210-motif-authoring-all16.md`
§3.12–3.15, §5.2) deliberately left open — bossa, samba, funk, ballad, each a
`motif::generate` (Option-2) seed, not a transform of existing authored content.
Every value below is a PROPOSAL for owner sign-off. Once signed off, wiring is
mechanical (`Giotto`): add one new `TrackRole::kLead` `StylePattern` entry with
`.events` empty (Option-2 selector) and `.motif = &kLeadMotif` to each style's
peak-variation section, exactly the shape already used 11 times in the corpus.

---

## 1. What I measured (evidence, not recall)

- **`MotifSpec` shape** (`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:136-148`):
  `transform`, `seed`, `length` (onsets, generator-only), `center_degree`
  (generator contour center), `vel`, `gate` (generator-only), `idiom_role`
  (whose same-section `kFixed` pattern supplies the candidate onset steps).
- **`motif::generate`** (`components/core/arrangrr/include/arrangrr/arranger/motif.hpp:165-226`):
  spreads `length` onsets evenly (with seeded jitter) across the `idiom_role`'s
  own onset mask, walks a bounded-leap (`kMaxLeap = 4` diatonic steps,
  `motif.hpp:60`) contour around `center_degree`, snaps the LAST onset to the
  nearest stable degree (root/fifth, `is_stable_degree`, `motif.hpp:71-74`), and
  hardcodes `ev.octave = 0` and `ev.src = NoteSource::kScaleDegree` for every
  generated event (`motif.hpp:216-221`) — **register control is `center_degree`
  alone**, resolved via `theory::degree_to_semitones` (unbounded, octave-wrapping,
  `components/core/chorddet/include/chorddet/theory.hpp:105-117`), not a separate
  `octave` field.
- **Resolution/register anchor**: `Arranger::resolve` (`arranger.hpp:750-784`)
  computes, for `NoteSource::kScaleDegree`: `note = kRoleAnchor[kLead] +
  key.root_pc + degree_to_semitones(mode, tone) + 12*octave + transpose`, with
  `kRoleAnchor[kLead] = 72` (`arranger.hpp:793-804`, C5) — the SAME anchor
  already used for `kArp`/`kPhrase`.
- **The `kLead` role already exists 11 times in the corpus, and I grep'd every
  instance's `MotifSpec`**: blues, motown, swing, shuffle, country, latin,
  reggae, disco, house, rock, pop — **all 11 of 11 use `MotifTransform::
  kDiatonicTranspose`**. None use `kRetrograde` or `kDisplacement` on a `kLead`
  role (those are reserved for bass/arp motifs in the same batch). This is not
  a coincidence of authoring convenience — it is the corpus's own idiom: a
  melodic "answer" phrase is varied by re-pitching it (sequencing the lick up
  or down a diatonic step, the real jazz/pop/Motown-horn device), never by
  time-reversing or phase-shifting a sung/blown line.
- **Every authored `kScaleDegree` `kLead` phrase I read in full** (motown
  `kHornAnswer`, latin `kMamboHorn`, reggae `kMelodica`, disco `kLeadLine`,
  house `kLeadHook`) sits in the **degree range 2–7** (3rd through octave above
  the key tonic) — never at or below the root, confirming `kRoleAnchor[kLead]
  = 72` is meant to carry a line ABOVE the mid-register comping (anchor 60) it
  answers, and every one of them is wired into exactly ONE section (its
  style's peak variation — `kDP`/`kVarD`), never every section.
- **Onset-mask candidates I computed directly from each style's own VarD/`kDP`
  `kFixed` pattern** (the field `idiom_onset_mask` will actually read):
  - bossa `kDD` (drums, `bossa.hpp:83`): side-stick `{0,3,6,8,10,11,14}` + kick
    `{0,4,8,12}` + closed hat `{0,2,4,6,8,10,12,14}` → **10 distinct candidate
    steps**.
  - samba `kPercRich` (perc, `samba.hpp:29`): guiro `{0,4,8,12}` + cowbell
    `{2,6,10,14}` → **8 distinct steps, exactly every even 16th** (samba's
    `kDD` drums are richer, 20+ onsets, but the task's own `idiom_role` is
    fixed to `kPerc` for samba, not `kDrums`).
  - funk `kDD` (drums, `funk.hpp:166`): kick `{0,3,6,8,10,13}` + snare
    `{2,4,7,11,12,15}` + closed/open hat `{0,2,4,6,8,10,12,14}` → **13 distinct
    steps**, the busiest/most syncopated candidate set of the four.
  - ballad `kVarDDrums` (`ballad.hpp:228`): kick `{0,8}` + snare `{4,12}` +
    ride `{0,2,4,6,8,10,12,14}` → **8 distinct steps, every even 16th**.
- **Current file state, confirmed by read, not assumed**: samba currently has
  **zero** `MotifSpec` instances at all (its bass motif is still HOLD per
  Finding B); bossa already carries `kTwoBassMotif` (seed 1201, bass only);
  funk already carries `kFillBassMotif` (seed 1401, fill-bass only); ballad
  already carries `kFillBassMotif` (1501) and `kArpHarpMotif` (1502). The seeds
  proposed below are the next free slot in each style's own numbering, so no
  collision if the still-HOLD bass/lead lanes are adopted later.
- **Gate-length vocabulary** (`style_model.hpp:253-259`, PPQN 960): `kGateStaccato
  = 50`, `kGateHat = 120`, `kGateStab = 200`, `kGate8th = 240`, `kGateBeat =
  360`, `kGateHalfBar = 1800`, `kGateHeld = 3600` (just under a full bar).
- **Musicological grounding** (external, cited): bossa nova melody is
  "lyrical and conversational... stepwise motion and gentle syncopation," the
  nylon guitar's *batida* implying the pulse while the voice/melody floats
  around it — [Guitar World, "Hole Notes"](https://www.guitarworld.com/uncategorized/hole-notes-bossa-nova-rhythms-antonio-carlos-jobim),
  [siccasguitars.com](https://www.siccasguitars.com/blogs/stories/joao-gilberto-classical-nylon-guitar).
  Samba's cavaquinho is "the quintessential melodic voice," while the melody is
  "often carried by a flute or clarinet, providing ornamented, virtuosic
  lines," against a surdo/tamborim rhythm bed —
  [Grokipedia, samba-sincopado](https://grokipedia.com/page/samba_sincopado),
  [educationmagz.blog](https://www.educationmagz.blog/musical-instruments-brazil-history-sound).
  Funk is defined by "tight horn stabs riding a static, one-chord vamp" and
  the clavinet's "bright, biting, percussive" tone, syncopated onto the "and"
  16ths — [Splice, "What is Funk?"](https://splice.com/blog/what-is-funk-music/),
  [Sound on Sound, "Top Brass"](https://www.soundonsound.com/techniques/top-brass-part-2).

---

## 2. The proposed table

All four use **`kDiatonicTranspose`** (§1, dominant unanimous corpus evidence —
not an open fork) and are wired as a single new `TrackRole::kLead` pattern into
each style's **peak/fullest variation only** (`bossa::kDP`/VarD, `samba::kDP`/VarD,
`funk::kDP`/VarD, `ballad::kVarDPatterns`), matching every one of the 11 existing
`kLead` instances' own placement convention.

| Style | Section | idiom_role | transform | seed | length | center_degree | vel | gate | gm_program |
|---|---|---|---|---|---|---|---|---|---|
| **bossa** | VarD (`kDP`) | `kDrums` | `kDiatonicTranspose` | 1202 | 6 | 4 (5th, +7 st → G5 @72 anchor) | 64 | `kGate8th` (240) | 73 — Flute |
| **samba** | VarD (`kDP`) | `kPerc` | `kDiatonicTranspose` | 1302 | 6 | 7 (octave, +12 st → C6) | 78 | `kGateStaccato` (50) | 73 — Flute (flagged, §4.1) |
| **funk** | VarD (`kDP`) | `kDrums` | `kDiatonicTranspose` | 1402 | 3 | 0 (root, → C5) | 96 | `kGateStab` (200) | 61 — Brass Section (flagged, §4.2) |
| **ballad** | VarD (`kVarDPatterns`) | `kDrums` | `kDiatonicTranspose` | 1503 | 3 | 2 (3rd, +4 st → E5) | 62 | `kGateHeld` (3600, flagged, §4.3) | 53 — Voice Oohs (flagged, §4.3) |

Illustrative attach shape (NOT a patch — Giotto decides the exact diff):

```cpp
// bossa.hpp, additive, alongside kTwoBassMotif:
inline constexpr MotifSpec kLeadMotif{
    .transform = MotifTransform::kDiatonicTranspose,
    .seed = 1202, .length = 6, .center_degree = 4, .vel = 64, .gate = kGate8th,
    .idiom_role = TrackRole::kDrums,
};
// ...appended into kDP's StylePattern list:
{.role = TrackRole::kLead, .policy = RolePolicy::kChordTone,
 .events = Span<const StyleEvent>(), .gm_program = 73, .motif = &kLeadMotif}
```

`.events` stays default-empty — that empty span IS the Option-2 selector
(`motif.hpp:124-136`); `.policy = kChordTone` is not a misnomer here, it is the
same "not `kFixed`" placeholder every one of the 11 existing `kLead` patterns
already uses, since the per-event `NoteSource::kScaleDegree` (hardcoded by
`generate()`) is what actually governs resolution, not the pattern-level
`RolePolicy`.

---

## 3. Rationale per line

**bossa — `center_degree=4`, `vel=64`, `gate=kGate8th`, length 6, Flute@73.**
Bossa's own header comment already names its bass "smooth, unaccented"; the
5th (a stable cadence degree in `is_stable_degree`, so the contour naturally
gravitates back there) gives a floating, non-aggressive center that matches
the genre's understated register, one register class above Chord1/Chord2's
mid comping (anchor 60) without leaping to the very top (samba's octave,
below). `vel=64` sits below bossa's Chord1 stabs (66-84) and above its softest
pad tones (44-52) — present but never assertive, matching "a conversation
above the harmony" rather than a stabbed accent. `kGate8th` (an 8th note) gives
a connected, legato-leaning line without the full-bar smear `kGateHeld` would
cause at 6 onsets/bar — it is the same gate class the analogous
`kDiatonicTranspose` `kLead` lines elsewhere in the corpus (disco `kLeadLine`,
house `kLeadHook`) use for their moving notes. `idiom_role=kDrums` reuses
bossa's own side-stick/kick/hat mask (10 candidate steps) so the generated
line breathes with the actual cross-stick groove, per the reflection's own
recommendation.

**samba — `center_degree=7`, `vel=78`, `gate=kGateStaccato`, length 6,
Flute@73 (flagged).** Real samba melody is carried by a flute sitting HIGH and
bright over the surdo/tamborim bed (researched, §1); degree 7 (a full stable
octave above tonic, C6 at the kLead anchor) gives that bright, cutting
register, distinct from bossa's mid-floating 5th. `vel=78` matches samba's own
Chord1 strum velocities (76-86) — present and driving, matching the genre's
higher overall dynamic (samba kicks reach vel 118) without out-shouting the
comping. `kGateStaccato` gives the bouncy, articulate attack samba's own
Chord1 (`kAC`/`kBC`, both `kGateStaccato`) already uses, rather than bossa's
smoother legato feel — genuinely differentiating the two "Latin" siblings on
articulation, not just tempo. `idiom_role=kPerc` (fixed by the task's own
framing, §1) reuses `kPercRich`'s 8-step even-16th mask.

**funk — `center_degree=0`, `vel=96`, `gate=kGateStab`, length 3, Brass
Section@61 (flagged).** Funk's own file header names its bass "staccato
single-note root-pop" and its genre identity (researched, §1) is "tight...
stabs riding a static... vamp" — a stab-lead should reaffirm the tonal center
percussively, not wander melodically, so `center_degree=0` (root) mirrors the
bass's own idiom directly. `length=3` is deliberately the SHORTEST of the four
— the reflection's own language is "short, syncopated... stab," not a line —
and `vel=96` sits close to funk's own Chord1 stab velocities (78-92) and its
kick/snare accents (100-120), i.e. hot enough to read as an accent, not a
melodic pad. `kGateStab` (the gate literally named for this articulation, and
the one every stab-comping role in the corpus already uses) is the direct
match. `idiom_role=kDrums` draws from funk's own 13-step syncopated candidate
mask — the richest, most syncopated onset source of the four — so the stab's
placement inherits funk's own 16th-note push-and-pull instead of falling on
the beat.

**ballad — `center_degree=2`, `vel=62`, `gate=kGateHeld`, length 3, Voice
Oohs@53 (flagged).** This is the reflection's own most-conspicuous model gap:
"a ballad style with no vocal-style line at all." `center_degree=2` (the
3rd) is the classic vocal-melody color tone — warmer and more "singing" than
a root or fifth center — sitting at E5 against the C5 `kLead` anchor.
`vel=62` sits just above the harp arp (58) and below the drum backbeat
accents (78-86), i.e. the most prominent single voice in the mix while
staying inside ballad's own soft dynamic envelope (everything else in the file
sits at vel 44-90). `length=3` is intentionally minimal: combined with
`gate=kGateHeld` (3600 ticks, ~15 sixteenths) the three onsets legato-overlap
across almost the whole bar by design — each new onset's note-on effectively
cuts the previous note's tail, producing a single, continuously-sounding
melodic line rather than three separate stabs, which is the "slow,
`kGateHeld`-style" line the reflection asked for. `idiom_role=kDrums` draws
from ballad's own 8-step even-16th ride/kick/snare mask.

---

## 4. What I flagged (not decided alone)

**4.1 — samba's `gm_program`.** I proposed Flute (73) for BOTH bossa and
samba, on real evidence (both genres' melody is carried by flute/clarinet over
a guitar/cavaquinho comp, §1). But bossa's own Chord2 is already "Nylon
Guitar" (24) and samba's Chord2 is the SAME nylon-guitar voice standing in for
cavaquinho — putting an identical Flute lead on top of both would make two
adjacent "Latin" styles sound MORE alike at the very moment 9210 exists to
differentiate them. An honest alternative: Piccolo (GM 72, one octave above
Flute's usual register) for samba specifically, to keep the bright/high
character while giving the two styles a genuinely distinct timbre. I did not
resolve this myself — it is a real aesthetic call between "correct per genre"
(both are flute-family lines) and "distinct per pair" (this corpus's stated
anti-sameness goal).

**4.2 — funk's `gm_program`.** The reflection's own language is "a syncopated
clav/horn stab," naming BOTH instrument families and picking neither. I chose
Brass Section (61) because it is the corpus's own established "stab lead"
voice (motown, latin already use it for the identical idiom), but Clavinet (GM
7) is at least as authentic to funk specifically (researched, §1: "the
Hohner Clavinet's bright, biting, percussive tone is a funk signature") and
would differentiate funk's stab from motown/latin's horn timbre rather than
repeating it. Genuinely a coin flip between "reuse the corpus's proven stab
voice" and "give funk its own signature texture" — owner call.

**4.3 — ballad's `gate`/`gm_program` pairing.** `kGateHeld` is what the
reflection literally asked for, and I kept it, but I want the tension named
plainly: at `length=3` with `kGateHeld`, EVERY generated note overlaps its
neighbor by design (§3) — a deliberate continuous-legato read, not a bug, but
it is a different articulation than any other generated/authored `kLead` line
in the corpus (all of which use `kGate8th`/`kGateStab`/`kGateBeat`, none
`kGateHeld`). If the owner wants a still-legato-but-audibly-separated phrase
instead, `kGateHalfBar` (1800) is the safer middle ground. Separately, Voice
Oohs (53) is my literal read of "vocal-style," but Alto Sax (65) is the more
conventional real-instrument choice for a solo ballad melody in arranger-style
practice generally — both are honest options, I have not picked for the
owner.

**4.4 — `center_degree`/`length` are genuinely the load-bearing, most
subjective numbers here**, exactly as the task named them. Every value above
is grounded in a measured corpus convention or a researched genre fact (§1,
§3), but they are STILL an authored aesthetic choice, not a derivation — the
owner should treat the whole middle three columns of the table (length,
center_degree, vel/gate) as the actual sign-off, not a formality.

**4.5 — No new dependency, no engine change.** Every value in this proposal
targets `MotifSpec`/`motif::generate` exactly as they ship today
(`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`,
`components/core/arrangrr/include/arrangrr/arranger/motif.hpp`); wiring is
constexpr, no-heap, dual-target (host + arm-none-eabi) by construction, same
class of change as the already-shipped Option-1 batch. **SHIPPABLE** once the
table above is signed off.

---

## 4b. OWNER SIGN-OFF (2026-07-16) — LOCKED

The owner reviewed all four §4 flags and the §2 table and signed off:

- **4.1 samba `gm_program` → Piccolo (GM 72)** (NOT Flute 73) — the
  anti-sameness choice: keep bossa on Flute, give samba a distinct bright
  timbre so the two "Latin" siblings do not converge.
- **4.2 funk `gm_program` → Clavinet (GM 7)** (NOT Brass 61) — funk's own
  signature texture, distinct from motown/latin's horn stab.
- **4.3 ballad → Voice Oohs (GM 53) + `kGateHeld` (3600)** — the literal
  "vocal-style" line the reflection asked for; the deliberate length-3 legato
  overlap (§3) is accepted as intended.
- **4.4 table (center_degree / length / vel / gate) — accepted as proposed**,
  no revisions.

Final locked table (supersedes §2's samba/funk/ballad `gm_program` and stands
for everything else):

| Style | Section | idiom_role | transform | seed | length | center_degree | vel | gate | gm_program |
|---|---|---|---|---|---|---|---|---|---|
| bossa | VarD (`kDP`) | `kDrums` | `kDiatonicTranspose` | 1202 | 6 | 4 | 64 | `kGate8th` | **73 Flute** |
| samba | VarD (`kDP`) | `kPerc` | `kDiatonicTranspose` | 1302 | 6 | 7 | 78 | `kGateStaccato` | **72 Piccolo** |
| funk | VarD (`kDP`) | `kDrums` | `kDiatonicTranspose` | 1402 | 3 | 0 | 96 | `kGateStab` | **7 Clavinet** |
| ballad | VarD (`kVarDPatterns`) | `kDrums` | `kDiatonicTranspose` | 1503 | 3 | 2 | 62 | `kGateHeld` | **53 Voice Oohs** |

Golden regeneration for these four styles is pre-authorized as part of node
9210 (owner Gate-A lock, "9210 all-16 styles, goldens will move").

---

## 5. Golden impact (for Giotto's staging, not a claim I can verify myself)

All four styles gain a wholly NEW `TrackRole::kLead` voice in their peak
variation where none exists today — this is new content, not a transform of
existing content, so it is the largest per-style golden delta of any 9210
batch so far (matching the reflection's own §5.2 prediction). Every other
section/role in these four files is untouched; repeat-0 playback of every
OTHER section stays byte-identical.

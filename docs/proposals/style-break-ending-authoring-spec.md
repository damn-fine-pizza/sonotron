# Authoring spec: 2-bar endings with real cadences (16 styles) + breaks for the 10 styles that lack one

Status: implementation-ready authoring spec (data-only), 2026-07-18. Author:
Ottorino (style/arrangement analyst). Owner has GREEN-LIT both fronts below for
implementation. This document is a SPEC — it specifies exact `StyleEvent`
content for a junior implementor to transcribe into the existing constexpr
tables; it is not itself a code change.

See also: `docs/proposals/style-break-ending-depth-and-exposure.md` (the
diagnosis this spec executes on — read it first for WHY), and
`docs/proposals/per-style-default-progressions.md` (the host-side
`ChordSequence` proposal referenced in §0.3 below).

**Assumption locked by the coordinator**: the `kBreak` auto-return-after-1-bar
core fix (`style-break-ending-depth-and-exposure.md` §4 item #5) is being
implemented separately. Every break design in Part B assumes the section
plays exactly once (1 bar) and then the engine returns to the active
variation on its own — breaks are authored here as a true 1-bar interruption,
not a vamp to hold indefinitely.

---

## 0. Schema, units, and the cadence device vocabulary (read this before anything else)

### 0.1 The exact schema, cited from real style headers

`StyleEvent` (`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:88-101`):

```cpp
struct StyleEvent {
  std::uint16_t step;   // 16th-grid position WITHIN THE SECTION (not the bar)
  std::int8_t tone;     // meaning depends on `src` (see 0.2)
  std::int8_t octave;   // octave offset, ADDED on top of `tone`'s pitch
  std::uint8_t vel;     // MIDI velocity, 1-127
  std::uint16_t gate;   // duration in TICKS (PPQN = 960)
  NoteSource src = NoteSource::kChordTone;      // optional, see 0.2
  ChordGesture gesture = ChordGesture::kNone;   // optional, see 0.4
};
```

Real examples, copied verbatim from the tree:

- `rock.hpp:252-254` (ending1 bass/chord, `RolePolicy::kChordTone`):
  ```cpp
  inline constexpr StyleEvent kEndBass[] = {
      {.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGateHeld},
  };
  inline constexpr StyleEvent kEndChord[] = {
      {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateHeld},
      {.step=0, .tone=kFifth, .octave=0, .vel=112, .gate=kGateHeld},
      {.step=0, .tone=kRoot, .octave=1, .vel=112, .gate=kGateHeld},
  };
  ```
- `blues.hpp:50-56` (`kLeadLick`, `NoteSource::kInterval` — the vocabulary
  Part A's cadence devices reuse):
  ```cpp
  inline constexpr StyleEvent kLeadLick[] = {
      {.step=8,  .tone=0,  .octave=0, .vel=76, .gate=kGate8th,      .src=NoteSource::kInterval},
      {.step=10, .tone=3,  .octave=0, .vel=70, .gate=kGate8th,      .src=NoteSource::kInterval},
      {.step=11, .tone=6,  .octave=0, .vel=66, .gate=kGateStaccato, .src=NoteSource::kInterval},
      {.step=12, .tone=7,  .octave=0, .vel=74, .gate=kGate8th,      .src=NoteSource::kInterval},
      {.step=14, .tone=10, .octave=0, .vel=72, .gate=kGateBeat,     .src=NoteSource::kInterval},
  };
  ```
- `rock.hpp:343` (`ChordGesture::kStrumDown`, the gesture vocabulary §0.4 uses):
  ```cpp
  {.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown},
  ```

`StylePattern` wraps one role's events with its policy and voice
(`style_model.hpp:150-167`, `rock.hpp:259-264`):

```cpp
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass,  .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1,.policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
    {.role=TrackRole::kPad,   .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7),
     .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};
```

`StyleSection` wraps the pattern array with `.bars` (`rock.hpp:380-381`):

```cpp
{.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
{.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
```

**Growing to 2 bars is exactly**: change `.bars=1` to `.bars=2` on the
`StyleSection` line, and add new `StyleEvent` entries with `step` in the
`[16, 31]` range to each role's array (bar 2 = steps 16-31, bar 1 = steps
0-15 — 16 steps/bar, unchanged). This exact scheme is already proven and used
16/16 times today for `Intro1` (every style already authors `Intro1` at
`.bars=2`; e.g. `rock.hpp:266-279`, `kIntro2Drums`/`kIntro2Chord`/
`kIntro2Patterns` use `step=16..30` unremarkably). No new field, no cap: no
`kMaxBars` exists anywhere in `components/core` (grepped, confirmed absent);
`StyleSection::bars` is `std::uint8_t`.

### 0.2 `tone` semantics — three encodings, pick per event

- `NoteSource::kChordTone` (default, omit `.src`): `tone` is a **chord-tone
  index** into the LIVE chord at that tick — `kRoot=0`, `kThird=1`,
  `kFifth=2`, `kSeventh=3` (`style_model.hpp:247-250`). This is what "the
  chord" means everywhere in this corpus; it always tracks whatever chord is
  actually playing, never a hard-coded pitch.
- `NoteSource::kInterval`: `tone` is a **signed semitone offset from the live
  chord's ROOT**, evaluated at playback (`style_model.hpp:59`,
  `blues.hpp:46-49`'s own comment: "every note is kInterval... bends against
  whichever of the I/IV/V chords is live"). This is the mechanism this spec
  uses for every color-tone/approach-tone cadence device in §0.3 — it is
  root-relative, so it stays musically correct regardless of which chord is
  actually live when the ending/break fires.
- `NoteSource::kScaleDegree`: `tone` is a scale degree of the current KEY —
  not used in this spec (no style in the corpus authors an ending melody
  against the key rather than the chord; keeping every device chord-relative
  is deliberately conservative and matches the corpus's existing idiom).

### 0.3 What "cadence" honestly means in THIS architecture — read before designing anything

**The live chord is host-supplied and NOT something `StyleEvent` data can
change.** `Arranger::on_tick` resolves chord-tone/interval events "against
the live chord" at the current tick (`arranger.hpp:19`); the actual harmonic
progression (which chord is playing at any given bar) comes from the host's
`ChordSequence`, fed per-style by `docs/proposals/per-style-default-
progressions.md` (a separate, not-yet-landed proposal) or by a live
chord/key panel later. **A style table cannot itself author a V→I or
ii→V→I with different ROOTS** — there is no field for it, and it would be
architecturally wrong to add one (memory: harmony-progression-architecture —
"default progression = host-supplied ChordSequence per style, NEVER a core
StyleDef field", precisely so styles stay user-definable via SFF import
without a baked-in progression).

So every "cadence" below is authored as one of two honest, StyleEvent-only
devices, both of which read as a real cadential arrival regardless of which
chord is actually live, and both of which get MORE convincing (a true
harmonic resolution) for free, later, once a per-style default progression
that lands on I at the ending is wired up — but do not require it to sound
like a real ending today:

- **Device 1 — Suspension resolution (sus4→3).** A `kInterval` tone at
  offset `+5` (a 4th above the live root) sounds briefly, THEN resolves down
  to the chord's own `kThird` (chordtone index 1) on the final downbeat. This
  is the actual suspension-and-resolution device tonal music uses for a
  cadential arrival, and it works against ANY live chord quality — the
  classic "hold... then let go" arranger-ending gesture.
- **Device 2 — Leading-tone approach.** A `kInterval` tone at offset `-1`
  (a half-step below the live root) sounds briefly, THEN resolves UP to
  `kRoot` on the final downbeat. The classical cadential-leading-tone motion,
  chord-quality-agnostic.
- **Device 3 — Dominant/blues color (b7 resolution).** A `kInterval` tone at
  offset `+10` (the b7 above the live root — exactly blues.hpp's own
  `kLeadLick` vocabulary) sounds, THEN resolves down to `kRoot` or `kThird`.
  Reads as a soul/blues/funk "vamp to the one," not a classical cadence —
  used only in the families where that is the idiomatically correct color
  (blues/motown/funk/dance).
- **Device 4 — Plagal/gospel color (Amen).** `kInterval` offsets `+9` (6th)
  and `+5` (4th) stacked, THEN resolving to the closed
  `kRoot`+`kThird`+`kFifth` triad. The "IV-I" arranger-ending color without
  requiring an actual root change — used for the pop/rock/country/gospel-
  adjacent family's BIGGER (ending2) tag.
- **Device 5 — Rhythmic tag, no pitch motion.** Chord tones only
  (`kChordTone`, no interval color), cadence expressed ENTIRELY through
  rhythm/density/register (a hit, silence, a bigger hit) — the honest choice
  for funk and the clave-based Latin styles, whose real-world endings are a
  rhythmic convention, not a harmonic one (see Part A §A3/§A5).

Finality is expressed **rhythmically**, never via tempo (there is no
ritardando/tempo-curve mechanism in core — confirmed absent by grep,
`style-break-ending-depth-and-exposure.md` §2.2): longer gate values on the
final chord (`kGateHeld` ≈ 3600 ticks, "just under a full bar" from its own
onset), FEWER onsets as bar 2 approaches (a thinning drum part is a
"deceleration" the ear reads as finality even at a fixed BPM), and a clear,
louder downbeat on step 16 (beat 1 of bar 2) that every role lands on
together.

### 0.4 `ChordGesture` — use it; the current corpus uses it ZERO times in any break/ending

`kStrumUp`/`kStrumDown`/`kRollUp`/`kRollDown` (`style_model.hpp:70-76`) turn
one `StyleEvent` into a spread/arpeggiated attack instead of a flat
simultaneous block-chord hit. This spec uses `kRollUp` for the softest
family (ballad) and `kStrumDown` for the guitar-idiom families (rock/country)
— see the per-family tables. This single change (already proven code,
already used in every style's main variations, e.g. `rock.hpp:343`) is what
turns a "4 notes fire at once" hold into an audible, human gesture.

### 0.5 Role availability per style (checked, not assumed)

Every one of the 16 styles already declares `kChord2Voice`/`kArpVoice`
(`grep -c` across all 16 files: 5 and 3 hits respectively per file — the
per-style GM voice constant plus its uses), so `Chord2`/`Arp` roles are safe
to use in every style's ending/break. `kLeadVoice` is declared in 15/16
styles — **`basic` alone has NO `kLeadVoice` constant** (`grep -c
kLeadVoice basic.hpp` → 0) — so `basic`'s ending/break must not add a `Lead`
role; every other style may. `TrackRole::kPerc` is already used in all 16
styles' main variations (verified by grep), so a Perc hit in a break/ending
is idiomatically safe everywhere it is called for below.

### 0.6 What is explicitly OUT of scope / flagged, not silently assumed

- No new dependency, no core/ABI change anywhere in this spec, beyond the
  separately-handled `kBreak` auto-return fix (already assumed done, per the
  coordinator's brief).
- `TrackRole::kCc` (Control Change automation) exists in the enum
  (`timeline.hpp:36`) but is used by ZERO styles in the corpus today (grepped,
  confirmed) — a filter-sweep/CC-automation ending (e.g. a house-style filter
  close) would be a genuinely good device but is NOT included in this spec
  because there is no existing precedent proving the render path actually
  honors a style-authored CC event end-to-end; flagged as a follow-up
  worth a small spike, not assumed safe here.
- True multi-root harmonic cadences (a real V→I) depend on the host's
  `ChordSequence` (per `docs/proposals/per-style-default-progressions.md`)
  landing a dominant chord immediately before the ending fires — that
  proposal is not this spec's to redesign; this spec's cadences work
  correctly with or without it (§0.3).

---

## 1. Sharding plan (unit boundaries for parallel junior implementors)

Both parts are sharded along the SAME axis: `StyleFamily`
(`apps/gui-sonotron/src/browser_model.hpp:234-293`), the corpus's own
already-measured rhythmic-family clustering — reusing it here means the
junior assigned a family already has the right musical mental model for
every style in their shard.

| Shard | Styles | Part A (2-bar ending1+ending2) | Part B (new 1-bar break) |
|---|---|---|---|
| 1 — kPopRockBallad | pop, rock, ballad, country, motown | all 5 (§A.1) | pop, ballad, country (§B.1) — rock/motown already have a break |
| 2 — kFunkGroove | funk | 1 (§A.2) | none — funk already has a break |
| 3 — kDanceFourOnFloor | disco, house | both (§A.3) | house (§B.2) — disco already has a break |
| 4 — kSwingShuffleJazz | swing, blues, shuffle | all 3 (§A.4) | swing, shuffle (§B.3) — blues already has a break |
| 5 — kLatinClave | bossa, samba, reggae, latin | all 4 (§A.5) | bossa, samba, reggae (§B.4) — latin already has a break |
| 6 — kOther | basic | 1 (§A.6) | basic (§B.5) |

Each shard is a fully disjoint set of style header files — no two juniors
ever touch the same `.hpp`. Six shards is a reasonable split across 2-3
juniors (e.g. shard 1+2 to one, 3+4 to another, 5+6 to a third), or one
junior per shard if six are available.

---

## Part A — 2-bar endings, all 16 styles

**For every style below: bar 1 (steps 0-15) of ending1 and ending2 is LEFT
UNCHANGED — it is exactly what is authored in the tree today (already
measured and catalogued in `style-break-ending-depth-and-exposure.md` §2).
Only §0.1's two edits are new: `.bars=1` → `.bars=2` on both
`kEnding1`/`kEnding2` `StyleSection` entries, and the bar-2 (`step=16..31`)
events below appended to each role's existing `StyleEvent` array.**

Ending1's bar 2 = Device-driven cadential arrival (a single, clean
resolution). Ending2's bar 2 = a genuinely DIFFERENT idea, not "ending1 + one
more hit" (rejecting today's pattern) — a bigger/second tag using a
different device or a call-and-response rhythmic shape, per family below.

### A.1 — kPopRockBallad (pop, rock, ballad, country, motown)

**Shared bar-2 shape, ending1 — "suspension into the hold" (Device 1):**
adds a pickup on the LAST TWO STEPS of bar 1 (steps 14-15, already-existing
array, append these events) then resolves on step 16.

| Role | New events (append to the existing array) |
|---|---|
| Chord1 | `{.step=14, .tone=5, .octave=0, .vel=?, .gate=kGate8th, .src=NoteSource::kInterval}` (sus4 pickup) then `{.step=16, .tone=kThird, .octave=0, .vel=?, .gate=kGateHeld}` `{.step=16, .tone=kRoot, .octave=0, .vel=?, .gate=kGateHeld}` `{.step=16, .tone=kFifth, .octave=0, .vel=?, .gate=kGateHeld}` (resolved closed triad) |
| Pad | `{.step=16, .tone=kRoot, .octave=0, .vel=?, .gate=kGateHeld}` `{.step=16, .tone=kThird, .octave=0, .vel=?, .gate=kGateHeld}` `{.step=16, .tone=kFifth, .octave=0, .vel=?, .gate=kGateHeld}` `{.step=16, .tone=kSeventh, .octave=0, .vel=?, .gate=kGateHeld}` (restate the held 7th chord into bar 2 so the pad doesn't cut out before the section ends — bar 1's `kGateHeld` note ends ~240 ticks before step 16) |
| Bass | `{.step=16, .tone=kRoot, .octave=0, .vel=?, .gate=kGateHeld}` (restate the held root into bar 2) |
| Drums | `{.step=16, .tone=kKick, .octave=0, .vel=?, .gate=kGateHat}` `{.step=16, .tone=kCrash, .octave=0, .vel=?, .gate=kGateHeld}` (the arrival hit) |

`vel` guidance: match each style's existing bar-1 velocity for that role/hit
type (do not invent new dynamics — the arrival should feel continuous with
bar 1, just held longer).

**Shared bar-2 shape, ending2 — "Amen tag" (Device 4), genuinely distinct
from ending1:**

| Role | New events |
|---|---|
| Chord1 | `{.step=16, .tone=9, .octave=0, .vel=?, .gate=kGateBeat, .src=NoteSource::kInterval}` (6th color, the "IV" half of the tag) `{.step=16, .tone=5, .octave=0, .vel=?, .gate=kGateBeat, .src=NoteSource::kInterval}` (stacked 4th) — SHORT gate, this hit releases — then `{.step=24, .tone=kRoot, ...}` `{.step=24, .tone=kThird, ...}` `{.step=24, .tone=kFifth, ...}` `{.step=24, .tone=kRoot, .octave=1, ...}` gate `kGateHeld` (the BIG final hit, root doubled an octave up, on beat 3 of bar 2 — NOT beat 1, so it reads as a second, later, bigger arrival than ending1's) |
| Bass | `{.step=16, .tone=kRoot, .octave=0, .vel=?, .gate=kGateBeat}` (short, releases) `{.step=24, .tone=kRoot, .octave=-1, .vel=?, .gate=kGateHeld}` `{.step=24, .tone=kRoot, .octave=0, .vel=?, .gate=kGateHeld}` (doubled octave on the big hit) |
| Drums | `{.step=16, .tone=kKick, .gate=kGateHat}` (short punctuation) `{.step=24, .tone=kKick, .gate=kGateHat}` `{.step=24, .tone=kCrash, .gate=kGateHeld}` (the big hit) |
| Pad | `{.step=24, .tone=kRoot/.../kSeventh (all 4), .gate=kGateHeld}` (restate full chord under the big hit only — silent under the step-16 color tag, reinforcing that step 16 is the SMALL half of the tag and step 24 is the big one) |

**Per-style deltas (apply on top of the shared shape above):**

- **rock**: voice Chord1/Pad as the style's own established POWER-CHORD
  shape (root+fifth+root-octave-up, no third — matches the existing
  `kEndChord`/`kVarDChord` voicing already in `rock.hpp`), add
  `.gesture=ChordGesture::kStrumDown` on the step-16 and step-24 Chord1 hits
  (mirrors `rock.hpp:343`'s own existing use). Loud (vel 110+), crash-heavy.
- **pop**: keep the full triad+7th voicing (already `kEndChord` in
  `pop.hpp`), softer velocity (vel ~90-100), add a tambourine/shaker tap on
  Perc at step 16 using whichever of `kPercTamb`/`kPercShake` the style
  already declares (`pop.hpp` has both — use `kPercShake`, matching its own
  main-variation choice).
- **ballad**: the SPARSEST version — omit the step-14 pickup and the
  step-16/24 drum kick entirely (ballads do not add a kick hit for
  "finality"; they let the chord ring). Put `.gesture=ChordGesture::kRollUp`
  on the Pad's step-16 (ending1) and step-24 (ending2) chord — a harp-like
  spread instead of a block hit is the family's one intentional exception to
  "hit on the downbeat," matching the genre's understated dynamic range.
  Crash may stay (a soft cymbal swell, `vel` ≤ 80) but drop the kick.
- **country**: use Device 3 (b7 resolution, `tone=10 .src=kInterval`) INSTEAD
  of Device 1's sus4 for ending1's pickup at step 14 — a country/bluesy
  turnback color is more idiomatically correct than a classical suspension
  here. Add the style's own `kChord2Voice` (steel-guitar patch, already
  declared, used as `kSteel` array elsewhere in `country.hpp`) doubling the
  final chord at step 16/24 for the twang. Reuse `kPercTamb` (already
  declared) on the big hit.
- **motown**: ANTICIPATE the ending2 big hit by one step (step 23 instead of
  24) on Chord1/Chord2 only (drums/bass stay on 24) — a syncopated
  "horn-stab ahead of the beat" is the signature Motown horn-section
  punctuation; use the style's own `kChord2Voice` for this anticipated stab
  (a real horn-section color, distinct from the smoother Pad).

### A.2 — kFunkGroove (funk)

Funk endings are SHORT and STACCATO, not a ringing sustain — this is the one
family where "hold the final chord" is the WRONG genre read (funk's whole
idiom is rhythmic tension via silence, not harmonic sustain). Use Device 5
(rhythmic tag, no pitch motion).

**ending1 — "On the one," bar 2:**

| Role | New events |
|---|---|
| Drums | `{.step=16, .tone=kKick, .vel=124, .gate=kGateStab}` `{.step=16, .tone=kCrash, .vel=118, .gate=kGateBeat}` — ONE hit, short gate, then SILENCE for the rest of bar 2 (no more events) |
| Bass | `{.step=16, .tone=kRoot, .octave=0, .vel=120, .gate=kGateStab}` — one short hit |
| Chord1 | `{.step=16, .tone=kRoot, .vel=112, .gate=kGateStab}` `{.step=16, .tone=kThird, .vel=112, .gate=kGateStab}` `{.step=16, .tone=kSeventh, .vel=112, .gate=kGateStab}` (funk favors the dominant/minor-7 color over a plain triad — keep the 7th, drop the 5th, matching `funk.hpp`'s own existing `kBrkC`/`kEndChord` voicing choice already seen in the corpus) |
| Pad | Silent (funk's Pad role in this corpus is already sparse/absent from tight moments — do not add sustain here; it would fight the staccato read) |

**ending2 — "call and response," bar 2, genuinely distinct:** the band hits
once (as ending1), THEN a drum/perc answers alone.

| Role | New events |
|---|---|
| Drums | `{.step=16, .tone=kKick, .vel=124, .gate=kGateStab}` `{.step=16, .tone=kCrash, .vel=118, .gate=kGateBeat}` (the band hit, same as ending1) then, ALONE, `{.step=24, .tone=kSnare, .vel=110, .gate=kGateHat}` `{.step=26, .tone=kSnare, .vel=100, .gate=kGateHat}` `{.step=28, .tone=kTomLow, .vel=112, .gate=kGateHat}` `{.step=30, .tone=kKick, .vel=118, .gate=kGateStab}` (the drum-only "answer" tag — nothing else plays under it) |
| Bass/Chord1 | Same one-hit-then-silence as ending1's step-16 event — do NOT add anything at step 24+ (the answer is drums-only, which is what makes it a genuinely different idea from ending1, not ending1-plus-one-hit) |

### A.3 — kDanceFourOnFloor (disco, house)

DJ-friendly convention: the beat rides through the pickup, then DROPS OUT on
the downbeat of bar 2, leaving a big sustained chord ring — good for mixing
out. Device 1 (sus4) for the harmonic color, expressed through a thinning
kick pattern for the rhythmic "drop."

**ending1 — "ride into the hold," bar 2:**

| Role | New events |
|---|---|
| Drums | `{.step=16, .tone=kCrash, .vel=116, .gate=kGateHeld}` `{.step=16, .tone=kOpenHat, .vel=90, .gate=kGateHalfBar}` — the FOUR-ON-THE-FLOOR KICK STOPS here (no kick events in bar 2 at all — this is the "drop," the audible signal something just ended) |
| Bass | `{.step=16, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld}` |
| Chord1 | `{.step=14, .tone=5, .octave=0, .vel=100, .gate=kGate8th, .src=NoteSource::kInterval}` (sus4 pickup, Device 1) `{.step=16, .tone=kRoot/kThird/kFifth/kSeventh (all 4), .vel=108, .gate=kGateHeld}` |
| Pad | `{.step=16, .tone=kRoot/kThird/kFifth/kSeventh, .vel=104, .gate=kGateHeld}` (restate into bar 2) |

**ending2 — "the countdown," bar 2, genuinely distinct:** a decelerating
4-kick countdown (steps get FARTHER apart, the opposite of a drum fill's
acceleration — this reads as a runway INTO the stop, distinct from ending1's
straight ride-and-drop) before the same hold.

| Role | New events |
|---|---|
| Drums | `{.step=16, .tone=kKick, .vel=118, .gate=kGateHat}` `{.step=20, .tone=kKick, .vel=116, .gate=kGateHat}` `{.step=26, .tone=kKick, .vel=114, .gate=kGateHat}` (16→20→26: gaps of 4 then 6 steps — audibly decelerating) then `{.step=30, .tone=kCrash, .vel=120, .gate=kGateHeld}` (the arrival, held past the section end) |
| Bass | `{.step=16, .tone=kRoot, .octave=0, .vel=112, .gate=kGateBeat}` `{.step=30, .tone=kRoot, .octave=-1, .vel=116, .gate=kGateHeld}` (low final root under the crash) |
| Chord1/Pad | Silent through the countdown (steps 16-29), then the full held chord AT step 30 only, landing with the crash — the chord's ARRIVAL time itself is the "different idea" from ending1 (which resolves early at step 16 and just rings; ending2 makes you wait for it) |

**Per-style delta**: disco uses `kClap` doubling the `kSnare`-family accents
(disco's established kit voice, seen in its existing break at
`disco.hpp` step 14 `kClap`) on the countdown kicks' off-steps if desired;
house keeps a pure kick/open-hat/crash kit (no claps — house's existing
break-adjacent patterns in `house.hpp` favor `kOpenHat`/`kPercA`/`kPercD`
over claps).

### A.4 — kSwingShuffleJazz (swing, blues, shuffle)

Big-band/jazz tag convention: a walking bass approach into the final chord
(Device 3, b7 color — matches blues' own established `kInterval` vocabulary)
plus a swung ride cymbal. Triplet feel is already the style's own groove
default (`GrooveParams`) — do not add explicit triplet step positions here
(that is the groove engine's job, not the event table's); just keep the
rhythmic values consistent with the style's existing swing-family gate
choices (`kGateHat`/`kGate8th`, as already used throughout `blues.hpp`/
`swing.hpp`/`shuffle.hpp`).

**ending1 — "walk-up tag," bar 2:**

| Role | New events |
|---|---|
| Bass | `{.step=12, .tone=2, .octave=0, .vel=?, .gate=kGate8th, .src=NoteSource::kInterval}` (whole-step approach, a walking 2nd) `{.step=14, .tone=10, .octave=0, .vel=?, .gate=kGate8th, .src=NoteSource::kInterval}` (b7, Device 3) `{.step=16, .tone=kRoot, .octave=0, .vel=?, .gate=kGateHeld}` (arrival — this three-note walk-up is the classic jazz/blues turnaround bass line compressed into one bar, exactly the "ii-V-I compressed" feel the coordinator asked for, expressed root-relative per §0.3) |
| Chord1 | `{.step=16, .tone=kRoot/kThird/kSeventh, .vel=?, .gate=kGateHeld}` (dominant-7 voicing, no 5th — matches blues' own `kE1C`/`kEndChord` choice of dropping the 5th, already measured in the corpus) |
| Drums | `{.step=16, .tone=kCrash, .gate=kGateHalfBar}` `{.step=16, .tone=kKick, .gate=100}` (matches the existing bar-1 gate=100 kick idiom already used in blues/swing/shuffle endings) `{.step=24, .tone=kRide, .vel=?, .gate=kGateBeat}` (a ride tap sustaining the swing feel into the hold, rather than going dead silent — jazz endings ring with the cymbal, not just the chord) |

**ending2 — "shout chorus, double hit," bar 2, genuinely distinct:** the
classic big-band "doo-WAH, doo-WAAAH" two-hit tag — same chord both times,
but the SECOND hit is louder/longer, with a drum crash only on the second.

| Role | New events |
|---|---|
| Chord1 | `{.step=16, .tone=kRoot/kThird/kSeventh, .vel=100, .gate=kGateBeat}` (hit 1, short) `{.step=24, .tone=kRoot/kThird/kSeventh, .vel=118, .gate=kGateHeld}` (hit 2, louder, held) |
| Drums | `{.step=16, .tone=kKick, .vel=100, .gate=kGateHat}` (hit 1, no crash) `{.step=24, .tone=kKick, .vel=120, .gate=kGateHat}` `{.step=24, .tone=kCrash, .vel=120, .gate=kGateHeld}` (hit 2, WITH crash) |
| Bass | `{.step=16, .tone=kRoot, .vel=104, .gate=kGateBeat}` `{.step=24, .tone=kRoot, .octave=-1, .vel=116, .gate=kGateHeld}` (octave drop on the big hit, the classic "bottom drops out" jazz-ending gesture) |

**Per-style deltas**: blues keeps its existing harmonica `kLeadLick`-style
color (add one `NoteSource::kInterval` grace note at `tone=6` [b5, blue
note] at step 15 on Lead, gate `kGateStaccato`, right before the arrival —
consistent with `blues.hpp`'s own established blue-note vocabulary); swing
doubles the walk-up bass with `kArp` (its own declared `kArpVoice`) playing
the same three notes an octave up, a big-band horn-section unison walk-up;
shuffle keeps it plainest (bass+drums+chord1 only, no Arp/Lead addition —
shuffle's existing corpus entries are the leanest of the three siblings, per
the family's own measured role count).

### A.5 — kLatinClave (bossa, samba, reggae, latin)

The most genre-divergent family — do NOT reuse one shape across all four
(this is exactly the trap the diagnosis doc flagged for blues≈funk breaks).

**latin (mambo/salsa tag)** — Device 5, rhythmic, clave-locked:

| Role | ending1 bar 2 | ending2 bar 2 (distinct) |
|---|---|---|
| Drums/Perc | `{.step=16, .tone=kCrash, .gate=kGateHeld}` `{.step=16, .tone=kCowbell, .gate=kGateHat}` then Perc `{.step=18,.tone=kClaves,...}` `{.step=21,.tone=kClaves,...}` `{.step=24,.tone=kClaves,...}` (a 3-hit clave tag pattern, syncopated, matching the style's existing `kPercTamb`-family clave use) | A REPEATED 2-hit mambo stab: `{.step=16,...}` AND `{.step=24,...}` both full-band unison hits (timbale/cowbell/crash), i.e. TWO tag hits instead of one clave phrase — the mambo "corte" (cut) convention, genuinely a different shape from ending1's single flowing clave phrase |
| Bass/Chord1 | Unison stab on step 16 only, chord tones, `kGateStab` | Unison stabs on BOTH step 16 and step 24 |

**samba (breque — the stop-time break-into-tag)**:

| Role | ending1 bar 2 | ending2 bar 2 |
|---|---|---|
| Drums | Surdo-style accelerating kick pickup at steps 12/13/14/15 (already established as the surdo idiom if present in `samba.hpp`'s own kick voicings) leading to `{.step=16,.tone=kCrash,.gate=kGateHeld}` | Same pickup, but landing on a SPLIT hit: half the kit on 16, the rest (agogo/repinique-style perc, via `kHiAgogo`/`kLoAgogo`) on step 18 — a call-response inside the same bar |
| Bass/Chord1 | Root triad stab at 16, held | Root triad stab at 16 (short) THEN a repeated eighth-note tag on Chord1 at steps 24/26/28/30 (a rhythmic vamp-out, not a harmonic change) before cutting |

**bossa (understated — NOT a big hit)**: bossa endings in real practice are
soft, not a band stab — the nylon-guitar chord simply resolves and rings,
with a ride-cymbal swell, no crash/unison hit at all.

| Role | ending1 bar 2 | ending2 bar 2 |
|---|---|---|
| Drums | `{.step=16, .tone=kRide, .vel=70, .gate=kGateHeld}` (swell, no crash, no kick) | Same ride swell, PLUS a soft `kSideStick` tap at step 24 (the one addition — still no crash, staying true to the genre's dynamic ceiling) |
| Chord1/Pad | Device 1 (sus4→3), soft velocities (≤ 90), `.gesture=ChordGesture::kRollUp` on the resolved chord (the one bossa/nylon-guitar-appropriate use of the roll gesture alongside ballad) | The resolved chord repeats at step 24 with one added color tone (`tone=14`, a 9th, `.src=kInterval`) for a slightly richer "9th chord" second voicing — bossa's harmonic sophistication read as a color upgrade, not a volume one |

**reggae (drop to the skank, then cut — NOT a big hit)**: reggae's ending
should NOT converge on the same "hit and hold" shape as every other family;
its idiom is subtraction, not addition — bass and kick drop out, leaving
only the offbeat guitar/organ skank (Chord2), which then itself cuts.

| Role | ending1 bar 2 | ending2 bar 2 |
|---|---|---|
| Drums/Bass | SILENT through all of bar 2 (this is the point — the one-drop kick/snare that anchors reggae's whole groove is what disappears) | Same silence, but the snare returns ALONE for a single rimshot at step 28 (a "one last dropped beat" before the cut) |
| Chord2 (skank) | `{.step=18,...}` `{.step=22,...}` `{.step=26,...}` `{.step=30,...}` short offbeat stabs (`kGateStaccato`), chord tones, continuing the skank rhythm ALONE into silence — then nothing (no final chord hold at all; reggae's ending is silence, not a ring) | Same skank pattern, but each hit is progressively QUIETER (vel descending e.g. 90→75→60→45) — a fade read entirely through velocity, the reggae-idiomatic equivalent of "finality," since there is no ringing chord to hold |

This is a deliberate, flagged exception to §0.3's general "hold the final
chord" pattern — reggae is the one family where the genre-correct ending is
to have LESS at the end, not more; forcing a held triad here would be the
same "same skeleton everywhere" mistake this whole spec exists to fix.

### A.6 — kOther (basic)

`basic` is the neutral/tutorial style (per its own `StyleFamily::kOther`
placement and per `per-style-default-progressions.md`'s own I-IV-V-I choice
for it — "the textbook cadence"). Give it the plainest, most textbook version
of Device 2 (leading-tone approach) — the single most "this is what a
cadence sounds like" gesture, appropriate for a style whose entire purpose is
being the unmarked reference point.

| Role | ending1 bar 2 | ending2 bar 2 (distinct — Device 4, Amen) |
|---|---|---|
| Chord1 | `{.step=15, .tone=-1, .octave=0, .vel=90, .gate=kGateStaccato, .src=NoteSource::kInterval}` (leading tone) `{.step=16, .tone=kRoot/kThird/kFifth, .vel=100, .gate=kGateHeld}` | `{.step=16, .tone=9(6th)/5(4th), .vel=90, .gate=kGateBeat, .src=kInterval}` then `{.step=24, .tone=kRoot/kThird/kFifth/kSeventh, .vel=110, .gate=kGateHeld}` |
| Bass | `{.step=16, .tone=kRoot, .vel=100, .gate=kGateHeld}` | `{.step=24, .tone=kRoot, .octave=-1, .vel=108, .gate=kGateHeld}` |
| Drums | `{.step=16, .tone=kKick, .gate=120}` `{.step=16, .tone=kCrash, .gate=kGateHalfBar}` | `{.step=24, .tone=kKick, .gate=120}` `{.step=24, .tone=kCrash, .gate=kGateHeld}` |
| Pad | Restate the held chord into bar 2, unchanged voicing | Restate at step 24 only (silent under the step-16 color tag, same logic as A.1) |

No Lead role addition (per §0.5 — `basic` has no `kLeadVoice` constant).

---

## Part B — 1-bar breaks for the 10 styles that lack one

Every break below auto-returns to the active variation after exactly 1 bar
(the separately-implemented core fix). Design goal per style: a real
stop/drop/anticipation idiom appropriate to ITS genre — explicitly NOT a
copy of another style's break (rejecting the measured blues≈funk mistake)
and NOT a copy of that style's own variation bar with the labels changed.

### B.1 — kPopRockBallad: pop, ballad, country

All three use the same SKELETON as the existing rock/motown/disco/funk break
idiom (accented downbeat → silence → pickup into the next bar,
`rock.hpp:354-366`) since that IS the correct pop/rock-family convention —
but each gets its own instrumentation and dynamic ceiling so they read as
different, not copy-pasted:

**pop** (bright, radio-friendly — a "everybody drops out but the vocal-cue
hand-clap" stop):

| Role | Events |
|---|---|
| Drums | `{.step=0, .tone=kKick, .vel=110, .gate=kGateHat}` `{.step=0, .tone=kCrash, .vel=100, .gate=kGateStab}` (accented downbeat, moderate — pop doesn't hit as hard as rock) then SILENCE until `{.step=12, .tone=kClap, .vel=100, .gate=kGateHat}` `{.step=14, .tone=kClap, .vel=104, .gate=kGateHat}` (a hand-clap pickup instead of rock's snare/tom fill — the pop-idiomatic choice, using the style's own already-declared `kPercShake`/clap-family kit) |
| Bass | `{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateStab}` (one pop, then silent) |
| Chord1 | `{.step=0, .tone=kRoot/kThird/kFifth, .vel=96, .gate=kGateStab}` |

**ballad** (the softest possible break — a held fermata-like chord, NOT a
hit-and-silence; ballads "break" by suddenly going quiet under a sustained
pad, not by stopping):

| Role | Events |
|---|---|
| Pad | `{.step=0, .tone=kRoot/kThird/kFifth/kSeventh, .vel=60, .gate=kGateHeld, .gesture=ChordGesture::kRollUp}` (soft, spread, rings through the whole bar — this IS the break; nothing else plays) |
| Drums | `{.step=0, .tone=kCrash, .vel=50, .gate=kGateHeld}` (a single very soft cymbal swell, no kick at all) |
| Bass/Chord1 | SILENT the entire bar (the "break" for a ballad is the rhythm section dropping out under the pad, not a stab) |

**country** (a "hitch" — a single guitar-and-kick stop with a pickup lick on
the steel/Chord2 part, matching the genre's twang idiom):

| Role | Events |
|---|---|
| Drums | `{.step=0, .tone=kKick, .vel=112, .gate=kGateHat}` `{.step=0, .tone=kRimshot, .vel=100, .gate=kGateStab}` (rimshot, not crash — country's backbeat voice, matching `country.hpp`'s own established snare-substitute choice) then silence until `{.step=13, .tone=kRimshot, .vel=90, .gate=kGateHat}` `{.step=15, .tone=kRimshot, .vel=100, .gate=kGateHat}` (a two-hit rimshot pickup, not a tom fill) |
| Bass | `{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateStab}` |
| Chord2 (steel) | `{.step=8, .tone=10, .octave=0, .vel=80, .gate=kGate8th, .src=NoteSource::kInterval}` (a b7 pedal-steel bend into the pickup — the one genuinely country-specific color, using the style's own declared `kChord2Voice`) |

### B.2 — kDanceFourOnFloor: house

Disco's existing break (`disco.hpp`, measured: kick+crash stab, then
tomMid/tomLow/clap/openHat pickup at steps 12-15) is the sibling to build
against WITHOUT copying — house's break should be the "filter-drop" read
(even without CC automation per §0.6, this is achievable rhythmically): the
KICK STOPS (house's whole identity is the four-on-the-floor kick, so
removing it IS the break) while a single sustained chord/percussion loop
continues underneath, then the kick returns via the pickup.

| Role | Events |
|---|---|
| Drums | NO kick at all this bar (this is the entire point of a house break) — `{.step=0, .tone=kOpenHat, .vel=90, .gate=kGateHalfBar}` (the hat keeps the pulse alive without the kick) then a pickup: `{.step=12, .tone=kKick, .vel=100, .gate=kGateHat}` `{.step=14, .tone=kKick, .vel=110, .gate=kGateHat}` `{.step=15, .tone=kCrash, .vel=116, .gate=kGateStab}` (the kick's RETURN is the release of tension, not its absence — the inverse of every other family's break shape) |
| Chord1/Pad | Chord1 `{.step=0, .tone=kRoot/kThird/kFifth/kSeventh, .vel=80, .gate=kGateHeld}` held through the whole bar (the sustained chord loop under the missing kick) |
| Bass | SILENT until the pickup: `{.step=14, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHat}` (re-enters WITH the kick, reinforcing the "return" moment) |

### B.3 — kSwingShuffleJazz: swing, shuffle

Blues already owns the "stop-time" shape in this family (accented downbeat,
snare/tom pickup) — swing and shuffle should NOT copy it (the diagnosis
doc's explicit warning against blues≈funk). Both get a "drop to brushes/ride
only" break instead — a real big-band/swing convention (the rhythm section
lays out except a ride/brush pulse, then the band re-enters).

**swing**:

| Role | Events |
|---|---|
| Drums | `{.step=0, .tone=kRide, .vel=70, .gate=kGateHat}` `{.step=4, .tone=kRide, .vel=60, .gate=kGateHat}` `{.step=8, .tone=kRide, .vel=70, .gate=kGateHat}` `{.step=12, .tone=kRide, .vel=60, .gate=kGateHat}` (ride alone, swung quarters, no kick/snare AT ALL this bar) then `{.step=15, .tone=kCrash, .vel=110, .gate=kGateStab}` (a single pickup accent on the "and" leading the band back in) |
| Bass/Chord1 | SILENT the whole bar except one soft "walking" cue at step 14: Bass `{.step=14, .tone=kFifth, .octave=-1, .vel=70, .gate=kGate8th}` (a lone upright-bass pickup note, not a full chord stab — swing rhythm sections lay out on a break, they don't stab) |

**shuffle**:

| Role | Events |
|---|---|
| Drums | `{.step=0, .tone=kClosedHat, .vel=60, .gate=kGateHat}` `{.step=3, .tone=kClosedHat, .vel=50, .gate=kGateHat}` `{.step=6, .tone=kClosedHat, .vel=60, .gate=kGateHat}` (a shuffled hat pulse alone, no kick/snare — a lighter, more sparse version of swing's ride break, matching shuffle's own leaner corpus footprint per Part A's family note) then `{.step=14, .tone=kSnare, .vel=90, .gate=kGateHat}` `{.step=15, .tone=kSnare, .vel=100, .gate=kGateHat}` (two-hit snare pickup, simpler than swing's cymbal accent — the two siblings stay audibly distinct from each other, not just from blues) |
| Bass/Chord1 | SILENT the whole bar (leanest of the three swing-family breaks, matching shuffle's measured role-count minimum) |

### B.4 — kLatinClave: bossa, samba, reggae

Latin already owns the "timbale/cowbell/claves pickup" shape
(`latin.hpp`, measured) — bossa/samba/reggae must each be genuinely
different, continuing the divergence Part A §A.5 already established for
their endings.

**samba** (breque — the genre's own named stop-time convention, a real
1-bar interruption idiom): a syncopated unison hit landing OFF the downbeat
(not on step 0 — samba's breque characteristically lands on the "e" of beat
1 or on beat 2, not the top of the bar), full silence around it.

| Role | Events |
|---|---|
| Drums/Perc | SILENCE at step 0 (deliberately — the anticipation of silence IS the samba breque's signature, unlike every other family's downbeat-first shape) then `{.step=3, .tone=kSurdo-equivalent (use whichever low drum note the style's own kick-substitute constant is; if none, kKick), .vel=118, .gate=kGateStab}` `{.step=3, .tone=kCrash, .vel=110, .gate=kGateStab}` (the unison stop, off the downbeat) then silence again until a short tag `{.step=12, .tone=kHiAgogo, .vel=90, .gate=kGateHat}` `{.step=14, .tone=kLoAgogo, .vel=95, .gate=kGateHat}` (agogo bells lead back into the groove — samba's own idiomatic pickup voice, distinct from latin's timbale choice) |
| Bass/Chord1 | Unison stab at step 3 ONLY (matching the drums), chord tones, `kGateStab` — silent elsewhere |

**bossa** (soft, NOT a stop-time stab — bossa's break is a rhythmic
hesitation, not a hit; the nylon-guitar comp simply skips a beat then
resumes with a slightly altered voicing):

| Role | Events |
|---|---|
| Chord1 | `{.step=0, .tone=kRoot/kThird/kFifth, .vel=70, .gate=kGateHalfBar}` (a soft comp chord, same as any bar — NOT accented) then SILENCE at the point the groove would normally comp again (no event at step 8) — the missing hit, not an added one, IS the break — then `{.step=12, .tone=kRoot/kThird/kFifth/(tone=14, 9th color), .vel=75, .gate=kGate8th}` (resumes with a richer voicing, the "something changed" cue) |
| Drums | `{.step=0, .tone=kRide, .vel=55, .gate=kGateHat}` (kept very soft throughout, no crash, no accent — bossa never gets loud) |
| Bass | `{.step=0, .tone=kRoot, .vel=70, .gate=kGateHalfBar}`, silent after |

**reggae** (a dub-style drop-out — matching Part A's reggae ending logic:
subtraction, not addition; the one-drop kick/snare and bass vanish for the
whole bar, leaving only the offbeat skank, then everything crashes back in
on beat 1 of the NEXT bar via the normal variation content, not authored
here):

| Role | Events |
|---|---|
| Drums/Bass | SILENT the entire bar (the drop-out IS the break) |
| Chord2 (skank) | `{.step=2, .tone=kRoot/kThird, .vel=70, .gate=kGateStaccato}` `{.step=6, ...}` `{.step=10, ...}` `{.step=14, ...}` (the offbeat skank continues alone, unaccented, exactly as it would in a normal bar — reggae's break is defined by what's MISSING, the rhythm section, not by anything new added) |
| Perc (optional) | A single soft `kShortGuiro` or hand-percussion tick at step 8 if the style already declares one, purely as a "the record is still spinning" cue — omit if it reads as too busy; not load-bearing to the idiom |

### B.5 — kOther: basic

The plainest possible break — half-time drop for one bar, the textbook
"stop-and-anticipate" every arranger workstation teaches first. No
genre-specific color (appropriate for the neutral/tutorial style).

| Role | Events |
|---|---|
| Drums | `{.step=0, .tone=kKick, .vel=110, .gate=kGateHat}` `{.step=0, .tone=kCrash, .vel=100, .gate=kGateStab}` then SILENCE until `{.step=14, .tone=kSnare, .vel=100, .gate=kGateHat}` `{.step=15, .tone=kSnare, .vel=108, .gate=kGateHat}` (the single most generic two-hit pickup — this is deliberately the "textbook" version every other style's break is a genre-flavored departure from) |
| Bass | `{.step=0, .tone=kRoot, .vel=100, .gate=kGateStab}` |
| Chord1 | `{.step=0, .tone=kRoot/kThird/kFifth, .vel=96, .gate=kGateStab}` |

---

## 2. Verification checklist for whoever implements this

- `.bars` bumped 1→2 on BOTH `kEnding1` and `kEnding2` `StyleSection` entries,
  for all 16 styles (Part A) — bar-1 content is untouched.
- Every new event's `step` falls in `[16, 31]` for endings, `[0, 15]` for
  breaks (breaks stay 1 bar, `.bars=1`, unchanged from today's convention).
- Every `NoteSource::kInterval` event carries `.src=NoteSource::kInterval`
  explicitly (it is NOT the default; omitting it silently reverts to
  `kChordTone`, which would misinterpret the semitone offset as a chord-tone
  INDEX — e.g. `tone=10` would try to read the corpus's own chord-tone index
  10, undefined for a triad/7th chord, instead of the intended b7 color).
- `ChordGesture` fields (`kRollUp`/`kStrumDown`) are spelled exactly as in
  `style_model.hpp:70-76` and only applied to the styles named in §A (ballad,
  bossa get `kRollUp`; rock, country get `kStrumDown`) — not corpus-wide.
- No style's break/ending adds a `Lead` role for `basic` (§0.5) or any role
  whose GM voice constant isn't already declared in that style's own header.
- `clang-format` the touched headers before handing back (house style, per
  `.claude/rules/cli-corrections.md`).
- This is data-only: no file outside
  `components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` should
  need to change for either Part A or Part B.

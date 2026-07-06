# Proposal — Per-style feel values for the 16 builtin styles (roadmap 9100)

Status: DATA + feasibility proposal (not code). Author: Ottorino.
Scope: the concrete per-style FEEL TABLE the 9100 implementor bakes into the
builtin style tables (9110 default `GrooveParams`, 9120 default tempo), plus an
exact statement of what `GrooveParams.swing`/`swing_grid` can and cannot express
(input to 9130 triplet/shuffle grid).

Read first: `docs/reflections/style-differentiation-and-generation.md` (the feel-gap
diagnosis), `app/core/include/arrangrr/arranger/groove.hpp` (the model targeted),
`app/core/include/arrangrr/common/time.hpp` (`BpmX100`, clamps 2000..40000).

---

## 0 — How the swing model actually behaves (measured, not recalled)

Constants: `kTicksPerStep = kPpqn/4 = 240` ticks per 16th; one beat = 960 ticks.

`groove::apply` (groove.hpp:65) swings ONLY the off-beat of `swing_grid`:
- `swing_grid = 8` → period 2 → delays `step % 4 == 2`, i.e. steps **2, 6, 10, 14**
  (the "and" of each beat — the off-8th).
- `swing_grid = 16` → period 1 → delays every **odd** step (the off-16th).

Delay applied to that off-beat: `offset = kTicksPerStep*2*swing/300 = 1.6 * swing`
ticks. So the swung off-8th (nominal 480 ticks) lands at `480 + 1.6*swing`:

| swing % | off-8th tick | long:short ratio | feel |
|---:|---:|---:|---|
| 0   | 480 | 1.0 : 1 | straight |
| 50  | 560 | 1.4 : 1 | light swing |
| 62  | 579 | 1.52 : 1 | medium jazz swing (~3:2) |
| 72  | 595 | 1.65 : 1 | firm shuffle |
| 75  | 600 | 1.67 : 1 | (5:3) |
| 100 | 640 | 2.0 : 1 | **exact triplet 8th** (hard bebop/shuffle) |

**Key result: `swing=100, swing_grid=8` places the off-8th at exactly 2/3 of the
beat = a real triplet 8th.** The two-note (long-short) swing/shuffle feel is
therefore FULLY expressible by the existing model — no new grid required for it.
At `swing_grid=16` the same math swings the off-16th (subtle 16th push).

Two caveats the implementor must know:
1. Swing only moves the OFF-beat step. The current feel-genres fake their swing by
   authoring the swung note on **step 3/7/11/15** (e.g. blues/shuffle ride: steps
   `{0,3},{4,7},{8,11},{12,15}` — that is 720 ticks = a *dotted* 3:1, harder than a
   triplet). Those authored notes sit on steps swing does NOT touch. To let
   `GrooveParams.swing` drive the feel, the feel-genre patterns must be **re-authored
   onto the straight off-8ths (steps 2,6,10,14)**; this is deliberate golden churn on
   those 3 styles and is the whole point of 9100.
2. `accent` (groove.hpp:80) boosts beat 1 (+30%·a) and beat 3 (+15%·a) but **subtracts
   on beats 2 & 4** (−8%·a on steps 4,12). The snare backbeat that defines pop/rock/
   motown/country/funk sits exactly on steps 4 & 12, so a large `accent` *softens the
   backbeat* — musically wrong for those genres. Keep `accent` small and only where the
   pulse (not the backbeat) is the identity. Any `accent > 0` also churns velocities.

---

## 1 — The feel table (copy these into the style tables)

`swing_grid` is don't-care wherever `swing = 0` (no effect); shown as 8 = default.

| style   | tempo (BpmX100) | swing % | swing_grid | accent % | triplet grid (9130) needed? | rationale (genre norm + corpus evidence) |
|---------|---:|---:|---:|---:|---|---|
| basic   | 12000 | 0  | 8 | 0 | no  | Neutral demo. STRAIGHT — keep current default, byte-identical golden. |
| pop     | 12000 | 0  | 8 | 0 | no  | Pop ~100–130, 120 canonical. Straight backbeat (kick 1&3, snare 2&4). STRAIGHT/frozen. |
| rock    | 13000 | 0  | 8 | 0 | no  | Driving rock ~120–140. Straight 8ths, hard backbeat. STRAIGHT/frozen. |
| ballad  | 7200  | 0  | 8 | 0 | no  | Slow ballad 60–80. Straight, long gates. STRAIGHT/frozen. |
| funk    | 10800 | 0  | 8 | 0 | no  | Classic funk ~100–115. Identity is 16th SYNCOPATION, not swing; keep straight. (Optional light 16th-swing: swing≈15, grid=16 — churns goldens, not essential.) STRAIGHT/frozen. |
| disco   | 12200 | 0  | 8 | 0 | no  | Disco 110–130. Four-on-the-floor, straight 8th octave bass. STRAIGHT/frozen. |
| house   | 12800 | 0  | 8 | 0 | no  | House typical 128. Four-on-the-floor, straight. STRAIGHT/frozen. |
| swing   | 14000 | 62 | 8 | 12 | no (two-note swing covered) | **FEEL-GENRE.** Medium jazz swing 120–160; swing=62 ≈ 3:2 medium ride feel. Re-author ride/comp onto straight off-8ths so groove drives it. |
| bossa   | 13000 | 0  | 8 | 0 | no  | Bossa nova is **NOT swung** — feel is clave SYNCOPATION, already authored (kTwoBass steps 0,6,8,14). ~130 quarter (60–90 half-note dance pulse). Keep straight. STRAIGHT/frozen. |
| samba   | 10400 | 0  | 8 | 0 | no (subtle 16th microtiming ≠ triplet) | Samba counted in 2, ~96–104 quarter. Identity = surdo + tamborim 16ths, authored. Its real "samba swing" is a subtle 16th push, NOT a triplet; keep straight to freeze goldens. STRAIGHT/frozen. |
| reggae  | 7500  | 0  | 8 | 0 | no  | Roots reggae 60–90. One-drop + off-beat skank already authored; straight. STRAIGHT/frozen. |
| country | 12000 | 0  | 8 | 0 | no  | Boom-chick train beat ~120. Straight. STRAIGHT/frozen. |
| blues   | 6600  | 75 | 8 | 10 | **YES for authentic 12/8** (long-short covered by swing) | **FEEL-GENRE.** Slow blues 12/8, 60–80. swing=75 ≈ firm shuffle; re-author ride onto straight off-8ths. True three-note 12/8 ride needs 9130 (see §2). |
| shuffle | 13000 | 72 | 8 | 10 | no  | **FEEL-GENRE.** Texas shuffle ~120–140, firm triplet-8ths (2:1-ish); swing=72. Re-author shuffle hats/bass onto straight off-8ths. |
| latin   | 18000 | 0  | 8 | 0 | no  | Salsa/mambo in cut time — ~180 quarter (≈90 in 2/2). Feel = clave + tumbao, authored; straight. STRAIGHT/frozen. (High felt tempo is the cut-time norm — flag below.) |
| motown  | 12400 | 0  | 8 | 0 | no  | Soul backbeat ~120–130. Straight 8th soul bass, backbeat snare. STRAIGHT/frozen. |

### Which styles MOVE vs STAY
- **STRAIGHT / frozen goldens (13):** basic, pop, rock, ballad, funk, disco, house,
  bossa, samba, reggae, country, latin, motown. `swing=0, accent=0` ⇒ `groove::apply`
  returns a zero timing offset and unchanged velocity ⇒ **byte-identical output**.
  Only tempo (9120) may touch these — see the golden note below.
- **FEEL-GENRES that MOVE (3):** swing, shuffle, blues. Their identity IS the swing;
  they get real swing values and are expected to churn (they must also be re-authored
  onto straight off-8ths so the groove engine drives the feel).

Note the musicological correction: **bossa and samba are feel-genres but their feel is
clave syncopation, not triplet swing** — they stay straight. Only swing/shuffle/blues
are *swing* genres.

---

## 2 — 9130: what a true triplet grid must add (and what it need NOT)

**Already covered by `swing` + `swing_grid` — do NOT build a model addition for these:**
- Two-note long-short **8th swing** (swing/jazz ride, Texas shuffle, blues shuffle ride,
  boogie shuffle bass): `swing_grid=8`, swing 62–100. swing=100 = exact triplet 8th.
- Two-note **16th swing** (optional funk/latin micro-push): `swing_grid=16`.

**NOT expressible by any `swing` value — this is the ONLY thing 9130 must add:**
- **Three evenly-spaced triplet notes per beat.** Triplet-8th positions are 0, 320, 640
  ticks; the 16th grid offers 0, 240, 480, 720. Ticks **320 and 640 are not
  representable**, so a pattern that must *articulate all three* triplet notes (a true
  12/8 ride "spang-a-lang" playing the middle triplet, boogie-woogie 8th-note triplet
  bass/piano, gospel 12/8, triplet drum fills) cannot be authored today.

**Who needs it:** among the 16, **only `blues`** has a genre norm (12/8) that genuinely
wants three-note triplet subdivision. Its current authoring fakes it as a two-note
long-short (steps 0,3), which `swing=75, grid=8` reproduces faithfully as a shuffle — so
9130 is a **quality upgrade for blues (and any future gospel/boogie style), not a blocker
for any current style's basic feel.** shuffle and swing are two-note by nature and need
nothing beyond `swing`.

**What 9130 should provide (design hint, not code):** a per-style opt-in subdivision that
adds authorable step positions at `beat*k/3` — i.e. a 12-slot (triplet-8th) or 24-slot
(triplet-16th) bar grid — so an event can land on the middle triplet. This is additive
metadata + a resolve-time position map; it is no-heap and dual-target friendly (pure
integer tick math, same regime as `kTicksPerStep`). It does NOT require touching
`GrooveParams`.

---

## 3 — Ranges, feasibility, flags

- **All values are in-range.** Tempos 6600..18000 sit inside `kMinBpm=2000`..
  `kMaxBpm=40000`. swing/accent 0..75 inside 0..100. `swing_grid` ∈ {8,16}. Nothing
  clamps.
- **No dependency added.** Every value is data baked into existing `constexpr` tables;
  no new library, no heap, no host/arm divergence. SHIPPABLE within the core doctrine.
- **Golden churn is bounded and intentional:** velocity/timing churn is limited to the 3
  feel-genres (swing, shuffle, blues); the other 13 stay byte-identical at `swing=0,
  accent=0`.
- **FLAG — tempo vs goldens (9120):** per-style default tempo does NOT move note tick
  positions (scheduling is tick-based); it only changes the tempo meta / real-time
  playback. If the golden harness captures a tempo meta event, styles whose default ≠
  120.00 will churn *only that meta*. The implementor must confirm whether goldens pin
  tempo before landing 9120. This is orthogonal to the swing churn above.
- **FLAG — latin felt tempo:** 18000 (180.00) is the genre-correct cut-time salsa/mambo
  quarter tempo, but it is high relative to the other styles and makes the authored 16th
  montuno fast. This is authentic; noting it as a conscious choice, not an error. Owner
  may prefer a tamer default (e.g. 15000) if the builtins are meant to feel uniform.

---

## 4 — What needs an owner decision

1. **Re-authoring the 3 feel-genres onto straight off-8ths** so `GrooveParams.swing`
   drives the feel (vs keeping the baked dotted authoring). Recommended: re-author —
   otherwise the per-style swing knob only partially engages. This is the intended
   churn of 9100.
2. **Whether 9130 is built now for `blues`** (authentic three-note 12/8) or deferred —
   the two-note shuffle approximation via `swing` ships today without it.
3. **Optional accent/16th-swing on pulse/funk styles** — musically minor, and it churns
   otherwise-frozen goldens. Recommended: leave at 0 for now.
4. **latin default tempo** (180.00 authentic-fast vs a tamer uniform default).

# Proposal — Per-style feel values for the 16 builtin styles (roadmap 9100)

Status: DATA + feasibility proposal (not code). Author: Ottorino.
Scope: the concrete per-style FEEL TABLE the 9100 implementor bakes into the
builtin style tables (9110 default `GrooveParams`, 9120 default tempo), plus an
exact statement of what `GrooveParams.swing`/`swing_grid` can and cannot express
(input to 9130 triplet/shuffle grid).

Read first: `docs/reflections/style-differentiation-and-generation.md` (the feel-gap
diagnosis), `components/arrangrr/include/arrangrr/arranger/groove.hpp` (the model targeted),
`components/arrangrr/include/arrangrr/common/time.hpp` (`BpmX100`, clamps 2000..40000).

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

---

## 5 — Swing re-authoring spec (implementer-ready)

*(merged from `per-style-feel-swing-reauthoring.md`, 2026-07-11)*

The concrete, event-by-event re-authoring for the three FEEL-GENRES `swing`,
`shuffle`, `blues` so that the shipped `GrooveParams.swing` / `swing_grid`
produce the swing feel from STRAIGHT-grid notes, instead of the current
hand-authored dotted placement. Roadmap 9100 (feel) swing pass.

Read alongside: `components/arrangrr/include/arrangrr/arranger/groove.hpp` (the engine),
`components/arrangrr/include/arrangrr/arranger/style_model.hpp` (`StyleEvent.step`, `Style.groove`),
and the three tables `styles/{swing,shuffle,blues}.hpp`.

### 5.0 — Per-style offsets (building on the formula in §0 above)

`groove::apply` (groove.hpp:71-75), with `swing_grid == 8` (period 2), delays
ONLY the steps where `step % 4 == 2`, i.e. **steps 2, 6, 10, 14** (the off-8ths).
The delay is constant per style (independent of which off-8th) — same formula
as §0 above, `offset = (kTicksPerStep * 2 * swing) / 300 = (480 * swing) / 300`
(integer division), evaluated here for the three feel-genres' chosen swing values:

| style   | swing % | off-8th offset (ticks) | off-8th at step 2 lands | long:short |
|---------|--------:|-----------------------:|------------------------:|-----------:|
| swing   | 62      | 99                     | 480 + 99 = **579**      | 1.52 : 1   |
| shuffle | 72      | 115                    | 480 + 115 = **595**     | 1.63 : 1   |
| blues   | 75      | 120                    | 480 + 120 = **600**     | 1.67 : 1   |

Consequences the implementer must internalise:

1. Steps **3, 7, 11, 15** (the current dotted authoring) are NOT touched by the
   engine — a note left there ignores the swing knob entirely. That is why the
   swung material must MOVE onto 2/6/10/14.
2. Steps **0, 4, 8, 12** (down-/back-beat) get offset 0 — they stay put. Keep all
   downbeat kicks, backbeat snares and walking-bass quarters exactly where they are.
3. The engine swings EVERY note that sits on 2/6/10/14 globally. So some notes that
   do NOT move (comping stabs, shout-chorus snares, ghost notes already authored on
   even off-8ths) will now be swung too. That is correct for comping/stabs and a
   negligible artifact for the odd 16th-pickup tom in `blues kBrkD` (see §5.4).

### 5.1 — Exact `GrooveParams` per style

Set the `.groove` field on each `kStyle` aggregate. Field order in the struct is
`swing, humanize_timing, humanize_velocity, accent, swing_grid, quantize, seed`,
so the designated initializers below are in declaration order (C++20 requirement).
In the `Style` aggregate, `groove` comes BEFORE `tempo`, so add it between
`.sections` and the existing `.tempo`.

```cpp
// swing.hpp — .tempo=14000 already present; add .groove:
inline constexpr Style kStyle{.name="swing",   .sections=Span<const StyleSection>(kSections),
    .groove={.swing=62, .accent=12, .swing_grid=8}, .tempo=14000};

// shuffle.hpp — .tempo=13000 already present:
inline constexpr Style kStyle{.name="shuffle", .sections=Span<const StyleSection>(kSections),
    .groove={.swing=72, .accent=10, .swing_grid=8}, .tempo=13000};

// blues.hpp — .tempo=6600 already present:
inline constexpr Style kStyle{.name="blues",   .sections=Span<const StyleSection>(kSections),
    .groove={.swing=75, .accent=10, .swing_grid=8}, .tempo=6600};
```

`humanize_timing = humanize_velocity = quantize = 0` (defaults) — DELIBERATE:
this pass isolates swing so the regenerated golden is verifiable to the tick.
Humanize is an orthogonal later pass; adding it now would obscure whether swing
landed correctly. `seed` default (1), `swing_grid = 8` (off-8th), all in range.

Accent justification + measured effect (integer math): at accent 10–12,
`(30*a)/100 = +3` on beat 1 (step 0), `(15*a)/100 = +1` on beat 3 (step 8), and
`(8*a)/100 = 0` on beats 2 & 4 — so the backbeat is NOT softened (the groove.hpp
caveat is moot at these small values). Effect is a light pulse lift only; it
churns velocity on steps 0 and 8 by +3/+1. Keep it, or drop to 0 if the owner
wants zero velocity churn — musically marginal either way.

### 5.2 — Note-table re-authoring: the global remap rule

**Rule R (apply to all three styles):** in every event array, for every event
with `step ∈ {3, 7, 11, 15}`, remap the step:

```
3 → 2      7 → 6      11 → 10      15 → 14
```

Nothing else changes: `tone`, `octave`, `vel`, `gate`, `src`, `gesture` stay.
Notes already on 0/2/4/6/8/10/12/14 stay. Onbeat drums/bass stay.

**Exceptions — DO NOT remap these (they are 16th figures / ornaments, not swung
eighths):**

- `kFDD` in ALL three styles — the peak dense 16th-note descending tom fill. It
  already populates BOTH 2/6/10/14 and 3/7/11/15; it is a straight 16th blast, not
  a long-short figure. Leave it entirely as authored.
- `blues kBrkD` — the stop-time pickup roll on steps 12,13,14,15 is a 16th pickup,
  not a swung eighth. Leave step 15 (and 13) as authored.
- `kLeadLick` step 11 in ALL three styles — the chromatic/blue grace note is a
  half-step pickup INTO step 12; remapping to 10 would collide with the step-10
  line note and destroy the bebop approach. Leave step 11. (Its swung eighths at
  10 and 14 are already on even off-8ths and swing correctly.)

That is the whole spec. The per-array lists in §5.3 are the exhaustive expansion
of Rule R (verified by grep against the current tables) so the implementer can
tick them off; they contain no information beyond Rule R + the exceptions.

### 5.3 — Exhaustive per-array change list (expansion of Rule R)

Notation: `array: step→step, …  (role/what)`. Every listed step moves; unlisted
steps in the array stay. "(already even)" flags companion notes on 2/6/10/14 that
DON'T move but WILL be swung by the engine.

#### swing.hpp
- `kArp8`:     3→2, 7→6, 11→10, 15→14   (vibraphone arp eighths)
- `kIn1D`:     11→10, 15→14             (ride)
- `kIn2D`:     7→6, 15→14               (ride; closedHat 4,12 stay)
- `kAD`:       7→6, 15→14               (ride; hats/kick stay)
- `kBD`:       3→2, 7→6, 11→10, 15→14   (ride; snare 6,14 already even → swings)
- `kBC`:       11→10, 11→10             (chord stab pair; 2,6,14 already even)
- `kDD`:       3→2, 7→6, 11→10, 15→14   (ride; snare 2,6,10,14 already even → swing)
- `kFAD`:      11→10, 15→14             (snare fill)
- `kFBD`:      3→2, 7→6, 11→10, 15→14   (tom/snare fill)
- `kFCD`:      3→2, 7→6, 11→10, 15→14   (tom/snare fill)
- EXCEPT `kFDD` (dense 16th fill — leave), `kLeadLick` step 11 (grace — leave).
- Untouched by design: `kWalkBass` (0,4,8,12 straight quarters), `kIn2C`/`kAC`/
  `kCC`/`kDC` and `kCh2A`/`kCh2B` (already authored on even off-8ths / onbeats),
  `kCD` (VarC quarter-note ride, no offbeats).

#### shuffle.hpp
- `kArpShuf`:  3→2, 7→6, 11→10, 15→14   (clean-guitar shuffle eighths)
- `kShufBass`: 3→2, 7→6, 11→10, 15→14   (root-fifth SHUFFLE BASS — core identity)
- `kIn1D`:     11→10, 15→14             (hat, snare pickup)
- `kIn2D`:     3→2, 7→6, 11→10, 15→14   (closed-hat shuffle)
- `kAD`:       3→2, 7→6, 11→10, 15→14   (closed-hat shuffle)
- `kBD`:       kick 7→6, snare 15→14, closedHat 3→2,7→6,11→10,15→14
- `kCD`:       ghost snare 3→2, 11→10 (6,14 already even); closedHat 3→2,7→6,11→10,15→14
- `kDD`:       kick 7→6, snare 15→14, closedHat 3→2,7→6,11→10,15→14 (openHat 0,4,8,12 stay)
- `kAC`:       3→2 (×3), 11→10 (×3)     (triad stabs)
- `kBC`:       3→2 (×3), 11→10 (×3)     (0,8 stay)
- `kCC`:       3→2 (×3), 11→10 (×3)
- `kDC`:       3→2 (×3), 11→10 (×3)     (0,8 stay)
- `kFAD`:      11→10, 15→14
- `kFBD`:      7→6, 11→10, 15→14
- `kFCD`:      3→2, 7→6, 11→10, 15→14
- EXCEPT `kFDD` (dense 16th fill — leave), `kLeadLick` step 11 (grace — leave).

#### blues.hpp
- `kArpTrip`:  3→2, 7→6, 11→10, 15→14   (ship-now 2-note; see §5.5 for 9130)
- `kBoogieBass`: 7→6, 15→14             (root-fifth-seventh boogie; 0,4,8,12 stay)
- `kIn1D`:     11→10, 15→14             (ride)
- `kIn2D`:     3→2, 7→6, 11→10, 15→14   (ride; kick/snare stay)
- `kAD`:       3→2, 7→6, 11→10, 15→14   (ride)
- `kBD`:       ride 3→2,7→6,11→10,15→14; kick 7→6; snare 15→14
- `kDD`:       ride 3→2,7→6,11→10,15→14; kick 7→6; snare 15→14
- `kAC`:       3→2 (×3), 11→10 (×3)
- `kBC`:       3→2 (×2), 11→10 (×2)
- `kDC`:       3→2 (×2), 11→10 (×2)
- `kFAD`:      11→10, 15→14
- `kFBD`:      3→2, 7→6, 11→10, 15→14
- `kFCD`:      3→2, 7→6, 11→10, 15→14
- EXCEPT `kFDD` (dense 16th fill — leave), `kBrkD` (stop-time 16th pickup — leave),
  `kLeadLick` step 11 (grace — leave).
- Untouched by design: `kCD`/`kCC` (VarC slow-drag stop-time, only steps 0/8).

### 5.4 — Verification intent (what the regenerated golden SHOULD show)

After Rule R + `.groove`, the emitted note-on tick of any re-authored off-8th is
`step*240 + offset`, with the constant offset from §5.0. Concrete checks:

- **swing** (offset 99): a ride note remapped to step 2 emits at **579**; step 6 →
  1440+99 = **1539**; step 10 → 2400+99 = **2499**; step 14 → 3360+99 = **3459**.
- **shuffle** (offset 115): step 2 → **595**; step 6 → **1555**; step 10 → **2515**;
  step 14 → **3475**. The `kShufBass` fifth that was at step 3 (720) now emits at
  595 — earlier and swung, i.e. the bounce is now engine-driven.
- **blues** (offset 120): step 2 → **600**; step 6 → **1560**; step 10 → **2520**;
  step 14 → **3480**.

Invariants to assert:
- Onbeat events (steps 0/4/8/12) emit at exactly `step*240` (offset 0) — unchanged.
- Gate is preserved: the same offset rides note-on AND note-off (groove.hpp:100,108),
  so every re-authored note keeps its `gate`; only its absolute position shifts.
- Notes still on odd steps (the §5.2 exceptions) emit at `step*240` (no swing), EXCEPT
  any exception-array note that happens to sit on an EVEN off-8th: e.g. `blues kBrkD`
  tomMid at step 14 emits at 3480 (swung), while its neighbours at 13 (3120) and 15
  (3600) stay — an accepted ~120-tick lilt in a 1-bar stop-time pickup.
- The 13 straight styles are NOT touched → their goldens stay byte-identical.

If the regenerated golden shows the swung notes at exactly these ticks (not merely
"some churn"), the feel landed as intended.

### 5.5 — What CANNOT be reached this way (needs 9130 — do NOT block on it)

The model swings a note into TWO-note long-short per beat. That is exactly right
for `swing` and `shuffle` (jazz ride and Texas shuffle are two-note by nature) —
they need nothing beyond §5.1–§5.2, and `swing=100` would give an exact triplet-8th
if a harder feel is ever wanted.

`blues` is genuinely 12/8. The ship-now approximation renders it as a TWO-note
shuffle (downbeat + one swung off-8th), which swing=75 reproduces faithfully:

- **Ship-now (2-note shuffle) blues events:** the ride in `kIn1D/kIn2D/kAD/kBD/kDD`
  (down-beat + swung "and"), `kBoogieBass` (root-fifth-seventh boogie as swung
  eighths), `kArpTrip` (climbs on swung eighths, NOT true triplets despite the
  name), and the dominant-7 comp stabs. All authentic-enough for a shuffle-blues.
- **What 9130 would later refine:** a true three-note 12/8 "spang-a-lang" ride that
  articulates the MIDDLE triplet partial; a genuine triplet arp (`kArpTrip`); and an
  optional 8th-note-triplet boogie bass. These need authorable slots at tick 320 and
  640 within each beat (beat*1/3, beat*2/3), which the 16th grid (0/240/480/720)
  cannot name. 9130 = additive per-style triplet subdivision (12- or 24-slot bar
  grid), pure integer tick math, no `GrooveParams` change, no-heap, dual-target —
  as described in §2 above.

The swing pass ships complete WITHOUT 9130; 9130 is a later quality upgrade for
blues (and future gospel/boogie styles) only.

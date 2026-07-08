# Proposal — Swing re-authoring spec (implementer-ready)

Status: IMPLEMENTER SPEC (not code). Author: Ottorino.
Companion to `docs/proposals/per-style-feel-values.md` (§0–§4, the feel table).
This is the concrete, event-by-event re-authoring for the three FEEL-GENRES
`swing`, `shuffle`, `blues` so that the shipped `GrooveParams.swing` /
`swing_grid` produce the swing feel from STRAIGHT-grid notes, instead of the
current hand-authored dotted placement. Roadmap 9100 (feel) swing pass.

> Note on where this lives: per the author's write boundary this spec is a NEW
> doc rather than an in-place append to `per-style-feel-values.md`; treat it as
> the "Swing re-authoring spec" section of that proposal.

Read alongside: `components/arrangrr/include/arrangrr/arranger/groove.hpp` (the engine),
`components/arrangrr/include/arrangrr/arranger/style_model.hpp` (`StyleEvent.step`, `Style.groove`),
and the three tables `styles/{swing,shuffle,blues}.hpp`.

---

## 0 — The one fact that drives everything (measured from groove.hpp)

`groove::apply` (groove.hpp:71-75), with `swing_grid == 8` (period 2), delays
ONLY the steps where `step % 4 == 2`, i.e. **steps 2, 6, 10, 14** (the off-8ths).
The delay is constant per style (independent of which off-8th):

```
offset = (kTicksPerStep * 2 * swing) / 300 = (480 * swing) / 300   // integer div
```

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
   negligible artifact for the odd 16th-pickup tom in `blues kBrkD` (see §4).

---

## 1 — Exact `GrooveParams` per style

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

---

## 2 — Note-table re-authoring: the global remap rule

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

That is the whole spec. The per-array lists in §3 are the exhaustive expansion of
Rule R (verified by grep against the current tables) so the implementer can tick
them off; they contain no information beyond Rule R + the exceptions.

---

## 3 — Exhaustive per-array change list (expansion of Rule R)

Notation: `array: step→step, …  (role/what)`. Every listed step moves; unlisted
steps in the array stay. "(already even)" flags companion notes on 2/6/10/14 that
DON'T move but WILL be swung by the engine.

### swing.hpp
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

### shuffle.hpp
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

### blues.hpp
- `kArpTrip`:  3→2, 7→6, 11→10, 15→14   (ship-now 2-note; see §4 for 9130)
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

---

## 4 — Verification intent (what the regenerated golden SHOULD show)

After Rule R + `.groove`, the emitted note-on tick of any re-authored off-8th is
`step*240 + offset`, with the constant offset from §0. Concrete checks:

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
- Notes still on odd steps (the §2 exceptions) emit at `step*240` (no swing), EXCEPT
  any exception-array note that happens to sit on an EVEN off-8th: e.g. `blues kBrkD`
  tomMid at step 14 emits at 3480 (swung), while its neighbours at 13 (3120) and 15
  (3600) stay — an accepted ~120-tick lilt in a 1-bar stop-time pickup.
- The 13 straight styles are NOT touched → their goldens stay byte-identical.

If the regenerated golden shows the swung notes at exactly these ticks (not merely
"some churn"), the feel landed as intended.

---

## 5 — What CANNOT be reached this way (needs 9130 — do NOT block on it)

The model swings a note into TWO-note long-short per beat. That is exactly right
for `swing` and `shuffle` (jazz ride and Texas shuffle are two-note by nature) —
they need nothing beyond §1–§2, and `swing=100` would give an exact triplet-8th if
a harder feel is ever wanted.

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
  as described in `per-style-feel-values.md` §2.

The swing pass ships complete WITHOUT 9130; 9130 is a later quality upgrade for
blues (and future gospel/boogie styles) only.

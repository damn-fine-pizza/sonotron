# Director vocabulary — the intention axes

Status: **axis set proposed 2026-07-07** — `energy · tension · valence` (motion deferred).
Design altitude: the Director's *perceptual* vocabulary, not ABI/param detail. Companions:
`workstation-vision.md` (the three tiers), `flows.md` (#3 conduct intention), `ux-workstation.md`
(the demoted read-only intention rail, §4.7). Supersedes the loose "working set: energy / tension /
mood" mentioned there.

## The law this obeys

The Director speaks **perceptual intention**, never **realization levers**. An axis is legitimate
only if it names *what the music feels like it is doing*, not *how a part is voiced*. Density,
register, instrumentation, groove intensity, space/reverb, timbre/color — those are the
**Arranger's** business: they are how intention is *realized*, downstream. A proposed axis that is
really a realization knob wearing a mood costume does not belong here. (This is the
Director → Arranger seam from `workstation-vision.md`.)

Two consequences:
- The axes are **few and orthogonal** — each a genuinely independent thing the conductor wants to
  steer, legible at a glance on the podium (flow #3). More axes ≠ more expressive; redundant axes
  dilute the WOW and clutter the surface.
- The same axis value can be realized many ways — the Arranger owns that mapping. This vocabulary
  fixes *meaning*, not *mechanism*.

## Grounding — why three, not five

The dimensional-affect field (Russell's circumplex) treats **valence × arousal** as the 2-axis
standard that "covers the vast majority of emotions"; the common 3-axis extension (VAD) adds
*dominance*, but dominance is empirically unstable and often dropped, and secondary axes (tension,
potency, energy) carry thinner evidence. Adaptive-game middleware (Wwise/FMOD) uses no fixed
emotional axes at all — an ad-hoc parameter set that in practice orbits a single dominant
"intensity" knob driving vertical layering. Net: **nobody ships five orthogonal emotional axes.**
Two is the field norm, three the generous extension. So our set is three, deliberately.

We keep the two universal axes (arousal → **energy**, **valence**) and, instead of the shaky
*dominance*, take **tension** as the third — because tension is *musically operational* (harmonic
instability, expectation, the build→resolve of flow #3) where dominance is a psychological
control construct with no clean mapping onto a band. A conscious deviation, not an oversight.

## The three axes

### 1. energy — how hard the music pushes
- **Poles:** at rest / sparse / laid-back  ↔  driving / full / relentless.
- **Is:** perceived arousal — the sense of activity and drive, "how much is going on and how hard
  it hits."
- **Is NOT:** literal note density or track count (that is how the Arranger *realizes* energy), and
  NOT loudness alone.
- **Realized by (Arranger, examples):** parts entering/leaving, pattern busyness, groove drive,
  dynamics, register lift. This is the field's "intensity" knob → vertical layering.

### 2. tension — how unresolved the music feels
- **Poles:** settled / consonant / at-home  ↔  strained / suspended / must-resolve.
- **Is:** harmonic and rhythmic instability, expectation, the felt need for release.
- **Is NOT:** energy — you can be tense and quiet (a held dissonance, pianissimo) or loud and
  resolved (a triumphant tutti on the tonic). Orthogonal to energy by construction.
- **Special rule — resolution lands in time.** When tension resolves, the release lands *in time*
  (on a downbeat/boundary), never at random — a universal commit-on-boundary rigor. The stronger
  claim "a build *obliges* a drop" (flow #3, step 4) is NOT an axis law but a **style-conditioned
  default**: EDM/techno/house profiles apply the build→resolve arc; drone/ambient/suspense suspend
  it — the same "genre-specific behaviour lives downstream" pattern as engine-driven swing (node
  9100). A tension held and never resolved is a legitimate conductor's choice (user-wins), not an
  architectural impossibility.
- **Realized by (Arranger, examples):** dissonance/extensions, pedal points, suspensions, rolls or
  fills approaching a peak, harmonic rhythm, withholding the root.

### 3. valence — the emotional color, dark ↔ bright
- **Poles:** dark / heavy / melancholic  ↔  bright / open / uplifting.
- **Is:** the pleasant↔unpleasant, negative↔positive axis of the circumplex — the *character* of
  the moment, independent of how hard it pushes.
- **Is NOT:** timbre/tone-color (a realization/Arranger concern — the "colore timbrico" split out
  of the old `mood`), and NOT mode alone (major≠happy is a cliché the Arranger may honor or subvert).
- **Replaces the old `mood`.** `mood` bundled valence + timbral color + more; we keep the clean
  perceptual half (valence) and hand the rest to the Arranger. The podium mockup already draws this
  axis as "neutral → bright" — i.e. valence.
- **Realized by (Arranger, examples):** mode/scale color, chord-quality choices, voicing openness,
  melodic contour, instrumentation warmth.

## The deferred fourth — motion (candidate, not adopted)

The only additional axis that survives the orthogonality test is **motion / agitation** — perceived
*pace*, independent of energy: high energy + low motion = a powerful static wall; low energy + high
motion = a nervous, fidgety pianissimo. It is genuinely perceptual and not a realization knob.
**Not adopted now** — we start from the minimal orthogonal core and add on evidence, per "kept
loose / expandable." Recorded here so a future add is principled, not ad-hoc.

**Explicitly NOT axes** (they are realization, never perceptual intention, however tempting):
density, register, instrumentation, groove/swing amount belong to the **Arranger**; space/reverb
and timbre/color live further downstream in the **Engines / melodd** (arrangrr decides symbolically
and never processes audio); `tempo` is an initial condition set at flow #1, not something realized
continuously. Steering any of them is a downstream job or a direct manual override, never a
Director axis.

## How an axis behaves — trajectory, not setpoint

You do not set "the value now"; you set **where it should arrive and how fast** (flow #3, step 2).
So each axis is a triple:

```
axis = { current, target, rate }   →   a trajectory the music walks
```

- **current** — the truthful now (observable, mirrored on the podium).
- **target** — where the conductor pointed it.
- **rate** — how fast to get there (e.g. "rising over 8 bars").

Two realization regimes as the trajectory advances (the core distinction of flow #3):
- **structure = stepped** — discrete changes fire at **thresholds** with **hysteresis** (the arp
  switches on halfway up; the pad re-enters; a fill fires before the peak) — hysteresis so it does
  not flap around the switch point.
- **color = continuous** — smooth morph (groove drive, register, dynamics open gradually).

Changes commit on **musical boundaries** (bar/phrase), never mid-tick; queued ones show a countdown
on the podium ("→4").

## Composition — the axes together

The axes are orthogonal, but their *combination* is where character lives (the circumplex
quadrants):
- high energy + low valence + high tension → aggressive, driving, dark (a peak before a drop);
- low energy + high valence + low tension → calm, warm, resolved (a settled outro);
- low energy + high tension → suspense (quiet dread);
- high energy + high valence + resolving tension → euphoric release (the drop landing on the
  downbeat).

The Director holds a **target point** in this space + a rate, and walks the music toward it —
committing structure on boundaries, morphing color continuously.

## User-wins & determinism

- **The human always wins.** At any moment a manual override (mute a part, force a section, play a
  chord) beats the Director. In trajectory terms: the override updates `current` immediately (it is
  the truthful now), and the Director recomputes a fresh trajectory from there — it never fights the
  hand, it re-proposes from where you now are. The axes describe the Director's *intent*, never a
  cage. (`hook-interface.md` §4, user-wins.)
- **Deterministic.** Same axes + same trajectory + same seed → the same build, byte-exact next week.
  The intention is a reproducible musical object.

## Open / deferred
- The exact **widget** to express current→target+rate on the podium (kept loose).
- Whether **motion** graduates to a fourth axis (needs musical evidence).
- Numeric ranges/quantization per axis (an ABI/param concern, not this altitude).
- Whether valence carries a mode-bias hint to the Arranger, or stays purely perceptual.

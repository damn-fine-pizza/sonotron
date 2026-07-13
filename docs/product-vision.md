# Product Vision — sonotron, the intention-driven workstation

Status: **product definition, owner-validated 2026-07-07.** Product/design altitude only — no
ABI/command detail here. The names are fixed:

- **sonotron** — the outer, host-only workstation product (the "intention-driven workstation").
- **arrangrr** — the core symbolic MIDI-brain component: STM32-capable, realization-free.
- **melodd** — the optional host-only audio peer engine that realizes sound.

Category term to use internally instead of "DAW": **generative arranger / live-instrument**.

## What it is

An **intention-driven composition/performance workstation** — NOT a linear DAW (Reaper/Ardour),
NOT a clip-groovebox clone (Ableton Session), NOT an "AI makes you a track" generator (Suno). You
**conduct musical INTENTION** — energy, tension, valence, and a trajectory over time — and the machine
**composes and performs a full band toward that intention**, live, and **reproducibly** (same
intention → same result, byte-exact, next week).

sonotron is **NOT "MIDI-only"** — it has audio. But audio is subordinate *color*, never a
mixing/editing surface. It stays non-linear, non-DAW. Measured against Ableton it loses; measured
against Genos + Band-in-a-Box + a modular generative rig it wins on ground nobody holds.

**Not a recording editor:** it records the *music* — MIDI, patterns, gestures, chords — and can
re-harmonize it, which is more than an audio track, not less. Recording and clip-prep are in scope
and central, in **MIDI / session** form. "Not a DAW" is only about the audio-timeline surface:

- **NOT**: a linear audio timeline, audio tracks, audio editing/warp/comp, a mixer, or a plugin
  host. That is the DAW gravity, refused.
- **IS** (and records / loops / prepares clips): an arranger with a **looper and scenes** — MIDI
  capture, re-harmonizable clips, launchable scenes, all reproducible. The clip/session paradigm is
  *home* (Session view, yes); the linear audio arrangement timeline is *not* (Arrangement view, no).

## The three pillars (what no competitor offers together)

1. **Reproducible generativity** — every variation is a seeded, recallable fact. The "seed is a
   musical object": audition N variations, lock one, recall it byte-exact next week.
2. **Harmonic intelligence** — NTT: every arranger note is a chord tone by construction, so
   re-harmonizing a loop on the fly is lossless, and wrong notes are impossible.
3. **"No wrong notes" for the human hand, not only the machine** — the NTT resolver pointed at live
   input, so anyone can solo over the running band.

## Capabilities — what it does

1. **Conduct intention** — set or gesture a musical target (energy / tension / valence + a rate of
   change); the band re-arranges *toward* it gradually, committing changes on musical boundaries.
2. **Steer harmony live** — play chords (even one finger); the band follows and HOLDS them; the
   human hand beats the machine (`kLivePriority`).
3. **Build structure** — trigger sections / variations / fills / transitions; capture a trajectory
   as a **Song** (song-form without a timeline you paint on).
4. **Shape the feel** — style & section, groove / swing / humanize, mute & solo parts, arp.
5. **Deploy audio color** — the Arranger deploys clips / loops / samples / risers / vocal-chops as
   opaque decisions; simple recording of your gear/voice becomes arrangeable material.
6. **Make sound via hosted sources** — VST/CLAP/LV2 instruments + melodd + sampler realize the
   MIDI; VST MIDI-FX in the chain.
7. **Observe & steer everything** — every component is observable *and* interactive via uniform
   hooks; the GUI mirrors truthfully and you can override any decision.
8. **Reproduce byte-exact** — deterministic: same intention / seed → same result next week (the
   seed is a musical object).
9. **Drive real gear or run standalone** — MIDI out to hardware synths, or self-contained with
   hosted plugins/melodd.
10. **Hand-author, assisted on demand** — put notes into the sequencer yourself, as always; the
    sequencer acts as a **pull-based copilot** that PROPOSES / ADJUSTS / ADDS *only when you ask*,
    never on its own — and nothing lands until you accept. The note-level form of "the machine
    proposes, you command": **you always hold the pen.** (Pairs with #2; bottom-up counterpart to
    the top-down #1.)

(Deferred, not v1: reduce a cut-down arrangrr core to STM32; the Director's ML second-brain.)

## Two products, one boundary

- **arrangrr** — the **symbolic arrangement brain**, a reusable component. **STM32-capable.** It
  owns the arranger + sequencers + MIDI-FX + live harmony, and it DECIDES about audio too — but
  only **symbolically, by reference** ("deploy clip N at bar X, loop to clock"), never by
  processing it. It is **realization-ignorant**: it makes NO sound (MIDI → external synths/VSTs;
  audio clips → melodd/sampler realize them). Cheap opaque decisions, no heap/DSP → runs on a chip.
  It exposes **hooks** on EVERY component (Director, Arranger, sequencers, harmony, groove, arp,
  looper, MIDI-FX) with two faces — **observability** (see its state/decisions) and **interaction**
  (influence/redirect/override) — so the outer product can watch and steer any of it.
- **The workstation (sonotron)** — the host-only outer product. It **knows and orchestrates**
  arrangrr + VST instruments + audio + the Director + the GUI, plugging into arrangrr's hooks. This
  is where the "product" identity lives, and where MIDI-only is no longer the pitch.

## Three tiers

```
[ Director / Intention Engine ]   reasons in energy / tension / valence / trajectory; holds a TARGET
            │  (intention, via hooks)     the music walks toward, gradually, committing on musical boundaries
            ▼
[ Arranger ]                      TRANSLATES intention → section, variation-per-track, patterns
            │                              on/off, fills, mutes, which clips, transitions
            ▼
[ Engines ]                       MIDI sequencers · audio clip player · sampler · looper · mixer · MIDI-FX
```

- The **Director speaks perceptual intention**, never realization levers (density/register/etc.
  belong to the Arranger). "Gradual" = threshold+hysteresis for STRUCTURE (discrete) and continuous
  morph only for COLOR; tension must resolve (a release landed on a downbeat).
- **arrangrr already IS the bottom two tiers** (arranger + sequencers + sections + harmony). The new
  work is the Director on top + audio-color engines beside + the GUI shell. A layer, not a rewrite.

## Sound & audio

- **arrangrr makes no sound at all and processes no audio, forever** — MIDI or audio. It never
  records, edits, or plays back waveforms; that is *why* STM32, determinism, and reproducibility
  work at all. It emits **symbols**: MIDI events (realized by external synths/VSTs) and **opaque
  audio-clip decisions** ("deploy clip N at bar X, loop to clock" — realized by melodd/the sampler).
  Symmetric doctrine: *a Program is a MIDI reference to an external sound; an audio clip is a
  reference to an external asset, not internal DSP.* It DECIDES audio, never PROCESSES it — which is
  exactly what keeps it STM32-capable. VST hosting, VST MIDI-FX, audio processing/mixing/recording,
  and "arrangrr-as-a-plugin" ALL live in sonotron. So **"MIDI-only" is retired as the PRODUCT
  slogan** while the core itself stays realization-free.
- **Audio = arrangeable color:** clips, loops, textures, vocal chops, risers, samples, simple
  recording. Opaque, content-addressed, deployed by the Arranger as a decision. No linear timeline,
  no mixing-desk editing.
- Sound, when present, is realized by **melodd** (and a sampler/clip engine) — a **separate,
  OPTIONAL peer** at the Engines tier, a sibling of the MIDI sequencers/sampler behind the same
  uniform engine interface. arrangrr **uses it, never embeds it**; absent (STM32, or a host without
  audio) → graceful no-op, no hard dependency. This is what keeps the STM32 build honest. Rules:
  - melodd is authoritative over **sound**, never over **time**; the core never learns that melodd
    (or a microphone) exists — it emits symbolic decisions a peer realizes.
  - External audio **input** (voice/instrument) enters only inside melodd as an **opaque,
    content-addressed asset** the command/seed log *points to but never regenerates* — the seed owns
    every note and not one sample. Loop-to-clock yes; warp/edit on a linear timeline no.

## Architecture — peer modules, one orchestrator

```
          ORCHESTRATOR / host backend (per-deployment binary)
          ┌──────────┴───────────┐
      arrangrr                 melodd
     (core brain)             (audio, host)
   ── mutually name-blind; only a small POD interface between them ──
                    │  UDS-JSONL socket
                    ▼
      GUI — SEPARATE PROCESS · pure client · links nothing
```

- **arrangrr** exposes a port (`Command` in / `OutEvent`+clock out) and does **not** know a
  consumer exists.
- **melodd** exposes its own port and does **not** know arrangrr exists.
- The **orchestrator** (the target binary's `main`) is the only thing that knows both and wires
  their ports. Ports-and-adapters; the interface is POD and transport-agnostic (in-proc queue /
  socket / a wire between two chips), so it can descend to an MCU. Keep the port contract heap-free
  even host-side.

### Deployment matrix

| Target | Binary | Uses | Preset today |
|---|---|---|---|
| **Linux** | dev simulation | arrangrr (+ optional melodd-sim) | `host` |
| **STM32** | firmware | **arrangrr** alone (MIDI brain) | `arm` |
| **SBC** (Cortex-A / DSP) | full instrument | **arrangrr + melodd** | *(future)* |

arrangrr is the **invariant kernel** on every target (freestanding, portable). melodd appears only
where the HW has audio. "One binary uses both" on the SBC still means two name-blind libraries + an
orchestrator layer — never mutual `#include`, never an `#ifdef melodd` inside the core.

## Why it's not a clone

The abstract "energy/tension over time drives music" exists (game adaptive-music middleware; Suno).
What's unclaimed is the **substrate**: symbolic, deterministic, **byte-reproducible** arrangement
decisions, on a **live** MIDI brain that drives real gear or hosted plugins, **co-steered by the
human hand** on harmony (the hand beats the machine, like `kLivePriority`), and eventually
**reducible to a chip**. Novelty = the substrate + living-instrument + reproducibility, not the
"intention over time" idea itself.

## Targets & sequencing

- **Today: Linux x86 host + GUI.** Build it until it works *musically*. macOS/Windows later.
- **STM32 stays a real but DEFERRED target for arrangrr.** Once the GUI is finished and musically
  working, decide what to backport into the core (e.g. a reduced rule-based intention-follower
  behind the same hooks). Not now.
- The Director v1 is **rule / state-machine / constraint** (deterministic, replayable) — ML, if
  ever, is a later host-only second brain emitting into the *same* interface.

---

# Director vocabulary — the intention axes

The Director's *perceptual* vocabulary, not ABI/param detail. Axis set: **energy · tension ·
valence** (motion deferred).

## The law this obeys

The Director speaks **perceptual intention**, never **realization levers**. An axis is legitimate
only if it names *what the music feels like it is doing*, not *how a part is voiced*. Density,
register, instrumentation, groove intensity, space/reverb, timbre/color — those are the
**Arranger's** business: they are how intention is *realized*, downstream. A proposed axis that is
really a realization knob wearing a mood costume does not belong here. Two consequences:

- The axes are **few and orthogonal** — each a genuinely independent thing the conductor wants to
  steer, legible at a glance on the podium. More axes ≠ more expressive; redundant axes dilute the
  WOW and clutter the surface.
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
instability, expectation, the build→resolve arc) where dominance is a psychological control
construct with no clean mapping onto a band. A conscious deviation, not an oversight.

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
  claim "a build *obliges* a drop" is NOT an axis law but a **style-conditioned default**:
  EDM/techno/house profiles apply the build→resolve arc; drone/ambient/suspense suspend it — the
  same "genre-specific behaviour lives downstream" pattern as engine-driven swing (node 9100). A
  tension held and never resolved is a legitimate conductor's choice (user-wins), not an
  architectural impossibility.
- **Realized by (Arranger, examples):** dissonance/extensions, pedal points, suspensions, rolls or
  fills approaching a peak, harmonic rhythm, withholding the root.

### 3. valence — the emotional color, dark ↔ bright
- **Poles:** dark / heavy / melancholic  ↔  bright / open / uplifting.
- **Is:** the pleasant↔unpleasant, negative↔positive axis of the circumplex — the *character* of
  the moment, independent of how hard it pushes.
- **Is NOT:** timbre/tone-color (a realization/Arranger concern), and NOT mode alone (major≠happy is
  a cliché the Arranger may honor or subvert).
- **Replaces the old `mood`.** `mood` bundled valence + timbral color + more; we keep the clean
  perceptual half (valence) and hand the rest to the Arranger. The podium mockup draws this axis as
  "neutral → bright" — i.e. valence.
- **Realized by (Arranger, examples):** mode/scale color, chord-quality choices, voicing openness,
  melodic contour, instrumentation warmth.

## The deferred fourth — motion (candidate, not adopted)

The only additional axis that survives the orthogonality test is **motion / agitation** — perceived
*pace*, independent of energy: high energy + low motion = a powerful static wall; low energy + high
motion = a nervous, fidgety pianissimo. It is genuinely perceptual and not a realization knob.
**Not adopted now** — we start from the minimal orthogonal core and add on evidence. Recorded here
so a future add is principled, not ad-hoc.

**Explicitly NOT axes** (they are realization, never perceptual intention, however tempting):
density, register, instrumentation, groove/swing amount belong to the **Arranger**; space/reverb
and timbre/color live further downstream in the **Engines / melodd** (arrangrr decides symbolically
and never processes audio); `tempo` is an initial condition, not something realized continuously.
Steering any of them is a downstream job or a direct manual override, never a Director axis.

## How an axis behaves — trajectory, not setpoint

You do not set "the value now"; you set **where it should arrive and how fast**. So each axis is a
triple:

```
axis = { current, target, rate }   →   a trajectory the music walks
```

- **current** — the truthful now (observable, mirrored on the podium).
- **target** — where the conductor pointed it.
- **rate** — how fast to get there (e.g. "rising over 8 bars").

Two realization regimes as the trajectory advances:
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
  cage.
- **Deterministic.** Same axes + same trajectory + same seed → the same build, byte-exact next week.
  The intention is a reproducible musical object.

## Open / deferred
- The exact **widget** to express current→target+rate on the podium (kept loose).
- Whether **motion** graduates to a fourth axis (needs musical evidence).
- Numeric ranges/quantization per axis (an ABI/param concern, not this altitude).
- Whether valence carries a mode-bias hint to the Arranger, or stays purely perceptual.

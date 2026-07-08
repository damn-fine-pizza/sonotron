# Product Vision — sonotron, the intention-driven workstation

Status: **product definition, owner-validated 2026-07-07.** Product altitude only — no ABI/
command detail here. The product is named **sonotron** (decided 2026-07-07).
This is the OUTER product; `docs/product-identity.md` remains the identity of **arrangrr**, its
MIDI-brain component (unchanged: MIDI-only, STM32-capable, VST/audio-ignorant).

## What it is

An **intention-driven composition/performance workstation** — NOT a linear DAW (Reaper/Ardour),
NOT a clip-groovebox clone (Ableton Session), NOT an "AI makes you a track" generator (Suno). You
**conduct musical INTENTION** — energy, tension, valence, and a trajectory over time — and the machine
**composes and performs a full band toward that intention**, live, and **reproducibly** (same
intention → same result, byte-exact, next week).

The product is **NOT "MIDI-only"** — it has audio. But audio is subordinate *color*, never a
mixing/editing surface. It stays non-linear, non-DAW.

## What it does — capabilities (the "cosa fa", before flows)

1. **Conduct intention** — set or gesture a musical target (energy / tension / valence + a rate of
   change); the band re-arranges *toward* it gradually, committing changes on musical boundaries.
2. **Steer harmony live** — play chords (even one finger); the band follows and HOLDS them; the
   human hand beats the machine (`kLivePriority`).
3. **Build structure** — trigger sections / variations / fills / transitions; capture a trajectory
   as a **Song** (song-form without a timeline you paint on).
4. **Shape the feel** — style & section, groove / swing / humanize, mute & solo parts, arp.
5. **Deploy audio color** — the Arranger deploys clips / loops / samples / risers / vocal-chops as
   opaque decisions; simple recording of your gear/voice becomes arrangeable material (optional
   audio peer).
6. **Make sound via hosted sources** — VST/CLAP/LV2 instruments + melodd + sampler realize the
   MIDI; VST MIDI-FX in the chain (all in the outer product).
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
- **The workstation** — the host-only outer product. It **knows and orchestrates** arrangrr + VST
  instruments + audio + the Director + the GUI, plugging into arrangrr's hooks. This is where the
  "product" identity lives, and where MIDI-only is no longer the pitch.

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
  morph only for COLOR; tension must resolve (a release landed on a downbeat). *(axis set:
  energy / tension / valence, motion deferred — see `docs/design/director-vocabulary.md`.)*
- **arrangrr already IS the bottom two tiers** (arranger + sequencers + sections + harmony). The new
  work is the Director on top + audio-color engines beside + the GUI shell. A layer, not a rewrite.

## Sound & audio

- **arrangrr makes no sound at all** — MIDI or audio. It emits **symbols**: MIDI events (realized
  by external synths/VSTs) and **opaque audio-clip decisions** ("deploy clip N at bar X, loop to
  clock" — realized by melodd/the sampler). Symmetric doctrine: *a Program is a MIDI reference to an
  external sound; an audio clip is a reference to an external asset, not internal DSP.* It DECIDES
  audio, never PROCESSES it — which is exactly what keeps it STM32-capable. VST hosting, VST MIDI-FX,
  audio processing/mixing/recording, and "arrangrr-as-a-plugin" ALL live in the outer product.
- **Audio = arrangeable color:** clips, loops, textures, vocal chops, risers, samples, simple
  recording. Opaque, content-addressed, deployed by the Arranger as a decision. No linear timeline,
  no mixing-desk editing.
- **The audio engine is a SEPARATE, OPTIONAL peer** at the Engines tier — a sibling of the MIDI
  sequencers/sampler, behind the same uniform engine interface. arrangrr **uses it, never embeds
  it**; if it is absent (STM32, or a host without audio) arrangrr simply doesn't use it and degrades
  gracefully — no hard dependency. This is what keeps the STM32 build honest.

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
  working, decide what to backport into the core (e.g. a reduced rule-based intention-follower behind
  the same hooks). Not now.
- The Director v1 is **rule / state-machine / constraint** (deterministic, replayable) — ML, if ever,
  is a later host-only second brain emitting into the *same* interface.

# arrangrr — product identity

The one page every future decision is measured against. If a proposal fights
what is written here, the proposal is wrong, not this page. Distilled from
`docs/DESIGN.md` (D1, D2, D26, D33, D37, D38, D43) and the reflections in
`docs/reflections/` (gap-analysis, live-daw UX, melodd, external-audio-input).

## What it is

A **generative MIDI live instrument / arranger** — a machine that plays *with*
you (chord-reactive, "no wrong notes") and can play *itself* (a deterministic
co-pilot). Not a recording editor: **it records the *music* — MIDI, patterns,
gestures, chords — and can re-harmonize it**, which is more than an audio track,
not less.

Category, said plainly: a **generative arranger / live instrument**. Never a
"DAW", not even as internal shorthand — the word imports a gravity (linear audio
timeline, audio tracks, mixing, plugin hosting) the whole design exists to
refuse. Measured against Ableton it loses; measured against Genos +
Band-in-a-Box + a modular generative rig it wins on ground nobody holds.

## The three pillars (what no competitor offers together)

1. **Reproducible generativity** — every variation is a seeded, recallable fact
   (D16 + D29). The "seed is a musical object": audition N variations, lock one,
   recall it byte-exact next week.
2. **Harmonic intelligence** — NTT (D24): every arranger note is a chord tone by
   construction, so re-harmonizing a loop on the fly is lossless, and wrong notes
   are impossible.
3. **"No wrong notes" for the human hand, not only the machine** — the NTT
   resolver pointed at live input, so anyone can solo over the running band.

## What it is NOT (precise, so "no DAW" is never misread as "no recording")

- **NOT**: a linear audio timeline, audio tracks, audio editing/warp/comp, a
  mixer, or a plugin host. That is the DAW gravity, refused.
- **IS** (and records / loops / prepares clips): an arranger with a **looper and
  scenes** — MIDI capture, re-harmonizable clips, launchable scenes, all
  reproducible. The clip/session paradigm is *home* (Session view, yes); the
  linear audio arrangement timeline is *not* (Arrangement view, no).

Recording and clip-prep are in scope and central — in **MIDI / session** form.
"Not a DAW" is only about the audio-timeline surface.

## Audio (arrangeable color, realized off-core)

The **core makes no sound and processes no audio, forever (D1)** — it never records,
edits, or plays back waveforms; that is *why* STM32 (D2/D33), determinism, and
reproducibility work at all. But it is not literally "MIDI-only": the core is a
**realization-free SYMBOLIC arrangement brain** — it DECIDES both MIDI events *and*
audio-clip deployment (opaque references, "deploy clip N at bar X"), and REALIZES
neither. Symmetric to "a Program is a MIDI reference to an external sound." So
**"MIDI-only" is retired as the PRODUCT slogan** (the workstation product is
MIDI + audio + VST — see `docs/design/workstation-vision.md`); the core itself stays
realization-free and STM32-capable.

Sound/audio, when present, is realized by **`melodd`** (and a sampler/clip engine) — a
host-only audio companion (D43), a **separate, OPTIONAL peer** at the Engines tier that
the core USES, never embeds; absent (STM32, or a host without audio) → graceful no-op.
Rules:
- `melodd` is authoritative over **sound**, never over **time**; the core never
  learns that `melodd` (or a microphone) exists — it emits symbolic decisions a peer realizes.
- External audio **input** (voice/instrument) enters only inside `melodd` as an
  **opaque, content-addressed asset** the command/seed log *points to but never
  regenerates* — the seed owns every note and not one sample. Loop-to-clock yes;
  warp/edit on a linear timeline no.

## Architecture — peer modules, one orchestrator (D43)

```
          ORCHESTRATOR / host backend (per-deployment binary)
          ┌──────────┴───────────┐
      arrangrr                 melodd
     (core brain)             (audio, host)
   ── mutually name-blind; only a small POD interface between them ──
                    │  UDS-JSONL socket (D26/D38)
                    ▼
      GUI — SEPARATE PROCESS · pure client · links nothing (D38)
```

- **arrangrr** exposes a port (`Command` in / `OutEvent`+clock out — D26) and
  does **not** know a consumer exists.
- **melodd** exposes its own port and does **not** know arrangrr exists.
- The **orchestrator** (the target binary's `main`) is the only thing that knows
  both and wires their ports. Ports-and-adapters; the interface is POD and
  transport-agnostic (in-proc queue / socket / a wire between two chips), so it
  can descend to an MCU. Keep the port contract heap-free even host-side.

## Deployment matrix

| Target | Binary | Uses | Preset today |
|---|---|---|---|
| **Linux** | dev simulation | arrangrr (+ optional melodd-sim) | `host` |
| **STM32** | firmware | **arrangrr** alone (MIDI brain) | `arm` |
| **SBC** (Cortex-A / DSP) | full instrument | **arrangrr + melodd** | *(future)* |

arrangrr is the **invariant kernel** on every target (freestanding, portable).
`melodd` appears only where the HW has audio. "One binary uses both" on the SBC
still means two name-blind libraries + an orchestrator layer — never mutual
`#include`, never an `#ifdef melodd` inside the core.

## Naming — decided 2026-07-07

Category term to use internally instead of "DAW": **generative arranger /
live-instrument**.

The names are fixed:
- **sonotron** — the outer, host-only workstation product (the "intention-driven
  workstation"; see `docs/design/workstation-vision.md`).
- **arrangrr** — the core symbolic MIDI-brain component (this page): STM32-capable,
  realization-free.
- **melodd** — the optional host-only audio peer engine (D43) that realizes sound.

Decided against this page, not against a feature list.

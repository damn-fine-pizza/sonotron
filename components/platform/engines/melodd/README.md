# melodd — the built-in GM audio realizer

Phase-5 Item F / `0910` (D43), first slice. `melodd` is arrangrr's
HOST-ONLY audio companion: a MIDI-only symbolic core (`components/
arrangrr`) makes decisions but never sound (`docs/design/
workstation-vision.md`); `melodd` is one of the "Engines" that REALIZES
that MIDI into audio, exactly as an external hardware synth would.

## What this first slice is

A single built-in General MIDI SoundFont synth, wrapped in a clean
"realizer" library plus a standalone drop-in binary:

- **`melodd::Synth`** (`include/melodd/synth.hpp`, `src/synth.cpp`) — wraps
  [TinySoundFont](https://github.com/schellingb/TinySoundFont) (vendored at
  `third_party/tinysoundfont`, see its `ARRGRR_VENDOR.md`). It loads a `.sf2`
  SoundFont, accepts MIDI note-on/note-off/program-change/pitch-bend (and,
  since TinySoundFont routes it for free, control-change), and renders
  interleaved stereo float audio frames. This is the reusable "Engines tier"
  core: one voice per channel/preset, no effects, no mixing, no editing
  surface. It knows nothing about ALSA, an audio device, or arrangrr itself.
- **`apps/tools/melodd`** — a standalone host binary that opens an ALSA
  sequencer MIDI **input** port named `melodd` and a
  [miniaudio](https://github.com/mackron/miniaudio) (vendored at
  `third_party/miniaudio` — see `third_party/miniaudio/ARRGRR_VENDOR.md`)
  playback device. Its audio callback pulls rendered frames straight from a
  `melodd::Synth`; incoming ALSA MIDI bytes are decoded and fed into the same
  `Synth`. This makes it a **drop-in FluidSynth replacement** for the demo
  wiring (`apps/demo/lib/launch.sh`) — connect arrangrr's MIDI output to it
  with `aconnect` and you hear the band, no external synth required.

## Why the standalone binary, not in-process (yet)

This slice is deliberately the **standalone-first** path: a separate process,
wired over ALSA like any other external synth, because that is the smallest
change that makes the workstation audible today and it costs the core
nothing (the core stays exactly as ignorant of `melodd` as it is of any
other MIDI destination).

**Documented follow-up, not this slice:** an **in-process peer** path, where
the GUI/daemon feeds arrangrr's `OutEvent` MIDI stream directly to a
`melodd::Synth` instance living in the same process — no ALSA round-trip,
no separate binary — via the orchestrator's opaque-reference pattern
(`components/orchestrator`, the same "consume a peer through a contract,
not a concrete type" shape Phase 4 already proved). That follow-up reuses
`melodd::Synth` completely unchanged; only the wiring (who calls
`note_on`/`render`) moves from an ALSA MIDI parser + miniaudio device
callback to a direct in-process call from the daemon's own output sink.

## Scope discipline (read before extending)

Per `docs/backlog/melodd-audio-companion.md` and
`docs/design/phase5-execution-plan.md` Item F: **`melodd` may own the
sound, never the time, and the core may never learn its name.** Concretely,
for this component:

- One synth voice per MIDI channel realizing General MIDI. No mixer, no
  effects chain, no audio editing surface — that is the DAW-gravity the
  product-identity docs refuse.
- `melodd` is a **downstream sink** of MIDI, exactly like an external synth.
  It never becomes authoritative over time; it does its own best-effort
  sub-buffer placement of incoming events on its own audio callback,
  downstream of and never commanding the core's scheduler.
- No soundfont is vendored (licensing/size): `melodd` loads a system GM
  `.sf2` at runtime, mirroring `apps/demo/lib/launch.sh`'s discovery order
  (see `apps/tools/melodd/main.cpp`).
- `components/melodd` links only `third_party/tinysoundfont` — never
  `arrangrr`, `runtime`, or `hostrt`. It is realization-ignorant of its
  caller, same as `components/midisrc`'s stage adapter is ignorant of its
  neighbours (D43).

## Regime

HOST-ONLY. This component and `apps/tools/melodd` must never be added to
the arm-none-eabi (firmware) branch of the top-level `CMakeLists.txt` — same
placement discipline as `components/midisrc` and `components/orchestrator`.

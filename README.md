# arrangrr

MIDI-only, realtime-first music brain: arranger + sequencer + MIDI looper.
Primary runtime target **STM32**; Linux is the devenv/simulation host (CLI + virtual MIDI).
Never audio — it drives external synths, grooveboxes and DAWs over DIN-5 and USB MIDI.

The full design document (vision, decisions log D1–D33, dream feature list,
milestone roadmap M0–M13, CLI API design, L1/L0 contract) lives in
[`docs/DESIGN.md`](docs/DESIGN.md).

## Tooling

- [`app/tools/arrstyle-converter`](app/tools/arrstyle-converter/README.md) —
  host-only CLI that imports alien style/song formats (Standard MIDI File,
  ChordPro; SFF inspect-only) into arrangrr's native style/song JSON.
  Dependency-free, excluded from the ARM build. Design:
  [`DESIGN.md`](app/tools/arrstyle-converter/DESIGN.md).

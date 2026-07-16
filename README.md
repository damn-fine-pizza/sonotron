# sonotron

An **intention-driven composition/performance workstation** — not a linear DAW.
You conduct musical *intention* (energy / tension / valence and a trajectory
over time) and the machine composes and performs a full band toward it, live
and reproducibly. sonotron is **not** MIDI-only: it has audio, but audio is
subordinate *color*, never a mixing/editing surface.

Three names, one product:

- **sonotron** — the outer, host-only workstation product described above.
- **arrangrr** (`components/core/arrangrr`) — the symbolic MIDI-brain core: arranger
  + sequencers + MIDI-FX + live harmony. Dual-target (Linux host + STM32),
  no-heap, and **realization-free** — it decides, it never renders sound; MIDI
  goes to external synths/DAWs, audio-clip decisions go to a peer realizer.
- **melodd** (`components/platform/engines/melodd` + `apps/tools/melodd`) — the optional,
  host-only audio peer that realizes sound for arrangrr: a General MIDI
  SoundFont player (TinySoundFont + miniaudio). Absent on the STM32 target or
  on a host without audio, arrangrr degrades gracefully — melodd owns sound,
  never time.

## Documentation

The design docs were consolidated into a flat set of canonical files under
[`docs/`](docs/):

- [`docs/product-vision.md`](docs/product-vision.md) — the product definition:
  what sonotron is, the three pillars, the arrangrr/melodd boundary, and the
  sound & audio doctrine. Owner-validated 2026-07-07; this is the authoritative
  naming source.
- [`docs/architecture.md`](docs/architecture.md) — how the repository is
  organized and how the components compose at runtime.
- [`docs/DESIGN.md`](docs/DESIGN.md) — the full design document: architectural
  principles, module taxonomy, the CLI API design, the ABI/L1-L0 contract, and
  the canonical numbered roadmap & decision record (§22) — the old `D1..D54`
  decisions log and `M0..M13` milestone list are retired as nomenclature in
  favor of that single numbered tree (see DESIGN.md §0).
- [`docs/roadmap.md`](docs/roadmap.md) — a proposal (marked `Status: PROPOSAL`
  in the file itself) for a single canonical numbered roadmap/decision record;
  left as found, not resolved here.
- [`docs/phase5-plan.md`](docs/phase5-plan.md) — the current execution plan and
  ordering for the active Phase-5 work items.
- [`docs/phase5-design-reviews.md`](docs/phase5-design-reviews.md) —
  consolidated architecture/scope design reviews for those Phase-5 items.
- [`docs/gui-and-ux.md`](docs/gui-and-ux.md) — the GUI/UX spec: the desktop
  workstation screen, the GUI↔core wire contract, and the TUI piano/MIDI-
  monitor spec.
- [`docs/style-corpus-and-generation.md`](docs/style-corpus-and-generation.md)
  — the arranger STYLE subsystem: data model, per-style FEEL values, the
  Yamaha SFF/CASM corpus, and generation/stylizer options.
- [`docs/backlog-future.md`](docs/backlog-future.md) — judged-but-unscheduled
  future directions (arranger/sequencer code-boundary split, further melodd
  audio work, external audio input capture).

## Layout

`components/` splits on the regime axis — `core/` (platform-agnostic,
freestanding-capable, cross-builds for STM32 too) vs `platform/` (host-only,
needs a hosted OS). See `docs/architecture.md` §2/§3 for the full doctrine.

`components/core/` — dual-target building blocks:

- `arrangrr` — the symbolic MIDI-brain core (see above).
- `chorddet` — dual-target, freestanding chord-detection peer (`ChordDetector`
  + `FollowedContext`), no heap.
- `runtime` — the dual-target, no-heap kernel (`Transport` + `OutScheduler` +
  the generic `Pipeline`/`Stage` mechanism) shared by host and firmware.
- `common` — the base, dependency-free header-only layer `runtime` and
  `arrangrr` both build on.
- `audio_engine` — the `ISoundEngine` contract (dispatch/all_notes_off/render/
  name), POD-only, no deps; the pluggable sound-engine seam a concrete engine
  implements.

`components/platform/` — host-only building blocks (flat, no further regime
nesting):

- `hostrt` — host-only runtime/shell: ALSA MIDI I/O, the UDS control-socket
  server/client, the REPL/console, and the TUI panels (piano, groove, arp,
  parts, style chooser) that drive arrangrr interactively.
- `orchestrator` — host-only; names and composes concrete multi-stage
  pipelines (e.g. Accompany) out of `runtime`'s `Pipeline` mechanism and
  `midisrc`'s MIDI-source stage.
- `midisrc` — host-only MIDI-source material: the hand-rolled SMF reader and
  its `Diagnostics` reporting.
- `audio` — the concrete host audio backend (`AudioBackend`, the miniaudio
  device layer) and `SoundfontEngine`, the first concrete `ISoundEngine`
  implementation (wraps `melodd::Synth`). Promoted out of `apps/gui-sonotron`
  so any pure-client app can reuse it.
- `engines/melodd` — the optional GM SoundFont audio realizer (host-only).

`components/samplrr` — a reserved slot for a future host-only sampler engine,
not yet folded into `platform/`; no code yet.

`apps/` — the deployable binaries:

- `gui-sonotron` — the host-only Dear ImGui desktop workstation GUI (the
  6-zone screen: Transport, Browser, Repeat Zone, Intention-over-Parts,
  Sequence Edit). Talks to the backend either as a UDS-JSONL client of a
  running `sonotron-server`, or, by default, by hosting the engine
  in-process on a dedicated thread — see `docs/architecture.md` and
  `docs/DESIGN.md` (D38) for the current split.
- `sonotron-server` — the headless backend: ALSA + the UDS control socket +
  the tick-timer clock drive around the same `hostrt` Shell/`runtime`/
  `arrangrr` composition `cli-arrangrr` uses; also drives the deterministic
  `--script` golden-test mode.
- `demo/` — shell wrappers (`clean/`, `jam/`, `lib/launch.sh`) that wire a
  synth (FluidSynth or melodd) and `cli-arrangrr` together for a self-
  contained live demo.
- `tools/arrstyle-converter` — host-only CLI that imports alien style/song
  formats (Standard MIDI File, ChordPro; SFF inspect-only) into arrangrr's
  native style/song JSON. Dependency-free, excluded from the ARM build.
  Design: [`DESIGN.md`](apps/tools/arrstyle-converter/DESIGN.md).
- `tools/arrstyle-extractor` — a dependency-free (Python stdlib only)
  knowledge-base extractor that derives an abstract, generation-oriented KB
  from the Yamaha SFF/Korg/SMF style corpus.
- `tools/cli-arrangrr` — the host arrangrr binary: REPL/TUI, the
  `--script` golden-test driver, and a `--connect` pure-UDS-client mode
  against a running `sonotron-server`.
- `tools/melodd` — a standalone drop-in-synth binary wrapping
  `melodd::Synth` (ALSA MIDI in, miniaudio out); a direct FluidSynth
  replacement for the demo wiring.

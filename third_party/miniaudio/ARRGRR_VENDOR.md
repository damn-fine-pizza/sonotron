# Vendored: miniaudio

- Upstream: https://github.com/mackron/miniaudio
- Pinned tag: `0.11.25`
- Pinned commit: `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`
- License: public domain (Unlicense) OR MIT-0, vendor's choice (see
  `LICENSE`, copied verbatim from upstream) — this project treats it as MIT-0

## What was vendored

Single-header cross-platform audio playback/capture library. Only the
header itself is needed to build the library.

```
miniaudio/
  miniaudio.h
  miniaudio_impl.c
  LICENSE
```

`miniaudio_impl.c` is OURS (not upstream's) — a one-line translation unit
that defines `MINIAUDIO_IMPLEMENTATION` before including `miniaudio.h`, per
the header's own documented usage. Upstream ships no build system for the
header; this is the minimal way to turn a single-header library into a
linkable static library, same idea as `third_party/imgui`'s
`CMakeLists.txt` (ours, not upstream's).

## How it is built

`CMakeLists.txt` in this directory (ours) compiles `miniaudio_impl.c` into
a static library target `miniaudio`, publishing this directory as its
public include path (`#include <miniaudio.h>`), and links the platform
audio backend libraries it needs on Linux (`dl`, `pthread`, `m`; ALSA is
opened at runtime via `dlopen`, not linked).

## Regime

Host-only. Never added to the freestanding/arm-none-eabi build graph
(`third_party/CMakeLists.txt` is only entered from the host branch of the
top-level `CMakeLists.txt`). Never included by any file under
`components/` other than `components/melodd`'s standalone playback binary
(`apps/tools/melodd`) — the `melodd` library itself does not link
miniaudio; only the app that owns the audio device does.

## Why this dependency (D43/`0800`, owner-approved 2026-07-13)

Chosen together with TinySoundFont for `melodd`'s first slice
(`components/melodd/README.md`): single-header, permissive license, no
transitive dependencies, widely used cross-platform audio-callback
library, small enough to audit.

## Updating the pin

Re-run the same vendoring: fetch the upstream `miniaudio.h` at a new
tagged release, diff it against the current copy, bump the tag/commit in
this file. Do not hand-edit the vendored header.

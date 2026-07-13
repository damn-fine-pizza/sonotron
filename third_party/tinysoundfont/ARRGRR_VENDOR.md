# Vendored: TinySoundFont

- Upstream: https://github.com/schellingb/TinySoundFont
- Pinned branch/commit: `main` @ `fbc913531b85f5707f49115110bb86b1cd583885`
- License: MIT (see `LICENSE`, copied verbatim from upstream)

## What was vendored

Single-header SoundFont2 (`.sf2`) synthesizer. Only the header itself is
needed to build the library.

```
tinysoundfont/
  tsf.h
  tsf_impl.c
  LICENSE
```

`tsf_impl.c` is OURS (not upstream's) — a one-line translation unit that
defines `TSF_IMPLEMENTATION` before including `tsf.h`, per the header's own
documented usage (see the comment block at the top of `tsf.h`). Upstream
ships no build system for the header; this is the minimal way to turn a
single-header library into a linkable static library, same idea as
`third_party/imgui`'s `CMakeLists.txt` (ours, not upstream's).

## How it is built

`CMakeLists.txt` in this directory (ours) compiles `tsf_impl.c` into a
static library target `tinysoundfont`, publishing this directory as its
public include path (`#include <tsf.h>`).

## Regime

Host-only. Never added to the freestanding/arm-none-eabi build graph
(`third_party/CMakeLists.txt` is only entered from the host branch of the
top-level `CMakeLists.txt`). Never included by any file under
`components/` other than `components/melodd`.

## Why this dependency (D43/`0800`, owner-approved 2026-07-13)

Chosen together with `miniaudio` for `melodd`'s first slice
(`components/melodd/README.md`): single-header, permissive license, no
transitive dependencies, widely used, small enough to audit. TinySoundFont
renders a General MIDI SoundFont; `melodd` does not vendor a soundfont
itself (licensing/size) — it loads a system GM `.sf2` at runtime.

## Updating the pin

Re-run the same vendoring: fetch the upstream `tsf.h` at a new commit,
diff it against the current copy, bump the commit hash in this file. Do
not hand-edit the vendored header.

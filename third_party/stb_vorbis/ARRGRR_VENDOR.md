# Vendored: stb_vorbis

- Upstream: https://github.com/nothings/stb
- Pinned file/commit: `stb_vorbis.c` @ `1ee679ca2ef753a528db5ba6801e1067b40481b8` (2021-07-12, "update version numbers", stb_vorbis v1.22)
- License: dual MIT / Public Domain (Unlicense) -- see `LICENSE`, copied verbatim from the upstream repo root (stb_vorbis.c has no separate per-file license file; its own trailing comment block carries the identical dual-license text).

## What was vendored

Single-file public-domain Ogg Vorbis decoder. Unlike `third_party/tinysoundfont`
(which ships a separate `tsf.h` declarations header + `tsf_impl.c`), upstream
`stb_vorbis.c` is ONE file that plays BOTH roles via macros -- its own
internal `#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H` guard (lines ~74-414)
holds the declarations, and `#ifndef STB_VORBIS_HEADER_ONLY` (lines ~420
onward) holds the implementation. There is no separate `stb_vorbis.h`
upstream to vendor.

```
stb_vorbis/
  stb_vorbis.c       (upstream, verbatim, do not hand-edit)
  stb_vorbis_impl.c  (OURS -- compiles the full implementation into a TU)
  LICENSE            (upstream repo root LICENSE, verbatim)
```

`stb_vorbis_impl.c` is OURS (not upstream's), mirroring `tinysoundfont/
tsf_impl.c`'s own pattern: a one-line translation unit that `#include`s
`stb_vorbis.c` WITHOUT `STB_VORBIS_HEADER_ONLY` defined, pulling in the full
implementation once. Any OTHER translation unit that needs only the
declarations (to call into the symbols this TU compiles) must `#define
STB_VORBIS_HEADER_ONLY` before its own `#include <stb_vorbis.c>`, then
`#undef` it again -- otherwise it would recompile the full implementation a
second time and the linker would reject the duplicate symbol definitions.
`third_party/tinysoundfont/tsf_impl.c` does exactly this to unlock TSF's
`.sf3` decode path; `components/platform/engines/melodd/tests/
test_stb_vorbis_linked.cpp` does the same to prove the link actually works.

## How it is built

`CMakeLists.txt` in this directory (ours) compiles `stb_vorbis_impl.c` into
a static library target `stb_vorbis`, publishing this directory as its
public include path (`#include <stb_vorbis.c>`, header-only or full,
depending on whether the includer defines `STB_VORBIS_HEADER_ONLY` first).

## Regime

Host-only. Never added to the freestanding/arm-none-eabi build graph
(`third_party/CMakeLists.txt` is only entered from the host branch of the
top-level `CMakeLists.txt`). Only reached from
`third_party/tinysoundfont/tsf_impl.c` (to unlock TSF's `.sf3` decode path)
and from `components/platform/engines/melodd/tests/
test_stb_vorbis_linked.cpp` (a link-proof test) -- never included by any
other file under `components/`.

## Why this dependency (Phase-1 sound task #7, docs/proposals/audio-engine-fluidsynth-build-vs-buy.md SS6 "B2", owner-approved 2026-07-18)

TSF's own `.sf3` (Ogg-Vorbis-compressed SoundFont) decode path
(`tsf_decode_ogg`/`tsf_decode_sf3_samples`, `tsf.h` ~lines 867-935) already
existed, gated behind `STB_VORBIS_INCLUDE_STB_VORBIS_H` -- this is purely a
vendoring exercise to activate an already-written code path, not new
architecture. Single-file, public-domain/MIT dual-licensed, no transitive
dependencies, same class of dependency as `third_party/tinysoundfont`
itself. `.sf3` is a file-size/packaging benefit (smaller SoundFonts to
bundle/download), not a timbral one -- it does not by itself address the
"sounds synthetic" gap (that is deliverable A, `ReverbSoundEngine`, the same
task's B1).

## Updating the pin

Re-run the same vendoring: fetch upstream `stb_vorbis.c` at a new commit,
diff it against the current copy, bump the commit hash in this file. Do not
hand-edit the vendored file (`stb_vorbis_impl.c` and `CMakeLists.txt` are
ours and may be edited; `stb_vorbis.c` itself is not).

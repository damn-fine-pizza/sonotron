# Vendored: GLFW

- Upstream: https://github.com/glfw/glfw
- Pinned tag: `3.4`
- Pinned commit: `a74efa0d5628b74adc0426af4c5710e287fa7c2c`
- License: zlib/libpng (see `LICENSE.md`, copied verbatim from upstream)

## What was vendored

The full buildable core: `CMakeLists.txt`, `CMake/`, `include/`, `src/`,
`deps/` (bundled header-only helpers GLFW's own CMake needs, e.g. glad/
mingw headers), plus `LICENSE.md`, `README.md`, `CONTRIBUTORS.md`.

NOT vendored: upstream `docs/`, `examples/`, `tests/` — not needed to build
the library and not referenced unless `GLFW_BUILD_DOCS` / `_EXAMPLES` /
`_TESTS` are turned on, which the top-level build forces OFF (see
`third_party/CMakeLists.txt`).

## How it is built

GLFW's own upstream `CMakeLists.txt` is used unmodified — it is a real CMake
project vendored whole (minus the trimmed directories above), added via
`add_subdirectory()`. `GLFW_BUILD_WAYLAND` / `GLFW_BUILD_X11` follow GLFW's
own UNIX defaults (both ON on Linux), matching the "Wayland-native" rationale
in `docs/design/gui-toolkit-decision.md`.

## Regime

Host-only. Never added to the freestanding/arm-none-eabi build graph. Never
linked by any file under `components/`.

## Updating the pin

Re-run the same vendoring: clone the upstream tag, copy the directories
listed above verbatim (do not hand-edit vendored sources), bump the
tag/commit in this file.

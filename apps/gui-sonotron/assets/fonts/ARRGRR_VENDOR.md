# Vendored: JetBrains Mono NL (Regular)

- Upstream: https://github.com/JetBrains/JetBrainsMono
- Distribution channel: Fedora package `jetbrains-mono-nl-fonts`
  (`2.304-10.fc44`, `noarch`), copied from the build host's system font
  directory (`/usr/share/fonts/jetbrains-mono-nl-fonts/JetBrainsMonoNL-Regular.ttf`
  on the Fedora host underlying this container, reachable here at
  `/run/host/usr/share/fonts/jetbrains-mono-nl-fonts/JetBrainsMonoNL-Regular.ttf`).
  No network fetch was needed — the font ships as a stock Fedora package.
- License: SIL Open Font License 1.1 (see `LICENSE-OFL.txt`, copied verbatim
  from the same Fedora package's `/usr/share/licenses/jetbrains-mono-nl-fonts/OFL.txt`).
- "NL" = "No Ligatures" build of JetBrains Mono. Chosen over the ligature
  build on purpose: Dear ImGui has no ligature-substitution shaping, so a
  ligature font would just waste glyph-atlas space on unused glyph forms.
  Same license, same metrics, same monospace grid as regular JetBrains Mono.

## What was vendored

Only the Regular weight, as a single TTF — this is a UI/dashboard font, not
a text editor; no bold/italic styling is currently used anywhere in the app.

```
assets/fonts/
  JetBrainsMonoNL-Regular.ttf
  LICENSE-OFL.txt
  ARRGRR_VENDOR.md   (this file)
```

## How it is used

Loaded at runtime by `apps/gui-sonotron/main.cpp` via
`ImGui::GetIO().Fonts->AddFontFromFileTTF(...)`, rasterized at
`font_size * content_scale` pixels — see `content_scale_for()` (queries
`glfwGetWindowContentScale`), `resolved_base_font_size_px()`, and
`load_font()` in `main.cpp`. If the file is missing or fails to parse at
startup, the app logs a warning and falls back to ImGui's built-in bitmap
font (ProggyClean), scaled to the same pixel size, rather than failing to
start.

The *logical* (1x DPI) base font size is a configurable knob, not a hardcoded
constant: the `"font_size"` key in the layout JSON
(`~/.config/sonotron/layout.json` by default — see `Layout::font_size_px` in
`src/layout_model.hpp`). Default is `kDefaultFontSizePx` (14px) if the key is
absent or its value falls outside `[kMinFontSizePx, kMaxFontSizePx]`
(6..64px); the resolved value round-trips through save/load like the rest of
the layout file.

The font PATH resolution order (see `font_path()` in `main.cpp`) is:

1. `SONOTRON_FONT_PATH` env override (used by tests/verification only).
2. The source-tree `assets/fonts/JetBrainsMonoNL-Regular.ttf` path, baked in
   at compile time via the `SONOTRON_ASSETS_DIR` CMake define (works when
   running the freshly built binary straight from the build tree during
   development; a real install layout is a later, separate milestone).

## Regime

Host-only asset. This app (`gui-sonotron`) is a pure client (D38): it never
links `arrangrr_core`/`hostrt` and this directory is never added to the
arm-none-eabi build graph. A font file is a data asset, not a code
dependency — no new third-party code library was introduced.

## Updating the pin

Re-copy `JetBrainsMonoNL-Regular.ttf` and `OFL.txt` from a refreshed
`jetbrains-mono-nl-fonts` Fedora package (or from the upstream GitHub
release, same license) and bump the version noted above. Do not hand-edit
the font binary.

# Vendored: Dear ImGui

- Upstream: https://github.com/ocornut/imgui
- Pinned tag: `v1.92.8`
- Pinned commit: `8936b58fe26e8c3da834b8f60b06511d537b4c63`
- License: MIT (see `LICENSE.txt`, copied verbatim from upstream)

## What was vendored

Core library sources/headers only (root of the upstream repo) plus the two
backend files this project uses. Upstream `examples/`, `docs/`, and `misc/`
are NOT vendored — they are not needed to build the library.

```
imgui/
  imconfig.h
  imgui.h
  imgui_internal.h
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
  LICENSE.txt
  backends/
    imgui_impl_glfw.h / .cpp
    imgui_impl_opengl3.h / .cpp
    imgui_impl_opengl3_loader.h
```

## How it is built

`CMakeLists.txt` in this directory (ours, not upstream's — Dear ImGui ships
no build system of its own) compiles the above into a static library target
`imgui`, linking `glfw` (from `third_party/glfw`) and `OpenGL::GL`.

## Regime

Host-only. Never added to the freestanding/arm-none-eabi build graph. Never
included by any file under `components/`.

## Updating the pin

Re-run the same vendoring: clone the upstream tag, copy the files listed
above verbatim (do not hand-edit vendored sources), bump the tag/commit in
this file.

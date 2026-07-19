# Vendored third-party dependencies (host-only). Never add_subdirectory()'d
# from the firmware/freestanding branch of the top-level CMakeLists.txt —
# the core stays dependency-free (docs/DESIGN.md, D1-family constraints).
# See the per-library ARRGRR_VENDOR.md for provenance and pinned versions.

# GLFW is vendored as a full, unmodified upstream CMake project (minus the
# trimmed docs/examples/tests directories — see glfw/ARRGRR_VENDOR.md). We
# only need the library, so force its own examples/tests/docs/install off
# before entering its CMakeLists.txt.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

add_subdirectory(imgui)

# stb_vorbis: single-file public-domain Ogg Vorbis decoder, vendored solely
# to unlock TinySoundFont's already-present .sf3 (compressed SoundFont)
# decode path (Phase-1 sound task #7, docs/proposals/
# audio-engine-fluidsynth-build-vs-buy.md SS6 "B2", owner-approved
# 2026-07-18). Must be entered BEFORE tinysoundfont: tinysoundfont's own
# CMakeLists.txt links against the `stb_vorbis` target defined here.
add_subdirectory(stb_vorbis)

# TinySoundFont + miniaudio: melodd's first-slice audio dependency (D43/
# `0800`, owner-approved 2026-07-13). Single-header, permissive license, see
# each library's own ARRGRR_VENDOR.md.
add_subdirectory(tinysoundfont)
add_subdirectory(miniaudio)

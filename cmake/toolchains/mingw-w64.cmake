# Cross toolchain for a Windows build from a Linux host, via mingw-w64
# (docs/proposals/looper-in-gui-contract.md §7 item 12, the cross-platform
# MIDI backend track -- local cross path; CI's own windows-latest leg builds
# natively with MSVC instead, see components/platform/hostrt/winmm_midi.hpp).
# Probed by scripts/gui-sonotron/build.sh windows (x86_64-w64-mingw32-g++);
# mirrors both this repo's own cmake/toolchains/arm-cortex-m7.cmake (the four
# CMAKE_FIND_ROOT_PATH_MODE_* settings) and the vendored GLFW-compatible
# pattern (third_party/glfw/CMake/x86_64-w64-mingw32.cmake).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

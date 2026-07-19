# Cross toolchain for a macOS build from a Linux host, via osxcross
# (docs/proposals/looper-in-gui-contract.md §7 item 12, the cross-platform
# MIDI backend track). Probed by scripts/gui-sonotron/build.sh osx
# (o64-clang++). osxcross is almost certainly NOT installed on a plain dev
# box -- that is expected; this file is code-complete / UNVERIFIED locally,
# to be verified via CI macos-latest instead (the owner's primary
# verification path for macOS -- see components/platform/hostrt/
# coremidi_midi.hpp's own doc comment). Standard osxcross pattern, mirroring
# this repo's own cmake/toolchains/arm-cortex-m7.cmake (the four
# CMAKE_FIND_ROOT_PATH_MODE_* settings).

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER o64-clang)
set(CMAKE_CXX_COMPILER o64-clang++)

# osxcross wraps the target SDK root itself (OSXCROSS_SDK / the o64-clang*
# wrapper scripts already resolve -isysroot); CMake's own find_* commands
# still need CMAKE_FIND_ROOT_PATH pointed at that SDK so libraries/headers
# resolve from it rather than from the Linux host's own /usr.
if(DEFINED ENV{OSXCROSS_SDK})
  set(CMAKE_FIND_ROOT_PATH "$ENV{OSXCROSS_SDK}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

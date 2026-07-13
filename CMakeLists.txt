cmake_minimum_required(VERSION 3.28)

# C is needed only for the single-header C libraries melodd vendors
# (TinySoundFont/miniaudio, third_party/tinysoundfont and third_party/
# miniaudio) -- enabled at top level (rather than relying solely on GLFW's
# own nested project(LANGUAGES C)) so the Ninja generator sets up the C
# object/archive rules correctly from the start.
project(sonotron LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No C++ modules here: scanning only adds -fmodules-ts noise to every compile
# command (breaking clang-tidy on the GCC database) and slows builds down.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Firmware = any cross build with CMAKE_SYSTEM_NAME=Generic (see cmake/toolchains/).
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
  set(ARRANGRR_FIRMWARE ON)
  # Pinned minimum for the C++26 subset (DESIGN.md §20).
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "arm-none-eabi-g++ >= 14 required (found ${CMAKE_CXX_COMPILER_VERSION})")
  endif()
else()
  set(ARRANGRR_FIRMWARE OFF)
endif()

# Phase-1 runtime extraction (docs/design/orchestrator-pipeline-extraction.md):
# components/common (the base, dependency-free layer) and components/runtime
# (the dual-target kernel: Transport + OutScheduler + the Stage port) are
# unconditional — BOTH the arm-none-eabi branch and the host branch need them.
add_subdirectory(components/common)
add_subdirectory(components/runtime)
# Phase-4b promotion (§16.1/§16.8): components/chorddet (ChordDetector +
# FollowedContext + the chorddet Seam-C stage) is ALSO unconditional and
# dual-target -- arrangrr now depends on it for these types (D43: the
# dependency runs arrangrr -> chorddet, chorddet never depends on arrangrr).
add_subdirectory(components/chorddet)
add_subdirectory(components/arrangrr)

if(ARRANGRR_FIRMWARE)
  add_subdirectory(tests/arm-smoke)
else()
  enable_testing()
  # Phase 4c/4d: the MIDI-source material (SMF reader + Diagnostics + Stage
  # adapter, promoted out of arrstyle-converter, §16.5) and
  # components/orchestrator (the Accompany pipeline NAME/composition,
  # §16.3/§16.8) both need to exist BEFORE hostrt, which now depends on the
  # Accompany pipeline shape too (docs/design/orchestrator-pipeline-
  # extraction.md §16.7: hostrt::Shell drives the SAME 3-stage pipeline type
  # interactively AND for the `midi-source load` L1 verb). HOST-ONLY; never
  # on the arm-none-eabi branch above.
  add_subdirectory(components/midisrc)
  add_subdirectory(components/orchestrator)
  add_subdirectory(components/hostrt)
  add_subdirectory(apps/tools/cli-arrangrr)
  # Phase 2a (docs/design/orchestrator-pipeline-extraction.md §15 /
  # docs/design/sonotron-server-phase2-brief.md): the headless server binary,
  # a thin main() around the SAME hostrt Shell/Runtime/arrangrr composition
  # cli-arrangrr already links -- additive, cli-arrangrr is untouched.
  add_subdirectory(apps/sonotron-server)
  add_subdirectory(components/arrangrr/tests)
  add_subdirectory(components/runtime/tests)
  add_subdirectory(components/hostrt/tests)
  add_subdirectory(tests/golden)
  add_subdirectory(tests/integration)
  # Host-only importer/compiler tooling (never in the ARM/firmware build).
  add_subdirectory(apps/tools/arrstyle-converter)
  # Host-only GUI pure client (D38) + its vendored toolkit. Never in the
  # ARM/firmware build; the GUI never links arrangrr or hostrt.
  add_subdirectory(third_party)
  add_subdirectory(apps/gui-sonotron)
  # Phase-5 Item F (docs/design/phase5-execution-plan.md, `0910`/D43): the
  # melodd GM SoundFont realizer + its standalone drop-in-synth binary.
  # HOST-ONLY, links third_party/tinysoundfont (just entered above) --
  # never on the arm-none-eabi branch above, same placement discipline as
  # components/midisrc / components/orchestrator.
  add_subdirectory(components/melodd)
  add_subdirectory(apps/tools/melodd)
endif()

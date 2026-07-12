cmake_minimum_required(VERSION 3.28)

project(sonotron LANGUAGES CXX)

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
add_subdirectory(components/arrangrr)

if(ARRANGRR_FIRMWARE)
  add_subdirectory(tests/arm-smoke)
else()
  enable_testing()
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
endif()

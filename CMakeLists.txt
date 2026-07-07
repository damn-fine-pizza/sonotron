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

add_subdirectory(components/arrangrr)

if(ARRANGRR_FIRMWARE)
  add_subdirectory(tests/arm-smoke)
else()
  enable_testing()
  add_subdirectory(components/hostrt)
  add_subdirectory(apps/tools/cli-arrangrr)
  add_subdirectory(components/arrangrr/tests)
  add_subdirectory(components/hostrt/tests)
  add_subdirectory(tests/golden)
  add_subdirectory(tests/integration)
  # Host-only importer/compiler tooling (never in the ARM/firmware build).
  add_subdirectory(apps/tools/arrstyle-converter)
endif()

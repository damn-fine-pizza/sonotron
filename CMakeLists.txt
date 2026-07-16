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

# Firmware-release path (Phase 6 Theme 1c): ARR_ASSERT is __builtin_trap() on
# failure everywhere by default (the dev/CI cross build net stays asserting).
# A genuine release firmware image must never trap on a REACHABLE runtime
# condition (D-STM32: graceful degradation, never crash) -- every ARR_ASSERT
# site in the core that guards a bounded-cap drop already degrades on its own
# once the assert itself is compiled out (the surrounding `if (cap) { ...;
# break; }` / `return false;` still runs); see the audit in the Phase 6 Theme
# 1c task notes. Opting into this flag is a deliberate, separate build (a new
# preset), not the default `arm` preset.
option(ARRANGRR_FIRMWARE_RELEASE
       "Compile ARR_ASSERT to a no-op (defines ARRANGRR_NO_ASSERT); firmware release builds only, dev arm preset stays asserting" OFF)

# Phase-1 runtime extraction (docs/design/orchestrator-pipeline-extraction.md):
# components/common (the base, dependency-free layer) and components/runtime
# (the dual-target kernel: Transport + OutScheduler + the Stage port) are
# unconditional — BOTH the arm-none-eabi branch and the host branch need them.
add_subdirectory(components/core/common)
add_subdirectory(components/core/runtime)
# Phase-4b promotion (§16.1/§16.8): components/core/chorddet (ChordDetector +
# FollowedContext + the chorddet Seam-C stage) is ALSO unconditional and
# dual-target -- arrangrr now depends on it for these types (D43: the
# dependency runs arrangrr -> chorddet, chorddet never depends on arrangrr).
add_subdirectory(components/core/chorddet)
add_subdirectory(components/core/arrangrr)
# `0910`/D43 + docs/proposals/isoundengine-contract.md (Corelli): ISoundEngine
# is the render/synthesis seam a future host-only audio engine talks to the
# rest of the system through -- a pure-virtual, POD-only-signature interface
# (dispatch/all_notes_off/render/name), core-capable NOW even though every
# concrete implementation today is host-only. Unconditional, alongside the
# other three core components above, because nothing in the contract itself
# needs a hosted OS; tests/arm-smoke/null_sound_engine_check.cpp compiles a
# trivial ISoundEngine implementation on arm-none-eabi so this claim is a
# CI-verified fact, not an aspirational one.
add_subdirectory(components/core/audio_engine)

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
  add_subdirectory(components/platform/midisrc)
  add_subdirectory(components/platform/orchestrator)
  add_subdirectory(components/platform/hostrt)
  add_subdirectory(apps/tools/cli-arrangrr)
  # Phase 2a (docs/design/orchestrator-pipeline-extraction.md §15 /
  # docs/design/sonotron-server-phase2-brief.md): the headless server binary,
  # a thin main() around the SAME hostrt Shell/Runtime/arrangrr composition
  # cli-arrangrr already links -- additive, cli-arrangrr is untouched.
  add_subdirectory(apps/sonotron-server)
  add_subdirectory(components/core/arrangrr/tests)
  add_subdirectory(components/core/runtime/tests)
  add_subdirectory(components/platform/hostrt/tests)
  add_subdirectory(tests/golden)
  add_subdirectory(tests/integration)
  # Host-only importer/compiler tooling (never in the ARM/firmware build).
  add_subdirectory(apps/tools/arrstyle-converter)
  # Host-only GUI pure client (D38) + its vendored toolkit. Never in the
  # ARM/firmware build; the GUI never links arrangrr or hostrt directly --
  # only gui_sonotron_engine does, see apps/gui-sonotron/CMakeLists.txt.
  add_subdirectory(third_party)
  # Phase-5 Item F (docs/design/phase5-execution-plan.md, `0910`/D43): the
  # melodd GM SoundFont realizer component. HOST-ONLY, links
  # third_party/tinysoundfont (just entered above) -- never on the
  # arm-none-eabi branch above, same placement discipline as
  # components/midisrc / components/orchestrator. Ordered BEFORE
  # components/platform/audio (Phase-6 Theme 2, docs/phase6-design-reviews.md
  # "Audio in the standalone GUI"): the `audio` component's SoundfontEngine
  # links `melodd` directly, so the component target must exist first (CMake
  # target_link_libraries requires the target to already be defined).
  add_subdirectory(components/platform/engines/melodd)
  # The audio device layer + ISoundEngine's first concrete implementation
  # (docs/proposals/isoundengine-contract.md, docs/proposals/
  # components-restructure-move-plan.md §2/§3 Step 5), promoted OUT of
  # apps/gui-sonotron. HOST-ONLY, ordered BEFORE apps/gui-sonotron: the GUI
  # binary links both `audio_ring` (gui_sonotron_engine) and `audio`
  # (gui_sonotron) directly.
  add_subdirectory(components/platform/audio)
  add_subdirectory(apps/gui-sonotron)
  # The melodd standalone drop-in-synth binary; unaffected by the reorder
  # above (it only needs components/melodd + third_party, both already
  # added).
  add_subdirectory(apps/tools/melodd)
endif()

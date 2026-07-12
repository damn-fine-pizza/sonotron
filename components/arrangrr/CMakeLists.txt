add_library(arrangrr STATIC
  src/version.cpp
  src/common_checks.cpp
  src/engine.cpp
)

target_include_directories(arrangrr PUBLIC include)

# Phase-1 runtime extraction (docs/design/orchestrator-pipeline-extraction.md
# Decision A / §14.5 point 3): arrangrr's own headers use the promoted
# Tick/MidiMessage/ARR_ASSERT primitives (now physically in components/common)
# pervasively, and the reduced Engine implements the Stage port declared in
# runtime/stage.hpp — hence PUBLIC links to BOTH. This ends arrangrr's
# previous "zero external target_link_libraries" property; the direction
# stays sanctioned (arrangrr -> runtime for the Stage port only; runtime never
# links back to arrangrr — that reverse edge is exactly the cycle Decision A
# exists to kill).
#
# Phase-4b promotion (§16.1/§16.2c/§16.8): ChordDetector/FollowedContext (and
# the theory vocabulary they need) moved to components/chorddet -- arrangrr's
# chord/arranger headers now depend on it too. The direction runs ONE way
# (arrangrr -> chorddet, PUBLIC); chorddet never links back to arrangrr (D43).
target_link_libraries(arrangrr PUBLIC common runtime chorddet)

# The whole core (and everything linking it) runs without exceptions/RTTI —
# same subset on host and firmware (D3, D32).
target_compile_options(arrangrr PUBLIC
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)

# PUBLIC, not PRIVATE: the core is ~100% headers — its real code compiles
# inside dependent TUs, and warnings that stop at the two ceremony files
# never see it (review finding).
target_compile_options(arrangrr PUBLIC
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
  -Werror
)

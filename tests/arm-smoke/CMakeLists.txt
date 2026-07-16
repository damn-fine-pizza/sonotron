# Link gate for the cross build: a freestanding image that pulls in the
# runtime kernel + the arranger stage together (Phase-1 runtime extraction —
# link_gate.cpp used to be components/arrangrr/src/engine_checks.cpp).
add_executable(firmware_stub main.cpp link_gate.cpp)
target_link_libraries(firmware_stub PRIVATE arrangrr runtime)
set_target_properties(firmware_stub PROPERTIES SUFFIX ".elf")

# Compile-only gate for components/core/audio_engine (docs/proposals/
# isoundengine-contract.md §4, Corelli): an OBJECT library so this TU is
# built (as part of the default ALL target) on every arm cross build without
# needing to link into firmware_stub -- the interface is core-capable NOW,
# but no concrete engine exists on this target yet, so there is nothing real
# to link against.
add_library(null_sound_engine_check OBJECT null_sound_engine_check.cpp)
target_link_libraries(null_sound_engine_check PRIVATE audio_engine_core)

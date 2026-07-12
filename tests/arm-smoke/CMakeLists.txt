# Link gate for the cross build: a freestanding image that pulls in the
# runtime kernel + the arranger stage together (Phase-1 runtime extraction —
# link_gate.cpp used to be components/arrangrr/src/engine_checks.cpp).
add_executable(firmware_stub main.cpp link_gate.cpp)
target_link_libraries(firmware_stub PRIVATE arrangrr runtime)
set_target_properties(firmware_stub PROPERTIES SUFFIX ".elf")

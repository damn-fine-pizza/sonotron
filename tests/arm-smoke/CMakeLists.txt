# Link gate for the cross build: a freestanding image that pulls in the core.
add_executable(firmware_stub main.cpp)
target_link_libraries(firmware_stub PRIVATE arrangrr_core)
set_target_properties(firmware_stub PROPERTIES SUFFIX ".elf")

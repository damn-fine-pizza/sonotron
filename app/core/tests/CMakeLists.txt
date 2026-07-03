add_executable(test_common test_common.cpp)
target_link_libraries(test_common PRIVATE arrangrr_core)
add_test(NAME common COMMAND test_common)

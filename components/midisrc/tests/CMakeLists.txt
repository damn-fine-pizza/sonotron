# See components/arrangrr/tests/CMakeLists.txt's header comment for the
# three-metric label convention (unit/functional/regression).
function(midisrc_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE midisrc)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(${name}
    PRIVATE MIDISRC_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/")
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

midisrc_test(test_midi_source_stage unit)

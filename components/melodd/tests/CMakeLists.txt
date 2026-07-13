# See components/arrangrr/tests/CMakeLists.txt's header comment for the
# three-metric label convention (unit/functional/regression).
function(melodd_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE melodd)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

# Depends on a system GM SoundFont being installed (fluid-soundfont-gm) --
# SKIPs gracefully (exit 77) rather than failing when absent, same
# convention as tests/integration/CMakeLists.txt.
melodd_test(test_synth_smoke functional)
set_tests_properties(test_synth_smoke PROPERTIES SKIP_RETURN_CODE 77)

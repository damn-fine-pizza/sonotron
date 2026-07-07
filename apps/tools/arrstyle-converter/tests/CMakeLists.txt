# Tool-local unit tests, wired like the project's other tests (add_test + CTest).
# Three-metric coverage labels (same rule as components/arrangrr/tests): a test that drives
# a SINGLE importer/module against fixtures is `unit`; one that drives the whole
# converter CLI end-to-end (argparse -> import -> serialize -> output) is
# `functional`. These do not feed the core/platform gate (gcovr filters them
# out) but are labelled so `ctest -L <cat>` stays coherent project-wide.
function(arrstyle_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE arrstyle_lib)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(${name}
    PRIVATE ARRSTYLE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/")
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

arrstyle_test(test_json unit)
arrstyle_test(test_midi_import unit)
arrstyle_test(test_chordpro_import unit)
arrstyle_test(test_validate unit)
arrstyle_test(test_cli functional)
arrstyle_test(test_sff_import unit)
arrstyle_test(test_genre unit)
arrstyle_test(test_canon unit)

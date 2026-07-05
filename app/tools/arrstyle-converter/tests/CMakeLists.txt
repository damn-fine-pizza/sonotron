# Tool-local unit tests, wired like the project's other tests (add_test + CTest).
function(arrstyle_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE arrstyle_lib)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(${name}
    PRIVATE ARRSTYLE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

arrstyle_test(test_json)
arrstyle_test(test_midi_import)
arrstyle_test(test_chordpro_import)
arrstyle_test(test_validate)
arrstyle_test(test_cli)
arrstyle_test(test_sff_import)
arrstyle_test(test_genre)
arrstyle_test(test_canon)

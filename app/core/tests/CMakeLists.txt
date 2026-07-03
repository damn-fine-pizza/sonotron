function(arrangrr_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE arrangrr_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

arrangrr_test(test_common)
arrangrr_test(test_midi)
arrangrr_test(test_engine)

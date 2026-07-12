# See components/arrangrr/tests/CMakeLists.txt's header comment for the
# three-metric label convention (unit/functional/regression).
function(runtime_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE runtime arrangrr)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

# test_scheduler: subject is OutScheduler in isolation; also links arrangrr
# for its StaticVector<> test-fixture container (arrangrr/common/
# static_vector.hpp) — no arrangrr production type is exercised here.
add_executable(test_scheduler test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE runtime arrangrr)
add_test(NAME test_scheduler COMMAND test_scheduler)
set_tests_properties(test_scheduler PROPERTIES LABELS unit)

# test_engine_fire_order / test_runtime_engine: both instantiate the arrangrr
# Stage-adapter through TestEngine (a Runtime<Engine> wrapper), so they need
# both libs. The CMake TARGET is named test_runtime_engine (not test_engine,
# which would collide with components/arrangrr/tests' own test_engine target)
# even though the source file is test_engine.cpp, mirroring the file's
# arrangrr-side twin.
runtime_test(test_engine_fire_order functional)
add_executable(test_runtime_engine test_engine.cpp)
target_link_libraries(test_runtime_engine PRIVATE runtime arrangrr)
add_test(NAME test_runtime_engine COMMAND test_runtime_engine)
set_tests_properties(test_runtime_engine PROPERTIES LABELS functional)

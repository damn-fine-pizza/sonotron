# gui-sonotron layout unit/functional tests, wired like the project's other
# tests (add_test + CTest). Same three-metric coverage labels as
# components/arrangrr/tests and apps/tools/arrstyle-converter/tests: a test
# driving a single module is `unit`; one driving the load/save file path
# end-to-end is `functional`.
#
# Host-only (this whole subtree is added only in the non-firmware branch of
# the top-level CMakeLists, where enable_testing() is in effect). Links only
# gui_sonotron_layout (which itself links only the vendored ImGui) — no
# arrangrr_core, no hostrt (D38).
function(gui_sonotron_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_layout)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_test(test_layout_model unit)
gui_sonotron_test(test_layout_json unit)
gui_sonotron_test(test_layout_nested_split unit)
gui_sonotron_test(test_layout_roundtrip functional)

# Brain-session seam tests (node 11600, G2): link only gui_sonotron_brain (no
# ImGui, no GLFW) -- pure model + wire-parser tests, host-only.
function(gui_sonotron_brain_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_brain)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_brain_test(test_brain_event unit)
gui_sonotron_brain_test(test_app_state unit)
gui_sonotron_brain_test(test_uds_brain_session unit)

# Zone-model tests (G3, docs/design/gui-fase2-mechanical-plan.md): link only
# gui_sonotron_models (no ImGui, no GLFW) -- pure model tests, host-only.
function(gui_sonotron_model_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_models)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_model_test(test_browser_model unit)
gui_sonotron_model_test(test_grid_model unit)
gui_sonotron_model_test(test_seqedit_model unit)
gui_sonotron_model_test(test_parts_model unit)

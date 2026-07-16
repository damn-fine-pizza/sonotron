function(hostrt_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE hostrt)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

hostrt_test(test_host unit)
hostrt_test(test_panels unit)
hostrt_test(test_panel_nav functional)
hostrt_test(test_style_chooser unit)
hostrt_test(test_rc_config unit)
hostrt_test(test_param_state_wire unit)
hostrt_test(test_tui_client unit)
# Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
# Performance"): the `pad assign|trigger|release` / `perf store|recall|save|
# load` L1 grammar, including the host-only perf-file round trip -- the
# subject is the shell's own string<->Command integration, same precedent as
# test_host's own shell command coverage -> functional.
hostrt_test(test_pad_perf functional)
# Phase-6 Theme 3 Item #3 (P4, docs/reflections/phase6-theme3-performance-
# format-v2-review.md): the v1 -> v2 Performance migrator -- mostly pure unit
# coverage of migrate_performance_v1_to_v2 itself, plus one functional smoke
# test proving Shell::perf_load's fallback wiring actually fires -> unit
# (matches test_param_state_wire's own "mostly pure, one file I/O smoke"
# precedent, not functional, since the bulk of the coverage never touches
# Shell).
hostrt_test(test_perf_migrate unit)

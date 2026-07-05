# Three-metric coverage labels (see scripts/coverage.sh and project memory
# `coverage-metrics`). Every test carries exactly ONE CTest label, chosen by the
# SUBJECT-UNDER-TEST (not by whether a driver/harness is used):
#   unit       — the subject is a SINGLE production module. Direct class
#                instances or pure functions qualify; so does a producer that
#                can only be observed by running it under an Engine HARNESS
#                (e.g. Arp, Arranger) — there the Engine is scaffolding, the
#                module under test is still one unit.
#   functional — the subject is the INTEGRATION itself: the Engine's own
#                cross-producer coordination (dispatch, fire order), the
#                chord-follow selector arbitrating three producers, or the
#                PanelManager/Shell navigation loop.
#   regression — written to pin a specific FIXED bug (red-before / green-after).
# A file may cover lines both ways; the label reflects its PRIMARY intent (CTest
# labels are per-binary, not per-CHECK). test_arp / test_arranger are the pivot
# calls (unit here) and are discussed in the QA report so the owner can flip
# them — doing so drops metric-1 branch coverage below the 80% gate.
function(arrangrr_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE arrangrr_core)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

arrangrr_test(test_common unit)
arrangrr_test(test_midi unit)
arrangrr_test(test_engine functional)
arrangrr_test(test_engine_fire_order functional)
arrangrr_test(test_timeline unit)
# test_step_locks pins the fixed sustain-chain lone-tie bug (a lone tie was
# extended by exactly one step): red-before / green-after — hence `regression`.
# It also carries Timeline parameter-lock coverage; regression is the primary
# intent per the three-metric plan.
arrangrr_test(test_step_locks regression)
arrangrr_test(test_chord unit)
arrangrr_test(test_chord_seq unit)
arrangrr_test(test_chord_detect unit)
arrangrr_test(test_chord_follow functional)
# test_arp / test_arranger: subject is ONE producer (Arp / Arranger); the Engine
# is only the harness that lets it emit over ticks -> unit (see header note).
arrangrr_test(test_arp unit)
arrangrr_test(test_arranger unit)
arrangrr_test(test_scheduler unit)
arrangrr_test(test_gesture unit)
arrangrr_test(test_voicing unit)

arrangrr_test(test_host unit)
target_link_libraries(test_host PRIVATE arrangrr_host)

arrangrr_test(test_panels unit)
target_link_libraries(test_panels PRIVATE arrangrr_host)

arrangrr_test(test_panel_nav functional)
target_link_libraries(test_panel_nav PRIVATE arrangrr_host)

arrangrr_test(test_style_chooser unit)
target_link_libraries(test_style_chooser PRIVATE arrangrr_host)

arrangrr_test(test_rc_config unit)
target_link_libraries(test_rc_config PRIVATE arrangrr_host)

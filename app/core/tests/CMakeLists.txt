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
# test_followed_context: the FollowedContext owner (explicit latch, D47 gate,
# stage/commit_now/commit_bar, establish_default no-op-if-explicit) plus the
# release-phantom regression and the immediate-default/shift-quantize input
# model. Supersedes the old test_chord_quantize (which pinned the removed D53
# quantized-default primitives). Torquato re-ratifies any further D53->immediate
# expectations here.
arrangrr_test(test_followed_context unit)
# test_chord_context_diagnosis: Torquato QA diagnosis of two user-reported
# chord-context bugs (idle-playback drift with zero input; a section/style
# switch resetting a steered chord). Subject is the Engine's own cross-tick
# and cross-command coordination of the followed chord context -> functional.
# Currently RED where it pins an open, unfixed defect (see the printed
# timeline and the CHECK comments in the file); do not silence it.
arrangrr_test(test_chord_context_diagnosis functional)
# test_chord_seq_vs_live_steer: the LIVE-PRIORITY arbitration (engine
# default) between fire_chord_seq and a live steer through the FollowedContext
# owner. Locks: a held live chord beats a running sequencer (band follows live,
# the seq comps ON the live chord), release resumes the sequencer, no-seq
# latches (Strada 1), the `chord follow sequencer` override, and kAuto's kept
# legacy last-writer clobber. Subject is the Engine's cross-tick arbitration ->
# functional.
arrangrr_test(test_chord_seq_vs_live_steer functional)
# test_input_zone: the Engine's per-port InputZone route-vs-suppress split and
# the detector's scale-aware single-finger steering, driven through the ABI ->
# the subject is the integration, so `functional`.
arrangrr_test(test_input_zone functional)
arrangrr_test(test_harmony_steer functional)
# test_arp / test_arranger: subject is ONE producer (Arp / Arranger); the Engine
# is only the harness that lets it emit over ticks -> unit (see header note).
arrangrr_test(test_arp unit)
# test_arp_functional drives the FULL Engine (push_command/push_midi_in/advance)
# for no-stuck-note invariants across transport stop and retrigger -> the subject
# is the arp's integration with transport/scheduler, so `functional`.
arrangrr_test(test_arp_functional functional)
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

# Integration tests exercise the CLI binary against the OS (ALSA / PTY / socket)
# end-to-end -> `functional` in the three-metric split. They may SKIP (77) when
# the environment is unavailable.
add_test(NAME live_alsa
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/live_alsa.sh $<TARGET_FILE:arrangrr_cli>)
set_tests_properties(live_alsa PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

add_test(NAME live_tracks
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/live_tracks.sh $<TARGET_FILE:arrangrr_cli>)
set_tests_properties(live_tracks PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

add_test(NAME tui_console
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tui_console.sh $<TARGET_FILE:arrangrr_cli>)
set_tests_properties(tui_console PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

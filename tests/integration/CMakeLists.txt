# Integration tests exercise the CLI binary against the OS (ALSA / PTY / socket)
# end-to-end -> `functional` in the three-metric split. They may SKIP (77) when
# the environment is unavailable.
add_test(NAME live_alsa
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/live_alsa.sh $<TARGET_FILE:cli_arrangrr>)
set_tests_properties(live_alsa PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

add_test(NAME live_tracks
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/live_tracks.sh $<TARGET_FILE:cli_arrangrr>)
set_tests_properties(live_tracks PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

add_test(NAME tui_console
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tui_console.sh $<TARGET_FILE:cli_arrangrr>)
set_tests_properties(tui_console PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

# Phase 2a (docs/design/orchestrator-pipeline-extraction.md §15 gate 2): a
# client round-trips one L1 line over sonotron-server's --control UDS socket.
add_test(NAME server_socket
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/server_socket.sh $<TARGET_FILE:sonotron_server>)
set_tests_properties(server_socket PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

# Phase 3 (docs/design/orchestrator-pipeline-extraction.md §17): a pure
# `cli-arrangrr --connect` client exchanges a note + a groove tweak with a
# live sonotron-server session and folds the kParamState echo into its own
# mirror.
add_test(NAME client_session
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/client_session.sh
          $<TARGET_FILE:sonotron_server> $<TARGET_FILE:cli_arrangrr>)
set_tests_properties(client_session PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS functional)

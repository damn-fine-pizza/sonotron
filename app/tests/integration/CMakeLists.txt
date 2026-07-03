add_test(NAME live_alsa
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/live_alsa.sh $<TARGET_FILE:arrangrr_cli>)
set_tests_properties(live_alsa PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)

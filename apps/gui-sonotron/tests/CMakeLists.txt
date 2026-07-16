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
gui_sonotron_test(test_layout_schema_upgrade functional)

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

# Engine-linking tests (Phase 2b): link gui_sonotron_engine, which pulls in
# hostrt/runtime/arrangrr/ALSA transitively -- D38 is retired for this
# milestone (see apps/gui-sonotron/CMakeLists.txt's gui_sonotron_engine
# comment). test_in_process_brain_session is the Gate-2b round-trip smoke
# (push a Command onto the ring -> engine tick -> drain an OutEvent -> decode
# -> expected state), requires no display and no ALSA sequencer device.
function(gui_sonotron_engine_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_engine)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_engine_test(test_brain_event_from_outevent unit)
gui_sonotron_engine_test(test_in_process_brain_session functional)

# `midi-source load <path>` round trip (GUI integrated-mode refinement):
# reuses the already-committed shared fixture `components/midisrc/tests/
# fixtures/tiny.mid` (also used by midisrc's/arrstyle-converter's own tests
# and tests/golden's accompany_basic golden) rather than adding a new binary
# asset under this tree.
target_compile_definitions(test_in_process_brain_session PRIVATE
  GUI_SONOTRON_TEST_MIDI_FIXTURE="${CMAKE_SOURCE_DIR}/components/platform/midisrc/tests/fixtures/tiny.mid")

# Phase-6 Theme 2 QA pass (docs/phase6-design-reviews.md "Audio in the
# standalone GUI", Decision 3/4): pins the negative half of the OutEvent ->
# AudioMidiEvent port filter (kPrimaryAudioOutPort == 0) -- non-primary-port
# kMidi traffic (midi-source replay, fixed port 1) must reach poll() but
# never the audio ring. See this file's own header comment for why the
# positive case (primary-port traffic reaching the ring) is NOT exercised
# here.
gui_sonotron_engine_test(test_audio_port_gate functional)
target_compile_definitions(test_audio_port_gate PRIVATE
  GUI_SONOTRON_TEST_MIDI_FIXTURE="${CMAKE_SOURCE_DIR}/components/platform/midisrc/tests/fixtures/tiny.mid")

# Flips Torquato's RED-by-design QA pin to GREEN (Nazzareno, closing the
# Theme-2 reachability gap): InProcessBrainSession::send() now auto-routes
# the default band on `style load` (kDefaultStyleRoutes,
# in_process_brain_session.cpp), so primary-port kMidi traffic IS reachable
# now -- a normal, registered, always-green test like every other one here.
gui_sonotron_engine_test(test_audio_primary_port_reachable functional)

# Device-independent AudioBackend/SoundfontEngine smoke test (Torquato QA
# pass): moved to components/platform/audio/tests (docs/proposals/
# components-restructure-move-plan.md §2/§3 Step 5, docs/proposals/
# isoundengine-contract.md) -- it links the `audio` component directly, not
# gui_sonotron_engine, and no longer belongs in this file.

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

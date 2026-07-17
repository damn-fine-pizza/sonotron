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

# Torquato QA pass (live-bug repro, real backend, no synthetic kBeat
# injection): drives a REAL InProcessBrainSession across several REAL
# wall-clock seconds and asserts AppState::bar() actually climbs -- see this
# test file's own header comment for why this is a different seam than
# test_grid_panel_auto_song.cpp's SpyBrainSession-based render/decision test.
gui_sonotron_engine_test(test_in_process_brain_session_bar_advance functional)

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

# scenes.json persistence round-trip (repeat-zone-real-contract.md §4/§8b
# decision 3) -- a functional test in the same load/save/temp-dir shape as
# test_layout_schema_upgrade.cpp, one directory over for its own file.
gui_sonotron_model_test(test_scenes_json functional)

# Real-content cell preview (repeat-zone-real-contract.md, "cell preview made
# real" pass): link gui_sonotron_preview directly, which pulls in arrangrr
# transitively (see apps/gui-sonotron/CMakeLists.txt's gui_sonotron_preview
# target comment) -- same shape as gui_sonotron_engine_test above, a second
# library allowed to touch the core.
function(gui_sonotron_preview_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_preview)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_preview_test(test_preview unit)

# Repeat-Zone auto-song functional/integration test (Torquato QA pass, red-
# before-green bug repro): drives the REAL render_grid_panel entry point
# (grid_panel.hpp) inside a headless ImGui context (no GPU, no window, no
# backend -- font atlas self-builds on first NewFrame()), because the
# reported bug lives in grid_panel.cpp's file-local (anonymous-namespace)
# update_auto_song, not in the already-green pure next_scene_to_launch() unit
# (test_grid_model.cpp). Links gui_sonotron_layout (render_grid_panel + imgui
# + gui_sonotron_preview transitively) AND gui_sonotron_brain (AppState::
# apply/apply_line's out-of-line implementation, needed to feed realistic
# "beat" events) -- the only test in this directory that needs both.
function(gui_sonotron_grid_panel_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_layout gui_sonotron_brain)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

gui_sonotron_grid_panel_test(test_grid_panel_auto_song functional)

# FIX PIN (Torquato QA, root-caused via a live SONOTRON_AUTOSONG_TRACE stderr
# trace, since removed): auto-song's advance branch (update_auto_song,
# grid_panel.cpp) advances fx.active_scene and re-sends `style section
# <name>`, AND now also sends `launch scene <n> quantize <q>` for the
# newly-active column -- exactly like the manual scene-header launch already
# did. This test proves the fix stays fixed; do not weaken or remove this
# assertion.
gui_sonotron_grid_panel_test(test_grid_panel_auto_song_launches_next_scene functional)

# Torquato QA pass, UI-AUTOMATING flavor of the live-bug repro (owner steer):
# the same headless-ImGui render loop as test_grid_panel_auto_song.cpp above,
# but wired end-to-end to a REAL InProcessBrainSession (real engine thread,
# real Play verb, real poll/apply/render pipeline) instead of that test's
# SpyBrainSession + hand-injected beat JSONL -- see this test file's own
# header comment. Needs gui_sonotron_engine on top of the grid-panel test's
# usual layout+brain link set (the only test in this directory that needs
# all three).
add_executable(test_grid_panel_auto_song_real_backend test_grid_panel_auto_song_real_backend.cpp)
target_link_libraries(test_grid_panel_auto_song_real_backend PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_panel_auto_song_real_backend PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_grid_panel_auto_song_real_backend COMMAND test_grid_panel_auto_song_real_backend)
set_tests_properties(test_grid_panel_auto_song_real_backend PROPERTIES LABELS functional)

# ---------------------------------------------------------------------------
# UI-AUTOMATION HARNESS (Torquato QA pass, owner steer): the missing piece
# every test above this line does NOT have -- REAL ImGui input injection
# (io.AddMousePosEvent/AddMouseButtonEvent, never a direct V02State/AppState
# field write) and assertions on the actual RENDERED DRAW DATA (ImGui::
# Render()'s ImDrawData), not just on model state. See imgui_headless_
# harness.hpp's own header comment for the full design (widget-rect
# discovery via rendered-color-vertex scanning + ImGui's own child-window
# list, no IMGUI_ENABLE_TEST_ENGINE, no product change) and each test file's
# own header comment for the specific owner-reported bug it pins.

# Bug #1 (no Repeat-Zone playhead): real backend, real Play-button click,
# real cell-launch click, rendered-draw-data assertion. Needs the same three
# libraries as test_grid_panel_auto_song_real_backend above (a real
# InProcessBrainSession) plus gui_sonotron_preview transitively (already
# pulled in by gui_sonotron_layout) for theme.hpp/neon_widgets.hpp's public
# color constants this harness locates widgets by.
add_executable(test_repeat_zone_playhead_ui_automation test_repeat_zone_playhead_ui_automation.cpp)
target_link_libraries(test_repeat_zone_playhead_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_repeat_zone_playhead_ui_automation PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_repeat_zone_playhead_ui_automation COMMAND test_repeat_zone_playhead_ui_automation)
set_tests_properties(test_repeat_zone_playhead_ui_automation PROPERTIES LABELS functional)

# Bug #2 (launch-cell preview diverges from Sequence Edit): no real backend
# needed (a SpyBrainSession is enough -- this pin is purely about RENDERED
# OUTPUT, never what gets sent), so this uses the lighter grid_panel_test
# link set (gui_sonotron_layout + gui_sonotron_brain), same group as
# test_grid_panel_auto_song above.
gui_sonotron_grid_panel_test(test_grid_cell_preview_vs_seqedit_ui_automation functional)

# Owner-recorded-trace corroboration (owner steer, 2026-07-16 live session):
# replays fixtures/owner_repro_2026_07_16.jsonl -- captured with the
# product-side --trace-input feature against the exact live session where
# the owner saw the playhead/preview bugs -- through imgui_headless_
# harness.hpp's JSONL loader, tick-by-tick against a REAL InProcessBrainSession.
# See this test file's own header comment for the honest divergence from a
# pixel-faithful replay (the trace's own coordinates were captured against
# main.cpp's full 6-zone layout, not this reduced 2-panel test window).
add_executable(test_repeat_zone_owner_trace_replay test_repeat_zone_owner_trace_replay.cpp)
target_link_libraries(test_repeat_zone_owner_trace_replay PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_repeat_zone_owner_trace_replay PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(test_repeat_zone_owner_trace_replay PRIVATE
  GUI_SONOTRON_OWNER_TRACE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/owner_repro_2026_07_16.jsonl")
add_test(NAME test_repeat_zone_owner_trace_replay COMMAND test_repeat_zone_owner_trace_replay)
set_tests_properties(test_repeat_zone_owner_trace_replay PROPERTIES LABELS functional)

# ---------------------------------------------------------------------------
# Torquato QA pass (docs/proposals/flow-verification-matrix-2026-07.md-driven
# expansion, 2026-07): 6 more flows off the matrix's own "hand to Torquato"
# list, all real click injection + real InProcessBrainSession + real
# rendered-output/engine readback -- see each file's own header comment for
# the matrix row it closes and (for the next-bar test) the task #19 root-
# cause trace. All link the same three-library set as the UI-automation
# tests above (a real backend + the real render loop).

# §2a "Press Play"/"Press Stop": both transport pad clicks, real backend.
add_executable(test_transport_play_stop_ui_automation test_transport_play_stop_ui_automation.cpp)
target_link_libraries(test_transport_play_stop_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_transport_play_stop_ui_automation PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_transport_play_stop_ui_automation COMMAND test_transport_play_stop_ui_automation)
set_tests_properties(test_transport_play_stop_ui_automation PROPERTIES LABELS functional)

# §2c "Click scene-header ▶" -- TASK #19 PIN: a manual scene-header click
# while playing must flip the real section/clip readback within one bar, not
# after GridModel::kDefaultSceneBars (8) bars ("waiting until the cell
# ends"). See this file's own header comment for the full root-cause trace.
add_executable(test_repeat_zone_scene_header_next_bar_ui_automation
  test_repeat_zone_scene_header_next_bar_ui_automation.cpp)
target_link_libraries(test_repeat_zone_scene_header_next_bar_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_repeat_zone_scene_header_next_bar_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_repeat_zone_scene_header_next_bar_ui_automation
  COMMAND test_repeat_zone_scene_header_next_bar_ui_automation)
set_tests_properties(test_repeat_zone_scene_header_next_bar_ui_automation PROPERTIES
  LABELS functional)

# §2c "Click a FILLED cell -> real launch + opens Sequence Edit".
add_executable(test_grid_cell_launch_open_seqedit_ui_automation
  test_grid_cell_launch_open_seqedit_ui_automation.cpp)
target_link_libraries(test_grid_cell_launch_open_seqedit_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_cell_launch_open_seqedit_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_grid_cell_launch_open_seqedit_ui_automation
  COMMAND test_grid_cell_launch_open_seqedit_ui_automation)
set_tests_properties(test_grid_cell_launch_open_seqedit_ui_automation PROPERTIES LABELS functional)

# §2c "M/S latch click -> real part mute|solo + PartsModel toggle".
add_executable(test_grid_ms_latch_ui_automation test_grid_ms_latch_ui_automation.cpp)
target_link_libraries(test_grid_ms_latch_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_ms_latch_ui_automation PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_grid_ms_latch_ui_automation COMMAND test_grid_ms_latch_ui_automation)
set_tests_properties(test_grid_ms_latch_ui_automation PROPERTIES LABELS functional)

# §2b "Click a style leaf while playing -> style switch (live morph)".
add_executable(test_browser_style_switch_while_playing_ui_automation
  test_browser_style_switch_while_playing_ui_automation.cpp)
target_link_libraries(test_browser_style_switch_while_playing_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_browser_style_switch_while_playing_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_browser_style_switch_while_playing_ui_automation
  COMMAND test_browser_style_switch_while_playing_ui_automation)
set_tests_properties(test_browser_style_switch_while_playing_ui_automation PROPERTIES
  LABELS functional)

# §2c "Auto-song bookkeeping surviving a transport stop/restart" -- extends
# test_grid_panel_auto_song_real_backend.cpp's happy-path shape with a real
# Stop/Play click cycle (real backend, real render loop).
add_executable(test_grid_panel_auto_song_stop_restart_ui_automation
  test_grid_panel_auto_song_stop_restart_ui_automation.cpp)
target_link_libraries(test_grid_panel_auto_song_stop_restart_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_panel_auto_song_stop_restart_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_grid_panel_auto_song_stop_restart_ui_automation
  COMMAND test_grid_panel_auto_song_stop_restart_ui_automation)
set_tests_properties(test_grid_panel_auto_song_stop_restart_ui_automation PROPERTIES
  LABELS functional)

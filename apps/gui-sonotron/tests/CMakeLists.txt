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

# A/B FIX PIN (docs/reflections/cli-vs-gui-ab-2026-07.md): proves the GUI's
# own reachable path (`style load`/`transport start`, nothing hand-written)
# now produces real harmonic movement instead of a static tonic triad --
# extended (Torquato QA pass, 2026-07-17, issue #29 / commit 7f07a6f) to
# cover all 16 built-in styles (one deliberate exception: funk's single-step
# vamp) -- see this test file's own header comment.
gui_sonotron_engine_test(test_default_style_progression_functional functional)

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

# ---------------------------------------------------------------------------
# PREVIEW ORACLE (Torquato QA pass, owner task #4: debug/oracle tooling for
# the class of bug where the Sequence-Edit/cell preview disagrees with what
# the engine actually plays). preview_oracle.cpp calls the REAL arrangrr
# kernel functions (Style::find/Arranger::resolve/motif::apply_repeat) plus a
# REAL InProcessBrainSession capture -- see preview_oracle.hpp's own header
# comment for the full two-halves design. Links arrangrr directly (ground
# truth) and gui_sonotron_engine (the real-capture half, InProcessBrainSession
# + BrainEvent).
add_library(gui_sonotron_preview_oracle STATIC preview_oracle.cpp)
target_link_libraries(gui_sonotron_preview_oracle PUBLIC arrangrr gui_sonotron_engine)
target_compile_options(gui_sonotron_preview_oracle PRIVATE -Wall -Wextra -Werror)

# RED tests pinning preview::preview_for()'s three confirmed fidelity gaps
# against the oracle above -- link gui_sonotron_preview (the code under test)
# + gui_sonotron_preview_oracle (the ground truth) + gui_sonotron_brain (the
# BrainEvent/BrainSession types the oracle's real-capture half returns).
function(gui_sonotron_preview_oracle_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_preview gui_sonotron_preview_oracle
    gui_sonotron_brain)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

# Oracle-backed acceptance test for all three of preview_for()'s originally-
# confirmed fidelity gaps (owner task #4): multi-bar truncation (bug #1,
# basic/kVarA/kBass), the placeholder-harmony divergence (bug #3, rock/kVarA/
# kBass vs the style's own real default progression), and the motif-repeat
# approximation (bug #2, both the underlying motif-kernel ground truth on
# basic/kVarD/kChord1 and its actual reachability on pop/kVarA/kChord2, a
# genuinely two-bar motif-driven pattern). Originally authored RED-before-
# green against the pre-fix preview_for(); Nazzareno's SLICE 1 (multi-bar
# window + per-bar real progression harmony + repeat=bar motif approximation)
# has since landed, so this is now the GREEN acceptance gate for that fix --
# see the test file's own header comment for the full design and the one
# honestly-documented caveat (pop's chord2 role goes silent in bar 2 at REAL
# playback, an engine-level motif-kernel constraint unrelated to preview; the
# preview's own approximation there is deliberate and owner-approved, not a
# residual bug). Kept as a standing regression test, not deleted.
gui_sonotron_preview_oracle_test(test_preview_multibar_truncation_red functional)

# Grid-panel functional/integration test helper: drives the REAL render_
# grid_panel entry point (grid_panel.hpp) inside a headless ImGui context (no
# GPU, no window, no backend -- font atlas self-builds on first NewFrame()),
# for behaviors that live in grid_panel.cpp's own file-local (anonymous-
# namespace) logic, not in an already-green pure unit (test_grid_model.cpp).
# Links gui_sonotron_layout (render_grid_panel + imgui + gui_sonotron_preview
# transitively) AND gui_sonotron_brain (AppState::apply/apply_line's
# out-of-line implementation, needed to feed realistic "beat" events).
function(gui_sonotron_grid_panel_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gui_sonotron_layout gui_sonotron_brain)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

# test_grid_panel_auto_song.cpp RETIRED (Torquato QA, song-mode Phase 1
# migration pass, docs/proposals/song-mode-scenechain-adoption.md): all 4 of
# its SpyBrainSession-based tests pinned the OLD per-frame update_auto_song
# FSM, which Phase 1 retired wholesale in favor of the core-driven
# SceneChain (reached only from the engine thread -- SpyBrainSession, which
# never drives a real Engine, has nothing real left to observe). Disposition
# of each, individually judged:
#   - test_auto_song_advances_scene_after_section_elapses: RETIRED, superseded
#     by test_grid_panel_auto_song_real_backend.cpp, test_grid_scene_length_
#     stepper_ui_automation.cpp, and test_song_mode_scenechain_contract.cpp
#     (all real-backend, real-timing equivalents of "advance after N bars").
#   - test_auto_song_stuck_after_transport_stop_then_restart: MIGRATED to
#     test_grid_panel_auto_song_real_backend.cpp's own test_auto_song_
#     advances_correctly_after_transport_stop_then_restart (real backend,
#     real stop/restart cycle).
#   - test_scene_bars_governs_advance_cadence: RETIRED, superseded by
#     test_grid_scene_length_stepper_ui_automation.cpp's own test 2 (proves
#     the stepper genuinely gates the real-backend advance timing).
#   - test_auto_song_armed_by_default_advances_without_manual_toggle:
#     RETIRED, superseded by test_grid_panel_auto_song_ends_on_ending_ui_
#     automation.cpp and test_grid_panel_auto_song_real_backend.cpp, neither
#     of which ever toggles fx.auto_song by hand (it defaults true).
# See this pass's own QA report for the full per-test rationale.

# test_grid_panel_auto_song_launches_next_scene.cpp RETIRED (Torquato QA,
# song-mode Phase 1 migration pass, docs/proposals/song-mode-scenechain-
# adoption.md): it pinned the OLD double-trigger fix ("auto-song's advance
# must ALSO send `launch scene <n> quantize <q>`" on top of `style section`),
# which design decision 4 of the Phase 1 proposal explicitly and permanently
# retired -- SceneChain never touches ClipMatrix, so no `launch scene` is
# ever sent for an auto-song advance again. The PERMANENT OPPOSITE assertion
# (`launch scene ` must NEVER appear) now lives in
# test_song_mode_scenechain_contract.cpp; see that file's own retirement note.
#
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
# (io.AddMousePosEvent/AddMouseButtonEvent, never a direct UiState/AppState
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

# Roadmap task #36 FIX PIN (Torquato QA pass, owner-observed LIVE bug):
# master Play pressed a second time (real Stop then real Play again) while
# the song sits mid-way must restart from scene 0, not re-launch the stale
# active_scene. Authored RED-first against the pre-fix tree; Nazzareno's fix
# (grid_panel.cpp, gated on fx.auto_song) landed concurrently while this test
# was being written -- both tests in this file are now the GREEN standing
# regression proof (root cause, RED trail, and the auto_song-OFF scope-lock
# companion test are all in this file's own header comment).
add_executable(test_grid_panel_master_play_restart_rewind_ui_automation
  test_grid_panel_master_play_restart_rewind_ui_automation.cpp)
target_link_libraries(test_grid_panel_master_play_restart_rewind_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_panel_master_play_restart_rewind_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_grid_panel_master_play_restart_rewind_ui_automation
  COMMAND test_grid_panel_master_play_restart_rewind_ui_automation)
set_tests_properties(test_grid_panel_master_play_restart_rewind_ui_automation PROPERTIES
  LABELS functional)

# Roadmap task #37 ACCEPTANCE TEST (Torquato QA pass, docs/proposals/song-
# form-option-a-wiring-plan.md §3/§4): a dedicated ENDING transport button.
# RED-first, per this file's own header comment -- the button does not exist
# in transport_panel.cpp yet at authoring time (Nazzareno implementing it in
# parallel, in the same shared tree); this suite's own locate step is the RED
# pin until his code lands.
add_executable(test_transport_ending_button_ui_automation
  test_transport_ending_button_ui_automation.cpp)
target_link_libraries(test_transport_ending_button_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_transport_ending_button_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_transport_ending_button_ui_automation
  COMMAND test_transport_ending_button_ui_automation)
set_tests_properties(test_transport_ending_button_ui_automation PROPERTIES
  LABELS functional)

# Song-form Option A ACCEPTANCE TEST (tasks #27/#12, docs/proposals/song-
# form-option-a-wiring-plan.md §2): the automatic counterpart to the manual
# ENDING-pad click test above -- proves the song holds at its last authored
# column (never wraps) and auto-song itself cues the Ending, with no click
# on the ENDING pad at all.
add_executable(test_grid_panel_auto_song_ends_on_ending_ui_automation
  test_grid_panel_auto_song_ends_on_ending_ui_automation.cpp)
target_link_libraries(test_grid_panel_auto_song_ends_on_ending_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_panel_auto_song_ends_on_ending_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_grid_panel_auto_song_ends_on_ending_ui_automation
  COMMAND test_grid_panel_auto_song_ends_on_ending_ui_automation)
set_tests_properties(test_grid_panel_auto_song_ends_on_ending_ui_automation PROPERTIES
  LABELS functional)

# Song-mode Phase 1 OBSERVABLE-CONTRACT hardening (Torquato QA pass,
# docs/proposals/song-mode-scenechain-adoption.md's own "Observable contract"
# section): no launch-scene/style-section double-send anywhere in an
# auto-song run, monotonic scene order, and harmony continuity across a
# scene boundary.
add_executable(test_song_mode_scenechain_contract
  test_song_mode_scenechain_contract.cpp)
target_link_libraries(test_song_mode_scenechain_contract PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_song_mode_scenechain_contract PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_song_mode_scenechain_contract
  COMMAND test_song_mode_scenechain_contract)
set_tests_properties(test_song_mode_scenechain_contract PROPERTIES
  LABELS functional)

# Task #6 ACCEPTANCE TEST: the always-visible per-scene LENGTH stepper in
# every scene-header column -- real click injection changes GridModel::
# scene_bars, and (via a real InProcessBrainSession) a dialed-down scene
# length genuinely gates auto-song's own advance timing, not a vestigial
# display number.
add_executable(test_grid_scene_length_stepper_ui_automation
  test_grid_scene_length_stepper_ui_automation.cpp)
target_link_libraries(test_grid_scene_length_stepper_ui_automation PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine)
target_compile_options(test_grid_scene_length_stepper_ui_automation PRIVATE
  -Wall -Wextra -Werror)
add_test(NAME test_grid_scene_length_stepper_ui_automation
  COMMAND test_grid_scene_length_stepper_ui_automation)
set_tests_properties(test_grid_scene_length_stepper_ui_automation PROPERTIES
  LABELS functional)

# Host layer as a library so unit tests can link shell/jsonl directly.
add_library(hostrt STATIC
  shell.cpp
  shell_parse.cpp
  shell_io_commands.cpp
  shell_music_commands.cpp
  shell_clip_commands.cpp
  shell_loop_commands.cpp
  shell_pad_commands.cpp
  shell_fx_commands.cpp
  perf_v1_migrate.cpp
  shell_input.cpp
  shell_view.cpp
  shell_chooser.cpp
  jsonl.cpp
  param_state_wire.cpp
  param_state_mirror.cpp
  event_labels.cpp
  midi_hal_factory.cpp
  console.cpp
  kitty_keys.cpp
  note_names.cpp
  gm_program.cpp
  panel_manager.cpp
  piano_view.cpp
  parts_view.cpp
  groove_view.cpp
  arp_view.cpp
  midi_monitor.cpp
  ui_style.cpp
  style_chooser.cpp
  rc_config.cpp
  uds_server.cpp
  uds_client.cpp
  client_event.cpp
  tui_client.cpp
)
target_include_directories(hostrt PUBLIC .)
# Phase 4d (docs/design/orchestrator-pipeline-extraction.md §16.3/§16.7):
# `hostrt::Shell` drives `orchestrator::AccompanyPipeline` (the same 3-stage
# `[MIDI-source, chorddet, arrangrr]` shape, host-only via the MIDI-source
# stage) both interactively and for the `midi-source load` L1 verb.
target_link_libraries(hostrt PUBLIC arrangrr runtime orchestrator)

# IMidiHal backend selection (docs/proposals/looper-in-gui-contract.md §7
# item 12, the cross-platform MIDI backend seam): exactly one concrete
# backend .cpp is compiled per platform, chosen by CMAKE_SYSTEM_NAME/APPLE/
# WIN32 -- never by the build host -- so a plain Linux `cmake`+build stays
# byte-for-byte unaffected while macOS/Windows configure without ever
# reaching `find_package(ALSA REQUIRED)`. midi_hal_factory.cpp's own
# preprocessor branch (see that file) picks the SAME backend at compile time,
# so the CMake selection below and the factory's selection can never
# disagree.
if(APPLE)
  # CoreMidiMidi (components/platform/hostrt/coremidi_midi.{hpp,cpp}):
  # code-complete, CI-verified only (no macOS/osxcross SDK on this dev host).
  target_sources(hostrt PRIVATE coremidi_midi.cpp)
  target_link_libraries(hostrt PUBLIC "-framework CoreMIDI" "-framework CoreFoundation")
elseif(WIN32)
  # WinMmMidi (components/platform/hostrt/winmm_midi.{hpp,cpp}): code-
  # complete, exercised by the mingw-w64 cross-build (cmake/toolchains/
  # mingw-w64.cmake); a full run is CI-verified only.
  target_sources(hostrt PRIVATE winmm_midi.cpp)
  target_link_libraries(hostrt PUBLIC winmm)
else()
  # Linux, the only backend verifiable on this dev host.
  find_package(ALSA REQUIRED)
  target_sources(hostrt PRIVATE alsa_midi.cpp)
  target_link_libraries(hostrt PUBLIC ALSA::ALSA)
endif()

target_compile_options(hostrt PRIVATE -Wall -Wextra -Werror)

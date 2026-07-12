find_package(ALSA REQUIRED)

# Host layer as a library so unit tests can link shell/jsonl directly.
add_library(hostrt STATIC
  shell.cpp
  shell_parse.cpp
  shell_io_commands.cpp
  shell_music_commands.cpp
  shell_input.cpp
  shell_view.cpp
  shell_chooser.cpp
  jsonl.cpp
  alsa_midi.cpp
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
)
target_include_directories(hostrt PUBLIC .)
target_link_libraries(hostrt PUBLIC arrangrr runtime ALSA::ALSA)
target_compile_options(hostrt PRIVATE -Wall -Wextra -Werror)

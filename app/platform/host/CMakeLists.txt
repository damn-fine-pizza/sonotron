find_package(ALSA REQUIRED)

# Host layer as a library so unit tests can link shell/jsonl directly.
add_library(arrangrr_host STATIC
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
  panel_manager.cpp
  piano_view.cpp
  midi_monitor.cpp
  ui_style.cpp
  style_chooser.cpp
  rc_config.cpp
)
target_include_directories(arrangrr_host PUBLIC .)
target_link_libraries(arrangrr_host PUBLIC arrangrr_core ALSA::ALSA)
target_compile_options(arrangrr_host PRIVATE -Wall -Wextra -Werror)

add_executable(arrangrr_cli main.cpp)
set_target_properties(arrangrr_cli PROPERTIES OUTPUT_NAME arrangrr)
target_link_libraries(arrangrr_cli PRIVATE arrangrr_host)
target_compile_options(arrangrr_cli PRIVATE -Wall -Wextra -Werror)

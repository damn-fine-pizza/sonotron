find_package(ALSA REQUIRED)

add_executable(arrangrr_cli
  main.cpp
  shell.cpp
  jsonl.cpp
  alsa_midi.cpp
)
set_target_properties(arrangrr_cli PROPERTIES OUTPUT_NAME arrangrr)
target_link_libraries(arrangrr_cli PRIVATE arrangrr_core ALSA::ALSA)
target_compile_options(arrangrr_cli PRIVATE -Wall -Wextra -Werror)

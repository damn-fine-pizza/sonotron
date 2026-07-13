# apps/tools/melodd: the standalone drop-in-synth binary (Phase-5 Item F,
# `0910`/D43). Opens an ALSA sequencer MIDI input port named "melodd" and a
# miniaudio playback device; its audio callback pulls rendered frames from a
# melodd::Synth, ALSA input drives the same Synth. See
# components/melodd/README.md for the standalone-vs-in-process scope note.
#
# HOST-ONLY: links melodd + miniaudio + ALSA. Never on the arm-none-eabi
# (firmware) branch of the top-level CMakeLists.txt.
find_package(ALSA REQUIRED)

add_executable(melodd_bin main.cpp)
set_target_properties(melodd_bin PROPERTIES OUTPUT_NAME melodd)
target_link_libraries(melodd_bin PRIVATE melodd common miniaudio ALSA::ALSA)
target_compile_options(melodd_bin PRIVATE -Wall -Wextra -Werror)

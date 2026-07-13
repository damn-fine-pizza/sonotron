# components/melodd: HOST-ONLY GM SoundFont realizer (Phase-5 Item F, `0910`/
# D43) — the audio-realization companion the workstation-vision Engines tier
# calls for. Wraps TinySoundFont (vendored, third_party/tinysoundfont) behind
# a clean `melodd::Synth` interface: MIDI channel-voice messages in, float
# audio frames out. One voice per channel, no effects, no mixing, no editing
# surface -- see README.md for the scope discipline and the documented
# in-process follow-up.
#
# HOST-ONLY: this whole component links TinySoundFont (std::string file I/O)
# and must NEVER be added on the arm-none-eabi (firmware) branch of the
# top-level CMakeLists.txt.
add_library(melodd STATIC
  src/synth.cpp
  src/soundfont_discovery.cpp
)
target_include_directories(melodd PUBLIC include)
target_link_libraries(melodd PRIVATE tinysoundfont)
target_compile_options(melodd PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)

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
  src/dispatch.cpp
)
target_include_directories(melodd PUBLIC include)
# dispatch.hpp's public signature names arrangrr::MidiMessage
# (components/common/include/common/midi/message.hpp). Reach that header
# directly via its include directory rather than target_link_libraries(melodd
# ... common), which would additionally force common's INTERFACE
# -fno-exceptions/-fno-rtti/-fno-threadsafe-statics compile options onto this
# target's OWN TUs (synth.cpp/soundfont_discovery.cpp/dispatch.cpp) --
# breaking the documented "no -fno-exceptions on the melodd target" invariant
# (README.md, Phase-6 design review traced ground). PUBLIC so every consumer
# that reaches dispatch.hpp through this target (gui_sonotron_audio,
# apps/tools/melodd) resolves the include too, without inheriting the flags.
target_include_directories(melodd PUBLIC ${CMAKE_SOURCE_DIR}/components/core/common/include)
target_link_libraries(melodd PRIVATE tinysoundfont)
target_compile_options(melodd PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)

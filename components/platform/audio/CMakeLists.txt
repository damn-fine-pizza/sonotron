find_package(Threads REQUIRED)

# audio_ring: the SPSC ring template + the shared AudioMidiEvent POD (docs/
# proposals/components-restructure-move-plan.md §2/§3 Step 5, Palladio): a
# light INTERFACE target so gui_sonotron_engine (never touches an audio
# device) can reach the shared header without linking melodd/miniaudio.
# Reaches components/core/common's include directory directly (NOT through
# target_link_libraries(audio_ring ... common)) to avoid inheriting
# common's INTERFACE -fno-exceptions/-fno-rtti/-fno-threadsafe-statics onto
# every consumer of this ring -- the same avoidance
# components/platform/engines/melodd's own CMakeLists documents for the
# identical reason.
add_library(audio_ring INTERFACE)
target_include_directories(audio_ring INTERFACE include)
target_include_directories(audio_ring INTERFACE
  ${CMAKE_SOURCE_DIR}/components/core/common/include)

# audio: the concrete device layer (docs/proposals/isoundengine-contract.md,
# Corelli §2) -- AudioBackend (owns the miniaudio device + an
# audio_engine::ISoundEngine& chosen once at construction + the render/
# panic mutex) and SoundfontEngine (the first concrete ISoundEngine,
# wrapping melodd::Synth, Corelli §1/§8). HOST-ONLY: links melodd/
# miniaudio/Threads -- never on the arm-none-eabi branch of the top-level
# CMakeLists.txt.
add_library(audio STATIC
  src/audio_backend.cpp
  src/soundfont_engine.cpp
)
target_include_directories(audio PUBLIC include)
# i_sound_engine.hpp is reached directly via its include directory, NOT
# target_link_libraries(audio ... audio_engine_core): audio_engine_core
# links components/core/common INTERFACE, which would otherwise force
# -fno-exceptions/-fno-rtti onto this HOST-ONLY target's own TUs (both use
# std::string via melodd::Synth) -- the same avoidance melodd's own
# CMakeLists documents for reaching common's include directly.
target_include_directories(audio PUBLIC
  ${CMAKE_SOURCE_DIR}/components/core/audio_engine/include)
target_link_libraries(audio PUBLIC audio_ring melodd miniaudio Threads::Threads)
target_compile_options(audio PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)

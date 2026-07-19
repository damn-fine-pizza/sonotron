#include "midi_hal.hpp"

#include <memory>

// The platform-selected half of the seam declared in midi_hal.hpp: exactly
// one of these three branches is compiled into any given build (CMakeLists
// selects the matching backend .cpp per platform -- see components/platform/
// hostrt/CMakeLists.txt), and the preprocessor branch below picks the same
// backend at the call site so the two selections can never disagree.
#if defined(__linux__)
#include "alsa_midi.hpp"
#elif defined(__APPLE__)
#include "coremidi_midi.hpp"
#elif defined(_WIN32)
#include "winmm_midi.hpp"
#else
#error "make_midi_hal(): no IMidiHal backend for this host platform"
#endif

namespace arrangrr::host {

std::unique_ptr<IMidiHal> make_midi_hal() {
#if defined(__linux__)
  return std::make_unique<AlsaMidi>();
#elif defined(__APPLE__)
  return std::make_unique<CoreMidiMidi>();
#elif defined(_WIN32)
  return std::make_unique<WinMmMidi>();
#endif
}

}  // namespace arrangrr::host

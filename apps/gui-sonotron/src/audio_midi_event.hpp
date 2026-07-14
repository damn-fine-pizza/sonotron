#pragma once

#include <cstddef>
#include <cstdint>

#include "common/midi/message.hpp"
#include "spsc_ring.hpp"

// Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in the standalone
// GUI"): the POD carried across the SECOND SPSC ring, engine thread ->
// gui_sonotron_audio's ma_device callback (Decision 1/2/3 of that review).
// Deliberately NOT arrangrr::OutEvent -- no Kind/Param/tick, just what
// melodd::Synth needs -- so gui_sonotron_audio never has to name
// arrangrr::OutEvent/Param/Kind and stays arrangrr-free (arrangrr::
// MidiMessage itself lives in components/common, already a dependency-free
// type apps/tools/melodd reuses the same way).
//
// This header lives in the shared apps/gui-sonotron/src include root (every
// library target in this directory adds the same include directory) so both
// gui_sonotron_engine (producer, in_process_brain_session.cpp) and
// gui_sonotron_audio (consumer, audio_engine.cpp) can name the exact same
// SpscRing<AudioMidiEvent, N> instantiation without a link edge between
// those two libraries.

namespace sonotron {

struct AudioMidiEvent {
  std::uint8_t port = 0;
  arrangrr::MidiMessage msg{};
};

// Human-triggered note traffic is orders of magnitude below what a bounded
// ring needs to absorb between two audio callbacks; 1024 slots of a 4-byte
// AudioMidiEvent is ~4 KB, trivial headroom (mirrors kOutEventRingCapacity's
// own sizing note in in_process_brain_session.cpp).
inline constexpr std::size_t kAudioMidiRingCapacity = 1024;
using AudioMidiRing = SpscRing<AudioMidiEvent, kAudioMidiRingCapacity>;

}  // namespace sonotron

#pragma once

#include <cstddef>
#include <cstdint>

#include "audio/spsc_ring.hpp"
#include "common/midi/message.hpp"

// Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in the standalone
// GUI"): the POD carried across the SECOND SPSC ring, engine thread ->
// the audio device layer's ma_device callback (Decision 1/2/3 of that
// review). Deliberately NOT arrangrr::OutEvent -- no Kind/Param/tick, just
// what an ISoundEngine needs -- so this component never has to name
// arrangrr::OutEvent/Param/Kind and stays arrangrr-free (arrangrr::
// MidiMessage itself lives in components/core/common, already a
// dependency-free type apps/tools/melodd reuses the same way).
//
// Promoted out of apps/gui-sonotron (docs/proposals/
// components-restructure-move-plan.md §2/§3 Step 5, Palladio): this header
// and spsc_ring.hpp are content-wise regime-neutral (<atomic>/<array>/
// <cstdint> only, no heap, no exceptions) but are shared between two
// libraries that must NOT depend on each other -- gui_sonotron_engine
// (producer, apps/gui-sonotron/src/in_process_brain_session.cpp) and the
// `audio` component (consumer, audio_backend.cpp) -- so they live here,
// in their own platform-tier component (`audio_ring`, header-only), rather
// than inside either consumer's own tree.

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

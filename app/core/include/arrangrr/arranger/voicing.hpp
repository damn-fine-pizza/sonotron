#pragma once

#include <cstdint>

#include "arrangrr/arranger/style_model.hpp"  // VoicingPolicy
#include "arrangrr/common/time.hpp"           // TickOffset
#include "arrangrr/timeline/timeline.hpp"     // TrackRole

// Voicing stage of the arranger's resolution pipeline (D40). One StyleEvent no
// longer maps to one note in isolation: the fire loop gathers a role's notes
// for a step (after gesture expansion and NTT resolution) into a bounded group,
// and this stage may reshape the CHORD-TONE members of that group for smooth
// voice-leading — retaining common tones and minimizing motion from the role's
// previous voicing. It never invents notes and never touches melodic
// (scale-degree / interval) or fixed (drum) notes: it only re-octaves chord
// tones, so the NTT wrong-note guarantee is preserved.
//
// Freestanding, no heap, deterministic: the only state is a small per-role
// memory of the last voicing, reset on style load and transport start.

namespace arrangrr {

// One resolved note flowing through the per-step pipeline. Gesture expansion +
// resolve() produce these; the voicing stage may re-octave the chord-tone ones;
// the fire loop schedules a note-on/off pair per entry.
struct NoteReq {
  int note = -1;                 // resolved MIDI pitch 0..127 (<0 = dropped)
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;        // ticks
  TickOffset gesture_delay = 0;  // extra scheduling offset from a gesture (0 = none)
  bool chord_tone = false;       // true = a chord tone eligible for re-voicing
};

// Per-role voice-leading memory. kAsWritten is a pure identity (the historical
// behavior — notes sound at their authored register); kLead smooths motion
// between successive chords. Scaffold: both paths are identity until Phase 1
// implements kLead.
class VoicingState {
 public:
  // Clears remembered voicings so a fresh style/transport start never leads
  // from stale notes. Called from Arranger::load_style and on_transport_start.
  void reset() noexcept {
    // Phase 1: clear the per-role last-voicing memory. Nothing to clear yet.
  }

  // Reshapes the chord-tone members of reqs[0..n) for `role` under `policy`,
  // updating the remembered voicing. Scaffold stub: identity (kAsWritten
  // reproduces today's output exactly; kLead is implemented in Phase 1).
  void voice(TrackRole /*role*/, VoicingPolicy /*policy*/, NoteReq* /*reqs*/, int /*n*/) noexcept {}
};

}  // namespace arrangrr

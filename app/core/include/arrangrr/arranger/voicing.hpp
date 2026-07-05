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
// behavior — notes sound at their authored register); kLead re-octaves a role's
// chord tones so each voice follows the nearest octave of its previous pitch,
// retaining common tones and minimizing motion between successive chords.
//
// The model: a role's chord is authored as several chord-tone events in a fixed
// order (root, third, fifth, ...). Across a chord change the SAME pattern plays,
// only the live chord differs, so event slot j is always "the j-th chord tone".
// kLead moves each slot's resolved pitch by whole octaves to sit closest to
// where that slot sounded last — a common tone (same pitch class) does not move
// at all. Only chord tones are touched; melodic (scale-degree/interval) and
// fixed (drum) notes pass through untouched, so the NTT guarantee holds.
//
// Intended for chordal comping parts (chord1/chord2/pad); a style author leaves
// a bass/lead part as kAsWritten. Deterministic, no heap: the only state is a
// small per-role array of the last voiced pitches, cleared on reset().
class VoicingState {
 public:
  // Clears remembered voicings so a fresh style/transport start never leads
  // from stale notes. Called from Arranger::load_style and on_transport_start.
  void reset() noexcept {
    for (int r = 0; r < kRoleCount; ++r) {
      m_count[r] = 0;
    }
  }

  // Reshapes the chord-tone members of reqs[0..n) for `role` under `policy`,
  // updating the remembered voicing. kAsWritten is identity (reproduces today's
  // output exactly); kLead re-octaves each chord tone toward the previous
  // voicing. The first chord after a reset keeps its authored register (nothing
  // to lead from yet).
  void voice(TrackRole role, VoicingPolicy policy, NoteReq* reqs, int n) noexcept {
    if (policy != VoicingPolicy::kLead) {
      return;  // kAsWritten: authored register, no re-voicing
    }
    const int r = static_cast<int>(role);
    if (r < 0 || r >= kRoleCount) {
      return;
    }
    int voiced[kMaxVoiceTones];
    int m = 0;
    const int prev_count = m_count[r];
    for (int i = 0; i < n && m < kMaxVoiceTones; ++i) {
      if (!reqs[i].chord_tone) {
        continue;  // melodic / drum notes are never re-voiced
      }
      int note = reqs[i].note;
      if (prev_count > 0) {
        const int slot = m < prev_count ? m : prev_count - 1;
        note = nearest_octave(note, m_last[r][slot]);
      }
      reqs[i].note = note;
      voiced[m] = note;
      ++m;
    }
    // A step with no chord tones (e.g. a lone melodic/passing note in a kLead
    // part) must NOT erase the voicing history — leave it for the next chord to
    // lead from, exactly as a pure rest (count == 0, handled by the caller) does.
    if (m > 0) {
      for (int i = 0; i < m; ++i) {
        m_last[r][i] = voiced[i];
      }
      m_count[r] = m;
    }
  }

 private:
  static constexpr int kRoleCount = 10;     // TrackRole count (kDrums..kCc)
  static constexpr int kMaxVoiceTones = 8;  // chord tones tracked per role

  // Moves `note` by whole octaves to sit as close as possible to `target`,
  // staying inside the MIDI range. A note already within a tritone of the
  // target does not move, so a common tone is retained exactly.
  static int nearest_octave(int note, int target) noexcept {
    while (note - target > 6 && note - 12 >= 0) {
      note -= 12;
    }
    while (target - note > 6 && note + 12 <= 127) {
      note += 12;
    }
    return note;
  }

  int m_last[kRoleCount][kMaxVoiceTones] = {};
  int m_count[kRoleCount] = {};
};

}  // namespace arrangrr

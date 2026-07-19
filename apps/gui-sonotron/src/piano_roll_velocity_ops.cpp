#include "piano_roll_edit_ops.hpp"

#include <algorithm>
#include <cstdint>

namespace sonotron {

// Hover+scroll-wheel (or Alt+drag) velocity nudge (design doc §3 "if
// cheap"): adjusts `step_index`'s vel by `delta`, clamped into [1,127] so
// the nudge can never itself silence an audible step (vel==0 has a
// different meaning -- "empty slot", never produced by this gesture) nor
// overflow past the ABI's 7-bit MIDI velocity range. Reads the FULL current
// step first and re-supplies every other field unchanged through
// write_step_full_restate -- the same full-restate discipline every other
// op in this file's sibling (piano_roll_edit_ops.cpp) already follows, so a
// velocity nudge can never silently strip an existing probability/ratchet/
// micro/tie lock. An already-empty step (vel == 0, including an
// out-of-range `step_index`, which StepPatternModel::step() defensively
// returns as an all-zero empty step) is left untouched -- there is no note
// there to nudge.
bool apply_velocity_delta(StepPatternModel& track, BrainSession* session, int track_idx,
                          std::size_t step_index, int delta) {
  StepPatternStep s = track.step(step_index);
  if (s.vel == 0) {
    return false;
  }
  const int new_vel = std::clamp(static_cast<int>(s.vel) + delta, 1, 127);
  s.vel = static_cast<std::uint8_t>(new_vel);
  return write_step_full_restate(track, session, track_idx, step_index, s);
}

}  // namespace sonotron

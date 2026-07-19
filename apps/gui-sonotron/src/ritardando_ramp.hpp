#pragma once

#include "common/time.hpp"

// Ritardando (Arm A, "HOST-NUDGE" -- docs/proposals/ritardando-tempo-curve-
// fork.md, owner-picked over the core-primitive Arm B): pure ramp math for a
// GUI-only tempo ramp-down authored across the Ending section's own bar
// span. Deliberately free of Shell/OutEvent/Engine so it is trivially unit-
// testable (test_ritardando_ramp.cpp) -- the stateful wiring (arming off
// OutEvent::kSection, stepping off OutEvent::kBeat, pushing through the
// existing Param::kTransportTempo write path) lives in
// in_process_brain_session.cpp's own RitardandoState, right where those
// events are already handled; only the curve itself is hoisted out here so
// it can be pinned independently of that engine-thread plumbing.
//
// HOST-ONLY, but MAY name arrangrr::BpmX100 (a common/time.hpp type, not an
// arrangrr/ core type) -- included only from gui_sonotron_engine's own
// translation unit and this header's own unit test, both of which already
// link the core transitively (see apps/gui-sonotron/CMakeLists.txt's
// gui_sonotron_engine target comment), so this does not reopen the core-free
// invariant gui_sonotron_layout/gui_sonotron_models hold to.

namespace sonotron::ritardando {

// Where the ramp settles, relative to the tempo captured the instant the
// Ending section is entered. 0.60 sits inside the owner-specified 55-65%
// band: audibly a deliberate ritardando (not a token wobble) while stopping
// well short of feeling like the band stalled before the ending's own
// bar-count auto-stop (untouched by this feature -- arranger.hpp's
// section_is_ending-gated stop_transport, see the proposal doc's own §4)
// actually lands.
inline constexpr float kTargetBpmRatio = 0.60F;

// Ease-IN curve (t^2): the ramp barely moves for the first part of the
// ending, then falls away faster as the final bar approaches -- the
// "held, then given" shape of a hand-conducted rit., versus a linear ramp's
// uniform (and audibly more mechanical) slope. Plain host arithmetic (GUI-
// only authoring code, not the realtime core) -- float is fine here, D32
// only forbids float NEAR the realtime tick path.
constexpr float eased(float t) noexcept { return t * t; }

// The bpm the ramp should be holding after `elapsed_pulses` of its own
// `total_pulses` (each pulse == one OutEvent::kBeat, 1/24th of a beat --
// common/time.hpp's kMidiClockDivider). `total_pulses == 0` and
// `elapsed_pulses >= total_pulses` both defensively resolve to `target_bpm`
// (ramp complete) rather than a div-by-zero or an out-of-range extrapolation.
constexpr arrangrr::BpmX100 bpm_at(arrangrr::BpmX100 start_bpm, arrangrr::BpmX100 target_bpm,
                                   std::uint32_t elapsed_pulses,
                                   std::uint32_t total_pulses) noexcept {
  if (total_pulses == 0 || elapsed_pulses >= total_pulses) {
    return target_bpm;
  }
  const float t = static_cast<float>(elapsed_pulses) / static_cast<float>(total_pulses);
  const float span = static_cast<float>(start_bpm) - static_cast<float>(target_bpm);
  return static_cast<arrangrr::BpmX100>(static_cast<float>(start_bpm) - span * eased(t));
}

}  // namespace sonotron::ritardando

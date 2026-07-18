#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Host-side, editable "step pattern" -- a 1:1 hand-copied mirror of the
// core's arrangrr::Timeline::Track/Step (components/core/arrangrr/include/
// arrangrr/timeline/timeline.hpp), the SAME D38 discipline track_roles.hpp
// already uses for TrackRole: gui-sonotron never includes the core, so this
// is a read-only literal copy of the core's own shape, not an invented one.
//
// This is Phase 1 of the Sequence Edit step-sequencer feature (roadmap node
// 11600/11610): the EDITABLE SOURCE OF TRUTH the Repeat Zone's step-track
// gesture (grid_panel.cpp) and the Sequence Edit "step" view (seqedit_panel.
// cpp) both read/write, kept RICH (note/vel/gate plus the full param-lock
// set: probability/ratchet/micro/tie) rather than a bare on/off flag, so a
// future Phase-2 piano-roll can be a richer VIEW over this SAME data, never
// a second content kind. Every write here is paired by the caller with the
// matching `track step ...` wire command (in_process_brain_session.cpp's
// translator) so the core's own arrangrr::Timeline stays in sync -- this
// class is the GUI's own local echo of that core state, not a replacement
// for it.

namespace sonotron {

// Hand-copied literal (D38), numerically IDENTICAL to arrangrr::kMaxTracks/
// kMaxStepsPerTrack/kMaxRatchet/kTicksPerStep (arrangrr/config.hpp,
// arrangrr/timeline/timeline.hpp).
inline constexpr std::size_t kStepPatternMaxTracks = 16;
inline constexpr std::size_t kStepPatternMaxSteps = 64;
inline constexpr std::uint8_t kStepPatternMaxRatchet = 8;
inline constexpr std::uint32_t kStepPatternTicksPerStep = 240;  // 960 PPQN / 4

// Field-for-field mirror of arrangrr::Step (timeline.hpp). Defaults match the
// core's own neutral values (a default-constructed Step reproduces the
// original one-note-per-step behaviour byte-for-byte).
struct StepPatternStep {
  std::uint8_t note = 0;
  std::uint8_t vel = 0;  // 0 = empty slot (no event)
  std::uint16_t gate = 0;
  std::uint8_t probability = 100;
  std::uint8_t ratchet = 1;
  std::uint8_t micro = 0;
  bool tie = false;
};

// Field-for-field mirror of arrangrr::Track (timeline.hpp), minus the raw
// TrackRole enum (this header stays core-free, D38 -- the OWNING container,
// SeqEditModel, threads the role as a plain std::size_t index into
// track_roles.hpp's own 9-entry vocabulary instead).
class StepPatternModel {
 public:
  StepPatternModel() = default;
  StepPatternModel(std::size_t role_index, std::uint8_t port, std::uint8_t channel)
      : m_role_index(role_index), m_port(port), m_channel(channel) {}

  std::size_t role_index() const { return m_role_index; }
  std::uint8_t port() const { return m_port; }
  std::uint8_t channel() const { return m_channel; }

  std::uint8_t length() const { return m_length; }
  // Clamped into [1, kStepPatternMaxSteps]; a 0 or out-of-range request is a
  // silent no-op (mirrors arrangrr::Timeline::set_length's own bounds check).
  void set_length(std::size_t steps);

  const StepPatternStep& step(std::size_t index) const;

  // Mirrors arrangrr::Timeline::set_step's own validation exactly: rejects
  // note/vel > 127 and an audible step (vel > 0) with gate == 0 (that would
  // land the NoteOff on the NoteOn's own tick -- a guaranteed stuck note),
  // clamps probability to [0,100] and ratchet to [1,kStepPatternMaxRatchet].
  // Returns false (no write) on an out-of-range `index` or a rejected note/
  // vel/gate combination.
  bool set_step(std::size_t index, std::uint8_t note, std::uint8_t vel, std::uint16_t gate,
                std::uint8_t probability = 100, std::uint8_t ratchet = 1, std::uint8_t micro = 0,
                bool tie = false);

  // Clears one step back to its neutral (silent) default -- mirrors the CLI's
  // `track step <i> clear`.
  void clear_step(std::size_t index);

  bool mute() const { return m_mute; }
  void set_mute(bool mute) { m_mute = mute; }
  bool solo() const { return m_solo; }
  void set_solo(bool solo) { m_solo = solo; }

 private:
  std::size_t m_role_index = 0;
  std::uint8_t m_port = 0;
  std::uint8_t m_channel = 0;
  std::uint8_t m_length = 16;
  bool m_mute = false;
  bool m_solo = false;
  std::array<StepPatternStep, kStepPatternMaxSteps> m_steps{};
};

// Bounded pool of StepPatternModel tracks (mirrors arrangrr::Timeline's own
// add-only, kStepPatternMaxTracks-capped pool) -- the container SeqEditModel
// owns so a step-track clip created by grid_panel.cpp's own gesture and
// edited by seqedit_panel.cpp's "step" view always read/write the SAME
// instance.
class StepPatternStore {
 public:
  // Returns the new track's index, or -1 when the pool (kStepPatternMaxTracks)
  // is already full.
  int create_track(std::size_t role_index, std::uint8_t port, std::uint8_t channel);

  std::size_t track_count() const { return m_tracks.size(); }

  StepPatternModel* track(std::size_t index);
  const StepPatternModel* track(std::size_t index) const;

 private:
  std::vector<StepPatternModel> m_tracks;
};

}  // namespace sonotron

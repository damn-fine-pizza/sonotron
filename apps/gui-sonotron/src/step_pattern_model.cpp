#include "step_pattern_model.hpp"

namespace sonotron {

void StepPatternModel::set_length(std::size_t steps) {
  if (steps == 0 || steps > kStepPatternMaxSteps) {
    return;
  }
  m_length = static_cast<std::uint8_t>(steps);
}

const StepPatternStep& StepPatternModel::step(std::size_t index) const {
  static const StepPatternStep kEmpty{};
  return index < kStepPatternMaxSteps ? m_steps[index] : kEmpty;
}

bool StepPatternModel::set_step(std::size_t index, std::uint8_t note, std::uint8_t vel,
                                std::uint16_t gate, std::uint8_t probability, std::uint8_t ratchet,
                                std::uint8_t micro, bool tie) {
  if (index >= kStepPatternMaxSteps || note > 127 || vel > 127) {
    return false;
  }
  if (vel > 0 && gate == 0) {
    return false;
  }
  StepPatternStep s;
  s.note = note;
  s.vel = vel;
  s.gate = gate;
  s.probability = probability > 100 ? 100 : probability;
  s.ratchet =
      ratchet < 1 ? 1 : (ratchet > kStepPatternMaxRatchet ? kStepPatternMaxRatchet : ratchet);
  s.micro = micro;
  s.tie = tie;
  m_steps[index] = s;
  return true;
}

void StepPatternModel::clear_step(std::size_t index) {
  if (index < kStepPatternMaxSteps) {
    m_steps[index] = StepPatternStep{};
  }
}

int StepPatternStore::create_track(std::size_t role_index, std::uint8_t port,
                                   std::uint8_t channel) {
  if (m_tracks.size() >= kStepPatternMaxTracks) {
    return -1;
  }
  m_tracks.emplace_back(role_index, port, channel);
  return static_cast<int>(m_tracks.size() - 1);
}

StepPatternModel* StepPatternStore::track(std::size_t index) {
  return index < m_tracks.size() ? &m_tracks[index] : nullptr;
}

const StepPatternModel* StepPatternStore::track(std::size_t index) const {
  return index < m_tracks.size() ? &m_tracks[index] : nullptr;
}

}  // namespace sonotron

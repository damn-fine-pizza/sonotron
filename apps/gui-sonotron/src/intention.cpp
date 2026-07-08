#include "intention.hpp"

#include <algorithm>

namespace sonotron {

float clamp_unit(float value) { return std::clamp(value, 0.0F, 1.0F); }

IntentionState mock_intention_state() {
  IntentionState state;
  state.axes[kEnergy] = IntentionAxis{.label = "ENERGY", .current = 0.62F, .target = 0.85F};
  state.axes[kTension] = IntentionAxis{.label = "TENSION", .current = 0.25F, .target = 0.55F};
  state.axes[kValence] = IntentionAxis{.label = "VALENCE", .current = 0.70F, .target = 0.40F};
  return state;
}

}  // namespace sonotron

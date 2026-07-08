// Unit tests for the Intention view-model (pure data — no ImGui): the
// clamp_unit range guard and the mock state's shape/invariants.

#include "src/intention.hpp"

#include <cmath>
#include <cstring>

#include "test.hpp"

namespace {

bool approx(float a, float b, float epsilon = 1e-4F) { return std::fabs(a - b) < epsilon; }

void test_clamp_unit_bounds() {
  CHECK(approx(sonotron::clamp_unit(-0.5F), 0.0F));
  CHECK(approx(sonotron::clamp_unit(0.0F), 0.0F));
  CHECK(approx(sonotron::clamp_unit(0.42F), 0.42F));
  CHECK(approx(sonotron::clamp_unit(1.0F), 1.0F));
  CHECK(approx(sonotron::clamp_unit(1.5F), 1.0F));
}

void test_mock_state_shape() {
  const sonotron::IntentionState state = sonotron::mock_intention_state();
  CHECK(state.axes.size() == 3);
  CHECK(std::strcmp(state.axes[sonotron::kEnergy].label, "ENERGY") == 0);
  CHECK(std::strcmp(state.axes[sonotron::kTension].label, "TENSION") == 0);
  CHECK(std::strcmp(state.axes[sonotron::kValence].label, "VALENCE") == 0);
}

void test_mock_values_in_unit_range_and_have_visible_gap() {
  const sonotron::IntentionState state = sonotron::mock_intention_state();
  for (const sonotron::IntentionAxis& axis : state.axes) {
    CHECK(axis.current >= 0.0F && axis.current <= 1.0F);
    CHECK(axis.target >= 0.0F && axis.target <= 1.0F);
    // Every mock axis is deliberately asymmetric so the panel's
    // current->target rendering is always exercised.
    CHECK(!approx(axis.current, axis.target));
  }
}

void test_mock_valence_moves_downward() {
  // Valence is the one axis whose target sits below its current, proving the
  // marker can render to the left of the fill, not only the right.
  const sonotron::IntentionState state = sonotron::mock_intention_state();
  CHECK(state.axes[sonotron::kValence].target < state.axes[sonotron::kValence].current);
}

}  // namespace

int main() {
  test_clamp_unit_bounds();
  test_mock_state_shape();
  test_mock_values_in_unit_range_and_have_visible_gap();
  test_mock_valence_moves_downward();
  return sonotron::test::failures();
}

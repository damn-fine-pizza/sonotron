// Unit tests for sonotron::ritardando::eased/bpm_at (ritardando_ramp.hpp,
// Arm A "HOST-NUDGE" -- docs/proposals/ritardando-tempo-curve-fork.md): pins
// the pure ramp-curve math independently of the Shell/OutEvent wiring that
// consumes it in in_process_brain_session.cpp's RitardandoState.

#include "src/ritardando_ramp.hpp"

#include "test.hpp"

using arrangrr::BpmX100;
using sonotron::ritardando::bpm_at;
using sonotron::ritardando::eased;

namespace {

void test_eased_endpoints_and_midpoint() {
  CHECK(eased(0.0F) == 0.0F);
  CHECK(eased(1.0F) == 1.0F);
  CHECK(eased(0.5F) == 0.25F);
}

// total_pulses == 0 defensively resolves to target_bpm (ramp complete)
// rather than a div-by-zero, regardless of elapsed_pulses.
void test_zero_total_pulses_returns_target() {
  CHECK(bpm_at(12000, 7200, 0, 0) == 7200);
  CHECK(bpm_at(12000, 7200, 5, 0) == 7200);
}

// elapsed_pulses >= total_pulses (ramp complete, or overshoot) also
// resolves to target_bpm exactly.
void test_elapsed_at_or_past_total_returns_target() {
  CHECK(bpm_at(12000, 7200, 96, 96) == 7200);
  CHECK(bpm_at(12000, 7200, 200, 96) == 7200);
}

// elapsed_pulses == 0 (ramp just armed) returns start_bpm exactly (t == 0,
// eased(0) == 0).
void test_zero_elapsed_returns_start() {
  CHECK(bpm_at(12000, 7200, 0, 96) == 12000);
}

// Exact quarter/half/three-quarter points along a 96-pulse ramp from 120.00
// to 72.00 bpm (bpm_x100 12000 -> 7200, span 4800): the t^2 ease-in curve
// moves LESS in the first half than the second (11700 -> 10800 is a 900
// bpm_x100 drop over the first quarter-to-half stretch, while 9300 -> 7200
// is a 2100 drop over the last quarter), the "held, then given" shape
// documented on ritardando::eased's own header comment.
void test_quarter_half_three_quarter_points() {
  CHECK(bpm_at(12000, 7200, 24, 96) == 11700);
  CHECK(bpm_at(12000, 7200, 48, 96) == 10800);
  CHECK(bpm_at(12000, 7200, 72, 96) == 9300);
}

// The curve is monotonically non-increasing as elapsed_pulses advances --
// tempo never ticks back UP mid-ramp.
void test_monotonically_non_increasing() {
  BpmX100 previous = bpm_at(12000, 7200, 0, 96);
  for (std::uint32_t elapsed = 1; elapsed <= 96; ++elapsed) {
    const BpmX100 current = bpm_at(12000, 7200, elapsed, 96);
    CHECK(current <= previous);
    previous = current;
  }
}

// start_bpm == target_bpm (nothing to ramp) returns that same value for
// every point along the ramp, never drifting off it.
void test_start_equals_target_is_a_no_op() {
  CHECK(bpm_at(9000, 9000, 0, 20) == 9000);
  CHECK(bpm_at(9000, 9000, 10, 20) == 9000);
  CHECK(bpm_at(9000, 9000, 20, 20) == 9000);
}

}  // namespace

int main() {
  test_eased_endpoints_and_midpoint();
  test_zero_total_pulses_returns_target();
  test_elapsed_at_or_past_total_returns_target();
  test_zero_elapsed_returns_start();
  test_quarter_half_three_quarter_points();
  test_monotonically_non_increasing();
  test_start_equals_target_is_a_no_op();
  return sonotron::test::failures();
}

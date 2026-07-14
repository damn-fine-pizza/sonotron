// Unit tests for BoundaryLatch (Phase-5 Item #9, docs/phase5-design-reviews.md
// "Pad/Scene live -> Performance", Corelli fix #2): the ONE shared "arm now,
// fire when a boundary tick arrives" primitive Engine::m_perf_recall reuses.
// Pure freestanding POD -- no Engine harness needed, mirroring test_common.cpp's
// own treatment of a common/ primitive in isolation.

#include "arrangrr/common/boundary_latch.hpp"

#include "common/time.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

void test_default_latch_is_inert() {
  BoundaryLatch latch;
  CHECK(!latch.pending);
  CHECK(latch.window == 0);
  // A default-constructed (never armed) latch never fires, at any tick --
  // including tick 0, where a naive `t % window == 0` without the `pending`
  // guard would look due.
  CHECK(!latch.due(0));
  CHECK(!latch.due(kTicksPerBar));
  CHECK(!latch.due(12345));
}

void test_window_zero_guard_no_div_by_zero() {
  // A latch somehow left `pending` with `window == 0` (the documented
  // sentinel) must not attempt t % 0 -- due() short-circuits on
  // `window != 0` BEFORE the modulo. If that guard were ever dropped this
  // would be undefined behavior, not just a wrong answer.
  BoundaryLatch latch;
  latch.pending = true;
  latch.window = 0;
  CHECK(!latch.due(0));
  CHECK(!latch.due(1));
}

void test_arm_one_bar_due_at_exact_multiples() {
  BoundaryLatch latch;
  latch.arm(1);
  CHECK(latch.pending);
  CHECK(latch.window == kTicksPerBar);
  CHECK(!latch.due(1));
  CHECK(!latch.due(kTicksPerBar - 1));
  CHECK(latch.due(kTicksPerBar));  // exact 1-bar boundary
  CHECK(!latch.due(kTicksPerBar + 1));
  CHECK(latch.due(2 * kTicksPerBar));  // every subsequent bar multiple also matches
}

void test_arm_n_bars_due_only_at_the_n_bar_window() {
  BoundaryLatch latch;
  latch.arm(3);
  CHECK(latch.window == 3 * kTicksPerBar);
  CHECK(!latch.due(kTicksPerBar));      // 1 bar in: not yet
  CHECK(!latch.due(2 * kTicksPerBar));  // 2 bars in: not yet
  CHECK(latch.due(3 * kTicksPerBar));   // 3rd bar: due
}

void test_arm_clamps_n_bars_below_one() {
  BoundaryLatch latch;
  latch.arm(0);
  CHECK(latch.window == kTicksPerBar);  // clamped to 1, mirrors ClipMatrix::arm's own clamp
  BoundaryLatch latch2;
  latch2.arm(1);
  CHECK(latch2.window == latch.window);
}

void test_clear_consumes_the_arm() {
  BoundaryLatch latch;
  latch.arm(1);
  CHECK(latch.due(kTicksPerBar));
  latch.clear();
  CHECK(!latch.pending);
  CHECK(!latch.due(kTicksPerBar));      // no longer due once cleared
  CHECK(!latch.due(2 * kTicksPerBar));  // nor at any later multiple
}

void test_rearm_replaces_the_previous_window() {
  // A second arm() call before the first fires REPLACES it (last-writer-
  // wins) -- there is only ONE shared latch, by design, not a queue.
  BoundaryLatch latch;
  latch.arm(4);
  latch.arm(1);
  CHECK(latch.window == kTicksPerBar);
  CHECK(latch.due(kTicksPerBar));
}

}  // namespace

int main() {
  test_default_latch_is_inert();
  test_window_zero_guard_no_div_by_zero();
  test_arm_one_bar_due_at_exact_multiples();
  test_arm_n_bars_due_only_at_the_n_bar_window();
  test_arm_clamps_n_bars_below_one();
  test_clear_consumes_the_arm();
  test_rearm_replaces_the_previous_window();
  return arrangrr::test::failures();
}

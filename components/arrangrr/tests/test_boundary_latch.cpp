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

// --- Phase 7 (node T0): the threaded ticks_per_bar parameter ----------------

// arm()'s second argument is the LIVE bar length (Engine threads
// m_transport.ticks_per_bar() at arm time, e.g. Engine::perf_recall /
// apply_pending_pad_fires's own arming call). A genuine non-4/4 value (a
// 3-beat bar, 3 * kTicksPerBeat = 2880) changes the window exactly as the
// default (kTicksPerBar) case does -- the SAME arithmetic, a different
// divisor.
void test_arm_with_explicit_non_default_ticks_per_bar() {
  BoundaryLatch latch;
  constexpr Tick kThreeBeatBar = 3 * kTicksPerBeat;  // 2880: a genuine 3/4 bar
  CHECK(kThreeBeatBar != kTicksPerBar);              // sanity: really a different meter
  latch.arm(2, kThreeBeatBar);
  CHECK(latch.window == 2 * kThreeBeatBar);  // 5760, NOT 2 * kTicksPerBar (7680)
  CHECK(!latch.due(kThreeBeatBar));          // 1 bar in: not yet
  CHECK(latch.due(2 * kThreeBeatBar));       // 2nd 3-beat bar: due
  CHECK(!latch.due(2 * kTicksPerBar));       // the OLD 4/4 window's tick is NOT a hit
}

// The window is a SNAPSHOT taken at arm() time, not re-derived from a later
// ticks_per_bar value on every due() check -- due() only ever reads the
// already-computed `window` field (see the struct itself: due() takes no
// ticks_per_bar parameter at all). This is BoundaryLatch's own documented
// contract (Engine::m_perf_recall/m_pad_latch rely on it: a Performance
// recall armed under one meter must still land on the bar it was actually
// quantized to, even if the SAME recall it is waiting for is what changes
// the meter). Torquato QA (Phase 7, node T0): this is the baseline this
// pin proves for CONTRAST against ClipMatrix::on_bar's own DIFFERENT
// (live-recomputed, not snapshotted) behavior under the identical scenario
// -- see test_clip_matrix_live_meter_change_regression.cpp.
void test_arm_window_is_frozen_at_arm_time_immune_to_a_later_meter_change() {
  BoundaryLatch latch;
  latch.arm(2, kTicksPerBar);  // armed while the meter is still 4/4
  CHECK(latch.window == 2 * kTicksPerBar);
  // The "meter changes" here is simulated by simply never re-arming: due()
  // has no ticks_per_bar parameter to feed a new value through, by design.
  CHECK(!latch.due(kTicksPerBar));             // 1 bar in: not yet
  CHECK(!latch.due(2 * (3 * kTicksPerBeat)));  // a 3/4-bar-window tick: NOT a hit
  CHECK(latch.due(2 * kTicksPerBar));          // fires exactly where it was armed to
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
  test_arm_with_explicit_non_default_ticks_per_bar();
  test_arm_window_is_frozen_at_arm_time_immune_to_a_later_meter_change();
  return arrangrr::test::failures();
}

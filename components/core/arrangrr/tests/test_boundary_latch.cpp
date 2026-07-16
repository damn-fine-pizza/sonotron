// Unit tests for BoundaryLatch (Phase-5 Item #9, docs/phase5-design-reviews.md
// "Pad/Scene live -> Performance", Corelli fix #2): the ONE shared "arm now,
// fire when a boundary tick arrives" primitive Engine::m_perf_recall reuses.
// Pure freestanding POD -- no Engine harness needed, mirroring test_common.cpp's
// own treatment of a common/ primitive in isolation.
//
// Phase 7 (node 8100 hardening, Torquato QA F1/F2): rewritten for the
// bar-COUNT-based shape (due_bar_index, arm(n_bars, bar_index_now),
// due(bar_index_now)) that replaced the old tick-window/absolute-modulo
// shape -- see boundary_latch.hpp's own header comment for why the old shape
// could permanently miss its own promotion the instant a meter change landed
// between arm() and due().

#include "arrangrr/common/boundary_latch.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

void test_default_latch_is_inert() {
  BoundaryLatch latch;
  CHECK(!latch.pending);
  CHECK(latch.due_bar_index == 0);
  // A default-constructed (never armed) latch never fires, at any bar index
  // -- including bar index 0, where a naive check without the `pending`
  // guard would look due.
  CHECK(!latch.due(0));
  CHECK(!latch.due(1));
  CHECK(!latch.due(12345));
}

void test_arm_one_bar_due_at_the_next_bar_only() {
  BoundaryLatch latch;
  latch.arm(1, /*bar_index_now=*/0);
  CHECK(latch.pending);
  CHECK(latch.due_bar_index == 0);  // n_bars=1 from bar 0 -> due AT bar 0
  CHECK(latch.due(0));
  // Unlike the old tick-window shape (which matched every SUBSEQUENT
  // multiple of the window forever), a bar-count due() fires EXACTLY once,
  // at its own due_bar_index -- a later bar index is simply not a match
  // (production always clear()s right after firing, so this never mattered
  // operationally; it is a cleaner contract than "matches every multiple").
  CHECK(!latch.due(1));
  CHECK(!latch.due(2));
}

void test_arm_n_bars_due_only_at_the_nth_upcoming_bar() {
  BoundaryLatch latch;
  latch.arm(3, /*bar_index_now=*/5);
  CHECK(latch.due_bar_index == 5 + 2);  // the 3rd upcoming bar: 5, 6, [7]
  CHECK(!latch.due(5));                 // 1 bar in: not yet
  CHECK(!latch.due(6));                 // 2 bars in: not yet
  CHECK(latch.due(7));                  // 3rd bar: due
}

void test_arm_clamps_n_bars_below_one() {
  BoundaryLatch latch;
  latch.arm(0, /*bar_index_now=*/10);
  CHECK(latch.due_bar_index == 10);  // clamped to 1, mirrors ClipMatrix::arm's own clamp
  BoundaryLatch latch2;
  latch2.arm(1, /*bar_index_now=*/10);
  CHECK(latch2.due_bar_index == latch.due_bar_index);
}

void test_clear_consumes_the_arm() {
  BoundaryLatch latch;
  latch.arm(1, /*bar_index_now=*/0);
  CHECK(latch.due(0));
  latch.clear();
  CHECK(!latch.pending);
  CHECK(!latch.due(0));  // no longer due once cleared
  CHECK(!latch.due(1));
}

void test_rearm_replaces_the_previous_target() {
  // A second arm() call before the first fires REPLACES it (last-writer-
  // wins) -- there is only ONE shared latch, by design, not a queue.
  BoundaryLatch latch;
  latch.arm(4, /*bar_index_now=*/0);
  latch.arm(1, /*bar_index_now=*/0);
  CHECK(latch.due_bar_index == 0);
  CHECK(latch.due(0));
}

// --- Phase 7 (node 8100 hardening): meter-change immunity -------------------

// arm()'s second argument is Transport::bar_index() AT ARM TIME -- a
// discrete COUNT of bar boundaries the re-anchored Transport gate has
// closed, never a tick window. Arming mid-song (a nonzero bar_index_now)
// works exactly like arming at the start: the due point is always
// bar_index_now + (n_bars - 1), regardless of what meter was, is, or later
// becomes live in between.
void test_arm_mid_song_counts_from_the_live_bar_index() {
  BoundaryLatch latch;
  latch.arm(2, /*bar_index_now=*/41);
  CHECK(latch.due_bar_index == 42);
  CHECK(!latch.due(41));
  CHECK(latch.due(42));
}

// THE regression guard this hardening pass exists for: a meter change
// between arm() and due() cannot desynchronize a bar-COUNT latch the way it
// broke the old tick-window shape -- due() only ever compares Transport's
// own bar_index() (already meter-change-safe by construction, runtime/
// transport.hpp), never re-derives a tick length of its own. Simulating "the
// meter changed after arm()" here just means: the caller keeps advancing
// bar_index_now one bar at a time, however long each of those bars turned out
// to be in ticks -- the due tick's own TICK value is irrelevant to this
// type; only the ORDINAL bar count is.
void test_due_is_immune_to_whatever_the_meter_did_between_arm_and_due() {
  BoundaryLatch latch;
  latch.arm(1, /*bar_index_now=*/2);  // armed while bar_index was 2
  CHECK(latch.due_bar_index == 2);
  CHECK(!latch.due(2 - 1));  // hasn't reached it yet (defensive; bar_index never decreases)
  CHECK(latch.due(2));       // fires at the very next bar boundary, no matter its tick length
}

}  // namespace

int main() {
  test_default_latch_is_inert();
  test_arm_one_bar_due_at_the_next_bar_only();
  test_arm_n_bars_due_only_at_the_nth_upcoming_bar();
  test_arm_clamps_n_bars_below_one();
  test_clear_consumes_the_arm();
  test_rearm_replaces_the_previous_target();
  test_arm_mid_song_counts_from_the_live_bar_index();
  test_due_is_immune_to_whatever_the_meter_did_between_arm_and_due();
  return arrangrr::test::failures();
}

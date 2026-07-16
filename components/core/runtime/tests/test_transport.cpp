// Torquato QA (Phase 7, node 8100 hardening pass): pure unit tests for
// Transport itself -- no Engine/Runtime harness needed, mirroring
// components/arrangrr/tests/test_boundary_latch.cpp's own precedent for a
// small POD primitive. Covers the re-anchored bar-boundary gate
// (at_bar_boundary/advance_bar_tick) directly, plus start()/stop()/locate()'s
// own re-anchoring contract -- the seams node 8100 (SceneChain) and every
// other bar-gated consumer (chord commit_bar, fire_clips, Performance
// recall, pad fires) build on.

#include "runtime/transport.hpp"

#include "common/time.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// A freshly-constructed Transport already sits at the tick-0 boundary
// (m_next_bar_tick defaults to 0, matching m_tick's own default) -- so
// at_bar_boundary() is true before any start()/advance_one() call at all.
void test_default_transport_is_at_the_tick_zero_boundary() {
  Transport t;
  CHECK(t.tick() == 0);
  CHECK(t.at_bar_boundary());
}

// The stable (never-changing) 4/4 meter: advancing one bar at a time and
// calling advance_bar_tick() after each boundary reproduces EXACTLY the
// historical `tick % kTicksPerBar == 0` sequence: 0, kTicksPerBar,
// 2*kTicksPerBar, ... -- byte-identical for the default path.
void test_stable_meter_reproduces_the_absolute_modulo_sequence() {
  Transport t;
  t.start();
  CHECK(t.at_bar_boundary());  // tick 0
  t.advance_bar_tick();
  for (Tick bar = 1; bar <= 3; ++bar) {
    for (Tick i = 0; i < kTicksPerBar; ++i) {
      const bool expect_boundary = (i == kTicksPerBar - 1);  // one tick before the next multiple
      t.advance_one();
      CHECK(t.at_bar_boundary() == expect_boundary);
      if (expect_boundary) {
        CHECK(t.tick() == bar * kTicksPerBar);
        t.advance_bar_tick();
      }
    }
  }
}

// THE regression guard this whole file exists for: a mid-song meter change
// re-anchors the NEXT boundary at (transition_tick + NEW ticks_per_bar), not
// at the smallest multiple of the new ticks_per_bar counted from absolute
// tick 0 (the pre-fix bug's own worked example, transport.hpp's own header
// comment: a 4/4 song at tick 8*kTicksPerBeat switching to 3/4 must next
// land at 8q+3q=11q, never at the buggy-early 9q).
void test_meter_change_re_anchors_from_the_transition_tick_not_from_zero() {
  Transport t;
  t.start();
  t.advance_bar_tick();
  // First bar boundary (tick == kTicksPerBar), still 4/4: re-anchor normally,
  // exactly like Engine::on_tick does for every ordinary bar.
  for (Tick i = 0; i < kTicksPerBar; ++i) {
    t.advance_one();
  }
  CHECK(t.at_bar_boundary());
  CHECK(t.tick() == kTicksPerBar);
  t.advance_bar_tick();
  // Advance to the SECOND boundary (tick == 2*kTicksPerBar) but do NOT
  // re-anchor it yet -- the meter change below must land while this
  // boundary is still pending, exactly mirroring Engine::on_tick's own
  // ordering (fire_scene mutates the meter BEFORE advance_bar_tick() runs).
  for (Tick i = 0; i < kTicksPerBar; ++i) {
    t.advance_one();
  }
  CHECK(t.at_bar_boundary());
  CHECK(t.tick() == 2 * kTicksPerBar);

  // The transition: change the meter to 3/4 HERE, on this same boundary tick
  // (mirrors Engine::on_tick's own ordering -- fire_scene/apply_performance
  // mutate the meter BEFORE advance_bar_tick() re-anchors).
  CHECK(t.set_time_sig(3));
  const Tick new_ticks_per_bar = 3 * kTicksPerBeat;
  CHECK(t.ticks_per_bar() == new_ticks_per_bar);
  t.advance_bar_tick();  // re-anchors using the meter that is live NOW

  const Tick correct_next_boundary = 2 * kTicksPerBar + new_ticks_per_bar;
  const Tick buggy_early_boundary =
      ((2 * kTicksPerBar) / new_ticks_per_bar + 1) * new_ticks_per_bar;
  CHECK(buggy_early_boundary < correct_next_boundary);  // sanity: genuinely different ticks

  // Advance to the buggy-early tick: the FIXED gate must NOT report a
  // boundary there.
  while (t.tick() < buggy_early_boundary) {
    t.advance_one();
  }
  CHECK(!t.at_bar_boundary());

  // Advance the rest of the way to the meter-correct boundary: the FIXED
  // gate reports it there, exactly.
  while (t.tick() < correct_next_boundary) {
    t.advance_one();
  }
  CHECK(t.at_bar_boundary());
  CHECK(t.tick() == correct_next_boundary);
}

// start(): MIDI Start semantics -- rewinds to tick 0 AND re-seeds the gate to
// the tick-0 boundary, regardless of where the transport was before (a
// stop()/replay must not carry a stale mid-grid m_next_bar_tick forward).
void test_start_rewinds_and_reseeds_the_bar_gate() {
  Transport t;
  t.start();
  t.advance_bar_tick();
  for (Tick i = 0; i < kTicksPerBar + 17; ++i) {
    t.advance_one();  // land somewhere mid-grid, off any boundary
  }
  CHECK(!t.at_bar_boundary());
  CHECK(t.set_time_sig(5));  // also leave a non-default meter in effect

  t.start();  // MIDI Start: rewind + re-seed
  CHECK(t.tick() == 0);
  CHECK(t.playing());
  CHECK(t.at_bar_boundary());  // the gate is fresh again, at tick 0
  // start() does not touch the time signature itself (only Engine's own
  // style-load/scene-transition paths do) -- byte-identical scope to before.
  CHECK(t.time_sig().beats_per_bar == 5);
}

// stop(): only the run state changes -- position and the bar gate are left
// exactly where they were (MIDI Stop is not a rewind).
void test_stop_leaves_position_and_gate_untouched() {
  Transport t;
  t.start();
  t.advance_bar_tick();
  for (Tick i = 0; i < 100; ++i) {
    t.advance_one();
  }
  const Tick tick_before = t.tick();
  t.stop();
  CHECK(!t.playing());
  CHECK(t.tick() == tick_before);
  CHECK(!t.at_bar_boundary());  // the gate itself is unperturbed by stop()
}

// locate(): a direct position jump (e.g. a host seek) is always treated as a
// FRESH bar start, mirroring start()'s own tick/next-bar-tick pairing -- the
// jumped-to tick itself immediately reads as a boundary, and the NEXT
// boundary is one full (live) bar later, not some stale offset surviving
// from before the jump.
void test_locate_re_anchors_the_gate_at_the_new_position() {
  Transport t;
  t.start();
  t.advance_bar_tick();
  for (Tick i = 0; i < 10; ++i) {
    t.advance_one();  // tick 10, mid-bar, NOT a boundary
  }
  CHECK(!t.at_bar_boundary());

  constexpr Tick kLocateTarget = 5000;  // an arbitrary mid-song tick
  t.locate(kLocateTarget);
  CHECK(t.tick() == kLocateTarget);
  CHECK(t.at_bar_boundary());  // the jump itself reads as a fresh bar start

  t.advance_bar_tick();
  for (Tick i = 0; i < kTicksPerBar - 1; ++i) {
    t.advance_one();
    CHECK(!t.at_bar_boundary());
  }
  t.advance_one();  // exactly one live bar after the locate target
  CHECK(t.at_bar_boundary());
  CHECK(t.tick() == kLocateTarget + kTicksPerBar);
}

}  // namespace

int main() {
  test_default_transport_is_at_the_tick_zero_boundary();
  test_stable_meter_reproduces_the_absolute_modulo_sequence();
  test_meter_change_re_anchors_from_the_transition_tick_not_from_zero();
  test_start_rewinds_and_reseeds_the_bar_gate();
  test_stop_leaves_position_and_gate_untouched();
  test_locate_re_anchors_the_gate_at_the_new_position();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_transport: all OK\n");
  }
  return arrangrr::test::failures();
}

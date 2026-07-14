// Torquato QA (Phase 7, node T0 hardening pass): pins a genuine behavioral
// DIVERGENCE the variable time-signature engine introduces between the two
// "arm now, fire at a boundary" primitives the codebase treats as siblings.
//
// BoundaryLatch::arm(n_bars, ticks_per_bar) computes `window = n_bars *
// ticks_per_bar` ONCE, at arm time, and stores it (boundary_latch.hpp:
// `Tick window`). due(t) only ever reads that stored field -- it takes no
// ticks_per_bar parameter at all, so a later change to the transport's live
// meter can NEVER retroactively move a latch that is already armed
// (test_boundary_latch.cpp's own
// test_arm_window_is_frozen_at_arm_time_immune_to_a_later_meter_change proves
// this directly).
//
// ClipMatrix::on_bar(transport_tick, on_due, ticks_per_bar) does NOT snapshot
// anything at arm() time -- arm() only records `n_bars` on the Clip; on_bar
// RECOMPUTES `window = c.n_bars * ticks_per_bar` fresh on EVERY call, using
// whatever ticks_per_bar Engine::fire_clips threads in THAT tick
// (m_transport.ticks_per_bar(), read live). ClipMatrix's own header comment
// describes this pairing as mirroring BoundaryLatch's "per-clip n_bars window
// check" (clip_matrix.hpp's on_bar doc comment) -- but the two now behave
// DIFFERENTLY the moment the transport's time signature changes while a
// multi-bar clip sits armed: a clip requested for "the next 2 bars" is not
// actually anchored to 2 bars from when it was armed -- it is anchored to
// "the next tick, from NOW, whose absolute position is a multiple of
// n_bars * (whatever the CURRENT live ticks_per_bar happens to be)". A
// Performance recall (or a style switch) that changes beats_per_bar while a
// clip is still counting down retunes that clip's fire point to an entirely
// different tick, with no relationship to the bar count the caller actually
// armed for.
//
// Concrete minimized reproducer (all values below are exact, no rounding):
//   - A clip is armed for n_bars = 2 while the meter is a stable 4/4
//     (ticks_per_bar = kTicksPerBar = 3840). Held under a STABLE meter, the
//     clip is due at tick 2 * 3840 = 7680 (test_clip_matrix.cpp's own
//     test_on_bar_respects_n_bars_multi_bar_window pins exactly this).
//   - Before that tick arrives, the meter changes to a 3-beat bar (2880
//     ticks) -- e.g. an immediate Performance recall lands one bar in
//     (Engine::apply_performance -> Transport::set_time_sig, reachable via
//     the ABI's kPerformanceRecall verb).
//   - Engine only ever calls ClipMatrix::on_bar at ticks that are multiples
//     of the transport's OWN live bar length (the on_tick gate,
//     `m_transport.tick() % m_transport.ticks_per_bar() == 0`) -- once the
//     meter is 3-beat, the next such tick is 5760 (2 * 2880), not 7680.
//   - At tick 5760, on_bar recomputes window = 2 * 2880 = 5760 for THIS
//     clip -- 5760 % 5760 == 0 -- and fires it, 1920 ticks (exactly two
//     4/4 beats) before the tick a stable meter would have produced, and
//     with NO relationship to "2 bars from when it was armed" left at all.
//
// This is exactly the class of race Torquato's mandate exists to catch: a
// clip's own quantize primitive silently reacts to a DIFFERENT subsystem's
// state (Transport::set_time_sig) the caller who armed it never touched.
// Handoff to Nazzareno: NOT fixed here (accuse, don't repair) -- either
// ClipMatrix::on_bar should snapshot its window at arm() time exactly like
// BoundaryLatch already does (the two primitives would then genuinely
// mirror each other again), or the owner may decide the live-recompute is
// the INTENDED behavior for clips specifically -- but that intent is not
// stated anywhere in docs/reflections/phase7-design-variable-timesig-engine.md,
// which discusses ClipMatrix::on_bar and BoundaryLatch::arm as receiving
// "the SAME threaded ticks_per_bar" without ever discussing what happens when
// that threaded value changes between an arm() and its own fire.

#include "arrangrr/clip/clip_matrix.hpp"

#include "common/time.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

constexpr Tick kFourBeatBar = kTicksPerBar;                 // 3840: the stable-4/4 window
constexpr Tick kThreeBeatBar = 3 * kTicksPerBeat;           // 2880: the meter it changes to
constexpr Tick kStableExpectedFireTick = 2 * kFourBeatBar;  // 7680: "2 bars from arm", stable meter
constexpr Tick kActualEarlyFireTick = 2 * kThreeBeatBar;    // 5760: where it ACTUALLY fires

// RED (Torquato QA finding, Phase 7 node T0): armed for "the next 2 bars"
// under a stable 4/4, this clip fires 1920 ticks EARLY the moment the meter
// changes to 3/4 one bar into the count-down -- because ClipMatrix::on_bar
// recomputes its window from whatever live ticks_per_bar Engine threads in
// on each call, rather than freezing it at arm() time the way BoundaryLatch
// does (see this file's own header comment for the full trace).
void test_on_bar_window_is_retuned_by_a_live_meter_change_mid_arm() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, /*n_bars=*/2));

  // Bar 1, still 4/4 (Engine's own bar-boundary gate only ever calls on_bar
  // at multiples of the CURRENT live ticks_per_bar -- 3840 here).
  int calls = 0;
  clips.on_bar(kFourBeatBar, [&](std::size_t, const Clip&) { ++calls; }, kFourBeatBar);
  CHECK(calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kArmed);  // still counting down, as expected

  // The meter now changes to a 3-beat bar (e.g. an immediate Performance
  // recall lands right here) -- BEFORE this clip's own 2-bar window (7680)
  // has elapsed. Engine's bar-boundary gate now fires at multiples of 2880;
  // the next one is 5760, not 7680.
  clips.on_bar(kActualEarlyFireTick, [&](std::size_t, const Clip&) { ++calls; }, kThreeBeatBar);

  // FINDING: this fails. The clip fires HERE (calls becomes 1, state flips
  // to kPlaying) even though NEITHER the clip NOR the caller ever asked for
  // "2 bars of 2880 ticks" -- it was armed for "2 bars", under a meter that
  // was 3840 ticks per bar at the time. A stable-meter-invariant quantize
  // primitive (the property BoundaryLatch already guarantees, see
  // test_boundary_latch.cpp's
  // test_arm_window_is_frozen_at_arm_time_immune_to_a_later_meter_change)
  // would leave this clip armed until kStableExpectedFireTick (7680), not
  // fire it at kActualEarlyFireTick (5760).
  CHECK(calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kArmed);
}

// Baseline (GREEN, for contrast): under a meter that never changes, the
// SAME arm(0, kPlaying, 2) genuinely does fire at kStableExpectedFireTick
// (7680) and nowhere earlier -- proving the "2 bars from arm" expectation is
// exactly what a STABLE meter already delivers today; it is a live meter
// CHANGE mid-arm (the scenario above) that breaks it, not the n_bars*
// ticks_per_bar arithmetic itself.
void test_on_bar_fires_at_the_expected_tick_when_the_meter_never_changes() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, /*n_bars=*/2));
  int calls = 0;
  clips.on_bar(kFourBeatBar, [&](std::size_t, const Clip&) { ++calls; }, kFourBeatBar);
  CHECK(calls == 0);
  clips.on_bar(kStableExpectedFireTick, [&](std::size_t, const Clip&) { ++calls; }, kFourBeatBar);
  CHECK(calls == 1);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
}

}  // namespace

int main() {
  test_on_bar_fires_at_the_expected_tick_when_the_meter_never_changes();
  test_on_bar_window_is_retuned_by_a_live_meter_change_mid_arm();
  return arrangrr::test::failures();
}

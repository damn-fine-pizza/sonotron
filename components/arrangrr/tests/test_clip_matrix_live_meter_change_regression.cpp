// Torquato QA (Phase 7, node T0 hardening pass, SUPERSEDED by node 8100
// hardening): originally pinned a genuine behavioral divergence between the
// codebase's two "arm now, fire at a boundary" primitives under a live meter
// change -- BoundaryLatch froze its quantize window at arm() time, but
// ClipMatrix::on_bar recomputed `n_bars * ticks_per_bar` fresh on EVERY call
// from whatever live ticks_per_bar Engine threaded in, so a meter change
// landing mid-countdown silently retuned an already-armed clip to an
// unrelated fire tick.
//
// Commit 28864bc fixed THAT specific divergence (freeze the window on the
// FIRST on_bar() check since arm()). Phase 7 node 8100 hardening (Torquato QA
// finding F3) then found 28864bc's fix was still incomplete: freezing a TICK
// window on the first check is still an absolute-tick match, so a meter
// change that happened BEFORE arm() (not just mid-countdown) left the frozen
// remainder unreachable forever. clip_matrix.hpp's whole mechanism was
// replaced with a bar-COUNT due point (Clip::due_bar_index, frozen directly
// at arm() time from Transport::bar_index()) -- see that header's own
// comment for the full argument. This file now pins the INTENT the original
// title promised (a clip's own quantize primitive is immune to whatever the
// meter does around it) against the new bar-count API: since ClipMatrix no
// longer consumes a tick length AT ALL, there is no live value left for a
// meter change to retune in the first place.

#include "arrangrr/clip/clip_matrix.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

// Armed for "the next 2 bars", the clip must NOT fire on the 1st upcoming
// bar and MUST fire exactly on the 2nd -- regardless of how long each of
// those bars would have been in ticks under any particular meter, since
// Clip::due_bar_index never reads a tick length at all.
void test_on_bar_fires_on_the_nth_upcoming_bar_not_before() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, /*n_bars=*/2, /*bar_index_now=*/0));

  int calls = 0;
  clips.on_bar([&](std::size_t, const Clip&) { ++calls; }, /*bar_index_now=*/0);
  CHECK(calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kArmed);  // still counting down

  clips.on_bar([&](std::size_t, const Clip&) { ++calls; }, /*bar_index_now=*/1);
  CHECK(calls == 1);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
}

// The regression guard this file exists for: arming, then observing several
// bar-index advances that would have spanned WILDLY different tick counts
// under a live meter (e.g. a 4/4 bar then a 3/4 bar) changes NOTHING about
// when this clip fires -- it only ever counts bar_index_now advances, never
// a tick span, so there is no "live ticks_per_bar" left anywhere in this
// class for any subsystem's meter change to retune.
void test_on_bar_is_immune_to_whatever_the_meter_would_have_done_in_between() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  // Armed mid-song (bar_index_now=5, as if several bars -- of whatever
  // lengths -- already played under one or more prior meters).
  CHECK(clips.arm(0, LaunchState::kPlaying, /*n_bars=*/3, /*bar_index_now=*/5));

  int calls = 0;
  clips.on_bar([&](std::size_t, const Clip&) { ++calls; }, /*bar_index_now=*/5);
  CHECK(calls == 0);
  clips.on_bar([&](std::size_t, const Clip&) { ++calls; }, /*bar_index_now=*/6);
  CHECK(calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kArmed);  // 2 of 3 armed bars elapsed
  clips.on_bar([&](std::size_t, const Clip&) { ++calls; }, /*bar_index_now=*/7);
  CHECK(calls == 1);  // the 3rd upcoming bar, exactly -- no earlier, no later
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
}

}  // namespace

int main() {
  test_on_bar_fires_on_the_nth_upcoming_bar_not_before();
  test_on_bar_is_immune_to_whatever_the_meter_would_have_done_in_between();
  return arrangrr::test::failures();
}

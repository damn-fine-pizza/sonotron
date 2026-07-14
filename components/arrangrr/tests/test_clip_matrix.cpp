// Unit tests for ClipMatrix (Phase-5 Item #2, docs/design/
// clip-primitive-design.md): pure bookkeeping -- add()/get()/arm()/force()/
// on_bar() -- mirroring PadEngine's own scope tripwire (no dispatch logic
// lives here; Engine::apply_clip_content/fire_clips, exercised by
// test_clip.cpp, own that).
//
// Phase 6 Theme 1b (Torquato QA, coverage-gate restoration): this file was
// entirely missing before -- ClipMatrix sat at 7% line coverage under the
// unit gate despite being just as pure/freestanding as pad_bank.hpp.
//
// All on_bar() tests share the SAME OnBarRecorder callback type below
// (instead of an ad-hoc lambda per test) so gcov measures ONE template
// instantiation of on_bar<Fn> across every call site, not one per distinct
// lambda type -- a distinct lambda per test function is a different type,
// hence a SEPARATE template instantiation each gcov counts independently,
// which artificially dilutes the aggregate branch-coverage percentage
// without adding any real test value. Consolidating is a measurement
// hygiene fix, not a behavior change: every scenario below is still
// exercised exactly as designed.

#include "arrangrr/clip/clip_matrix.hpp"

#include "arrangrr/config.hpp"
#include "common/time.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

struct OnBarRecorder {
  int calls = 0;
  std::size_t last_id = 999;
  LaunchState last_state = LaunchState::kStopped;
  void operator()(std::size_t id, const Clip& c) {
    ++calls;
    last_id = id;
    last_state = c.state;  // the callback observes the ALREADY-promoted state
  }
};

void test_add_returns_sequential_ids() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 1) == 0);
  CHECK(clips.add(TrackRole::kBass, 1, ContentKind::kChordSequence, 2) == 1);
  CHECK(clips.add(TrackRole::kChord1, 2, ContentKind::kStepTrack, 3) == 2);
  CHECK(clips.size() == 3);
}

void test_get_round_trip() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kBass, 7, ContentKind::kChordSequence, 42) == 0);
  const Clip* c = clips.get(0);
  CHECK(c != nullptr);
  CHECK(c->part_role == TrackRole::kBass);
  CHECK(c->scene_index == 7);
  CHECK(c->kind == ContentKind::kChordSequence);
  CHECK(c->content_index == 42);
  CHECK(c->state == LaunchState::kStopped);  // fresh clip: inert default
  CHECK(c->n_bars == 1);
}

void test_get_out_of_range_returns_null() {
  ClipMatrix clips;
  CHECK(clips.get(0) == nullptr);  // empty pool
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.get(1) == nullptr);  // one past the only registered clip
}

void test_add_beyond_pool_capacity_fails() {
  ClipMatrix clips;
  for (std::size_t i = 0; i < kMaxClips; ++i) {
    CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == static_cast<int>(i));
  }
  CHECK(clips.size() == kMaxClips);
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == -1);  // pool full
  CHECK(clips.size() == kMaxClips);                                             // unchanged
}

void test_arm_unknown_id_fails() {
  ClipMatrix clips;
  CHECK(!clips.arm(0, LaunchState::kPlaying, 1));  // empty pool
}

void test_arm_toward_playing_sets_armed() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, 2));
  const Clip* c = clips.get(0);
  CHECK(c->state == LaunchState::kArmed);
  CHECK(c->n_bars == 2);
}

void test_arm_toward_stopped_sets_queued_stop() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kStopped, 1));
  CHECK(clips.get(0)->state == LaunchState::kQueuedStop);
}

void test_arm_clamps_zero_n_bars_to_one() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, 0));
  CHECK(clips.get(0)->n_bars == 1);
}

void test_force_unknown_id_fails() {
  ClipMatrix clips;
  CHECK(!clips.force(0, LaunchState::kPlaying));
}

void test_force_sets_state_directly_no_arming() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.force(0, LaunchState::kPlaying));
  CHECK(clips.get(0)->state == LaunchState::kPlaying);  // direct, never kArmed
}

void test_on_bar_does_not_fire_before_the_boundary() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, 1));
  OnBarRecorder rec;
  clips.on_bar(kTicksPerBar / 2, rec);
  CHECK(rec.calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kArmed);  // still pending
}

void test_on_bar_promotes_armed_to_playing_at_the_boundary() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, 1));
  OnBarRecorder rec;
  clips.on_bar(kTicksPerBar, rec);
  CHECK(rec.calls == 1);
  CHECK(rec.last_id == 0);
  CHECK(rec.last_state == LaunchState::kPlaying);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
}

void test_on_bar_promotes_queued_stop_to_stopped() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.force(0, LaunchState::kPlaying));  // sounding first
  CHECK(clips.arm(0, LaunchState::kStopped, 1));
  OnBarRecorder rec;
  clips.on_bar(kTicksPerBar, rec);
  CHECK(rec.calls == 1);
  CHECK(rec.last_state == LaunchState::kStopped);
  CHECK(clips.get(0)->state == LaunchState::kStopped);
}

void test_on_bar_ignores_already_settled_clips() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.force(0, LaunchState::kPlaying));  // settled, not pending
  OnBarRecorder rec;
  clips.on_bar(kTicksPerBar, rec);
  CHECK(rec.calls == 0);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);  // unchanged
}

void test_on_bar_respects_n_bars_multi_bar_window() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.arm(0, LaunchState::kPlaying, /*n_bars=*/2));
  OnBarRecorder rec_one_bar;
  clips.on_bar(kTicksPerBar, rec_one_bar);
  CHECK(rec_one_bar.calls == 0);  // 1 bar is NOT a multiple of the 2-bar window
  CHECK(clips.get(0)->state == LaunchState::kArmed);
  OnBarRecorder rec_two_bars;
  clips.on_bar(2 * kTicksPerBar, rec_two_bars);
  CHECK(rec_two_bars.calls == 1);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
}

void test_on_bar_promotes_multiple_independent_clips_in_one_call() {
  ClipMatrix clips;
  CHECK(clips.add(TrackRole::kDrums, 0, ContentKind::kStyleSection, 0) == 0);
  CHECK(clips.add(TrackRole::kBass, 1, ContentKind::kChordSequence, 5) == 1);
  CHECK(clips.arm(0, LaunchState::kPlaying, 1));
  CHECK(clips.arm(1, LaunchState::kStopped, 1));
  OnBarRecorder rec;
  clips.on_bar(kTicksPerBar, rec);
  CHECK(rec.calls == 2);
  CHECK(clips.get(0)->state == LaunchState::kPlaying);
  CHECK(clips.get(1)->state == LaunchState::kStopped);
}

}  // namespace

int main() {
  test_add_returns_sequential_ids();
  test_get_round_trip();
  test_get_out_of_range_returns_null();
  test_add_beyond_pool_capacity_fails();

  test_arm_unknown_id_fails();
  test_arm_toward_playing_sets_armed();
  test_arm_toward_stopped_sets_queued_stop();
  test_arm_clamps_zero_n_bars_to_one();

  test_force_unknown_id_fails();
  test_force_sets_state_directly_no_arming();

  test_on_bar_does_not_fire_before_the_boundary();
  test_on_bar_promotes_armed_to_playing_at_the_boundary();
  test_on_bar_promotes_queued_stop_to_stopped();
  test_on_bar_ignores_already_settled_clips();
  test_on_bar_respects_n_bars_multi_bar_window();
  test_on_bar_promotes_multiple_independent_clips_in_one_call();

  return arrangrr::test::failures();
}

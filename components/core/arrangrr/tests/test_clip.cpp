// Functional tests for the clip launch primitive (Phase-5 Item #2,
// docs/design/clip-primitive-design.md): ClipMatrix's bookkeeping is pure
// POD state, but the musically-observable behavior only exists through
// Engine's cmd_clip/apply_clip_content/fire_clips wiring, so this drives the
// real ABI (kClipAdd/kClipLaunch/kClipStop/kSceneQuantize) through an Engine,
// exactly like test_input_zone/test_chord_seq_vs_live_steer -> functional.

#include "arrangrr/clip/clip_matrix.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = Op::kDo;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    e.push_command(Command{.op = Op::kSet, .param = Param::kKeySet, .a = 0, .b = 0},
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8));
  }
  // a = TrackRole part_role, b = scene_index, c = ContentKind | (content_index << 8).
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8));
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  const OutEvent* last_clip_event() const {
    const OutEvent* last = nullptr;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kClip) {
        last = &o;
      }
    }
    return last;
  }
  int clip_event_count() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kClip) {
        ++n;
      }
    }
    return n;
  }
  SectionType current_section() const { return e.arranger().current(); }
};

void test_clip_add_assigns_sequential_ids() {
  Band b;
  b.setup_basic();
  CHECK(b.e.clips().size() == 0);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarA));
  b.add_clip(TrackRole::kBass, 1, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.e.clips().size() == 2);
  CHECK(b.warns() == 0);
  const Clip* c0 = b.e.clips().get(0);
  const Clip* c1 = b.e.clips().get(1);
  CHECK(c0 != nullptr && c0->scene_index == 0 && c0->kind == ContentKind::kStyleSection);
  CHECK(c1 != nullptr && c1->scene_index == 1);
}

void test_clip_launch_immediate_style_section() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  // idx = clip id 0, boundary defaults to kImmediate.
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  CHECK(b.current_section() == SectionType::kVarB);
  const OutEvent* ce = b.last_clip_event();
  CHECK(ce != nullptr && ce->code == 0 &&
        ce->msg.status == static_cast<std::uint8_t>(LaunchState::kPlaying));
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

void test_clip_launch_quantized_next_bar() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0, Boundary::kNextBar);
  // Armed, not yet playing: no section change, one kClip echo (armed).
  CHECK(b.current_section() == SectionType::kVarA);
  CHECK(b.clip_event_count() == 1);
  CHECK(b.last_clip_event()->msg.status == static_cast<std::uint8_t>(LaunchState::kArmed));
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);

  b.ev.clear();
  b.advance(kTicksPerBar);  // cross the bar boundary: the clip fires
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  bool saw_playing = false;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kClip &&
        o.msg.status == static_cast<std::uint8_t>(LaunchState::kPlaying)) {
      saw_playing = true;
    }
  }
  CHECK(saw_playing);
}

void test_clip_launch_next_n_bars() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0, Boundary::kNextNBars, /*n_bars=*/2);
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);
  CHECK(b.e.clips().get(0)->n_bars == 2);

  b.advance(kTicksPerBar);  // one bar: not due yet (window is 2 bars)
  CHECK(b.current_section() == SectionType::kVarA);
  CHECK(b.e.clips().get(0)->state == LaunchState::kArmed);

  b.advance(kTicksPerBar);  // second bar boundary: due
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

// A step-track clip generalizes the design's "arm a pre-existing pattern to
// start"/"stop" primitive to a domain with NO play/stop state of its own
// (Timeline::Track has only mute/solo) -- launch un-mutes the track, stop
// re-mutes it, exactly as the clip-primitive design's "Key structural
// finding" describes.
void test_clip_stop_step_track_mutes() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTrackNew, static_cast<std::int32_t>(TrackRole::kLead), 0 | (3 << 8));
  const std::size_t track_idx = b.e.timeline().track_count() - 1;
  CHECK(!b.e.timeline().track(track_idx)->mute);  // fresh track starts un-muted

  b.add_clip(TrackRole::kLead, 0, ContentKind::kStepTrack, static_cast<std::uint16_t>(track_idx));
  b.cmd(Param::kTransportStart);
  b.ev.clear();

  // stop clip 0, immediate -> mutes the track.
  b.cmd(Param::kClipStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.timeline().track(track_idx)->mute);
  CHECK(b.e.clips().get(0)->state == LaunchState::kStopped);

  // launch clip 0, immediate -> un-mutes the track again.
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  CHECK(!b.e.timeline().track(track_idx)->mute);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
}

void test_clip_scene_launch_fans_out() {
  Band b;
  b.setup_basic();
  b.add_clip(TrackRole::kBass, 2, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTrackNew, static_cast<std::int32_t>(TrackRole::kLead), 0 | (3 << 8));
  const std::size_t track_idx = b.e.timeline().track_count() - 1;
  b.add_clip(TrackRole::kLead, 2, ContentKind::kStepTrack, static_cast<std::uint16_t>(track_idx));
  // A clip in a DIFFERENT scene must not fire.
  b.add_clip(TrackRole::kChord1, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarC));

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  // `launch scene 2 quantize 0` (immediate): idx = scene index.
  b.cmd(Param::kSceneQuantize, 0, 0, 0, /*idx=*/2);
  CHECK(b.current_section() == SectionType::kVarB);
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);
  CHECK(b.e.clips().get(1)->state == LaunchState::kPlaying);
  CHECK(b.e.clips().get(2)->state == LaunchState::kStopped);  // untouched, different scene
  CHECK(b.warns() == 0);
}

void test_clip_bad_id_warns() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);  // no clip registered at all
  CHECK(b.warns() == 1);
  CHECK(b.clip_event_count() == 0);
}

void test_clip_add_bad_role_warns() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kClipAdd, /*role*/ 99, /*scene*/ 0, /*kind|content*/ 0);
  CHECK(b.warns() == 1);
  CHECK(b.e.clips().size() == 0);
}

}  // namespace

int main() {
  test_clip_add_assigns_sequential_ids();
  test_clip_launch_immediate_style_section();
  test_clip_launch_quantized_next_bar();
  test_clip_launch_next_n_bars();
  test_clip_stop_step_track_mutes();
  test_clip_scene_launch_fans_out();
  test_clip_bad_id_warns();
  test_clip_add_bad_role_warns();
  return arrangrr::test::failures();
}

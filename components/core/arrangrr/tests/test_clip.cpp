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
  // idx = kNoExplicitClipId: the legacy sequential-append form (Repeat-Zone
  // binding contract Shape A, abi.hpp's kClipAdd comment) -- explicit here
  // since Command::idx now means "explicit clip id" for kClipAdd, and every
  // caller of THIS helper relies on the ORIGINAL sequential-id assignment
  // (test_clip_add_assigns_sequential_ids below would otherwise collide on
  // its second call).
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8),
        kNoExplicitClipId);
  }
  // The new Shape-A path: registers AT an explicit id instead of the
  // sequential counter.
  void add_clip_at(std::uint16_t id, TrackRole role, std::uint8_t scene, ContentKind kind,
                   std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8), id);
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

// Repeat-Zone binding contract, Shape A (docs/proposals/repeat-zone-real-
// contract.md §3/§8b decision 1): registering at an explicit id past the
// pool's tail pads every intervening index with an unclaimed placeholder
// (never returned by get(), see clip_matrix.hpp), then claims exactly `id`.
void test_clip_add_at_explicit_id_registers() {
  Band b;
  b.setup_basic();
  b.add_clip_at(5, TrackRole::kBass, 2, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.warns() == 0);
  const Clip* c5 = b.e.clips().get(5);
  CHECK(c5 != nullptr && c5->part_role == TrackRole::kBass && c5->scene_index == 2);
  // Every padded id below 5 stays unclaimed -- get() reports "no clip here".
  CHECK(b.e.clips().get(0) == nullptr);
  CHECK(b.e.clips().get(4) == nullptr);
}

// A later, LOWER explicit id (arriving after a higher one already padded
// through it -- the real GUI shape: cells fill in whatever order the user
// drags styles onto them, not row-major id order) still succeeds and fills
// exactly its own placeholder slot, leaving every other still-unclaimed
// index untouched.
void test_clip_add_at_out_of_order_fills_earlier_placeholder() {
  Band b;
  b.setup_basic();
  b.add_clip_at(10, TrackRole::kLead, 1, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarA));
  b.add_clip_at(2, TrackRole::kChord1, 0, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarC));
  CHECK(b.warns() == 0);
  const Clip* c2 = b.e.clips().get(2);
  const Clip* c10 = b.e.clips().get(10);
  CHECK(c2 != nullptr && c2->part_role == TrackRole::kChord1);
  CHECK(c10 != nullptr && c10->part_role == TrackRole::kLead);
  CHECK(b.e.clips().get(5) == nullptr);  // still an unclaimed placeholder
}

// Re-registering the SAME explicit id is rejected (ClipMatrix stays
// append-only, no retarget, §8b decision 1) -- the original registration is
// left untouched.
void test_clip_add_at_duplicate_id_warns() {
  Band b;
  b.setup_basic();
  b.add_clip_at(3, TrackRole::kBass, 0, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarA));
  b.ev.clear();
  b.add_clip_at(3, TrackRole::kLead, 4, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarC));
  CHECK(b.warns() == 1);
  const Clip* c3 = b.e.clips().get(3);
  CHECK(c3 != nullptr && c3->part_role == TrackRole::kBass && c3->scene_index == 0);
}

// An out-of-bounds explicit id (>= kMaxClips) is rejected cleanly, no crash.
void test_clip_add_at_out_of_bounds_warns() {
  Band b;
  b.setup_basic();
  b.add_clip_at(static_cast<std::uint16_t>(kMaxClips), TrackRole::kBass, 0,
                ContentKind::kStyleSection, static_cast<std::uint16_t>(SectionType::kVarA));
  CHECK(b.warns() == 1);
  CHECK(b.e.clips().get(kMaxClips) == nullptr);
}

// The full Shape-A round trip: an explicit-id registration is launchable and
// emits the same real kClip readback as a sequentially-assigned clip.
void test_clip_add_at_then_launch_and_readback() {
  Band b;
  b.setup_basic();
  b.add_clip_at(7, TrackRole::kBass, 0, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/7);
  CHECK(b.current_section() == SectionType::kVarB);
  const OutEvent* ce = b.last_clip_event();
  CHECK(ce != nullptr && ce->code == 7 &&
        ce->msg.status == static_cast<std::uint8_t>(LaunchState::kPlaying));
  CHECK(b.e.clips().get(7)->state == LaunchState::kPlaying);
  CHECK(b.warns() == 0);
}

// A scene fan-out (`launch scene <n>`) must never fire an add_at() padding
// placeholder: its default scene_index (0) would otherwise falsely match a
// scene-0 launch. get()'s m_used gate (clip_matrix.hpp) already excludes it;
// this proves the exclusion holds through Engine's own clip_scene_launch.
void test_clip_scene_launch_skips_unclaimed_placeholder() {
  Band b;
  b.setup_basic();
  // Pads ids 0..9 as unclaimed placeholders (default scene_index == 0);
  // the real clip at 10 lives on a DIFFERENT scene.
  b.add_clip_at(10, TrackRole::kBass, 1, ContentKind::kStyleSection,
                static_cast<std::uint16_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kSceneQuantize, 0, 0, 0, /*idx=*/0);  // launch scene 0
  CHECK(b.warns() == 1);                             // no real clip matched scene 0
  CHECK(b.clip_event_count() == 0);
  CHECK(b.e.clips().get(10)->state == LaunchState::kStopped);  // untouched
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
  test_clip_add_at_explicit_id_registers();
  test_clip_add_at_out_of_order_fills_earlier_placeholder();
  test_clip_add_at_duplicate_id_warns();
  test_clip_add_at_out_of_bounds_warns();
  test_clip_add_at_then_launch_and_readback();
  test_clip_scene_launch_skips_unclaimed_placeholder();
  return arrangrr::test::failures();
}

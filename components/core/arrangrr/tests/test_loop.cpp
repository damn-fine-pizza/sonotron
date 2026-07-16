// Functional tests for node 6000 (the Looper, Phase 7 SLICE 1 --
// docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md): LoopBuffer
// itself is exercised at the unit level (test_loop_event.cpp); the
// musically-observable record/overdub/erase/undo/launch behavior only
// exists through Engine's cmd_loop/apply_clip_content/fire_loop wiring, so
// this drives the real ABI (kLoopNew/kLoopRecordStart/kLoopRecordStop/
// kLoopErase/kLoopUndo/kLoopLength, plus the EXISTING kClipAdd/kClipLaunch/
// kClipStop for ContentKind::kLoopBuffer) through an Engine, exactly like
// test_clip.cpp -> functional.

#include "arrangrr/loop/loop_buffer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 1024>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = op;
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
  // Raw NoteOn(vel>0)/NoteOff(vel==0) bytes on `port`, channel 0 -- mirrors
  // test_arp_functional.cpp's own push_midi_in convention.
  void feed_note(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t b[3] = {0x90, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(b, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // idx = kNoExplicitClipId: the legacy sequential-append form (Repeat-Zone
  // binding contract Shape A, abi.hpp's kClipAdd comment) -- explicit here
  // since Command::idx now means "explicit clip id" for kClipAdd, and this
  // helper's callers rely on the ORIGINAL sequential-id assignment.
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8),
        kNoExplicitClipId);
  }
  // idx = kNoLoopExplicitId: the legacy sequential-append form (docs/
  // proposals/looper-in-gui-contract.md §7 item 5, the IDENTICAL fix already
  // shipped for kClipAdd/kNoExplicitClipId above) -- explicit here since
  // Command::idx now means "explicit loop-slot id" for kLoopNew, and every
  // bare `loop_new()` caller below relies on the ORIGINAL sequential-id
  // assignment.
  void loop_new() { cmd(Param::kLoopNew, 0, 0, 0, kNoLoopExplicitId); }
  // The new Shape-A path: registers AT an explicit id instead of the
  // sequential counter (mirrors test_clip.cpp's own add_clip_at).
  void loop_new_at(std::uint16_t id) { cmd(Param::kLoopNew, 0, 0, 0, id); }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  int count_midi(std::uint8_t port, std::uint8_t channel, std::uint8_t note, bool on) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port && o.msg.channel() == channel &&
          o.msg.d1 == note && (o.msg.type() == midi::kNoteOn) == on) {
        ++n;
      }
    }
    return n;
  }
};

void test_loop_new_and_clip_add() {
  Band b;
  b.loop_new();
  CHECK(b.e.loops().count() == 1);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  CHECK(b.e.clips().size() == 1);
  const Clip* c = b.e.clips().get(0);
  CHECK(c != nullptr && c->kind == ContentKind::kLoopBuffer && c->content_index == 0);
  CHECK(b.warns() == 0);
}

// The critical path: record -> stop (quantize-after) -> launch, and the
// captured note (chord-tone-relative, Fork A) resolves and sounds on the
// launching clip's own routed port/channel.
void test_loop_record_stop_and_launch_round_trip() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);  // C major key -> Cmaj7, root_pc 0
  b.ev.clear();
  b.loop_new();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  // Record on input port 3 (arbitrary, distinct from the playback route).
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  CHECK(b.e.loops().recording() && b.e.loops().recording_slot() == 0);
  b.feed_note(3, 64, 100);  // E: chord-tone index 1 of Cmaj7 (offset 4), rel tick 0
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);  // release after exactly one bar
  b.cmd(Param::kLoopRecordStop, /*grid=*/0, 0, 0, /*idx=*/0);
  CHECK(!b.e.loops().recording());
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).start == 0 && clip->event(0).duration == kTicksPerBar);
  CHECK(clip->event(0).source == LoopNoteSource::kChordTone);
  CHECK(clip->loop);  // captured loops repeat by default

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(kTicksPerBar);  // one full cycle: on at 0, off (+ the next cycle's on) at the wrap
  CHECK(b.count_midi(1, 2, 64, /*on=*/true) >= 1);
  CHECK(b.count_midi(1, 2, 64, /*on=*/false) >= 1);
  CHECK(b.warns() == 0);
}

void test_loop_overdub_merges_without_clearing() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 1);

  // Overdub: the FIRST note must survive; a second note is ADDED, not a
  // replacement (unlike ChordSequence::record, which always clears first).
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kOverdub), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 67, 100);
  b.advance(kTicksPerBar / 2);
  b.feed_note(3, 67, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 2);
}

void test_loop_undo_restores_prior_generation() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 1);

  // A second recording (kReplace) clears the slot -- undo must restore the
  // FIRST generation's single event.
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kReplace), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 62, 100);
  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 2);

  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().get(0)->count() == 1);  // back to the FIRST generation

  // Single generation: undo again finds nothing left to restore.
  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 1);
}

void test_loop_erase_clears_and_undo_restores() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 1);

  b.ev.clear();
  b.cmd(Param::kLoopErase, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().get(0)->count() == 0);

  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 1);  // the erased content comes back
}

// 6400: loop length modes, driven through kLoopLength.
void test_loop_length_modes() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar / 2);
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);  // ~half a bar of content

  // kFixed: pin an explicit tick length regardless of content.
  b.cmd(Param::kLoopLength, static_cast<std::int32_t>(LoopLengthMode::kFixed),
        static_cast<std::int32_t>(4 * kTicksPerBar), 0, /*idx=*/0, Op::kSet);
  CHECK(b.e.loops().get(0)->length() == 4 * kTicksPerBar);

  // kQuantized: snap the content length up to the grid.
  b.cmd(Param::kLoopLength, static_cast<std::int32_t>(LoopLengthMode::kQuantized),
        static_cast<std::int32_t>(kTicksPerBar), 0, /*idx=*/0, Op::kSet);
  CHECK(b.e.loops().get(0)->length() == kTicksPerBar);  // half a bar rounds up to 1

  // Bad args: kFixed with a non-positive length warns and leaves state alone.
  b.ev.clear();
  b.cmd(Param::kLoopLength, static_cast<std::int32_t>(LoopLengthMode::kFixed), 0, 0, /*idx=*/0,
        Op::kSet);
  CHECK(b.warns() == 1);
  CHECK(b.e.loops().get(0)->length_mode == LoopLengthMode::kQuantized);  // unchanged
}

// Target-conditional capacity (owner directive): kMaxLoopSlots differs by
// build target (arm-none-eabi: 8; host: 16, config.hpp) -- this fills the
// pool to ITS OWN compiled-in capacity and checks the boundary, so the SAME
// test source proves the right thing on either target without a hardcoded
// platform-specific number.
void test_loop_pool_capacity_is_target_conditional() {
  Band b;
  for (std::size_t i = 0; i < kMaxLoopSlots; ++i) {
    b.loop_new();
  }
  CHECK(b.e.loops().count() == kMaxLoopSlots);
  CHECK(b.warns() == 0);
  b.loop_new();  // one past capacity
  CHECK(b.warns() == 1);
  CHECK(b.e.loops().count() == kMaxLoopSlots);
}

void test_loop_bad_args_warn() {
  Band b;
  b.cmd(Param::kLoopRecordStart, 0, 0, 0, /*idx=*/0);  // no slot registered
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, 99 /*bad mode*/, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);  // not recording
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.cmd(Param::kLoopErase, 0, 0, 0, /*idx=*/99);  // bad slot id
  CHECK(b.warns() == 1);
  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/0);  // no shadow yet
  CHECK(b.warns() == 1);
}

// Byte-identity guard: registering/recording/launching a loop is opt-in --
// an Engine that never touches any kLoop* verb must behave EXACTLY as
// before (no clip, no warn, no MIDI) on every existing golden path.
void test_loop_default_inert() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(4 * kTicksPerBar);
  for (const OutEvent& o : b.ev) {
    CHECK(o.kind != OutEvent::Kind::kLoop);
  }
  CHECK(b.e.loops().count() == 0);
}

// Phase 7 (§7 item 5, docs/proposals/looper-in-gui-contract.md): the IDENTICAL
// explicit-id fix already shipped for kClipAdd, applied to kLoopNew --
// registering at an explicit id past the pool's tail pads every intervening
// index with an unclaimed placeholder (never returned by get(), see
// loop_buffer.hpp), then claims exactly `id`. Mirrors test_clip.cpp's own
// test_clip_add_at_explicit_id_registers.
void test_loop_new_at_explicit_id_registers() {
  Band b;
  b.loop_new_at(5);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().count() == 6);
  CHECK(b.e.loops().get(5) != nullptr);
  // Every padded id below 5 stays unclaimed -- get() reports "no slot here".
  CHECK(b.e.loops().get(0) == nullptr);
  CHECK(b.e.loops().get(4) == nullptr);
}

// A later, LOWER explicit id (arriving after a higher one already padded
// through it) still succeeds and fills exactly its own placeholder slot,
// leaving every other still-unclaimed index untouched. Mirrors
// test_clip.cpp's own test_clip_add_at_out_of_order_fills_earlier_placeholder.
void test_loop_new_at_out_of_order_fills_earlier_placeholder() {
  Band b;
  if (kMaxLoopSlots <= 3) {
    return;  // needs room for ids 0..3 on the smaller (device) pool
  }
  b.loop_new_at(3);
  b.loop_new_at(1);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().get(1) != nullptr);
  CHECK(b.e.loops().get(3) != nullptr);
  CHECK(b.e.loops().get(2) == nullptr);  // still an unclaimed placeholder
}

// Re-registering the SAME explicit id is rejected (LoopBuffer stays
// append-only, no retarget) -- the original registration is left untouched.
// Mirrors test_clip.cpp's own test_clip_add_at_duplicate_id_warns.
void test_loop_new_at_duplicate_id_warns() {
  Band b;
  b.loop_new_at(2);
  b.ev.clear();
  b.loop_new_at(2);
  CHECK(b.warns() == 1);
  CHECK(b.e.loops().count() == 3);
}

// An out-of-bounds explicit id (>= kMaxLoopSlots) is rejected cleanly, no
// crash. Mirrors test_clip.cpp's own test_clip_add_at_out_of_bounds_warns.
void test_loop_new_at_out_of_bounds_warns() {
  Band b;
  b.loop_new_at(static_cast<std::uint16_t>(kMaxLoopSlots));
  CHECK(b.warns() == 1);
  CHECK(b.e.loops().count() == 0);
}

// The full Shape-A round trip: an explicit-id registration is recordable
// exactly like a sequentially-assigned slot.
void test_loop_new_at_then_record_and_launch() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.loop_new_at(4);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 4);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));
  b.ev.clear();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/4);
  CHECK(b.e.loops().recording() && b.e.loops().recording_slot() == 4);
  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  b.cmd(Param::kLoopRecordStop, /*grid=*/0, 0, 0, /*idx=*/4);
  CHECK(!b.e.loops().recording());
  const LoopClip* clip = b.e.loops().get(4);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(b.warns() == 0);
}

}  // namespace

int main() {
  test_loop_new_and_clip_add();
  test_loop_record_stop_and_launch_round_trip();
  test_loop_overdub_merges_without_clearing();
  test_loop_undo_restores_prior_generation();
  test_loop_erase_clears_and_undo_restores();
  test_loop_length_modes();
  test_loop_pool_capacity_is_target_conditional();
  test_loop_bad_args_warn();
  test_loop_default_inert();
  test_loop_new_at_explicit_id_registers();
  test_loop_new_at_out_of_order_fills_earlier_placeholder();
  test_loop_new_at_duplicate_id_warns();
  test_loop_new_at_out_of_bounds_warns();
  test_loop_new_at_then_record_and_launch();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop: all OK\n");
  }
  return arrangrr::test::failures();
}

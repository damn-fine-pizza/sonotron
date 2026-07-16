// Torquato QA regression guards (Phase 7, node 6000 hardening pass, SLICE 1):
// pins the TWO already-fixed bugs loop_buffer.hpp's own header comments
// describe in detail, at the level where each is actually observable --
// GREEN today (both fixes are already landed in eaf9d50); kept as its own
// file/CTest entry so a future re-break of either specific guard is caught
// in isolation, same precedent as test_dual_arp_collision.cpp/test_insert_
// chain_fan_overflow_regression.cpp's own "prove the fix stays fixed" role.
//
// 1) Note-off-at-wrap (LoopBuffer::on_tick's own header comment: "a genuine
//    stuck-note-on-loop-restart bug found while writing this class's own
//    functional test"): an event whose duration == the loop's own length
//    must still release exactly at the wrap tick, in the SAME total D29
//    order (off before on) OutScheduler already guarantees everywhere else
//    (engine.hpp's own "D29 still sorts the off before the on" precedent) --
//    verified here at the OUTPUT event level (through the real scheduler),
//    not just LoopBuffer's raw fire() call order (which is deliberately
//    ON-then-OFF per iteration; the scheduler is what re-sequences it, see
//    this file's own assertion below for the exact reasoning).
//
// 2) tick-0 idempotency (LoopBufferPlayState::last_tick/has_ticked, same
//    header comment): a boundary-quantized clip launch that gets PROMOTED by
//    fire_clips mid-on_tick synchronously fires its own tick-0 content, and
//    fire_loop's ordinary per-clip pass reaches that SAME transport_tick
//    later in the SAME on_tick invocation -- must fire exactly once, not
//    twice, regardless of which call site reaches LoopBuffer::on_tick first.

#include "arrangrr/loop/loop_buffer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 2048>;

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
  void feed_note(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t b[3] = {0x90, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(b, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // idx = kNoExplicitClipId: the legacy sequential-append form (Repeat-Zone
  // binding contract Shape A, abi.hpp's kClipAdd comment) -- explicit here
  // since Command::idx now means "explicit clip id" for kClipAdd.
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8),
        kNoExplicitClipId);
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
  // Index (in emission order) of the FIRST matching MIDI event, or -1.
  int index_of(std::uint8_t port, std::uint8_t channel, std::uint8_t note, bool on) const {
    for (std::size_t i = 0; i < ev.size(); ++i) {
      const OutEvent& o = ev[i];
      if (o.kind == OutEvent::Kind::kMidi && o.port == port && o.msg.channel() == channel &&
          o.msg.d1 == note && (o.msg.type() == midi::kNoteOn) == on) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }
};

// Regression guard #1: the note-off-at-wrap fix. A single event spans the
// ENTIRE loop (duration == content_length == kTicksPerBar); at the wrap
// tick, BOTH the old cycle's note-off and the new cycle's note-on fire --
// and OutScheduler's own D29 total order places the off strictly before the
// on in the OUTPUT stream, exactly like every other same-tick chord-change
// release-then-sound precedent in this codebase (ChordEngine::sound()).
void test_loop_note_off_fires_exactly_at_the_wrap_tick_before_the_next_ons() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);  // explicit kMaj, root C
  b.ev.clear();
  b.cmd(Param::kLoopNew);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 64, 100);  // held for exactly one bar: duration == loop length
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).start == 0 && clip->event(0).duration == kTicksPerBar);
  CHECK(clip->length() == kTicksPerBar);  // duration == length: the exact-boundary case

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);  // fires tick 0's on synchronously
  b.advance(kTicksPerBar);                        // lands exactly on the wrap tick

  // Both fire at the SAME transport tick (the wrap): the off closing cycle 1
  // and the on opening cycle 2. If the wrap-boundary fix ever regressed
  // (comparing against the POST-wrap pos instead of the PRE-wrap len), the
  // off would never fire at all -- this count would drop to 0.
  CHECK(b.count_midi(1, 2, 64, /*on=*/false) == 1);
  CHECK(b.count_midi(1, 2, 64, /*on=*/true) == 2);  // tick-0 onset + the wrap's new-cycle onset
  const int off_idx = b.index_of(1, 2, 64, /*on=*/false);
  int last_on_idx = -1;
  for (std::size_t i = 0; i < b.ev.size(); ++i) {
    const OutEvent& o = b.ev[i];
    if (o.kind == OutEvent::Kind::kMidi && o.port == 1 && o.msg.channel() == 2 && o.msg.d1 == 64 &&
        o.msg.type() == midi::kNoteOn) {
      last_on_idx = static_cast<int>(i);
    }
  }
  CHECK(off_idx >= 0 && last_on_idx >= 0);
  // D29 total order: the wrap's own off must precede the wrap's own on in the
  // scheduled OUTPUT stream (even though LoopBuffer::on_tick's raw fire()
  // call order is deliberately on-then-off per iteration -- OutScheduler is
  // what re-sequences same-tick events into the off-before-on total order).
  CHECK(off_idx < last_on_idx);
}

// Regression guard #2: tick-0 idempotency across the two call sites that can
// both reach LoopBuffer::on_tick for the SAME transport_tick within ONE
// on_tick invocation -- a quantized ("next bar") launch PROMOTED by
// fire_clips fires its own tick-0 onset synchronously, and fire_loop's
// ordinary per-clip pass (which runs AFTER fire_clips in Engine::on_tick's
// own fixed order) reaches that identical tick again in the SAME call.
void test_loop_boundary_promoted_launch_fires_tick_zero_exactly_once() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);
  b.ev.clear();
  b.cmd(Param::kLoopNew);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  // A long hold (10 bars) so the loop's own content never wraps during this
  // test's short observation window -- isolates the idempotency property
  // from the wrap-boundary property regression guard #1 already covers.
  b.feed_note(3, 64, 100);
  b.advance(10 * kTicksPerBar);
  b.feed_note(3, 64, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->length() == 10 * kTicksPerBar);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  // Boundary::kNextBar: arms the launch instead of firing immediately --
  // ClipMatrix::on_bar promotes it once the bar boundary tick arrives, from
  // INSIDE Engine::on_tick's own fire_clips call, still before fire_loop's
  // own ordinary pass runs in that SAME invocation (engine.cpp's fixed
  // fire_timeline/fire_chord_seq/fire_clips/fire_arranger/fire_loop/fire_arp
  // order).
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0, Op::kDo, Boundary::kNextBar);
  b.advance(kTicksPerBar);  // the promotion boundary: fire_clips promotes THIS tick

  // Exactly one note-on for the loop's own tick-0 content -- NOT two (the
  // synchronous promotion fire + a duplicate ordinary fire_loop pass for the
  // identical transport_tick would double it if the has_ticked/last_tick
  // guard ever regressed).
  CHECK(b.count_midi(1, 2, 64, /*on=*/true) == 1);
}

}  // namespace

int main() {
  test_loop_note_off_fires_exactly_at_the_wrap_tick_before_the_next_ons();
  test_loop_boundary_promoted_launch_fires_tick_zero_exactly_once();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop_wrap_and_idempotency_regression: all OK\n");
  }
  return arrangrr::test::failures();
}

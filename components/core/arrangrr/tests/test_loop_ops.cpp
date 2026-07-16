// Torquato QA hardening pass (Phase 7, node 6000 SLICE 1): closes the
// remaining mandate gaps test_loop.cpp/test_loop_event.cpp do not already
// cover -- overdub's own WRAP alignment (not just "merges without
// clearing"), undo's SHARED single-generation shadow across DIFFERENT slots
// (test_loop.cpp only ever exercises undo on the SAME slot's own two
// generations), two loop slots playing SIMULTANEOUSLY and independently, and
// playback genuinely respecting the LIVE meter (T0) rather than the
// compile-time kTicksPerBar constant. Built on test_loop.cpp's own Band
// fixture shape -- no duplication of its existing cases.

#include "arrangrr/loop/loop_buffer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/perf/performance.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 2048>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate) {
    Command command;
    command.op = op;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = 1;
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
  // idx = kNoLoopExplicitId: the legacy sequential-append form (docs/
  // proposals/looper-in-gui-contract.md §7 item 5, the IDENTICAL fix already
  // shipped for kClipAdd/kNoExplicitClipId above) -- explicit here since
  // Command::idx now means "explicit loop-slot id" for kLoopNew, and every
  // bare `loop_new()` caller below relies on the ORIGINAL sequential-id
  // assignment.
  void loop_new() { cmd(Param::kLoopNew, 0, 0, 0, kNoLoopExplicitId); }
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

// 6100: overdub's new material aligns to the EXISTING content's own length,
// wrapping modulo it -- NOT the raw record_base offset. loop_buffer.hpp's
// own start_record comment documents this ("a SLICE-1 simplification"); this
// is the first test to actually cross the wrap boundary during an overdub
// pass (test_loop.cpp's own test_loop_overdub_merges_without_clearing only
// ever records within the FIRST cycle).
void test_loop_overdub_wraps_new_material_onto_existing_content_length() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar);  // exactly one bar of content: content_length == kTicksPerBar
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->content_length() == kTicksPerBar);

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kOverdub), 3, 0,
        /*idx=*/0);
  // Elapsed since THIS record_base: 1 bar + a quarter -- past the existing
  // content's own length once, so the new note's start must wrap modulo it.
  b.advance(kTicksPerBar + kTicksPerBar / 4);
  b.feed_note(3, 67, 100);
  b.advance(kTicksPerBar / 4);  // a quarter-bar hold: keeps a non-zero, distinct duration
  b.feed_note(3, 67, 0);
  // An explicit quarter-bar grid: stop_record's own quantize-after ALWAYS
  // runs (loop_event.hpp's LoopClip::quantize), so the default 1-bar grid
  // would snap this note's quarter-bar start back down to 0, masking the
  // very wrap this test exists to prove -- a finer grid keeps it distinct.
  b.cmd(Param::kLoopRecordStop, static_cast<std::int32_t>(kTicksPerBar / 4), 0, 0, /*idx=*/0);

  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 2);
  // The new event's start wrapped onto the existing 1-bar content: (1 bar +
  // 1/4 bar) % (1 bar) == 1/4 bar. Without the wrap, it would land at the
  // raw elapsed offset instead (5/4 bar), well past the loop's own length.
  CHECK(clip->event(1).start == kTicksPerBar / 4);
}

// Fork E: ONE shared shadow generation across the WHOLE pool, not one per
// slot. test_loop.cpp's own undo tests only ever exercise the SAME slot's two
// generations; this pins the cross-slot half of "shared shadow" explicitly --
// editing slot B after slot A consumes slot A's own undo capability entirely
// (the shadow now backs B), and undo(A) is a clean no-op/warn, not a crash or
// a stale restore.
void test_loop_undo_shared_shadow_moves_with_the_most_recent_edit() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);
  b.loop_new();  // slot 0
  b.loop_new();  // slot 1

  // Slot 0: one recorded note (this call also saves slot 0's -- empty --
  // shadow, but that generation is about to be superseded below).
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 60, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == 1);

  // Slot 1: a DIFFERENT recording. start_record's own save_shadow call now
  // overwrites the SHARED shadow with slot 1's (empty) prior content --
  // slot 0's own undo capability is gone the instant this starts, not merely
  // "also available".
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/1);
  b.feed_note(3, 67, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 67, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/1);
  CHECK(b.e.loops().get(1)->count() == 1);

  // Undo slot 0: the shadow no longer backs it (it backs slot 1's own prior
  // -- empty -- generation) -- a clean warn, and slot 0's content is left
  // COMPLETELY untouched (not silently reverted to some stale state).
  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 1);
  CHECK(b.e.loops().get(0)->count() == 1);  // untouched

  // Undo slot 1: the shadow DOES back it (slot 1's own prior generation,
  // empty before this recording) -- succeeds, restoring slot 1 to empty.
  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/1);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().get(1)->count() == 0);

  // The shadow is now consumed (single generation, no redo/toggle) -- a
  // THIRD undo, on EITHER slot, finds nothing left.
  b.ev.clear();
  b.cmd(Param::kLoopUndo, 0, 0, 0, /*idx=*/1);
  CHECK(b.warns() == 1);
}

// Per-slot playback (loop_buffer.hpp's own class header: "a real
// multi-track looper expects several captured loops ... to sound
// SIMULTANEOUSLY"): two DIFFERENT loop slots, launched together, sound their
// OWN distinct content independently -- stopping one never touches the
// other's still-playing notes.
void test_loop_two_slots_play_simultaneously_and_independently() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);
  b.loop_new();  // slot 0: a bass note
  b.loop_new();  // slot 1: a chord-role note
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.add_clip(TrackRole::kChord1, 0, ContentKind::kLoopBuffer, 1);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 1 | (3 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 48, 100);  // slot 0's own note
  b.advance(kTicksPerBar);
  b.feed_note(3, 48, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/1);
  b.feed_note(3, 72, 100);  // slot 1's own, DIFFERENT note
  b.advance(kTicksPerBar);
  b.feed_note(3, 72, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/1);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  // Clip 0 references loop slot 0 (part_role kBass, routed port1/ch2); clip 1
  // references loop slot 1 (part_role kChord1, routed port1/ch3).
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/1);
  // An immediate clip launch's own onset is scheduled through the SAME
  // OutScheduler every other immediate "start playing" launch uses
  // (apply_clip_content's kChordSequence branch has the identical shape) --
  // it is delivered on the NEXT flush point, not synchronously inside
  // push_command itself (engine.hpp's own on_tick unconditional-flush
  // comment); a tiny advance is what every functional test in this codebase
  // already does before observing an immediate launch's own onset.
  b.advance(1);
  CHECK(b.count_midi(1, 2, 48, /*on=*/true) == 1);  // slot 0's onset fired
  CHECK(b.count_midi(1, 3, 72, /*on=*/true) == 1);  // slot 1's onset ALSO fired, same instant

  // Stop clip 0 (slot 0) only: slot 0's own note releases; slot 1's note is
  // untouched (still sounding, no spurious release).
  b.ev.clear();
  b.cmd(Param::kClipStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.count_midi(1, 2, 48, /*on=*/false) == 1);  // slot 0 released
  CHECK(b.count_midi(1, 3, 72, /*on=*/false) == 0);  // slot 1 untouched
}

// T0: playback genuinely respects the LIVE meter, not the compile-time
// kTicksPerBar constant -- Engine::loop_record_stop's own grid fallback
// already threads m_transport.ticks_per_bar() (engine.cpp's own "same
// discipline as seq_stop's own T0 fix" comment); this proves the effect
// actually reaches the Looper's own wrap tick under a genuine 3-beat meter,
// via a validated Performance recall injected directly through the store
// (mirrors test_performance.cpp's own atomicity-test precedent: style_id =
// 0xFFFF so no builtin style needs loading first, keeping this test minimal).
void test_loop_playback_wrap_respects_the_live_non_default_meter() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, 0, 100);

  Performance p;
  p.style_id = 0xFFFF;           // none: apply_performance skips Arranger::load
  p.chord_sequence_id = 0xFFFF;  // none
  p.controller_map_id = 0xFFFF;  // none
  p.beats_per_bar = 3;           // a genuine 3/4 -- 3 * kTicksPerBeat, not kTicksPerBar (4 beats)
  p.routes[static_cast<std::size_t>(TrackRole::kBass)] =
      PerfRoute{.port = 1, .channel = 2, .enabled = 1};
  CHECK(b.e.performances().store(0, p));
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.transport().ticks_per_bar() == 3 * kTicksPerBeat);

  b.loop_new();
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  b.feed_note(3, 60, 100);
  b.advance(2 * kTicksPerBeat);  // 2 beats: less than a live (3-beat) bar,
                                 // more than half of one
  b.feed_note(3, 60, 0);
  // grid = 0: falls back to the LIVE m_transport.ticks_per_bar() (3 beats),
  // NOT the compile-time kTicksPerBar (4 beats) -- the exact T0 property
  // under test.
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  // Quantize-after snapped the 2-beat duration onto the LIVE 3-beat grid
  // (round to nearest, minimum one grid unit): nearest multiple of 3 beats to
  // 2 beats is 3 beats itself (loop_event.hpp's own round_to_grid).
  CHECK(clip->event(0).duration == 3 * kTicksPerBeat);
  CHECK(clip->length() == 3 * kTicksPerBeat);  // the loop's own wrap point

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(3 * kTicksPerBeat - 1);                  // one tick shy of the LIVE bar
  CHECK(b.count_midi(1, 2, 60, /*on=*/false) == 0);  // not yet -- confirms it
                                                     // is NOT wrapping at the
                                                     // stale 4-beat kTicksPerBar
  b.advance(1);                                      // now exactly 3 beats elapsed
  CHECK(b.count_midi(1, 2, 60, /*on=*/false) == 1);  // wraps at the LIVE meter's own bar
}

// Capacity bound (kMaxLoopEvents, per-slot event pool): filling a slot to ITS
// OWN compiled-in per-slot capacity and recording one note past it must
// degrade GRACEFULLY -- the note is silently dropped (loop_buffer.hpp's own
// documented discipline: "the note is silently dropped, same graceful
// degradation discipline as every other bounded pool here"), never a
// crash/trap, and the slot's own event count stays pinned at capacity.
void test_loop_event_pool_capacity_degrades_gracefully_never_crashes() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.loop_new();
  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  // kMaxLoopEvents distinct, non-overlapping one-tick notes (start i, held
  // for exactly 1 tick each) -- fills the slot to ITS OWN compiled-in
  // capacity (16x512 on host, 8x3072 on arm, config.hpp) without ever
  // touching the kMaxLoopHeldNotes concurrent-polyphony table (each note is
  // released before the next one opens).
  for (std::size_t i = 0; i < kMaxLoopEvents; ++i) {
    b.feed_note(3, static_cast<std::uint8_t>(60 + (i % 40)), 100);
    b.advance(1);
    b.feed_note(3, static_cast<std::uint8_t>(60 + (i % 40)), 0);
  }
  CHECK(b.e.loops().get(0)->count() == kMaxLoopEvents);

  // One MORE note, past capacity: LoopClip::record's own push_back fails,
  // note_on's own bounded-pool discipline drops it silently -- no crash, no
  // WarnCode (observe_loop_input has no EventSink to warn through, same gap
  // this file's sibling sounding-overflow regression documents for the
  // PLAYBACK side; here it is the intended, ALREADY-DOCUMENTED discipline for
  // the RECORDING side, not a new finding).
  b.ev.clear();
  b.feed_note(3, 100, 100);
  CHECK(b.e.loops().get(0)->count() == kMaxLoopEvents);  // pinned, not grown
  CHECK(b.warns() == 0);   // no observable signal either way -- a silent cap
  b.feed_note(3, 100, 0);  // the matching off: also a no-op (nothing was held)
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  CHECK(b.e.loops().get(0)->count() == kMaxLoopEvents);
}

}  // namespace

int main() {
  test_loop_overdub_wraps_new_material_onto_existing_content_length();
  test_loop_undo_shared_shadow_moves_with_the_most_recent_edit();
  test_loop_two_slots_play_simultaneously_and_independently();
  test_loop_playback_wrap_respects_the_live_non_default_meter();
  test_loop_event_pool_capacity_degrades_gracefully_never_crashes();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop_ops: all OK\n");
  }
  return arrangrr::test::failures();
}

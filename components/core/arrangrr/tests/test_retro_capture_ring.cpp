// Thorough unit-level QA of RetroCaptureRing itself (node 6300, "grab last N
// bars" -- retroactive capture): the pure ring/window logic, driven DIRECTLY
// (bypassing Engine/the ABI) so every physical-slot-eviction and
// window-clipping edge is pinned exactly, byte for byte. Complements
// test_retro_capture.cpp's own Engine-ABI-level functional coverage (arm/
// disarm dispatch, port gating, re-harmonization) -- this file is the
// pure-logic peer, same split as test_timeline (unit) vs test_engine
// (functional). Torquato's thorough follow-up to the 4-case smoke pass.

#include "arrangrr/loop/retro_capture.hpp"

#include "arrangrr/loop/loop_event.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// A fixed, deterministic harmonic context that always resolves through
// decompose_note's kInterval FALLBACK branch: chord invalid, key C major, and
// every note used below has pitch-class 1 (C#), which is diatonic to NEITHER
// C major (scale pcs {0,2,4,5,7,9,11}) nor any chord (no chord is playing) --
// so decompose_note always lands on `tone = note - key.root_pc(0) = note`,
// giving every captured event a `tone` value that is a bit-exact fingerprint
// of the raw MIDI note passed in, independent of ring physical placement.
ChordState invalid_chord() {
  return ChordState{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = false};
}
Key c_major() { return Key{.root_pc = 0, .mode = Mode::kMajor}; }

// (1) Circular-buffer overflow / oldest-out eviction: feed strictly MORE than
// kMaxRetroCaptureEvents notes so the ring wraps past its own capacity, then
// assert a full-window grab returns exactly the surviving (most recent)
// events, contiguous, uncorrupted, with no stale/overwritten leftovers.
void test_overflow_wraps_and_grab_returns_coherent_recent_window() {
  RetroCaptureRing ring;
  ring.arm(0);
  const ChordState chord = invalid_chord();
  const Key key = c_major();

  constexpr std::size_t kOverflowBy = 137;
  constexpr std::size_t kTotal = kMaxRetroCaptureEvents + kOverflowBy;
  for (std::size_t i = 0; i < kTotal; ++i) {
    const Tick t = static_cast<Tick>(i) * 2;
    ring.note_on(t, /*note=*/1, 100, chord, key);
    ring.note_off(t + 1, /*note=*/1);
  }
  CHECK(ring.count() == kMaxRetroCaptureEvents);  // ring stays bounded, never grows past capacity

  // Grab a window that DELIBERATELY reaches back to tick 0 -- if evicted
  // ticks (0 .. kOverflowBy*2 - 2, physically gone from the ring) ever leaked
  // back out, this is where a stale/overwritten event would surface. The
  // window is kept narrow enough (200 surviving events) to stay well under
  // the destination LoopClip's own pool capacity (kMaxLoopEvents, 512 on
  // host) -- that separate target-clip-capacity concern is pinned on its own
  // in test_dense_grab_exceeding_clip_capacity_drops_the_recent_tail below,
  // and must not be conflated with this ring-eviction check.
  const Tick oldest_surviving_tick = static_cast<Tick>(kOverflowBy) * 2;  // first non-evicted tick
  const Tick window_end = oldest_surviving_tick + 400;  // 200 surviving events, << 512
  LoopClip out;
  const bool ok = ring.grab(/*now=*/window_end, /*bar_ticks=*/window_end, /*n_bars=*/1, out);
  CHECK(ok);
  CHECK(out.count() == 200);

  // Every surviving event must be one of the NON-evicted ticks in range --
  // no earlier (evicted) tick may reappear (that would be a stale/
  // overwritten event leaking through), and every duration must be the
  // original 1, never corrupted.
  Tick min_start = static_cast<Tick>(-1);
  Tick max_start = 0;
  for (std::size_t i = 0; i < out.count(); ++i) {
    const LoopEvent& ev = out.event(i);
    CHECK(ev.duration == 1);
    CHECK(ev.start >= oldest_surviving_tick);  // never a stale/evicted tick
    if (ev.start < min_start) {
      min_start = ev.start;
    }
    if (ev.start > max_start) {
      max_start = ev.start;
    }
  }
  CHECK(min_start == oldest_surviving_tick);  // oldest surviving tick, exactly at the window edge
  CHECK(max_start == window_end - 2);         // newest tick inside the window
}

// (2) Late note-off after its note-on was EVICTED by ring overflow: hold a
// note, overflow the ring so that held note's physical slot gets overwritten
// by a NEW event, then send the stale note-off. It must be a clean no-op --
// it must NOT reach into the slot's new occupant and corrupt its duration.
void test_late_note_off_after_eviction_does_not_corrupt_new_occupant() {
  RetroCaptureRing ring;
  ring.arm(0);
  const ChordState chord = invalid_chord();
  const Key key = c_major();

  Tick t = 0;
  ring.note_on(t, /*note=*/1, 100, chord, key);  // NOTE_A: opens in physical slot 0, held open
  ++t;
  // Fill the ring with kMaxRetroCaptureEvents - 1 MORE fully-closed events
  // (distinct pitch, never overlapping NOTE_A's held bookkeeping) so the ring
  // reaches exactly its capacity WITHOUT evicting anything yet (NOTE_A's slot
  // 0 is still intact).
  for (std::size_t i = 0; i < kMaxRetroCaptureEvents - 1; ++i) {
    ring.note_on(t, /*note=*/13, 90, chord, key);
    ++t;
    ring.note_off(t, /*note=*/13);
    ++t;
  }
  CHECK(ring.count() == kMaxRetroCaptureEvents);

  // ONE more note-on: the ring is now full, so this push evicts the CURRENT
  // oldest slot -- physical slot 0, i.e. NOTE_A's still-open event -- and
  // must invalidate NOTE_A's held-note bookkeeping (invalidate_held) so a
  // later stale note-off can never reach this slot's new occupant.
  const Tick note_b_start = t;
  ring.note_on(note_b_start, /*note=*/25, 80, chord, key);  // NOTE_B: evicts NOTE_A's slot 0
  ring.note_off(note_b_start + 50, /*note=*/25);  // closes NOTE_B legitimately: duration 50

  // The stale, long-since-evicted NOTE_A note-off arrives NOW. If
  // invalidate_held ever failed to drop NOTE_A's held bookkeeping (or pointed
  // it at the wrong slot), this call would silently reach into NOTE_B's own
  // slot and overwrite its duration.
  ring.note_off(note_b_start + 999, /*note=*/1);

  CHECK(ring.count() == kMaxRetroCaptureEvents);  // note_off never changes ring occupancy

  // A NARROW window around NOTE_B only (not the whole 2048-deep ring) --
  // keeps the surviving-event count far below the destination LoopClip's own
  // pool capacity (kMaxLoopEvents, 512 on host), so this check stays isolated
  // from the SEPARATE target-clip-capacity finding pinned in
  // test_dense_grab_exceeding_clip_capacity_drops_the_recent_tail below.
  LoopClip out;
  const bool ok = ring.grab(/*now=*/note_b_start + 200, /*bar_ticks=*/300, /*n_bars=*/1, out);
  CHECK(ok);
  bool found_note_b = false;
  for (std::size_t i = 0; i < out.count(); ++i) {
    const LoopEvent& ev = out.event(i);
    CHECK(ev.tone != 1);  // NOTE_A's fingerprint must never resurface (its slot was overwritten)
    if (ev.tone == 25) {
      found_note_b = true;
      // The load-bearing assertion: NOTE_B's own legitimately-closed duration
      // (50) must survive the stale NOTE_A note-off untouched. A bug in
      // invalidate_held would instead stomp this to (note_b_start+999 -
      // note_b_start) == 999.
      CHECK(ev.duration == 50);
    }
  }
  CHECK(found_note_b);
}

// (3a) grab() window boundaries: exactly N bars, and grab when the ring holds
// FEWER than N bars (window_start clamps to 0, everything captured comes
// back untruncated).
void test_grab_window_exact_bars_and_fewer_than_requested() {
  RetroCaptureRing ring;
  ring.arm(0);
  const ChordState chord = invalid_chord();
  const Key key = c_major();
  const Tick bar = kTicksPerBar;

  // Three consecutive, non-overlapping, exactly-one-bar-long notes: bar 0, 1, 2.
  for (int b = 0; b < 3; ++b) {
    const Tick start = static_cast<Tick>(b) * bar;
    ring.note_on(start, /*note=*/1, 100, chord, key);
    ring.note_off(start + bar, /*note=*/1);
  }

  // grab last 1 bar ending right after bar 2 -- only bar 2's note should
  // survive; bar 1's note ends EXACTLY at window_start, the exclusive lower
  // boundary, so it must NOT be included.
  {
    LoopClip out;
    const bool ok = ring.grab(/*now=*/3 * bar, bar, /*n_bars=*/1, out);
    CHECK(ok);
    CHECK(out.count() == 1);
    CHECK(out.event(0).start == 0);
    CHECK(out.event(0).duration == bar);
  }

  // grab MORE bars than the ring actually holds (5 vs. 3 captured): the
  // window clamps to 0 and ALL three notes come back, each at its ORIGINAL
  // absolute tick (window_start == 0, so rebasing is a no-op).
  {
    LoopClip out;
    const bool ok = ring.grab(/*now=*/3 * bar, bar, /*n_bars=*/5, out);
    CHECK(ok);
    CHECK(out.count() == 3);
    for (int b = 0; b < 3; ++b) {
      bool found = false;
      for (std::size_t i = 0; i < out.count(); ++i) {
        if (out.event(i).start == static_cast<Tick>(b) * bar) {
          found = true;
          CHECK(out.event(i).duration == bar);
        }
      }
      CHECK(found);
    }
  }
}

// (3b) A grab window whose edge falls in the MIDDLE of a sustained note: the
// note is CROPPED to the window (kept, truncated), never dropped whole and
// never extended past the window edge -- pins the actual crop-not-drop
// contract precisely.
void test_grab_boundary_crossing_note_is_cropped_not_dropped() {
  RetroCaptureRing ring;
  ring.arm(0);
  const ChordState chord = invalid_chord();
  const Key key = c_major();
  const Tick bar = kTicksPerBar;

  // Starts half a bar before window_start, lasts a full bar -- so it spans
  // straight across the window's lower edge.
  ring.note_on(bar / 2, /*note=*/1, 100, chord, key);
  ring.note_off(bar / 2 + bar, /*note=*/1);

  LoopClip out;
  const bool ok = ring.grab(/*now=*/2 * bar, bar, /*n_bars=*/1, out);
  CHECK(ok);
  CHECK(out.count() == 1);
  CHECK(out.event(0).start == 0);           // cropped to the window's own start
  CHECK(out.event(0).duration == bar / 2);  // only the portion INSIDE the window survives

  // A note still OPEN (duration == 0, performer still holding) at grab time
  // is presumed sounding right up to `now` -- also cropped, not dropped.
  RetroCaptureRing ring2;
  ring2.arm(0);
  ring2.note_on(bar + 10, /*note=*/1, 100, chord, key);  // never released
  LoopClip out2;
  const bool ok2 = ring2.grab(/*now=*/2 * bar, bar, /*n_bars=*/1, out2);
  CHECK(ok2);
  CHECK(out2.count() == 1);
  CHECK(out2.event(0).start == 10);
  CHECK(out2.event(0).duration == bar - 10);
}

// (3c) An empty/never-armed ring, and a bar_ticks==0 window: both a clean
// no-op -- `out` is left COMPLETELY untouched (not even cleared).
void test_grab_empty_ring_and_zero_bar_ticks_are_clean_noops() {
  const ChordState chord = invalid_chord();
  const Key key = c_major();

  RetroCaptureRing ring;  // never armed, never fed
  LoopClip out;
  CHECK(out.record(LoopEvent{.start = 7,
                             .duration = 9,
                             .tone = 3,
                             .octave = 1,
                             .velocity = 42,
                             .source = LoopNoteSource::kInterval}));
  const bool ok = ring.grab(100, 10, 1, out);
  CHECK(!ok);
  CHECK(out.count() == 1);  // untouched -- the sentinel is still exactly there
  CHECK(out.event(0).start == 7);
  CHECK(out.event(0).duration == 9);
  CHECK(out.event(0).velocity == 42);

  // A ring WITH content, but bar_ticks == 0: also rejected, also untouched.
  RetroCaptureRing ring2;
  ring2.arm(0);
  ring2.note_on(0, 1, 100, chord, key);
  ring2.note_off(10, 1);
  LoopClip out2;
  CHECK(out2.record(LoopEvent{.start = 7,
                              .duration = 9,
                              .tone = 3,
                              .octave = 1,
                              .velocity = 42,
                              .source = LoopNoteSource::kInterval}));
  const bool ok2 = ring2.grab(100, /*bar_ticks=*/0, 1, out2);
  CHECK(!ok2);
  CHECK(out2.count() == 1);
  CHECK(out2.event(0).velocity == 42);

  // A ring that WAS fed but whose only content falls entirely OUTSIDE the
  // requested window: also a clean no-op, `out` untouched.
  RetroCaptureRing ring3;
  ring3.arm(0);
  ring3.note_on(0, 1, 100, chord, key);
  ring3.note_off(5, 1);
  LoopClip out3;
  CHECK(out3.record(LoopEvent{.start = 7,
                              .duration = 9,
                              .tone = 3,
                              .octave = 1,
                              .velocity = 42,
                              .source = LoopNoteSource::kInterval}));
  const bool ok3 =
      ring3.grab(/*now=*/1000, /*bar_ticks=*/1, /*n_bars=*/1, out3);  // window = [999,1000)
  CHECK(!ok3);
  CHECK(out3.count() == 1);
  CHECK(out3.event(0).velocity == 42);
}

// (4) Target-clip capacity finding: on host, kMaxLoopEvents (512) is SMALLER
// than kMaxRetroCaptureEvents (2048) -- config.hpp's own comment notes this is
// a deliberate host-only stack-safety choice, NOT target-conditional the way
// kMaxLoopEvents itself is. A dense capture (ring at full capacity) grabbed
// into a fresh LoopClip therefore overflows the CLIP's own pool, and
// LoopClip::record's pool-full drop is silent (`(void)out.record(...)`, no
// warning reaches the caller). grab() materializes oldest-to-newest and never
// breaks early, so when the destination pool fills partway through, the
// events actually kept are the OLDEST (stalest) ones in the window and the
// events closest to `now` -- the ones a "grab the last N bars" gesture most
// wants -- are the ones silently dropped. This directly contradicts the
// "coherent recent window" contract (retro_capture.hpp's own grab() comment)
// once the window's event count exceeds the destination clip's capacity.
void test_dense_grab_exceeding_clip_capacity_drops_the_recent_tail_not_the_stale_head() {
  static_assert(kMaxLoopEvents < kMaxRetroCaptureEvents,
                "this finding is specifically about the host LoopClip pool being smaller "
                "than the retro ring; re-check the assumption if the constants ever change");
  RetroCaptureRing ring;
  ring.arm(0);
  const ChordState chord = invalid_chord();
  const Key key = c_major();

  for (std::size_t i = 0; i < kMaxRetroCaptureEvents; ++i) {
    const Tick t = static_cast<Tick>(i) * 2;
    ring.note_on(t, /*note=*/1, 100, chord, key);
    ring.note_off(t + 1, /*note=*/1);
  }
  CHECK(ring.count() == kMaxRetroCaptureEvents);

  LoopClip out;  // fresh -- exactly kMaxLoopEvents capacity
  const Tick now = static_cast<Tick>(kMaxRetroCaptureEvents) * 2 + 10;
  const bool ok = ring.grab(now, now * 4, 1, out);
  CHECK(ok);
  // Silent truncation to the destination pool's own capacity, no warning
  // surfaces through grab()'s bool return at all.
  CHECK(out.count() == kMaxLoopEvents);

  const Tick latest_original_tick =
      static_cast<Tick>(kMaxRetroCaptureEvents - 1) * 2;  // closest to `now`
  bool found_most_recent_event = false;
  for (std::size_t i = 0; i < out.count(); ++i) {
    if (out.event(i).start == latest_original_tick) {
      found_most_recent_event = true;
    }
  }
  // RED: this is the bug. A "grab last N bars ending NOW" gesture should keep
  // the material closest to `now` when it must drop something to fit the
  // destination clip -- instead grab()'s oldest-to-newest materialization
  // order means LoopClip::record's silent pool-full drop discards exactly the
  // newest (most recent, most wanted) tail of the window and keeps the
  // stalest kMaxLoopEvents ticks instead. See test_retro_capture_ring.cpp's
  // own comment above this test for the full root-cause analysis.
  CHECK(found_most_recent_event);
}

}  // namespace

int main() {
  test_overflow_wraps_and_grab_returns_coherent_recent_window();
  test_late_note_off_after_eviction_does_not_corrupt_new_occupant();
  test_grab_window_exact_bars_and_fewer_than_requested();
  test_grab_boundary_crossing_note_is_cropped_not_dropped();
  test_grab_empty_ring_and_zero_bar_ticks_are_clean_noops();
  test_dense_grab_exceeding_clip_capacity_drops_the_recent_tail_not_the_stale_head();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_retro_capture_ring: all OK\n");
  }
  return arrangrr::test::failures();
}

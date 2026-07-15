#pragma once

#include <cstddef>
#include <cstdint>

#include "arrangrr/config.hpp"
#include "arrangrr/loop/loop_event.hpp"

// RetroCaptureRing: node 6300 ("grab last N bars" -- retroactive capture).
// Owner-locked forks for this slice:
//   STREAM = LIVE INPUT -- the ring taps the SAME live-note stream
//     LoopBuffer::note_on/note_off already taps during ordinary recording
//     (Engine::push_midi_in's own tee): "I was noodling, didn't hit record,
//     grab what I just played." NOT the arranger's generated output.
//   ARMED (Fork C) -- the ring is fed ONLY while an explicit "retroactive
//     capture armed" flag is on: a single branch per incoming live note when
//     disarmed, so ZERO cost on the hot path when the feature is off.
//
// ONE bounded, no-heap ring (not per-slot -- the owner's own framing for this
// slice) that, WHILE ARMED, continuously captures live input as chord-tone/
// scale-degree/interval-relative LoopEvents: decompose_note() runs at CAPTURE
// time against the chord/key LIVE at that moment (loop_event.hpp), exactly as
// LoopBuffer::note_on already does, so a later grab re-harmonizes correctly
// like every other loop. Note-on/off pairing (filling durations) mirrors
// LoopBuffer's own m_held discipline.
//
// Storage discipline: unlike LoopClip (whose LoopEvent::start is relative to
// the CLIP's own start), a ring event's `start` is the ABSOLUTE stream tick
// (Engine::m_now) the note began -- there is no fixed "loop start" while
// merely noodling with no target in mind yet. grab() rebases the window it
// materializes so the grabbed content itself starts at 0, exactly like every
// other LoopClip (LoopBuffer::stop_record's own zero-reference discipline,
// generalized here to a retroactively-chosen window instead of a
// start_record() call).
//
// Bounded via a plain FIXED-CAPACITY CIRCULAR BUFFER (not a StaticVector):
// once full, the oldest event is evicted (overwritten) to make room for the
// newest -- "fixed capacity, oldest-out" (config.hpp's own kMaxRetroCapture-
// Events comment) -- so an indefinitely long noodling session never grows
// memory.

namespace arrangrr {

// Bounded per-ring polyphony bookkeeping for pairing a live note-on with its
// later note-off, mirroring LoopBuffer's own kMaxLoopHeldNotes/
// LoopBufferHeldNote precedent (loop_buffer.hpp) at the SAME generous-but-
// bounded 16-concurrent-note headroom for one performer's two hands.
inline constexpr std::size_t kMaxRetroHeldNotes = 16;
struct RetroHeldNote {
  std::uint8_t note = 0;
  std::size_t slot = 0;  // physical index into RetroCaptureRing's own ring storage
  bool used = false;
};

class RetroCaptureRing {
 public:
  bool armed() const noexcept { return m_armed; }
  std::uint8_t port() const noexcept { return m_port; }
  // Number of events currently held in the ring (test/host observability,
  // mirrors LoopClip::count()/empty()).
  std::size_t count() const noexcept { return m_count; }
  bool empty() const noexcept { return m_count == 0; }

  // Arms capture on `port`: clears any prior ring content first (mirrors
  // LoopBuffer::start_record's own "abandon any in-progress capture, fresh
  // start" discipline -- an open note left over from a PRIOR arm session has
  // no reachable note-off anyway once the ring is cleared).
  void arm(std::uint8_t port) noexcept {
    m_armed = true;
    m_port = port;
    clear();
  }
  // Stops capturing; ring content is left ALONE (a later grab() can still
  // read whatever was captured while armed -- "arm, play, disarm, grab" is a
  // valid gesture, not just "arm, play, grab while still armed").
  void disarm() noexcept { m_armed = false; }

  // Called for every live note-on while armed (Engine::push_midi_in's tee,
  // mirroring LoopBuffer::note_on): decompose_note() against the chord/key
  // LIVE right now.
  void note_on(Tick now, std::uint8_t note, std::uint8_t velocity, const ChordState& chord,
               const Key& key) noexcept {
    if (!m_armed) {
      return;
    }
    std::int8_t tone = 0;
    std::int8_t octave = 0;
    LoopNoteSource source = LoopNoteSource::kInterval;
    decompose_note(note, chord, key, tone, octave, source);
    const std::size_t slot = push(LoopEvent{.start = now,
                                            .duration = 0,
                                            .tone = tone,
                                            .octave = octave,
                                            .velocity = velocity,
                                            .source = source});
    hold(note, slot);
  }

  // Called for every live note-off while armed. Closes the matching open
  // event's duration; a note-off with no matching held note (the note-on was
  // never captured, or its slot has since been evicted by ring overflow) is a
  // silent no-op, the same graceful degradation as LoopBuffer::note_off.
  void note_off(Tick now, std::uint8_t note) noexcept {
    if (!m_armed) {
      return;
    }
    std::size_t slot = 0;
    if (!release_held(note, slot)) {
      return;
    }
    LoopEvent& ev = m_ring[slot];
    ev.duration = now >= ev.start ? now - ev.start : 0;
  }

  // Materializes the ring's last `n_bars` (each `bar_ticks` long) ending at
  // `now` into `out` (cleared first, then filled -- left COMPLETELY untouched
  // if the window turns out empty, mirroring Performance recall's own
  // validate-everything-first-apply-nothing-on-failure discipline), rebasing
  // every captured event's absolute start tick so the grabbed window itself
  // starts at 0 -- a normal, playable, re-harmonizing loop, identical in kind
  // to a recorded one. Returns false when the ring holds nothing inside that
  // window (including an entirely empty/never-armed ring) or `bar_ticks==0`.
  bool grab(Tick now, Tick bar_ticks, std::uint8_t n_bars, LoopClip& out) const noexcept {
    if (m_count == 0 || bar_ticks == 0) {
      return false;
    }
    const std::uint8_t bars = n_bars < 1 ? std::uint8_t{1} : n_bars;
    const Tick window = bar_ticks * static_cast<Tick>(bars);
    const Tick window_start = now > window ? now - window : 0;
    // First pass: does the window actually contain anything? Two O(m_count)
    // passes over a bounded, compile-time-capped array is cheap -- grab() is
    // a one-shot user gesture, never the per-tick hot path.
    bool any = false;
    for (std::size_t i = 0; i < m_count && !any; ++i) {
      any = window_overlap(m_ring[physical(i)], window_start, now).overlaps;
    }
    if (!any) {
      return false;
    }
    out.clear();
    for (std::size_t i = 0; i < m_count; ++i) {
      const LoopEvent& ev = m_ring[physical(i)];
      const WindowOverlap ov = window_overlap(ev, window_start, now);
      if (!ov.overlaps) {
        continue;
      }
      LoopEvent rebased = ev;
      rebased.start = ov.rel_start;
      rebased.duration = ov.rel_end - ov.rel_start;
      // Pool-full drop is graceful degradation, same discipline as every
      // other bounded pool here (LoopBuffer::note_on's own comment).
      (void)out.record(rebased);
    }
    return true;
  }

 private:
  // The result of clipping one captured event to [window_start, now).
  struct WindowOverlap {
    Tick rel_start = 0;
    Tick rel_end = 0;
    bool overlaps = false;
  };

  // Clips `ev` to [window_start, now), rebasing onto window_start. A
  // still-open (duration == 0) event is presumed to still be sounding right
  // up to `now` -- the performer may still be holding the note the instant
  // grab() is called.
  static WindowOverlap window_overlap(const LoopEvent& ev, Tick window_start, Tick now) noexcept {
    const Tick ev_end = ev.duration > 0 ? ev.start + ev.duration : now;
    if (ev_end <= window_start || ev.start >= now) {
      return WindowOverlap{};
    }
    const Tick clipped_end = ev_end > now ? now : ev_end;
    const Tick rel_start = ev.start > window_start ? ev.start - window_start : 0;
    const Tick rel_end = clipped_end > window_start ? clipped_end - window_start : 0;
    if (rel_end <= rel_start) {
      return WindowOverlap{};
    }
    return WindowOverlap{.rel_start = rel_start, .rel_end = rel_end, .overlaps = true};
  }

  std::size_t push(const LoopEvent& ev) noexcept {
    const std::size_t write_at = (m_head + m_count) % kMaxRetroCaptureEvents;
    if (m_count == kMaxRetroCaptureEvents) {
      // Full: the physical slot about to be overwritten IS the current
      // oldest -- drop any held-note bookkeeping still pointing at it so a
      // later, genuinely-stale note-off can never mutate the slot's NEW
      // occupant.
      invalidate_held(write_at);
      m_head = (m_head + 1) % kMaxRetroCaptureEvents;
    } else {
      ++m_count;
    }
    m_ring[write_at] = ev;
    return write_at;
  }
  std::size_t physical(std::size_t logical_i) const noexcept {
    return (m_head + logical_i) % kMaxRetroCaptureEvents;
  }
  void clear() noexcept {
    m_head = 0;
    m_count = 0;
    for (RetroHeldNote& h : m_held) {
      h.used = false;
    }
  }
  void hold(std::uint8_t note, std::size_t slot) noexcept {
    for (RetroHeldNote& h : m_held) {
      if (!h.used) {
        h = RetroHeldNote{.note = note, .slot = slot, .used = true};
        return;
      }
    }
    // Table full: a graceful degradation, mirrors LoopBuffer::hold -- the
    // matching note-off will find nothing to close.
  }
  bool release_held(std::uint8_t note, std::size_t& slot) noexcept {
    for (RetroHeldNote& h : m_held) {
      if (h.used && h.note == note) {
        h.used = false;
        slot = h.slot;
        return true;
      }
    }
    return false;
  }
  void invalidate_held(std::size_t slot) noexcept {
    for (RetroHeldNote& h : m_held) {
      if (h.used && h.slot == slot) {
        h.used = false;
      }
    }
  }

  bool m_armed = false;
  std::uint8_t m_port = 0;
  LoopEvent m_ring[kMaxRetroCaptureEvents]{};
  std::size_t m_head = 0;
  std::size_t m_count = 0;
  RetroHeldNote m_held[kMaxRetroHeldNotes]{};
};

}  // namespace arrangrr

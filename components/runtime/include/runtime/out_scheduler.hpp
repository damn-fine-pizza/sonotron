#pragma once

#include <cstdint>

#include "common/assert.hpp"
#include "common/midi/message.hpp"
#include "common/time.hpp"

// Timestamped MIDI output queue with the D29 total order:
//   (tick, class_priority, seq_no)
//
// Phase-1 runtime extraction: moved out of components/arrangrr into
// components/runtime byte-for-byte. Namespace stays `arrangrr` for Phase 1
// (minimal churn); see runtime/transport.hpp's header comment.
// class_priority: realtime/clock < NoteOff < CC/other < NoteOn, so a NoteOff
// always precedes a NoteOn scheduled on the same tick and clock always leads.
// seq_no is a monotonic emission counter — the final tie-break that makes the
// heap deterministic build-to-build (goldens depend on it).

namespace arrangrr {

enum class EventClass : std::uint8_t {
  kRealtime = 0,
  kNoteOff = 1,
  kOther = 2,
  kNoteOn = 3,
};

constexpr EventClass classify(const MidiMessage& msg) noexcept {
  if (midi::is_realtime(msg.status)) {
    return EventClass::kRealtime;
  }
  const std::uint8_t type = msg.type();
  if (type == midi::kNoteOff) {
    return EventClass::kNoteOff;
  }
  if (type == midi::kNoteOn) {
    return EventClass::kNoteOn;
  }
  return EventClass::kOther;
}

struct ScheduledEvent {
  Tick tick = 0;
  std::uint32_t seq = 0;
  MidiMessage msg{};
  std::uint8_t port = 0;
  std::uint8_t cls = 0;
};

template <std::size_t N>
class OutScheduler {
 public:
  constexpr std::size_t size() const noexcept { return m_size; }
  constexpr bool empty() const noexcept { return m_size == 0; }
  static constexpr std::size_t capacity() noexcept { return N; }

  // False when full — the caller surfaces a warn event; clock/realtime must
  // never be the class that gets dropped (graceful degradation, §9.C).
  [[nodiscard]] constexpr bool schedule(std::uint8_t port, Tick tick,
                                        const MidiMessage& msg) noexcept {
    if (m_size == N) {
      return false;
    }
    m_heap[m_size] = ScheduledEvent{
        .tick = tick,
        .seq = m_seq++,
        .msg = msg,
        .port = port,
        .cls = static_cast<std::uint8_t>(classify(msg)),
    };
    sift_up(m_size++);
    return true;
  }

  // Pops every event due at or before `now`, in total order. Tombstoned
  // entries (status 0, see cancel_note_off) are dropped silently.
  // Sink signature: void(const ScheduledEvent&).
  template <typename Sink>
  constexpr void pop_due(Tick now, Sink&& sink) {
    while (m_size > 0 && m_heap[0].tick <= now) {
      const ScheduledEvent ev = m_heap[0];
      m_heap[0] = m_heap[--m_size];
      if (m_size > 0) {
        sift_down(0);
      }
      if (ev.msg.status != 0) {
        sink(ev);
      }
    }
  }

  // Retrigger support (§9.B: no duplicate note without an intervening off):
  // tombstones the EARLIEST pending NoteOff for (port, channel, note) whose
  // scheduled tick is at or after `on_tick`, so a re-fired note is not
  // truncated by the previous one's release, while a DELIBERATE gap (an off
  // that already lands strictly before the incoming on — e.g. a ratchet sub-hit
  // with sub_gate < slice) is left alone. Returns true when one was found — the
  // caller re-emits that off anchored to the incoming on's tick. `on_tick`
  // defaults to 0 (cancel the earliest off at any tick, the tick-agnostic
  // behaviour).
  //
  // Note: this same anti-stuck-note protection is exactly what a naive "tie"
  // (a step whose note-off is merely lengthened) collides with — the next
  // same-note step's on would tombstone the long off here and re-attack. The
  // sequencer avoids that upstream by suppressing the tied-into step's emission
  // (see Timeline::suppressed_by_tie), so no same-note on ever reaches here
  // while the tie is held.
  constexpr bool cancel_note_off(std::uint8_t port, std::uint8_t channel, std::uint8_t note,
                                 Tick on_tick = 0) noexcept {
    int best = -1;
    for (std::size_t i = 0; i < m_size; ++i) {
      const ScheduledEvent& e = m_heap[i];
      if (e.port != port || e.msg.status == 0 || e.msg.type() != midi::kNoteOff ||
          e.msg.channel() != channel || e.msg.d1 != note || e.tick < on_tick) {
        continue;
      }
      if (best < 0 || e.tick < m_heap[static_cast<std::size_t>(best)].tick) {
        best = static_cast<int>(i);
      }
    }
    if (best < 0) {
      return false;
    }
    m_heap[static_cast<std::size_t>(best)].msg.status = 0;  // tombstone
    return true;
  }

  constexpr void clear() noexcept { m_size = 0; }

 private:
  static constexpr bool before(const ScheduledEvent& a, const ScheduledEvent& b) noexcept {
    if (a.tick != b.tick) {
      return a.tick < b.tick;
    }
    if (a.cls != b.cls) {
      return a.cls < b.cls;
    }
    return a.seq < b.seq;
  }

  constexpr void sift_up(std::size_t i) noexcept {
    while (i > 0) {
      const std::size_t parent = (i - 1) / 2;
      if (!before(m_heap[i], m_heap[parent])) {
        break;
      }
      const ScheduledEvent tmp = m_heap[i];
      m_heap[i] = m_heap[parent];
      m_heap[parent] = tmp;
      i = parent;
    }
  }

  constexpr void sift_down(std::size_t i) noexcept {
    while (true) {
      const std::size_t left = 2 * i + 1;
      const std::size_t right = left + 1;
      std::size_t smallest = i;
      if (left < m_size && before(m_heap[left], m_heap[smallest])) {
        smallest = left;
      }
      if (right < m_size && before(m_heap[right], m_heap[smallest])) {
        smallest = right;
      }
      if (smallest == i) {
        return;
      }
      const ScheduledEvent tmp = m_heap[i];
      m_heap[i] = m_heap[smallest];
      m_heap[smallest] = tmp;
      i = smallest;
    }
  }

  ScheduledEvent m_heap[N]{};
  std::size_t m_size = 0;
  std::uint32_t m_seq = 0;
};

}  // namespace arrangrr

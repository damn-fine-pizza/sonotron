#pragma once

#include <cstdint>

#include "common/midi/message.hpp"

// Byte-stream MIDI parser, one instance per input port. Compliance rules it
// implements (DESIGN.md §9.A):
//  - running status on input;
//  - realtime bytes (F8..FF) pass through immediately and do NOT disturb
//    the running-status state, even mid-message (§24.7 nit);
//  - system common resets running status;
//  - NoteOn velocity 0 is normalized to a real NoteOff (release velocity 0) —
//    downstream always emits genuine NoteOff (§9.A output policy default);
//  - SysEx is skipped safely (F0..F7), robust to interruption;
//  - orphan data bytes (no status in effect) are dropped without error.
//
// Phase-4b promotion (docs/design/orchestrator-pipeline-extraction.md
// §16.4, Seam C): moved byte-for-byte out of components/core/arrangrr into
// components/core/runtime so BOTH the arrangrr stage and the new chorddet peer
// can each hold their OWN independent MidiParser instance for the same
// inbound byte stream, instead of sharing one parser/parsed-message path.
// Namespace stays `arrangrr` (minimal churn, same precedent as the Phase-1
// runtime extraction of Transport/OutScheduler).

namespace arrangrr {

class MidiParser {
 public:
  // Feeds one byte; invokes sink(const MidiMessage&) zero or more times.
  template <typename Sink>
  constexpr void feed(std::uint8_t byte, Sink&& sink) {
    if (midi::is_realtime(byte)) {
      sink(MidiMessage::realtime(byte));
      return;  // state untouched: realtime may interleave anywhere
    }

    if (midi::is_status(byte)) {
      if (byte == midi::kSysExStart) {
        m_in_sysex = true;
        m_running = 0;
        return;
      }
      if (byte == midi::kSysExEnd) {
        // EOX is System Common: besides closing the SysEx it terminates any
        // message being assembled and clears running status (MIDI 1.0).
        m_in_sysex = false;
        m_status = 0;
        m_running = 0;
        m_have = 0;
        return;
      }
      m_in_sysex = false;  // any status terminates a dangling SysEx
      m_status = byte;
      m_have = 0;
      m_needed = midi::data_length(byte);
      m_running = midi::is_channel_voice(byte) ? byte : 0;  // system common: no running status
      if (m_needed == 0) {
        sink(MidiMessage{.status=byte, .d1=0, .d2=0});
        m_status = 0;
      }
      return;
    }

    // Data byte.
    if (m_in_sysex) {
      return;  // SysEx payload skipped (P2 feature)
    }
    if (m_status == 0) {
      if (m_running == 0) {
        return;  // orphan data byte: drop
      }
      m_status = m_running;  // running status re-arms the last channel status
      m_have = 0;
      m_needed = midi::data_length(m_status);
    }
    m_data[m_have++] = byte;
    if (m_have == m_needed) {
      MidiMessage msg{.status=m_status, .d1=m_data[0], .d2=m_needed > 1 ? m_data[1] : std::uint8_t{0}};
      if (msg.type() == midi::kNoteOn && msg.d2 == 0) {
        msg = MidiMessage::note_off(msg.channel(), msg.d1, 0);
      }
      sink(msg);
      m_status = 0;  // next data byte re-arms from running_
      m_have = 0;
    }
  }

  template <typename Sink>
  constexpr void feed(const std::uint8_t* bytes, std::size_t n, Sink&& sink) {
    for (std::size_t i = 0; i < n; ++i) {
      feed(bytes[i], sink);
    }
  }

  constexpr void reset() noexcept {
    m_status = 0;
    m_running = 0;
    m_have = 0;
    m_needed = 0;
    m_in_sysex = false;
  }

 private:
  std::uint8_t m_status = 0;   // status of the message being assembled
  std::uint8_t m_running = 0;  // last channel-voice status for running status
  std::uint8_t m_data[2] = {0, 0};
  int m_have = 0;
  int m_needed = 0;
  bool m_in_sysex = false;
};

}  // namespace arrangrr

#pragma once

#include <cstdint>

#include "arrangrr/midi/message.hpp"

// Byte-stream MIDI parser, one instance per input port. Compliance rules it
// implements (DESIGN.md §9.A):
//  - running status on input;
//  - realtime bytes (F8..FF) pass through immediately and do NOT disturb
//    the running-status state, even mid-message (§29.7 nit);
//  - system common resets running status;
//  - NoteOn velocity 0 is normalized to a real NoteOff (release velocity 0) —
//    downstream always emits genuine NoteOff (§9.A output policy default);
//  - SysEx is skipped safely (F0..F7), robust to interruption;
//  - orphan data bytes (no status in effect) are dropped without error.

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
        in_sysex_ = true;
        running_ = 0;
        return;
      }
      if (byte == midi::kSysExEnd) {
        in_sysex_ = false;
        return;
      }
      in_sysex_ = false;  // any status terminates a dangling SysEx
      status_ = byte;
      have_ = 0;
      needed_ = midi::data_length(byte);
      running_ = midi::is_channel_voice(byte) ? byte : 0;  // system common: no running status
      if (needed_ == 0) {
        sink(MidiMessage{byte, 0, 0});
        status_ = 0;
      }
      return;
    }

    // Data byte.
    if (in_sysex_) return;  // SysEx payload skipped (P2 feature)
    if (status_ == 0) {
      if (running_ == 0) return;  // orphan data byte: drop
      status_ = running_;         // running status re-arms the last channel status
      have_ = 0;
      needed_ = midi::data_length(status_);
    }
    data_[have_++] = byte;
    if (have_ == needed_) {
      MidiMessage msg{status_, data_[0], needed_ > 1 ? data_[1] : std::uint8_t{0}};
      if (msg.type() == midi::kNoteOn && msg.d2 == 0) {
        msg = MidiMessage::note_off(msg.channel(), msg.d1, 0);
      }
      sink(msg);
      status_ = 0;  // next data byte re-arms from running_
      have_ = 0;
    }
  }

  template <typename Sink>
  constexpr void feed(const std::uint8_t* bytes, std::size_t n, Sink&& sink) {
    for (std::size_t i = 0; i < n; ++i) feed(bytes[i], sink);
  }

  constexpr void reset() noexcept {
    status_ = 0;
    running_ = 0;
    have_ = 0;
    needed_ = 0;
    in_sysex_ = false;
  }

 private:
  std::uint8_t status_ = 0;   // status of the message being assembled
  std::uint8_t running_ = 0;  // last channel-voice status for running status
  std::uint8_t data_[2] = {0, 0};
  int have_ = 0;
  int needed_ = 0;
  bool in_sysex_ = false;
};

}  // namespace arrangrr

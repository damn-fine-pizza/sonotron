#pragma once

#include <cstdint>

#include "arrangrr/config.hpp"
#include "arrangrr/midi/message.hpp"

// Tracks which notes are sounding on each (out port, channel), including
// notes held only by the sustain pedal, so Panic can silence everything with
// explicit NoteOffs (anti-stuck, §9.D). Observes the OUTPUT stream.

namespace arrangrr {

class NoteTracker {
 public:
  constexpr void observe(std::uint8_t port, const MidiMessage& msg) noexcept {
    if (port >= kMaxPorts || !midi::is_channel_voice(msg.status)) return;
    Channel& ch = state_[port][msg.channel()];
    switch (msg.type()) {
      case midi::kNoteOn:
        set_bit(ch.on, msg.d1);
        clear_bit(ch.sustained, msg.d1);
        ch.used = true;
        break;
      case midi::kNoteOff:
        if (ch.sustain) {
          if (test_bit(ch.on, msg.d1)) set_bit(ch.sustained, msg.d1);
        }
        clear_bit(ch.on, msg.d1);
        ch.used = true;
        break;
      case midi::kControlChange:
        if (msg.d1 == midi::kCcSustain) {
          const bool down = msg.d2 >= 64;
          if (!down) {
            // Pedal released: the synth drops pedal-held notes.
            ch.sustained[0] = ch.sustained[1] = 0;
          }
          ch.sustain = down;
          ch.used = true;
        }
        break;
      default:
        break;
    }
  }

  // Emits, per used (port, channel): explicit NoteOffs for every sounding
  // note, then CC123 (All Notes Off), CC120 (All Sound Off), CC121 (Reset All
  // Controllers). Sink signature: void(uint8_t port, const MidiMessage&).
  template <typename Sink>
  constexpr void panic(Sink&& sink) {
    for (std::uint8_t port = 0; port < kMaxPorts; ++port) {
      for (std::uint8_t c = 0; c < 16; ++c) {
        Channel& ch = state_[port][c];
        if (!ch.used) continue;
        for (int word = 0; word < 2; ++word) {
          const std::uint64_t sounding = ch.on[word] | ch.sustained[word];
          for (int bit = 0; bit < 64; ++bit) {
            if (sounding & (1ull << bit)) {
              sink(port, MidiMessage::note_off(c, static_cast<std::uint8_t>(word * 64 + bit)));
            }
          }
        }
        sink(port, MidiMessage::cc(c, midi::kCcAllNotesOff, 0));
        sink(port, MidiMessage::cc(c, midi::kCcAllSoundOff, 0));
        sink(port, MidiMessage::cc(c, midi::kCcResetAllControllers, 0));
        ch = Channel{};  // full reset, including sustain
      }
    }
  }

  constexpr bool any_sounding(std::uint8_t port, std::uint8_t channel) const noexcept {
    if (port >= kMaxPorts || channel >= 16) return false;
    const Channel& ch = state_[port][channel];
    return (ch.on[0] | ch.on[1] | ch.sustained[0] | ch.sustained[1]) != 0;
  }

 private:
  struct Channel {
    std::uint64_t on[2] = {0, 0};         // notes with a live NoteOn
    std::uint64_t sustained[2] = {0, 0};  // released but held by CC64
    bool sustain = false;
    bool used = false;  // ever saw traffic -> panic targets it
  };

  static constexpr void set_bit(std::uint64_t (&bits)[2], std::uint8_t note) noexcept {
    bits[note >> 6] |= (1ull << (note & 63u));
  }
  static constexpr void clear_bit(std::uint64_t (&bits)[2], std::uint8_t note) noexcept {
    bits[note >> 6] &= ~(1ull << (note & 63u));
  }
  static constexpr bool test_bit(const std::uint64_t (&bits)[2], std::uint8_t note) noexcept {
    return (bits[note >> 6] & (1ull << (note & 63u))) != 0;
  }

  Channel state_[kMaxPorts][16]{};
};

}  // namespace arrangrr

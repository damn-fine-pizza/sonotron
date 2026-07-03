#pragma once

#include <cstdint>

#include "arrangrr/chord/theory.hpp"
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/midi/message.hpp"

// Chord intelligence, mode B (D12): a single input note becomes the diatonic
// chord of its degree in the current key, with D19 smart richness or an
// explicit quality override. This is the live harmonizer half of D13: the
// chord sounds NOW on the configured destination; the recorded ChordSequence
// half arrives with M3.
//
// Voicing (M2): root position, close stack anchored at the played note
// (input D4 -> D4 F4 A4 C5), matching golden G1. Chord changes release the
// previous voicing first — same tick, and the D29 scheduler guarantees
// NoteOffs sort before NoteOns.

namespace arrangrr {

struct ChordResult {
  std::uint8_t root_note = 0;   // MIDI note of the chord root (= input)
  std::int8_t degree = -1;      // 0..6, or -1 when rejected
  ChordQuality quality = ChordQuality::kMaj;
  ChordShape shape;
};

class ChordEngine {
 public:
  using ScheduleFn = FunctionRef<void(std::uint8_t port, const MidiMessage& msg)>;

  constexpr void set_key(const Key& key) noexcept { key_ = key; }
  constexpr const Key& key() const noexcept { return key_; }

  constexpr void set_output(std::uint8_t port, std::uint8_t channel) noexcept {
    out_port_ = port;
    out_channel_ = static_cast<std::uint8_t>(channel & 0x0F);
  }
  constexpr std::uint8_t out_port() const noexcept { return out_port_; }

  constexpr void set_hold(bool hold) noexcept { hold_ = hold; }
  constexpr bool hold() const noexcept { return hold_; }

  // Interprets `note` in the current key. `override_quality` < 0 means smart
  // (D19). Returns degree -1 without sounding anything when the note is
  // chromatic to the key (D20: strictly diatonic, no surprises).
  ChordResult play(std::uint8_t note, std::int8_t override_quality, std::uint8_t velocity,
                   ScheduleFn schedule) {
    ChordResult r;
    r.root_note = note;
    const int degree = theory::degree_of(key_, static_cast<std::uint8_t>(note % 12));
    if (degree < 0) return r;  // not in key: caller warns

    r.degree = static_cast<std::int8_t>(degree);
    r.quality = override_quality >= 0
                    ? static_cast<ChordQuality>(override_quality)
                    : theory::smart_quality(key_.mode, degree);
    r.shape = theory::shape_of(r.quality);
    sound(note, r.quality, velocity, schedule);
    return r;
  }

  // Sounds an already-resolved chord (the ChordSequencer path): releases the
  // previous voicing and stacks the shape from `root_note` upward.
  void sound(std::uint8_t root_note, ChordQuality quality, std::uint8_t velocity,
             ScheduleFn schedule) {
    const ChordShape shape = theory::shape_of(quality);
    release(schedule);  // previous chord off first (same tick, D29 orders it)
    for (std::uint8_t i = 0; i < shape.count; ++i) {
      const int n = root_note + shape.offsets[i];
      if (n > 127) continue;  // clamp: drop tones that leave the range
      sounding_[sounding_count_++] = static_cast<std::uint8_t>(n);
      schedule(out_port_, MidiMessage::note_on(out_channel_, static_cast<std::uint8_t>(n),
                                               velocity));
    }
  }

  // Releases the current voicing (chord stop / transport panic path).
  void release(ScheduleFn schedule) {
    for (std::uint8_t i = 0; i < sounding_count_; ++i) {
      schedule(out_port_, MidiMessage::note_off(out_channel_, sounding_[i]));
    }
    sounding_count_ = 0;
  }

  constexpr bool sounding() const noexcept { return sounding_count_ > 0; }

 private:
  Key key_{};
  std::uint8_t out_port_ = 0;
  std::uint8_t out_channel_ = 0;
  bool hold_ = true;  // stored for the live-keyboard gestures of M5
  std::uint8_t sounding_[4] = {0, 0, 0, 0};
  std::uint8_t sounding_count_ = 0;
};

}  // namespace arrangrr

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

// D12 interpretation modes: how sparse input becomes a chord.
enum class ChordMode : std::uint8_t {
  kDiatonic = 0,  // B: degree in the key, D19 smart quality
  kSingle = 1,    // A: absolute major on the root, no key involved
  kShell = 2,     // C: lowest note is the root, partners complete the chord
};
inline constexpr std::uint8_t kChordModeCount = 3;
inline constexpr std::uint8_t kNoDegree = 0x7F;  // event degree for keyless modes

// The live harmonic context consumed by the arranger's NTT resolution (D24).
struct ChordState {
  std::uint8_t root_pc = 0;
  ChordQuality quality = ChordQuality::kMaj;
  bool valid = false;  // false until the first chord sounds
};

struct ChordResult {
  std::uint8_t root_note = 0;  // MIDI note of the chord root (= input)
  std::int8_t degree = -1;     // 0..6, or -1 when rejected
  ChordQuality quality = ChordQuality::kMaj;
  ChordShape shape;
};

class ChordEngine {
 public:
  using ScheduleFn = FunctionRef<void(std::uint8_t port, const MidiMessage& msg)>;

  constexpr void set_key(const Key& key) noexcept { m_key = key; }
  constexpr const Key& key() const noexcept { return m_key; }

  constexpr void set_output(std::uint8_t port, std::uint8_t channel) noexcept {
    m_out_port = port;
    m_out_channel = static_cast<std::uint8_t>(channel & 0x0F);
  }
  constexpr std::uint8_t out_port() const noexcept { return m_out_port; }

  constexpr void set_hold(bool hold) noexcept { m_hold = hold; }
  constexpr bool hold() const noexcept { return m_hold; }

  constexpr void set_mode(ChordMode mode) noexcept { m_mode = mode; }
  constexpr ChordMode mode() const noexcept { return m_mode; }

  // Interprets `note` in the current key. `override_quality` < 0 means smart
  // (D19). Returns degree -1 without sounding anything when the note is
  // chromatic to the key (D20: strictly diatonic, no surprises).
  ChordResult play(std::uint8_t note, std::int8_t override_quality, std::uint8_t velocity,
                   ScheduleFn schedule) {
    ChordResult r;
    r.root_note = note;
    const int degree = theory::degree_of(m_key, static_cast<std::uint8_t>(note % 12));
    if (degree < 0) {
      return r;  // not in key: caller warns
    }

    r.degree = static_cast<std::int8_t>(degree);
    r.quality = override_quality >= 0 ? static_cast<ChordQuality>(override_quality)
                                      : theory::smart_quality(m_key.mode, degree);
    r.shape = theory::shape_of(r.quality);
    sound(note, r.quality, velocity, schedule);
    return r;
  }

  // Mode A: absolute — the note is a major root, chromatic freely allowed.
  ChordResult play_single(std::uint8_t note, std::int8_t override_quality, std::uint8_t velocity,
                          ScheduleFn schedule) {
    ChordResult r;
    r.root_note = note;
    r.degree = static_cast<std::int8_t>(kNoDegree);
    r.quality =
        override_quality >= 0 ? static_cast<ChordQuality>(override_quality) : ChordQuality::kMaj;
    r.shape = theory::shape_of(r.quality);
    sound(note, r.quality, velocity, schedule);
    return r;
  }

  // Mode C: `notes` (sorted not required) — lowest is the root, the pitch
  // classes above it complete the chord. An override still wins.
  ChordResult play_shell(const std::uint8_t* notes, std::uint8_t count,
                         std::int8_t override_quality, std::uint8_t velocity, ScheduleFn schedule) {
    ChordResult r;
    std::uint8_t root = 127;
    for (std::uint8_t i = 0; i < count; ++i) {
      root = notes[i] < root ? notes[i] : root;
    }
    r.root_note = root;
    r.degree = static_cast<std::int8_t>(kNoDegree);
    std::uint8_t iv[3] = {0, 0, 0};
    std::uint8_t n = 0;
    for (std::uint8_t i = 0; i < count && n < 3; ++i) {
      const std::uint8_t rel = static_cast<std::uint8_t>((notes[i] + 12 - root) % 12);
      if (rel != 0) {
        iv[n++] = rel;
      }
    }
    r.quality = override_quality >= 0 ? static_cast<ChordQuality>(override_quality)
                                      : theory::complete_shell_full(iv, n);
    r.shape = theory::shape_of(r.quality);
    sound(root, r.quality, velocity, schedule);
    return r;
  }

  // Sounds an already-resolved chord (the ChordSequencer path): releases the
  // previous voicing and stacks the shape from `root_note` upward.
  void sound(std::uint8_t root_note, ChordQuality quality, std::uint8_t velocity,
             ScheduleFn schedule) {
    m_state = ChordState{static_cast<std::uint8_t>(root_note % 12), quality, true};
    const ChordShape shape = theory::shape_of(quality);
    release(schedule);  // previous chord off first (same tick, D29 orders it)
    for (std::uint8_t i = 0; i < shape.count; ++i) {
      const int n = root_note + shape.offsets[i];
      if (n > 127) {
        continue;  // clamp: drop tones that leave the range
      }
      m_sounding[m_sounding_count++] = static_cast<std::uint8_t>(n);
      schedule(m_out_port,
               MidiMessage::note_on(m_out_channel, static_cast<std::uint8_t>(n), velocity));
    }
  }

  // Releases the current voicing (chord stop / transport panic path).
  void release(ScheduleFn schedule) {
    for (std::uint8_t i = 0; i < m_sounding_count; ++i) {
      schedule(m_out_port, MidiMessage::note_off(m_out_channel, m_sounding[i]));
    }
    m_sounding_count = 0;
  }

  constexpr bool sounding() const noexcept { return m_sounding_count > 0; }
  constexpr const ChordState& state() const noexcept { return m_state; }

 private:
  Key m_key{};
  ChordMode m_mode = ChordMode::kDiatonic;
  std::uint8_t m_out_port = 0;
  std::uint8_t m_out_channel = 0;
  bool m_hold = true;  // stored for the live-keyboard gestures of M5
  std::uint8_t m_sounding[4] = {0, 0, 0, 0};
  std::uint8_t m_sounding_count = 0;
  ChordState m_state{};
};

}  // namespace arrangrr

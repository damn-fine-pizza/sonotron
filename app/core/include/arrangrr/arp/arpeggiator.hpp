#pragma once

#include <cstdint>

#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/time.hpp"  // Tick, TickOffset, kPpqn

// Arpeggiator engine: turns a set of held notes into a rhythmic sequence on the
// transport clock. Pure and freestanding (no heap, bounded state, no I/O), so
// the same engine serves all three roles from docs/DESIGN.md — a style part, a
// track MIDI-FX, and a live-keyboard effect. Deterministic: the random
// direction is a seeded position hash (D16), so a given seed always replays the
// same pattern.
//
// The engine is clock-driven: on_tick() is called every transport tick; on the
// grid boundary of the current rate it emits the next arp note (the caller
// schedules the note-on now and the note-off after the gate). Notes enter via
// note_on/note_off; latch keeps the chord sounding after the keys are released,
// until a fresh key starts a new chord.

namespace arrangrr {

enum class ArpDirection : std::uint8_t {
  kUp = 0,
  kDown = 1,
  kUpDown = 2,
  kDownUp = 3,
  kAsPlayed = 4,
  kRandom = 5,
};
inline constexpr std::uint8_t kArpDirectionCount = 6;

enum class ArpRate : std::uint8_t {
  kQuarter = 0,
  kEighth = 1,
  kSixteenth = 2,
  kThirtySecond = 3,
};
inline constexpr std::uint8_t kArpRateCount = 4;

// Ticks per arp step for a rate (PPQN 960: 1/4=960, 1/8=480, 1/16=240, 1/32=120).
constexpr std::uint32_t arp_rate_ticks(ArpRate rate) noexcept {
  switch (rate) {
    case ArpRate::kQuarter:
      return kPpqn;
    case ArpRate::kEighth:
      return kPpqn / 2;
    case ArpRate::kSixteenth:
      return kPpqn / 4;
    case ArpRate::kThirtySecond:
      return kPpqn / 8;
  }
  return kPpqn / 4;
}

struct ArpeggiatorParams {
  ArpRate rate = ArpRate::kSixteenth;
  ArpDirection direction = ArpDirection::kUp;
  std::uint8_t octaves = 1;  // 1..4
  std::uint8_t gate = 75;    // 0..100 %
  bool latch = false;
  std::uint32_t seed = 1;  // for kRandom
};

// Field selector for the ABI (kArp) and the host command/panel.
enum class ArpField : std::uint8_t {
  kEnabled = 0,
  kRate = 1,
  kDirection = 2,
  kOctaves = 3,
  kGate = 4,
  kLatch = 5,
  kSeed = 6,
};
inline constexpr std::uint8_t kArpFieldCount = 7;

inline constexpr std::uint8_t kMaxArpNotes = 8;  // held notes the arp plays from
inline constexpr std::uint8_t kMaxArpSeq = kMaxArpNotes * 4 * 2;  // octaves x up-down

class ArpeggiatorEngine {
 public:
  // emit(note, velocity, gate_ticks): the caller schedules note-on now and
  // note-off gate_ticks later.
  using EmitFn = FunctionRef<void(std::uint8_t note, std::uint8_t velocity, TickOffset gate)>;

  constexpr void set_params(const ArpeggiatorParams& params) noexcept { m_params = params; }
  constexpr const ArpeggiatorParams& params() const noexcept { return m_params; }

  void set_field(ArpField field, std::int32_t value) noexcept {
    switch (field) {
      case ArpField::kEnabled:
        break;  // enable is the caller's concern (routing), not the engine's
      case ArpField::kRate:
        m_params.rate = static_cast<ArpRate>(clamp_enum(value, kArpRateCount));
        break;
      case ArpField::kDirection:
        m_params.direction = static_cast<ArpDirection>(clamp_enum(value, kArpDirectionCount));
        break;
      case ArpField::kOctaves:
        m_params.octaves = static_cast<std::uint8_t>(value < 1 ? 1 : (value > 4 ? 4 : value));
        break;
      case ArpField::kGate:
        m_params.gate = static_cast<std::uint8_t>(value < 0 ? 0 : (value > 100 ? 100 : value));
        break;
      case ArpField::kLatch:
        set_latch(value != 0);
        break;
      case ArpField::kSeed:
        m_params.seed = static_cast<std::uint32_t>(value < 0 ? 0 : value);
        break;
    }
  }

  // A key is pressed. With latch on, the first press after a full release starts
  // a fresh chord (clears the held set); further presses extend it.
  void note_on(std::uint8_t note, std::uint8_t velocity) noexcept {
    if (note > 127 || m_count >= kMaxArpNotes) {
      return;
    }
    if (m_params.latch && m_physical == 0) {
      clear();  // new chord under latch
    }
    ++m_physical;
    for (std::uint8_t i = 0; i < m_count; ++i) {
      if (m_notes[i] == note) {
        return;  // already held (retrigger keeps the first velocity)
      }
    }
    m_notes[m_count] = note;
    m_vels[m_count] = velocity == 0 ? 1 : velocity;
    ++m_count;
  }

  void note_off(std::uint8_t note) noexcept {
    if (m_physical > 0) {
      --m_physical;
    }
    if (m_params.latch) {
      return;  // latched notes persist until the next fresh chord
    }
    remove(note);
  }

  void clear() noexcept {
    m_count = 0;
    m_step = 0;
  }
  void panic() noexcept {
    clear();
    m_physical = 0;
  }

  constexpr bool active() const noexcept { return m_count > 0; }
  constexpr std::uint8_t held_count() const noexcept { return m_count; }

  // One transport tick. On the rate-grid boundary, emits the next arp note.
  void on_tick(Tick transport_tick, EmitFn emit) {
    if (m_count == 0) {
      m_step = 0;
      return;
    }
    const std::uint32_t step_ticks = arp_rate_ticks(m_params.rate);
    if (transport_tick % step_ticks != 0) {
      return;
    }
    std::uint8_t seq_note[kMaxArpSeq];
    std::uint8_t seq_vel[kMaxArpSeq];
    const std::uint8_t size = build_sequence(seq_note, seq_vel);
    if (size == 0) {
      return;
    }
    std::uint8_t idx = 0;
    if (m_params.direction == ArpDirection::kRandom) {
      idx = static_cast<std::uint8_t>(hash(m_params.seed, m_step) % size);
    } else {
      idx = static_cast<std::uint8_t>(m_step % size);
    }
    const auto gate_ticks = static_cast<TickOffset>((step_ticks * m_params.gate) / 100);
    emit(seq_note[idx], seq_vel[idx], gate_ticks < 1 ? 1 : gate_ticks);
    ++m_step;
  }

 private:
  static constexpr std::uint8_t clamp_enum(std::int32_t v, std::uint8_t count) noexcept {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v >= count ? count - 1 : v));
  }
  static constexpr std::uint32_t hash(std::uint32_t seed, std::uint32_t step) noexcept {
    std::uint32_t h = seed * 2654435761u + step + 0x9E3779B9u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
  }

  void set_latch(bool on) noexcept {
    m_params.latch = on;
    if (!on) {
      // Turning latch off drops any notes no longer physically held: with none
      // down, the arp stops; the count can't exceed what is physical.
      if (m_physical == 0) {
        clear();
      }
    }
  }

  void remove(std::uint8_t note) noexcept {
    for (std::uint8_t i = 0; i < m_count; ++i) {
      if (m_notes[i] == note) {
        for (std::uint8_t j = i; j + 1 < m_count; ++j) {
          m_notes[j] = m_notes[j + 1];
          m_vels[j] = m_vels[j + 1];
        }
        --m_count;
        return;
      }
    }
  }

  // Builds the ordered arp sequence (pitch + velocity) from the held notes,
  // octaves and direction. Returns the sequence length.
  std::uint8_t build_sequence(std::uint8_t (&out)[kMaxArpSeq],
                              std::uint8_t (&out_vel)[kMaxArpSeq]) const noexcept {
    // Base order: as-played keeps insertion order; every other mode sorts up.
    std::uint8_t base[kMaxArpNotes];
    std::uint8_t base_vel[kMaxArpNotes];
    for (std::uint8_t i = 0; i < m_count; ++i) {
      base[i] = m_notes[i];
      base_vel[i] = m_vels[i];
    }
    if (m_params.direction != ArpDirection::kAsPlayed) {
      for (std::uint8_t i = 1; i < m_count; ++i) {
        for (std::uint8_t j = i; j > 0 && base[j - 1] > base[j]; --j) {
          const std::uint8_t n = base[j];
          base[j] = base[j - 1];
          base[j - 1] = n;
          const std::uint8_t v = base_vel[j];
          base_vel[j] = base_vel[j - 1];
          base_vel[j - 1] = v;
        }
      }
    }

    // Stack octaves ascending.
    std::uint8_t up[kMaxArpNotes * 4];
    std::uint8_t up_vel[kMaxArpNotes * 4];
    std::uint8_t up_n = 0;
    for (std::uint8_t oct = 0; oct < m_params.octaves; ++oct) {
      for (std::uint8_t i = 0; i < m_count; ++i) {
        const int note = base[i] + 12 * oct;
        if (note > 127) {
          continue;
        }
        up[up_n] = static_cast<std::uint8_t>(note);
        up_vel[up_n] = base_vel[i];
        ++up_n;
      }
    }
    if (up_n == 0) {
      return 0;
    }

    // Apply direction over the octave-stacked list.
    std::uint8_t size = 0;
    const auto push = [&](std::uint8_t i) {
      out[size] = up[i];
      out_vel[size] = up_vel[i];
      ++size;
    };
    switch (m_params.direction) {
      case ArpDirection::kDown:
      case ArpDirection::kDownUp:
        for (std::uint8_t i = up_n; i > 0; --i) {
          push(static_cast<std::uint8_t>(i - 1));
        }
        if (m_params.direction == ArpDirection::kDownUp) {
          for (std::uint8_t i = 1; i + 1 < up_n; ++i) {
            push(i);
          }
        }
        break;
      case ArpDirection::kUpDown:
        for (std::uint8_t i = 0; i < up_n; ++i) {
          push(i);
        }
        for (std::uint8_t i = up_n; i > 1; --i) {
          push(static_cast<std::uint8_t>(i - 2));
        }
        break;
      case ArpDirection::kUp:
      case ArpDirection::kAsPlayed:
      case ArpDirection::kRandom:
      default:
        for (std::uint8_t i = 0; i < up_n; ++i) {
          push(i);
        }
        break;
    }
    return size;
  }

  ArpeggiatorParams m_params{};
  std::uint8_t m_notes[kMaxArpNotes] = {};
  std::uint8_t m_vels[kMaxArpNotes] = {};
  std::uint8_t m_count = 0;      // playable notes (survives release under latch)
  std::uint8_t m_physical = 0;   // physically-held count (latch bookkeeping)
  std::uint32_t m_step = 0;      // advancing arp step index
};

}  // namespace arrangrr

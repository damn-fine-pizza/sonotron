#pragma once

#include <cstdint>

#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/midi/message.hpp"

// The Living Timeline primitive (D10): Track = role + MIDI destination +
// per-track length (polymeter) + mute/solo, filled by gestures. M1 ships the
// minimal "write" gesture: one note per grid step on a fixed 16th-note grid.
// Chord-follow transforms (D24) plug in between step data and scheduling in
// later milestones.

namespace arrangrr {

// M1 grid: one step = one sixteenth = 240 scheduler ticks (960 PPQN / 4).
inline constexpr std::uint32_t kTicksPerStep = kPpqn / 4;

// Upper bound on per-step retriggers (the `ratchet` param-lock).
inline constexpr std::uint8_t kMaxRatchet = 8;

enum class TrackRole : std::uint8_t {
  kDrums = 0,
  kPerc = 1,
  kBass = 2,
  kChord1 = 3,
  kChord2 = 4,
  kPad = 5,
  kArp = 6,
  kPhrase = 7,
  kLead = 8,
  kCc = 9,
};

// One grid slot. vel == 0 means the slot is empty (a NoteOn with velocity 0
// would be a NoteOff anyway, so zero is unambiguous as "no event").
//
// The trailing four fields are Elektron-style per-step parameter locks (D16
// determinism, D27 integer micro-timing). They are POD and neutral by default:
// a Step{} with probability=100, ratchet=1, micro=0, tie=false reproduces the
// original one-note-per-step behaviour byte-for-byte. Live RAM cost is the
// whole pool: kMaxTracks x kMaxStepsPerTrack slots.
struct Step {
  std::uint8_t note = 0;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;          // sounding length in scheduler ticks
  std::uint8_t probability = 100;  // 0..100 % chance the step fires (100 = always)
  std::uint8_t ratchet = 1;        // evenly-spaced retriggers within the step (1..8)
  std::int8_t micro = 0;           // micro-timing offset in ticks (signed, on+off together)
  bool tie = false;                // hold into the next step instead of retriggering
};
static_assert(sizeof(Step) == 8,
              "Step must stay 8 bytes: it is live RAM x kMaxTracks x kMaxStepsPerTrack");

struct Track {
  TrackRole role = TrackRole::kLead;
  std::uint8_t port = 0;
  std::uint8_t channel = 0;  // 0-based
  std::uint8_t length = 16;  // active steps; per-track => polymeter
  bool mute = false;
  bool solo = false;
  Step steps[kMaxStepsPerTrack]{};
};

class Timeline {
 public:
  // Schedules msg on `port` at `delay` ticks after the current stream tick.
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay, const MidiMessage& msg)>;

  // Returns the new track index, or -1 when the pool is full.
  int add_track(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    Track t;
    t.role = role;
    t.port = port;
    t.channel = static_cast<std::uint8_t>(channel & 0x0F);
    if (!m_tracks.push_back(t)) {
      return -1;
    }
    return static_cast<int>(m_tracks.size() - 1);
  }

  Track* track(std::size_t idx) noexcept {
    return idx < m_tracks.size() ? &m_tracks[idx] : nullptr;
  }
  const Track* track(std::size_t idx) const noexcept {
    return idx < m_tracks.size() ? &m_tracks[idx] : nullptr;
  }
  std::size_t track_count() const noexcept { return m_tracks.size(); }

  // The four param-lock arguments default to their neutral values, so every
  // existing 5-argument caller keeps the original behaviour unchanged.
  bool set_step(std::size_t idx, std::size_t step, std::uint8_t note, std::uint8_t vel,
                std::uint16_t gate, std::uint8_t probability = 100, std::uint8_t ratchet = 1,
                std::int8_t micro = 0, bool tie = false) noexcept {
    Track* t = track(idx);
    if (t == nullptr || step >= kMaxStepsPerTrack || note > 127 || vel > 127) {
      return false;
    }
    // gate 0 on an audible step would land the NoteOff on the NoteOn's own
    // tick, and the D29 class order (off before on) turns that into a
    // guaranteed stuck note. The binary ABI is the product boundary (D26):
    // the invariant lives here, not in a host-side check.
    if (vel > 0 && gate == 0) {
      return false;
    }
    Step s;
    s.note = note;
    s.vel = vel;
    s.gate = gate;
    s.probability = probability > 100 ? 100 : probability;
    s.ratchet = ratchet < 1 ? 1 : (ratchet > kMaxRatchet ? kMaxRatchet : ratchet);
    s.micro = micro;
    s.tie = tie;
    t->steps[step] = s;
    return true;
  }

  bool set_length(std::size_t idx, std::size_t steps) noexcept {
    Track* t = track(idx);
    if (t == nullptr || steps == 0 || steps > kMaxStepsPerTrack) {
      return false;
    }
    t->length = static_cast<std::uint8_t>(steps);
    return true;
  }

  bool any_solo() const noexcept {
    for (const Track& t : m_tracks) {
      if (t.solo) {
        return true;
      }
    }
    return false;
  }

  // Fires the grid slots that fall on `transport_tick` (call once per tick
  // while the transport plays; also with tick 0 right after start).
  void on_tick(Tick transport_tick, ScheduleFn schedule) {
    if (transport_tick % kTicksPerStep != 0) {
      return;
    }
    const std::uint32_t global_step = transport_tick / kTicksPerStep;
    const bool solo_active = any_solo();
    for (std::size_t ti = 0; ti < m_tracks.size(); ++ti) {
      const Track& t = m_tracks[ti];
      if (t.mute || (solo_active && !t.solo)) {
        continue;
      }
      const Step& s = t.steps[global_step % t.length];
      if (s.vel == 0) {
        continue;
      }
      // Probability gate (D16): a seeded position hash keyed on track index and
      // global step position. 100 % never consults the hash, so a neutral step
      // is byte-identical to the original path; same position => same verdict.
      if (s.probability < 100 &&
          hash(static_cast<std::uint32_t>(ti), global_step) % 100u >= s.probability) {
        continue;
      }
      emit_step(t, s, schedule);
    }
  }

 private:
  // Same seeded position hash as the arpeggiator (D16): pure, reproducible.
  static constexpr std::uint32_t hash(std::uint32_t seed, std::uint32_t pos) noexcept {
    std::uint32_t h = seed * 2654435761u + pos + 0x9E3779B9u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
  }

  // Micro-timing shift, clamped so the absolute tick can never go negative and
  // the D29 off-before-on order is preserved (off = on + gate, gate >= 1).
  static constexpr TickOffset shift_delay(TickOffset base, std::int8_t micro) noexcept {
    const TickOffset d = base + static_cast<TickOffset>(micro);
    return d < 0 ? 0 : d;
  }

  // Emits one firing step: a tie bridges the gate into the next step, otherwise
  // `ratchet` evenly-spaced micro-shifted note-on/off pairs (ratchet 1 = the
  // original single hit).
  static void emit_step(const Track& t, const Step& s, ScheduleFn& schedule) {
    const MidiMessage on = MidiMessage::note_on(t.channel, s.note, s.vel);
    const MidiMessage off = MidiMessage::note_off(t.channel, s.note);

    if (s.tie) {
      // Hold across the boundary: bridge the note-off one full step later so the
      // note sustains into the next step instead of retriggering.
      const TickOffset on_delay = shift_delay(0, s.micro);
      const auto off_delay = static_cast<TickOffset>(on_delay + static_cast<TickOffset>(s.gate) +
                                                     static_cast<TickOffset>(kTicksPerStep));
      schedule(t.port, on_delay, on);
      schedule(t.port, off_delay, off);
      return;
    }

    std::uint8_t ratchet = s.ratchet < 1 ? 1 : (s.ratchet > kMaxRatchet ? kMaxRatchet : s.ratchet);
    if (ratchet <= 1) {
      const TickOffset on_delay = shift_delay(0, s.micro);
      schedule(t.port, on_delay, on);
      schedule(t.port, static_cast<TickOffset>(on_delay + static_cast<TickOffset>(s.gate)), off);
      return;
    }

    // Subdivide the step into `ratchet` slices; bound each sub-hit inside its
    // slice so consecutive retriggers never overlap (D29 stays clean).
    const std::uint32_t slice = kTicksPerStep / ratchet;
    std::uint16_t sub_gate = static_cast<std::uint16_t>(slice > s.gate ? s.gate : slice);
    if (sub_gate == 0) {
      sub_gate = 1;
    }
    for (std::uint8_t i = 0; i < ratchet; ++i) {
      const auto base = static_cast<TickOffset>(static_cast<std::uint32_t>(i) * slice);
      const TickOffset on_delay = shift_delay(base, s.micro);
      schedule(t.port, on_delay, on);
      schedule(t.port, static_cast<TickOffset>(on_delay + static_cast<TickOffset>(sub_gate)), off);
    }
  }

  StaticVector<Track, kMaxTracks> m_tracks;
};

}  // namespace arrangrr

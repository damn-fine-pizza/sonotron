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
struct Step {
  std::uint8_t note = 0;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;  // sounding length in scheduler ticks
};

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
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay,
                                      const MidiMessage& msg)>;

  // Returns the new track index, or -1 when the pool is full.
  int add_track(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    Track t;
    t.role = role;
    t.port = port;
    t.channel = static_cast<std::uint8_t>(channel & 0x0F);
    if (!tracks_.push_back(t)) return -1;
    return static_cast<int>(tracks_.size() - 1);
  }

  Track* track(std::size_t idx) noexcept {
    return idx < tracks_.size() ? &tracks_[idx] : nullptr;
  }
  std::size_t track_count() const noexcept { return tracks_.size(); }

  bool set_step(std::size_t idx, std::size_t step, std::uint8_t note, std::uint8_t vel,
                std::uint16_t gate) noexcept {
    Track* t = track(idx);
    if (t == nullptr || step >= kMaxStepsPerTrack || note > 127 || vel > 127) return false;
    t->steps[step] = Step{note, vel, gate};
    return true;
  }

  bool set_length(std::size_t idx, std::size_t steps) noexcept {
    Track* t = track(idx);
    if (t == nullptr || steps == 0 || steps > kMaxStepsPerTrack) return false;
    t->length = static_cast<std::uint8_t>(steps);
    return true;
  }

  bool any_solo() const noexcept {
    for (const Track& t : tracks_)
      if (t.solo) return true;
    return false;
  }

  // Fires the grid slots that fall on `transport_tick` (call once per tick
  // while the transport plays; also with tick 0 right after start).
  void on_tick(Tick transport_tick, ScheduleFn schedule) {
    if (transport_tick % kTicksPerStep != 0) return;
    const std::uint32_t global_step = transport_tick / kTicksPerStep;
    const bool solo_active = any_solo();
    for (const Track& t : tracks_) {
      if (t.mute || (solo_active && !t.solo)) continue;
      const Step& s = t.steps[global_step % t.length];
      if (s.vel == 0) continue;
      schedule(t.port, 0, MidiMessage::note_on(t.channel, s.note, s.vel));
      schedule(t.port, static_cast<TickOffset>(s.gate),
               MidiMessage::note_off(t.channel, s.note));
    }
  }

 private:
  StaticVector<Track, kMaxTracks> tracks_;
};

}  // namespace arrangrr

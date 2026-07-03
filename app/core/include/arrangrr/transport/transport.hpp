#pragma once

#include <cstdint>

#include "arrangrr/common/time.hpp"

// Transport: musical position + run state at 960 internal PPQN (D27).
// It owns no I/O and never reads a clock — ticks are injected (§2 principle 1):
// the host real-clock thread or the virtual-clock driver calls the engine,
// which advances the transport. Start/stop/continue are timeline events (D29).

namespace arrangrr {

enum class TransportState : std::uint8_t {
  kStopped = 0,
  kPlaying = 1,
  kPaused = 2,
};

// Minimal 4/4 metric for M0 (a real TimeSignature engine lands with M5/M6;
// pinned here so bar:beat:tick exists from day one — review nit M5).
inline constexpr std::uint32_t kBeatsPerBar = 4;
inline constexpr std::uint32_t kTicksPerBeat = kPpqn;
inline constexpr std::uint32_t kTicksPerBar = kBeatsPerBar * kTicksPerBeat;

struct Position {
  std::uint32_t bar;   // 1-based
  std::uint8_t beat;   // 1-based, 1..kBeatsPerBar
  std::uint16_t tick;  // 0..kTicksPerBeat-1
};

class Transport {
 public:
  constexpr TransportState state() const noexcept { return state_; }
  constexpr bool playing() const noexcept { return state_ == TransportState::kPlaying; }
  constexpr Tick tick() const noexcept { return tick_; }

  constexpr BpmX100 bpm() const noexcept { return bpm_; }
  constexpr bool set_bpm(BpmX100 bpm) noexcept {
    if (bpm < kMinBpm || bpm > kMaxBpm) return false;
    bpm_ = bpm;
    return true;
  }

  // MIDI Start semantics: rewind to zero and play.
  constexpr void start() noexcept {
    tick_ = 0;
    state_ = TransportState::kPlaying;
  }
  // MIDI Continue semantics: play from the current position.
  constexpr void resume() noexcept { state_ = TransportState::kPlaying; }
  constexpr void stop() noexcept { state_ = TransportState::kStopped; }

  constexpr void locate(Tick t) noexcept { tick_ = t; }

  // Advance by exactly one tick; caller loops (the engine reacts per tick).
  constexpr void advance_one() noexcept { ++tick_; }

  // True when a MIDI clock byte (F8) belongs on this tick (960/24 = 40).
  static constexpr bool is_midi_clock_tick(Tick t) noexcept {
    return t % kMidiClockDivider == 0;
  }

  constexpr Position position() const noexcept {
    return Position{
        .bar = tick_ / kTicksPerBar + 1,
        .beat = static_cast<std::uint8_t>((tick_ % kTicksPerBar) / kTicksPerBeat + 1),
        .tick = static_cast<std::uint16_t>(tick_ % kTicksPerBeat),
    };
  }

 private:
  Tick tick_ = 0;
  BpmX100 bpm_ = kDefaultBpm;
  TransportState state_ = TransportState::kStopped;
};

}  // namespace arrangrr

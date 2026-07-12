#pragma once

#include <cstdint>

#include "common/time.hpp"

// Transport: musical position + run state at 960 internal PPQN (D27).
// It owns no I/O and never reads a clock — ticks are injected (§2 principle 1):
// the host real-clock thread or the virtual-clock driver calls the engine,
// which advances the transport. Start/stop/continue are timeline events (D29).
//
// Phase-1 runtime extraction: moved out of components/arrangrr into
// components/runtime byte-for-byte, minus the 3 bar/beat constants (now in
// common/time.hpp, needed by arrangrr-side modules that must not depend on
// runtime). Namespace stays `arrangrr` for Phase 1 (minimal churn); a later
// pass may rename to `namespace runtime` once the Stage port stabilizes.

namespace arrangrr {

enum class TransportState : std::uint8_t {
  kStopped = 0,
  kPlaying = 1,
  kPaused = 2,
};

struct Position {
  std::uint32_t bar;   // 1-based
  std::uint8_t beat;   // 1-based, 1..kBeatsPerBar
  std::uint16_t tick;  // 0..kTicksPerBeat-1
};

class Transport {
 public:
  constexpr TransportState state() const noexcept { return m_state; }
  constexpr bool playing() const noexcept { return m_state == TransportState::kPlaying; }
  constexpr Tick tick() const noexcept { return m_tick; }

  constexpr BpmX100 bpm() const noexcept { return m_bpm; }
  constexpr bool set_bpm(BpmX100 bpm) noexcept {
    if (bpm < kMinBpm || bpm > kMaxBpm) {
      return false;
    }
    m_bpm = bpm;
    return true;
  }

  // MIDI Start semantics: rewind to zero and play.
  constexpr void start() noexcept {
    m_tick = 0;
    m_state = TransportState::kPlaying;
  }
  // MIDI Continue semantics: play from the current position.
  constexpr void resume() noexcept { m_state = TransportState::kPlaying; }
  constexpr void stop() noexcept { m_state = TransportState::kStopped; }

  constexpr void locate(Tick t) noexcept { m_tick = t; }

  // Advance by exactly one tick; caller loops (the engine reacts per tick).
  constexpr void advance_one() noexcept { ++m_tick; }

  // True when a MIDI clock byte (F8) belongs on this tick (960/24 = 40).
  static constexpr bool is_midi_clock_tick(Tick t) noexcept { return t % kMidiClockDivider == 0; }

  constexpr Position position() const noexcept {
    return Position{
        .bar = m_tick / kTicksPerBar + 1,
        .beat = static_cast<std::uint8_t>((m_tick % kTicksPerBar) / kTicksPerBeat + 1),
        .tick = static_cast<std::uint16_t>(m_tick % kTicksPerBeat),
    };
  }

 private:
  Tick m_tick = 0;
  BpmX100 m_bpm = kDefaultBpm;
  TransportState m_state = TransportState::kStopped;
};

}  // namespace arrangrr

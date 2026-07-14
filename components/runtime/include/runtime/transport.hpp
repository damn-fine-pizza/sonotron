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

  // Phase 7 (node T0): the variable time-signature engine, mirroring the
  // EXACT pattern above for tempo -- a small, bounded, runtime-mutable
  // musical parameter that changes rarely (style load / scene change) and is
  // read often. set_time_sig validates like set_bpm (rejects out-of-range,
  // state unchanged on reject); ticks_per_bar() is a convenience forwarder
  // used at every threaded call boundary (Arranger::on_tick, ClipMatrix::
  // on_bar, BoundaryLatch::arm, ...).
  constexpr const TimeSig& time_sig() const noexcept { return m_time_sig; }
  constexpr bool set_time_sig(std::uint8_t beats_per_bar) noexcept {
    if (beats_per_bar < kMinBeatsPerBar || beats_per_bar > kMaxBeatsPerBar) {
      return false;
    }
    m_time_sig.beats_per_bar = beats_per_bar;
    return true;
  }
  constexpr Tick ticks_per_bar() const noexcept { return m_time_sig.ticks_per_bar(); }

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

  // Phase 7 (node T0): derives bar/beat/tick from the LIVE time signature
  // (m_time_sig), not the two compile-time constants -- a default-constructed
  // TimeSig computes the identical numeric value (common/time.hpp's own
  // byte-identity gate), so this is unchanged for every caller that never
  // touches set_time_sig.
  constexpr Position position() const noexcept {
    const Tick tpbar = m_time_sig.ticks_per_bar();
    const Tick tpbeat = m_time_sig.ticks_per_beat;
    return Position{
        .bar = m_tick / tpbar + 1,
        .beat = static_cast<std::uint8_t>((m_tick % tpbar) / tpbeat + 1),
        .tick = static_cast<std::uint16_t>(m_tick % tpbeat),
    };
  }

 private:
  Tick m_tick = 0;
  BpmX100 m_bpm = kDefaultBpm;
  TimeSig m_time_sig{};
  TransportState m_state = TransportState::kStopped;
};

}  // namespace arrangrr

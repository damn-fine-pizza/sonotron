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

  // Phase 7 (node 8100 finding, T0 follow-up): the bar-boundary GATE, tracked
  // explicitly instead of the tempting `tick % ticks_per_bar() == 0` absolute
  // modulo. That modulo only reproduces the correct boundary PHASE for a
  // CONSTANT meter -- the moment ticks_per_bar() changes mid-song
  // (set_time_sig above), the modulo silently re-phases the whole grid back
  // to whatever tick 0 would have produced under the NEW meter, instead of
  // continuing the grid from the tick the change actually landed on. Example:
  // a 4/4 song reaches tick 8q and switches to 3/4; the correct next boundary
  // is 8q + 3q = 11q, but `tick % 3q == 0` fires early, at 9q. This corrupts
  // every consumer gated on it (chord commit_bar, fire_clips, a pending
  // Performance recall, pad fires, and node 8100's own SceneChain::on_bar
  // driver) -- latent until now because no existing caller ever changed the
  // meter mid-song before node 8100 (T0's own style-load time-sig seed only
  // ever fires once, at a style LOAD/switch boundary Arranger itself already
  // gates independently).
  //
  // m_next_bar_tick holds the tick the CURRENT boundary lands on; advance_bar_
  // tick() (called by the caller AFTER it has finished reacting to this
  // boundary, so any set_time_sig made THIS tick already sizes the increment)
  // re-anchors it at the tick just consumed, so a meter change takes effect
  // from the transition point forward, never retroactively re-phasing
  // anything before it.
  //
  // Byte-identical for the default (never-changing) meter: m_next_bar_tick
  // starts at 0 (matching m_tick's own default) and is always advanced by
  // exactly the CURRENT ticks_per_bar() -- for a constant meter that produces
  // the identical sequence 0, kTicksPerBar, 2*kTicksPerBar, ... that
  // `tick % kTicksPerBar == 0` used to select, bit for bit.
  constexpr bool at_bar_boundary() const noexcept { return m_tick == m_next_bar_tick; }
  constexpr void advance_bar_tick() noexcept { m_next_bar_tick = m_tick + ticks_per_bar(); }

  // MIDI Start semantics: rewind to zero and play.
  constexpr void start() noexcept {
    m_tick = 0;
    m_next_bar_tick = 0;
    m_state = TransportState::kPlaying;
  }
  // MIDI Continue semantics: play from the current position.
  constexpr void resume() noexcept { m_state = TransportState::kPlaying; }
  constexpr void stop() noexcept { m_state = TransportState::kStopped; }

  // Re-anchors the bar grid at the new position (mirrors start()'s own
  // tick-0/next-bar-tick-0 pairing): a direct position jump is always treated
  // as a fresh bar start, never a stale mid-grid offset.
  constexpr void locate(Tick t) noexcept {
    m_tick = t;
    m_next_bar_tick = t;
  }

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
  Tick m_next_bar_tick = 0;  // Phase 7 (node 8100 finding): re-anchored bar-boundary gate
  BpmX100 m_bpm = kDefaultBpm;
  TimeSig m_time_sig{};
  TransportState m_state = TransportState::kStopped;
};

}  // namespace arrangrr

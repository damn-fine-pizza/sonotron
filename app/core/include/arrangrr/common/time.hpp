#pragma once

#include <cstdint>

// Musical time constants and fixed-point tempo math (D27, D33).
// All integers — no float anywhere near the realtime path (D32).

namespace arrangrr {

// Internal scheduling resolution: 960 pulses per quarter note (D27).
// 0.52 ms per tick @120 BPM; MIDI clock F8 divides evenly (960 / 24 = 40).
inline constexpr std::uint32_t kPpqn = 960;
inline constexpr std::uint32_t kMidiClockDivider = kPpqn / 24;
static_assert(kPpqn % 24 == 0, "internal PPQN must divide evenly into MIDI clock");
static_assert(kMidiClockDivider == 40);

// The musical grid users think in (quantize/view): 96 PPQN (D27).
inline constexpr std::uint32_t kGridPpqn = 96;
static_assert(kPpqn % kGridPpqn == 0);
inline constexpr std::uint32_t kTicksPerGridStep = kPpqn / kGridPpqn;  // 10

// Absolute musical time in internal ticks. u32 spans ~4 days @120 BPM (D33).
using Tick = std::uint32_t;
// Signed tick arithmetic (offsets, swing, humanize).
using TickOffset = std::int32_t;

// Tempo as beats-per-minute x100: 12000 = 120.00 BPM (D3/D27).
using BpmX100 = std::uint32_t;
inline constexpr BpmX100 kDefaultBpm = 12000;
inline constexpr BpmX100 kMinBpm = 2000;    // 20.00 BPM
inline constexpr BpmX100 kMaxBpm = 40000;   // 400.00 BPM

// Exact rational tick accumulation (no drift, u64-safe — no __int128, which
// 32-bit arm lacks):
//   ticks_per_us = bpm_x100 * kPpqn / kTickDenominator
// Derivation: bpm/100 beats per 60e6 us -> ticks/us = (bpm_x100/100)*kPpqn/60e6.
inline constexpr std::uint64_t kTickDenominator = 6'000'000'000ull;  // 60e6 us * 100

// Drift-free tick source: feed elapsed microseconds, harvest whole ticks.
// Numerator per call stays tiny (elapsed_us * bpm * ppqn), far below u64 range.
class TickAccumulator {
 public:
  constexpr void set_bpm(BpmX100 bpm) noexcept { bpm_ = bpm; }
  constexpr BpmX100 bpm() const noexcept { return bpm_; }

  // Returns the number of whole ticks that elapsed in `elapsed_us`.
  constexpr std::uint32_t advance_us(std::uint64_t elapsed_us) noexcept {
    acc_ += elapsed_us * static_cast<std::uint64_t>(bpm_) * kPpqn;
    const std::uint64_t ticks = acc_ / kTickDenominator;
    acc_ -= ticks * kTickDenominator;
    return static_cast<std::uint32_t>(ticks);
  }

  constexpr void reset() noexcept { acc_ = 0; }

 private:
  std::uint64_t acc_ = 0;
  BpmX100 bpm_ = kDefaultBpm;
};

}  // namespace arrangrr

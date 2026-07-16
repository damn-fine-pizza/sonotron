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

// Minimal 4/4 metric for M0 (a real TimeSignature engine lands with M5/M6;
// pinned here so bar:beat:tick exists from day one — review nit M5). Hoisted
// out of runtime/transport.hpp during the Phase-1 runtime extraction: arrangrr
// -side modules (arranger, chord_sequence) need these constants without
// depending on the runtime component that now owns Transport itself.
inline constexpr std::uint32_t kBeatsPerBar = 4;
inline constexpr std::uint32_t kTicksPerBeat = kPpqn;
inline constexpr std::uint32_t kTicksPerBar = kBeatsPerBar * kTicksPerBeat;

// Absolute musical time in internal ticks. u32 spans ~4 days @120 BPM (D33).
using Tick = std::uint32_t;
// Signed tick arithmetic (offsets, swing, humanize).
using TickOffset = std::int32_t;

// Phase 7 (node T0, prerequisite for 6000/8100): the variable time-signature
// engine. F1 (owner-locked, numerator-only for v1): only `beats_per_bar`
// (the numerator) varies at runtime; the beat UNIT stays pinned to
// kTicksPerBeat (== kPpqn, a quarter-note beat) for every time signature --
// true compound meters (6/8 felt as 2 dotted-quarter beats, not 6) would need
// `ticks_per_beat` to vary too, which touches the kPpqn/kGridPpqn compile-time
// relationship above and is deliberately out of scope here.
//
// Byte-identity gate: the default member initializers below are the LITERAL
// two constants whose product already defined kTicksPerBar -- a
// default-constructed TimeSig therefore computes ticks_per_bar() ==
// kBeatsPerBar * kTicksPerBeat == kTicksPerBar, bit-for-bit, not
// approximately. Every one of the ~13 production sites that used to read the
// compile-time kTicksPerBar/kTicksPerBeat constants directly now reads this
// struct's (or a threaded copy of its) runtime value instead, so every
// existing golden stays byte-identical for free until something genuinely
// calls Transport::set_time_sig with a non-4/4 value.
struct TimeSig {
  std::uint8_t beats_per_bar = kBeatsPerBar;     // numerator; runtime-variable, T0
  std::uint32_t ticks_per_beat = kTicksPerBeat;  // denominator granularity; PINNED
                                                 // to kPpqn for v1 (F1)
  constexpr Tick ticks_per_bar() const noexcept {
    return static_cast<Tick>(beats_per_bar) * ticks_per_beat;
  }
};
inline constexpr std::uint8_t kMinBeatsPerBar = 1;
// F2 (owner-locked): a small, honest cap living beside the carrier's own
// default/definition (common/time.hpp), NOT arrangrr/config.hpp -- the
// dependency runs arrangrr -> runtime -> common, never the reverse
// (runtime/transport.hpp, the carrier, depends only on common/time.hpp), so a
// bound for the carrier's own validated setter belongs down here.
inline constexpr std::uint8_t kMaxBeatsPerBar = 16;

// Tempo as beats-per-minute x100: 12000 = 120.00 BPM (D3/D27).
using BpmX100 = std::uint32_t;
inline constexpr BpmX100 kDefaultBpm = 12000;
inline constexpr BpmX100 kMinBpm = 2000;   // 20.00 BPM
inline constexpr BpmX100 kMaxBpm = 40000;  // 400.00 BPM

// Exact rational tick accumulation (no drift, u64-safe — no __int128, which
// 32-bit arm lacks):
//   ticks_per_us = bpm_x100 * kPpqn / kTickDenominator
// Derivation: bpm/100 beats per 60e6 us -> ticks/us = (bpm_x100/100)*kPpqn/60e6.
inline constexpr std::uint64_t kTickDenominator = 6'000'000'000ull;  // 60e6 us * 100

// Drift-free tick source: feed elapsed microseconds, harvest whole ticks.
// Numerator per call stays tiny (elapsed_us * bpm * ppqn), far below u64 range.
class TickAccumulator {
 public:
  constexpr void set_bpm(BpmX100 bpm) noexcept { m_bpm = bpm; }
  constexpr BpmX100 bpm() const noexcept { return m_bpm; }

  // Returns the number of whole ticks that elapsed in `elapsed_us`.
  constexpr std::uint32_t advance_us(std::uint64_t elapsed_us) noexcept {
    m_acc += elapsed_us * static_cast<std::uint64_t>(m_bpm) * kPpqn;
    const std::uint64_t ticks = m_acc / kTickDenominator;
    m_acc -= ticks * kTickDenominator;
    return static_cast<std::uint32_t>(ticks);
  }

  constexpr void reset() noexcept { m_acc = 0; }

 private:
  std::uint64_t m_acc = 0;
  BpmX100 m_bpm = kDefaultBpm;
};

}  // namespace arrangrr

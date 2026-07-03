#pragma once

#include <cstddef>

// Bounded-capacity configuration, anchored to the STM32H743 512 KB pool
// envelope (D33). Every MAX here is a compile-time constant; static_asserts
// keep the totals honest.

namespace arrangrr {

inline constexpr std::size_t kMaxPorts = 4;        // DIN in/out + USB in/out (D6)
inline constexpr std::size_t kMaxRoutes = 32;      // routing matrix entries
inline constexpr std::size_t kSchedulerCapacity = 4096;  // out-queue entries (D33)
inline constexpr std::size_t kMaxTracks = 16;            // timeline tracks
inline constexpr std::size_t kMaxStepsPerTrack = 64;     // write-gesture grid slots
inline constexpr std::size_t kMaxChordSequences = 16;    // D33: 16 x 128 x 12 B = 24 KB
inline constexpr std::size_t kMaxChordSteps = 128;       // free-duration steps per sequence

}  // namespace arrangrr

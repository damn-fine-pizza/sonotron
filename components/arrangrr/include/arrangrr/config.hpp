#pragma once

#include <cstddef>

// Bounded-capacity configuration, anchored to the STM32H743 512 KB pool
// envelope (D33). Every MAX here is a compile-time constant; static_asserts
// keep the totals honest.

namespace arrangrr {

inline constexpr std::size_t kMaxPorts = 4;              // DIN in/out + USB in/out (D6)
inline constexpr std::size_t kMaxRoutes = 32;            // routing matrix entries
inline constexpr std::size_t kSchedulerCapacity = 4096;  // out-queue entries (D33)
inline constexpr std::size_t kMaxTracks = 16;            // timeline tracks
inline constexpr std::size_t kMaxStepsPerTrack = 64;     // write-gesture grid slots
inline constexpr std::size_t kMaxChordSequences = 16;    // D33: 16 x 128 x 12 B = 24 KB
inline constexpr std::size_t kMaxChordSteps = 128;       // free-duration steps per sequence

// Repeat-Zone grid pool (Phase-5 Item #2, docs/design/clip-primitive-design.md):
// one ClipMatrix slot per launchable grid cell. Headroom over the GUI's own
// kRoleCount(10) x GridModel::kMaxSceneCount(8) = 80 cells
// (apps/gui-sonotron/src/grid_model.hpp).
inline constexpr std::size_t kMaxClips = 96;
// Budget (D33): Clip is a tiny 8-byte POD (TrackRole + scene_index +
// ContentKind + content_index + LaunchState + n_bars) -- pinned by
// static_assert(sizeof(Clip) == 8) in arrangrr/clip/clip_matrix.hpp, so even
// generous headroom stays a trivial slice of the STM32H743 512 KB envelope
// (kMaxClips x 8 B <= 2 KB well below kSchedulerCapacity's own 64 KB pool).
// Capped here so a future caller cannot silently balloon the grid past a sane
// size.
static_assert(kMaxClips <= 256, "ClipMatrix pool: keep the Repeat-Zone grid bounded (D33)");

}  // namespace arrangrr

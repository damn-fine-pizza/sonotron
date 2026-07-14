#pragma once

#include <cstddef>
#include <cstdint>

#include "arrangrr/abi.hpp"     // Boundary
#include "arrangrr/config.hpp"  // kMaxPads

// Pad banks (Phase-5 Item #9, docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): 8 banks x 4 pads = 32 flat, host-addressable trigger slots.
// WRAPPER-ONLY (locked decision): every PadType fans out to an EXISTING verb
// (the clip-launch primitive, an Arranger section request, or a Performance
// recall) -- there is NO new note/CC emission engine here. PadEngine is pure
// bookkeeping, exactly like ClipMatrix: it does NOT touch Arranger/ClipMatrix/
// PerformanceStore itself; Engine (arrangrr/src/engine.cpp's fire_pad)
// translates a fired pad into a call on the subsystem it already owns.

namespace arrangrr {

enum class PadType : std::uint8_t {
  kNone = 0,
  kPhrase = 1,       // wraps a ClipMatrix clip (ContentKind agnostic to the pad itself)
  kChord = 2,        // wraps a ClipMatrix clip (same launch path as kPhrase)
  kSceneColumn = 3,  // wraps a Repeat-Zone scene launch (clip_scene_launch)
  kVariation = 4,    // wraps an Arranger section request (a variation SectionType)
  kFill = 5,         // wraps an Arranger section request (a fill SectionType)
  kPerformance = 6,  // wraps a Performance recall
};

enum class PadMode : std::uint8_t {
  kOneShot = 0,  // trigger launches; release is a no-op
  kLoop = 1,     // trigger launches; the wrapped content loops on its own terms
  kHold = 2,     // trigger launches; release stops
  kToggle = 3,   // trigger flips launched/stopped
};

// Whether the pad's wrapped content transposes with the followed chord.
// CAPTURED but NOT YET WIRED to any dispatch behavior in v1 (like
// Performance::master_transpose/controller_map_id, this is a documented
// reserved field -- the wrapper-only scope tripwire forbids inventing a new
// per-pad transpose engine; a future item can consume it without a shape
// change here).
enum class PadPitch : std::uint8_t {
  kFixed = 0,
  kTransposeWithChord = 1,
};

// One pad's wrapper configuration. 12-byte POD, index-referenced (never a
// pointer/variant), mirroring Clip's own discipline (clip_matrix.hpp).
struct Pad {
  PadType type = PadType::kNone;
  PadMode mode = PadMode::kOneShot;
  Boundary sync = Boundary::kImmediate;  // this pad's OWN quantize boundary (not the trigger
                                         // Command's -- pad_trigger ignores the trigger
                                         // Command's boundary/n_bars entirely)
  PadPitch pitch = PadPitch::kFixed;
  std::uint8_t n_bars = 1;        // meaningful only when sync == Boundary::kNextNBars
  std::uint8_t dest_port = 0;     // reserved destination (see the header comment: not yet
  std::uint8_t dest_channel = 0;  // consumed by fire_pad's wrapper-only dispatch in v1)
  // Meaning depends on `type`:
  //   kPhrase / kChord    -> ClipMatrix clip id
  //   kSceneColumn        -> Repeat-Zone scene index
  //   kVariation / kFill  -> SectionType
  //   kPerformance        -> PerformanceStore slot
  //   kNone               -> unused
  std::uint16_t source_idx = 0;
  std::uint8_t source_aux = 0;  // reserved per-type auxiliary selector (unused v1)
};
static_assert(sizeof(Pad) == 12, "Pad RAM/wire budget pin (D33)");

// Transient runtime state, NEVER persisted (a Performance snapshots the RIG,
// not which pads happen to be lit right now).
struct PadRuntime {
  bool on = false;  // kToggle/kHold sounding state
};

// Pure bookkeeping pool, exactly like ClipMatrix: assign()/get() configure and
// read a pad; Engine (fire_pad, engine.cpp) is the ONE place that drives the
// subsystem a fired pad wraps.
class PadEngine {
 public:
  bool assign(std::size_t flat_id, const Pad& pad) noexcept {
    if (flat_id >= kMaxPads) {
      return false;
    }
    m_pads[flat_id] = pad;
    m_runtime[flat_id] = PadRuntime{};  // a re-assign drops any stale on/off state
    return true;
  }
  const Pad* get(std::size_t flat_id) const noexcept {
    return flat_id < kMaxPads ? &m_pads[flat_id] : nullptr;
  }
  PadRuntime* runtime(std::size_t flat_id) noexcept {
    return flat_id < kMaxPads ? &m_runtime[flat_id] : nullptr;
  }
  const PadRuntime* runtime(std::size_t flat_id) const noexcept {
    return flat_id < kMaxPads ? &m_runtime[flat_id] : nullptr;
  }

 private:
  Pad m_pads[kMaxPads]{};
  PadRuntime m_runtime[kMaxPads]{};
};

}  // namespace arrangrr

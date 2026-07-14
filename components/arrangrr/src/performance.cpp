#include "arrangrr/perf/performance.hpp"

#include "arrangrr/arranger/style.hpp"      // styles::kBuiltinCount, SectionType/kSectionTypeCount
#include "arrangrr/chord/chord_engine.hpp"  // ChordMode/kChordModeCount, and (transitively via
                                            // chorddet/theory.hpp + chorddet/followed_context.hpp)
                                            // Mode/kModeCount and ChordFollow

// perf::validate's definition lives here, not in performance.hpp: it is the
// ONE piece of this header's surface that needs the heavier arranger/style +
// chord vocabulary (see performance.hpp's own comment on the declaration).
// Free function, no Engine state, freestanding/dual-target -- Engine::
// validate_performance (engine.cpp) is a thin forwarder that supplies the
// one live value this needs (ChordSequencer::count()).

namespace arrangrr {
namespace perf {

bool validate(const Performance& p, std::size_t chord_sequence_count) noexcept {
  if (p.style_id != 0xFFFF && p.style_id >= styles::kBuiltinCount) {
    return false;
  }
  if (p.variation >= kSectionTypeCount) {
    return false;
  }
  if (p.chord_sequence_id != 0xFFFF &&
      static_cast<std::size_t>(p.chord_sequence_id) >= chord_sequence_count) {
    return false;
  }
  constexpr std::uint32_t kRoleMask = (1u << 10) - 1;
  if ((p.track_mute_mask & ~kRoleMask) != 0 || (p.track_solo_mask & ~kRoleMask) != 0) {
    return false;
  }
  if (p.key_root >= 12 || p.key_mode >= kModeCount) {
    return false;
  }
  if (p.chord_mode >= kChordModeCount) {
    return false;
  }
  if (p.chord_follow > static_cast<std::uint8_t>(ChordFollow::kLivePriority)) {
    return false;
  }
  for (const PerfRoute& route : p.routes) {
    if (route.port >= kMaxPorts || route.channel > 15) {
      return false;
    }
  }
  return true;
}

}  // namespace perf
}  // namespace arrangrr

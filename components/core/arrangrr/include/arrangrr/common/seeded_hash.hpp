#pragma once

#include <cstdint>

// The shared D16 deterministic position hash (docs/DESIGN.md 0100): "same
// seed => same output," with no PRNG *state* to carry -- the seed and the
// position ARE the whole input, so two independently-constructed callers with
// the same (seed, pos) always agree.
//
// This exact 2-argument formula existed, byte-for-byte, as a private static
// copy in THREE places before this extraction: `ArpeggiatorEngine::hash`
// (arp/arpeggiator.hpp), `Timeline::hash` (timeline/timeline.hpp), and (in
// spirit; groove's own version folds in two extra position components --
// tick/role/step -- with a different mixing sequence, so it is a related but
// genuinely distinct 4-argument variant, deliberately left as-is here to
// avoid perturbing its golden-pinned output) `groove::hash`
// (arranger/groove.hpp). The motif engine (roadmap 9210) is the fourth
// consumer of the plain 2-argument form -- reusing this one copy instead of
// hand-rolling a fourth, per docs/design/motif-engine-scope.md §0.2 /
// motif-engine-placement.md §3's shared flag.

namespace arrangrr {

constexpr std::uint32_t seeded_hash(std::uint32_t seed, std::uint32_t pos) noexcept {
  std::uint32_t h = seed * 2654435761u + pos + 0x9E3779B9u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return h;
}

}  // namespace arrangrr

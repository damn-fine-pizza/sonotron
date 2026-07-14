#include "arrangrr/perf/performance.hpp"

#include "arrangrr/arranger/style.hpp"      // styles::kBuiltinCount, SectionType/kSectionTypeCount
#include "arrangrr/chord/chord_engine.hpp"  // ChordMode/kChordModeCount, and (transitively via
                                            // chorddet/theory.hpp + chorddet/followed_context.hpp)
                                            // Mode/kModeCount and ChordFollow
#include "arrangrr/fx/insert_chain.hpp"     // kInsertTypeCount (Phase-6 Theme 3 Item #3)

// perf::validate's definition lives here, not in performance.hpp: it is the
// ONE piece of this header's surface that needs the heavier arranger/style +
// chord vocabulary (see performance.hpp's own comment on the declaration).
// Free function, no Engine state, freestanding/dual-target -- Engine::
// validate_performance (engine.cpp) is a thin forwarder that supplies the
// one live value this needs (ChordSequencer::count()).

namespace arrangrr {
namespace perf {

// A long but FLAT sequence of independent field-bound checks, each an
// early-return guard clause -- splitting it would scatter one cohesive
// validation pass across several functions for no readability gain (same
// rationale as Arranger::on_tick's own NOLINT, arranger.hpp).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
  // Phase 7 (node T0): beats_per_bar is a real runtime-variable numerator
  // (F1) -- reject a corrupt/out-of-range record the same way every other
  // field here does, matching Transport::set_time_sig's own bound.
  if (p.beats_per_bar < kMinBeatsPerBar || p.beats_per_bar > kMaxBeatsPerBar) {
    return false;
  }
  // Phase-6 Theme 3 Item #1/#3: master_transpose is a real std::int16_t as of
  // format_version 2 (P3) -- reject a corrupt/out-of-range record the same
  // way every other field here does, matching the live kMasterTranspose
  // command's own [-12, +12] bound.
  if (p.master_transpose < -12 || p.master_transpose > 12) {
    return false;
  }
  // Phase-6 Theme 3 Item #4: pad_bank_id is a plain index into the fixed
  // 8-bank layout (arrangrr/config.hpp's kMaxPadBanks) -- reject a corrupt/
  // out-of-range record the same way every other field here does.
  if (p.pad_bank_id >= kMaxPadBanks) {
    return false;
  }
  for (const PerfRoute& route : p.routes) {
    if (route.port >= kMaxPorts || route.channel > 15) {
      return false;
    }
  }
  // Phase-6 Theme 3 Item #3 (P2): routing_profile_id is RESERVED -- no
  // RoutingProfileStore exists yet, so the ONLY value accepted today is the
  // sentinel (exactly controller_map_id's own current, unbacked treatment).
  if (p.routing_profile_id != 0xFFFF) {
    return false;
  }
  // Phase-6 Theme 3 Item #3 (P1): every FX-chain slot's `type` must be a real
  // InsertType -- a corrupt/adversarial on-disk value would otherwise be
  // static_cast into Insert::type and dispatched against the WRONG active
  // union member inside Insert::process (Engine::apply_performance ->
  // Arranger::restore_fx -> InsertChain::restore, no further clamp there).
  // `enabled` rides a full byte (not a bitfield), so no range check needed --
  // `!= 0` is the only interpretation, same as every other route/PerfRoute
  // enabled byte above.
  for (const auto& role_chain : p.insert_chains) {
    for (const PerfInsert& ins : role_chain) {
      if (ins.type >= kInsertTypeCount) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace perf
}  // namespace arrangrr

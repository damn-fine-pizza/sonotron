#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui_style.hpp"

// Host-only renderer for the `arp` panel: the live arpeggiator's state and
// parameters (on/off, rate, direction, octaves, gate, latch). Pure: no terminal
// access; styling via UiStyle roles.
//
// Seam D (docs/design/orchestrator-pipeline-extraction.md §17.2): decoupled
// from the core `arrangrr::ArpeggiatorParams`/`ArpRate`/`ArpDirection` --
// `ArpViewParams` below redeclares the same field shape, arrangrr-free, with
// `rate`/`direction` riding the core enums' own raw numeric values (stable,
// additive-only ABI vocabulary, `arp/arpeggiator.hpp`). Today (Phase 3a) the
// caller (`Shell`) fills it straight off the live in-process
// `ArpeggiatorEngine::params()`; a future pure client (Phase 3b/3c) fills the
// SAME shape from parsed `kParamState` events.

namespace arrangrr::host {

// Panel rows, top to bottom.
enum class ArpRow : std::size_t {
  kEnabled = 0,
  kRate = 1,
  kDirection = 2,
  kOctaves = 3,
  kGate = 4,
  kLatch = 5,
};
inline constexpr std::size_t kArpRowCount = 6;

struct ArpViewParams {
  std::uint8_t rate = 2;       // core ArpRate's raw value (0=1/4,1=1/8,2=1/16,3=1/32)
  std::uint8_t direction = 0;  // core ArpDirection's raw value
  std::uint8_t octaves = 1;    // 1..4
  std::uint8_t gate = 75;      // 0..100 %
  bool latch = false;
};

std::vector<std::string> render_arp_panel(const ArpViewParams& params, bool enabled,
                                          std::uint8_t held, int selected, int cols,
                                          const UiStyle& style);

// Display names for the core ArpRate/ArpDirection raw values above.
const char* arp_rate_name(std::uint8_t rate);
const char* arp_direction_name(std::uint8_t direction);

}  // namespace arrangrr::host

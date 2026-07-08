#pragma once

#include <string>
#include <vector>

#include "arrangrr/arp/arpeggiator.hpp"
#include "ui_style.hpp"

// Host-only renderer for the `arp` panel: the live arpeggiator's state and
// parameters (on/off, rate, direction, octaves, gate, latch). Pure: no terminal
// access; styling via UiStyle roles.

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

std::vector<std::string> render_arp_panel(const ArpeggiatorParams& params, bool enabled,
                                          std::uint8_t held, int selected, int cols,
                                          const UiStyle& style);

const char* arp_rate_name(ArpRate rate);
const char* arp_direction_name(ArpDirection dir);

}  // namespace arrangrr::host

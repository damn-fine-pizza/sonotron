#include "arp_view.hpp"

#include <array>
#include <cstdio>

namespace arrangrr::host {

const char* arp_rate_name(ArpRate rate) {
  switch (rate) {
    case ArpRate::kQuarter:
      return "1/4";
    case ArpRate::kEighth:
      return "1/8";
    case ArpRate::kSixteenth:
      return "1/16";
    case ArpRate::kThirtySecond:
      return "1/32";
  }
  return "1/16";
}

const char* arp_direction_name(ArpDirection dir) {
  switch (dir) {
    case ArpDirection::kUp:
      return "up";
    case ArpDirection::kDown:
      return "down";
    case ArpDirection::kUpDown:
      return "up-down";
    case ArpDirection::kDownUp:
      return "down-up";
    case ArpDirection::kAsPlayed:
      return "as-played";
    case ArpDirection::kRandom:
      return "random";
  }
  return "up";
}

namespace {

std::string row_value(ArpRow row, const ArpeggiatorParams& p, bool enabled) {
  char buf[32];
  switch (row) {
    case ArpRow::kEnabled:
      return enabled ? "on" : "off";
    case ArpRow::kRate:
      return arp_rate_name(p.rate);
    case ArpRow::kDirection:
      return arp_direction_name(p.direction);
    case ArpRow::kOctaves:
      std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(p.octaves));
      return buf;
    case ArpRow::kGate:
      std::snprintf(buf, sizeof(buf), "%u%%", static_cast<unsigned>(p.gate));
      return buf;
    case ArpRow::kLatch:
      return p.latch ? "on" : "off";
  }
  return "";
}

constexpr std::array<const char*, kArpRowCount> kLabels = {"enabled", "rate",  "direction",
                                                           "octaves", "gate",  "latch"};

}  // namespace

std::vector<std::string> render_arp_panel(const ArpeggiatorParams& params, bool enabled,
                                          std::uint8_t held, int selected, int cols,
                                          const UiStyle& style) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < kArpRowCount; ++i) {
    const char cursor = (static_cast<int>(i) == selected) ? '>' : ' ';
    const std::string value = row_value(static_cast<ArpRow>(i), params, enabled);
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%c %-10s %s", cursor, kLabels[i], value.c_str());
    std::string line(buf);
    if (static_cast<int>(i) == selected) {
      line = style.apply(UiRole::kSuccess, line);
    }
    out.push_back(ansi::visible_truncate(line, static_cast<std::size_t>(cols > 0 ? cols : 0)));
  }
  char held_line[64];
  std::snprintf(held_line, sizeof(held_line), "held: %u notes   (hold keys, transport running)",
                static_cast<unsigned>(held));
  out.push_back(held_line);
  out.push_back("up/down param | left/right adjust | TAB exit");
  return out;
}

}  // namespace arrangrr::host

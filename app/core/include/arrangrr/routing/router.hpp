#pragma once

#include <cstdint>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/midi/message.hpp"

// Routing matrix in→out per port/channel with message-class filters and
// channel remap (soft-thru is just a route; §9.E).

namespace arrangrr {

// Filter bits: which message classes a route passes.
namespace route_pass {
inline constexpr std::uint8_t kNotes = 1u << 0;
inline constexpr std::uint8_t kCc = 1u << 1;
inline constexpr std::uint8_t kProgram = 1u << 2;  // PC + pressure fold here for M0
inline constexpr std::uint8_t kPitchBend = 1u << 3;
inline constexpr std::uint8_t kRealtime = 1u << 4;
inline constexpr std::uint8_t kSystem = 1u << 5;
inline constexpr std::uint8_t kAll = 0xFF;
}  // namespace route_pass

constexpr std::uint8_t route_class_bit(const MidiMessage& msg) noexcept {
  if (midi::is_realtime(msg.status)) return route_pass::kRealtime;
  if (midi::is_system(msg.status)) return route_pass::kSystem;
  switch (msg.type()) {
    case midi::kNoteOff:
    case midi::kNoteOn:
      return route_pass::kNotes;
    case midi::kControlChange:
      return route_pass::kCc;
    case midi::kPitchBend:
      return route_pass::kPitchBend;
    default:
      return route_pass::kProgram;
  }
}

struct Route {
  std::uint8_t in_port = 0;
  std::int8_t in_channel = -1;  // -1 = any channel
  std::uint8_t out_port = 0;
  std::int8_t out_channel = -1;  // -1 = keep incoming channel
  std::uint8_t pass = route_pass::kAll;
};

class Router {
 public:
  [[nodiscard]] constexpr bool add(const Route& route) noexcept {
    return routes_.push_back(route);
  }
  constexpr void clear() noexcept { routes_.clear(); }
  constexpr std::size_t count() const noexcept { return routes_.size(); }
  constexpr Span<const Route> routes() const noexcept { return routes_.span(); }

  // Fans the message out to every matching route.
  // Sink signature: void(uint8_t out_port, const MidiMessage&).
  template <typename Sink>
  constexpr void route(std::uint8_t in_port, const MidiMessage& msg, Sink&& sink) const {
    const std::uint8_t bit = route_class_bit(msg);
    const bool has_channel = midi::is_channel_voice(msg.status);
    for (const Route& r : routes_) {
      if (r.in_port != in_port) continue;
      if ((r.pass & bit) == 0) continue;
      if (has_channel && r.in_channel >= 0 && msg.channel() != r.in_channel) continue;
      MidiMessage out = msg;
      if (has_channel && r.out_channel >= 0) {
        out.status = static_cast<std::uint8_t>((msg.status & 0xF0u) | (r.out_channel & 0x0Fu));
      }
      sink(r.out_port, out);
    }
  }

 private:
  StaticVector<Route, kMaxRoutes> routes_;
};

}  // namespace arrangrr

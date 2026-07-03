#pragma once

#include <cstdint>

#include "arrangrr/midi/message.hpp"
#include "arrangrr/common/time.hpp"

// Core binary ABI (D26): the core never parses JSON or strings. The host
// resolves L1 string paths to these POD commands; the core emits POD events.
// Both directions are trivially copyable and cross the boundary on ring
// buffers or direct calls.

namespace arrangrr {

enum class Op : std::uint8_t {
  kSet = 0,
  kDo = 1,
  kGet = 2,
};

// Flat M0 parameter/action ids (the full L1 catalog grows with milestones;
// ids are stable — never reuse a value).
enum class Param : std::uint16_t {
  kNone = 0,
  kTransportTempo = 1,     // set: a = bpm_x100
  kTransportStart = 2,     // do
  kTransportStop = 3,      // do
  kTransportContinue = 4,  // do
  kPanic = 5,              // do
  kRouteAdd = 6,           // do: a = in_port | (in_ch & 0xFF) << 8
                           //     b = out_port | (out_ch & 0xFF) << 8
                           //     c = pass mask (route_pass::*)
  kRouteClear = 7,         // do
  kClockOutMask = 8,       // set: a = bitmask of ports that receive F8/FA/FB/FC
};

struct Command {
  Op op = Op::kDo;
  Param param = Param::kNone;
  std::int32_t a = 0;
  std::int32_t b = 0;
  std::int32_t c = 0;
};
static_assert(sizeof(Command) <= 16);

enum class WarnCode : std::uint16_t {
  kNone = 0,
  kSchedulerFull = 1,
  kRouteTableFull = 2,
  kUnknownCommand = 3,
  kBadArgument = 4,
};

// Event from core to host.
struct OutEvent {
  enum class Kind : std::uint8_t {
    kMidi = 0,       // msg on port, at tick
    kTransport = 1,  // code = TransportState
    kWarn = 2,       // code = WarnCode
  };

  Kind kind = Kind::kMidi;
  std::uint8_t port = 0;
  MidiMessage msg{};
  Tick tick = 0;
  std::uint16_t code = 0;

  static constexpr OutEvent midi(std::uint8_t port, const MidiMessage& m, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kMidi;
    e.port = port;
    e.msg = m;
    e.tick = t;
    return e;
  }
  static constexpr OutEvent transport(std::uint16_t state, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kTransport;
    e.code = state;
    e.tick = t;
    return e;
  }
  static constexpr OutEvent warn(WarnCode code, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kWarn;
    e.code = static_cast<std::uint16_t>(code);
    e.tick = t;
    return e;
  }
};
static_assert(sizeof(OutEvent) <= 16);

}  // namespace arrangrr

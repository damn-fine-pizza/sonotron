#include "jsonl.hpp"

#include <cstdio>

#include "arrangrr/transport/transport.hpp"

namespace arrangrr::host {

namespace {

const char* warn_name(std::uint16_t code) {
  switch (static_cast<WarnCode>(code)) {
    case WarnCode::kSchedulerFull:
      return "scheduler_full";
    case WarnCode::kRouteTableFull:
      return "route_table_full";
    case WarnCode::kUnknownCommand:
      return "unknown_command";
    case WarnCode::kBadArgument:
      return "bad_argument";
    default:
      return "unknown";
  }
}

const char* transport_name(std::uint16_t state) {
  switch (static_cast<TransportState>(state)) {
    case TransportState::kPlaying:
      return "playing";
    case TransportState::kPaused:
      return "paused";
    default:
      return "stopped";
  }
}

const char* realtime_name(std::uint8_t status) {
  switch (status) {
    case midi::kClock:
      return "clock";
    case midi::kStart:
      return "start";
    case midi::kContinue:
      return "continue";
    case midi::kStop:
      return "stop";
    case midi::kActiveSensing:
      return "active_sensing";
    case midi::kSystemReset:
      return "reset";
    default:
      return "realtime";
  }
}

std::string format(const char* fmt, auto... args) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), fmt, args...);
  return std::string(buf);
}

}  // namespace

std::string to_jsonl(const OutEvent& ev) {
  switch (ev.kind) {
    case OutEvent::Kind::kMidi: {
      const MidiMessage& m = ev.msg;
      if (midi::is_realtime(m.status)) {
        return format(R"({"ev":"midi-out","port":%u,"msg":"%s","@":%u})", ev.port,
                      realtime_name(m.status), ev.tick);
      }
      const unsigned ch = m.channel() + 1;  // 1-based on the wire for humans
      switch (m.type()) {
        case midi::kNoteOn:
          return format(R"({"ev":"midi-out","port":%u,"msg":"noteon","ch":%u,"note":%u,"vel":%u,"@":%u})",
                        ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kNoteOff:
          return format(R"({"ev":"midi-out","port":%u,"msg":"noteoff","ch":%u,"note":%u,"vel":%u,"@":%u})",
                        ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kControlChange:
          return format(R"({"ev":"midi-out","port":%u,"msg":"cc","ch":%u,"cc":%u,"val":%u,"@":%u})",
                        ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kProgramChange:
          return format(R"({"ev":"midi-out","port":%u,"msg":"program","ch":%u,"num":%u,"@":%u})",
                        ev.port, ch, m.d1, ev.tick);
        case midi::kPitchBend:
          return format(R"({"ev":"midi-out","port":%u,"msg":"pitchbend","ch":%u,"value":%d,"@":%u})",
                        ev.port, ch, (int(m.d2) << 7 | m.d1) - 8192, ev.tick);
        default:
          return format(R"({"ev":"midi-out","port":%u,"msg":"raw","status":%u,"d1":%u,"d2":%u,"@":%u})",
                        ev.port, m.status, m.d1, m.d2, ev.tick);
      }
    }
    case OutEvent::Kind::kTransport:
      return format(R"({"ev":"transport","state":"%s","@":%u})", transport_name(ev.code), ev.tick);
    case OutEvent::Kind::kWarn:
    default:
      return format(R"({"ev":"warn","code":"%s","@":%u})", warn_name(ev.code), ev.tick);
  }
}

std::string to_human(const OutEvent& ev) {
  switch (ev.kind) {
    case OutEvent::Kind::kMidi: {
      const MidiMessage& m = ev.msg;
      if (midi::is_realtime(m.status))
        return format("@%-8u p%u %s", ev.tick, ev.port, realtime_name(m.status));
      switch (m.type()) {
        case midi::kNoteOn:
          return format("@%-8u p%u ch%-2u note-on  %3u vel %3u", ev.tick, ev.port,
                        m.channel() + 1, m.d1, m.d2);
        case midi::kNoteOff:
          return format("@%-8u p%u ch%-2u note-off %3u vel %3u", ev.tick, ev.port,
                        m.channel() + 1, m.d1, m.d2);
        case midi::kControlChange:
          return format("@%-8u p%u ch%-2u cc %3u = %3u", ev.tick, ev.port, m.channel() + 1, m.d1,
                        m.d2);
        default:
          return format("@%-8u p%u status %02X %u %u", ev.tick, ev.port, m.status, m.d1, m.d2);
      }
    }
    case OutEvent::Kind::kTransport:
      return format("@%-8u transport %s", ev.tick, transport_name(ev.code));
    case OutEvent::Kind::kWarn:
    default:
      return format("@%-8u WARN %s", ev.tick, warn_name(ev.code));
  }
}

}  // namespace arrangrr::host

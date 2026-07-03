#include "jsonl.hpp"

#include <cstdio>

#include "arrangrr/arranger/style.hpp"
#include "arrangrr/chord/chord_engine.hpp"
#include "arrangrr/chord/theory.hpp"
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

std::string format(const char* fmt, auto... args);

const char* section_name(std::uint16_t code) {
  static constexpr const char* kNames[] = {
      "intro1", "intro2", "varA", "varB", "varC", "varD", "fillA",
      "fillB",  "fillC",  "fillD", "break", "ending1", "ending2",
  };
  return code < kSectionTypeCount ? kNames[code] : "?";
}

const char* pc_name(std::uint8_t pc, bool flats) {
  static constexpr const char* kSharp[12] = {"C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B"};
  static constexpr const char* kFlat[12] = {"C", "Db", "D", "Eb", "E", "F",
                                            "Gb", "G", "Ab", "A", "Bb", "B"};
  return (flats ? kFlat : kSharp)[pc % 12];
}

std::string note_name(std::uint8_t note, bool flats) {
  return format("%s%d", pc_name(note % 12, flats), note / 12 - 1);
}

const char* quality_suffix(ChordQuality q) {
  switch (q) {
    case ChordQuality::kMaj: return "";
    case ChordQuality::kMin: return "m";
    case ChordQuality::kDim: return "dim";
    case ChordQuality::kAug: return "aug";
    case ChordQuality::kMaj7: return "maj7";
    case ChordQuality::kMin7: return "m7";
    case ChordQuality::kDom7: return "7";
    case ChordQuality::kHalfDim7: return "m7b5";
    case ChordQuality::kDim7: return "dim7";
    case ChordQuality::kSus2: return "sus2";
    case ChordQuality::kSus4: return "sus4";
  }
  return "";
}

std::string roman_degree(std::uint8_t degree, ChordQuality q) {
  static constexpr const char* kUpper[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
  static constexpr const char* kLower[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
  if (degree == kNoDegree) return "-";  // keyless modes (single/shell)
  if (degree > 6) return "?";
  const bool minor_family = q == ChordQuality::kMin || q == ChordQuality::kMin7 ||
                            q == ChordQuality::kDim || q == ChordQuality::kDim7 ||
                            q == ChordQuality::kHalfDim7;
  std::string out = (minor_family ? kLower : kUpper)[degree];
  if (q == ChordQuality::kHalfDim7) out += "m7b5";
  else if (q == ChordQuality::kDim || q == ChordQuality::kDim7) out += "dim";
  return out;
}

std::string format(const char* fmt, auto... args) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), fmt, args...);
  return std::string(buf);
}

}  // namespace

std::string to_jsonl(const OutEvent& ev, bool prefer_flats) {
  switch (ev.kind) {
    case OutEvent::Kind::kSection:
      return format(R"({"ev":"section","name":"%s","@":%u})", section_name(ev.code), ev.tick);
    case OutEvent::Kind::kChord: {
      const auto degree = static_cast<std::uint8_t>(ev.code & 0xFF);
      const auto quality = static_cast<ChordQuality>(ev.code >> 8);
      return format(R"({"ev":"chord","in":"%s","out":"%s%s","deg":"%s","@":%u})",
                    note_name(ev.msg.status, prefer_flats).c_str(),
                    pc_name(ev.msg.status % 12, prefer_flats), quality_suffix(quality),
                    roman_degree(degree, quality).c_str(), ev.tick);
    }
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

std::string to_human(const OutEvent& ev, bool prefer_flats) {
  switch (ev.kind) {
    case OutEvent::Kind::kSection:
      return format("@%-8u section %s", ev.tick, section_name(ev.code));
    case OutEvent::Kind::kChord: {
      const auto quality = static_cast<ChordQuality>(ev.code >> 8);
      return format("@%-8u chord %s%s (%s)", ev.tick,
                    pc_name(ev.msg.status % 12, prefer_flats), quality_suffix(quality),
                    roman_degree(static_cast<std::uint8_t>(ev.code & 0xFF), quality).c_str());
    }
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

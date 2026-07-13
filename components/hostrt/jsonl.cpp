#include "jsonl.hpp"

#include <cstdio>

#include "arrangrr/chord/chord_engine.hpp"
#include "event_labels.hpp"
#include "note_names.hpp"

namespace arrangrr::host {

namespace {

constexpr int kNoteNameColumnWidth = 10;

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
      return format(
          R"({"ev":"chord","in":"%s","out":"%s%s","deg":"%s","@":%u})",
          note_name(ev.msg.status, {NoteNaming::kCde, prefer_flats, true}).c_str(),
          pitch_class_name(ev.msg.status, {NoteNaming::kCde, prefer_flats, false}).c_str(),
          quality_suffix(quality), roman_degree(degree, quality).c_str(), ev.tick);
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
          return format(
              R"({"ev":"midi-out","port":%u,"msg":"noteon","ch":%u,"note":%u,"vel":%u,"@":%u})",
              ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kNoteOff:
          return format(
              R"({"ev":"midi-out","port":%u,"msg":"noteoff","ch":%u,"note":%u,"vel":%u,"@":%u})",
              ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kControlChange:
          return format(R"({"ev":"midi-out","port":%u,"msg":"cc","ch":%u,"cc":%u,"val":%u,"@":%u})",
                        ev.port, ch, m.d1, m.d2, ev.tick);
        case midi::kProgramChange:
          return format(R"({"ev":"midi-out","port":%u,"msg":"program","ch":%u,"num":%u,"@":%u})",
                        ev.port, ch, m.d1, ev.tick);
        case midi::kPitchBend:
          return format(
              R"({"ev":"midi-out","port":%u,"msg":"pitchbend","ch":%u,"value":%d,"@":%u})", ev.port,
              ch, (int(m.d2) << 7 | m.d1) - 8192, ev.tick);
        default:
          return format(
              R"({"ev":"midi-out","port":%u,"msg":"raw","status":%u,"d1":%u,"d2":%u,"@":%u})",
              ev.port, m.status, m.d1, m.d2, ev.tick);
      }
    }
    case OutEvent::Kind::kChordFollowed: {
      const ChordState cur = followed_state(ev.msg.status, (ev.msg.d2 & 0x1) != 0);
      const ChordState next = followed_state(ev.msg.d1, (ev.msg.d2 & 0x2) != 0);
      const auto src = static_cast<std::uint8_t>((ev.msg.d2 >> 2) & 0x3);
      return format(
          R"({"ev":"chord-followed","cur":"%s","cur_pcs":%u,"next":"%s","next_pcs":%u,"src":"%s","@":%u})",
          followed_label(cur, prefer_flats).c_str(), followed_pcs(cur),
          followed_label(next, prefer_flats).c_str(), followed_pcs(next), producer_name(src),
          ev.tick);
    }
    case OutEvent::Kind::kBeat:
      return format(R"({"ev":"beat","bar":%u,"beat":%u,"pulse":%u,"@":%u})", ev.code, ev.msg.status,
                    ev.msg.d1, ev.tick);
    case OutEvent::Kind::kTransport:
      return format(R"({"ev":"transport","state":"%s","@":%u})", transport_name(ev.code), ev.tick);
    case OutEvent::Kind::kParamState:
      // Deliberately PERMANENT, not a Phase-3a placeholder (docs/design/
      // orchestrator-pipeline-extraction.md §17.3b, Phase 3b): kParamState
      // stays invisible on THIS channel forever -- an empty string renders no
      // line at all, keeping the golden harness's byte-identical stdout
      // stream untouched no matter which cmd_* handlers start emitting the
      // echo. param_state_wire.hpp's param_state_to_jsonl() is the real wire
      // encoding, wired ONLY into a control-plane broadcast, never stdout.
      return {};
    case OutEvent::Kind::kClip:
      // Phase-5 Item #2: which cell is armed/playing/stopped -- id rides
      // `code`, the LaunchState rides msg.status.
      return format(R"({"ev":"clip","id":%u,"state":"%s","@":%u})", ev.code,
                    clip_state_name(ev.msg.status), ev.tick);
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
      return format(
          "@%-8u chord %s%s (%s)", ev.tick,
          pitch_class_name(ev.msg.status, {NoteNaming::kCde, prefer_flats, false}).c_str(),
          quality_suffix(quality),
          roman_degree(static_cast<std::uint8_t>(ev.code & 0xFF), quality).c_str());
    }
    case OutEvent::Kind::kMidi: {
      const MidiMessage& m = ev.msg;
      if (midi::is_realtime(m.status)) {
        return format("@%-8u p%u %s", ev.tick, ev.port, realtime_name(m.status));
      }
      switch (m.type()) {
        case midi::kNoteOn:
          return format("@%-8u p%u ch%-2u note-on  %3u %-*s vel %3u", ev.tick, ev.port,
                        m.channel() + 1, m.d1, kNoteNameColumnWidth,
                        note_label(m.channel(), m.d1, prefer_flats).c_str(), m.d2);
        case midi::kNoteOff:
          return format("@%-8u p%u ch%-2u note-off %3u %-*s vel %3u", ev.tick, ev.port,
                        m.channel() + 1, m.d1, kNoteNameColumnWidth,
                        note_label(m.channel(), m.d1, prefer_flats).c_str(), m.d2);
        case midi::kControlChange:
          return format("@%-8u p%u ch%-2u cc %3u = %3u", ev.tick, ev.port, m.channel() + 1, m.d1,
                        m.d2);
        default:
          return format("@%-8u p%u status %02X %u %u", ev.tick, ev.port, m.status, m.d1, m.d2);
      }
    }
    case OutEvent::Kind::kChordFollowed: {
      const ChordState cur = followed_state(ev.msg.status, (ev.msg.d2 & 0x1) != 0);
      const ChordState next = followed_state(ev.msg.d1, (ev.msg.d2 & 0x2) != 0);
      const auto src = static_cast<std::uint8_t>((ev.msg.d2 >> 2) & 0x3);
      return format("@%-8u follow %s -> %s (%s)", ev.tick,
                    followed_label(cur, prefer_flats).c_str(),
                    followed_label(next, prefer_flats).c_str(), producer_name(src));
    }
    case OutEvent::Kind::kBeat:
      return format("@%-8u beat %u.%u.%u", ev.tick, ev.code, ev.msg.status, ev.msg.d1);
    case OutEvent::Kind::kTransport:
      return format("@%-8u transport %s", ev.tick, transport_name(ev.code));
    case OutEvent::Kind::kParamState:
      // Permanent (see to_jsonl's kParamState case above): no text shape on
      // this channel, ever.
      return {};
    case OutEvent::Kind::kClip:
      return format("@%-8u clip %u %s", ev.tick, ev.code, clip_state_name(ev.msg.status));
    case OutEvent::Kind::kWarn:
    default:
      return format("@%-8u WARN %s", ev.tick, warn_name(ev.code));
  }
}

}  // namespace arrangrr::host

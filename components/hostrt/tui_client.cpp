#include "tui_client.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "client_event.hpp"
#include "param_state_wire.hpp"

namespace arrangrr::host {

namespace {

// Groove panel row -> the exact field token cmd_groove's L1 grammar accepts
// (shell_music_commands.cpp's cmd_groove), in the SAME row order as the live
// TUI's own kGrooveRowField (shell_input.cpp) / GrooveViewParams field order
// (groove_view.hpp). Row 5 (seed) has no panel row, mirroring the live TUI.
constexpr std::array<const char*, TuiClient::kGrooveRowCount> kGrooveRowName = {
    "swing", "humanize-t", "humanize-v", "accent", "grid", "quantize",
};

// Arp panel row -> ArpRow order (shell_input.cpp), used both for clamping
// and for building the right L1 verb shape (cmd_arp's grammar is NOT
// uniform: "on|off", "rate <v>", "dir <v>", "octaves <v>", "gate <v>",
// "latch <v>").
enum class ArpRowKind {
  kEnabled = 0,
  kRate = 1,
  kDirection = 2,
  kOctaves = 3,
  kGate = 4,
  kLatch = 5
};

constexpr std::array<const char*, 4> kArpRateName = {"1/4", "1/8", "1/16", "1/32"};
constexpr std::array<const char*, 6> kArpDirectionName = {"up",     "down",      "updown",
                                                          "downup", "as-played", "random"};

// Mixer role index (0..7) -> the exact role token `part <role> ...` accepts
// (shell_parse.cpp's parse_role), the SAME Drums..Phrase order as
// shell_internal.hpp's kMixerParts / param_state_mirror.hpp's kPartRoleCount.
constexpr std::array<const char*, kPartRoleCount> kMixerRoleName = {
    "drums", "perc", "bass", "chord1", "chord2", "pad", "arp", "phrase",
};

// Builds the "groove.<field>=<value>" / "arp.<field>=<value>" / ...
// observability string last_param_change() reports.
std::string describe(const char* domain, const char* field, long value) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s.%s=%ld", domain, field, value);
  return std::string(buf);
}

// Groove's "sub" is a plain field index into kGrooveRowName; its value rides
// v0|v1<<8 (the ONLY 16-bit-valued field the mirror decodes, e.g. a wide
// style index -- groove/arp values themselves never need it, but the same
// packing rule applies uniformly).
std::string describe_groove(const ParamStateWire& w) {
  if (w.sub >= kGrooveRowCount) {
    return {};
  }
  return describe("groove", kGrooveRowName[w.sub],
                  static_cast<long>(w.v0) | (static_cast<long>(w.v1) << 8));
}

// Arp's grammar is NOT uniform per field (on/off vs named-value vs plain
// number), so each sub gets its own branch -- kept out of on_wire_line's own
// switch to stay under its cognitive-complexity budget.
std::string describe_arp(const ParamStateWire& w) {
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kEnabled)) {
    return describe("arp", "enabled", w.v0);
  }
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kRate) && w.v0 < kArpRateName.size()) {
    return std::string("arp.rate=") + kArpRateName[w.v0];
  }
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kDirection) &&
      w.v0 < kArpDirectionName.size()) {
    return std::string("arp.direction=") + kArpDirectionName[w.v0];
  }
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kOctaves)) {
    return describe("arp", "octaves", w.v0);
  }
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kGate)) {
    return describe("arp", "gate", w.v0);
  }
  if (w.sub == static_cast<std::uint8_t>(ArpRowKind::kLatch)) {
    return describe("arp", "latch", w.v0);
  }
  return {};
}

std::string describe_part(const char* domain, const ParamStateWire& w) {
  if (w.sub >= kMixerRoleName.size()) {
    return {};
  }
  return std::string(domain) + "." + kMixerRoleName[w.sub] + "=" + (w.v0 != 0 ? "on" : "off");
}

// Describes one decoded kParamState echo using the SAME field-name tables
// the on-connect state dump and every mutating cmd_* already share
// (param_state_wire.cpp's own param_state_name table) -- reusing whichever
// tag survived decoding. Returns empty for an unwired Param (additive-only
// ABI growth: a future field an older client build does not know yet is
// ignored, not fatal).
std::string describe_param_change(const ParamStateWire& w) {
  switch (w.param) {
    case Param::kGroove:
      return describe_groove(w);
    case Param::kArp:
      return describe_arp(w);
    case Param::kPartMute:
      return describe_part("part-mute", w);
    case Param::kPartSolo:
      return describe_part("part-solo", w);
    case Param::kStyleLoad:
      return describe("style", "load", static_cast<long>(w.v0) | (static_cast<long>(w.v1) << 8));
    case Param::kChordDetect:
      return describe("chord", "detect", w.v0);
    case Param::kChordFollow:
      return describe("chord", "follow", w.v0);
    case Param::kChordMode:
      return describe("chord", "mode", w.v0);
    case Param::kKeySet:
      return describe("key", "root_pc", w.v0);
    default:
      return {};
  }
}

}  // namespace

void TuiClient::on_wire_line(const std::string& line) {
  ParamStateWire w;
  if (parse_param_state_jsonl(line, w)) {
    m_mirror.apply(w);
    m_last_param_change = describe_param_change(w);
    return;
  }

  const ClientEvent ev = parse_client_event(line);
  if (ev.kind == ClientEvent::Kind::kMidiOut) {
    // Seam D (§17.2): the monitor's own core-free mirror -- no core OutEvent
    // ever reaches this file. The wire's "msg" field is a rendered label
    // (jsonl.cpp's own to_jsonl formatting), not the raw MidiMessage bytes,
    // so only port/tick are folded here; the monitor's log-line rendering
    // is future work, not required by this increment's gesture-round-trip
    // scope.
    (void)ev;  // observed, not yet folded into m_monitor (see file header)
  }
}

void TuiClient::send_note(const std::string& port_name, int channel_1based, std::uint8_t midi_note,
                          std::uint8_t velocity, bool on) {
  std::string line = "note " + port_name;
  if (channel_1based > 0) {
    line += ":" + std::to_string(channel_1based);
  }
  line += on ? " on " : " off ";
  line += std::to_string(midi_note);
  line += " ";
  line += std::to_string(velocity);
  m_send(line);
}

void TuiClient::groove_adjust(int row, int delta) {
  if (row < 0 || row >= kGrooveRowCount) {
    return;
  }
  const GrooveViewParams& p = m_mirror.groove;
  int next = 0;
  switch (row) {
    case 0:
      next = std::clamp(static_cast<int>(p.swing) + delta * 10, 0, 100);
      break;
    case 1:
      next = std::clamp(static_cast<int>(p.humanize_timing) + delta * 10, 0, 100);
      break;
    case 2:
      next = std::clamp(static_cast<int>(p.humanize_velocity) + delta * 10, 0, 100);
      break;
    case 3:
      next = std::clamp(static_cast<int>(p.accent) + delta * 10, 0, 100);
      break;
    case 4:
      next = (p.swing_grid == 16) ? 8 : 16;  // left/right both toggle
      break;
    case 5:
      next = std::clamp(static_cast<int>(p.quantize) + delta * 10, 0, 100);
      break;
    default:
      return;
  }
  m_send(std::string("groove ") + kGrooveRowName[static_cast<std::size_t>(row)] + " " +
         std::to_string(next));
}

void TuiClient::arp_adjust(int row, int delta) {
  if (row < 0 || row >= kArpRowCount) {
    return;
  }
  const ArpViewParams& p = m_mirror.arp;
  switch (static_cast<ArpRowKind>(row)) {
    case ArpRowKind::kEnabled:
      m_send(m_mirror.arp_enabled ? "arp off" : "arp on");
      return;
    case ArpRowKind::kRate: {
      const int next = std::clamp(static_cast<int>(p.rate) + delta, 0,
                                  static_cast<int>(kArpRateName.size()) - 1);
      m_send(std::string("arp rate ") + kArpRateName[static_cast<std::size_t>(next)]);
      return;
    }
    case ArpRowKind::kDirection: {
      const int next = std::clamp(static_cast<int>(p.direction) + delta, 0,
                                  static_cast<int>(kArpDirectionName.size()) - 1);
      m_send(std::string("arp dir ") + kArpDirectionName[static_cast<std::size_t>(next)]);
      return;
    }
    case ArpRowKind::kOctaves: {
      const int next = std::clamp(static_cast<int>(p.octaves) + delta, 1, 4);
      m_send("arp octaves " + std::to_string(next));
      return;
    }
    case ArpRowKind::kGate: {
      const int next = std::clamp(static_cast<int>(p.gate) + delta * 10, 0, 100);
      m_send("arp gate " + std::to_string(next));
      return;
    }
    case ArpRowKind::kLatch:
      m_send(p.latch ? "arp latch off" : "arp latch on");
      return;
  }
}

void TuiClient::part_toggle_mute(int role_index) {
  if (role_index < 0 || static_cast<std::size_t>(role_index) >= kMixerRoleName.size()) {
    return;
  }
  const bool muted = m_mirror.part_muted[static_cast<std::size_t>(role_index)];
  m_send(std::string("part ") + kMixerRoleName[static_cast<std::size_t>(role_index)] + " mute " +
         (muted ? "off" : "on"));
}

void TuiClient::part_toggle_solo(int role_index) {
  if (role_index < 0 || static_cast<std::size_t>(role_index) >= kMixerRoleName.size()) {
    return;
  }
  const bool soloed = m_mirror.part_soloed[static_cast<std::size_t>(role_index)];
  m_send(std::string("part ") + kMixerRoleName[static_cast<std::size_t>(role_index)] + " solo " +
         (soloed ? "off" : "on"));
}

}  // namespace arrangrr::host

#include "param_state_wire.hpp"

#include <cstdio>
#include <string_view>

namespace arrangrr::host {

namespace {

// The 9 Param domains this milestone's kParamState echo actually carries
// (abi.hpp's Kind::kParamState comment table) -- additive-only: a future
// Param growing its own state echo appends a new case here, never renumbers
// or reuses one of these strings.
const char* param_state_name(Param p) {
  switch (p) {
    case Param::kGroove:
      return "groove";
    case Param::kArp:
      return "arp";
    case Param::kPartMute:
      return "part-mute";
    case Param::kPartSolo:
      return "part-solo";
    case Param::kStyleLoad:
      return "style-load";
    case Param::kChordDetect:
      return "chord-detect";
    case Param::kChordFollow:
      return "chord-follow";
    case Param::kChordMode:
      return "chord-mode";
    case Param::kKeySet:
      return "key-set";
    default:
      return nullptr;
  }
}

bool param_state_from_name(const char* name, Param& out) {
  const std::string_view n(name);
  if (n == "groove") {
    out = Param::kGroove;
  } else if (n == "arp") {
    out = Param::kArp;
  } else if (n == "part-mute") {
    out = Param::kPartMute;
  } else if (n == "part-solo") {
    out = Param::kPartSolo;
  } else if (n == "style-load") {
    out = Param::kStyleLoad;
  } else if (n == "chord-detect") {
    out = Param::kChordDetect;
  } else if (n == "chord-follow") {
    out = Param::kChordFollow;
  } else if (n == "chord-mode") {
    out = Param::kChordMode;
  } else if (n == "key-set") {
    out = Param::kKeySet;
  } else {
    return false;
  }
  return true;
}

}  // namespace

std::string param_state_to_jsonl(const OutEvent& ev) {
  if (ev.kind != OutEvent::Kind::kParamState) {
    return {};
  }
  const char* name = param_state_name(static_cast<Param>(ev.code));
  if (name == nullptr) {
    return {};  // an unwired Param would be a producer bug -- render nothing, don't lie
  }
  char buf[160];
  std::snprintf(buf, sizeof(buf), R"({"ev":"param","param":"%s","sub":%u,"v0":%u,"v1":%u,"@":%u})",
                name, ev.port, ev.msg.status, ev.msg.d1, ev.tick);
  return std::string(buf);
}

bool parse_param_state_jsonl(const std::string& line, ParamStateWire& out) {
  char name_buf[32] = {};
  unsigned sub = 0;
  unsigned v0 = 0;
  unsigned v1 = 0;
  unsigned tick = 0;
  const int matched = std::sscanf(
      line.c_str(), R"({"ev":"param","param":"%31[^"]","sub":%u,"v0":%u,"v1":%u,"@":%u})",
      static_cast<char*>(name_buf), &sub, &v0, &v1, &tick);
  if (matched != 5) {
    return false;
  }
  Param param{};
  if (!param_state_from_name(name_buf, param)) {
    return false;
  }
  out.param = param;
  out.sub = static_cast<std::uint8_t>(sub);
  out.v0 = static_cast<std::uint8_t>(v0);
  out.v1 = static_cast<std::uint8_t>(v1);
  out.tick = static_cast<Tick>(tick);
  return true;
}

}  // namespace arrangrr::host

#include <vector>

#include "shell.hpp"
#include "shell_internal.hpp"

// Phase-5 Item #10 (docs/phase5-design-reviews.md "MIDI-FX insert chain",
// node 5100/5200): the L1 grammar for the per-role insert chain, mirroring
// shell_pad_commands.cpp's own `pad assign|trigger|release` convention.
//   fx set <role> <slot> <type>
//   fx param <role> <slot> <param_id> <value>
//   fx enable <role> <slot> <0|1>
//   fx clear <role> [<slot>]   (no slot = the whole chain)

namespace arrangrr::host {

using namespace shell_detail;

namespace {

bool parse_insert_type(const std::string& s, InsertType& out) {
  if (s == "scalelock") {
    out = InsertType::kScaleLock;
  } else if (s == "velocity") {
    out = InsertType::kVelocityProc;
  } else if (s == "echo") {
    out = InsertType::kEcho;
  } else if (s == "noterepeat") {
    out = InsertType::kNoteRepeat;
  } else {
    return false;
  }
  return true;
}

bool parse_fx_slot(const std::string& s, std::uint64_t& out, std::string& error) {
  if (!parse_u64(s, out) || out >= kMaxInserts) {
    error = "bad fx slot: " + s;
    return false;
  }
  return true;
}

}  // namespace

// `fx set <role> <slot> <type>` | `fx param <role> <slot> <param_id> <value>` |
// `fx enable <role> <slot> <0|1>` | `fx clear <role> [<slot>]`.
bool Shell::cmd_fx(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "usage: fx set <role> <slot> <type> | fx param <role> <slot> <param_id> <value> | "
      "fx enable <role> <slot> <0|1> | fx clear <role> [<slot>]";
  if (t.size() < 3) {
    error = kUsage;
    return false;
  }
  TrackRole role{};
  if (!parse_role(t[2], role)) {
    error = "unknown role: " + t[2];
    return false;
  }

  if (t[1] == "set") {
    if (t.size() < 5) {
      error = kUsage;
      return false;
    }
    std::uint64_t slot = 0;
    if (!parse_fx_slot(t[3], slot, error)) {
      return false;
    }
    InsertType type{};
    if (!parse_insert_type(t[4], type)) {
      error = "unknown fx type: " + t[4];
      return false;
    }
    Command c;
    c.param = Param::kFxSet;
    c.idx = static_cast<std::uint16_t>(role);
    c.a = static_cast<std::int32_t>(slot);
    c.b = static_cast<std::int32_t>(type);
    m_engine.push_command(c, m_sink);
    return true;
  }

  if (t[1] == "param") {
    if (t.size() < 6) {
      error = kUsage;
      return false;
    }
    std::uint64_t slot = 0;
    if (!parse_fx_slot(t[3], slot, error)) {
      return false;
    }
    std::uint64_t param_id = 0;
    if (!parse_u64(t[4], param_id) || param_id > 255) {
      error = "bad fx param id: " + t[4];
      return false;
    }
    int value = 0;
    if (!parse_int(t[5], value)) {
      error = "bad fx param value: " + t[5];
      return false;
    }
    Command c;
    c.param = Param::kFxParam;
    c.idx = static_cast<std::uint16_t>(role);
    c.a = static_cast<std::int32_t>(slot);
    c.b = static_cast<std::int32_t>(param_id);
    c.c = value;
    m_engine.push_command(c, m_sink);
    return true;
  }

  if (t[1] == "enable") {
    if (t.size() < 5) {
      error = kUsage;
      return false;
    }
    std::uint64_t slot = 0;
    if (!parse_fx_slot(t[3], slot, error)) {
      return false;
    }
    std::uint64_t on = 0;
    if (!parse_u64(t[4], on)) {
      error = "bad fx enable value: " + t[4];
      return false;
    }
    Command c;
    c.param = Param::kFxEnable;
    c.idx = static_cast<std::uint16_t>(role);
    c.a = static_cast<std::int32_t>(slot);
    c.b = on != 0 ? 1 : 0;
    m_engine.push_command(c, m_sink);
    return true;
  }

  if (t[1] == "clear") {
    Command c;
    c.param = Param::kFxClear;
    c.idx = static_cast<std::uint16_t>(role);
    c.a = -1;  // default: the whole chain
    if (t.size() >= 4) {
      std::uint64_t slot = 0;
      if (!parse_fx_slot(t[3], slot, error)) {
        return false;
      }
      c.a = static_cast<std::int32_t>(slot);
    }
    m_engine.push_command(c, m_sink);
    return true;
  }

  error = kUsage;
  return false;
}

}  // namespace arrangrr::host

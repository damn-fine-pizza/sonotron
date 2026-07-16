#include "shell.hpp"
#include "shell_internal.hpp"

// Phase 7 (node 6000, the Looper) -- the Looper's first-ever L1 verb family
// (docs/proposals/looper-in-gui-contract.md §3/§7 item 4): `loop new`,
// `loop record <slot> record|overdub|replace <port>`, `loop stop <slot>
// [grid]`, `loop erase <slot>`, `loop undo <slot>`, `loop length <slot>
// auto|fixed <ticks>|quantized <grid>`, mirroring shell_clip_commands.cpp's
// own shape closely. Every verb rides an ALREADY-frozen Param (kLoopNew/
// kLoopRecordStart/kLoopRecordStop/kLoopErase/kLoopUndo/kLoopLength,
// abi.hpp) -- zero new ABI here (item 5's kLoopNew explicit-id argument is a
// separate, core-side change; `loop new` below always sends the legacy
// sentinel, matching `clip add`'s own absent-id default -- the `clip add
// ... loop <slot> id <n>` grammar that would exercise the explicit-id path
// from the host side is item 6, a later slice). MVP verb set only (owner
// sign-off §9b): record/stop/erase/undo/length + new -- overdub-mode UI
// selection and retro-capture arm/grab stay out of this pass (v2).

namespace arrangrr::host {

using namespace shell_detail;

namespace {

bool parse_loop_slot(const std::string& s, std::uint16_t& out, std::string& error) {
  std::uint64_t value = 0;
  if (!parse_u64(s, value) || value > 0xFFFF) {
    error = "bad loop slot: " + s;
    return false;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

bool parse_record_mode(const std::string& s, LoopRecordMode& out) {
  if (s == "record") {
    out = LoopRecordMode::kRecord;
  } else if (s == "overdub") {
    out = LoopRecordMode::kOverdub;
  } else if (s == "replace") {
    out = LoopRecordMode::kReplace;
  } else {
    return false;
  }
  return true;
}

}  // namespace

bool Shell::cmd_loop(const std::vector<std::string>& t, std::string& error) {
  const std::string& verb = t[1];

  // `loop new` -- registers a new empty loop slot (mirrors kSeqNew/kClipAdd's
  // own host/script-only registration convention). Always the legacy
  // sequential-append form here (Command::idx = kNoLoopExplicitId): the
  // GUI-facing explicit-id path (item 6's `clip add ... loop <slot> id <n>`)
  // is a later slice.
  if (verb == "new") {
    Command c;
    c.param = Param::kLoopNew;
    c.idx = kNoLoopExplicitId;
    m_engine.push_command(c, m_sink);
    return true;
  }

  // `loop record <slot> record|overdub|replace <port>` -- starts capturing
  // live notes arriving on `port` into `slot` (LoopBuffer::start_record).
  if (verb == "record") {
    if (t.size() < 5) {
      error = "usage: loop record <slot> record|overdub|replace <port>";
      return false;
    }
    std::uint16_t slot = 0;
    if (!parse_loop_slot(t[2], slot, error)) {
      return false;
    }
    LoopRecordMode mode{};
    if (!parse_record_mode(t[3], mode)) {
      error = "loop record mode: record|overdub|replace";
      return false;
    }
    const int port = find_port(t[4], /*input=*/true);
    if (port < 0) {
      error = "unknown input port: " + t[4];
      return false;
    }
    Command c;
    c.param = Param::kLoopRecordStart;
    c.idx = slot;
    c.a = static_cast<std::int32_t>(mode);
    c.b = port;
    m_engine.push_command(c, m_sink);
    return true;
  }

  // `loop stop <slot> [grid]` -- closes the active recording (quantize-after,
  // grid in ticks; 0/absent = the engine's own one-bar default).
  if (verb == "stop") {
    if (t.size() < 3) {
      error = "usage: loop stop <slot> [grid]";
      return false;
    }
    std::uint16_t slot = 0;
    if (!parse_loop_slot(t[2], slot, error)) {
      return false;
    }
    std::uint64_t grid = 0;
    if (t.size() >= 4 && !parse_u64(t[3], grid)) {
      error = "bad grid: " + t[3];
      return false;
    }
    Command c;
    c.param = Param::kLoopRecordStop;
    c.idx = slot;
    c.a = static_cast<std::int32_t>(grid);
    m_engine.push_command(c, m_sink);
    return true;
  }

  // `loop erase <slot>` -- clears the slot's content (shadow-saved first for
  // undo, LoopBuffer::erase).
  if (verb == "erase") {
    if (t.size() < 3) {
      error = "usage: loop erase <slot>";
      return false;
    }
    std::uint16_t slot = 0;
    if (!parse_loop_slot(t[2], slot, error)) {
      return false;
    }
    Command c;
    c.param = Param::kLoopErase;
    c.idx = slot;
    m_engine.push_command(c, m_sink);
    return true;
  }

  // `loop undo <slot>` -- restores the single retained prior generation
  // (Fork E: ONE shared shadow generation across the whole pool -- a UI
  // affordance for "undo" must reflect this bounded, non-per-cell truth
  // honestly, docs/proposals/looper-in-gui-contract.md §6).
  if (verb == "undo") {
    if (t.size() < 3) {
      error = "usage: loop undo <slot>";
      return false;
    }
    std::uint16_t slot = 0;
    if (!parse_loop_slot(t[2], slot, error)) {
      return false;
    }
    Command c;
    c.param = Param::kLoopUndo;
    c.idx = slot;
    m_engine.push_command(c, m_sink);
    return true;
  }

  // `loop length <slot> auto|fixed <ticks>|quantized <grid>` (6400).
  if (verb == "length") {
    if (t.size() < 4) {
      error = "usage: loop length <slot> auto|fixed <ticks>|quantized <grid>";
      return false;
    }
    std::uint16_t slot = 0;
    if (!parse_loop_slot(t[2], slot, error)) {
      return false;
    }
    LoopLengthMode mode{};
    std::uint64_t value = 0;
    if (t[3] == "auto") {
      mode = LoopLengthMode::kAuto;
    } else if (t[3] == "fixed" && t.size() >= 5) {
      mode = LoopLengthMode::kFixed;
      if (!parse_u64(t[4], value)) {
        error = "bad length (ticks): " + t[4];
        return false;
      }
    } else if (t[3] == "quantized" && t.size() >= 5) {
      mode = LoopLengthMode::kQuantized;
      if (!parse_u64(t[4], value)) {
        error = "bad grid (ticks): " + t[4];
        return false;
      }
    } else {
      error = "usage: loop length <slot> auto|fixed <ticks>|quantized <grid>";
      return false;
    }
    Command c;
    c.op = Op::kSet;
    c.param = Param::kLoopLength;
    c.idx = slot;
    c.a = static_cast<std::int32_t>(mode);
    c.b = static_cast<std::int32_t>(value);
    m_engine.push_command(c, m_sink);
    return true;
  }

  error = "loop new|record|stop|erase|undo|length ...";
  return false;
}

}  // namespace arrangrr::host

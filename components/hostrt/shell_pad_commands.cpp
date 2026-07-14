#include <vector>

#include "midisrc/file_io.hpp"
#include "perf_v1_migrate.hpp"
#include "shell.hpp"
#include "shell_internal.hpp"

// Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): the L1 grammar for pad banks (`pad assign|trigger|release`,
// mirroring shell_clip_commands.cpp's own `clip add`/`launch`/`stop clip`
// convention) and the Performance recall store (`perf store|recall|save|
// load`). `perf save`/`perf load` are the ONE host-only file-I/O path
// (Architectural Principle #2: storage sits behind a HAL the core never
// touches) -- mirroring shell_io_commands.cpp's export_smf()/
// load_midi_source() precedent, here over performance.hpp's
// serialize()/deserialize() instead of a Standard MIDI File.

namespace arrangrr::host {

using namespace shell_detail;

namespace {

bool parse_pad_type(const std::string& s, PadType& out) {
  if (s == "none") {
    out = PadType::kNone;
  } else if (s == "phrase") {
    out = PadType::kPhrase;
  } else if (s == "chord") {
    out = PadType::kChord;
  } else if (s == "scene") {
    out = PadType::kSceneColumn;
  } else if (s == "variation") {
    out = PadType::kVariation;
  } else if (s == "fill") {
    out = PadType::kFill;
  } else if (s == "performance") {
    out = PadType::kPerformance;
  } else {
    return false;
  }
  return true;
}

bool parse_pad_mode(const std::string& s, PadMode& out) {
  if (s == "oneshot") {
    out = PadMode::kOneShot;
  } else if (s == "loop") {
    out = PadMode::kLoop;
  } else if (s == "hold") {
    out = PadMode::kHold;
  } else if (s == "toggle") {
    out = PadMode::kToggle;
  } else {
    return false;
  }
  return true;
}

bool parse_flat_pad_id(const std::string& s, std::uint16_t& out, std::string& error) {
  std::uint64_t v = 0;
  if (!parse_u64(s, v) || v >= kMaxPads) {
    error = "bad pad id: " + s;
    return false;
  }
  out = static_cast<std::uint16_t>(v);
  return true;
}

bool parse_perf_slot(const std::string& s, std::uint16_t& out, std::string& error) {
  std::uint64_t v = 0;
  if (!parse_u64(s, v) || v >= kMaxPerformances) {
    error = "bad performance slot: " + s;
    return false;
  }
  out = static_cast<std::uint16_t>(v);
  return true;
}

// Phase-6 Theme 3 Item #4 (companion to Item #2): the host L1 hook for
// kPadBankSelect -- left undriven from the host when Item #4 shipped the
// engine-side verb. Mirrors cmd_transpose's own validate-parse-only style
// (shell_music_commands.cpp): the engine is the source of truth for the
// [0, kMaxPadBanks) bound (Engine::pad_bank_select rejects outside it), this
// parse only rejects an unparsable token. Split out of cmd_pad (rather than
// inlined there) to keep cmd_pad's own cognitive complexity under the
// clang-tidy gate, same discipline as every other case-handler split in this
// codebase.
bool build_pad_bank_command(const std::string& token, Command& c, std::string& error) {
  std::uint64_t bank = 0;
  if (!parse_u64(token, bank) || bank > 0xFFFFFFFFu) {
    error = "bad pad bank: " + token;
    return false;
  }
  c.op = Op::kSet;
  c.param = Param::kPadBankSelect;
  c.a = static_cast<std::int32_t>(bank);
  return true;
}

// The whole-file buffer size: header + every possible slot's tight wire
// record + the trailing CRC-32. Sized exactly (not a magic guess) so
// perf_save() never has to grow-and-retry.
constexpr std::size_t kPerfFileBufferSize = sizeof(PerformanceStoreHeader) +
                                            kMaxPerformances * kPerformanceRecordWireSize +
                                            sizeof(std::uint32_t);

}  // namespace

// `pad assign <id> <type> <mode> <dest>[:ch] <source> [aux] [quantize <n>]`
// | `pad trigger <id>` | `pad release <id>` | `pad bank <n>`.
bool Shell::cmd_pad(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "usage: pad assign <id> <type> <mode> <dest>[:ch] <source> [aux] [quantize <n>] | "
      "pad trigger <id> | pad release <id> | pad bank <n>";
  if (t.size() < 2) {
    error = kUsage;
    return false;
  }
  if (t[1] == "bank" && t.size() >= 3) {
    Command c;
    if (!build_pad_bank_command(t[2], c, error)) {
      return false;
    }
    m_engine.push_command(c, m_sink);
    return true;
  }
  if ((t[1] == "trigger" || t[1] == "release") && t.size() >= 3) {
    std::uint16_t id = 0;
    if (!parse_flat_pad_id(t[2], id, error)) {
      return false;
    }
    Command c;
    c.param = t[1] == "trigger" ? Param::kPadTrigger : Param::kPadRelease;
    c.idx = id;
    m_engine.push_command(c, m_sink);
    return true;
  }
  if (t[1] != "assign" || t.size() < 7) {
    error = kUsage;
    return false;
  }
  Command c;
  if (!pad_assign_from_tokens(t, c, error)) {
    return false;
  }
  m_engine.push_command(c, m_sink);
  return true;
}

// `pad assign <id> <type> <mode> <dest>[:ch] <source> [aux] [quantize <n>]`'s
// own token parsing, split out of cmd_pad (see shell.hpp's own comment) --
// `t[1] == "assign"` and `t.size() >= 7` are already checked by the caller.
bool Shell::pad_assign_from_tokens(const std::vector<std::string>& t, Command& c,
                                   std::string& error) {
  std::uint16_t id = 0;
  if (!parse_flat_pad_id(t[2], id, error)) {
    return false;
  }
  PadType type{};
  if (!parse_pad_type(t[3], type)) {
    error = "unknown pad type: " + t[3];
    return false;
  }
  PadMode mode{};
  if (!parse_pad_mode(t[4], mode)) {
    error = "unknown pad mode: " + t[4];
    return false;
  }
  std::string dest_name;
  int dest_ch = -1;
  if (!split_port_channel(t[5], dest_name, dest_ch)) {
    error = "bad pad destination: " + t[5];
    return false;
  }
  const int dest_port = find_port(dest_name, false);
  if (dest_port < 0) {
    error = "unknown port: " + dest_name;
    return false;
  }
  std::uint64_t source_idx = 0;
  if (!parse_u64(t[6], source_idx) || source_idx > 0xFFFF) {
    error = "bad source index: " + t[6];
    return false;
  }
  std::size_t at = 7;
  std::uint64_t aux = 0;
  if (at < t.size() && t[at] != "quantize") {
    if (!parse_u64(t[at], aux) || aux > 0xFF) {
      error = "bad source aux: " + t[at];
      return false;
    }
    ++at;
  }
  Boundary boundary = Boundary::kImmediate;
  std::uint8_t n_bars = 1;
  if (!parse_quantize_suffix(t, at, boundary, n_bars, error)) {
    return false;
  }
  c.param = Param::kPadAssign;
  c.idx = id;
  c.a = static_cast<std::int32_t>(type) | (static_cast<std::int32_t>(mode) << 8) |
        (static_cast<std::int32_t>(boundary) << 16) |
        (static_cast<std::int32_t>(PadPitch::kFixed) << 24);
  c.b = dest_port | ((dest_ch < 0 ? 0 : dest_ch) << 8) | (static_cast<std::int32_t>(n_bars) << 16);
  c.c = static_cast<std::int32_t>(static_cast<std::uint32_t>(source_idx) |
                                  (static_cast<std::uint32_t>(aux) << 24));
  return true;
}

// `perf store <slot>` | `perf recall <slot> [quantize <n>]` | `perf save
// <file>` | `perf load <file>`.
bool Shell::cmd_perf(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "usage: perf store <slot> | perf recall <slot> [quantize <n>] | perf save <file> | "
      "perf load <file>";
  if (t.size() < 3) {
    error = kUsage;
    return false;
  }
  if (t[1] == "store") {
    std::uint16_t slot = 0;
    if (!parse_perf_slot(t[2], slot, error)) {
      return false;
    }
    Command c;
    c.param = Param::kPerformanceStore;
    c.idx = slot;
    m_engine.push_command(c, m_sink);
    return true;
  }
  if (t[1] == "recall") {
    std::uint16_t slot = 0;
    if (!parse_perf_slot(t[2], slot, error)) {
      return false;
    }
    Boundary boundary = Boundary::kImmediate;
    std::uint8_t n_bars = 1;
    if (!parse_quantize_suffix(t, 3, boundary, n_bars, error)) {
      return false;
    }
    Command c;
    c.param = Param::kPerformanceRecall;
    c.idx = slot;
    c.boundary = boundary;
    c.n_bars = n_bars;
    m_engine.push_command(c, m_sink);
    return true;
  }
  if (t[1] == "save") {
    return perf_save(t[2], error);
  }
  if (t[1] == "load") {
    return perf_load(t[2], error);
  }
  error = kUsage;
  return false;
}

bool Shell::perf_save(const std::string& path, std::string& error) {
  std::vector<std::uint8_t> buf(kPerfFileBufferSize);
  const std::size_t n =
      serialize(m_engine.performances(), Span<std::uint8_t>(buf.data(), buf.size()));
  if (n == 0) {
    error = "performance store too large to serialize (internal)";
    return false;
  }
  buf.resize(n);
  return midisrc::write_binary_file(path, buf, error);
}

// Tries the native format_version 2 deserialize() FIRST; on failure, falls
// back to the v1 -> v2 migrator (Phase-6 Theme 3 Item #3, P4) -- the ONE
// place a v1-on-disk file gets a second chance, since the core's own
// deserialize() hard-rejects anything but its own current version (line 588:
// the device never migrates). Only when BOTH fail does this report the
// original "bad performance file" error, unchanged from before this item.
bool Shell::perf_load(const std::string& path, std::string& error) {
  std::vector<std::uint8_t> buf;
  if (!midisrc::read_binary_file(path, buf, error)) {
    return false;
  }
  const Span<const std::uint8_t> data(buf.data(), buf.size());
  PerformanceStore loaded;
  if (deserialize(data, loaded) || migrate_performance_v1_to_v2(data, loaded)) {
    m_engine.performances() = loaded;
    return true;
  }
  error = "bad performance file: " + path;
  return false;
}

}  // namespace arrangrr::host

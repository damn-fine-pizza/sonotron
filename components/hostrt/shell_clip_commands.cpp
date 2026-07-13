#include "shell.hpp"
#include "shell_internal.hpp"

// Phase-5 Item #2 (docs/design/clip-primitive-design.md): the L1 grammar for
// the clip launch primitive -- `clip add <role> <scene> style|seq|track
// <selector>` (host/script-only registration, no design-doc verb of its own;
// see kClipAdd's own comment in abi.hpp for why it exists), plus the
// documented `launch clip <id> quantize <n>` / `stop clip <id> [quantize
// <n>]` / `launch scene <n> quantize <q>` (docs/design/ux-workstation.md
// §7/§8, grid_model.hpp's own kGridLaunchWired comment).

namespace arrangrr::host {

using namespace shell_detail;

namespace {

// Parses an optional trailing `quantize <n>` at token index `at` (one past
// the id/scene number). Absent -> Boundary::kImmediate. n == 0 -> immediate,
// n == 1 -> next bar, n > 1 -> next N bars (n_bars clamped to 255, the
// Command field's own width).
bool parse_quantize_suffix(const std::vector<std::string>& t, std::size_t at, Boundary& boundary,
                           std::uint8_t& n_bars, std::string& error) {
  boundary = Boundary::kImmediate;
  n_bars = 1;
  if (at >= t.size()) {
    return true;
  }
  if (t[at] != "quantize" || at + 1 >= t.size()) {
    error = "usage: ... quantize <n>";
    return false;
  }
  std::uint64_t n = 0;
  if (!parse_u64(t[at + 1], n)) {
    error = "bad quantize value: " + t[at + 1];
    return false;
  }
  if (n == 0) {
    boundary = Boundary::kImmediate;
  } else if (n == 1) {
    boundary = Boundary::kNextBar;
  } else {
    boundary = Boundary::kNextNBars;
    n_bars = n > 255 ? static_cast<std::uint8_t>(255) : static_cast<std::uint8_t>(n);
  }
  return true;
}

bool parse_clip_id(const std::string& s, std::uint16_t& out, std::string& error) {
  std::uint64_t value = 0;
  if (!parse_u64(s, value) || value > 0xFFFF) {
    error = "bad id: " + s;
    return false;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

}  // namespace

// `clip add <role> <scene> style <section>` / `clip add <role> <scene> seq
// <index>` / `clip add <role> <scene> track <index>`.
bool Shell::cmd_clip(const std::vector<std::string>& t, std::string& error) {
  if (t.size() != 6 || t[1] != "add") {
    error = "usage: clip add <role> <scene> style|seq|track <section|index>";
    return false;
  }
  TrackRole role{};
  if (!resolve_track_role(t[2], role)) {
    error = "unknown role: " + t[2];
    return false;
  }
  std::uint64_t scene = 0;
  if (!parse_u64(t[3], scene) || scene > 255) {
    error = "bad scene index: " + t[3];
    return false;
  }
  ContentKind kind{};
  std::uint64_t content_index = 0;
  if (t[4] == "style") {
    SectionType section{};
    if (!parse_section(t[5], section)) {
      error = "unknown section: " + t[5];
      return false;
    }
    kind = ContentKind::kStyleSection;
    content_index = static_cast<std::uint64_t>(section);
  } else if (t[4] == "seq") {
    if (!parse_u64(t[5], content_index)) {
      error = "bad sequence index: " + t[5];
      return false;
    }
    kind = ContentKind::kChordSequence;
  } else if (t[4] == "track") {
    if (!parse_u64(t[5], content_index)) {
      error = "bad track index: " + t[5];
      return false;
    }
    kind = ContentKind::kStepTrack;
  } else {
    error = "unknown clip content kind: " + t[4];
    return false;
  }
  Command c;
  c.param = Param::kClipAdd;
  c.a = static_cast<std::int32_t>(role);
  c.b = static_cast<std::int32_t>(scene);
  c.c = static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8);
  m_engine.push_command(c, m_sink);
  return true;
}

// `launch clip <id> quantize <n>` / `launch scene <n> quantize <q>`.
bool Shell::cmd_launch(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3 || (t[1] != "clip" && t[1] != "scene")) {
    error = "usage: launch clip <id> quantize <n> | launch scene <n> quantize <q>";
    return false;
  }
  std::uint16_t target = 0;
  if (!parse_clip_id(t[2], target, error)) {
    return false;
  }
  Boundary boundary = Boundary::kImmediate;
  std::uint8_t n_bars = 1;
  if (!parse_quantize_suffix(t, 3, boundary, n_bars, error)) {
    return false;
  }
  Command c;
  c.param = t[1] == "clip" ? Param::kClipLaunch : Param::kSceneQuantize;
  c.idx = target;
  c.boundary = boundary;
  c.n_bars = n_bars;
  m_engine.push_command(c, m_sink);
  return true;
}

// `stop clip <id> [quantize <n>]` (B3, ux-workstation.md §8; the quantize
// suffix is optional here -- an unquantized stop is immediate, matching the
// shorter form the table documents alongside grid_model.hpp's own longer one).
bool Shell::cmd_stop_clip(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3 || t[1] != "clip") {
    error = "usage: stop clip <id> [quantize <n>]";
    return false;
  }
  std::uint16_t target = 0;
  if (!parse_clip_id(t[2], target, error)) {
    return false;
  }
  Boundary boundary = Boundary::kImmediate;
  std::uint8_t n_bars = 1;
  if (!parse_quantize_suffix(t, 3, boundary, n_bars, error)) {
    return false;
  }
  Command c;
  c.param = Param::kClipStop;
  c.idx = target;
  c.boundary = boundary;
  c.n_bars = n_bars;
  m_engine.push_command(c, m_sink);
  return true;
}

}  // namespace arrangrr::host

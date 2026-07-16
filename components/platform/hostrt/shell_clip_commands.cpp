#include "shell.hpp"
#include "shell_internal.hpp"

// Phase-5 Item #2 (docs/design/clip-primitive-design.md): the L1 grammar for
// the clip launch primitive -- `clip add <role> <scene> style|seq|track
// <selector>` (host/script-only registration, no design-doc verb of its own;
// see kClipAdd's own comment in abi.hpp for why it exists), plus the
// documented `launch clip <id> quantize <n>` / `stop clip <id> [quantize
// <n>]` / `launch scene <n> quantize <q>` (docs/design/ux-workstation.md
// §7/§8, grid_model.hpp's own kGridLaunchWired comment).
//
// Phase 7 (node 6000, the Looper -- docs/proposals/looper-in-gui-contract.md
// §7 item 6): a fourth `loop <slot-id>` selector alongside style/seq/track,
// registering a ContentKind::kLoopBuffer clip referencing an EXISTING
// LoopBuffer slot (registered separately via `loop new`, shell_loop_
// commands.cpp) -- `content_index` carries the slot id exactly like `seq`/
// `track` carry their own pool index. This is the verb a future hold-gesture
// (item 11) drives to turn a recorded loop into a real grid cell.

namespace arrangrr::host {

using namespace shell_detail;

namespace {

// parse_quantize_suffix now lives in shell_internal.hpp/shell_parse.cpp
// (shell_detail namespace, already `using`-imported below) -- hoisted for
// Phase-5 Item #9's pad/performance verbs, which share the exact same
// `quantize <n>` spelling (shell_pad_commands.cpp).

bool parse_clip_id(const std::string& s, std::uint16_t& out, std::string& error) {
  std::uint64_t value = 0;
  if (!parse_u64(s, value) || value > 0xFFFF) {
    error = "bad id: " + s;
    return false;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

// Resolves `t[4]`'s content-kind selector (style|seq|track|loop) plus its
// own `t[5]` argument into `kind`/`content_index`, hoisted out of cmd_clip
// itself to keep that function's own cognitive complexity down (readability-
// function-cognitive-complexity) -- purely mechanical, no behavior change.
bool resolve_clip_content(const std::vector<std::string>& t, ContentKind& kind,
                          std::uint64_t& content_index, std::string& error) {
  if (t[4] == "style") {
    SectionType section{};
    if (!parse_section(t[5], section)) {
      error = "unknown section: " + t[5];
      return false;
    }
    kind = ContentKind::kStyleSection;
    content_index = static_cast<std::uint64_t>(section);
    return true;
  }
  if (t[4] == "seq") {
    if (!parse_u64(t[5], content_index)) {
      error = "bad sequence index: " + t[5];
      return false;
    }
    kind = ContentKind::kChordSequence;
    return true;
  }
  if (t[4] == "track") {
    if (!parse_u64(t[5], content_index)) {
      error = "bad track index: " + t[5];
      return false;
    }
    kind = ContentKind::kStepTrack;
    return true;
  }
  if (t[4] == "loop") {
    if (!parse_u64(t[5], content_index)) {
      error = "bad loop slot id: " + t[5];
      return false;
    }
    kind = ContentKind::kLoopBuffer;
    return true;
  }
  error = "unknown clip content kind: " + t[4];
  return false;
}

}  // namespace

// `clip add <role> <scene> style <section>` / `clip add <role> <scene> seq
// <index>` / `clip add <role> <scene> track <index>`, with an OPTIONAL
// trailing `id <n>` (Repeat-Zone binding contract Shape A, docs/proposals/
// repeat-zone-real-contract.md §3/§8b decision 1): additive-only, mirrors the
// established `quantize <n>` suffix idiom. Absent -> the Command carries
// kNoExplicitClipId, preserving the ORIGINAL sequential-append behavior
// byte-identically (every existing golden/script uses the absent-id form).
// Present -> the core registers AT that exact id instead (bounds/uniqueness
// validated core-side, Engine::clip_add).
bool Shell::cmd_clip(const std::vector<std::string>& t, std::string& error) {
  if ((t.size() != 6 && t.size() != 8) || t[1] != "add") {
    error = "usage: clip add <role> <scene> style|seq|track|loop <section|index|slot-id> [id <n>]";
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
  if (!resolve_clip_content(t, kind, content_index, error)) {
    return false;
  }
  std::uint16_t explicit_id = kNoExplicitClipId;
  if (t.size() == 8) {
    if (t[6] != "id") {
      error =
          "usage: clip add <role> <scene> style|seq|track|loop <section|index|slot-id> [id <n>]";
      return false;
    }
    if (!parse_clip_id(t[7], explicit_id, error)) {
      return false;
    }
    if (explicit_id == kNoExplicitClipId) {
      error = "bad id: " + t[7] + " (reserved sentinel value)";
      return false;
    }
  }
  Command c;
  c.param = Param::kClipAdd;
  c.idx = explicit_id;
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

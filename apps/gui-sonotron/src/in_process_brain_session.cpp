#include "in_process_brain_session.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include "arrangrr/abi.hpp"
#include "arrangrr/arranger/style_model.hpp"
#include "arrangrr/clip/clip_matrix.hpp"
#include "arrangrr/loop/loop_buffer.hpp"
#include "arrangrr/perf/performance.hpp"
#include "arrangrr/scene/scene_chain.hpp"
#include "audio/spsc_ring.hpp"
#include "brain_event_from_outevent.hpp"
#include "cadence_progressions.hpp"
#include "common/time.hpp"
#include "default_style_progressions.hpp"
#include "gm_program.hpp"
#include "midi_hal.hpp"
#include "ritardando_ramp.hpp"
#include "shell.hpp"

// The engine-thread half of Phase 2b's "integrated" mode (docs/design/
// sonotron-server-phase2-brief.md). Mirrors apps/sonotron-server/main.cpp's
// own live loop (AlsaMidi + Shell + a tick clock) closely on purpose --
// same backend shape, minus the UDS control socket (replaced by the Command
// ring) and minus the poll()-driven wakeup (replaced by a short sleep, since
// there is no fd to block on here). Hardware MIDI-in now reaches the engine
// too (docs/proposals/looper-in-gui-contract.md §7 item 3, fixing the stale
// "MIDI-in hardware is explicitly deferred" note this comment used to carry):
// run_engine() drains ALSA input every iteration, same as sonotron-server's
// own live loop.

namespace sonotron {

namespace {

using arrangrr::Boundary;
using arrangrr::BpmX100;
using arrangrr::Command;
using arrangrr::ContentKind;
using arrangrr::kBeatsPerBar;
using arrangrr::kMaxPorts;
using arrangrr::kMaxScenes;
using arrangrr::kMidiClockDivider;
using arrangrr::kMinBpm;
using arrangrr::kNoExplicitClipId;
using arrangrr::kNoLoopExplicitId;
using arrangrr::kPpqn;
using arrangrr::kSceneRepeatInfinite;
using arrangrr::LoopLengthMode;
using arrangrr::LoopRecordMode;
using arrangrr::Op;
using arrangrr::OutEvent;
using arrangrr::Param;
using arrangrr::Performance;
using arrangrr::SceneTransitionKind;
using arrangrr::section_is_ending;
using arrangrr::SectionType;
using arrangrr::Span;
using arrangrr::Style;
using arrangrr::StyleSection;
using arrangrr::TickAccumulator;
using arrangrr::TrackRole;
using arrangrr::host::IMidiHal;
using arrangrr::host::kAlsaClientName;
using arrangrr::host::make_midi_hal;
using arrangrr::host::PortDef;
using arrangrr::host::Shell;

// Human input rate is orders of magnitude below the tick rate; 1024 slots of
// a <=20 B Command is ~20 KB, trivial (Corelli §15.2's own sizing
// recommendation). The Command ring NEVER silently drops (see send()); the
// OutEvent ring MAY drop under backpressure (asymmetric policy, correction
// #3) -- 1024 slots of a <=16 B OutEvent (~16 KB) is equally generous
// headroom, not a tight bound.
constexpr std::size_t kCommandRingCapacity = 1024;
constexpr std::size_t kOutEventRingCapacity = 1024;

// Phase-6 Theme 2 port-filtering (owner decision, docs/phase6-design-
// reviews.md "Audio in the standalone GUI", Decision 3/4 -- differs from
// Corelli's own "realize every kMidi event unconditionally" default): the
// owner chose to realize ONLY the primary integrated output port, not every
// port. Today's default integrated wiring below opens exactly one output
// port, "out0", and Shell::cmd_port's m_next_out counter starts at 0
// (components/platform/hostrt/shell_io_commands.cpp) -- so out0 is always index 0. A
// single named constant, not a magic literal, so a future multi-port
// realization config only ever needs to change this one spot.
constexpr std::uint8_t kPrimaryAudioOutPort = 0;

// Follow-up to the Theme-2 QA pass's reachability finding
// (test_audio_primary_port_unreachable.cpp's own header comment,
// docs/phase6-design-reviews.md "Audio in the standalone GUI"): `style load`
// alone never routes any TrackRole to an output port (Route::enabled
// defaults to false, arranger.hpp) -- a freshly loaded style is silent until
// something issues `style route`, and no gui-sonotron panel exposes that
// verb. Owner decision: auto-enable default band routing the moment the GUI
// loads a style, so "load style -> play -> hear the band" needs zero new UI.
//
// This table is EXACTLY apps/demo/jam/setup.acmd's own `style route` lines,
// byte-for-byte (role name + the 1-based channel exactly as written after
// the ':' in that file's "out0:N") -- so a GUI-loaded style routes and
// sounds identically to the proven demo jam session, and there is no
// separate 1-vs-0-index channel judgement call to make here: it is the
// demo's own already-working numbers, copied. Roles absent from this table
// stay unrouted, mirroring the demo (it does not route drums-and-friends'
// remaining roles either). Every "out0" below resolves to kPrimaryAudioOutPort
// (0) -- see that constant's own comment -- so these routes land inside the
// Theme-2 audio gate.
struct DefaultStyleRoute {
  const char* role_name;
  std::uint8_t channel_one_based;
};
constexpr std::array<DefaultStyleRoute, 7> kDefaultStyleRoutes = {{
    {.role_name = "drums", .channel_one_based = 10},
    {.role_name = "perc", .channel_one_based = 10},
    {.role_name = "bass", .channel_one_based = 2},
    {.role_name = "chord1", .channel_one_based = 3},
    {.role_name = "chord2", .channel_one_based = 7},
    {.role_name = "pad", .channel_one_based = 5},
    {.role_name = "arp", .channel_one_based = 6},
}};

// `midi-source load <path>` (Accompany, Phase 4d) carries a variable-length
// filesystem path that the fixed-size ABI `Command` POD (abi.hpp, <=20 B, no
// string field) has no room for -- so it rides its own pair of rings instead
// of command_line_to_command()'s Command translation, mirroring --control's
// own Shell::load_midi_source() entry point (shell_io_commands.cpp) without
// duplicating any file-I/O or SMF-parsing logic here. This is a rare,
// user-initiated action (not a per-tick hot path), so a modest capacity is
// ample headroom, not a tight bound.
constexpr std::size_t kMaxPathBytes = 256;
constexpr std::size_t kPathCommandRingCapacity = 16;
constexpr std::size_t kMaxPathErrorBytes = 200;
constexpr std::size_t kPathResultRingCapacity = 16;

// GUI -> engine: a raw path, fixed-size and trivially copyable so it fits
// SpscRing's contract. `length` lets the trailing bytes stay unspecified
// (no need to zero-pad every push).
struct PathCommand {
  std::array<char, kMaxPathBytes> bytes{};
  std::uint16_t length = 0;
};

// Engine -> GUI: the outcome of one PathCommand. Only ever pushed on
// FAILURE (Shell::load_midi_source's own error string, truncated to fit) --
// success is silent, the same observable shape `style load`/`part ...`
// already have on the GUI side (command_line_to_command's own name/role
// resolution never emits an event on success either).
struct PathResult {
  std::array<char, kMaxPathErrorBytes> error_bytes{};
  std::uint16_t error_length = 0;
};

// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md):
// grid_panel.cpp's Play (auto-song armed) / manual scene-header click both
// need to hand the engine thread an ORDERED LIST of (section, bar-length)
// pairs -- one per populated scene column -- to build into a core SceneChain.
// A `Command` POD has no room for a variable-length list, so this rides its
// own pair of rings, mirroring the PathCommand precedent above: fixed-size,
// trivially copyable, rare/user-initiated (one push per Play press or scene
// click), never a per-tick hot path.
constexpr std::size_t kMaxSongScenes = 8;  // grid_model.hpp's GridModel::kMaxSceneCount
constexpr std::size_t kSongBuildRingCapacity = 4;

// Task #5 (per-section REPEAT COUNT, Phase-1: host-only, ZERO ABI/core
// change): the sentinel a SongBuildScene::repeat carries to mean "hold this
// scene forever" -- distinct from any real finite repeat count (those are
// always >= 1, never 0). Mirrors GridModel::kSceneRepeatInfinite's own
// "hold forever" sentinel one layer up (grid_panel.cpp's build_and_play_song
// translates GridModel's kSceneRepeatInfinite into the wire's own literal
// "inf" token, decoded back into THIS constant below) -- deliberately a
// SEPARATE constant, not the same numeric value, since this file has no
// dependency on grid_model.hpp (D38: this whole wire-translation layer never
// reaches into the GUI-side model headers) and the wire's own token grammar
// ("inf", not a magic number) is what actually crosses that boundary.
constexpr std::uint8_t kSongBuildRepeatInfinite = 0;

// Song-mode Phase 2 (docs/proposals/song-mode-scenechain-adoption.md's own
// Phasing section): a scene's own STYLE/GROOVE/KEY/TEMPO overrides, carried
// on the wire right alongside section/n_bars/repeat -- one more `Performance`
// field group apply_song_build's existing base-capture-and-override loop
// (below) now copies onto each scene's own `Performance` before it is
// stored, exactly the same mechanism Phase 1 already uses for `variation`.
// No new engine-side event/watcher is needed: the core's own SceneChain ->
// apply_scene_transition -> apply_performance chain (engine.cpp) already
// applies whatever is baked into a step's stored Performance the instant
// that step becomes active -- this struct only widens what gets baked in.
//
// Sentinels mirror the exact conventions GridModel::SceneGroove/kNoStyle
// Override/kNoTempoOverride already use one layer up (D38: this file still
// has no dependency on grid_model.hpp, same wire-token-not-shared-constant
// discipline as kSongBuildRepeatInfinite above): style_id 0xFFFF is the
// SAME sentinel core Performance::style_id already uses for "keep current"
// (Engine::apply_performance skips Arranger::load() outright when it sees
// it), so it needs no translation at all, unlike kSongBuildRepeatInfinite's
// deliberately-distinct numeric encoding. tempo_x100 0 is never a real tempo
// (arrangrr::kMinBpm is 2000), so it is an unambiguous "inherit" value the
// same way GridModel's own kNoTempoOverride is. Groove and key are each
// gated by their own explicit override flag (has_groove_override/
// has_key_override), matching GridModel's own reasoning: 0 is a musically
// valid value for every one of those fields, so no numeric sentinel could
// serve as "inherit" there.
struct SongBuildScene {
  SectionType section = SectionType::kVarA;
  std::uint8_t n_bars = 1;
  // Task #5: 1 (the default) plays this scene once before the chain
  // advances to the next; N in [2,255] repeats it N times (apply_song_build
  // below emits N consecutive, identical kSceneAdd steps referencing the
  // SAME PerformanceStore slot); kSongBuildRepeatInfinite (0) holds this
  // scene forever and truncates the rest of the built chain -- see apply_
  // song_build's own header comment for why that costs only ONE kSceneAdd
  // step, not an unbounded one.
  std::uint8_t repeat = 1;
  std::uint16_t style_id = 0xFFFF;  // 0xFFFF == core's own "keep current" sentinel
  bool has_groove_override = false;
  // swing, humanize_timing, humanize_velocity, accent, swing_grid, quantize --
  // index-parallel with arrangrr::GrooveParams' own field order.
  std::array<std::uint8_t, 6> groove{0, 0, 0, 0, 8, 0};
  bool has_key_override = false;
  std::uint8_t key_root = 0;     // pitch class 0..11
  std::uint8_t key_mode = 0;     // arrangrr::Mode
  std::uint16_t tempo_x100 = 0;  // 0 == inherit (arrangrr::kMinBpm is 2000, never a real tempo)
};

struct SongBuildCommand {
  std::array<SongBuildScene, kMaxSongScenes> scenes{};
  std::uint8_t scene_count = 0;
};

// Splits on ASCII space (single delimiter, no quoting) -- sufficient for the
// fixed-shape command lines translated below; Shell's own tokenizer (private
// to hostrt) does the same for exec_line's richer grammar.
// A bare non-negative decimal integer (clip/scene ids, quantize counts) --
// enough for the fixed-shape lines translated below; no sign, no whitespace.
bool parse_uint(std::string_view s, std::uint64_t& out) {
  if (s.empty()) {
    return false;
  }
  std::uint64_t value = 0;
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<std::uint64_t>(c - '0');
  }
  out = value;
  return true;
}

// A signed decimal integer with an optional leading '-' (Phase-6 Theme 3
// Item #1's `transpose <-12..12>` line) -- parse_uint's twin, kept as a
// small separate helper rather than widening parse_uint's own no-sign
// contract (every OTHER caller here relies on that never accepting '-').
bool parse_int(std::string_view s, std::int64_t& out) {
  if (s.empty()) {
    return false;
  }
  const bool negative = s[0] == '-';
  const std::string_view digits = negative ? s.substr(1) : s;
  std::uint64_t magnitude = 0;
  if (!parse_uint(digits, magnitude)) {
    return false;
  }
  out = negative ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
  return true;
}

// Decodes an optional trailing `quantize <n>` starting at token index `at` in
// a `launch clip/scene ...` or `stop clip ...` line (mirrors
// components/platform/hostrt/shell_clip_commands.cpp's own parse_quantize_suffix --
// deliberate small duplication, D38: this pure-client translator never
// reaches into hostrt's own parsing helpers). `ok` is set false only on a
// MALFORMED trailing quantize (present but unparsable); an ABSENT suffix is
// a valid immediate default.
bool parse_quantize_suffix(const std::vector<std::string_view>& t, std::size_t at,
                           Boundary& boundary, std::uint8_t& n_bars) {
  boundary = Boundary::kImmediate;
  n_bars = 1;
  if (at >= t.size()) {
    return true;
  }
  if (t[at] != "quantize" || at + 1 >= t.size()) {
    return false;
  }
  std::uint64_t n = 0;
  if (!parse_uint(t[at + 1], n)) {
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

// Resolves a section-name token to a SectionType (Repeat-Zone binding
// contract, docs/proposals/repeat-zone-real-contract.md §3/§8b decision 1's
// `clip add ... style <section> id <n>` line, grid_panel.cpp's own send()
// shape). Mirrors components/platform/hostrt/shell_parse.cpp's own
// parse_section() spellings exactly, duplicated deliberately (D38: this
// pure-client translator never reaches into hostrt's own parsing helpers --
// same discipline parse_quantize_suffix above already documents).
bool parse_section_name(std::string_view s, SectionType& out) {
  struct Entry {
    std::string_view name;
    SectionType type;
  };
  static constexpr Entry kSections[] = {
      {.name = "intro1", .type = SectionType::kIntro1},
      {.name = "intro2", .type = SectionType::kIntro2},
      {.name = "varA", .type = SectionType::kVarA},
      {.name = "varB", .type = SectionType::kVarB},
      {.name = "varC", .type = SectionType::kVarC},
      {.name = "varD", .type = SectionType::kVarD},
      {.name = "fillA", .type = SectionType::kFillA},
      {.name = "fillB", .type = SectionType::kFillB},
      {.name = "fillC", .type = SectionType::kFillC},
      {.name = "fillD", .type = SectionType::kFillD},
      {.name = "break", .type = SectionType::kBreak},
      {.name = "ending1", .type = SectionType::kEnding1},
      {.name = "ending2", .type = SectionType::kEnding2},
  };
  for (const Entry& e : kSections) {
    if (s == e.name) {
      out = e.type;
      return true;
    }
  }
  return false;
}

// Splits `s` on every occurrence of `sep` (no quoting, no escaping) -- the
// SongBuildScene's own groove/key sub-tokens (comma/colon-joined decimals)
// are the only callers; mirrors split_ws's own "no delimiter merging needed"
// simplicity, just with a caller-chosen separator instead of a fixed set of
// whitespace bytes.
std::vector<std::string_view> split_on(std::string_view s, char sep) {
  std::vector<std::string_view> tokens;
  std::size_t start = 0;
  while (start <= s.size()) {
    const std::size_t pos = s.find(sep, start);
    const std::size_t end = pos == std::string_view::npos ? s.size() : pos;
    tokens.push_back(s.substr(start, end - start));
    if (pos == std::string_view::npos) {
      break;
    }
    start = pos + 1;
  }
  return tokens;
}

// Song-mode Phase 2 (docs/proposals/song-mode-scenechain-adoption.md's own
// Phasing section): decodes the FOUR trailing per-scene tokens `song build`
// grew alongside `<section> <bars> <repeat>` -- `<style> <groove> <key>
// <tempo>`. Every one of the four accepts the literal `-` for "no override"
// (SongBuildScene's own default-constructed sentinels already ARE "no
// override", so a `-` token simply leaves `scene` untouched field-by-field).
// `style_token`: `-` or a decimal builtin style-table index.
// `groove_token`: `-` or six comma-joined decimals `swing,humanize_timing,
// humanize_velocity,accent,swing_grid,quantize` (SongBuildScene::groove's own
// field order).
// `key_token`: `-` or `root:mode` (two colon-joined decimals).
// `tempo_token`: `-` or a decimal tempo_x100 (BPM * 100).
// Returns false (with `detail` set) on any malformed non-`-` token; `scene`
// is only ever partially written on failure (the caller discards the whole
// SongBuildCommand on any parse failure anyway, mirroring every other
// per-scene parse failure in this line's own handler below).
bool parse_song_build_scene_overrides(std::string_view style_token, std::string_view groove_token,
                                      std::string_view key_token, std::string_view tempo_token,
                                      SongBuildScene& scene, std::string& detail) {
  if (style_token != "-") {
    std::uint64_t style_id = 0;
    if (!parse_uint(style_token, style_id) || style_id > 0xFFFE) {
      detail = "bad style override: " + std::string(style_token);
      return false;
    }
    scene.style_id = static_cast<std::uint16_t>(style_id);
  }
  if (groove_token != "-") {
    const std::vector<std::string_view> fields = split_on(groove_token, ',');
    if (fields.size() != scene.groove.size()) {
      detail = "bad groove override (need 6 comma-joined values): " + std::string(groove_token);
      return false;
    }
    for (std::size_t i = 0; i < fields.size(); ++i) {
      std::uint64_t value = 0;
      if (!parse_uint(fields[i], value) || value > 255) {
        detail = "bad groove override field: " + std::string(fields[i]);
        return false;
      }
      scene.groove[i] = static_cast<std::uint8_t>(value);
    }
    scene.has_groove_override = true;
  }
  if (key_token != "-") {
    const std::vector<std::string_view> fields = split_on(key_token, ':');
    std::uint64_t root = 0;
    std::uint64_t mode = 0;
    if (fields.size() != 2 || !parse_uint(fields[0], root) || root > 11 ||
        !parse_uint(fields[1], mode) || mode > 6) {
      detail =
          "bad key override (need root:mode, root 0..11, mode 0..6): " + std::string(key_token);
      return false;
    }
    scene.key_root = static_cast<std::uint8_t>(root);
    scene.key_mode = static_cast<std::uint8_t>(mode);
    scene.has_key_override = true;
  }
  if (tempo_token != "-") {
    std::uint64_t tempo = 0;
    if (!parse_uint(tempo_token, tempo) || tempo == 0 || tempo > 0xFFFF) {
      detail = "bad tempo override: " + std::string(tempo_token);
      return false;
    }
    scene.tempo_x100 = static_cast<std::uint16_t>(tempo);
  }
  return true;
}

std::vector<std::string_view> split_ws(std::string_view s) {
  std::vector<std::string_view> tokens;
  std::size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && s[i] == ' ') {
      ++i;
    }
    const std::size_t start = i;
    while (i < s.size() && s[i] != ' ') {
      ++i;
    }
    if (i > start) {
      tokens.push_back(s.substr(start, i - start));
    }
  }
  return tokens;
}

// Task #11 Phase 1 (Sequence Edit step sequencer): mirrors shell_music_
// commands.cpp's own parse_step_lock exactly (D38 duplication, same
// discipline as parse_quantize_suffix/parse_section_name above) -- this
// pure-client translator never reaches into hostrt's own parsing helpers.
bool parse_step_lock(std::string_view key, std::string_view val, std::uint64_t& probability,
                     std::uint64_t& ratchet, std::uint64_t& micro, bool& tie, std::string& error) {
  if (key == "prob") {
    if (!parse_uint(val, probability) || probability > 100) {
      error = "bad prob (0..100): " + std::string(val);
      return false;
    }
  } else if (key == "ratchet") {
    if (!parse_uint(val, ratchet) || ratchet < 1 || ratchet > 8) {
      error = "bad ratchet (1..8): " + std::string(val);
      return false;
    }
  } else if (key == "micro") {
    if (!parse_uint(val, micro) || micro > 127) {
      error = "bad micro (0..127, forward-only): " + std::string(val);
      return false;
    }
  } else if (key == "tie") {
    if (val == "on") {
      tie = true;
    } else if (val == "off") {
      tie = false;
    } else {
      error = "bad tie (on|off): " + std::string(val);
      return false;
    }
  } else {
    error = "track step: unknown param-lock: " + std::string(key);
    return false;
  }
  return true;
}

// Outcome of translating one L1 text line into a Command POD.
enum class TranslateOutcome {
  kOk,               // `out` holds the translated Command.
  kUnknownCommand,   // the line is not one this translator recognizes at all.
  kInvalidArgument,  // recognized shape, but a name/role did not resolve --
                     // `detail` holds a human-readable reason.
};

// Phase-6 Theme 3 Item #2's companion (docs/reflections/phase6-theme3-pad-
// drum-cc-scope.md): `pad bank <n>` -- the kPadBankSelect host hook Item #4
// left undriven from either host, mirrors components/platform/hostrt/
// shell_pad_commands.cpp's own `pad bank` verb. The engine is the source of
// truth for the [0, kMaxPadBanks) bound (Engine::pad_bank_select rejects
// outside it); this parse only rejects an unparsable token. Split out of
// command_line_to_command (rather than inlined there) to keep that already
// large dispatch's cognitive complexity from growing further, same
// discipline as hostrt's own case-handler splits.
TranslateOutcome translate_pad_bank(const std::vector<std::string_view>& t, Command& out,
                                    std::string& detail) {
  std::uint64_t bank = 0;
  if (!parse_uint(t[2], bank) || bank > 0xFFFFFFFFu) {
    detail = "bad pad bank: " + std::string(t[2]);
    return TranslateOutcome::kInvalidArgument;
  }
  out.op = Op::kSet;
  out.param = Param::kPadBankSelect;
  out.a = static_cast<std::int32_t>(bank);
  return TranslateOutcome::kOk;
}

// Translates the L1 command lines gui-sonotron's panels currently send
// (transport_panel.cpp, main.cpp's Transport menu, browser_panel.cpp's style
// tree, parts_panel.cpp's mute/solo checkboxes) directly into the ABI
// Command. `style load <name>` and `part <role> mute|solo on|off` need
// Shell-side name resolution (builtin style name -> index, track-role name ->
// enum) -- reached through Shell::resolve_style_index()/resolve_track_role(),
// the two public pure lookups exposed for exactly this caller (see shell.hpp)
// -- so no dispatch logic is duplicated and no Shell-internal header leaks
// into this translation unit.
TranslateOutcome command_line_to_command(std::string_view line, Command& out, std::string& detail) {
  out = Command{};
  out.op = Op::kDo;
  if (line == "transport start") {
    out.param = Param::kTransportStart;
    return TranslateOutcome::kOk;
  }
  if (line == "transport stop") {
    out.param = Param::kTransportStop;
    return TranslateOutcome::kOk;
  }
  if (line == "transport continue") {
    out.param = Param::kTransportContinue;
    return TranslateOutcome::kOk;
  }
  if (line == "panic") {
    out.param = Param::kPanic;
    return TranslateOutcome::kOk;
  }

  const std::vector<std::string_view> t = split_ws(line);

  if (t.size() == 3 && t[0] == "style" && t[1] == "load") {
    const std::string name(t[2]);
    const int index = Shell::resolve_style_index(name);
    if (index < 0) {
      detail = "unknown style: " + name;
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kStyleLoad;
    out.a = index;
    return TranslateOutcome::kOk;
  }

  // `style switch <name> [section <section-name>]` -- the LIVE morph
  // counterpart to `style load` above (browser_panel.cpp sends this instead
  // of `style load` whenever the transport is already playing): rides the
  // existing Param::kStyleSwitch verb (engine.cpp's style_switch()), which
  // quantizes to the next bar boundary while playing instead of hard-
  // resetting the arranger.
  //
  // Owner task #2: switching style used to always force SectionType::kVarA,
  // so changing style while playing silently reverted whatever section was
  // actually active. This translator is a pure text->Command mapper with no
  // access to AppState/GridModel (grid_model.hpp's documented gap) -- it
  // cannot look up the current section itself -- so the caller now passes it
  // explicitly through the optional `section <name>` suffix (browser_panel.
  // cpp resolves it from AppState::section(), the engine's own last-
  // committed section echo, before calling send()). The 3-token form (no
  // suffix) is kept and still defaults to SectionType::kVarA -- the
  // arranger's own default section -- for any caller that does not (or
  // cannot) supply one, e.g. test_audio_primary_port_reachable.cpp's plain
  // `style switch basic`.
  if (t.size() == 3 && t[0] == "style" && t[1] == "switch") {
    const std::string name(t[2]);
    const int index = Shell::resolve_style_index(name);
    if (index < 0) {
      detail = "unknown style: " + name;
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kStyleSwitch;
    out.a = index;
    out.b = static_cast<std::int32_t>(SectionType::kVarA);
    out.boundary = Boundary::kNextBar;
    return TranslateOutcome::kOk;
  }
  if (t.size() == 5 && t[0] == "style" && t[1] == "switch" && t[3] == "section") {
    const std::string name(t[2]);
    const int index = Shell::resolve_style_index(name);
    if (index < 0) {
      detail = "unknown style: " + name;
      return TranslateOutcome::kInvalidArgument;
    }
    SectionType section{};
    if (!parse_section_name(t[4], section)) {
      detail = "unknown section: " + std::string(t[4]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kStyleSwitch;
    out.a = index;
    out.b = static_cast<std::int32_t>(section);
    out.boundary = Boundary::kNextBar;
    return TranslateOutcome::kOk;
  }

  // `style section <name>` -- SLICE 4a item 5 (docs/proposals/repeat-zone-
  // real-contract.md): grid_panel.cpp's scene-header ▶ click applies the
  // COLUMN's own SectionType through the EXISTING Param::kStyleSection verb
  // (mirrors components/platform/hostrt/shell_music_commands.cpp's own
  // `style section` L1 verb exactly, including its parse_section_name
  // spellings above). Quantizes to the next bar while playing, immediate
  // when stopped -- Engine::cmd_style's own kStyleSection handling
  // (Arranger::request(section, !playing)), not reimplemented here.
  if (t.size() == 3 && t[0] == "style" && t[1] == "section") {
    SectionType section{};
    if (!parse_section_name(t[2], section)) {
      detail = "unknown section: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kStyleSection;
    out.a = static_cast<std::int32_t>(section);
    return TranslateOutcome::kOk;
  }

  // Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
  // scope.md): `transpose <-12..12>`, the live global transpose -- mirrors
  // components/platform/hostrt/shell_music_commands.cpp's own `transpose` L1 verb.
  // The engine itself is the source of truth for the bound
  // (Engine::cmd_master_transpose rejects outside [-12, +12]); this parse
  // only rejects an unparsable token.
  if (t.size() == 2 && t[0] == "transpose") {
    std::int64_t semitones = 0;
    if (!parse_int(t[1], semitones) || semitones < -12 || semitones > 12) {
      detail = "bad transpose (-12..12): " + std::string(t[1]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = Param::kMasterTranspose;
    out.a = static_cast<std::int32_t>(semitones);
    return TranslateOutcome::kOk;
  }

  // Tempo nudge (transport_panel.cpp's BPM label): `bpm <N>` sets the global
  // tempo, mirroring components/platform/hostrt/shell.cpp's own `bpm` L1 verb. The GUI
  // widget only ever emits an integer BPM; the engine is the source of truth
  // for the range (it clamps to 20..400, shell_parse.cpp), so we reject only an
  // unparsable/out-of-range token here for a clean error. a = bpm_x100
  // (abi.hpp Param::kTransportTempo).
  if (t.size() == 2 && t[0] == "bpm") {
    std::int64_t bpm = 0;
    if (!parse_int(t[1], bpm) || bpm < 20 || bpm > 400) {
      detail = "bad bpm (20..400): " + std::string(t[1]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = Param::kTransportTempo;
    out.a = static_cast<std::int32_t>(bpm * 100);
    return TranslateOutcome::kOk;
  }

  if (t.size() == 3 && t[0] == "pad" && t[1] == "bank") {
    return translate_pad_bank(t, out, detail);
  }

  if (t.size() == 4 && t[0] == "part" && (t[2] == "mute" || t[2] == "solo") &&
      (t[3] == "on" || t[3] == "off")) {
    const std::string role_name(t[1]);
    TrackRole role{};
    if (!Shell::resolve_track_role(role_name, role)) {
      detail = "unknown role: " + role_name;
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = t[2] == "mute" ? Param::kPartMute : Param::kPartSolo;
    out.a = static_cast<std::int32_t>(role);
    out.b = t[3] == "on" ? 1 : 0;
    return TranslateOutcome::kOk;
  }

  // `program <port>[:ch] <voice>` -- Workstream B: the Browser's Voices/Kits
  // pickers (browser_model.cpp's build_program_verb/build_kit_verb) already
  // emit this exact line but nothing here translated it, so both pickers
  // silently no-op in the default in-process backend. Mirrors
  // components/platform/hostrt/shell_music_commands.cpp's own Shell::
  // cmd_program grammar and encoding (`b = port | (channel << 8)`, channel
  // 0-based) exactly, EXCEPT for port name resolution: cmd_program resolves
  // an arbitrary named port through Shell::find_port's full port table
  // (reachable only from inside Shell); this pure-client translator has no
  // such table (D38 discipline, same as the note/loop arms above), so it
  // accepts only the ONE named port the integrated engine's own default
  // topology ever opens ("out0", kPrimaryAudioOutPort -- see that constant's
  // own comment above) plus a bare numeric index (the note/loop arms' own
  // existing convention), and rejects anything else as an unknown port.
  if (t.size() >= 3 && t[0] == "program") {
    const std::string_view port_token = t[1];
    std::string_view port_name = port_token;
    int channel = -1;
    const std::size_t colon = port_token.find(':');
    if (colon != std::string_view::npos) {
      port_name = port_token.substr(0, colon);
      std::uint64_t ch = 0;
      if (!parse_uint(port_token.substr(colon + 1), ch) || ch < 1 || ch > 16) {
        detail = "bad program destination: " + std::string(port_token);
        return TranslateOutcome::kInvalidArgument;
      }
      channel = static_cast<int>(ch - 1);
    }
    std::uint64_t port = 0;
    if (port_name == "out0") {
      port = kPrimaryAudioOutPort;
    } else if (!parse_uint(port_name, port) || port >= kMaxPorts) {
      detail = "unknown output port: " + std::string(port_name);
      return TranslateOutcome::kInvalidArgument;
    }
    std::string voice(t[2]);
    for (std::size_t i = 3; i < t.size(); ++i) {
      voice += ' ';
      voice += t[i];
    }
    const int program = arrangrr::host::parse_gm_program(voice);
    if (program < 0) {
      detail = "unknown GM voice: " + voice;
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = Param::kProgram;
    out.a = program;
    out.b = static_cast<std::int32_t>(port) | ((channel < 0 ? 0 : channel) << 8);
    return TranslateOutcome::kOk;
  }

  // Repeat-Zone binding contract, Shape A (docs/proposals/repeat-zone-real-
  // contract.md §3/§8b decision 1): `clip add <role> <scene> style <section>
  // id <n>` -- grid_panel.cpp's own on-first-fill registration (a browser
  // style drop), ALWAYS carrying an explicit id (the GUI's own
  // cell_id(role,scene)), so the core registers AT that stable id instead of
  // the sequential counter. Only ContentKind::kStyleSection is reachable from
  // here (owner decision 2, §8b: drag-a-style ONLY for this pass -- no
  // seq/track authoring from the GUI yet); the CLI/script `clip add` grammar
  // (shell_clip_commands.cpp) still covers seq/track for host/script use.
  if (t.size() == 8 && t[0] == "clip" && t[1] == "add" && t[4] == "style" && t[6] == "id") {
    TrackRole role{};
    if (!Shell::resolve_track_role(std::string(t[2]), role)) {
      detail = "unknown role: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t scene = 0;
    if (!parse_uint(t[3], scene) || scene > 255) {
      detail = "bad scene index: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    SectionType section{};
    if (!parse_section_name(t[5], section)) {
      detail = "unknown section: " + std::string(t[5]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t id = 0;
    if (!parse_uint(t[7], id) || id >= static_cast<std::uint64_t>(kNoExplicitClipId)) {
      detail = "bad id: " + std::string(t[7]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kClipAdd;
    out.idx = static_cast<std::uint16_t>(id);
    out.a = static_cast<std::int32_t>(role);
    out.b = static_cast<std::int32_t>(scene);
    out.c = static_cast<std::int32_t>(ContentKind::kStyleSection) |
            (static_cast<std::int32_t>(section) << 8);
    return TranslateOutcome::kOk;
  }

  // Task #11 Phase 1 (Sequence Edit step sequencer, HOST-ONLY, zero ABI
  // change -- roadmap node 11600/11610): `clip add <role> <scene> track
  // <track-idx> id <n>` -- the kClipAdd counterpart to the style case above,
  // registering a ContentKind::kStepTrack clip referencing an EXISTING step
  // track (created via `track new` below) at this cell's own stable id.
  // Mirrors the style case's shape exactly: t[4] == "track" instead of
  // "style", t[5] a bare numeric track index instead of a section name.
  if (t.size() == 8 && t[0] == "clip" && t[1] == "add" && t[4] == "track" && t[6] == "id") {
    TrackRole role{};
    if (!Shell::resolve_track_role(std::string(t[2]), role)) {
      detail = "unknown role: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t scene = 0;
    if (!parse_uint(t[3], scene) || scene > 255) {
      detail = "bad scene index: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t track_idx = 0;
    if (!parse_uint(t[5], track_idx) || track_idx > 0xFFFF) {
      detail = "bad track index: " + std::string(t[5]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t id = 0;
    if (!parse_uint(t[7], id) || id >= static_cast<std::uint64_t>(kNoExplicitClipId)) {
      detail = "bad id: " + std::string(t[7]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kClipAdd;
    out.idx = static_cast<std::uint16_t>(id);
    out.a = static_cast<std::int32_t>(role);
    out.b = static_cast<std::int32_t>(scene);
    out.c = static_cast<std::int32_t>(ContentKind::kStepTrack) |
            (static_cast<std::int32_t>(track_idx) << 8);
    return TranslateOutcome::kOk;
  }

  // `track new <role> <port> <channel>` -- mirrors Shell::track_new
  // (shell_music_commands.cpp) exactly for the Command shape (a = role, b =
  // port | (channel << 8)), but with a BARE NUMERIC port/channel instead of
  // Shell's named-port + optional ":ch" resolution (D38: this pure-client
  // translator has no Shell state to resolve a named port against, same
  // convention the `note <port> ...` verb above already established).
  // `channel` here is already 0-based, mapping directly onto Command::b's
  // own 0-based packing with no extra +/-1 step. The CALLER (grid_panel.cpp's
  // step-track creation gesture) is responsible for following this with
  // `track mute <idx> on` (see below): arrangrr::Timeline::add_track's own
  // Track defaults to mute=false, so an unmuted new track would sound the
  // instant the transport is running (Timeline fires every registered track
  // unconditionally).
  if (t.size() == 5 && t[0] == "track" && t[1] == "new") {
    TrackRole role{};
    if (!Shell::resolve_track_role(std::string(t[2]), role)) {
      detail = "unknown role: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t port = 0;
    if (!parse_uint(t[3], port) || port >= kMaxPorts) {
      detail = "bad track port: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t channel = 0;
    if (!parse_uint(t[4], channel) || channel > 15) {
      detail = "bad track channel: " + std::string(t[4]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kTrackNew;
    out.a = static_cast<std::int32_t>(role);
    out.b = static_cast<std::int32_t>(port) | (static_cast<std::int32_t>(channel) << 8);
    return TranslateOutcome::kOk;
  }

  // `track step <idx> <step#> <note|clear> [vel] [gate] [prob=][ratchet=]
  // [micro=][tie=]` -- mirrors Shell::track_step (shell_music_commands.cpp)
  // exactly for validation/encoding, with a BARE NUMERIC track index instead
  // of Shell's name lookup (same D38 convention as `track new` above).
  // `<step#>` stays 1-based (mirrors the CLI's own convention exactly). `64`
  // below mirrors arrangrr::kMaxStepsPerTrack (config.hpp), hand-copied (D38).
  if (t.size() >= 5 && t[0] == "track" && t[1] == "step") {
    std::uint64_t idx = 0;
    if (!parse_uint(t[2], idx) || idx > 0xFFFF) {
      detail = "bad track index: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t step = 0;
    if (!parse_uint(t[3], step) || step < 1 || step > 64) {
      detail = "bad step number: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kTrackStep;
    out.idx = static_cast<std::uint16_t>(idx);
    out.a = static_cast<std::int32_t>(step - 1);  // wire is 0-based
    if (t[4] == "clear") {
      out.b = 0;
      out.c = 0;
      return TranslateOutcome::kOk;
    }
    std::uint64_t note = 0;
    if (!parse_uint(t[4], note) || note > 127) {
      detail = "bad note: " + std::string(t[4]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t vel = 100;
    std::uint64_t gate = 120;  // half a step (240 ticks/step / 2), mirrors the CLI's own default
    std::uint64_t probability = 100;
    std::uint64_t ratchet = 1;
    std::uint64_t micro = 0;
    bool tie = false;
    bool has_locks = false;
    int positional = 0;  // 0 -> vel, 1 -> gate
    for (std::size_t i = 5; i < t.size(); ++i) {
      const std::string_view tok = t[i];
      const std::size_t eq = tok.find('=');
      if (eq == std::string_view::npos) {
        if (positional == 0) {
          if (!parse_uint(tok, vel) || vel < 1 || vel > 127) {
            detail = "bad velocity: " + std::string(tok);
            return TranslateOutcome::kInvalidArgument;
          }
          positional = 1;
        } else if (positional == 1) {
          if (!parse_uint(tok, gate) || gate == 0 || gate > 0xFFFF) {
            detail = "bad gate: " + std::string(tok);
            return TranslateOutcome::kInvalidArgument;
          }
          positional = 2;
        } else {
          detail = "track step: unexpected token: " + std::string(tok);
          return TranslateOutcome::kInvalidArgument;
        }
        continue;
      }
      has_locks = true;
      if (!parse_step_lock(tok.substr(0, eq), tok.substr(eq + 1), probability, ratchet, micro, tie,
                           detail)) {
        return TranslateOutcome::kInvalidArgument;
      }
    }
    out.b = static_cast<std::int32_t>(note) | (static_cast<std::int32_t>(vel) << 8);
    out.c = static_cast<std::int32_t>(gate);
    if (has_locks) {
      // Opt-in extended encoding (ABI kTrackStep): bit 31 of c flags the
      // locks, mirroring abi.hpp's own kTrackStep comment exactly.
      out.b |= static_cast<std::int32_t>(probability << 16) |
               static_cast<std::int32_t>(ratchet << 24) |
               static_cast<std::int32_t>(tie ? (1u << 28) : 0u);
      out.c |= static_cast<std::int32_t>((static_cast<std::uint32_t>(micro) & 0xFFu) << 16) |
               static_cast<std::int32_t>(0x80000000u);
    }
    return TranslateOutcome::kOk;
  }

  // `track length <idx> <steps>` -- mirrors Shell::cmd_track's own "length"
  // verb exactly (Op::kSet, matching abi.hpp's kTrackLength comment).
  if (t.size() == 4 && t[0] == "track" && t[1] == "length") {
    std::uint64_t idx = 0;
    if (!parse_uint(t[2], idx) || idx > 0xFFFF) {
      detail = "bad track index: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t steps = 0;
    if (!parse_uint(t[3], steps)) {
      detail = "bad length: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = Param::kTrackLength;
    out.idx = static_cast<std::uint16_t>(idx);
    out.a = static_cast<std::int32_t>(steps);
    return TranslateOutcome::kOk;
  }

  // `track mute|solo <idx> on|off` -- mirrors Shell::cmd_track's own
  // mute/solo verbs exactly (Op::kSet, matching abi.hpp's kTrackMute/
  // kTrackSolo comments).
  if (t.size() == 4 && t[0] == "track" && (t[1] == "mute" || t[1] == "solo")) {
    std::uint64_t idx = 0;
    if (!parse_uint(t[2], idx) || idx > 0xFFFF) {
      detail = "bad track index: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    if (t[3] != "on" && t[3] != "off") {
      detail = "track mute|solo <idx> on|off";
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = t[1] == "mute" ? Param::kTrackMute : Param::kTrackSolo;
    out.idx = static_cast<std::uint16_t>(idx);
    out.a = t[3] == "on" ? 1 : 0;
    return TranslateOutcome::kOk;
  }

  // Phase-5 Item #2 (docs/design/clip-primitive-design.md): `launch clip <id>
  // quantize <n>` / `launch scene <n> quantize <q>` -- grid_panel.cpp's own
  // send() shape.
  if (t.size() >= 3 && t[0] == "launch" && (t[1] == "clip" || t[1] == "scene")) {
    std::uint64_t target = 0;
    if (!parse_uint(t[2], target) || target > 0xFFFF) {
      detail = "bad id: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    Boundary boundary = Boundary::kImmediate;
    std::uint8_t n_bars = 1;
    if (!parse_quantize_suffix(t, 3, boundary, n_bars)) {
      detail = "usage: launch clip|scene <id> quantize <n>";
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = t[1] == "clip" ? Param::kClipLaunch : Param::kSceneQuantize;
    out.idx = static_cast<std::uint16_t>(target);
    out.boundary = boundary;
    out.n_bars = n_bars;
    return TranslateOutcome::kOk;
  }

  // `stop clip <id> [quantize <n>]`.
  if (t.size() >= 3 && t[0] == "stop" && t[1] == "clip") {
    std::uint64_t target = 0;
    if (!parse_uint(t[2], target) || target > 0xFFFF) {
      detail = "bad clip id: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    Boundary boundary = Boundary::kImmediate;
    std::uint8_t n_bars = 1;
    if (!parse_quantize_suffix(t, 3, boundary, n_bars)) {
      detail = "usage: stop clip <id> [quantize <n>]";
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kClipStop;
    out.idx = static_cast<std::uint16_t>(target);
    out.boundary = boundary;
    out.n_bars = n_bars;
    return TranslateOutcome::kOk;
  }

  // `note <port> on|off <midinote> [velocity]` -- docs/proposals/looper-in-
  // gui-contract.md §2/§7 items 1/2 (the on-screen-keyboard/pad note path):
  // the SAME grammar Shell::cmd_note implements as an L1 verb (shell_music_
  // commands.cpp), so a note sent from the GUI's own note-input surface (a
  // later slice) reaches the engine exactly like a hardware note. `port` here
  // is a BARE numeric index only (this translator's existing convention --
  // e.g. kDefaultStyleRoutes/kPrimaryAudioOutPort above -- never a named
  // alias like "in0"; run_engine()'s own default topology always opens input
  // 0 first, so `note 0 ...` addresses it). No ":ch" suffix (every existing
  // caller here also omits per-command channel overrides); the resulting
  // Param::kNoteRaw Command carries the SAME idx/a/b/c packing cmd_note
  // builds -- run_engine()'s Command-ring drain special-cases this Param and
  // feeds it straight through feed_midi(), never Engine::push_command()
  // (kNoteRaw has no case there, see that function's own comment).
  if (t.size() >= 4 && t.size() <= 5 && t[0] == "note") {
    std::uint64_t port = 0;
    if (!parse_uint(t[1], port) || port >= kMaxPorts) {
      detail = "bad note port: " + std::string(t[1]);
      return TranslateOutcome::kInvalidArgument;
    }
    if (t[2] != "on" && t[2] != "off") {
      detail = "note <port> on|off <midinote> [velocity]";
      return TranslateOutcome::kInvalidArgument;
    }
    const bool on = t[2] == "on";
    std::uint64_t note = 0;
    if (!parse_uint(t[3], note) || note > 127) {
      detail = "bad midi note: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    // Default velocity: 100 for note-on, 64 for note-off (matches cmd_note's
    // own kPianoReleaseVelocity default, shell_internal.hpp -- a private
    // hostrt constant this pure-client translator never reaches into, D38).
    std::uint64_t vel = on ? 100 : 64;
    if (t.size() == 5) {
      if (!parse_uint(t[4], vel) || vel < 1 || vel > 127) {
        detail = "bad velocity: " + std::string(t[4]);
        return TranslateOutcome::kInvalidArgument;
      }
    }
    out.param = Param::kNoteRaw;
    out.idx = static_cast<std::uint16_t>(port);
    out.a = static_cast<std::int32_t>(note);
    out.b = static_cast<std::int32_t>(vel);
    out.c = on ? 1 : 0;
    return TranslateOutcome::kOk;
  }

  // `loop new` / `loop record <slot> record|overdub|replace <port>` /
  // `loop stop <slot> [grid]` / `loop erase <slot>` / `loop undo <slot>` /
  // `loop length <slot> auto|fixed <ticks>|quantized <grid>` -- docs/
  // proposals/looper-in-gui-contract.md §7 item 4: the SAME `loop ...`
  // grammar shell_loop_commands.cpp implements, translated here so the
  // integrated GUI backend can drive it too. Unlike kNoteRaw above, every one
  // of these Params already has an Engine::push_command case (Engine::
  // cmd_loop, engine.cpp) -- no drain-loop special case is needed, they ride
  // the normal Command ring exactly like `clip add`/`launch` above. Ports are
  // BARE numeric indices, same convention as the `note` verb just above.
  if (t.size() == 2 && t[0] == "loop" && t[1] == "new") {
    // Always the legacy sequential-append form: no `id <n>` suffix here (that
    // lands with item 6's `clip add ... loop <slot> id <n>` grammar, a later
    // slice) -- mirrors shell_loop_commands.cpp's own `loop new` exactly.
    out.param = Param::kLoopNew;
    out.idx = kNoLoopExplicitId;
    return TranslateOutcome::kOk;
  }

  if (t.size() == 5 && t[0] == "loop" && t[1] == "record") {
    std::uint64_t slot = 0;
    if (!parse_uint(t[2], slot) || slot > 0xFFFF) {
      detail = "bad loop slot: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    LoopRecordMode mode{};
    if (t[3] == "record") {
      mode = LoopRecordMode::kRecord;
    } else if (t[3] == "overdub") {
      mode = LoopRecordMode::kOverdub;
    } else if (t[3] == "replace") {
      mode = LoopRecordMode::kReplace;
    } else {
      detail = "loop record mode: record|overdub|replace";
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t port = 0;
    if (!parse_uint(t[4], port) || port >= kMaxPorts) {
      detail = "bad loop record port: " + std::string(t[4]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kLoopRecordStart;
    out.idx = static_cast<std::uint16_t>(slot);
    out.a = static_cast<std::int32_t>(mode);
    out.b = static_cast<std::int32_t>(port);
    return TranslateOutcome::kOk;
  }

  if (t.size() >= 3 && t.size() <= 4 && t[0] == "loop" && t[1] == "stop") {
    std::uint64_t slot = 0;
    if (!parse_uint(t[2], slot) || slot > 0xFFFF) {
      detail = "bad loop slot: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    std::uint64_t grid = 0;
    if (t.size() == 4 && !parse_uint(t[3], grid)) {
      detail = "bad grid: " + std::string(t[3]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = Param::kLoopRecordStop;
    out.idx = static_cast<std::uint16_t>(slot);
    out.a = static_cast<std::int32_t>(grid);
    return TranslateOutcome::kOk;
  }

  if (t.size() == 3 && t[0] == "loop" && (t[1] == "erase" || t[1] == "undo")) {
    std::uint64_t slot = 0;
    if (!parse_uint(t[2], slot) || slot > 0xFFFF) {
      detail = "bad loop slot: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    out.param = t[1] == "erase" ? Param::kLoopErase : Param::kLoopUndo;
    out.idx = static_cast<std::uint16_t>(slot);
    return TranslateOutcome::kOk;
  }

  if (t.size() >= 4 && t.size() <= 5 && t[0] == "loop" && t[1] == "length") {
    std::uint64_t slot = 0;
    if (!parse_uint(t[2], slot) || slot > 0xFFFF) {
      detail = "bad loop slot: " + std::string(t[2]);
      return TranslateOutcome::kInvalidArgument;
    }
    LoopLengthMode mode{};
    std::uint64_t value = 0;
    if (t[3] == "auto") {
      mode = LoopLengthMode::kAuto;
    } else if (t[3] == "fixed" && t.size() == 5) {
      mode = LoopLengthMode::kFixed;
      if (!parse_uint(t[4], value)) {
        detail = "bad length (ticks): " + std::string(t[4]);
        return TranslateOutcome::kInvalidArgument;
      }
    } else if (t[3] == "quantized" && t.size() == 5) {
      mode = LoopLengthMode::kQuantized;
      if (!parse_uint(t[4], value)) {
        detail = "bad grid (ticks): " + std::string(t[4]);
        return TranslateOutcome::kInvalidArgument;
      }
    } else {
      detail = "usage: loop length <slot> auto|fixed <ticks>|quantized <grid>";
      return TranslateOutcome::kInvalidArgument;
    }
    out.op = Op::kSet;
    out.param = Param::kLoopLength;
    out.idx = static_cast<std::uint16_t>(slot);
    out.a = static_cast<std::int32_t>(mode);
    out.b = static_cast<std::int32_t>(value);
    return TranslateOutcome::kOk;
  }

  return TranslateOutcome::kUnknownCommand;
}

// Builds one `style route <role> <port>:<ch>` Command out of a
// DefaultStyleRoute entry, through the SAME encoding cmd_style()'s "route"
// verb uses (components/platform/hostrt/shell_music_commands.cpp): `b = port |
// (channel << 8)`, where `channel` is 0-based (split_port_channel converts
// the 1-based text form -- shell_parse.cpp -- before encoding). The role
// name always resolves: kDefaultStyleRoutes only ever holds the fixed,
// known-good names parse_role() accepts (mirrored 1:1 with setup.acmd).
Command make_default_style_route_command(const DefaultStyleRoute& route) {
  Command cmd{};
  cmd.op = Op::kSet;
  cmd.param = Param::kStyleRoute;
  TrackRole role = TrackRole::kLead;
  Shell::resolve_track_role(route.role_name, role);
  cmd.a = static_cast<std::int32_t>(role);
  const std::int32_t channel_zero_based = static_cast<std::int32_t>(route.channel_one_based) - 1;
  cmd.b = static_cast<std::int32_t>(kPrimaryAudioOutPort) | (channel_zero_based << 8);
  return cmd;
}

// docs/proposals/per-style-default-progressions.md: default per-style
// harmonic progression, HOST data (default_style_progressions.hpp) feeding
// the core's EXISTING ChordSequence object through the EXISTING
// kKeySet/kSeqNew/kSeqClear/kSeqAdd/kSeqLoop/kSeqPlay Command verbs -- zero
// new StyleDef field, zero ABI change, zero core touch. Mirrors
// make_default_style_route_command()'s own idiom exactly: a small constexpr
// table turned into raw Command PODs, injected at the SAME site
// (run_engine()'s command-drain loop, right after a successful
// kStyleLoad/kStyleSwitch) -- never routed through command_line_to_command(),
// since nothing in gui-sonotron today ever types a `key`/`seq` line (no
// free-text console exists, per the proposal's own §3.2 finding).
//
// Trap #2 (sequence-pool exhaustion, proposal §3.3.2): `kSeqNew` appends a
// NEW slot to the core's bounded 16-sequence pool every time it is called;
// re-issuing it on every style load/switch would exhaust the pool after 16
// switches in one session. Fix: `seq_already_created` lets the caller ask
// for `kSeqNew` only the FIRST time (run_engine() tracks this with a local
// bool, reset naturally on every fresh engine-thread start) and `kSeqClear`
// every time after -- always targeting pool slot 0 by construction (this
// builder emits raw Command PODs directly, so it never needs Shell's
// name-based `seq use <name>` resolution).
void append_default_progression_commands(const sonotron::DefaultProgression& prog,
                                         bool seq_already_created, std::uint32_t ticks_per_bar,
                                         std::vector<Command>& out) {
  Command key_cmd{};
  key_cmd.op = Op::kSet;
  key_cmd.param = Param::kKeySet;
  key_cmd.a = prog.key_root_pc;
  key_cmd.b = static_cast<std::int32_t>(prog.key_mode);
  out.push_back(key_cmd);

  Command seq_setup{};
  seq_setup.param = seq_already_created ? Param::kSeqClear : Param::kSeqNew;
  out.push_back(seq_setup);

  // Same velocity kSeqAdd's own CLI verb hardcodes (shell_music_commands.cpp's
  // seq_add(), `c.b = (quality + 1) | (100 << 8);`) -- kept identical so a
  // host-injected progression sounds exactly as loud as a hand-typed one.
  constexpr std::uint8_t kDefaultVelocity = 100;
  constexpr std::uint8_t kOctave4Base = 60;  // matches parse_note()'s no-octave default (C4=60)
  for (std::uint8_t i = 0; i < prog.step_count; ++i) {
    const sonotron::ProgressionStep& step = prog.steps[i];
    Command add_cmd{};
    add_cmd.param = Param::kSeqAdd;
    add_cmd.a = static_cast<std::int32_t>(kOctave4Base) + step.root_pc;
    add_cmd.b = (static_cast<std::int32_t>(step.quality_ovr) + 1) |
                (static_cast<std::int32_t>(kDefaultVelocity) << 8);
    add_cmd.c = static_cast<std::int32_t>(step.bars) * static_cast<std::int32_t>(ticks_per_bar);
    out.push_back(add_cmd);
  }

  Command loop_cmd{};
  loop_cmd.op = Op::kSet;
  loop_cmd.param = Param::kSeqLoop;
  loop_cmd.a = 1;
  out.push_back(loop_cmd);

  Command play_cmd{};
  play_cmd.param = Param::kSeqPlay;
  out.push_back(play_cmd);
}

// The MAIN progression always lives in pool slot 0, by construction: it is
// the FIRST ChordSequence ever created (the very first style load's own
// kSeqNew, append_default_progression_commands()'s "trap #2" comment). The
// cadence tag (Option B, docs/proposals/gui-live-harmony-musical-design.md
// S3.4) is created SECOND, the first time any Ending fires, and always
// occupies slot 1 thereafter -- see CadenceState's own header comment.
constexpr std::uint16_t kMainProgressionSlot = 0;
constexpr std::uint16_t kCadenceSlot = 1;

// Default-harmony-progression state, owned by run_engine()'s own stack frame
// (engine-thread-local, resets naturally on every fresh run_engine() call --
// a brand-new Shell/Engine also has a brand-new, empty ChordSequencer pool,
// see append_default_progression_commands()'s trap #2 comment).
struct DefaultProgressionState {
  // Set true the first time a kSeqNew actually goes out; never reset after
  // (trap #2: exactly one kSeqNew, ever, per session).
  bool seq_created = false;
  // Trap #3 (phase alignment, docs/proposals/per-style-default-progressions.
  // md §3.3.3): a single pending slot, last-write-wins by design (mirrors
  // Arranger's own m_pending_style/m_pending_valid pair) -- a second style
  // pick before the first one lands simply replaces what is pending, never
  // queues both.
  bool pending_valid = false;
  std::uint32_t pending_arm_bar_index = 0;
  std::vector<Command> pending_commands;
  // The kBuiltins index this progression is CURRENTLY keyed to -- set from
  // `cmd.a` every time handle_style_change_progression() runs (the ONLY
  // place that value is available). Option B's cadence-entry hook (below)
  // reads this to know which style's cadence tag to author, since it fires
  // later, off a kSection OutEvent, with no Command of its own to read a
  // style index from.
  std::int32_t active_style_index = 0;
};

// Option B cadence tags (docs/proposals/gui-live-harmony-musical-design.md
// S3.4, cadence_progressions.hpp): builds the SAME shape of Command batch
// append_default_progression_commands() does above, but targeting the
// SHARED cadence pool slot (kCadenceSlot) instead of the main progression's
// slot 0 -- kSeqUse/kSeqNew-or-Clear/kSeqTranspose/kSeqAdd*/kSeqLoop(false)/
// kSeqPlay, all through the existing verb surface, zero ABI change.
//
// Re-keying (kSeqTranspose): Engine::seq_add resolves each added chord's
// DEGREE against the CURRENT sequence's OWN captured `key` field
// (ChordSequence::key, set once at kSeqNew time from whatever the global
// key was then), never the engine's live global key -- so a shared slot
// reused across DIFFERENT styles must be explicitly re-keyed to the
// CURRENTLY active style before its content is rebuilt, or a later style's
// chords would resolve (or fail kNotInKey) against an earlier style's
// stale key/mode. kSeqTranspose (Engine::seq_transpose ->
// ChordSequence::transpose_to) is the existing verb for exactly this, and
// is harmless to issue even on the very first-ever creation (the sequence
// is empty at that point, nothing to re-derive).
void append_cadence_commands(const CadenceProgression& cadence, std::uint8_t key_root_pc,
                             arrangrr::Mode key_mode, bool seq_already_created,
                             std::uint32_t ticks_per_bar, std::vector<Command>& out) {
  Command use_cmd{};
  use_cmd.op = Op::kDo;
  use_cmd.param = Param::kSeqUse;
  use_cmd.idx = kCadenceSlot;
  if (seq_already_created) {
    out.push_back(use_cmd);
  }

  Command seq_setup{};
  seq_setup.param = seq_already_created ? Param::kSeqClear : Param::kSeqNew;
  out.push_back(seq_setup);

  Command transpose_cmd{};
  transpose_cmd.op = Op::kSet;
  transpose_cmd.param = Param::kSeqTranspose;
  transpose_cmd.a = static_cast<std::int32_t>(key_root_pc);
  transpose_cmd.b = static_cast<std::int32_t>(key_mode);
  out.push_back(transpose_cmd);

  constexpr std::uint8_t kDefaultVelocity = 100;  // matches append_default_progression_commands
  constexpr std::uint8_t kOctave4Base = 60;
  for (const sonotron::ProgressionStep& step : cadence.steps) {
    Command add_cmd{};
    add_cmd.param = Param::kSeqAdd;
    add_cmd.a = static_cast<std::int32_t>(kOctave4Base) + step.root_pc;
    add_cmd.b = (static_cast<std::int32_t>(step.quality_ovr) + 1) |
                (static_cast<std::int32_t>(kDefaultVelocity) << 8);
    add_cmd.c = static_cast<std::int32_t>(step.bars) * static_cast<std::int32_t>(ticks_per_bar);
    out.push_back(add_cmd);
  }

  Command loop_cmd{};
  loop_cmd.op = Op::kSet;
  loop_cmd.param = Param::kSeqLoop;
  loop_cmd.a = 0;  // holds on the resolved tonic, never wraps back to the approach chord
  out.push_back(loop_cmd);

  Command play_cmd{};
  play_cmd.param = Param::kSeqPlay;
  out.push_back(play_cmd);
}

// Option B state (docs/proposals/gui-live-harmony-musical-design.md S3.4),
// owned by run_engine()'s own stack frame, same discipline as
// DefaultProgressionState/RitardandoState above -- the sink lambda (below)
// can only RECORD what the OutEvent stream reported (kSection), since
// `shell` does not exist yet at that point in this file's constructor
// argument; run_engine()'s own outer loop consumes these flags and acts.
struct CadenceState {
  // Set true the first time the cadence pool slot is created (kSeqNew);
  // every subsequent Ending re-authors it with kSeqClear instead -- the
  // SAME "trap #2" pool-exhaustion guard DefaultProgressionState::
  // seq_created applies to the main progression, applied here to a SECOND,
  // SHARED slot reused across every style and every Ending -- never one
  // cadence slot per style.
  bool seq_created = false;
  // True from the moment the cadence sequence is swapped in (an Ending
  // entry) until the swap-back to the main progression actually fires.
  bool active = false;
  bool saw_ending_entry = false;
  SectionType which_ending = SectionType::kEnding1;
  // The OTHER trigger for "resume the main progression" (the task's own
  // "next fresh VarA/style load" wording): a live return to a non-Ending
  // section while the cadence slot is still selected -- mirrors
  // RitardandoState::saw_non_ending_section's own kSection observation, but
  // gated on `active` so an ordinary VarA<->VarB<->VarC<->VarD switch that
  // never touched Ending at all does NOT retrigger a swap-back (S3.2: the
  // chord track must stay untouched by such switches). A fresh style
  // load/switch is the OTHER trigger, handled inline inside
  // handle_style_change_progression()'s own leading kSeqUse -- see that
  // function's comment.
  bool saw_return_to_main = false;
};

// Record-only half of Option B's Ending-entry/swap-back detection (mirrors
// RitardandoState's own kSection observation in run_engine()'s sink lambda):
// called from that lambda for EVERY OutEvent, fully additive, never
// interferes with the ritardando block's own flags. `saw_return_to_main`
// only latches while a cadence is actually `active`, so an ordinary
// VarA<->VarB<->VarC<->VarD switch that never touched Ending at all does not
// spuriously trigger a swap-back (S3.2: such switches must leave the chord
// track untouched). Extracted into its own function (rather than left
// inline like ritardando's own block) purely to keep the sink lambda's
// contribution to run_engine()'s cognitive complexity under the project's
// clang-tidy threshold -- no behavior change.
void observe_cadence_section_event(const OutEvent& ev, CadenceState& cadence) {
  if (ev.kind != OutEvent::Kind::kSection) {
    return;
  }
  const auto section = static_cast<SectionType>(ev.code);
  if (section_is_ending(section)) {
    cadence.saw_ending_entry = true;
    cadence.which_ending = section;
  } else if (cadence.active) {
    cadence.saw_return_to_main = true;
  }
}

// Option B (Cadence, S3.4) step: consumes what the sink lambda (Shell's own
// ctor argument, run_engine() below) recorded onto `cadence` during the
// advance_by() call that just returned, and acts against `shell` -- this
// cannot happen inside the sink lambda itself, `shell` does not exist yet at
// that point in run_engine()'s own scope (CadenceState's own header
// comment). Extracted out of run_engine() (rather than left inline), same
// reasoning as drain_command_ring's own extraction above: keeps run_engine()'s
// own cognitive complexity under the project's clang-tidy threshold, no
// behavior change. Cancel/swap-back is handled BEFORE arming a fresh entry,
// same ordering as the ritardando block in run_engine() this mirrors, so a
// same-iteration leave-then-re-enter (were that ever possible) always lands
// on the freshly-armed state, never the stale one.
void step_cadence(Shell& shell, CadenceState& cadence,
                  const DefaultProgressionState& progression_state) {
  if (cadence.saw_return_to_main) {
    // The OTHER "resume the main progression" trigger (S3.2/S3.4): a live
    // return to a non-Ending section while the cadence was active, with no
    // intervening style load/switch -- handle_style_change_progression()'s
    // own leading kSeqUse handles that other trigger inline. Paired
    // kSeqUse+kSeqPlay (never a bare kSeqUse), exactly the idiom the spike
    // proved is required to avoid leaving the main slot's phase stale
    // (docs/proposals/gui-live-harmony-musical-design.md S3.4).
    Command use_main{};
    use_main.op = Op::kDo;
    use_main.param = Param::kSeqUse;
    use_main.idx = kMainProgressionSlot;
    shell.push_command(use_main);
    Command play_main{};
    play_main.param = Param::kSeqPlay;
    shell.push_command(play_main);
    cadence.active = false;
  }
  cadence.saw_return_to_main = false;

  if (cadence.saw_ending_entry) {
    const sonotron::DefaultProgression& main_prog =
        sonotron::default_progression_for(progression_state.active_style_index);
    const sonotron::CadenceProgression& tag =
        sonotron::cadence_progression_for(progression_state.active_style_index);
    std::vector<Command> cadence_cmds;
    cadence_cmds.reserve(sonotron::kCadenceStepCount + 4);
    append_cadence_commands(tag, main_prog.key_root_pc, main_prog.key_mode, cadence.seq_created,
                            static_cast<std::uint32_t>(shell.engine().transport().ticks_per_bar()),
                            cadence_cmds);
    for (const Command& cadence_cmd : cadence_cmds) {
      shell.push_command(cadence_cmd);
    }
    cadence.seq_created = true;
    cadence.active = true;
  }
  cadence.saw_ending_entry = false;
}

// Builds and dispatches the default progression for `cmd` right after a
// successful kStyleLoad/kStyleSwitch (the caller must have already called
// `shell.push_command(cmd)`). Handles trap #3: `style load` is
// unconditionally immediate in the core (Engine::cmd_style's kStyleLoad
// branch never defers), but `style switch` quantizes to the next bar WHILE
// PLAYING (Engine::style_switch, engine.cpp) -- and Engine::cmd_seq never
// reads Command::boundary for ANY seq verb, so firing the progression at
// ring-pop time on a LIVE switch would start the new harmony loop "now", up
// to a bar ahead of the style morph it is meant to accompany
// (browser_panel.cpp only ever sends `style switch` while fx.playing(), so
// this is the COMMON case for a switch, not an edge one). Fix, host-only, no
// core touch: hold the built commands and let
// release_pending_progression_if_due() below release them the moment
// Transport::bar_index() -- the SAME monotonic bar counter the core itself
// advances at every bar boundary (runtime/transport.hpp's own "shared
// primitive every bar-gated consumer should count against" recommendation)
// -- moves past the value observed here. This lands the progression on the
// first real bar boundary crossed after the request, the same downbeat the
// deferred style morph itself targets (Arranger::on_tick's own
// section-relative bar gate, which stays in phase with Transport's grid as
// long as the meter does not change mid-switch).
//
// Option B addition (docs/proposals/gui-live-harmony-musical-design.md
// S3.4): a fresh style load/switch is one of the two "resume the main
// progression" triggers (the other, a live return to a non-Ending section,
// is handled in run_engine()'s own outer loop off `cadence.saw_return_to_
// main`). An Ending entry may have left the pool's CURRENT slot pointed at
// the shared cadence slot (kCadenceSlot); append_default_progression_
// commands() always builds its kSeqClear-or-kSeqNew/kSeqAdd*/kSeqPlay
// batch against WHATEVER slot is current at push time, so this function
// must force it back to kMainProgressionSlot FIRST -- guarded on `state.
// seq_created` (the main slot has ever been created at all) so the very
// first-ever style load, before pool slot 0 exists, never issues a kSeqUse
// against an empty pool (which would warn). `cadence.active` also resets
// here: whichever trigger fires first (this one, or the non-Ending-section
// one below) is the one that actually resumes the main progression.
void handle_style_change_progression(Shell& shell, const Command& cmd,
                                     DefaultProgressionState& state, CadenceState& cadence) {
  state.active_style_index = cmd.a;
  if (state.seq_created) {
    Command use_main{};
    use_main.op = Op::kDo;
    use_main.param = Param::kSeqUse;
    use_main.idx = kMainProgressionSlot;
    shell.push_command(use_main);
  }
  cadence.active = false;

  std::vector<Command> progression_cmds;
  progression_cmds.reserve(sonotron::kMaxProgressionSteps + 4);
  append_default_progression_commands(
      sonotron::default_progression_for(cmd.a), state.seq_created,
      static_cast<std::uint32_t>(shell.engine().transport().ticks_per_bar()), progression_cmds);
  state.seq_created = true;

  const bool immediate = cmd.param == Param::kStyleLoad || cmd.boundary == Boundary::kImmediate ||
                         !shell.engine().transport().playing();
  if (immediate) {
    for (const Command& progression_cmd : progression_cmds) {
      shell.push_command(progression_cmd);
    }
    state.pending_valid = false;
    return;
  }
  state.pending_commands = std::move(progression_cmds);
  state.pending_arm_bar_index = shell.engine().transport().bar_index();
  state.pending_valid = true;
}

// Releases a pending (deferred) progression the first time the transport's
// own bar counter moves past the value observed at arm time -- i.e. the
// first real bar boundary crossed since the live `style switch` that armed
// it, matching the deferred style morph's own landing point. `!=` rather
// than `>` so a transport stop/restart in between (which resets bar_index()
// to 0) still eventually releases the pending progression instead of
// leaving it stuck forever.
// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md): builds
// and plays a SceneChain from `build`'s populated scene list. Runs entirely
// on the engine thread -- the only place `Engine&` (and therefore
// `Engine::performances()`, `capture_performance()` being private) is
// reachable. Captures the CURRENT live rig into PerformanceStore slot 0 via
// the EXISTING `kPerformanceStore` verb (Engine::perf_store's own synchronous
// capture, engine.cpp), reads it back as `base`, then for each scene clones
// `base` with ONLY `variation` (the scene's own SectionType) overridden --
// Phase 1's own "scenes differ only by section" scope (design decision 2) --
// forcing `chord_sequence_id = 0xFFFF` so `apply_performance` never restarts
// the harmony loop on a scene transition (design decision 1). Rebuilds the
// whole chain (`kSceneClear` + one or more `kSceneAdd` per scene, `n_bars`
// from the per-scene length stepper, `beats_per_bar` from the captured base
// so Phase 1 never forces an implicit meter reset) and starts it
// (`kScenePlay`) -- exactly the ABI sequence the observable contract
// requires, and no more.
//
// Task #5 (per-section REPEAT COUNT) + repeat-count Phase-2 (core primitive,
// arrangrr/scene/scene_chain.hpp -- SceneStep::repeat_count, SceneChain::
// on_bar's own lap-cycling, Engine::scene_add reading `cmd.idx`): each
// scene's own `repeat` (SongBuildScene, above) decides how many bars the
// scene holds before the chain advances, but the expansion now happens
// INSIDE the core, not here. This loop emits exactly ONE `kSceneAdd` per
// populated scene column, translating `repeat` into `cmd.idx`, which
// Engine::scene_add reads straight into the pushed SceneStep's own
// `repeat_count` -- SceneChain::on_bar then holds that single step for
// `n_bars * repeat_count` bars, firing `kSceneLap` OutEvents (not a real
// transition/TransitionFn re-fire) for every intermediate lap and a genuine
// transition only on the final one. This SUPERSEDES the earlier host-expand
// mechanism, which used to push `repeat` CONSECUTIVE, identical `kSceneAdd`
// steps (one core SceneStep per repeat, each referencing the same
// PerformanceStore slot `i`) to reach the same total hold length -- an
// up-to-8x step-count blowup this file no longer needs now that the core
// itself understands "hold this step K times."
//
// The wire's finite range for `repeat`, [1,255], IS Engine::scene_add's own
// clamp range for `cmd.idx` (`cmd.idx` is `std::uint16_t`, so the
// `std::uint8_t repeat` widens without loss) -- no translation math needed,
// a finite repeat is passed straight through. kSongBuildRepeatInfinite (0,
// this file's own sentinel, above) is NOT the same numeric value as the
// core's own infinite sentinel, arrangrr::kSceneRepeatInfinite (255,
// scene_chain.hpp) -- deliberately kept distinct, same reason as always
// (D38: this file has no dependency on grid_model.hpp/arrangrr's own
// repeat-sentinel constants) -- so this loop translates explicitly rather
// than passing `repeat` through unchanged: Engine::scene_add treats
// `cmd.idx == 0` as "repeat_count = 1", NOT infinite, so
// kSongBuildRepeatInfinite must never reach `cmd.idx` verbatim.
//
// An infinite scene (kSongBuildRepeatInfinite) still needs only ONE step,
// same as before, but for a slightly different underlying reason now:
// SceneChain::on_bar never completes the final lap of a step whose
// repeat_count is arrangrr::kSceneRepeatInfinite (its own "hold forever"
// semantic), so that step is never left, and this loop BREAKS immediately
// after emitting it, deliberately never building the remaining scenes at
// all (they are simply unreachable until a fresh song-build call replaces
// the whole chain -- e.g. a manual scene-header click, activate_scene_column
// in grid_panel.cpp, which is exactly the "explicit user gesture to resume"
// the design calls for).
//
// Budget guard: `kMaxScenes` (arrangrr/config.hpp, 64) is the core
// SceneChain's own hard step-count ceiling (a fixed-capacity StaticVector).
// Now that every scene costs exactly ONE step regardless of its own repeat
// count, `kMaxSongScenes`(8) populated columns can never come remotely close
// to that ceiling on its own -- the "8 populated columns x 8 repeat == kMax
// Scenes" arithmetic this comment used to cite no longer applies to THIS
// function's own math (that invariant is now purely a CORE-side capacity
// fact, still hardened by the cross-file static_asserts in config.hpp --
// those stay valid as documentation of the core's own ceiling relationship,
// they are just not exercised by anything this loop computes anymore).
// `steps_emitted` and this guard stay in place as pure defense-in-depth
// against a non-GUI-originated wire command (a raw `song build` line typed
// by hand, or sent by a future non-GUI client, is not bound to respect
// kMaxSongScenes) -- the guard stays real, not decorative, exactly as
// before, it is just no longer a load-bearing capacity fact of this
// function's own arithmetic.
//
// Slot 0 doubles as BOTH the scratch capture target and scene 0's own step
// Performance: `base` is copied out locally before scene 0's own store()
// overwrites slot 0, so this never races itself, and reusing slot 0 (rather
// than a fresh scratch slot past the scene count) keeps every
// PerformanceStore::store() call here trivially sequential (0, 1, 2, ...),
// satisfying its "no gaps" contract regardless of how many scenes a PREVIOUS
// song-build call left the pool sized to.
//
// Returns true iff the chain just built ends on an infinite (truncated)
// scene -- the caller (run_engine's own Ending-cue watcher, below) needs
// this: SceneChain::playing() flips true->false BOTH when a normal song
// genuinely reaches its last populated column's own hold-completion (the
// real "cue the Ending" signal, design decision 3) AND when THIS build's own
// infinite scene reaches the end of its (deliberately truncated,
// one-step-long) chain -- the core has no way to tell those two cases apart
// from playing() alone, since both are "the chain has no more steps to
// advance into". An infinite hold must NEVER cue an Ending or stop the
// transport; the caller uses this return value to suppress that cue
// specifically for the infinite case, without touching the genuine "song
// really ended" path at all.
bool apply_song_build(Shell& shell, const SongBuildCommand& build) {
  if (build.scene_count == 0) {
    return false;
  }
  Command capture_cmd{};
  capture_cmd.param = Param::kPerformanceStore;
  capture_cmd.idx = 0;
  shell.push_command(capture_cmd);
  const Performance* captured = shell.engine().performances().get(0);
  if (captured == nullptr) {
    return false;  // defensive: PerformanceStore::store() at slot 0 can never fail.
  }
  const Performance base = *captured;

  Command clear_cmd{};
  clear_cmd.param = Param::kSceneClear;
  shell.push_command(clear_cmd);

  std::size_t steps_emitted = 0;
  bool ends_infinite = false;
  for (std::uint8_t i = 0; i < build.scene_count && steps_emitted < kMaxScenes; ++i) {
    Performance perf = base;
    perf.variation = static_cast<std::uint8_t>(build.scenes[i].section);
    perf.chord_sequence_id = 0xFFFF;
    // Song-mode Phase 2: layer this scene's own style/groove/key/tempo
    // overrides onto the base capture, exactly like `variation` right above
    // -- an unset override (this struct's own sentinel/flag) leaves the
    // base-captured field untouched, so a scene with no Phase-2 overrides at
    // all behaves byte-identically to Phase 1's own `perf = base` copy.
    const SongBuildScene& scene = build.scenes[i];
    if (scene.style_id != 0xFFFF) {
      perf.style_id = scene.style_id;
    }
    if (scene.has_groove_override) {
      perf.groove.swing = scene.groove[0];
      perf.groove.humanize_timing = scene.groove[1];
      perf.groove.humanize_velocity = scene.groove[2];
      perf.groove.accent = scene.groove[3];
      perf.groove.swing_grid = scene.groove[4];
      perf.groove.quantize = scene.groove[5];
    }
    if (scene.has_key_override) {
      perf.key_root = scene.key_root;
      perf.key_mode = scene.key_mode;
    }
    if (scene.tempo_x100 != 0) {
      perf.tempo_x100 = scene.tempo_x100;
    }
    if (!shell.engine().performances().store(i, perf)) {
      break;  // pool exhausted -- play the steps already built rather than none.
    }
    const std::uint8_t repeat = build.scenes[i].repeat;
    const bool infinite = repeat == kSongBuildRepeatInfinite;
    // Exactly one kSceneAdd per scene now (see this function's own header
    // comment): cmd.idx carries the repeat count straight into the core's
    // own SceneStep::repeat_count via Engine::scene_add, with an explicit
    // sentinel translation for the infinite case (kSongBuildRepeatInfinite
    // is 0, arrangrr::kSceneRepeatInfinite is 255 -- passing 0 through
    // verbatim would mean "repeat_count = 1" to Engine::scene_add, not
    // infinite).
    Command add_cmd{};
    add_cmd.param = Param::kSceneAdd;
    add_cmd.a = static_cast<std::int32_t>(i);
    add_cmd.b = static_cast<std::int32_t>(build.scenes[i].n_bars) |
                (static_cast<std::int32_t>(base.beats_per_bar) << 8);
    add_cmd.c = static_cast<std::int32_t>(SceneTransitionKind::kCut);
    add_cmd.idx = infinite ? kSceneRepeatInfinite : repeat;
    shell.push_command(add_cmd);
    ++steps_emitted;
    if (infinite) {
      ends_infinite = true;
      break;  // truncate the rest of the built chain -- this scene holds forever.
    }
  }

  Command play_cmd{};
  play_cmd.param = Param::kScenePlay;
  shell.push_command(play_cmd);
  return ends_infinite;
}

// Drains the Command ring: never silently dropped by the PRODUCER side (see
// InProcessBrainSession::send()) -- the engine just applies whatever is
// queued, in FIFO order, through the exact same Engine::push_command every
// exec_line handler already calls. ONE special case: Param::kNoteRaw (docs/
// proposals/looper-in-gui-contract.md §2/§7 items 1/2) has no case in Engine::
// push_command's own dispatch switch (it would land on the unhandled-param
// default and emit kUnknownCommand, see abi.hpp's own kNoteRaw comment) -- it
// is decoded to raw MIDI bytes and fed through the SAME feed_midi() entry
// point run_engine()'s own ALSA drain uses, exactly like Shell::cmd_note's
// own L1 handling (shell_music_commands.cpp). Extracted out of run_engine()
// (rather than left inline) purely to keep that function's own cognitive
// complexity under the project's clang-tidy threshold -- no behavior change.
void drain_command_ring(Shell& shell, SpscRing<Command, kCommandRingCapacity>& command_ring,
                        DefaultProgressionState& progression_state, CadenceState& cadence) {
  Command cmd;
  while (command_ring.try_pop(cmd)) {
    if (cmd.param == Param::kNoteRaw) {
      std::uint8_t bytes[3];
      Shell::note_raw_to_bytes(cmd, bytes);
      shell.feed_midi(static_cast<std::uint8_t>(cmd.idx & 0xFF),
                      Span<const std::uint8_t>(bytes, sizeof(bytes)));
      continue;
    }
    shell.push_command(cmd);
    if (cmd.param == Param::kStyleLoad || cmd.param == Param::kStyleSwitch) {
      handle_style_change_progression(shell, cmd, progression_state, cadence);
    }
  }
}

void release_pending_progression_if_due(Shell& shell, DefaultProgressionState& state) {
  if (!state.pending_valid ||
      shell.engine().transport().bar_index() == state.pending_arm_bar_index) {
    return;
  }
  for (const Command& progression_cmd : state.pending_commands) {
    shell.push_command(progression_cmd);
  }
  state.pending_valid = false;
}

std::uint64_t monotonic_us() {
  const auto epoch = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
}

// Engine-thread-local ritardando state (Arm A, "HOST-NUDGE" -- docs/
// proposals/ritardando-tempo-curve-fork.md, owner-picked over the core-
// primitive Arm B): rides nothing but the EXISTING tempo write path
// (Param::kTransportTempo via Shell::push_command) and the EXISTING
// OutEvent::kSection/kBeat events this file already decodes -- zero ABI
// change, zero core change, zero firmware impact (this whole file is
// host-only, never built for arm-none-eabi, see this app's own
// CMakeLists.txt comment). The pure ramp-curve math (ritardando::bpm_at) is
// hoisted into ritardando_ramp.hpp so it stays independently unit-testable;
// this struct is the stateful wiring, owned by run_engine()'s own
// stack frame -- same "resets naturally on every fresh run_engine() call"
// discipline as DefaultProgressionState above. Armed the instant an Ending
// kSection fires (arrangrr::section_is_ending(), true for kEnding1/kEnding2)
// and stepped once per outer-loop iteration off however many OutEvent::kBeat
// pulses landed during that iteration's shell.advance_by() call.
struct RitardandoState {
  bool active = false;
  // The OutEvent sink (Shell's own ctor argument, which runs BEFORE `shell`
  // itself exists in run_engine()'s scope) can only record WHAT happened;
  // arming or stepping the ramp needs `shell` (to read the current style/bpm
  // and to push the next tempo command), so run_engine()'s outer loop
  // consumes and clears these two flags once per iteration, right after
  // shell.advance_by() returns -- mirroring the existing
  // scene_chain_was_playing idiom just below it, for exactly the same
  // reason.
  bool saw_ending_entry = false;
  bool saw_non_ending_section = false;
  // Which Ending fired (kEnding1 or kEnding2 -- section_is_ending() accepts
  // both) -- recorded so run_engine() looks up the RIGHT StyleSection's own
  // `bars` for the ramp length, not a guessed default.
  SectionType which_ending = SectionType::kEnding1;
  std::uint32_t elapsed_pulses = 0;
  std::uint32_t total_pulses = 0;
  BpmX100 start_bpm = 0;
  BpmX100 target_bpm = 0;
  // The bpm THIS ramp last wrote -- lets run_engine() tell "nothing else
  // touched tempo since my last step" apart from "a manual bpm/transport
  // command landed out from under me", which cancels the ramp outright (the
  // task's own "simplest safe behavior", no resume/queue semantic). 0 is a
  // safe "nothing written yet" sentinel: BpmX100 0 is never a valid tempo
  // (kMinBpm is 2000, 20.00 bpm).
  BpmX100 last_written_bpm = 0;
};

}  // namespace

struct InProcessBrainSession::Impl {
  using Status = BrainSession::Status;

  SpscRing<Command, kCommandRingCapacity> command_ring;
  SpscRing<OutEvent, kOutEventRingCapacity> out_event_ring;
  SpscRing<PathCommand, kPathCommandRingCapacity> path_command_ring;
  SpscRing<PathResult, kPathResultRingCapacity> path_result_ring;
  // Song-mode Phase 1: see SongBuildCommand's own header comment.
  SpscRing<SongBuildCommand, kSongBuildRingCapacity> song_build_ring;
  // Phase-6 Theme 2 (Decision 1/2/3): sonotron::audio::AudioBackend's
  // producer-side ring handle, set (or left null) by set_audio_ring()
  // BEFORE start() -- see that method's own doc comment for the
  // synchronization argument. Never touched after run_engine() reads it
  // once at thread-start.
  AudioMidiRing* audio_ring = nullptr;
  // Roadmap 14110: default hooks reproduce today's real behavior byte-for-
  // byte (monotonic_us()/the loop's own 500us pacing sleep, both below) --
  // set_clock_hooks_for_test() overrides one or both BEFORE start(); see
  // that method's own doc comment (in_process_brain_session.hpp) for the
  // synchronization argument. Never touched after run_engine() starts
  // reading them.
  std::function<std::uint64_t()> now_us_hook = monotonic_us;
  std::function<void()> wait_hook = [] {
    std::this_thread::sleep_for(std::chrono::microseconds(500));
  };
  std::atomic<bool> running{false};
  std::atomic<bool> prefer_flats{false};
  std::atomic<Status> status{Status::kDisconnected};
  std::thread engine_thread;
  BrainSnapshot snapshot;
  // GUI-thread-only (never touched by the engine thread): the Command ring's
  // never-silently-drop policy (Corelli §15.2 correction #3) and the
  // "command not yet translated" note both surface here as a synthetic
  // BrainEvent the next poll() returns. Pushing them onto out_event_ring
  // instead would violate that ring's single-producer contract (the engine
  // thread is its only producer).
  std::vector<BrainEvent> local_warnings;

  void run_engine();
};

// Ownership discipline (Corelli §15.6): `shell` (and the Runtime/Stage it
// owns) is a local variable of THIS function, on the engine thread's own
// stack -- never stored in `Impl`, never returned, never reachable from the
// GUI thread. The GUI thread only ever touches `command_ring`/`out_event_ring`
// (both safe to share, that is the whole point of an SPSC ring) and the
// plain atomics above.
void InProcessBrainSession::Impl::run_engine() {
  const std::unique_ptr<IMidiHal> midi = make_midi_hal();
  std::string alsa_error;
  const bool alsa_ok = midi->open(kAlsaClientName, alsa_error);
  if (!alsa_ok) {
    std::fprintf(stderr,
                 "sonotron: integrated engine: ALSA unavailable (%s) -- running without sound\n",
                 alsa_error.c_str());
  }

  // Snapshot once, before the engine loop starts (GUI thread already set it,
  // if at all, before start() -- see set_audio_ring()'s own doc comment for
  // why no atomic is needed here).
  AudioMidiRing* const audio_out_ring = audio_ring;

  // Ritardando (Arm A): see RitardandoState's own header comment. Declared
  // before `shell` because the sink lambda below (Shell's own ctor argument)
  // captures it by reference.
  RitardandoState ritardando;

  // Option B (docs/proposals/gui-live-harmony-musical-design.md S3.4): see
  // CadenceState's own header comment. Declared alongside `ritardando` for
  // the same reason -- the sink lambda below captures it by reference too.
  CadenceState cadence;

  Shell shell([this, &midi, alsa_ok, audio_out_ring, &ritardando, &cadence](const OutEvent& ev) {
    if (alsa_ok && ev.kind == OutEvent::Kind::kMidi) {
      midi->send(ev.port, ev.msg);
    }
    // Ritardando (Arm A): record WHAT happened; run_engine()'s own outer
    // loop (below) does the actual arm/step/push against `shell`, which does
    // not exist yet at this point in this constructor's own lambda argument.
    if (ev.kind == OutEvent::Kind::kSection) {
      const auto section = static_cast<SectionType>(ev.code);
      if (section_is_ending(section)) {
        ritardando.saw_ending_entry = true;
        ritardando.which_ending = section;
      } else {
        ritardando.saw_non_ending_section = true;
      }
    } else if (ev.kind == OutEvent::Kind::kBeat && ritardando.active) {
      ++ritardando.elapsed_pulses;
    }
    // Option B (Cadence, S3.4): same record-only split as ritardando just
    // above -- fully additive, reads the SAME kSection events, never
    // interferes with the ritardando block's own flags. Extracted into its
    // own function (rather than left inline like ritardando's own block) to
    // keep this constructor lambda's own contribution to run_engine()'s
    // cognitive complexity under the project's clang-tidy threshold -- no
    // behavior change.
    observe_cadence_section_event(ev, cadence);
    // Phase-6 Theme 2 (Decision 1/3/4): realize ONLY the primary integrated
    // output port through the ISoundEngine wired behind AudioBackend.
    // audio_out_ring is null in --control mode and whenever no AudioBackend
    // was attached;
    // try_push is best-effort (MAY drop under backpressure, same asymmetric
    // policy as out_event_ring below) -- this is a felt-latency interactive
    // path, never a stall point for the engine thread.
    if (audio_out_ring != nullptr && ev.kind == OutEvent::Kind::kMidi &&
        ev.port == kPrimaryAudioOutPort) {
      (void)audio_out_ring->try_push(AudioMidiEvent{.port = ev.port, .msg = ev.msg});
    }
    // engine -> GUI: best-effort, MAY drop under backpressure (asymmetric
    // policy, matches the existing UDS broadcast precedent for a slow
    // client -- gui-contract-map.md's "best-effort; a slow client drops
    // events rather than stalling MIDI").
    (void)out_event_ring.try_push(ev);
  });

  // Default topology mirrors sonotron-server's own live launch: one in, one
  // out, wired thru, so a bare integrated launch is immediately playable.
  if (alsa_ok) {
    shell.set_port_hook([&midi](const PortDef& def) {
      std::string port_error;
      if (!midi->create_port(def, port_error)) {
        std::fprintf(stderr, "sonotron: integrated engine: %s\n", port_error.c_str());
      }
    });
    std::string ignored;
    shell.exec_line("port open in in0", ignored);
    shell.exec_line("port open out out0", ignored);
    shell.exec_line("thru in0 out0", ignored);
  }

  prefer_flats.store(shell.prefer_flats(), std::memory_order_relaxed);
  status.store(Status::kConnected, std::memory_order_release);

  TickAccumulator acc;
  std::uint64_t last_us = now_us_hook();

  // Default harmonic progression (docs/proposals/per-style-default-
  // progressions.md): re-applied on EVERY successful style load/switch,
  // mirroring kDefaultStyleRoutes's own "always re-apply" idiom -- see
  // handle_style_change_progression()'s own comment for trap #3 (phase
  // alignment). This DOES reverse Engine::cmd_style's own "load keeps the
  // HARMONY" discipline (engine.cpp's kStyleLoad comment: establish_default
  // only seeds a key when nothing explicit is in force) -- but that guard is
  // about not clobbering a LIVE chord a future detect/pad panel might steer;
  // today nothing in gui-sonotron ever plays a live chord (the proposal's
  // own §3.3.1 finding), so there is nothing genuine yet to "keep", and this
  // progression's own kKeySet/kSeqAdd calls ARE this style's explicit chord
  // going forward. The owner-approved trade-off (proposal §4 item 1, option
  // (a)) has an explicit expiry: once a live chord/detect/pad path ships,
  // re-applying unconditionally here would silently clobber a user-steered
  // chord and this gate must learn to check ChordEngine::explicit_set()
  // (unreachable from the GUI thread today without a new engine-thread-side
  // readback -- a real, if small, core/ABI change requiring its own
  // sign-off, not done here).
  DefaultProgressionState progression_state;

  // Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md,
  // design decision 3): the SceneChain HOLDS its last step rather than
  // looping (scene_chain.hpp's own on_bar), so `playing()` flips true->false
  // exactly once, the instant the chain's own final bar-hold elapses. This
  // local, engine-thread-only bool is the edge detector; a fresh run_engine()
  // (a brand-new session) naturally starts it false, matching a chain that
  // has never played.
  bool scene_chain_was_playing = false;

  // Task #5 Phase-1 (per-section repeat count, host-only): an infinite-hold
  // scene's own built chain is DELIBERATELY truncated right after its single
  // step (see apply_song_build's own header comment), which makes
  // SceneChain::playing() flip true->false the instant that step's hold
  // completes -- structurally identical, from the core's point of view, to a
  // normal song genuinely reaching the end of its last populated column.
  // This flag, set from apply_song_build's own return value each time a NEW
  // song is built, tells the Ending-cue watcher below which case it is
  // looking at, so an infinite hold is never mistaken for "the song
  // finished" and never cues an Ending or stops the transport.
  bool active_song_ends_infinite = false;

  while (running.load(std::memory_order_acquire)) {
    drain_command_ring(shell, command_ring, progression_state, cadence);
    release_pending_progression_if_due(shell, progression_state);

    SongBuildCommand song_build;
    while (song_build_ring.try_pop(song_build)) {
      active_song_ends_infinite = apply_song_build(shell, song_build);
    }

    // Hardware MIDI-in (docs/proposals/looper-in-gui-contract.md §7 item 3):
    // drains ALSA input and feeds it through the SAME feed_midi() entry point
    // the kNoteRaw special case above uses -- mirrors sonotron-server's own
    // run_server loop exactly (apps/sonotron-server/main.cpp:324-326).
    // drain_input() is itself a safe no-op when the backend never opened
    // (see e.g. AlsaMidi's own m_seq == nullptr guard, alsa_midi.cpp), so
    // this needs no extra alsa_ok gate; before this fix, physical MIDI
    // hardware plugged into gui-sonotron's `in0` ALSA port produced
    // literally zero effect (the port was opened, a route was staged, and no
    // byte was ever read off the wire).
    midi->drain_input([&shell](std::uint8_t port, const std::uint8_t* bytes, std::size_t len) {
      shell.feed_midi(port, Span<const std::uint8_t>(bytes, len));
    });

    // Drain the path-command ring the same way: apply every queued
    // `midi-source load <path>` straight to the engine-owned Shell (through
    // the exact same load_midi_source() --control's cmd_midi_source calls),
    // and on failure hand the error string back through path_result_ring --
    // best-effort, matching the OutEvent ring's own "a slow client drops
    // events rather than stalling MIDI" policy (this is a rare user action,
    // not a hot path, so dropping should never actually happen in practice).
    PathCommand path_cmd;
    while (path_command_ring.try_pop(path_cmd)) {
      const std::string path(path_cmd.bytes.data(), path_cmd.length);
      std::string error;
      if (!shell.load_midi_source(path, error)) {
        PathResult result;
        const std::size_t n = std::min(error.size(), kMaxPathErrorBytes - 1);
        std::copy_n(error.begin(), n, result.error_bytes.begin());
        result.error_length = static_cast<std::uint16_t>(n);
        (void)path_result_ring.try_push(result);
      }
    }

    const std::uint64_t now_us = now_us_hook();
    acc.set_bpm(shell.engine().transport().bpm());
    const std::uint32_t ticks = acc.advance_us(now_us - last_us);
    last_us = now_us;
    if (ticks > 0) {
      std::string tick_error;
      shell.advance_by(ticks, tick_error);
    }

    // Ritardando (Arm A) step: consume what the OutEvent sink observed during
    // the advance_by() call just above, then act on `shell` (arm on Ending
    // entry, cancel on leaving the Ending section or on an external tempo
    // write, otherwise push this iteration's ramp bpm) -- this cannot happen
    // inside the sink itself, see RitardandoState's own header comment.
    if (ritardando.saw_non_ending_section) {
      ritardando.active = false;
    }
    ritardando.saw_non_ending_section = false;

    if (ritardando.saw_ending_entry) {
      const Style* const style = shell.engine().arranger().current_style();
      const StyleSection* const ending_section =
          style == nullptr ? nullptr : style->find(ritardando.which_ending);
      const std::uint32_t bars = ending_section == nullptr ? 1 : ending_section->bars;
      const std::uint32_t beats_per_bar = style == nullptr ? kBeatsPerBar : style->beats_per_bar;
      constexpr std::uint32_t kPulsesPerBeat = kPpqn / kMidiClockDivider;  // 24, MIDI-clock rate
      ritardando.start_bpm = shell.engine().transport().bpm();
      ritardando.target_bpm =
          std::max(kMinBpm, static_cast<BpmX100>(static_cast<float>(ritardando.start_bpm) *
                                                 ritardando::kTargetBpmRatio));
      ritardando.total_pulses = bars * beats_per_bar * kPulsesPerBeat;
      ritardando.elapsed_pulses = 0;
      // A fresh arm must not be mistaken for "a manual write raced my last
      // step" the first time this ramp pushes below -- 0 is the "nothing
      // written yet" sentinel (RitardandoState's own comment).
      ritardando.last_written_bpm = 0;
      ritardando.active = true;
    }
    ritardando.saw_ending_entry = false;

    if (ritardando.active) {
      const BpmX100 live_bpm = shell.engine().transport().bpm();
      if (ritardando.last_written_bpm != 0 && live_bpm != ritardando.last_written_bpm) {
        // A manual `bpm`/`transport tempo` command changed tempo since our
        // own last step -- cancel outright rather than fight it (the
        // simplest safe behavior; no resume/queue semantic, per the task).
        ritardando.active = false;
      } else {
        const BpmX100 next_bpm =
            ritardando::bpm_at(ritardando.start_bpm, ritardando.target_bpm,
                               ritardando.elapsed_pulses, ritardando.total_pulses);
        Command tempo_cmd;
        tempo_cmd.op = Op::kSet;
        tempo_cmd.param = Param::kTransportTempo;
        tempo_cmd.a = static_cast<std::int32_t>(next_bpm);
        shell.push_command(tempo_cmd);
        ritardando.last_written_bpm = shell.engine().transport().bpm();
        if (ritardando.elapsed_pulses >= ritardando.total_pulses) {
          ritardando.active = false;  // ramp complete, holding at target_bpm
        }
      }
    }

    // Option B (Cadence, S3.4) step: consume what the sink observed during
    // the advance_by() call just above -- extracted into its own function,
    // same "keep run_engine()'s own cognitive complexity under the
    // project's clang-tidy threshold" reasoning as drain_command_ring's own
    // extraction above, no behavior change.
    step_cadence(shell, cadence, progression_state);

    // Song-mode Phase 1, design decision 3: cue the Ending the instant the
    // SceneChain's own last step finishes holding -- the bare, bar-quantized
    // `style section ending1` verb alone (never paired with a scene/clip
    // launch), the SAME cue the dedicated ENDING transport pad already sends
    // (transport_panel.cpp), preserving the "last column plays out to an
    // ending, transport stops" behavior without any clip-arm latch (the old
    // race this replaces could only happen because ClipMatrix clip arms were
    // still in flight; SceneChain never touches ClipMatrix at all).
    //
    // Task #5 Phase-1 addition: suppress this cue when the currently-built
    // song ends on an infinite scene (active_song_ends_infinite) -- see the
    // flag's own declaration above for why the two cases are otherwise
    // indistinguishable from playing() alone.
    const bool scene_chain_playing_now = shell.engine().scenes().playing();
    if (scene_chain_was_playing && !scene_chain_playing_now && !active_song_ends_infinite) {
      Command ending_cmd{};
      ending_cmd.param = Param::kStyleSection;
      ending_cmd.a = static_cast<std::int32_t>(SectionType::kEnding1);
      shell.push_command(ending_cmd);
    }
    scene_chain_was_playing = scene_chain_playing_now;

    prefer_flats.store(shell.prefer_flats(), std::memory_order_relaxed);

    // Short sleep, not a busy spin: there is no fd to block on here (unlike
    // sonotron-server's timerfd + poll()), so this is the clock's wakeup
    // cadence -- 0.5 ms, same interval sonotron-server's timerfd uses.
    // (Roadmap 14110: real sleep_for is the DEFAULT `wait_hook` value above;
    // a test may swap it for a cheap yield so it never waits real time.)
    wait_hook();
  }
}

InProcessBrainSession::InProcessBrainSession() : m_impl(std::make_unique<Impl>()) {}

InProcessBrainSession::~InProcessBrainSession() { stop(); }

bool InProcessBrainSession::start() {
  if (m_impl->running.load(std::memory_order_acquire)) {
    return true;  // already running
  }
  m_impl->status.store(Status::kConnecting, std::memory_order_release);
  m_impl->running.store(true, std::memory_order_release);
  m_impl->engine_thread = std::thread([this] { m_impl->run_engine(); });
  return true;
}

void InProcessBrainSession::stop() {
  if (!m_impl->running.exchange(false, std::memory_order_acq_rel)) {
    return;  // was not running
  }
  if (m_impl->engine_thread.joinable()) {
    m_impl->engine_thread.join();
  }
  m_impl->status.store(Status::kDisconnected, std::memory_order_release);
}

void InProcessBrainSession::set_audio_ring(AudioMidiRing* ring) { m_impl->audio_ring = ring; }

void InProcessBrainSession::set_clock_hooks_for_test(ClockHooks hooks) {
  // An empty (default-constructed) std::function field means "the caller
  // did not override this one" -- leave Impl's own default in place rather
  // than replacing a real hook with an empty std::function that would throw
  // std::bad_function_call the moment run_engine() called it.
  if (hooks.now_us) {
    m_impl->now_us_hook = std::move(hooks.now_us);
  }
  if (hooks.wait) {
    m_impl->wait_hook = std::move(hooks.wait);
  }
}

void InProcessBrainSession::send(std::string_view command_line) {
  if (command_line == "quit" || command_line == "exit") {
    return;  // never tear down the shared engine thread from a stray Enter
  }

  // `midi-source load <path>` does not translate to a Command POD (see the
  // path-carrying rings' comment above) -- handled here, before
  // command_line_to_command(), which stays Command-only. Mirrors
  // --control's own "usage: midi-source load <path>" text
  // (shell_io_commands.cpp's cmd_midi_source) for any other shape.
  const std::vector<std::string_view> ms_tokens = split_ws(command_line);
  if (!ms_tokens.empty() && ms_tokens[0] == "midi-source") {
    BrainEvent note;
    note.kind = BrainEvent::Kind::kError;
    note.valid = true;
    note.cmd = std::string(command_line);
    if (ms_tokens.size() != 3 || ms_tokens[1] != "load") {
      note.error = "usage: midi-source load <path>";
      m_impl->local_warnings.push_back(std::move(note));
      return;
    }
    const std::string_view path = ms_tokens[2];
    if (path.size() >= kMaxPathBytes) {
      note.error = "path too long (max " + std::to_string(kMaxPathBytes - 1) + " bytes)";
      m_impl->local_warnings.push_back(std::move(note));
      return;
    }
    PathCommand path_cmd;
    std::copy(path.begin(), path.end(), path_cmd.bytes.begin());
    path_cmd.length = static_cast<std::uint16_t>(path.size());
    if (!m_impl->path_command_ring.try_push(path_cmd)) {
      // Never-drop policy for GUI -> engine (same as the Command ring): warn
      // rather than silently swallow the user's load request.
      BrainEvent warn;
      warn.kind = BrainEvent::Kind::kWarn;
      warn.valid = true;
      warn.warn_code = "command_ring_full";
      m_impl->local_warnings.push_back(std::move(warn));
    }
    return;
  }

  // Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md) +
  // task #5 (per-section REPEAT COUNT, host-only, ZERO ABI/core change) +
  // Phase 2 (per-scene style/groove/key/tempo overrides, same doc's own
  // Phasing section): `song build <count> <section0> <bars0> <repeat0>
  // <style0> <groove0> <key0> <tempo0> <section1> ...` -- grid_panel.cpp's
  // own build_and_play_song()/activate_scene_column() send this instead of
  // the retired `style section` + `launch scene` pair. Each scene's own
  // `repeatN` token is either a decimal `1`..`255` (play this scene that
  // many times before advancing) or the literal `inf` (hold this scene
  // forever -- decoded to kSongBuildRepeatInfinite, apply_song_build's own
  // header comment covers what that does to the built chain). The four
  // Phase-2 tokens (`styleN`/`grooveN`/`keyN`/`tempoN`) each accept the
  // literal `-` for "no override" -- parse_song_build_scene_overrides above
  // decodes their real grammar. Like `midi-source load` above, the payload
  // (a variable-length scene list) does not fit the fixed-size ABI `Command`
  // POD, so it rides its own ring rather than command_line_to_command()'s
  // translation, and is handled here, before that call.
  constexpr std::size_t kSongBuildSceneStride = 7;
  const std::vector<std::string_view> song_tokens = split_ws(command_line);
  if (song_tokens.size() >= 2 && song_tokens[0] == "song" && song_tokens[1] == "build") {
    BrainEvent note;
    note.kind = BrainEvent::Kind::kError;
    note.valid = true;
    note.cmd = std::string(command_line);
    std::uint64_t count = 0;
    if (song_tokens.size() < 3 || !parse_uint(song_tokens[2], count) || count == 0 ||
        count > kMaxSongScenes || song_tokens.size() != 3 + count * kSongBuildSceneStride) {
      note.error =
          "usage: song build <count> <section> <bars> <repeat> <style> <groove> <key> <tempo> ...";
      m_impl->local_warnings.push_back(std::move(note));
      return;
    }
    SongBuildCommand build;
    build.scene_count = static_cast<std::uint8_t>(count);
    for (std::uint64_t i = 0; i < count; ++i) {
      const std::size_t base_idx = 3 + (i * kSongBuildSceneStride);
      SectionType section{};
      if (!parse_section_name(song_tokens[base_idx], section)) {
        note.error = "unknown section: " + std::string(song_tokens[base_idx]);
        m_impl->local_warnings.push_back(std::move(note));
        return;
      }
      std::uint64_t bars = 0;
      if (!parse_uint(song_tokens[base_idx + 1], bars) || bars == 0 || bars > 255) {
        note.error = "bad bar count: " + std::string(song_tokens[base_idx + 1]);
        m_impl->local_warnings.push_back(std::move(note));
        return;
      }
      const std::string_view repeat_token = song_tokens[base_idx + 2];
      std::uint8_t repeat = 1;
      if (repeat_token == "inf") {
        repeat = kSongBuildRepeatInfinite;
      } else {
        std::uint64_t repeat_value = 0;
        if (!parse_uint(repeat_token, repeat_value) || repeat_value == 0 || repeat_value > 255) {
          note.error = "bad repeat count: " + std::string(repeat_token);
          m_impl->local_warnings.push_back(std::move(note));
          return;
        }
        repeat = static_cast<std::uint8_t>(repeat_value);
      }
      SongBuildScene scene{
          .section = section, .n_bars = static_cast<std::uint8_t>(bars), .repeat = repeat};
      std::string override_detail;
      if (!parse_song_build_scene_overrides(song_tokens[base_idx + 3], song_tokens[base_idx + 4],
                                            song_tokens[base_idx + 5], song_tokens[base_idx + 6],
                                            scene, override_detail)) {
        note.error = override_detail;
        m_impl->local_warnings.push_back(std::move(note));
        return;
      }
      build.scenes[i] = scene;
    }
    if (!m_impl->song_build_ring.try_push(build)) {
      BrainEvent warn;
      warn.kind = BrainEvent::Kind::kWarn;
      warn.valid = true;
      warn.warn_code = "command_ring_full";
      m_impl->local_warnings.push_back(std::move(warn));
    }
    return;
  }

  Command cmd;
  std::string detail;
  switch (command_line_to_command(command_line, cmd, detail)) {
    case TranslateOutcome::kOk:
      break;
    case TranslateOutcome::kUnknownCommand: {
      BrainEvent note;
      note.kind = BrainEvent::Kind::kError;
      note.valid = true;
      note.error = "integrated mode does not translate this command to a Command POD yet";
      note.cmd = std::string(command_line);
      m_impl->local_warnings.push_back(std::move(note));
      return;
    }
    case TranslateOutcome::kInvalidArgument: {
      BrainEvent note;
      note.kind = BrainEvent::Kind::kError;
      note.valid = true;
      note.error = detail;
      note.cmd = std::string(command_line);
      m_impl->local_warnings.push_back(std::move(note));
      return;
    }
  }
  if (!m_impl->command_ring.try_push(cmd)) {
    // Asymmetric overflow policy (Corelli §15.2 correction #3): the Command
    // ring never silently drops. At 1024 slots this should be unreachable;
    // surface it as a warn-class note the next poll() returns rather than
    // dropping the user's command unnoticed.
    BrainEvent warn;
    warn.kind = BrainEvent::Kind::kWarn;
    warn.valid = true;
    warn.warn_code = "command_ring_full";
    m_impl->local_warnings.push_back(std::move(warn));
    return;
  }

  // Auto-route the default band (see kDefaultStyleRoutes's own comment)
  // right after a successful `style load` OR `style switch` -- and only then:
  // routing before a style is active would apply to whatever style comes
  // next, not this one, and there is nothing to route if the command itself
  // never reached the ring. `style switch` (the browser's playing-time morph,
  // browser_panel.cpp) MUST route too: Route::enabled defaults to false and
  // ONLY kStyleRoute flips it, so a switch that was never preceded by a load
  // (e.g. the user pressed Play, THEN picked a style) would otherwise emit no
  // MIDI on out0 and stay silent forever. Re-enabling an already-enabled
  // route is idempotent, so routing on every load/switch is harmless. Same
  // validated Command path as every other verb here (never a shortcut around
  // push_command).
  if (cmd.param == Param::kStyleLoad || cmd.param == Param::kStyleSwitch) {
    for (const DefaultStyleRoute& route : kDefaultStyleRoutes) {
      if (!m_impl->command_ring.try_push(make_default_style_route_command(route))) {
        BrainEvent warn;
        warn.kind = BrainEvent::Kind::kWarn;
        warn.valid = true;
        warn.warn_code = "command_ring_full";
        m_impl->local_warnings.push_back(std::move(warn));
      }
    }
  }
}

void InProcessBrainSession::poll(std::vector<BrainEvent>& out) {
  for (BrainEvent& warn : m_impl->local_warnings) {
    out.push_back(std::move(warn));
  }
  m_impl->local_warnings.clear();

  PathResult path_result;
  while (m_impl->path_result_ring.try_pop(path_result)) {
    BrainEvent note;
    note.kind = BrainEvent::Kind::kError;
    note.valid = true;
    note.error = std::string(path_result.error_bytes.data(), path_result.error_length);
    note.cmd = "midi-source load";
    out.push_back(std::move(note));
  }

  const bool prefer_flats = m_impl->prefer_flats.load(std::memory_order_relaxed);
  OutEvent ev;
  while (m_impl->out_event_ring.try_pop(ev)) {
    out.push_back(brain_event_from_outevent(ev, prefer_flats));
  }
}

const BrainSnapshot& InProcessBrainSession::snapshot() const { return m_impl->snapshot; }

BrainSession::Status InProcessBrainSession::status() const {
  return m_impl->status.load(std::memory_order_acquire);
}

}  // namespace sonotron

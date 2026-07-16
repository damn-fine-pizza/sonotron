#include "in_process_brain_session.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <utility>

#include "arrangrr/abi.hpp"
#include "arrangrr/arranger/style_model.hpp"
#include "arrangrr/clip/clip_matrix.hpp"
#include "arrangrr/loop/loop_buffer.hpp"
#include "audio/spsc_ring.hpp"
#include "brain_event_from_outevent.hpp"
#include "common/time.hpp"
#include "midi_hal.hpp"
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
using arrangrr::Command;
using arrangrr::ContentKind;
using arrangrr::kMaxPorts;
using arrangrr::kNoExplicitClipId;
using arrangrr::kNoLoopExplicitId;
using arrangrr::LoopLengthMode;
using arrangrr::LoopRecordMode;
using arrangrr::Op;
using arrangrr::OutEvent;
using arrangrr::Param;
using arrangrr::SectionType;
using arrangrr::Span;
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

  // `style switch <name>` -- the LIVE morph counterpart to `style load` above
  // (browser_panel.cpp sends this instead of `style load` whenever the
  // transport is already playing): rides the existing Param::kStyleSwitch
  // verb (engine.cpp's style_switch()), which quantizes to the next bar
  // boundary while playing instead of hard-resetting the arranger. The GUI
  // has no current-section readback (grid_model.hpp's documented gap), so the
  // target section always defaults to SectionType::kVarA, the arranger's own
  // default section -- same name resolution as `style load`.
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

std::uint64_t monotonic_us() {
  const auto epoch = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
}

}  // namespace

struct InProcessBrainSession::Impl {
  using Status = BrainSession::Status;

  SpscRing<Command, kCommandRingCapacity> command_ring;
  SpscRing<OutEvent, kOutEventRingCapacity> out_event_ring;
  SpscRing<PathCommand, kPathCommandRingCapacity> path_command_ring;
  SpscRing<PathResult, kPathResultRingCapacity> path_result_ring;
  // Phase-6 Theme 2 (Decision 1/2/3): sonotron::audio::AudioBackend's
  // producer-side ring handle, set (or left null) by set_audio_ring()
  // BEFORE start() -- see that method's own doc comment for the
  // synchronization argument. Never touched after run_engine() reads it
  // once at thread-start.
  AudioMidiRing* audio_ring = nullptr;
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

  Shell shell([this, &midi, alsa_ok, audio_out_ring](const OutEvent& ev) {
    if (alsa_ok && ev.kind == OutEvent::Kind::kMidi) {
      midi->send(ev.port, ev.msg);
    }
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
  std::uint64_t last_us = monotonic_us();

  while (running.load(std::memory_order_acquire)) {
    // Drain the Command ring: never silently dropped by the PRODUCER side
    // (see InProcessBrainSession::send()) -- the engine just applies
    // whatever is queued, in FIFO order, through the exact same
    // Engine::push_command every exec_line handler already calls. ONE
    // special case: Param::kNoteRaw (docs/proposals/looper-in-gui-contract.md
    // §2/§7 items 1/2) has no case in Engine::push_command's own dispatch
    // switch (it would land on the unhandled-param default and emit
    // kUnknownCommand, see abi.hpp's own kNoteRaw comment) -- it is decoded
    // to raw MIDI bytes and fed through the SAME feed_midi() entry point
    // item 3's ALSA drain below uses, exactly like Shell::cmd_note's own L1
    // handling (shell_music_commands.cpp).
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

    const std::uint64_t now_us = monotonic_us();
    acc.set_bpm(shell.engine().transport().bpm());
    const std::uint32_t ticks = acc.advance_us(now_us - last_us);
    last_us = now_us;
    if (ticks > 0) {
      std::string tick_error;
      shell.advance_by(ticks, tick_error);
    }
    prefer_flats.store(shell.prefer_flats(), std::memory_order_relaxed);

    // Short sleep, not a busy spin: there is no fd to block on here (unlike
    // sonotron-server's timerfd + poll()), so this is the clock's wakeup
    // cadence -- 0.5 ms, same interval sonotron-server's timerfd uses.
    std::this_thread::sleep_for(std::chrono::microseconds(500));
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

#pragma once

#include <cstdint>
#include <string>

#include "arrangrr/engine.hpp"

// Shared file-local constants and parsing helpers for the Shell translation
// units. The Shell method bodies live across several shell_*.cpp files, so the
// helpers they all reach for cannot hide in a per-file anonymous namespace —
// they are hoisted here into arrangrr::host::shell_detail and defined once in
// shell_parse.cpp. Constants are inline constexpr (ODR-safe) at the same scope.

namespace arrangrr::host {

namespace shell_detail {

// Panel geometry used when no live terminal is attached (script/flat mode).
inline constexpr int kDefaultPanelColumns = 80;
inline constexpr int kDefaultPanelRows = 24;

// Global TUI shortcut bytes (raw control chars — work on every terminal).
inline constexpr std::uint8_t kCtrlPlayStop = 0x10;  // CTRL+P: transport play/stop
inline constexpr std::uint8_t kCtrlChooser = 0x60;   // backtick `: focus the styles panel
inline constexpr std::uint8_t kCtrlQuit = 0x03;      // CTRL+C: always quit the app (ISIG is off)
inline constexpr std::uint8_t kCtrlLayout = 0x1A;    // CTRL+Z: toggle 1<->2 panels per row
inline constexpr std::uint8_t kCtrlApplyNow = 0x1C;  // CTRL+\: apply the chooser now

// Variation/style stepping keys (docs/TUI_SPEC.md). `-`/`=` step the variation
// (section); `_`/`+` (shift+`-`/`=`) step the style. All clamp (no wrap) and
// only step a PENDING selection — the switch is debounced (see main.cpp).
inline constexpr std::uint8_t kStepSectionPrev = 0x2D;  // '-'  previous variation
inline constexpr std::uint8_t kStepSectionNext = 0x3D;  // '='  next variation
inline constexpr std::uint8_t kStepStylePrev = 0x5F;    // '_'  previous style
inline constexpr std::uint8_t kStepStyleNext = 0x2B;    // '+'  next style

// Chooser edit bytes (raw control chars a plain TTY delivers).
inline constexpr std::uint8_t kEnterCr = 0x0D;
inline constexpr std::uint8_t kEnterLf = 0x0A;
inline constexpr std::uint8_t kBackspaceDel = 0x7F;
inline constexpr std::uint8_t kBackspaceBs = 0x08;
inline constexpr std::uint8_t kAsciiDigitLow = '0';
inline constexpr std::uint8_t kAsciiDigitHigh = '9';

// The simulated piano feeds the same input port real hardware uses (in0).
inline constexpr std::uint8_t kPianoInputPort = 0;
inline constexpr std::uint8_t kPianoReleaseVelocity = 64;

// Piano policy limits (docs/TUI_SPEC.md §4) and MIDI wire ranges.
inline constexpr int kPianoMinOctave = -1;
inline constexpr int kPianoMaxOctave = 9;
inline constexpr int kSemitonesPerOctave = 12;
inline constexpr int kMidiNoteMax = 127;
inline constexpr int kMidiChannels = 16;
inline constexpr int kMidiVelocityMin = 1;
inline constexpr int kMidiVelocityMax = 127;

// Parsing helpers shared across the Shell translation units (defined in
// shell_parse.cpp). Each returns false on a malformed token, leaving `out`
// untouched unless noted.
bool parse_u64(const std::string& s, std::uint64_t& out);
bool parse_bpm_x100(const std::string& s, std::uint32_t& out);
bool split_port_channel(const std::string& s, std::string& name, int& channel);
bool parse_note(const std::string& s, std::uint8_t& out);
bool parse_pc(const std::string& s, std::uint8_t& out);
bool parse_mode(const std::string& s, Mode& out);
bool parse_quality(const std::string& s, std::int8_t& out);
bool key_prefers_flats(std::uint8_t root_pc, Mode mode);
bool parse_duration(const std::string& s, std::uint64_t& out_ticks);
bool parse_section(const std::string& s, SectionType& out);
bool iequals(const char* a, const std::string& b);
int find_builtin_style(const std::string& name);
bool parse_role(const std::string& s, TrackRole& out);
bool parse_hex_byte(const std::string& s, std::uint8_t& out);
bool parse_int(const std::string& s, int& out);

}  // namespace shell_detail

}  // namespace arrangrr::host

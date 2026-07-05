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

// Two playable surfaces selected by panel focus (whole keyboard, no pitch
// split). The PIANO panel plays the MELODY surface: its keys feed kPianoInputPort
// (in0), zone kMelody — they SOUND and steer nobody. The CHORDS panel plays the
// HARMONY surface: the SAME key bindings feed kHarmonyInputPort, zone kHarmony —
// their notes are output-suppressed (silent) but OBSERVED by the ChordDetector,
// so playing there re-harmonizes the band. kHarmonyInputPort is a logical engine
// port (< kMaxPorts); it is the chord-detect port under the default topology and
// is fed only by the chords-panel keys (no ALSA hardware port is created for it).
inline constexpr std::uint8_t kPianoInputPort = 0;
inline constexpr std::uint8_t kHarmonyInputPort = 1;
inline constexpr std::uint8_t kPianoReleaseVelocity = 64;

// Toggle-mode auto-repeat debounce window (microseconds). OS key auto-repeat is
// typically an initial delay of ~250-500 ms then a sustained ~30 ms cadence; a
// window this wide swallows the sustained machine-gun (and slides forward on
// every repeat, so a held key stays one note). Toggle mode cannot perfectly tell
// a fast intentional re-tap from auto-repeat — that is why kitty/momentary is
// preferred — but this stops the flood.
inline constexpr std::uint64_t kToggleAutoRepeatDebounceUs = 300'000;

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

// D47 chord-follow labels, shared by the `chord follow` confirmation and the
// chords panel so the wording matches. `label` is the short selector name;
// `hint` is the one-line meaning of who steers the band.
inline const char* chord_follow_label(ChordFollow follow) {
  switch (follow) {
    case ChordFollow::kDetect:
      return "detect";
    case ChordFollow::kSequencer:
      return "sequencer";
    case ChordFollow::kManual:
      return "manual";
    case ChordFollow::kAuto:
    default:
      return "auto";
  }
}
inline const char* chord_follow_hint(ChordFollow follow) {
  switch (follow) {
    case ChordFollow::kDetect:
      return "live keys steer";
    case ChordFollow::kSequencer:
      return "the sequence steers";
    case ChordFollow::kManual:
      return "chord play steers";
    case ChordFollow::kAuto:
    default:
      return "any source steers";
  }
}

// Parses a `chord follow` argument into the selector (`seq` is an alias of
// `sequencer`). The one place that knows the argument spelling — kept next to
// label/hint so the three stay in sync when a value is added.
inline bool parse_chord_follow(const std::string& s, ChordFollow& out) {
  if (s == "auto") {
    out = ChordFollow::kAuto;
  } else if (s == "detect") {
    out = ChordFollow::kDetect;
  } else if (s == "sequencer" || s == "seq") {
    out = ChordFollow::kSequencer;
  } else if (s == "manual") {
    out = ChordFollow::kManual;
  } else {
    return false;
  }
  return true;
}

}  // namespace shell_detail

}  // namespace arrangrr::host

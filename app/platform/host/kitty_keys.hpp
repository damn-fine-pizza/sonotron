#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// Kitty keyboard protocol (a.k.a. "progressive enhancement", CSI u) helpers,
// host-only. This is the ONLY honest way to get true key press/release + real
// polyphony out of a terminal: a raw/canonical-off TTY delivers a byte on key
// PRESS ONLY — there is no key-release event in a standard TTY. Terminals that
// implement the kitty protocol (kitty, foot, ghostty, wezterm, recent xterm)
// will, once the right flags are pushed, emit distinct escape sequences for
// press, repeat and release, each carrying the key's unicode code.
//
// Spec: https://sw.kovidgoyal.net/kitty/keyboard-protocol/
//
// This file is pure: it defines the escape-sequence constants, a tiny fd-level
// enable/disable, and a unit-testable parser for the incoming key events. It
// has no piano/shell/terminal-layout knowledge.

namespace arrangrr::host {

namespace kitty {

// Progressive-enhancement flag bits we push (kitty spec, "Progressive
// enhancement"):
//   0x1 disambiguate escape codes
//   0x2 report event types              (press / repeat / release)
//   0x8 report all keys as escape codes (so plain letter keys also get
//                                        release events — required for the
//                                        piano's A..L musical keys)
// We push 0x1|0x2|0x8 = 11. We deliberately omit 0x4 (report alternate keys)
// and 0x10 (report associated text): the key's base unicode code is all the
// piano needs.
inline constexpr unsigned kFlagDisambiguate = 0x1;
inline constexpr unsigned kFlagReportEvents = 0x2;
inline constexpr unsigned kFlagReportAllKeys = 0x8;
inline constexpr unsigned kPushFlags = kFlagDisambiguate | kFlagReportEvents | kFlagReportAllKeys;

// Push flags:  CSI > flags u   (here CSI > 11 u).
// Pop one entry off the terminal's flag stack:  CSI < number u  (here 1).
// Popping when nothing was pushed is a no-op per the spec, so a stray disable
// on a non-supporting terminal is harmless.
inline constexpr std::string_view kEnable = "\x1b[>11u";
inline constexpr std::string_view kDisable = "\x1b[<1u";

// Startup capability probe (host-only). We ask the terminal two questions in
// one round trip:
//   kQueryFlags              CSI ? u    "report your current keyboard flags"
//   kQueryDeviceAttributes   CSI c      Primary Device Attributes
// A terminal that implements the kitty protocol answers kQueryFlags with a
// CSI ? <flags> u report; one that does not simply ignores it. EVERY terminal
// answers Device Attributes with CSI ? ... c, so that reply is our reliable
// "the terminal has finished talking" sentinel: read until the DA reply lands,
// then a kitty-flags reply seen alongside it means the protocol is supported.
inline constexpr std::string_view kQueryFlags = "\x1b[?u";
inline constexpr std::string_view kQueryDeviceAttributes = "\x1b[c";

// Modifier bitmask bits (kitty spec, "Modifiers"). The wire field is the
// bitmask PLUS ONE, so a decoded value of 0 means "no modifiers".
inline constexpr std::uint8_t kModShift = 0x1;
inline constexpr std::uint8_t kModAlt = 0x2;
inline constexpr std::uint8_t kModCtrl = 0x4;

// Writes the enable (push) or disable (pop) sequence to `fd`. The caller is
// responsible for gating this behind isatty so scripts/pipes stay byte-for-byte
// unchanged; on a terminal that does not implement the protocol the sequence is
// silently ignored.
void set_progressive_enhancement(int fd, bool enable);

}  // namespace kitty

// One parsed kitty key event. `code` is the key's base unicode code point
// (e.g. 97 = 'a'); `type` distinguishes press / autorepeat / release;
// `modifiers` is the DECODED modifier bitmask (kitty::kMod* bits) — the raw
// wire value minus one, so 0 means no modifiers.
struct KittyKeyEvent {
  enum class Type { kPress, kRepeat, kRelease };

  std::uint32_t code = 0;
  Type type = Type::kPress;
  std::uint8_t modifiers = 0;
};

// Parses the body of a kitty key escape sequence — the bytes between the
// leading "CSI" (ESC '[') and the terminating 'u', e.g. the "97;1:3" of
// "\x1b[97;1:3u". A single trailing 'u' is tolerated. The grammar is
//
//     unicode-key-code[:shifted[:base]] [; modifiers[:event-type] [; text]]
//
// where event-type is 1 press (default), 2 repeat, 3 release. Only the base
// unicode code and the event type are extracted; the other fields are skipped.
// Returns nullopt for anything that is not a well-formed kitty key event (a
// plain letter, an empty body, a non-numeric code, an unknown event type).
std::optional<KittyKeyEvent> parse_kitty_key(std::string_view csi_body);

// Maps a Ctrl-modified ASCII key to the control byte a plain raw-mode TTY would
// have delivered for it — the bridge that lets kitty-protocol Ctrl chords reach
// the same handler as their plain-byte equivalents. Returns nullopt when the
// Ctrl bit is not set or the key has no control byte. `modifiers` is a decoded
// bitmask (kitty::kMod*). Pure and unit-testable, no I/O.
//   letter A..Z / a..z + Ctrl -> upper & 0x1f   (e.g. 'p' -> 0x10)
//   space + Ctrl              -> 0x00
//   backslash '\\' + Ctrl     -> 0x1c
std::optional<std::uint8_t> control_byte_for(std::uint32_t code, std::uint8_t modifiers);

}  // namespace arrangrr::host

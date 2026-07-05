#include "kitty_keys.hpp"

#include <unistd.h>

namespace arrangrr::host {

namespace {

// Parses a run of ASCII decimal digits into `out`. Requires a non-empty,
// digits-only view and guards against absurd lengths (a real key code is at
// most a few digits). Returns false otherwise — this is what makes a plain
// letter body fail cleanly rather than parse as a bogus code.
bool parse_decimal(std::string_view s, std::uint32_t& out) {
  if (s.empty() || s.size() > 7) {
    return false;
  }

  std::uint32_t value = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<std::uint32_t>(c - '0');
  }

  out = value;
  return true;
}

// The three sub-fields of a ';'-delimited section are separated by ':'. Returns
// the first sub-field (up to the first ':' or the whole view when absent).
std::string_view first_subfield(std::string_view section) {
  const std::size_t colon = section.find(':');
  return colon == std::string_view::npos ? section : section.substr(0, colon);
}

}  // namespace

namespace kitty {

void set_progressive_enhancement(int fd, bool enable) {
  const std::string_view seq = enable ? kEnable : kDisable;
  (void)!write(fd, seq.data(), seq.size());
}

}  // namespace kitty

std::optional<KittyKeyEvent> parse_kitty_key(std::string_view body) {
  // Tolerate the terminating 'u' so both "97;1:3" and "97;1:3u" parse.
  if (!body.empty() && body.back() == 'u') {
    body.remove_suffix(1);
  }
  if (body.empty()) {
    return std::nullopt;
  }

  // Section 0 = key codes (unicode[:shifted[:base]]); we want the base code.
  const std::size_t first_semi = body.find(';');
  const std::string_view key_section =
      first_semi == std::string_view::npos ? body : body.substr(0, first_semi);

  std::uint32_t code = 0;
  if (!parse_decimal(first_subfield(key_section), code)) {
    return std::nullopt;
  }

  // Section 1 = modifiers[:event-type]. Absent event-type means a press; the
  // modifiers sub-field is the raw bitmask plus one (so "5" = 0x4 = ctrl), and
  // its absence means no modifiers.
  KittyKeyEvent::Type type = KittyKeyEvent::Type::kPress;
  std::uint8_t modifiers = 0;
  if (first_semi != std::string_view::npos) {
    std::string_view rest = body.substr(first_semi + 1);
    const std::size_t second_semi = rest.find(';');
    const std::string_view mod_section =
        second_semi == std::string_view::npos ? rest : rest.substr(0, second_semi);

    const std::string_view mod_value = first_subfield(mod_section);
    if (!mod_value.empty()) {
      std::uint32_t raw = 0;
      if (!parse_decimal(mod_value, raw)) {
        return std::nullopt;
      }
      if (raw > 0) {
        modifiers = static_cast<std::uint8_t>(raw - 1);
      }
    }

    const std::size_t mod_colon = mod_section.find(':');
    if (mod_colon != std::string_view::npos) {
      std::uint32_t event = 0;
      if (!parse_decimal(mod_section.substr(mod_colon + 1), event)) {
        return std::nullopt;
      }
      switch (event) {
        case 1:
          type = KittyKeyEvent::Type::kPress;
          break;
        case 2:
          type = KittyKeyEvent::Type::kRepeat;
          break;
        case 3:
          type = KittyKeyEvent::Type::kRelease;
          break;
        default:
          return std::nullopt;
      }
    }
  }

  return KittyKeyEvent{.code = code, .type = type, .modifiers = modifiers};
}

std::optional<std::uint8_t> control_byte_for(std::uint32_t code, std::uint8_t modifiers) {
  // Only Ctrl chords carry a control byte, and only over ASCII.
  constexpr std::uint32_t kAsciiMax = 0x7F;
  if ((modifiers & kitty::kModCtrl) == 0 || code > kAsciiMax) {
    return std::nullopt;
  }

  constexpr std::uint32_t kCaseBit = 0x20;   // 'a' - 'A'; clears to uppercase
  constexpr std::uint32_t kCtrlMask = 0x1F;  // uppercase letter -> control byte
  constexpr std::uint8_t kCtrlSpace = 0x00;
  constexpr std::uint8_t kCtrlBackslash = 0x1C;

  const bool is_letter = (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z');
  if (is_letter) {
    return static_cast<std::uint8_t>((code & ~kCaseBit) & kCtrlMask);
  }
  if (code == ' ') {
    return kCtrlSpace;
  }
  if (code == '\\') {
    return kCtrlBackslash;
  }
  return std::nullopt;
}

}  // namespace arrangrr::host

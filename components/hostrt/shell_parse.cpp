#include "shell_internal.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>

#include "shell.hpp"

// Definitions of the shared parsing helpers declared in shell_internal.hpp.
// Moved verbatim from shell.cpp's former anonymous namespace; the only change
// is the enclosing namespace (arrangrr::host::shell_detail) so the Shell method
// bodies scattered across shell_*.cpp can all reach them.

namespace arrangrr::host {

namespace shell_detail {

bool parse_u64(const std::string& s, std::uint64_t& out) {
  if (s.empty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  out = std::strtoull(s.c_str(), &end, 10);
  return end != nullptr && *end == '\0' && errno == 0;  // reject trailing junk AND overflow
}

// "120" or "120.5" or "120.50" -> bpm_x100.
bool parse_bpm_x100(const std::string& s, std::uint32_t& out) {
  const auto dot = s.find('.');
  std::uint64_t whole = 0, frac = 0;
  if (dot == std::string::npos) {
    if (!parse_u64(s, whole)) {
      return false;
    }
  } else {
    std::string f = s.substr(dot + 1);
    if (f.empty() || f.size() > 2) {
      return false;
    }
    if (!parse_u64(s.substr(0, dot), whole) || !parse_u64(f, frac)) {
      return false;
    }
    if (f.size() == 1) {
      frac *= 10;
    }
  }
  // Far above any real tempo (the core clamps to 20..400); rejecting here keeps
  // whole * 100 from wrapping and a wrapped-small value from sneaking past the
  // clamp.
  constexpr std::uint64_t kMaxBpmWhole = 100000;
  if (whole > kMaxBpmWhole) {
    return false;
  }
  out = static_cast<std::uint32_t>(whole * 100 + frac);
  return true;
}

// "name" or "name:ch" (1-based channel) -> name + channel (-1 = unspecified).
bool split_port_channel(const std::string& s, std::string& name, int& channel) {
  const auto colon = s.find(':');
  channel = -1;
  if (colon == std::string::npos) {
    name = s;
    return true;
  }
  name = s.substr(0, colon);
  std::uint64_t ch = 0;
  if (!parse_u64(s.substr(colon + 1), ch) || ch < 1 || ch > 16) {
    return false;
  }
  channel = static_cast<int>(ch - 1);
  return true;
}

// Note name in scientific pitch notation (C4 = 60): letter, optional #/b,
// octave -1..9. Plain MIDI numbers are accepted too.
bool parse_note(const std::string& s, std::uint8_t& out) {
  std::uint64_t raw = 0;
  if (parse_u64(s, raw)) {
    if (raw > 127) {
      return false;
    }
    out = static_cast<std::uint8_t>(raw);
    return true;
  }
  if (s.empty()) {
    return false;
  }
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
  const char letter = s[0];
  if (letter < 'A' || letter > 'G') {
    return false;
  }
  int semi = kSemis[letter - 'A'];
  std::size_t pos = 1;
  if (pos < s.size() && s[pos] == '#') {
    ++semi;
    ++pos;
  } else if (pos < s.size() && s[pos] == 'b') {
    --semi;
    ++pos;
  }
  if (pos >= s.size()) {
    const int dflt = (4 + 1) * 12 + semi;  // no octave -> octave 4 (C4 = 60)
    if (dflt < 0 || dflt > 127) {
      return false;
    }
    out = static_cast<std::uint8_t>(dflt);
    return true;
  }
  bool negative = false;
  if (s[pos] == '-') {
    negative = true;
    ++pos;
  }
  std::uint64_t octave = 0;
  if (!parse_u64(s.substr(pos), octave) || octave > 9) {
    return false;
  }
  const int oct = negative ? -static_cast<int>(octave) : static_cast<int>(octave);
  if (oct < -1) {
    return false;
  }
  const int note = (oct + 1) * 12 + semi;
  if (note < 0 || note > 127) {
    return false;
  }
  out = static_cast<std::uint8_t>(note);
  return true;
}

// Pitch class only (key roots): letter + optional #/b.
bool parse_pc(const std::string& s, std::uint8_t& out) {
  if (s.empty()) {
    return false;
  }
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};
  if (s[0] < 'A' || s[0] > 'G') {
    return false;
  }
  int semi = kSemis[s[0] - 'A'];
  if (s.size() == 2) {
    if (s[1] == '#') {
      ++semi;
    } else if (s[1] == 'b') {
      --semi;
    } else {
      return false;
    }
  } else if (s.size() > 2) {
    return false;
  }
  out = static_cast<std::uint8_t>((semi + 12) % 12);
  return true;
}

bool parse_mode(const std::string& s, Mode& out) {
  struct Entry {
    const char* name;
    Mode mode;
  };
  static constexpr Entry kModes[] = {
      {.name = "major", .mode = Mode::kMajor},
      {.name = "minor", .mode = Mode::kMinor},
      {.name = "dorian", .mode = Mode::kDorian},
      {.name = "phrygian", .mode = Mode::kPhrygian},
      {.name = "lydian", .mode = Mode::kLydian},
      {.name = "mixolydian", .mode = Mode::kMixolydian},
      {.name = "locrian", .mode = Mode::kLocrian},
  };
  for (const Entry& e : kModes) {
    if (s == e.name) {
      out = e.mode;
      return true;
    }
  }
  return false;
}

bool parse_quality(const std::string& s, std::int8_t& out) {
  struct Entry {
    const char* name;
    ChordQuality q;
  };
  static constexpr Entry kQ[] = {
      {.name = "maj", .q = ChordQuality::kMaj},
      {.name = "min", .q = ChordQuality::kMin},
      {.name = "dim", .q = ChordQuality::kDim},
      {.name = "aug", .q = ChordQuality::kAug},
      {.name = "maj7", .q = ChordQuality::kMaj7},
      {.name = "min7", .q = ChordQuality::kMin7},
      {.name = "m7", .q = ChordQuality::kMin7},
      {.name = "7", .q = ChordQuality::kDom7},
      {.name = "dom7", .q = ChordQuality::kDom7},
      {.name = "m7b5", .q = ChordQuality::kHalfDim7},
      {.name = "halfdim", .q = ChordQuality::kHalfDim7},
      {.name = "dim7", .q = ChordQuality::kDim7},
      {.name = "sus2", .q = ChordQuality::kSus2},
      {.name = "sus4", .q = ChordQuality::kSus4},
  };
  for (const Entry& e : kQ) {
    if (s == e.name) {
      out = static_cast<std::int8_t>(e.q);
      return true;
    }
  }
  return false;
}

// Flat-side keys spell with flats (Bb, Eb, ...): true when the parent major
// signature has flats. Parent major root = key root minus the mode's offset.
bool key_prefers_flats(std::uint8_t root_pc, Mode mode) {
  static constexpr std::uint8_t kOffset[7] = {0, 9, 2, 4, 5, 7, 11};
  const std::uint8_t parent =
      static_cast<std::uint8_t>((root_pc + 12 - kOffset[static_cast<int>(mode)]) % 12);
  return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 || parent == 6;
}

// "2bars" / "1bar" / "4beats" / "1beat" -> ticks.
bool parse_duration(const std::string& s, std::uint64_t& out_ticks) {
  auto strip = [&](const char* suffix, std::uint64_t mult) {
    const std::size_t n = std::string(suffix).size();
    if (s.size() <= n || s.substr(s.size() - n) != suffix) {
      return false;
    }
    std::uint64_t v = 0;
    if (!parse_u64(s.substr(0, s.size() - n), v) || v == 0) {
      return false;
    }
    out_ticks = v * mult;
    return true;
  };
  return strip("bars", kTicksPerBar) || strip("bar", kTicksPerBar) ||
         strip("beats", kTicksPerBeat) || strip("beat", kTicksPerBeat);
}

bool parse_section(const std::string& s, SectionType& out) {
  struct Entry {
    const char* name;
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

// Case-insensitive comparison of a C string against a std::string (ASCII).
bool iequals(const char* a, const std::string& b) {
  std::size_t i = 0;
  for (; a[i] != '\0' && i < b.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return a[i] == '\0' && i == b.size();
}

// Resolves a style name to its builtin index, case-insensitively (a superset of
// how `style load` maps names to indices). Returns -1 when nothing matches.
int find_builtin_style(const std::string& name) {
  for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
    if (iequals(styles::kBuiltins[i]->name, name)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool parse_role(const std::string& s, TrackRole& out) {
  struct Entry {
    const char* name;
    TrackRole role;
  };
  static constexpr Entry kRoles[] = {
      {.name = "drums", .role = TrackRole::kDrums},
      {.name = "perc", .role = TrackRole::kPerc},
      {.name = "bass", .role = TrackRole::kBass},
      {.name = "chord1", .role = TrackRole::kChord1},
      {.name = "chord2", .role = TrackRole::kChord2},
      {.name = "pad", .role = TrackRole::kPad},
      {.name = "arp", .role = TrackRole::kArp},
      {.name = "phrase", .role = TrackRole::kPhrase},
      {.name = "lead", .role = TrackRole::kLead},
      {.name = "cc", .role = TrackRole::kCc},
  };
  for (const Entry& e : kRoles) {
    if (s == e.name) {
      out = e.role;
      return true;
    }
  }
  return false;
}

bool parse_hex_byte(const std::string& s, std::uint8_t& out) {
  if (s.empty() || s.size() > 2) {
    return false;
  }
  char* end = nullptr;
  const unsigned long v = std::strtoul(s.c_str(), &end, 16);
  if (!end || *end != '\0' || v > 0xFF) {
    return false;
  }
  out = static_cast<std::uint8_t>(v);
  return true;
}

bool parse_int(const std::string& s, int& out) {
  if (s.empty()) {
    return false;
  }

  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }

  out = static_cast<int>(v);
  return true;
}

}  // namespace shell_detail

}  // namespace arrangrr::host

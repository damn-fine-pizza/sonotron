#include "casm.hpp"

#include <array>

namespace arrstyle {

namespace {

// ----------------------------------------------------------------------------
// Bounded big-endian reads over the raw byte buffer (SFF chunks are big-endian).
// ----------------------------------------------------------------------------

constexpr std::size_t kChunkHeaderSize = 8;  // 4-byte tag + 4-byte length

std::uint32_t read_be32(const std::vector<std::uint8_t>& b, std::size_t off) noexcept {
  return (static_cast<std::uint32_t>(b[off]) << 24) |
         (static_cast<std::uint32_t>(b[off + 1]) << 16) |
         (static_cast<std::uint32_t>(b[off + 2]) << 8) | static_cast<std::uint32_t>(b[off + 3]);
}

bool tag_equals(const std::vector<std::uint8_t>& b, std::size_t off, const char* tag) noexcept {
  return b[off] == static_cast<std::uint8_t>(tag[0]) &&
         b[off + 1] == static_cast<std::uint8_t>(tag[1]) &&
         b[off + 2] == static_cast<std::uint8_t>(tag[2]) &&
         b[off + 3] == static_cast<std::uint8_t>(tag[3]);
}

std::size_t find_marker(const std::vector<std::uint8_t>& b, const char* marker) noexcept {
  const std::size_t len = 4;
  if (b.size() < len) {
    return std::string::npos;
  }
  for (std::size_t i = 0; i + len <= b.size(); ++i) {
    if (tag_equals(b, i, marker)) {
      return i;
    }
  }
  return std::string::npos;
}

// ----------------------------------------------------------------------------
// CASM field offsets, reverse-engineered from user-provided files and confirmed
// stable across the corpus. Named so no offset is a bare magic number.
// ----------------------------------------------------------------------------

constexpr std::size_t kCtabSourceChannel = 0;
constexpr std::size_t kCtabName = 1;
constexpr std::size_t kCtabNameLen = 8;
constexpr std::size_t kCtabDestChannel = 9;
constexpr std::size_t kCtabSourceRoot = 18;
constexpr std::size_t kCtabSourceType = 19;
constexpr std::size_t kCtabNtr = 20;
constexpr std::size_t kCtabNtt = 21;         // SFF1 only
constexpr std::size_t kCtabNoteLow = 23;     // SFF1 only
constexpr std::size_t kCtabNoteHigh = 24;    // SFF1 only
constexpr std::size_t kCtabRetrigger = 25;   // SFF1 only
constexpr std::size_t kCtabSff1MinLen = 26;  // must reach through offset 25
constexpr std::size_t kCtb2MinLen = 21;      // must reach through offset 20 (NTR)

constexpr std::uint8_t kPitchClassMask = 0x0F;
constexpr std::uint8_t kPitchClasses = 12;

NoteTranspositionRule to_ntr(std::uint8_t v) noexcept {
  switch (v) {
    case 0:
      return NoteTranspositionRule::kRootTranspose;
    case 1:
      return NoteTranspositionRule::kRootFixed;
    case 2:
      return NoteTranspositionRule::kGuitar;
    default:
      return NoteTranspositionRule::kUnknown;
  }
}

NoteTranspositionTable to_ntt(std::uint8_t v) noexcept {
  switch (v) {
    case 0:
      return NoteTranspositionTable::kBypass;
    case 1:
      return NoteTranspositionTable::kMelody;
    case 2:
      return NoteTranspositionTable::kChord;
    case 3:
      return NoteTranspositionTable::kBass;
    case 4:
      return NoteTranspositionTable::kMelodicMinor;
    case 6:
      return NoteTranspositionTable::kHarmonicMinor;
    default:
      return NoteTranspositionTable::kUnknown;
  }
}

std::string extract_name(const std::vector<std::uint8_t>& b, std::size_t off, std::size_t len) {
  std::string out;
  for (std::size_t i = 0; i < len && off + i < b.size(); ++i) {
    const std::uint8_t c = b[off + i];
    if (c == 0) {
      break;
    }
    out.push_back(static_cast<char>(c));
  }
  // Trim trailing spaces (Yamaha pads names to 8 chars).
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

bool parse_ctab(const std::vector<std::uint8_t>& b, std::size_t begin, std::size_t end, bool sff2,
                CasmChannel& out) {
  const std::size_t len = end > begin ? end - begin : 0;
  const std::size_t need = sff2 ? kCtb2MinLen : kCtabSff1MinLen;
  if (len < need) {
    return false;
  }
  out = CasmChannel{};
  out.source_channel = b[begin + kCtabSourceChannel];
  out.name = extract_name(b, begin + kCtabName, kCtabNameLen);
  out.destination_channel = b[begin + kCtabDestChannel];
  out.source_chord_root = b[begin + kCtabSourceRoot];
  out.source_chord_type = b[begin + kCtabSourceType];
  out.ntr = to_ntr(b[begin + kCtabNtr]);
  if (!sff2) {
    // SFF1 Ctab carries NTT, register limits and the retrigger rule inline.
    out.ntt = to_ntt(b[begin + kCtabNtt]);
    out.note_low = b[begin + kCtabNoteLow];
    out.note_high = b[begin + kCtabNoteHigh];
    out.retrigger = b[begin + kCtabRetrigger];
  }
  // The SFF2 Ctb2 stores NTT / limits in a richer per-chord-group sub-structure
  // that is NOT fully decoded here; the role-derived policy covers those files.
  return true;
}

// Splits a comma- or NUL-separated Sdec body into section names.
std::vector<std::string> parse_sdec(const std::vector<std::uint8_t>& b, std::size_t begin,
                                    std::size_t end) {
  std::vector<std::string> names;
  std::string current;
  for (std::size_t i = begin; i < end; ++i) {
    const std::uint8_t c = b[i];
    if (c == ',' || c == 0) {
      if (!current.empty()) {
        names.push_back(current);
      }
      current.clear();
    } else {
      current.push_back(static_cast<char>(c));
    }
  }
  if (!current.empty()) {
    names.push_back(current);
  }
  return names;
}

void parse_cseg(const std::vector<std::uint8_t>& b, std::size_t begin, std::size_t end,
                CasmSegment& seg) {
  std::size_t p = begin;
  while (p + kChunkHeaderSize <= end) {
    const std::uint32_t len = read_be32(b, p + 4);
    const std::size_t body = p + kChunkHeaderSize;
    const std::size_t body_end = body + len <= end ? body + len : end;
    if (tag_equals(b, p, "Sdec")) {
      seg.section_names = parse_sdec(b, body, body_end);
    } else if (tag_equals(b, p, "Ctab") || tag_equals(b, p, "Ctb2")) {
      CasmChannel channel;
      if (parse_ctab(b, body, body_end, tag_equals(b, p, "Ctb2"), channel)) {
        seg.channels.push_back(channel);
      }
    }
    // Other sub-chunks (Cntt, ...) are informational and skipped.
    if (len == 0) {
      break;
    }
    p = body + len;
  }
}

// ----------------------------------------------------------------------------
// Minimal, bounded SMF marker scanner (FF 06). Handles running status so that
// channel messages are skipped by the correct width.
// ----------------------------------------------------------------------------

constexpr std::size_t kSmfHeaderSize = 14;
constexpr std::uint8_t kStatusBit = 0x80;
constexpr std::uint8_t kMetaStatus = 0xFF;
constexpr std::uint8_t kSysexStart = 0xF0;
constexpr std::uint8_t kSysexEscape = 0xF7;
constexpr std::uint8_t kStatusHighNibble = 0xF0;
constexpr std::uint8_t kMetaMarker = 0x06;
constexpr std::uint8_t kNoteOff = 0x80;
constexpr std::uint8_t kNoteOn = 0x90;
constexpr std::uint8_t kPolyAftertouch = 0xA0;
constexpr std::uint8_t kControlChange = 0xB0;
constexpr std::uint8_t kProgramChange = 0xC0;
constexpr std::uint8_t kChannelAftertouch = 0xD0;
constexpr std::uint8_t kPitchBend = 0xE0;

bool read_vlq(const std::vector<std::uint8_t>& b, std::size_t& pos, std::size_t end,
              std::uint32_t& out) noexcept {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    if (pos >= end) {
      return false;
    }
    const std::uint8_t byte = b[pos++];
    value = (value << 7) | static_cast<std::uint32_t>(byte & 0x7F);
    if ((byte & 0x80) == 0) {
      out = value;
      return true;
    }
  }
  return false;
}

// Bytes of data following a channel status (0 = uses running status width 2/1).
int channel_data_len(std::uint8_t status) noexcept {
  switch (status & kStatusHighNibble) {
    case kNoteOff:
    case kNoteOn:
    case kPolyAftertouch:
    case kControlChange:
    case kPitchBend:
      return 2;
    case kProgramChange:
    case kChannelAftertouch:
      return 1;
    default:
      return -1;
  }
}

// Consumes one meta event (status 0xFF already read). Appends a StyleMarker
// when it is a marker meta. Returns false on a truncated/malformed event.
bool consume_meta(const std::vector<std::uint8_t>& b, std::size_t& p, std::size_t end,
                  std::uint32_t tick, std::vector<StyleMarker>& out) {
  if (p >= end) {
    return false;
  }
  const std::uint8_t meta = b[p++];
  std::uint32_t len = 0;
  if (!read_vlq(b, p, end, len) || p + len > end) {
    return false;
  }
  if (meta == kMetaMarker) {
    std::string name;
    for (std::uint32_t i = 0; i < len; ++i) {
      name.push_back(static_cast<char>(b[p + i]));
    }
    out.push_back(StyleMarker{.tick = tick, .name = name});
  }
  p += len;
  return true;
}

// Skips a length-prefixed (sysex) payload. Returns false on truncation.
bool skip_variable(const std::vector<std::uint8_t>& b, std::size_t& p, std::size_t end) {
  std::uint32_t len = 0;
  if (!read_vlq(b, p, end, len) || p + len > end) {
    return false;
  }
  p += len;
  return true;
}

// Reads the status byte at `p`, honouring running status. Returns false when no
// valid status is available.
bool read_status(const std::vector<std::uint8_t>& b, std::size_t& p, std::uint8_t& running,
                 std::uint8_t& status) {
  status = b[p];
  if ((status & kStatusBit) != 0) {
    ++p;
    running = status;
    return true;
  }
  status = running;
  return (status & kStatusBit) != 0;
}

void scan_track_markers(const std::vector<std::uint8_t>& b, std::size_t begin, std::size_t end,
                        std::vector<StyleMarker>& out) {
  std::size_t p = begin;
  std::uint32_t tick = 0;
  std::uint8_t running = 0;
  while (p < end) {
    std::uint32_t delta = 0;
    if (!read_vlq(b, p, end, delta)) {
      return;
    }
    tick += delta;
    std::uint8_t status = 0;
    if (p >= end || !read_status(b, p, running, status)) {
      return;
    }
    if (status == kMetaStatus) {
      if (!consume_meta(b, p, end, tick, out)) {
        return;
      }
      running = 0;
      continue;
    }
    if (status == kSysexStart || status == kSysexEscape) {
      if (!skip_variable(b, p, end)) {
        return;
      }
      running = 0;
      continue;
    }
    const int data_len = channel_data_len(status);
    if (data_len < 0) {
      return;
    }
    p += static_cast<std::size_t>(data_len);
  }
}

}  // namespace

bool decode_casm(const std::vector<std::uint8_t>& bytes, const std::string& source, CasmData& out,
                 Diagnostics& diag) {
  out = CasmData{};
  const std::size_t casm_off = find_marker(bytes, "CASM");
  if (casm_off == std::string::npos) {
    diag.info("no CASM chunk present in SFF file", source);
    return false;
  }
  out.present = true;
  out.sff2 = find_marker(bytes, "Ctb2") != std::string::npos ||
             find_marker(bytes, "Sff2") != std::string::npos;

  if (casm_off + kChunkHeaderSize > bytes.size()) {
    diag.warn("truncated CASM header", source);
    return false;
  }
  const std::uint32_t casm_len = read_be32(bytes, casm_off + 4);
  const std::size_t begin = casm_off + kChunkHeaderSize;
  const std::size_t end = begin + casm_len <= bytes.size() ? begin + casm_len : bytes.size();

  std::size_t p = begin;
  while (p + kChunkHeaderSize <= end) {
    const std::uint32_t len = read_be32(bytes, p + 4);
    const std::size_t body = p + kChunkHeaderSize;
    const std::size_t body_end = body + len <= end ? body + len : end;
    if (tag_equals(bytes, p, "CSEG")) {
      CasmSegment seg;
      parse_cseg(bytes, body, body_end, seg);
      if (!seg.channels.empty()) {
        out.segments.push_back(std::move(seg));
      }
    }
    if (len == 0) {
      break;
    }
    p = body + len;
  }

  if (out.segments.empty()) {
    diag.warn("CASM chunk present but no channel tables were decoded", source);
    return false;
  }
  return true;
}

std::vector<StyleMarker> scan_style_markers(const std::vector<std::uint8_t>& bytes) {
  std::vector<StyleMarker> markers;
  const std::size_t head = find_marker(bytes, "MThd");
  if (head == std::string::npos || head + kSmfHeaderSize > bytes.size()) {
    return markers;
  }
  std::size_t p = head + kSmfHeaderSize;
  while (p + kChunkHeaderSize <= bytes.size()) {
    if (!tag_equals(bytes, p, "MTrk")) {
      break;  // SFF tracks come first; stop at the first non-MTrk chunk.
    }
    const std::uint32_t len = read_be32(bytes, p + 4);
    const std::size_t body = p + kChunkHeaderSize;
    const std::size_t body_end = body + len <= bytes.size() ? body + len : bytes.size();
    scan_track_markers(bytes, body, body_end, markers);
    p = body + len;
  }
  return markers;
}

bool parse_style_section(const std::string& name, SectionKind& kind, SectionVariation& variation) {
  auto starts_with = [&name](const char* prefix) {
    const std::string p = prefix;
    return name.size() >= p.size() && name.compare(0, p.size(), p) == 0;
  };
  std::size_t rest = 0;
  if (starts_with("Intro")) {
    kind = SectionKind::kIntro;
    rest = 5;
  } else if (starts_with("Main")) {
    kind = SectionKind::kMain;
    rest = 4;
  } else if (starts_with("Fill In")) {
    kind = SectionKind::kFill;
    rest = 7;
  } else if (starts_with("Fill")) {
    kind = SectionKind::kFill;
    rest = 4;
  } else if (starts_with("Ending")) {
    kind = SectionKind::kEnding;
    rest = 6;
  } else if (starts_with("Break")) {
    kind = SectionKind::kBreak;
    variation = SectionVariation::kNone;
    return true;
  } else {
    return false;
  }
  variation = SectionVariation::kNone;
  for (std::size_t i = rest; i < name.size(); ++i) {
    const char c = name[i];
    if (c == 'A' || c == 'a') {
      variation = SectionVariation::kA;
      break;
    }
    if (c == 'B' || c == 'b') {
      variation = SectionVariation::kB;
      break;
    }
    if (c == 'C' || c == 'c') {
      variation = SectionVariation::kC;
      break;
    }
    if (c == 'D' || c == 'd') {
      variation = SectionVariation::kD;
      break;
    }
  }
  return true;
}

Role role_from_destination_channel(std::uint8_t destination_channel) {
  switch (destination_channel) {
    case 8:
      return Role::kPercussion;  // Rhythm Sub
    case 9:
      return Role::kDrums;  // Rhythm Main
    case 10:
      return Role::kBass;
    case 11:
      return Role::kChord1;
    case 12:
      return Role::kChord2;
    case 13:
      return Role::kPad;
    case 14:
      return Role::kPhrase;  // Phrase 1
    case 15:
      return Role::kLead;  // Phrase 2
    default:
      return Role::kUnassigned;
  }
}

// Exposed for the importer: turn a decoded source-chord root byte into a pitch
// class, clamped into 0..11 (C = 0).
std::uint8_t casm_root_pitch_class(std::uint8_t raw_root) noexcept {
  const std::uint8_t pc = raw_root & kPitchClassMask;
  return pc < kPitchClasses ? pc : 0;
}

}  // namespace arrstyle

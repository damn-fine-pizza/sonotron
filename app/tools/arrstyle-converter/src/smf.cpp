#include "smf.hpp"

#include <array>

namespace arrstyle {

namespace {

// Named MIDI constants (no magic numbers).
constexpr std::uint8_t kStatusBit = 0x80;
constexpr std::uint8_t kMetaStatus = 0xFF;
constexpr std::uint8_t kSysexStart = 0xF0;
constexpr std::uint8_t kSysexEscape = 0xF7;
constexpr std::uint8_t kNoteOff = 0x80;
constexpr std::uint8_t kNoteOn = 0x90;
constexpr std::uint8_t kPolyAftertouch = 0xA0;
constexpr std::uint8_t kControlChange = 0xB0;
constexpr std::uint8_t kProgramChange = 0xC0;
constexpr std::uint8_t kChannelAftertouch = 0xD0;
constexpr std::uint8_t kPitchBend = 0xE0;
constexpr std::uint8_t kStatusHighNibble = 0xF0;
constexpr std::uint8_t kChannelMask = 0x0F;

constexpr std::uint8_t kMetaTrackName = 0x03;
constexpr std::uint8_t kMetaTempo = 0x51;
constexpr std::uint8_t kMetaTimeSig = 0x58;

constexpr std::uint16_t kSmpteFlag = 0x8000;
constexpr std::uint64_t kMilliBpmScale = 60'000'000'000ULL;
constexpr std::uint32_t kDefaultTempoMicros = 500'000;  // 120 BPM

// A cursor over the byte buffer that never reads past the end.
class Cursor {
 public:
  Cursor(const std::vector<std::uint8_t>& bytes, std::size_t begin, std::size_t end)
      : m_bytes(bytes), m_pos(begin), m_end(end) {}

  bool at_end() const noexcept { return m_pos >= m_end; }
  std::size_t remaining() const noexcept { return m_end > m_pos ? m_end - m_pos : 0; }
  std::size_t pos() const noexcept { return m_pos; }

  bool read_u8(std::uint8_t& out) noexcept {
    if (m_pos >= m_end) {
      return false;
    }
    out = m_bytes[m_pos++];
    return true;
  }

  bool read_u16(std::uint16_t& out) noexcept {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    if (!read_u8(a) || !read_u8(b)) {
      return false;
    }
    out = static_cast<std::uint16_t>((a << 8) | b);
    return true;
  }

  bool read_u32(std::uint32_t& out) noexcept {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t c = 0;
    std::uint8_t d = 0;
    if (!read_u8(a) || !read_u8(b) || !read_u8(c) || !read_u8(d)) {
      return false;
    }
    out = (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(b) << 16) |
          (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
    return true;
  }

  // Variable-length quantity (7 bits per byte, high bit = continue).
  bool read_vlq(std::uint32_t& out) noexcept {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      std::uint8_t byte = 0;
      if (!read_u8(byte)) {
        return false;
      }
      value = (value << 7) | static_cast<std::uint32_t>(byte & 0x7F);
      if ((byte & 0x80) == 0) {
        out = value;
        return true;
      }
    }
    return false;  // more than 4 bytes is malformed
  }

  bool skip(std::size_t n) noexcept {
    if (m_pos + n > m_end) {
      m_pos = m_end;
      return false;
    }
    m_pos += n;
    return true;
  }

  std::uint8_t peek() const noexcept { return m_pos < m_end ? m_bytes[m_pos] : 0; }

 private:
  const std::vector<std::uint8_t>& m_bytes;
  std::size_t m_pos;
  std::size_t m_end;
};

std::uint32_t micros_to_milli_bpm(std::uint32_t micros_per_quarter) noexcept {
  if (micros_per_quarter == 0) {
    return kMilliBpmScale / kDefaultTempoMicros;
  }
  return static_cast<std::uint32_t>(kMilliBpmScale / micros_per_quarter);
}

// One open Note On awaiting its Note Off, keyed by (channel, note).
struct PendingNote {
  bool active = false;
  std::uint32_t tick = 0;
  std::uint8_t velocity = 0;
};

using NoteTable = std::array<PendingNote, 128 * 16>;

std::size_t note_index(std::uint8_t channel, std::uint8_t note) noexcept {
  return static_cast<std::size_t>(channel) * 128 + note;
}

// Emit a completed note (a Note On paired with its Note Off / velocity-0).
void emit_note(SmfTrack& track, NoteTable& pending, std::uint8_t channel, std::uint8_t note,
               std::uint32_t end_tick) {
  const std::size_t idx = note_index(channel, note);
  if (!pending[idx].active) {
    return;
  }
  track.notes.push_back(SmfNote{.tick = pending[idx].tick,
                                .channel = channel,
                                .note = note,
                                .velocity = pending[idx].velocity,
                                .gate = end_tick - pending[idx].tick});
  pending[idx].active = false;
}

// Handles a meta event (status byte already consumed). Returns false on error.
bool handle_meta(Cursor& cursor, SmfFile& file, SmfTrack& track, Diagnostics& diag,
                 const std::string& source) {
  std::uint8_t meta_type = 0;
  std::uint32_t len = 0;
  if (!cursor.read_u8(meta_type) || !cursor.read_vlq(len)) {
    diag.error("truncated meta event", source);
    return false;
  }
  if (cursor.remaining() < len) {
    diag.error("meta event length exceeds track", source);
    return false;
  }
  if (meta_type == kMetaTrackName) {
    for (std::size_t i = 0; i < len; ++i) {
      std::uint8_t ch = 0;
      cursor.read_u8(ch);
      track.name.push_back(static_cast<char>(ch));
    }
  } else if (meta_type == kMetaTempo && len == 3) {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t c = 0;
    cursor.read_u8(a);
    cursor.read_u8(b);
    cursor.read_u8(c);
    const std::uint32_t micros = (static_cast<std::uint32_t>(a) << 16) |
                                 (static_cast<std::uint32_t>(b) << 8) |
                                 static_cast<std::uint32_t>(c);
    file.tempo_milli_bpm = micros_to_milli_bpm(micros);
    file.has_tempo = true;
  } else if (meta_type == kMetaTimeSig && len >= 2) {
    std::uint8_t nn = 0;
    std::uint8_t dd = 0;
    cursor.read_u8(nn);
    cursor.read_u8(dd);
    cursor.skip(len - 2);
    file.time_sig_num = nn;
    file.time_sig_den = static_cast<std::uint8_t>(1u << dd);
    file.has_time_sig = true;
  } else {
    cursor.skip(len);
  }
  return true;
}

// Handles one channel-voice message (status byte already consumed). Notes are
// paired; every other class is counted. Returns false on a truncated message.
bool handle_channel(Cursor& cursor, SmfFile& file, SmfTrack& track, NoteTable& pending,
                    std::uint8_t status, std::uint32_t abs_tick, Diagnostics& diag,
                    const std::string& source) {
  const std::uint8_t high = status & kStatusHighNibble;
  const std::uint8_t channel = status & kChannelMask;
  if (high == kNoteOn || high == kNoteOff) {
    std::uint8_t note = 0;
    std::uint8_t vel = 0;
    if (!cursor.read_u8(note) || !cursor.read_u8(vel)) {
      diag.error("truncated note event", source);
      return false;
    }
    note &= 0x7F;
    // Note On with velocity 0 is a Note Off.
    if (high == kNoteOn && vel > 0) {
      pending[note_index(channel, note)] =
          PendingNote{.active = true, .tick = abs_tick, .velocity = vel};
    } else {
      emit_note(track, pending, channel, note, abs_tick);
    }
    return true;
  }
  switch (high) {
    case kPolyAftertouch:
      cursor.skip(2);
      ++file.dropped_aftertouch;
      return true;
    case kControlChange:
      cursor.skip(2);
      ++file.dropped_cc;
      return true;
    case kProgramChange:
      cursor.skip(1);
      ++file.dropped_program_change;
      return true;
    case kChannelAftertouch:
      cursor.skip(1);
      ++file.dropped_aftertouch;
      return true;
    case kPitchBend:
      cursor.skip(2);
      ++file.dropped_pitch_bend;
      return true;
    default:
      diag.error("unrecognized status byte", source);
      return false;
  }
}

// Close every Note On still open at end-of-track (gate clamped to the end).
void flush_open_notes(SmfTrack& track, SmfFile& file, NoteTable& pending, std::uint32_t abs_tick) {
  for (std::uint16_t channel = 0; channel < 16; ++channel) {
    for (std::uint16_t note = 0; note < 128; ++note) {
      const std::size_t idx =
          note_index(static_cast<std::uint8_t>(channel), static_cast<std::uint8_t>(note));
      if (!pending[idx].active) {
        continue;
      }
      const std::uint32_t end = abs_tick > pending[idx].tick ? abs_tick : pending[idx].tick + 1;
      emit_note(track, pending, static_cast<std::uint8_t>(channel), static_cast<std::uint8_t>(note),
                end);
      ++file.unmatched_note_on;
    }
  }
}

bool parse_track(Cursor& cursor, SmfTrack& track, SmfFile& file, Diagnostics& diag,
                 const std::string& source) {
  NoteTable pending{};  // [channel*128 + note]
  std::uint32_t abs_tick = 0;
  std::uint8_t running_status = 0;

  while (!cursor.at_end()) {
    std::uint32_t delta = 0;
    if (!cursor.read_vlq(delta)) {
      diag.error("truncated delta-time in track", source);
      return false;
    }
    abs_tick += delta;

    std::uint8_t status = cursor.peek();
    if ((status & kStatusBit) != 0) {
      cursor.read_u8(status);
      running_status = status;
    } else {
      status = running_status;  // running status: reuse the last channel status
      if ((status & kStatusBit) == 0) {
        diag.error("data byte with no running status", source);
        return false;
      }
    }

    if (status == kMetaStatus) {
      if (!handle_meta(cursor, file, track, diag, source)) {
        return false;
      }
      running_status = 0;  // meta clears running status
      continue;
    }
    if (status == kSysexStart || status == kSysexEscape) {
      std::uint32_t len = 0;
      if (!cursor.read_vlq(len) || !cursor.skip(len)) {
        diag.error("truncated sysex event", source);
        return false;
      }
      ++file.dropped_sysex;
      running_status = 0;
      continue;
    }
    if (!handle_channel(cursor, file, track, pending, status, abs_tick, diag, source)) {
      return false;
    }
    if (abs_tick > file.total_ticks) {
      file.total_ticks = abs_tick;
    }
  }

  flush_open_notes(track, file, pending, abs_tick);
  return true;
}

}  // namespace

bool parse_smf(const std::vector<std::uint8_t>& bytes, const std::string& source, SmfFile& out,
               Diagnostics& diag) {
  constexpr std::size_t kHeaderChunkSize = 14;
  if (bytes.size() < kHeaderChunkSize) {
    diag.error("file too small to be a Standard MIDI File", source);
    return false;
  }
  if (bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != 'h' || bytes[3] != 'd') {
    diag.error("missing MThd header", source);
    return false;
  }

  Cursor header(bytes, 4, bytes.size());
  std::uint32_t header_len = 0;
  header.read_u32(header_len);
  if (header_len < 6) {
    diag.error("invalid MThd length", source);
    return false;
  }
  header.read_u16(out.format);
  header.read_u16(out.ntrks);
  header.read_u16(out.division);
  // Skip any extra header bytes beyond the standard six.
  header.skip(header_len - 6);

  if ((out.division & kSmpteFlag) != 0) {
    diag.error("SMPTE time division is not supported (need ticks-per-quarter)", source);
    return false;
  }
  if (out.division == 0) {
    diag.error("zero time division", source);
    return false;
  }

  out.tempo_milli_bpm = micros_to_milli_bpm(kDefaultTempoMicros);

  std::size_t pos = header.pos();
  std::uint16_t parsed = 0;
  while (pos + 8 <= bytes.size() && parsed < out.ntrks) {
    if (bytes[pos] != 'M' || bytes[pos + 1] != 'T' || bytes[pos + 2] != 'r' ||
        bytes[pos + 3] != 'k') {
      diag.warn("skipping non-MTrk chunk", source);
      // Read this chunk's length and skip it (also covers SFF CASM chunks).
      Cursor chunk(bytes, pos + 4, bytes.size());
      std::uint32_t len = 0;
      if (!chunk.read_u32(len)) {
        break;
      }
      pos = chunk.pos() + len;
      continue;
    }
    Cursor len_cursor(bytes, pos + 4, bytes.size());
    std::uint32_t track_len = 0;
    if (!len_cursor.read_u32(track_len)) {
      diag.error("truncated track header", source);
      return false;
    }
    const std::size_t track_begin = len_cursor.pos();
    const std::size_t track_end =
        track_begin + track_len <= bytes.size() ? track_begin + track_len : bytes.size();
    Cursor track_cursor(bytes, track_begin, track_end);
    SmfTrack track;
    if (!parse_track(track_cursor, track, out, diag, source)) {
      return false;
    }
    out.tracks.push_back(std::move(track));
    pos = track_begin + track_len;
    ++parsed;
  }

  if (out.tracks.empty()) {
    diag.error("no track chunks found", source);
    return false;
  }
  return true;
}

}  // namespace arrstyle

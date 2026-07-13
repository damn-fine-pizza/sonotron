#include "midisrc/smf_write.hpp"

namespace arrstyle {

namespace {

constexpr std::uint8_t kMetaStatus = 0xFF;
constexpr std::uint8_t kMetaEndOfTrack = 0x2F;

void push_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void push_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

// Variable-length quantity: 7 bits per byte, high bit set on every byte but
// the last (the mirror of Cursor::read_vlq in smf.cpp).
void push_vlq(std::vector<std::uint8_t>& out, std::uint32_t value) {
  std::uint8_t buf[5];
  int n = 0;
  buf[n++] = static_cast<std::uint8_t>(value & 0x7F);
  value >>= 7;
  while (value > 0) {
    buf[n++] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
    value >>= 7;
  }
  for (int i = n - 1; i >= 0; --i) {
    out.push_back(buf[i]);
  }
}

// One event's wire bytes (status + its data bytes, per MidiMessage::wire_length()).
void push_message(std::vector<std::uint8_t>& out, const arrangrr::MidiMessage& msg) {
  out.push_back(msg.status);
  const int len = msg.wire_length();
  if (len >= 2) {
    out.push_back(msg.d1);
  }
  if (len >= 3) {
    out.push_back(msg.d2);
  }
}

}  // namespace

std::vector<std::uint8_t> write_smf(const std::vector<SmfEvent>& events, std::uint16_t division) {
  std::vector<std::uint8_t> track;
  arrangrr::Tick prev_tick = 0;
  for (const SmfEvent& ev : events) {
    // System Realtime bytes (Clock/Start/Continue/Stop/...) have no place in
    // a Standard MIDI File track -- playback regenerates transport, it is
    // never stored as bytes -- and the existing parser does not model them as
    // a channel-voice message either (they would fail to round-trip through
    // parse_smf). A clock-out-enabled live session's OutEvent::midi stream
    // carries these; skip them here rather than at every producer.
    if (arrangrr::midi::is_realtime(ev.msg.status)) {
      continue;
    }
    const std::uint32_t delta = ev.tick >= prev_tick ? ev.tick - prev_tick : 0;
    push_vlq(track, delta);
    push_message(track, ev.msg);
    prev_tick = ev.tick;
  }
  // End-of-Track meta (delta 0, FF 2F 00).
  push_vlq(track, 0);
  track.push_back(kMetaStatus);
  track.push_back(kMetaEndOfTrack);
  push_vlq(track, 0);

  std::vector<std::uint8_t> out;
  out.reserve(14 + 8 + track.size());

  // MThd: "MThd" + length(6, u32 BE) + format(u16) + ntrks(u16) + division(u16).
  out.push_back('M');
  out.push_back('T');
  out.push_back('h');
  out.push_back('d');
  push_u32(out, 6);
  push_u16(out, 0);  // format 0: a single multi-channel track
  push_u16(out, 1);  // ntrks
  push_u16(out, division);

  // MTrk: "MTrk" + length(u32 BE) + track bytes.
  out.push_back('M');
  out.push_back('T');
  out.push_back('r');
  out.push_back('k');
  push_u32(out, static_cast<std::uint32_t>(track.size()));
  out.insert(out.end(), track.begin(), track.end());

  return out;
}

}  // namespace arrstyle

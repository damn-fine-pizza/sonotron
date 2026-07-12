#include <initializer_list>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/midi/parser.hpp"
#include "arrangrr/routing/note_tracker.hpp"
#include "arrangrr/routing/router.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Msgs = StaticVector<MidiMessage, 64>;

Msgs parse(std::initializer_list<std::uint8_t> bytes) {
  MidiParser p;
  Msgs out;
  for (std::uint8_t b : bytes) {
    p.feed(b, [&](const MidiMessage& m) { CHECK(out.push_back(m)); });
  }
  return out;
}

void test_parser_basic_and_running_status() {
  // NoteOn ch1, then running status for two more notes.
  const Msgs m = parse({0x90, 60, 100, 62, 90, 64, 80});
  CHECK(m.size() == 3);
  CHECK(m[0].type() == midi::kNoteOn && m[0].d1 == 60 && m[0].d2 == 100);
  CHECK(m[1].d1 == 62 && m[1].d2 == 90);
  CHECK(m[2].d1 == 64 && m[2].d2 == 80);
}

void test_parser_noteon_v0_normalized_to_noteoff() {
  const Msgs m = parse({0x90, 60, 0});
  CHECK(m.size() == 1);
  CHECK(m[0].type() == midi::kNoteOff && m[0].d1 == 60);
}

void test_parser_realtime_interleaved_preserves_running_status() {
  // F8 arrives mid-message: must pass through and not break assembly.
  const Msgs m = parse({0x90, 60, 0xF8, 100, 0xF8, 62, 101});
  CHECK(m.size() == 4);
  CHECK(m[0].status == midi::kClock);
  CHECK(m[1].type() == midi::kNoteOn && m[1].d1 == 60 && m[1].d2 == 100);
  CHECK(m[2].status == midi::kClock);
  CHECK(m[3].type() == midi::kNoteOn && m[3].d1 == 62);  // running status survived
}

void test_parser_sysex_skipped_safely() {
  // SysEx payload dropped; message after it parses; running status cleared.
  const Msgs m = parse({0xF0, 1, 2, 3, 0xF7, 0x80, 60, 64});
  CHECK(m.size() == 1);
  CHECK(m[0].type() == midi::kNoteOff);
  // Interrupted SysEx (no F7) terminated by next status byte.
  const Msgs n = parse({0xF0, 1, 2, 0x90, 61, 99});
  CHECK(n.size() == 1);
  CHECK(n[0].type() == midi::kNoteOn && n[0].d1 == 61);
}

void test_parser_orphan_data_dropped() {
  const Msgs m = parse({42, 60, 0x90, 60, 100});
  CHECK(m.size() == 1);
  CHECK(m[0].type() == midi::kNoteOn);
}

void test_parser_all_branches() {
  // Drives a single parser through every decision branch of feed() in one
  // stateful stream: realtime pass-through, SysEx open/skip/close, status
  // terminating a dangling SysEx, running-status re-arm, orphan drop,
  // NoteOn-v0 normalization, zero/one/two data-byte messages, and reset.
  MidiParser p;
  Msgs out;
  auto sink = [&](const MidiMessage& m) { CHECK(out.push_back(m)); };
  const auto feed = [&](std::initializer_list<std::uint8_t> bytes) {
    for (std::uint8_t b : bytes) {
      p.feed(b, sink);
    }
  };

  feed({0xF8});  // realtime standalone -> passes through
  CHECK(out.size() == 1 && out[0].status == midi::kClock);

  out.clear();
  feed({0x90, 60, 100});  // channel voice, 2 data bytes
  feed({62, 90});         // running status re-arms last channel status
  feed({0x90, 64, 0});    // NoteOn velocity 0 -> NoteOff
  CHECK(out.size() == 3);
  CHECK(out[0].type() == midi::kNoteOn && out[0].d2 == 100);
  CHECK(out[1].type() == midi::kNoteOn && out[1].d1 == 62);
  CHECK(out[2].type() == midi::kNoteOff && out[2].d1 == 64);

  out.clear();
  feed({0xC3, 42});  // Program Change: single data byte (needed == 1)
  feed({43});        // running status on a 1-byte message
  CHECK(out.size() == 2);
  CHECK(out[0].type() == midi::kProgramChange && out[0].d1 == 42);
  CHECK(out[1].type() == midi::kProgramChange && out[1].d1 == 43);

  out.clear();
  feed({0xF2, 0x10, 0x02});  // system common: clears running status
  feed({77});                // orphan data byte, no running status -> dropped
  feed({0xF6});              // Tune Request: zero data bytes, emitted at once
  CHECK(out.size() == 2);
  CHECK(out[0].status == midi::kSongPosition);
  CHECK(out[1].status == midi::kTuneRequest);

  out.clear();
  feed({0xF0, 1, 2, 3});  // SysEx open; payload bytes skipped
  feed({0xF8});           // realtime inside SysEx: passes, state untouched
  feed({0xF7});           // EOX closes SysEx
  feed({0x80, 60, 64});   // NoteOff after SysEx (type != NoteOn)
  CHECK(out.size() == 2);
  CHECK(out[0].status == midi::kClock);
  CHECK(out[1].type() == midi::kNoteOff && out[1].d1 == 60);

  out.clear();
  feed({0xF0, 9});       // SysEx open + one payload byte
  feed({0x90, 61, 99});  // a status byte terminates the dangling SysEx
  CHECK(out.size() == 1 && out[0].type() == midi::kNoteOn && out[0].d1 == 61);

  out.clear();
  feed({0x90, 60});  // half a message pending
  p.reset();
  feed({100});  // orphan after reset -> dropped
  CHECK(out.empty());
}

void test_router_filters_and_remap() {
  Router r;
  CHECK(r.add(Route{
      .in_port = 0, .in_channel = 0, .out_port = 1, .out_channel = 4, .pass = route_pass::kNotes}));
  int hits = 0;
  MidiMessage got{};
  std::uint8_t got_port = 0;
  auto sink = [&](std::uint8_t p, const MidiMessage& m) {
    ++hits;
    got_port = p;
    got = m;
  };
  r.route(0, MidiMessage::note_on(0, 60, 100), sink);
  CHECK(hits == 1 && got_port == 1 && got.channel() == 4 && got.d1 == 60);
  r.route(0, MidiMessage::note_on(3, 60, 100), sink);  // wrong channel
  CHECK(hits == 1);
  r.route(0, MidiMessage::cc(0, 7, 1), sink);  // CC filtered out
  CHECK(hits == 1);
  r.route(1, MidiMessage::note_on(0, 60, 100), sink);  // wrong port
  CHECK(hits == 1);
}

void test_note_tracker_panic_with_sustain() {
  NoteTracker t;
  // Note held by pedal: NoteOn, pedal down, NoteOff -> still sounding.
  t.observe(0, MidiMessage::note_on(2, 60, 100));
  t.observe(0, MidiMessage::cc(2, midi::kCcSustain, 127));
  t.observe(0, MidiMessage::note_off(2, 60));
  CHECK(t.any_sounding(0, 2));
  // Plus a plainly-held note on another channel.
  t.observe(0, MidiMessage::note_on(5, 72, 100));

  StaticVector<MidiMessage, 16> off;
  t.panic([&](std::uint8_t port, const MidiMessage& m) {
    CHECK(port == 0);
    CHECK(off.push_back(m));
  });
  // ch2: NoteOff 60 + 3 mode CCs; ch5: NoteOff 72 + 3 mode CCs.
  CHECK(off.size() == 8);
  CHECK(off[0].type() == midi::kNoteOff && off[0].d1 == 60 && off[0].channel() == 2);
  CHECK(off[1].d1 == midi::kCcAllNotesOff);
  CHECK(off[4].type() == midi::kNoteOff && off[4].d1 == 72 && off[4].channel() == 5);
  CHECK(!t.any_sounding(0, 2) && !t.any_sounding(0, 5));
}

void test_parser_system_common() {
  // Song Position (F2, 2 data), Song Select (F3, 1 data), Tune Request (F6).
  const Msgs m = parse({0xF2, 0x10, 0x02, 0xF3, 5, 0xF6});
  CHECK(m.size() == 3);
  CHECK(m[0].status == midi::kSongPosition && m[0].d1 == 0x10 && m[0].d2 == 0x02);
  CHECK(m[1].status == midi::kSongSelect && m[1].d1 == 5);
  CHECK(m[2].status == midi::kTuneRequest);
  // System common must clear running status: the dangling data byte is orphan.
  const Msgs n = parse({0x90, 60, 100, 0xF6, 61});
  CHECK(n.size() == 2);  // NoteOn + TuneRequest, orphan 61 dropped
}

void test_parser_one_data_byte_messages() {
  const Msgs m = parse({0xC3, 42, 0xD2, 100});
  CHECK(m.size() == 2);
  CHECK(m[0].type() == midi::kProgramChange && m[0].channel() == 3 && m[0].d1 == 42);
  CHECK(m[1].type() == midi::kChannelPressure && m[1].channel() == 2 && m[1].d1 == 100);
}

void test_parser_reset() {
  MidiParser p;
  Msgs out;
  auto sink = [&](const MidiMessage& m) { CHECK(out.push_back(m)); };

  // Half a channel message, then reset: the trailing data byte is an orphan.
  p.feed(0x90, sink);
  p.feed(60, sink);  // half a message
  p.reset();
  p.feed(100, sink);  // orphan after reset: dropped
  CHECK(out.empty());
  const std::uint8_t rest[] = {0x80, 60, 0};
  p.feed(rest, 3, sink);  // NoteOff parses cleanly (type != NoteOn path)
  CHECK(out.size() == 1 && out[0].type() == midi::kNoteOff);

  // One stream that walks the same parser through realtime pass-through,
  // running status, NoteOn-v0 normalization, an EOX-terminated SysEx and a
  // system-common message that clears running status.
  out.clear();
  const std::uint8_t stream[] = {
      0x90, 62,   100,   // NoteOn, arms running status 0x90
      64,   90,          // running status -> second NoteOn
      66,   0,           // running status -> NoteOn v0 normalized to NoteOff
      0xF8,              // realtime passes through, parsing state untouched
      0xF0, 0x01, 0xF7,  // SysEx: payload skipped, EOX closes it
      0xF2, 0x10, 0x02,  // system common clears running status
      0xF6,              // Tune Request: zero data bytes, emitted immediately
  };
  p.feed(stream, sizeof(stream), sink);
  CHECK(out.size() == 6);
  CHECK(out[0].type() == midi::kNoteOn && out[0].d1 == 62);
  CHECK(out[1].type() == midi::kNoteOn && out[1].d1 == 64);   // running status
  CHECK(out[2].type() == midi::kNoteOff && out[2].d1 == 66);  // v0 -> NoteOff
  CHECK(out[3].status == midi::kClock);
  CHECK(out[4].status == midi::kSongPosition);
  CHECK(out[5].status == midi::kTuneRequest);

  // reset() also clears an open SysEx: the payload byte after reset is an
  // orphan (not swallowed), and a later NoteOn parses normally.
  out.clear();
  p.feed(0xF0, sink);  // SysEx opens
  p.feed(0x7F, sink);  // payload byte
  p.reset();
  p.feed(0x7E, sink);  // orphan after reset
  const std::uint8_t after[] = {0x90, 65, 90};
  p.feed(after, 3, sink);
  CHECK(out.size() == 1 && out[0].type() == midi::kNoteOn && out[0].d1 == 65);
}

void test_note_tracker_high_notes_and_bounds() {
  NoteTracker t;
  t.observe(0, MidiMessage::note_on(0, 100, 90));  // second bitmap word
  t.observe(0, MidiMessage::note_on(0, 5, 90));
  CHECK(t.any_sounding(0, 0));
  // Out-of-range port/channel are ignored gracefully.
  t.observe(7, MidiMessage::note_on(0, 60, 90));
  CHECK(!t.any_sounding(7, 0));
  CHECK(!t.any_sounding(0, 16));

  StaticVector<MidiMessage, 8> out;
  t.panic([&](std::uint8_t, const MidiMessage& m) { CHECK(out.push_back(m)); });
  CHECK(out.size() == 5);  // NoteOff 5, NoteOff 100, then 3 CCs
  CHECK(out[0].d1 == 5 && out[1].d1 == 100);
}

void test_parser_more_edges() {
  // Orphan SysEx end (no start): ignored.
  const Msgs a = parse({0xF7, 0x90, 60, 100});
  CHECK(a.size() == 1 && a[0].type() == midi::kNoteOn);
  // Realtime inside a SysEx payload passes; payload still skipped.
  const Msgs b = parse({0xF0, 1, 0xF8, 2, 0xF7});
  CHECK(b.size() == 1 && b[0].status == midi::kClock);
  // MTC quarter frame (F1, 1 data byte).
  const Msgs c = parse({0xF1, 0x23});
  CHECK(c.size() == 1 && c[0].status == midi::kMtcQuarterFrame && c[0].d1 == 0x23);
  // Poly pressure and pitch bend (2 data bytes each).
  const Msgs d = parse({0xA1, 60, 55, 0xE2, 0x00, 0x40});
  CHECK(d.size() == 2);
  CHECK(d[0].type() == midi::kPolyPressure && d[0].channel() == 1);
  CHECK(d[1].type() == midi::kPitchBend && d[1].channel() == 2);
  // CC with value 0 must NOT be normalized (only NoteOn v0 is).
  const Msgs e = parse({0xB0, 64, 0});
  CHECK(e.size() == 1 && e[0].type() == midi::kControlChange && e[0].d2 == 0);
}

void test_parser_eox_terminates_and_kills_running_status() {
  // EOX (F7) is System Common: it aborts the message being assembled...
  const Msgs a = parse({0x90, 60, 0xF7, 64});
  CHECK(a.size() == 0);  // no phantom NoteOn assembled across the F7
  // ...and clears running status: data after it is orphan, not a new note.
  const Msgs b = parse({0x90, 60, 100, 0xF7, 62, 100});
  CHECK(b.size() == 1);
  CHECK(b[0].type() == midi::kNoteOn && b[0].d1 == 60);
}

void test_message_wire_lengths() {
  CHECK(MidiMessage::note_on(0, 60, 1).wire_length() == 3);
  CHECK((MidiMessage{0xC0, 1, 0}.wire_length() == 2));
  CHECK((MidiMessage{0xD0, 1, 0}.wire_length() == 2));
  CHECK((MidiMessage{0xE0, 0, 0x40}.wire_length() == 3));
  CHECK((MidiMessage{midi::kSongPosition, 0, 0}.wire_length() == 3));
  CHECK((MidiMessage{midi::kSongSelect, 0, 0}.wire_length() == 2));
  CHECK((MidiMessage{midi::kMtcQuarterFrame, 0, 0}.wire_length() == 2));
  CHECK((MidiMessage{midi::kTuneRequest, 0, 0}.wire_length() == 1));
  CHECK(MidiMessage::realtime(midi::kClock).wire_length() == 1);
  CHECK(midi::data_length(midi::kSysExStart) == -1);
}



void test_router_realtime_and_system() {
  Router r;
  CHECK(
      r.add(Route{.in_port = 0,
                  .in_channel = -1,
                  .out_port = 0,
                  .out_channel = -1,
                  .pass = static_cast<std::uint8_t>(route_pass::kRealtime | route_pass::kSystem)}));
  int hits = 0;
  auto sink = [&](std::uint8_t, const MidiMessage&) { ++hits; };
  r.route(0, MidiMessage::realtime(midi::kClock), sink);
  CHECK(hits == 1);
  r.route(0, MidiMessage{midi::kSongPosition, 1, 2}, sink);
  CHECK(hits == 2);
  r.route(0, MidiMessage::note_on(0, 60, 1), sink);  // notes filtered out
  CHECK(hits == 2);
  // Pitch bend and program classes.
  Router r2;
  CHECK(r2.add(
      Route{.in_port = 0,
            .in_channel = -1,
            .out_port = 0,
            .out_channel = -1,
            .pass = static_cast<std::uint8_t>(route_pass::kPitchBend | route_pass::kProgram)}));
  hits = 0;
  r2.route(0, MidiMessage{0xE0, 0, 0x40}, sink);
  r2.route(0, MidiMessage{0xC0, 5, 0}, sink);
  r2.route(0, MidiMessage::cc(0, 7, 1), sink);  // CC filtered
  CHECK(hits == 2);
}

void test_note_tracker_pedal_release_clears() {
  NoteTracker t;
  t.observe(0, MidiMessage::note_on(0, 60, 100));
  t.observe(0, MidiMessage::cc(0, midi::kCcSustain, 127));
  t.observe(0, MidiMessage::note_off(0, 60));
  CHECK(t.any_sounding(0, 0));
  t.observe(0, MidiMessage::cc(0, midi::kCcSustain, 0));  // pedal up -> synth releases
  CHECK(!t.any_sounding(0, 0));
}

}  // namespace

int main() {
  test_parser_basic_and_running_status();
  test_parser_noteon_v0_normalized_to_noteoff();
  test_parser_realtime_interleaved_preserves_running_status();
  test_parser_sysex_skipped_safely();
  test_parser_orphan_data_dropped();
  test_parser_system_common();
  test_parser_one_data_byte_messages();
  test_parser_reset();
  test_parser_more_edges();
  test_parser_eox_terminates_and_kills_running_status();
  test_parser_all_branches();
  test_message_wire_lengths();
  test_router_filters_and_remap();
  test_router_realtime_and_system();
  test_note_tracker_panic_with_sustain();
  test_note_tracker_high_notes_and_bounds();
  test_note_tracker_pedal_release_clears();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_midi: all OK\n");
  }
  return arrangrr::test::failures();
}

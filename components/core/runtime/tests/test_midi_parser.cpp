// Unit tests for MidiParser (components/core/runtime/midi_parser.hpp): a pure
// byte-stream -> MidiMessage parser, one instance per input port. Pure,
// freestanding, no Engine/Runtime.
//
// Phase 6 Theme 1b (Torquato QA, coverage-gate restoration): this file was
// entirely missing before -- midi_parser.hpp sat at 57% line coverage under
// the unit gate despite being the single most unit-testable file in the
// runtime/ core scope (a pure state machine over bytes, DESIGN.md §9.A).
// Covers: running status, realtime interleaving mid-message, system-common
// resetting running status, NoteOn-vel-0 normalization (both fresh-status
// and running-status paths), SysEx skip + interruption-robustness, orphan
// data bytes, a zero-data-byte status (immediate emission), the multi-byte
// feed() overload, and reset().

#include "runtime/midi_parser.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

struct Sink {
  MidiMessage last{};
  int calls = 0;
  void operator()(const MidiMessage& m) {
    last = m;
    ++calls;
  }
};

void test_simple_note_on_three_bytes() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 3), sink);  // channel 3
  CHECK(sink.calls == 0);  // status alone: message not complete yet
  p.feed(60, sink);
  CHECK(sink.calls == 0);
  p.feed(100, sink);
  CHECK(sink.calls == 1);
  CHECK(sink.last.type() == midi::kNoteOn);
  CHECK(sink.last.channel() == 3);
  CHECK(sink.last.d1 == 60 && sink.last.d2 == 100);
}

void test_running_status_reuses_last_channel_voice_status() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 2), sink);
  p.feed(64, sink);
  p.feed(90, sink);
  CHECK(sink.calls == 1);
  // No new status byte: running status re-arms the SAME NoteOn on channel 2.
  p.feed(67, sink);
  p.feed(80, sink);
  CHECK(sink.calls == 2);
  CHECK(sink.last.type() == midi::kNoteOn);
  CHECK(sink.last.channel() == 2);
  CHECK(sink.last.d1 == 67 && sink.last.d2 == 80);
}

void test_realtime_bytes_interleave_without_disturbing_the_running_message() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);
  CHECK(sink.calls == 0);  // d2 not received yet
  // A realtime clock byte arrives MID-MESSAGE (D9.A §24.7 nit).
  p.feed(midi::kClock, sink);
  CHECK(sink.calls == 1);
  CHECK(sink.last.status == midi::kClock);
  // The dangling NoteOn is untouched: completing it now still works.
  p.feed(100, sink);
  CHECK(sink.calls == 2);
  CHECK(sink.last.type() == midi::kNoteOn);
  CHECK(sink.last.d1 == 60 && sink.last.d2 == 100);
}

void test_note_on_velocity_zero_normalizes_to_note_off_fresh_status() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 5), sink);
  p.feed(72, sink);
  p.feed(0, sink);  // velocity 0
  CHECK(sink.calls == 1);
  CHECK(sink.last.type() == midi::kNoteOff);
  CHECK(sink.last.channel() == 5);
  CHECK(sink.last.d1 == 72);
  CHECK(sink.last.d2 == 0);
}

void test_note_on_velocity_zero_normalizes_via_running_status_too() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 1), sink);
  p.feed(50, sink);
  p.feed(60, sink);
  CHECK(sink.calls == 1 && sink.last.type() == midi::kNoteOn);
  // Second note, same running status, vel 0 -- must STILL normalize.
  p.feed(51, sink);
  p.feed(0, sink);
  CHECK(sink.calls == 2);
  CHECK(sink.last.type() == midi::kNoteOff);
  CHECK(sink.last.d1 == 51 && sink.last.d2 == 0);
}

void test_system_common_status_resets_running_status() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);
  p.feed(100, sink);
  CHECK(sink.calls == 1);  // running status now armed to this NoteOn
  // Song Position (system common, 2 data bytes) resets running status.
  p.feed(midi::kSongPosition, sink);
  p.feed(1, sink);
  p.feed(2, sink);
  CHECK(sink.calls == 2);
  CHECK(sink.last.status == midi::kSongPosition);
  // An orphan data byte now MUST be dropped -- running status was cleared,
  // not left pointing at the old NoteOn.
  p.feed(77, sink);
  CHECK(sink.calls == 2);  // unchanged: silently dropped
}

void test_zero_data_byte_status_emits_immediately() {
  MidiParser p;
  Sink sink;
  p.feed(midi::kTuneRequest, sink);  // data_length == 0
  CHECK(sink.calls == 1);
  CHECK(sink.last.status == midi::kTuneRequest);
}

void test_orphan_data_byte_with_no_status_in_effect_is_dropped() {
  MidiParser p;
  Sink sink;
  p.feed(66, sink);  // a bare data byte, nothing came before it
  CHECK(sink.calls == 0);
  // The parser recovers cleanly: a real message right after still works.
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);
  p.feed(90, sink);
  CHECK(sink.calls == 1);
}

void test_sysex_payload_is_skipped_safely() {
  MidiParser p;
  Sink sink;
  p.feed(midi::kSysExStart, sink);
  CHECK(sink.calls == 0);
  // An arbitrary run of "data" bytes inside the SysEx payload never fires.
  const std::uint8_t payload[] = {1, 2, 3, 127};
  for (std::uint8_t b : payload) {
    p.feed(b, sink);
  }
  CHECK(sink.calls == 0);
  p.feed(midi::kSysExEnd, sink);
  CHECK(sink.calls == 0);  // EOX itself does not emit a message
  // After EOX, running/status are cleared: an orphan data byte is dropped.
  p.feed(55, sink);
  CHECK(sink.calls == 0);
  // A fresh real message parses normally afterward.
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);
  p.feed(90, sink);
  CHECK(sink.calls == 1);
}

void test_sysex_interrupted_by_a_new_status_is_robust() {
  MidiParser p;
  Sink sink;
  p.feed(midi::kSysExStart, sink);
  p.feed(1, sink);
  p.feed(2, sink);  // mid-SysEx, no EOX yet
  CHECK(sink.calls == 0);
  // A genuine status byte interrupts the SysEx (robust to interruption,
  // DESIGN.md §9.A) and starts a fresh message normally.
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 4), sink);
  p.feed(30, sink);
  p.feed(40, sink);
  CHECK(sink.calls == 1);
  CHECK(sink.last.type() == midi::kNoteOn);
  CHECK(sink.last.channel() == 4);
  CHECK(sink.last.d1 == 30 && sink.last.d2 == 40);
}

void test_multi_byte_feed_overload_parses_an_array() {
  MidiParser p;
  Sink sink;
  const std::uint8_t bytes[] = {static_cast<std::uint8_t>(midi::kNoteOn | 0),  61, 91,
                                static_cast<std::uint8_t>(midi::kNoteOff | 0), 61, 0};
  p.feed(bytes, sizeof(bytes), sink);
  CHECK(sink.calls == 2);
  CHECK(sink.last.type() == midi::kNoteOff);
  CHECK(sink.last.d1 == 61);
}

void test_reset_clears_running_status_and_partial_assembly() {
  MidiParser p;
  Sink sink;
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);  // partial: status armed, one data byte received
  p.reset();
  // The dangling data byte that WOULD have completed the message is now an
  // orphan: reset() cleared status, running status, and the byte counter.
  p.feed(100, sink);
  CHECK(sink.calls == 0);
  // The parser is fully usable again afterward.
  p.feed(static_cast<std::uint8_t>(midi::kNoteOn | 0), sink);
  p.feed(60, sink);
  p.feed(90, sink);
  CHECK(sink.calls == 1);
}

}  // namespace

int main() {
  test_simple_note_on_three_bytes();
  test_running_status_reuses_last_channel_voice_status();
  test_realtime_bytes_interleave_without_disturbing_the_running_message();
  test_note_on_velocity_zero_normalizes_to_note_off_fresh_status();
  test_note_on_velocity_zero_normalizes_via_running_status_too();
  test_system_common_status_resets_running_status();
  test_zero_data_byte_status_emits_immediately();
  test_orphan_data_byte_with_no_status_in_effect_is_dropped();
  test_sysex_payload_is_skipped_safely();
  test_sysex_interrupted_by_a_new_status_is_robust();
  test_multi_byte_feed_overload_parses_an_array();
  test_reset_clears_running_status_and_partial_assembly();
  return arrangrr::test::failures();
}

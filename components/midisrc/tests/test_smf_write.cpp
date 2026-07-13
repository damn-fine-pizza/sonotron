#include "midisrc/smf_write.hpp"

#include <cstdio>
#include <vector>

#include "common/midi/message.hpp"
#include "common/time.hpp"
#include "midisrc/diagnostics.hpp"
#include "midisrc/file_io.hpp"
#include "midisrc/smf.hpp"
#include "test.hpp"

// Round-trip correctness proof for the writer: a known event stream is
// serialized with write_smf() and fed straight back through the existing
// (already-trusted) arrstyle::parse_smf() — the parsed notes/header must
// match what went in.

namespace {

using arrangrr::MidiMessage;
using arrstyle::SmfEvent;

// A minimal, tick-ordered stream: two overlapping notes on different
// channels plus one dropped (unmodelled) Control Change, mirroring the mix
// of message kinds a real session's OutEvent::midi stream can carry.
std::vector<SmfEvent> sample_events() {
  return {
      SmfEvent{.tick = 0, .msg = MidiMessage::note_on(0, 60, 100)},
      SmfEvent{.tick = 200, .msg = MidiMessage::cc(0, 7, 100)},
      SmfEvent{.tick = 480, .msg = MidiMessage::note_on(1, 67, 90)},
      SmfEvent{.tick = 960, .msg = MidiMessage::note_off(0, 60, 64)},
      // A Note On with velocity 0 is the alternate Note-Off spelling.
      SmfEvent{.tick = 1440, .msg = MidiMessage::note_on(1, 67, 0)},
  };
}

void test_round_trip_notes_and_header() {
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf(sample_events());

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(bytes, "round-trip", parsed, diag));
  CHECK(!diag.has_errors());

  CHECK(parsed.format == 0);
  CHECK(parsed.ntrks == 1);
  CHECK(parsed.division == arrangrr::kPpqn);
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.dropped_cc == 1);
  CHECK(parsed.unmatched_note_on == 0);

  const std::vector<arrstyle::SmfNote>& notes = parsed.tracks[0].notes;
  CHECK(notes.size() == 2);
  if (notes.size() == 2) {
    CHECK(notes[0].tick == 0);
    CHECK(notes[0].channel == 0);
    CHECK(notes[0].note == 60);
    CHECK(notes[0].velocity == 100);
    CHECK(notes[0].gate == 960);

    CHECK(notes[1].tick == 480);
    CHECK(notes[1].channel == 1);
    CHECK(notes[1].note == 67);
    CHECK(notes[1].velocity == 90);
    CHECK(notes[1].gate == 960);
  }
}

void test_custom_division_round_trips() {
  const std::vector<SmfEvent> events = {
      SmfEvent{.tick = 0, .msg = MidiMessage::note_on(2, 48, 80)},
      SmfEvent{.tick = 240, .msg = MidiMessage::note_off(2, 48, 0)},
  };
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf(events, /*division=*/480);

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(bytes, "custom-division", parsed, diag));
  CHECK(!diag.has_errors());
  CHECK(parsed.division == 480);
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.tracks[0].notes.size() == 1);
  if (!parsed.tracks[0].notes.empty()) {
    const arrstyle::SmfNote& note = parsed.tracks[0].notes[0];
    CHECK(note.tick == 0);
    CHECK(note.channel == 2);
    CHECK(note.note == 48);
    CHECK(note.velocity == 80);
    CHECK(note.gate == 240);
  }
}

void test_empty_stream_is_a_valid_empty_file() {
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf({});

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(bytes, "empty", parsed, diag));
  CHECK(!diag.has_errors());
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.tracks[0].notes.empty());
}

// Out-of-order ticks are clamped to a zero delta rather than producing an
// underflowed (corrupt) VLQ — this only guards against misuse; a real caller
// always presorts.
void test_out_of_order_tick_is_clamped_not_corrupt() {
  const std::vector<SmfEvent> events = {
      SmfEvent{.tick = 500, .msg = MidiMessage::note_on(0, 60, 100)},
      SmfEvent{.tick = 100, .msg = MidiMessage::note_off(0, 60, 0)},
  };
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf(events);

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(bytes, "out-of-order", parsed, diag));
  CHECK(!diag.has_errors());
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.tracks[0].notes.size() == 1);
  if (!parsed.tracks[0].notes.empty()) {
    // The clamped delta means the Note Off lands at the SAME absolute tick
    // as the Note On (500), giving a zero-length gate rather than a huge one.
    CHECK(parsed.tracks[0].notes[0].tick == 500);
    CHECK(parsed.tracks[0].notes[0].gate == 0);
  }
}

// A clock-out-enabled live session's OutEvent::midi stream interleaves
// System Realtime bytes (MIDI clock and friends) with the note traffic --
// exactly the scenario tests/golden/running_status.acmd exercises for the
// PARSER. The writer must drop them (they cannot round-trip through
// parse_smf, which does not model them as a channel-voice message) rather
// than emit a track parse_smf then rejects.
void test_realtime_bytes_are_dropped_not_written() {
  const std::vector<SmfEvent> events = {
      SmfEvent{.tick = 0, .msg = MidiMessage::note_on(0, 60, 100)},
      SmfEvent{.tick = 10, .msg = MidiMessage::realtime(arrangrr::midi::kClock)},
      SmfEvent{.tick = 20, .msg = MidiMessage::note_off(0, 60, 0)},
  };
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf(events);

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(bytes, "realtime-dropped", parsed, diag));
  CHECK(!diag.has_errors());
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.tracks[0].notes.size() == 1);
  if (!parsed.tracks[0].notes.empty()) {
    CHECK(parsed.tracks[0].notes[0].tick == 0);
    CHECK(parsed.tracks[0].notes[0].gate == 20);
  }
}

// Full disk round trip: the same path `export-smf` exercises
// (write_smf -> write_binary_file -> read_binary_file -> parse_smf).
void test_disk_round_trip() {
  const std::string path = "test_smf_write_disk_round_trip.mid";
  const std::vector<std::uint8_t> bytes = arrstyle::write_smf(sample_events());

  std::string error;
  CHECK(midisrc::write_binary_file(path, bytes, error));

  std::vector<std::uint8_t> reread;
  CHECK(midisrc::read_binary_file(path, reread, error));
  CHECK(reread == bytes);

  arrstyle::SmfFile parsed;
  arrstyle::Diagnostics diag;
  CHECK(arrstyle::parse_smf(reread, path, parsed, diag));
  CHECK(!diag.has_errors());
  CHECK(parsed.tracks.size() == 1);
  CHECK(parsed.tracks[0].notes.size() == 2);

  std::remove(path.c_str());
}

}  // namespace

int main() {
  test_round_trip_notes_and_header();
  test_custom_division_round_trips();
  test_empty_stream_is_a_valid_empty_file();
  test_out_of_order_tick_is_clamped_not_corrupt();
  test_realtime_bytes_are_dropped_not_written();
  test_disk_round_trip();
  return midisrc::test::failures();
}

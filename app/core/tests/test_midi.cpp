#include <initializer_list>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/midi/parser.hpp"
#include "arrangrr/routing/note_tracker.hpp"
#include "arrangrr/routing/router.hpp"
#include "arrangrr/scheduler/out_scheduler.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Msgs = StaticVector<MidiMessage, 64>;

Msgs parse(std::initializer_list<std::uint8_t> bytes) {
  MidiParser p;
  Msgs out;
  for (std::uint8_t b : bytes) p.feed(b, [&](const MidiMessage& m) { CHECK(out.push_back(m)); });
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

void test_scheduler_total_order() {
  OutScheduler<16> s;
  // Insert same-tick events in adversarial order; expect
  // realtime < NoteOff < CC < NoteOn, then seq within class (D29).
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 60, 100)));
  CHECK(s.schedule(0, 10, MidiMessage::cc(0, 7, 100)));
  CHECK(s.schedule(0, 10, MidiMessage::note_off(0, 55)));
  CHECK(s.schedule(0, 10, MidiMessage::realtime(midi::kClock)));
  CHECK(s.schedule(0, 5, MidiMessage::note_on(0, 40, 1)));  // earlier tick wins overall
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 61, 100)));  // same class: seq order

  StaticVector<MidiMessage, 8> out;
  s.pop_due(10, [&](const ScheduledEvent& ev) { CHECK(out.push_back(ev.msg)); });
  CHECK(out.size() == 6);
  CHECK(out[0].d1 == 40);                        // tick 5 first
  CHECK(out[1].status == midi::kClock);          // realtime
  CHECK(out[2].type() == midi::kNoteOff);        // NoteOff before NoteOn
  CHECK(out[3].type() == midi::kControlChange);  // CC before NoteOn
  CHECK(out[4].d1 == 60);                        // NoteOns in emission order
  CHECK(out[5].d1 == 61);
  CHECK(s.empty());
}

void test_scheduler_due_only() {
  OutScheduler<4> s;
  CHECK(s.schedule(0, 100, MidiMessage::note_on(0, 60, 1)));
  int fired = 0;
  s.pop_due(99, [&](const ScheduledEvent&) { ++fired; });
  CHECK(fired == 0);
  s.pop_due(100, [&](const ScheduledEvent&) { ++fired; });
  CHECK(fired == 1);
}

void test_router_filters_and_remap() {
  Router r;
  CHECK(r.add(Route{.in_port = 0, .in_channel = 0, .out_port = 1, .out_channel = 4,
                    .pass = route_pass::kNotes}));
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
  p.feed(0x90, sink);
  p.feed(60, sink);  // half a message
  p.reset();
  p.feed(100, sink);  // orphan after reset: dropped
  CHECK(out.empty());
  const std::uint8_t rest[] = {0x80, 60, 0};
  p.feed(rest, 3, sink);
  CHECK(out.size() == 1 && out[0].type() == midi::kNoteOff);
}

void test_scheduler_clear_and_refill() {
  OutScheduler<8> s;
  CHECK(s.schedule(0, 1, MidiMessage::note_on(0, 60, 1)));
  CHECK(s.schedule(0, 2, MidiMessage::note_on(0, 61, 1)));
  s.clear();
  CHECK(s.empty());
  int fired = 0;
  s.pop_due(100, [&](const ScheduledEvent&) { ++fired; });
  CHECK(fired == 0);
  // Refill in reverse tick order to exercise deeper sift paths.
  for (std::uint32_t t = 8; t > 0; --t)
    CHECK(s.schedule(0, t, MidiMessage::note_on(0, static_cast<std::uint8_t>(t), 1)));
  CHECK(!s.schedule(0, 9, MidiMessage::note_on(0, 9, 1)));  // full
  Tick last = 0;
  s.pop_due(100, [&](const ScheduledEvent& ev) {
    CHECK(ev.tick >= last);
    last = ev.tick;
  });
  CHECK(last == 8);
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
  test_scheduler_total_order();
  test_scheduler_due_only();
  test_scheduler_clear_and_refill();
  test_router_filters_and_remap();
  test_note_tracker_panic_with_sustain();
  test_note_tracker_high_notes_and_bounds();
  test_note_tracker_pedal_release_clears();
  if (arrangrr::test::failures() == 0) std::printf("test_midi: all OK\n");
  return arrangrr::test::failures();
}

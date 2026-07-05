#include "arrangrr/scheduler/out_scheduler.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/midi/message.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// classify() maps each wire message to its scheduling class (D29 total order).
void test_classify_all_classes() {
  CHECK(classify(MidiMessage::realtime(midi::kClock)) == EventClass::kRealtime);
  CHECK(classify(MidiMessage::note_off(0, 60)) == EventClass::kNoteOff);
  CHECK(classify(MidiMessage::note_on(0, 60, 100)) == EventClass::kNoteOn);
  CHECK(classify(MidiMessage::cc(0, 7, 100)) == EventClass::kOther);
  CHECK(classify(MidiMessage::program(0, 5)) == EventClass::kOther);
  // NoteOn with velocity 0 is still a NoteOn on the wire (normalization happens
  // in the parser, not here).
  CHECK(classify(MidiMessage::note_on(0, 60, 0)) == EventClass::kNoteOn);
}

// A full scheduler rejects further inserts and keeps its size pinned at N.
void test_schedule_full_rejects() {
  OutScheduler<3> s;
  CHECK(s.schedule(0, 1, MidiMessage::note_on(0, 60, 1)));
  CHECK(s.schedule(0, 2, MidiMessage::note_on(0, 61, 1)));
  CHECK(s.schedule(0, 3, MidiMessage::note_on(0, 62, 1)));
  CHECK(s.size() == 3);
  CHECK(!s.schedule(0, 4, MidiMessage::note_on(0, 63, 1)));  // full: rejected
  CHECK(s.size() == 3);
  // Even a realtime/clock event is rejected when full (no class privilege here).
  CHECK(!s.schedule(0, 0, MidiMessage::realtime(midi::kClock)));
  CHECK(s.size() == 3);
}

// pop_due only drains events at or before `now`, leaving the future intact.
void test_pop_due_boundary() {
  OutScheduler<8> s;
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 60, 1)));
  CHECK(s.schedule(0, 20, MidiMessage::note_on(0, 61, 1)));
  CHECK(s.schedule(0, 30, MidiMessage::note_on(0, 62, 1)));

  int fired = 0;
  s.pop_due(9, [&](const ScheduledEvent&) { ++fired; });
  CHECK(fired == 0);  // nothing due yet
  s.pop_due(10, [&](const ScheduledEvent& ev) {
    ++fired;
    CHECK(ev.tick == 10 && ev.msg.d1 == 60);
  });
  CHECK(fired == 1);
  CHECK(s.size() == 2);
  // Drain the rest; the last pop empties the heap (size hits 0 mid-loop).
  StaticVector<Tick, 4> ticks;
  s.pop_due(1000, [&](const ScheduledEvent& ev) { CHECK(ticks.push_back(ev.tick)); });
  CHECK(ticks.size() == 2 && ticks[0] == 20 && ticks[1] == 30);
  CHECK(s.empty());
}

// cancel_note_off tombstones the EARLIEST pending NoteOff for (port,ch,note),
// and pop_due then drops that tombstoned entry silently (status == 0 branch).
void test_cancel_note_off_tombstones_earliest() {
  OutScheduler<8> s;
  // Three NoteOffs for the same (port 0, ch 0, note 60) at ticks 5, 10, 1.
  CHECK(s.schedule(0, 5, MidiMessage::note_off(0, 60)));
  CHECK(s.schedule(0, 10, MidiMessage::note_off(0, 60)));
  CHECK(s.schedule(0, 1, MidiMessage::note_off(0, 60)));

  CHECK(s.cancel_note_off(0, 0, 60));  // cancels the tick-1 (earliest) one

  StaticVector<Tick, 4> ticks;
  s.pop_due(1000, [&](const ScheduledEvent& ev) { CHECK(ticks.push_back(ev.tick)); });
  // The tick-1 event was tombstoned and dropped; only 5 and 10 survive.
  CHECK(ticks.size() == 2 && ticks[0] == 5 && ticks[1] == 10);
  CHECK(s.empty());
}

// When the earliest matching NoteOff is NOT at the heap root (a lower-tick
// non-matching event sits at the root), cancel must still find it by scanning:
// exercises the "e.tick < best.tick" replacement branch of the linear search.
void test_cancel_note_off_replacement_branch() {
  OutScheduler<8> s;
  // A NoteOn at tick 1 becomes the heap root (lowest tick). The two NoteOffs
  // at ticks 5 and 3 are scanned afterwards; the tick-3 one must win.
  CHECK(s.schedule(0, 1, MidiMessage::note_on(0, 60, 100)));
  CHECK(s.schedule(0, 5, MidiMessage::note_off(0, 60)));
  CHECK(s.schedule(0, 3, MidiMessage::note_off(0, 60)));

  CHECK(s.cancel_note_off(0, 0, 60));  // must tombstone the tick-3 NoteOff

  StaticVector<Tick, 4> on_ticks;
  StaticVector<Tick, 4> off_ticks;
  s.pop_due(1000, [&](const ScheduledEvent& ev) {
    if (ev.msg.type() == midi::kNoteOn) {
      CHECK(on_ticks.push_back(ev.tick));
    } else if (ev.msg.type() == midi::kNoteOff) {
      CHECK(off_ticks.push_back(ev.tick));
    }
  });
  CHECK(on_ticks.size() == 1 && on_ticks[0] == 1);
  CHECK(off_ticks.size() == 1 && off_ticks[0] == 5);  // tick-3 was cancelled
}

// cancel_note_off must ignore non-matching entries: wrong port, wrong channel,
// wrong note, and non-NoteOff types all hit the `continue` guard, and an
// already-tombstoned entry (status 0) is skipped too.
void test_cancel_note_off_no_match_and_guards() {
  OutScheduler<16> s;
  CHECK(s.schedule(1, 5, MidiMessage::note_off(0, 60)));   // wrong port
  CHECK(s.schedule(0, 6, MidiMessage::note_off(3, 60)));   // wrong channel
  CHECK(s.schedule(0, 7, MidiMessage::note_off(0, 61)));   // wrong note
  CHECK(s.schedule(0, 8, MidiMessage::note_on(0, 60, 1)));  // not a NoteOff
  CHECK(s.schedule(0, 9, MidiMessage::cc(0, 7, 1)));        // not a NoteOff

  // No matching (port 0, ch 0, note 60) NoteOff exists.
  CHECK(!s.cancel_note_off(0, 0, 60));
  CHECK(s.size() == 5);  // nothing tombstoned

  // Now add a real match and a second match; cancel twice. The second cancel
  // must skip the first (now status 0) tombstone and find the other.
  CHECK(s.schedule(0, 2, MidiMessage::note_off(0, 60)));
  CHECK(s.schedule(0, 4, MidiMessage::note_off(0, 60)));
  CHECK(s.cancel_note_off(0, 0, 60));  // tombstones tick-2
  CHECK(s.cancel_note_off(0, 0, 60));  // tick-2 now status 0 -> skip; tombstones tick-4
  CHECK(!s.cancel_note_off(0, 0, 60));  // both gone now

  int off60 = 0;
  s.pop_due(1000, [&](const ScheduledEvent& ev) {
    if (ev.msg.type() == midi::kNoteOff && ev.msg.channel() == 0 && ev.msg.d1 == 60 &&
        ev.port == 0) {
      ++off60;
    }
  });
  CHECK(off60 == 0);  // every matching NoteOff was cancelled
}

// clear() empties the heap without draining, and the scheduler is reusable.
void test_clear_resets() {
  OutScheduler<4> s;
  CHECK(s.schedule(0, 1, MidiMessage::note_on(0, 60, 1)));
  CHECK(s.schedule(0, 2, MidiMessage::note_on(0, 61, 1)));
  CHECK(!s.empty());
  s.clear();
  CHECK(s.empty() && s.size() == 0);
  int fired = 0;
  s.pop_due(1000, [&](const ScheduledEvent&) { ++fired; });
  CHECK(fired == 0);
  CHECK(s.schedule(0, 1, MidiMessage::note_on(0, 62, 1)));  // usable again
  CHECK(s.size() == 1);
}

// Same-tick tie-break: realtime < NoteOff < Other < NoteOn, then insertion seq.
void test_same_tick_total_order() {
  OutScheduler<8> s;
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 70, 1)));   // seq0, NoteOn
  CHECK(s.schedule(0, 10, MidiMessage::note_off(0, 55)));     // seq1, NoteOff
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 71, 1)));   // seq2, NoteOn
  CHECK(s.schedule(0, 10, MidiMessage::cc(0, 7, 1)));         // seq3, Other
  CHECK(s.schedule(0, 10, MidiMessage::realtime(midi::kStart)));  // seq4, Realtime

  StaticVector<MidiMessage, 8> out;
  s.pop_due(10, [&](const ScheduledEvent& ev) { CHECK(out.push_back(ev.msg)); });
  CHECK(out.size() == 5);
  CHECK(out[0].status == midi::kStart);            // realtime first
  CHECK(out[1].type() == midi::kNoteOff);          // then NoteOff
  CHECK(out[2].type() == midi::kControlChange);    // then Other/CC
  CHECK(out[3].type() == midi::kNoteOn && out[3].d1 == 70);  // NoteOns by seq
  CHECK(out[4].type() == midi::kNoteOn && out[4].d1 == 71);
}

}  // namespace

int main() {
  test_classify_all_classes();
  test_schedule_full_rejects();
  test_pop_due_boundary();
  test_cancel_note_off_tombstones_earliest();
  test_cancel_note_off_replacement_branch();
  test_cancel_note_off_no_match_and_guards();
  test_clear_resets();
  test_same_tick_total_order();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scheduler: all OK\n");
  }
  return arrangrr::test::failures();
}

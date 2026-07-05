#include "arrangrr/scheduler/out_scheduler.hpp"

#include <cstdint>

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

// One shared sink type for every pop_due() call below. Reusing a single
// callable keeps pop_due<Sink> a SINGLE template instantiation (gcovr counts
// branches per instantiation, so a fresh lambda per test would only dilute the
// metric). It records tick, class and first data byte so callers can assert
// the exact drained order.
struct Recorder {
  StaticVector<ScheduledEvent, 32>* log = nullptr;
  void operator()(const ScheduledEvent& ev) const { CHECK(log->push_back(ev)); }
};

// Fill an N=4 heap and drain it: this is the only test that exercises
// OutScheduler<4>::sift_down at all (the existing N=4 test clears without
// popping). The mixed payload — two events on the same tick with different
// classes, two more on the same tick with the same class — also drives
// before() through its tick-equal, class-equal and seq tie-break branches on
// this instantiation. Filling to N=4 covers schedule()'s "full" branch here.
void test_full_drain_and_order_n4() {
  OutScheduler<4> s;
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 60, 1)));   // seq0 NoteOn
  CHECK(s.schedule(0, 10, MidiMessage::note_off(0, 55)));     // seq1 NoteOff same tick
  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 61, 1)));   // seq2 NoteOn same tick/class
  CHECK(s.schedule(0, 5, MidiMessage::note_on(0, 99, 1)));    // seq3 earliest tick
  CHECK(s.size() == 4);
  CHECK(!s.schedule(0, 7, MidiMessage::note_on(0, 62, 1)));   // full: rejected
  CHECK(s.size() == 4);

  StaticVector<ScheduledEvent, 32> log;
  s.pop_due(1000, Recorder{&log});
  CHECK(log.size() == 4);
  // tick 5 leads; then at tick 10 the NoteOff (lower class) precedes the two
  // NoteOns, which keep their emission (seq) order.
  CHECK(log[0].tick == 5 && log[0].msg.d1 == 99);
  CHECK(log[1].tick == 10 && log[1].msg.type() == midi::kNoteOff && log[1].msg.d1 == 55);
  CHECK(log[2].tick == 10 && log[2].msg.type() == midi::kNoteOn && log[2].msg.d1 == 60);
  CHECK(log[3].tick == 10 && log[3].msg.type() == midi::kNoteOn && log[3].msg.d1 == 61);
  CHECK(s.empty());

  // Sweep several insertion permutations of four distinct ticks through the
  // same N=4 instantiation. Each fill/drain reshapes the heap so sift_down
  // visits both-children, left-only and no-swap nodes with the left- and the
  // right-child winning in turn — driving sift_down<4> across its branches.
  const Tick perms[4][4] = {{4, 3, 2, 1}, {1, 3, 2, 4}, {3, 1, 4, 2}, {2, 4, 1, 3}};
  for (const auto& p : perms) {
    for (Tick t : p) {
      CHECK(s.schedule(0, t, MidiMessage::note_on(0, static_cast<std::uint8_t>(t), 1)));
    }
    CHECK(s.size() == 4);
    StaticVector<ScheduledEvent, 32> drained;
    s.pop_due(1000, Recorder{&drained});
    CHECK(drained.size() == 4);
    CHECK(drained[0].tick == 1 && drained[1].tick == 2 && drained[2].tick == 3 &&
          drained[3].tick == 4);
    CHECK(s.empty());
  }
}

// Same-tick mixed-class events on N=3 push before() to full coverage on that
// small instantiation (tick-equal with class differing, and class-equal
// falling through to the seq tie-break).
void test_same_tick_before_n3() {
  OutScheduler<3> s;
  CHECK(s.schedule(0, 30, MidiMessage::note_on(0, 70, 1)));  // seq0
  CHECK(s.schedule(0, 30, MidiMessage::note_off(0, 71)));    // seq1 same tick, NoteOff
  CHECK(s.schedule(0, 30, MidiMessage::note_on(0, 72, 1)));  // seq2 same tick/class as seq0

  StaticVector<ScheduledEvent, 32> log;
  s.pop_due(30, Recorder{&log});
  CHECK(log.size() == 3);
  CHECK(log[0].msg.type() == midi::kNoteOff && log[0].msg.d1 == 71);  // class leads
  CHECK(log[1].msg.d1 == 70 && log[2].msg.d1 == 72);                  // seq order
  CHECK(s.empty());
}

// Fill an N=16 heap (schedule()'s "full" branch on this instantiation) with a
// same-tick trio embedded so before() is driven through its class-equal and
// seq branches here too, then drain in strict order.
void test_fill_and_before_n16() {
  OutScheduler<16> s;
  // A same-tick trio at tick 100 (two NoteOns + one NoteOff between classes).
  CHECK(s.schedule(0, 100, MidiMessage::note_on(0, 80, 1)));  // seq0
  CHECK(s.schedule(0, 100, MidiMessage::note_off(0, 81)));    // seq1
  CHECK(s.schedule(0, 100, MidiMessage::note_on(0, 82, 1)));  // seq2
  // Thirteen more at distinct, descending ticks to reach capacity 16.
  for (Tick t = 13; t >= 1; --t) {
    CHECK(s.schedule(0, t, MidiMessage::note_on(0, static_cast<std::uint8_t>(t), 1)));
  }
  CHECK(s.size() == 16);
  CHECK(!s.schedule(0, 200, MidiMessage::note_on(0, 90, 1)));  // full: rejected
  CHECK(s.size() == 16);

  StaticVector<ScheduledEvent, 32> log;
  s.pop_due(1000, Recorder{&log});
  CHECK(log.size() == 16);
  // Ticks 1..13 come out ascending first.
  bool ascending = true;
  for (std::size_t i = 1; i < 13; ++i) {
    ascending = ascending && log[i - 1].tick <= log[i].tick;
  }
  CHECK(ascending);
  // The tick-100 trio comes last, class then seq ordered.
  CHECK(log[13].msg.type() == midi::kNoteOff && log[13].msg.d1 == 81);
  CHECK(log[14].msg.d1 == 80 && log[15].msg.d1 == 82);
  CHECK(s.empty());
}

// Drive pop_due<Recorder> through every one of its own branches on one
// instantiation: an empty-heap pop (loop guard m_size > 0 false), a pop that
// stops on a future event (tick <= now false), a tombstoned entry skipped
// (status != 0 false), a normal sink call (status != 0 true), a sift after a
// pop (m_size > 0 true) and the final pop that empties the heap (m_size > 0
// false, sift_down skipped).
void test_pop_due_recorder_all_paths() {
  OutScheduler<8> s;
  StaticVector<ScheduledEvent, 32> log;

  // Empty heap: the while loop never runs.
  s.pop_due(1000, Recorder{&log});
  CHECK(log.size() == 0);

  CHECK(s.schedule(0, 10, MidiMessage::note_on(0, 60, 1)));
  CHECK(s.schedule(0, 20, MidiMessage::note_off(0, 60)));
  CHECK(s.schedule(0, 30, MidiMessage::note_on(0, 61, 1)));
  CHECK(s.schedule(0, 40, MidiMessage::note_off(0, 61)));

  // Tombstone the tick-20 NoteOff so pop_due hits the status == 0 skip.
  CHECK(s.cancel_note_off(0, 0, 60));

  // Stops after tick 10: the tick-20 tombstone is popped-and-dropped, ticks 30
  // and 40 stay in the future (tick <= now goes false).
  s.pop_due(20, Recorder{&log});
  CHECK(log.size() == 1 && log[0].tick == 10);  // the tombstone emitted nothing
  CHECK(s.size() == 2);

  // Drain the rest; the last pop empties the heap.
  s.pop_due(1000, Recorder{&log});
  CHECK(log.size() == 3 && log[1].tick == 30 && log[2].tick == 40);
  CHECK(s.empty());
}

// Push cancel_note_off on N=8 through all its guard branches (wrong port,
// wrong channel, wrong note, non-NoteOff type, already-tombstoned) plus the
// first-match and tick-replacement paths, so this instantiation's comparator
// is fully covered (the existing guard test used N=16).
void test_cancel_guards_n8() {
  OutScheduler<8> s;
  CHECK(s.schedule(1, 5, MidiMessage::note_off(0, 60)));    // wrong port
  CHECK(s.schedule(0, 6, MidiMessage::note_off(3, 60)));    // wrong channel
  CHECK(s.schedule(0, 7, MidiMessage::note_off(0, 61)));    // wrong note
  CHECK(s.schedule(0, 8, MidiMessage::note_on(0, 60, 1)));  // not a NoteOff
  CHECK(!s.cancel_note_off(0, 0, 60));                      // no match: best < 0

  CHECK(s.schedule(0, 4, MidiMessage::note_off(0, 60)));  // a real match
  CHECK(s.schedule(0, 2, MidiMessage::note_off(0, 60)));  // earlier match (tick replace)
  CHECK(s.cancel_note_off(0, 0, 60));   // tombstones tick-2 (the earliest)
  CHECK(s.cancel_note_off(0, 0, 60));   // tick-2 now status 0 -> skip; tombstones tick-4
  CHECK(!s.cancel_note_off(0, 0, 60));  // both matches gone

  StaticVector<ScheduledEvent, 32> log;
  s.pop_due(1000, Recorder{&log});
  int matching_offs = 0;
  for (std::size_t i = 0; i < log.size(); ++i) {
    const ScheduledEvent& e = log[i];
    if (e.msg.type() == midi::kNoteOff && e.msg.channel() == 0 && e.msg.d1 == 60 && e.port == 0) {
      ++matching_offs;
    }
  }
  CHECK(matching_offs == 0);  // every (0,0,60) NoteOff was cancelled
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
  test_full_drain_and_order_n4();
  test_same_tick_before_n3();
  test_fill_and_before_n16();
  test_pop_due_recorder_all_paths();
  test_cancel_guards_n8();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scheduler: all OK\n");
  }
  return arrangrr::test::failures();
}

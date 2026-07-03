#include "arrangrr/engine.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 128>;

Command route_add(std::uint8_t in_port, std::int8_t in_ch, std::uint8_t out_port,
                  std::int8_t out_ch, std::uint8_t pass) {
  Command c;
  c.op = Op::kDo;
  c.param = Param::kRouteAdd;
  c.a = in_port | ((in_ch & 0xFF) << 8);
  c.b = out_port | ((out_ch & 0xFF) << 8);
  c.c = pass;
  return c;
}

void test_thru_note_in_note_out() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  e.push_command(route_add(0, -1, 1, -1, route_pass::kAll), sink);
  CHECK(ev.empty());

  const std::uint8_t bytes[] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(bytes), sink);
  CHECK(ev.size() == 1);
  CHECK(ev[0].kind == OutEvent::Kind::kMidi);
  CHECK(ev[0].port == 1);
  CHECK(ev[0].msg.type() == midi::kNoteOn && ev[0].msg.d1 == 60);
  CHECK(ev[0].tick == 0);
}

void test_scheduled_events_fire_on_advance() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  e.schedule_at(0, 10, MidiMessage::note_on(0, 64, 90), sink);
  CHECK(ev.empty());  // not due yet
  e.advance_ticks(9, sink);
  CHECK(ev.empty());
  e.advance_ticks(1, sink);  // now == 10
  CHECK(ev.size() == 1);
  CHECK(ev[0].tick == 10 && ev[0].msg.d1 == 64);
}

void test_events_fire_with_stopped_transport() {
  // D29 / golden G1: stream time advances regardless of transport state.
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  CHECK(!e.transport().playing());
  e.schedule_at(0, 5, MidiMessage::note_on(0, 62, 80), sink);
  e.advance_ticks(5, sink);
  CHECK(ev.size() == 1 && ev[0].tick == 5);
}

void test_transport_clock_emission() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  Command mask;
  mask.op = Op::kSet;
  mask.param = Param::kClockOutMask;
  mask.a = 0b0001;  // port 0
  e.push_command(mask, sink);

  Command start;
  start.param = Param::kTransportStart;
  e.push_command(start, sink);
  // Expect: FA (Start) + transport state event.
  CHECK(ev.size() == 2);
  CHECK(ev[0].kind == OutEvent::Kind::kMidi && ev[0].msg.status == midi::kStart);
  CHECK(ev[1].kind == OutEvent::Kind::kTransport);

  ev.clear();
  e.advance_ticks(80, sink);  // two clock periods (40 ticks each)
  int clocks = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.status == midi::kClock) ++clocks;
  }
  CHECK(clocks == 2);

  ev.clear();
  Command stop;
  stop.param = Param::kTransportStop;
  e.push_command(stop, sink);
  CHECK(ev.size() == 2 && ev[0].msg.status == midi::kStop);
  ev.clear();
  e.advance_ticks(80, sink);
  CHECK(ev.empty());  // no clock while stopped
}

void test_panic_via_engine() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  e.push_command(route_add(0, -1, 0, -1, route_pass::kAll), sink);

  const std::uint8_t on[] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on), sink);
  ev.clear();

  Command panic;
  panic.param = Param::kPanic;
  e.push_command(panic, sink);
  // NoteOff 60 first (class order), then the three channel-mode CCs.
  CHECK(ev.size() == 4);
  CHECK(ev[0].msg.type() == midi::kNoteOff && ev[0].msg.d1 == 60);
  CHECK(ev[1].msg.d1 == midi::kCcAllNotesOff);
  CHECK(ev[2].msg.d1 == midi::kCcAllSoundOff);
  CHECK(ev[3].msg.d1 == midi::kCcResetAllControllers);
}

void test_warn_on_unknown_command() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  Command bogus;
  bogus.param = static_cast<Param>(9999);
  e.push_command(bogus, sink);
  CHECK(ev.size() == 1);
  CHECK(ev[0].kind == OutEvent::Kind::kWarn);
  CHECK(ev[0].code == static_cast<std::uint16_t>(WarnCode::kUnknownCommand));
}

}  // namespace

int main() {
  test_thru_note_in_note_out();
  test_scheduled_events_fire_on_advance();
  test_events_fire_with_stopped_transport();
  test_transport_clock_emission();
  test_panic_via_engine();
  test_warn_on_unknown_command();
  if (arrangrr::test::failures() == 0) std::printf("test_engine: all OK\n");
  return arrangrr::test::failures();
}

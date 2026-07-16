#include "arrangrr/engine.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"
#include "test_harness.hpp"

// Phase-1 runtime extraction: the Transport/clock/scheduler-drain-driven
// subset of this file moved to components/core/runtime/tests/test_engine.cpp
// (Corelli/Palladio: "genuinely runtime concerns"). What stays here exercises
// the Stage's OWN command dispatch/warn-code surface — arrangrr's domain.

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
  test::TestEngine e;
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

void test_panic_via_engine() {
  test::TestEngine e;
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

void test_warn_on_bad_tempo_and_route() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  Command tempo;
  tempo.op = Op::kSet;
  tempo.param = Param::kTransportTempo;
  tempo.a = 1;  // absurd bpm_x100
  e.push_command(tempo, sink);
  CHECK(ev.size() == 1 && ev[0].code == static_cast<std::uint16_t>(WarnCode::kBadArgument));
  ev.clear();
  e.push_command(route_add(9, -1, 0, -1, route_pass::kAll), sink);  // bad in port
  CHECK(ev.size() == 1 && ev[0].code == static_cast<std::uint16_t>(WarnCode::kBadArgument));
}

void test_route_table_full_warns() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  for (std::size_t i = 0; i < kMaxRoutes; ++i) {
    e.push_command(route_add(0, -1, 0, -1, route_pass::kAll), sink);
  }
  CHECK(ev.empty());
  e.push_command(route_add(0, -1, 0, -1, route_pass::kAll), sink);
  CHECK(ev.size() == 1 && ev[0].code == static_cast<std::uint16_t>(WarnCode::kRouteTableFull));
  // Route clear frees the table again.
  ev.clear();
  Command clear;
  clear.param = Param::kRouteClear;
  e.push_command(clear, sink);
  e.push_command(route_add(0, -1, 0, -1, route_pass::kAll), sink);
  CHECK(ev.empty());
}

void test_input_port_out_of_range_ignored() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  const std::uint8_t bytes[] = {0x90, 60, 100};
  e.push_midi_in(9, Span<const std::uint8_t>(bytes), sink);
  CHECK(ev.empty());
}

void test_warn_on_unknown_command() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  Command bogus;
  bogus.param = static_cast<Param>(9999);
  e.push_command(bogus, sink);
  CHECK(ev.size() == 1);
  CHECK(ev[0].kind == OutEvent::Kind::kWarn);
  CHECK(ev[0].code == static_cast<std::uint16_t>(WarnCode::kUnknownCommand));
}

// Out-of-range ports on the arp setters are rejected (guard false branches):
// the arp keeps its valid output route and input port, and a captured note-off
// on the arp input port exercises observe_arp_input's release path.
void test_engine_arp_setter_guards() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  e.set_arp_out(1, 5);          // valid route: port 1, channel 5
  e.set_arp_enabled(true, 0);   // listen on input port 0
  CHECK(e.arp_enabled());
  e.set_arp_out(99, 99);        // out of range: must NOT clobber port 1 / ch 5
  e.set_arp_enabled(true, 99);  // in_port out of range: input port stays 0

  e.arp().set_field(ArpField::kRate, static_cast<std::int32_t>(ArpRate::kEighth));

  Command start;
  start.param = Param::kTransportStart;
  e.push_command(start, sink);

  const std::uint8_t on1[3] = {0x90, 60, 100};
  const std::uint8_t on2[3] = {0x90, 64, 100};
  const std::uint8_t off1[3] = {0x80, 64, 0};  // release: observe_arp_input note_off path
  e.push_midi_in(0, Span<const std::uint8_t>(on1, 3), sink);
  e.push_midi_in(0, Span<const std::uint8_t>(on2, 3), sink);
  e.push_midi_in(0, Span<const std::uint8_t>(off1, 3), sink);
  ev.clear();
  e.advance_ticks(480 * 2, sink);

  int arp_ons = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn && o.msg.d2 > 0) {
      ++arp_ons;
      CHECK(o.port == 1 && o.msg.channel() == 5);  // route survived the bad set_arp_out
    }
  }
  CHECK(arp_ons > 0);
}

// set_chord_detect with an out-of-range port keeps the default detect port, and
// a non-note message on the detect port leaves the recognized chord untouched
// (observe_chord_input's else-return branch).
void test_engine_chord_detect_guard_and_nonnote() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  e.set_chord_detect(true, 99);  // out of range: detect port stays 0
  CHECK(e.chord_detect());

  auto feed = [&](std::uint8_t s, std::uint8_t d1, std::uint8_t d2) {
    const std::uint8_t b[3] = {s, d1, d2};
    e.push_midi_in(0, Span<const std::uint8_t>(b, 3), sink);
  };
  feed(0x90, 60, 100);  // C E G on port 0 -> C major recognized
  feed(0x90, 64, 100);
  feed(0x90, 67, 100);
  CHECK(e.chords().state().valid);
  CHECK(e.chords().state().root_pc == 0);
  CHECK(e.chords().state().quality == ChordQuality::kMaj);

  // A CC on the detect port is a non-note message: the chord must not change.
  feed(0xB0, 7, 100);
  CHECK(e.chords().state().root_pc == 0 && e.chords().state().quality == ChordQuality::kMaj);

  // Toggling detection off then on again clears the held set (enable branch).
  e.set_chord_detect(false, 0);
  CHECK(!e.chord_detect());
  e.set_chord_detect(true, 0);
  CHECK(e.chord_detect());
}

// Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b): every
// Param this milestone wires a kParamState echo for emits exactly ONE such
// event, tagging `code` with the Param itself and packing `port`/`msg` per
// the abi.hpp Kind::kParamState comment table.
void test_param_state_echo() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto last = [&]() -> const OutEvent& { return ev[ev.size() - 1]; };

  // kGroove: port=GrooveField, status|d1<<8 = value.
  Command groove;
  groove.op = Op::kSet;
  groove.param = Param::kGroove;
  groove.a = static_cast<std::int32_t>(GrooveField::kSwing);
  groove.b = 42;
  e.push_command(groove, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kGroove));
  CHECK(last().port == static_cast<std::uint8_t>(GrooveField::kSwing));
  CHECK(last().msg.status == 42 && last().msg.d1 == 0);
  ev.clear();

  // kArp: port=ArpField, status|d1<<8 = value.
  Command arp;
  arp.op = Op::kSet;
  arp.param = Param::kArp;
  arp.a = static_cast<std::int32_t>(ArpField::kGate);
  arp.b = 60;
  e.push_command(arp, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kArp));
  CHECK(last().port == static_cast<std::uint8_t>(ArpField::kGate));
  CHECK(last().msg.status == 60 && last().msg.d1 == 0);
  ev.clear();

  // kPartMute/kPartSolo: port=TrackRole, status=0/1.
  Command mute;
  mute.op = Op::kSet;
  mute.param = Param::kPartMute;
  mute.a = static_cast<std::int32_t>(TrackRole::kBass);
  mute.b = 1;
  e.push_command(mute, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kPartMute));
  CHECK(last().port == static_cast<std::uint8_t>(TrackRole::kBass));
  CHECK(last().msg.status == 1);
  ev.clear();

  Command solo;
  solo.op = Op::kSet;
  solo.param = Param::kPartSolo;
  solo.a = static_cast<std::int32_t>(TrackRole::kPad);
  solo.b = 1;
  e.push_command(solo, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kPartSolo));
  CHECK(last().port == static_cast<std::uint8_t>(TrackRole::kPad));
  CHECK(last().msg.status == 1);
  ev.clear();

  // kChordDetect: port=input port, status=0/1 enabled.
  Command detect;
  detect.op = Op::kSet;
  detect.param = Param::kChordDetect;
  detect.a = 1;
  detect.b = 2;  // input port 2
  e.push_command(detect, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kChordDetect));
  CHECK(last().port == 2);
  CHECK(last().msg.status == 1);
  ev.clear();

  // kChordFollow: port=0, status=ChordFollow.
  Command follow;
  follow.op = Op::kSet;
  follow.param = Param::kChordFollow;
  follow.a = static_cast<std::int32_t>(ChordFollow::kManual);
  e.push_command(follow, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kChordFollow));
  CHECK(last().port == 0);
  CHECK(last().msg.status == static_cast<std::uint8_t>(ChordFollow::kManual));
  ev.clear();

  // kChordMode: port=0, status=ChordMode.
  Command mode;
  mode.op = Op::kSet;
  mode.param = Param::kChordMode;
  mode.a = static_cast<std::int32_t>(ChordMode::kShell);
  e.push_command(mode, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kChordMode));
  CHECK(last().port == 0);
  CHECK(last().msg.status == static_cast<std::uint8_t>(ChordMode::kShell));
  ev.clear();

  // kKeySet: port=0, status=root pc, d1=Mode.
  Command key;
  key.op = Op::kSet;
  key.param = Param::kKeySet;
  key.a = 7;  // G
  key.b = static_cast<std::int32_t>(Mode::kMixolydian);
  e.push_command(key, sink);
  CHECK(last().kind == OutEvent::Kind::kParamState);
  CHECK(last().code == static_cast<std::uint16_t>(Param::kKeySet));
  CHECK(last().port == 0);
  CHECK(last().msg.status == 7);
  CHECK(last().msg.d1 == static_cast<std::uint8_t>(Mode::kMixolydian));
  ev.clear();

  // kStyleLoad: port=0, status|d1<<8 = builtin style index.
  Command style;
  style.op = Op::kDo;
  style.param = Param::kStyleLoad;
  style.a = 0;  // builtin 0 always exists
  e.push_command(style, sink);
  bool saw_param_state = false;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kParamState) {
      saw_param_state = true;
      CHECK(o.code == static_cast<std::uint16_t>(Param::kStyleLoad));
      CHECK(o.port == 0);
      CHECK(o.msg.status == 0 && o.msg.d1 == 0);
    }
  }
  CHECK(saw_param_state);
}

}  // namespace

int main() {
  test_thru_note_in_note_out();
  test_engine_arp_setter_guards();
  test_engine_chord_detect_guard_and_nonnote();
  test_panic_via_engine();
  test_warn_on_bad_tempo_and_route();
  test_route_table_full_warns();
  test_input_port_out_of_range_ignored();
  test_warn_on_unknown_command();
  test_param_state_echo();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_engine: all OK\n");
  }
  return arrangrr::test::failures();
}

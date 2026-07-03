// Host-layer unit tests: JSONL/human encoders (every branch) and the shell
// (command parsing, @tick queue, error paths). Links arrangrr_host.

#include <string>
#include <vector>

#include "arrangrr/transport/transport.hpp"
#include "jsonl.hpp"
#include "shell.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

void test_jsonl_all_kinds() {
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"noteon","ch":1,"note":60,"vel":100,"@":0})");
  CHECK(to_jsonl(OutEvent::midi(1, MidiMessage::note_off(4, 61, 64), 7)) ==
        R"({"ev":"midi-out","port":1,"msg":"noteoff","ch":5,"note":61,"vel":64,"@":7})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::cc(2, 7, 99), 3)) ==
        R"({"ev":"midi-out","port":0,"msg":"cc","ch":3,"cc":7,"val":99,"@":3})");
  CHECK(to_jsonl(OutEvent::midi(0, {0xC5, 12, 0}, 1)) ==
        R"({"ev":"midi-out","port":0,"msg":"program","ch":6,"num":12,"@":1})");
  // Pitch bend center: d1=0x00 d2=0x40 -> value 0.
  CHECK(to_jsonl(OutEvent::midi(0, {0xE0, 0x00, 0x40}, 2)) ==
        R"({"ev":"midi-out","port":0,"msg":"pitchbend","ch":1,"value":0,"@":2})");
  // Channel pressure has no dedicated encoder -> raw.
  CHECK(to_jsonl(OutEvent::midi(0, {0xD0, 42, 0}, 4)) ==
        R"({"ev":"midi-out","port":0,"msg":"raw","status":208,"d1":42,"d2":0,"@":4})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kClock), 40)) ==
        R"({"ev":"midi-out","port":0,"msg":"clock","@":40})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kStart), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"start","@":0})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kContinue), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"continue","@":0})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kStop), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"stop","@":0})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kActiveSensing), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"active_sensing","@":0})");
  CHECK(to_jsonl(OutEvent::midi(0, MidiMessage::realtime(midi::kSystemReset), 0)) ==
        R"({"ev":"midi-out","port":0,"msg":"reset","@":0})");

  CHECK(to_jsonl(OutEvent::transport(
            static_cast<std::uint16_t>(TransportState::kPlaying), 5)) ==
        R"({"ev":"transport","state":"playing","@":5})");
  CHECK(to_jsonl(OutEvent::transport(
            static_cast<std::uint16_t>(TransportState::kStopped), 5)) ==
        R"({"ev":"transport","state":"stopped","@":5})");
  CHECK(to_jsonl(OutEvent::transport(
            static_cast<std::uint16_t>(TransportState::kPaused), 5)) ==
        R"({"ev":"transport","state":"paused","@":5})");

  CHECK(to_jsonl(OutEvent::warn(WarnCode::kSchedulerFull, 1)) ==
        R"({"ev":"warn","code":"scheduler_full","@":1})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kRouteTableFull, 1)) ==
        R"({"ev":"warn","code":"route_table_full","@":1})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kUnknownCommand, 1)) ==
        R"({"ev":"warn","code":"unknown_command","@":1})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kBadArgument, 1)) ==
        R"({"ev":"warn","code":"bad_argument","@":1})");
}

void test_human_encoder() {
  // Smoke every branch of the human formatter (content is for eyes, so just
  // check the discriminating substring).
  auto has = [](const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
  };
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 0)), "note-on"));
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::note_off(0, 60, 64), 0)), "note-off"));
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::cc(0, 7, 1), 0)), "cc"));
  CHECK(has(to_human(OutEvent::midi(0, {0xC0, 1, 0}, 0)), "status"));
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::realtime(midi::kClock), 0)), "clock"));
  CHECK(has(to_human(OutEvent::transport(1, 0)), "transport playing"));
  CHECK(has(to_human(OutEvent::warn(WarnCode::kBadArgument, 0)), "WARN bad_argument"));
}

struct ShellFixture {
  std::vector<OutEvent> events;
  Shell shell{[this](const OutEvent& ev) { events.push_back(ev); }};
  std::string err;

  bool run(const char* line) { return shell.exec_line(line, err); }
  int midi_count() const {
    int n = 0;
    for (const OutEvent& e : events)
      if (e.kind == OutEvent::Kind::kMidi) ++n;
    return n;
  }
};

void test_shell_happy_path() {
  ShellFixture f;
  CHECK(f.run("# comment only"));
  CHECK(f.run(""));
  CHECK(f.run("port open in kbd"));
  CHECK(f.run("port open out virt as synth"));  // alias form
  CHECK(f.run("thru kbd synth"));
  CHECK(f.run("@0 midi send kbd 90 3C 64"));
  CHECK(f.run("@10 panic"));
  CHECK(f.run("advance 48"));
  CHECK(f.midi_count() == 5);  // noteon + noteoff + 3 mode CCs
  CHECK(f.shell.engine().now() == 48);
  CHECK(f.run("quit"));
  CHECK(f.shell.quit_requested());
}

void test_shell_tempo_and_bars() {
  ShellFixture f;
  CHECK(f.run("transport tempo 98.5"));
  CHECK(f.shell.engine().transport().bpm() == 9850);
  CHECK(f.run("transport tempo 120"));
  CHECK(f.shell.engine().transport().bpm() == 12000);
  CHECK(f.run("advance 2bars"));
  CHECK(f.shell.engine().now() == 2 * kTicksPerBar);
}

void test_shell_transport_and_clock() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("clock out synth"));
  CHECK(f.run("transport start"));
  CHECK(f.run("advance 40"));
  CHECK(f.run("transport stop"));
  CHECK(f.run("transport continue"));
  CHECK(f.run("clock out none"));
  int clocks = 0;
  for (const OutEvent& e : f.events)
    if (e.kind == OutEvent::Kind::kMidi && e.msg.status == midi::kClock) ++clocks;
  CHECK(clocks == 2);  // immediate F8 at start + F8 at tick 40
}

void test_shell_route_channel_remap() {
  ShellFixture f;
  CHECK(f.run("port open in kbd"));
  CHECK(f.run("port open out synth"));
  CHECK(f.run("route kbd:1 -> synth:5"));
  CHECK(f.run("midi send kbd 90 3C 64"));  // ch1 -> remapped to ch5
  CHECK(f.midi_count() == 1);
  CHECK(f.events.back().msg.channel() == 4);
  CHECK(f.run("midi send kbd 91 3C 64"));  // ch2: filtered out
  CHECK(f.midi_count() == 1);
}

void test_shell_error_paths() {
  ShellFixture f;
  CHECK(!f.run("bogus"));
  CHECK(!f.err.empty());
  CHECK(!f.run("transport tempo abc"));
  CHECK(!f.run("transport warp"));
  CHECK(!f.run("route nowhere -> nada"));
  CHECK(!f.run("thru a b"));
  CHECK(!f.run("clock out nothing"));
  CHECK(!f.run("midi send ghost 90 3C 64"));
  CHECK(f.run("port open in kbd"));
  CHECK(!f.run("midi send kbd ZZ"));
  CHECK(!f.run("midi send kbd 1234"));
  CHECK(!f.run("advance many"));
  CHECK(!f.run("@xyz panic"));
  CHECK(!f.run("@5"));
  CHECK(!f.run("port open sideways x"));
  CHECK(!f.run("port open in"));
  // @tick in the past: advance first, then schedule behind.
  CHECK(f.run("advance 100"));
  CHECK(!f.run("@50 panic"));
  // Port slots are bounded at kMaxPorts.
  CHECK(f.run("port open in a"));
  CHECK(f.run("port open in b"));
  CHECK(f.run("port open in c"));
  CHECK(!f.run("port open in d"));
}

void test_shell_pending_order_same_tick() {
  // Two lines queued on the same tick run in insertion order.
  ShellFixture f;
  CHECK(f.run("port open in kbd"));
  CHECK(f.run("port open out synth"));
  CHECK(f.run("thru kbd synth"));
  CHECK(f.run("@5 midi send kbd 90 3C 64"));
  CHECK(f.run("@5 midi send kbd 90 3E 64"));
  CHECK(f.run("advance 10"));
  CHECK(f.midi_count() == 2);
  CHECK(f.events[0].msg.d1 == 60);
  CHECK(f.events[1].msg.d1 == 62);
}

}  // namespace

int main() {
  test_jsonl_all_kinds();
  test_human_encoder();
  test_shell_happy_path();
  test_shell_tempo_and_bars();
  test_shell_transport_and_clock();
  test_shell_route_channel_remap();
  test_shell_error_paths();
  test_shell_pending_order_same_tick();
  if (arrangrr::test::failures() == 0) std::printf("test_host: all OK\n");
  return arrangrr::test::failures();
}

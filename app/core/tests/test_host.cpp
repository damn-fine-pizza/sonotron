// Host-layer unit tests: JSONL/human encoders (every branch) and the shell
// (command parsing, @tick queue, error paths). Links arrangrr_host.

#include <string>
#include <vector>

#include "alsa_midi.hpp"
#include "console.hpp"
#include "arrangrr/arranger/arranger.hpp"
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

void test_shell_parse_edges() {
  ShellFixture f;
  // Tempo grammar corner cases.
  CHECK(!f.run("transport tempo 120.505"));  // >2 fraction digits
  CHECK(!f.run("transport tempo 120."));     // empty fraction
  CHECK(!f.run("transport tempo .5"));       // empty whole part
  CHECK(!f.run("transport tempo 12x.5"));    // junk in whole part
  CHECK(f.run("transport tempo 98.5"));      // 1 fraction digit -> x10
  CHECK(f.shell.engine().transport().bpm() == 9850);
  // Channel suffix grammar.
  CHECK(f.run("port open in kbd"));
  CHECK(f.run("port open out synth"));
  CHECK(!f.run("route kbd:0 -> synth"));    // channels are 1-based
  CHECK(!f.run("route kbd:17 -> synth"));   // above 16
  CHECK(!f.run("route kbd:x -> synth"));    // not a number
  CHECK(f.run("route kbd -> synth:16"));    // boundary is valid
  // Numeric port references (indices work like names).
  CHECK(f.run("route 0 -> 0"));
  CHECK(!f.run("route 9 -> 0"));  // beyond kMaxPorts
  // Tokenizer: tabs and trailing comments.
  CHECK(f.run("advance\t10\t# trailing comment"));
  CHECK(f.shell.engine().now() == 10);
  // midi send happy path via numeric port + single-digit hex.
  CHECK(f.run("midi send 0 90 3C 7F"));
  CHECK(f.run("midi send 0 80 3C 0"));
}

void test_shell_track_commands() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("track new bass synth:3 bass"));
  CHECK(f.run("track new drum synth:10 drums"));
  CHECK(f.run("track length drum 4"));
  CHECK(f.run("track step bass 1 C2 100 120"));
  CHECK(f.run("track step drum 1 36 110"));         // numeric note, default gate
  CHECK(f.run("track step drum 3 F#1"));            // default vel+gate
  CHECK(f.run("track step drum 3 clear"));
  CHECK(f.run("track mute drum on"));
  CHECK(f.run("track solo bass on"));
  CHECK(f.run("track solo bass off"));
  CHECK(f.run("track mute drum off"));
  CHECK(f.run("transport start"));
  // bass C2=36 on ch3, drum 36 on ch10 both fire at step 0.
  CHECK(f.midi_count() == 2);
  const Track* bass = f.shell.engine().timeline().track(0);
  CHECK(bass != nullptr && bass->channel == 2 && bass->role == TrackRole::kBass);

  // Error paths.
  CHECK(!f.run("track new x nowhere"));
  CHECK(!f.run("track new x synth:1 wizard"));
  CHECK(!f.run("track step ghost 1 C2"));
  CHECK(!f.run("track step bass 0 C2"));    // steps are 1-based
  CHECK(!f.run("track step bass 65 C2"));   // beyond kMaxStepsPerTrack
  CHECK(!f.run("track step bass 1 H2"));    // no such note letter
  CHECK(!f.run("track step bass 1 C2 0"));  // vel out of range
  CHECK(!f.run("track step bass 1 C2 100 0"));  // gate zero
  CHECK(!f.run("track length bass nope"));
  CHECK(!f.run("track mute bass maybe"));
  CHECK(!f.run("track fly bass 1"));
}

void test_note_name_parsing() {
  ShellFixture f;
  CHECK(f.run("port open out s"));
  CHECK(f.run("track new t s:1"));
  // C4 = 60 (scientific pitch), C-1 = 0, G9 = 127, flats and sharps.
  CHECK(f.run("track step t 1 C4"));
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 60);
  CHECK(f.run("track step t 1 C-1"));
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 0);
  CHECK(f.run("track step t 1 G9"));
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 127);
  CHECK(f.run("track step t 1 Bb2"));
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 46);
  CHECK(f.run("track step t 1 F#3"));
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 54);
  CHECK(!f.run("track step t 1 G#9"));  // above 127
  CHECK(!f.run("track step t 1 Cb-1"));  // below 0
  CHECK(f.run("track step t 1 C"));      // octave optional: defaults to 4
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 60);
  CHECK(!f.run("track step t 1 128"));   // numeric out of range
}

void test_shell_chord_commands() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("chord out synth:1"));
  CHECK(f.run("key C major"));
  CHECK(!f.shell.prefer_flats());
  CHECK(f.run("play D"));  // Dm7: chord event + 4 note-ons
  int chords = 0;
  for (const OutEvent& o : f.events)
    if (o.kind == OutEvent::Kind::kChord) ++chords;
  CHECK(chords == 1 && f.midi_count() == 4);
  CHECK(f.run("chord play G 7 90"));  // explicit quality + velocity
  CHECK(f.run("chord stop"));
  CHECK(f.run("chord hold off"));
  CHECK(f.run("chord hold on"));
  // Flat-side key flips the spelling preference.
  CHECK(f.run("key F major"));
  CHECK(f.shell.prefer_flats());
  CHECK(f.run("key A minor"));
  CHECK(!f.shell.prefer_flats());
  CHECK(f.run("key Bb major"));
  CHECK(f.shell.prefer_flats());
  // Errors.
  CHECK(!f.run("key H major"));
  CHECK(!f.run("key C ionianish"));
  CHECK(!f.run("play X"));
  CHECK(!f.run("play D maj7 999"));
  CHECK(!f.run("chord out nowhere"));
  CHECK(!f.run("chord hold maybe"));
  CHECK(!f.run("chord flip"));
}

void test_shell_chord_modes_cli() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("chord out synth"));
  CHECK(f.run("chord mode single"));
  CHECK(f.run("play F#"));  // chromatic allowed in absolute mode
  CHECK(f.midi_count() == 3);
  CHECK(f.run("chord mode shell"));
  CHECK(f.run("play C E Bb"));  // multi-note completion
  CHECK(f.run("play D F C5 80"));  // trailing velocity after notes
  CHECK(f.run("chord mode diatonic"));
  CHECK(!f.run("chord mode wizard"));
  CHECK(!f.run("play C-1"));  // note 0 cannot be packed
}

void test_shell_style_commands() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("style route drums synth:10"));
  CHECK(f.run("style route bass synth:2"));
  CHECK(f.run("style section varB"));
  CHECK(f.shell.engine().arranger().current() == SectionType::kVarB);
  CHECK(f.run("style section fillA"));
  CHECK(f.run("style section intro1"));
  CHECK(f.run("style section ending1"));
  // Errors.
  CHECK(!f.run("style load funkytown"));
  CHECK(!f.run("style route wizard synth:1"));
  CHECK(!f.run("style route bass nowhere"));
  CHECK(!f.run("style section chorus"));
  CHECK(!f.run("style dance"));
  CHECK(!f.run("style load"));
}

void test_jsonl_section_rendering() {
  CHECK(to_jsonl(OutEvent::section(static_cast<std::uint16_t>(SectionType::kVarB), 7)) ==
        R"({"ev":"section","name":"varB","@":7})");
  CHECK(to_jsonl(OutEvent::section(99, 0)) == R"({"ev":"section","name":"?","@":0})");
  CHECK(to_human(OutEvent::section(static_cast<std::uint16_t>(SectionType::kFillA), 7))
            .find("section fillA") != std::string::npos);
}

void test_jsonl_chord_rendering() {
  // ii in C major: D4 input, min7 -> {"in":"D4","out":"Dm7","deg":"ii"}.
  const OutEvent ev = OutEvent::chord(0, 1, static_cast<std::uint8_t>(ChordQuality::kMin7),
                                      62, 4, 100, 0);
  CHECK(to_jsonl(ev) == R"({"ev":"chord","in":"D4","out":"Dm7","deg":"ii","@":0})");
  // Flat spelling: Bb root.
  const OutEvent bb = OutEvent::chord(0, 0, static_cast<std::uint8_t>(ChordQuality::kMaj7),
                                      70, 4, 100, 5);
  CHECK(to_jsonl(bb, true) == R"({"ev":"chord","in":"Bb4","out":"Bbmaj7","deg":"I","@":5})");
  CHECK(to_jsonl(bb, false) == R"({"ev":"chord","in":"A#4","out":"A#maj7","deg":"I","@":5})");
  // Half-diminished renders lowercase with m7b5, dominant uppercase.
  const OutEvent halfdim = OutEvent::chord(
      0, 6, static_cast<std::uint8_t>(ChordQuality::kHalfDim7), 71, 4, 100, 0);
  CHECK(to_jsonl(halfdim) == R"({"ev":"chord","in":"B4","out":"Bm7b5","deg":"viim7b5","@":0})");
  const std::string human = to_human(ev);
  CHECK(human.find("chord Dm7 (ii)") != std::string::npos);
  // Keyless modes render degree "-".
  const OutEvent keyless = OutEvent::chord(
      0, kNoDegree, static_cast<std::uint8_t>(ChordQuality::kMaj), 66, 3, 100, 0);
  CHECK(to_jsonl(keyless) == R"({"ev":"chord","in":"F#4","out":"F#","deg":"-","@":0})");
}

void test_shell_seq_commands() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("chord out synth"));
  CHECK(f.run("key C major"));
  CHECK(f.run("seq new verse"));
  CHECK(f.run("seq add D 1bar"));
  CHECK(f.run("seq add G maj7 2bars"));  // explicit quality + duration
  CHECK(f.run("seq add C 4beats"));
  CHECK(f.run("seq add A"));  // default: smart quality, 1 bar
  const ChordSequence* seq = f.shell.engine().sequences().current();
  CHECK(seq->count() == 4);
  CHECK(seq->step(1).duration == 2 * kTicksPerBar);
  CHECK(seq->step(1).quality_ovr == static_cast<std::int8_t>(ChordQuality::kMaj7));
  CHECK(seq->step(2).duration == 4 * kTicksPerBeat);
  CHECK(f.run("seq del 1"));
  CHECK(seq->count() == 3);
  CHECK(f.run("seq loop on"));
  CHECK(seq->loop);
  CHECK(f.run("seq transpose to G major"));
  CHECK(seq->key.root_pc == 7);
  CHECK(f.run("seq transpose to Bb"));  // mode kept
  CHECK(seq->key.root_pc == 10);
  CHECK(f.run("seq transpose +2"));
  CHECK(seq->key.root_pc == 0);
  CHECK(f.run("seq transpose -1"));
  CHECK(seq->key.root_pc == 11);
  CHECK(f.run("seq play"));
  CHECK(f.run("transport start"));
  CHECK(f.run("seq stop"));  // stops playback (not recording)
  CHECK(f.run("seq clear"));
  CHECK(seq->count() == 0);
  // Recording flow.
  CHECK(f.run("seq new chorus"));
  CHECK(f.run("seq use verse"));
  CHECK(f.run("seq use 1"));  // numeric index works too
  CHECK(f.run("seq rec"));
  CHECK(f.shell.engine().sequences().recording());
  CHECK(f.run("play D"));
  CHECK(f.run("advance 3840"));
  CHECK(f.run("play G"));
  CHECK(f.run("advance 3840"));
  CHECK(f.run("seq stop"));
  CHECK(!f.shell.engine().sequences().recording());
  CHECK(f.shell.engine().sequences().current()->count() == 2);
  // Errors.
  CHECK(!f.run("seq use ghost"));
  CHECK(!f.run("seq add X 1bar"));
  CHECK(!f.run("seq add D 3parsecs"));
  CHECK(!f.run("seq transpose to H"));
  CHECK(!f.run("seq transpose sideways"));
  CHECK(!f.run("seq transpose +0"));
  CHECK(!f.run("seq transpose +30"));
  CHECK(!f.run("seq del 0"));
  CHECK(!f.run("seq del abc"));
  CHECK(!f.run("seq warp"));
  CHECK(!f.run("seq"));
}

void test_line_editor() {
  LineEditor ed;
  auto type = [&](const char* text) {
    LineEditor::Result last;
    for (const char* c = text; *c; ++c) last = ed.feed(static_cast<std::uint8_t>(*c));
    return last;
  };
  // Plain typing + Enter completes a line.
  LineEditor::Result r = type("play D\r");
  CHECK(r.line && *r.line == "play D");
  CHECK(ed.buffer().empty());
  // Arrows: left + insert in the middle.
  type("ply");
  type("\x1b[D");  // left
  type("a");
  CHECK(ed.buffer() == "play");
  CHECK(ed.cursor() == 3);
  type("\x1b[C");  // right
  CHECK(ed.cursor() == 4);
  // Backspace.
  type("\x7f");
  CHECK(ed.buffer() == "pla");
  // Home/End.
  type("\x1b[H");
  CHECK(ed.cursor() == 0);
  type("\x1b[F");
  CHECK(ed.cursor() == 3);
  // Delete key (CSI 3~) at home removes the first char.
  type("\x1b[H");
  type("\x1b[3~");
  CHECK(ed.buffer() == "la");
  // Ctrl-U clears.
  type("\x15");
  CHECK(ed.buffer().empty());
  // History: up recalls, down comes back to the stashed fresh line.
  type("second\r");
  type("thi");
  type("\x1b[A");  // up -> "second"
  CHECK(ed.buffer() == "second");
  type("\x1b[A");  // up -> "play D"
  CHECK(ed.buffer() == "play D");
  type("\x1b[B");  // down -> "second"
  CHECK(ed.buffer() == "second");
  type("\x1b[B");  // down -> fresh "thi"
  CHECK(ed.buffer() == "thi");
  // Ctrl-C clears a non-empty line; on empty -> quit. Ctrl-D quits on empty.
  r = type("\x03");
  CHECK(!r.quit && ed.buffer().empty());
  r = type("\x03");
  CHECK(r.quit);
  r = type("\x04");
  CHECK(r.quit);
  // Empty Enter completes an empty line without polluting history.
  r = type("\r");
  CHECK(r.line && r.line->empty());
  type("\x1b[A");
  CHECK(ed.buffer() == "second");
}

void test_help_command() {
  ShellFixture f;
  CHECK(f.run("help"));
  CHECK(f.run("help chord"));
  CHECK(f.run("help seq"));
  CHECK(f.run("help style"));
  CHECK(f.run("help track"));
  CHECK(f.run("help midi"));
  CHECK(f.run("help nonsense"));  // unknown topic falls back to the overview
  CHECK(f.events.empty());        // help never touches the engine
}

void test_alsa_null_state_is_safe() {
  // Without open(): every entry point must be a graceful no-op. Covers the
  // guard branches without needing a sequencer device.
  AlsaMidi alsa;
  CHECK(alsa.poll_fd_count() == 0);
  struct pollfd fds[4];
  CHECK(alsa.fill_poll_fds(fds, 4) == 0);
  alsa.send(0, MidiMessage::note_on(0, 60, 100));  // no port map -> ignored
  int called = 0;
  alsa.drain_input([&](std::uint8_t, const std::uint8_t*, std::size_t) { ++called; });
  CHECK(called == 0);
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
  test_shell_parse_edges();
  test_shell_track_commands();
  test_note_name_parsing();
  test_shell_chord_commands();
  test_shell_seq_commands();
  test_shell_chord_modes_cli();
  test_shell_style_commands();
  test_jsonl_section_rendering();
  test_jsonl_chord_rendering();
  test_help_command();
  test_line_editor();
  test_alsa_null_state_is_safe();
  test_shell_pending_order_same_tick();
  if (arrangrr::test::failures() == 0) std::printf("test_host: all OK\n");
  return arrangrr::test::failures();
}

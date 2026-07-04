// Host-layer unit tests: JSONL/human encoders (every branch) and the shell
// (command parsing, @tick queue, error paths). Links arrangrr_host.

#include <algorithm>
#include <string>
#include <vector>

#include "alsa_midi.hpp"
#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/transport/transport.hpp"
#include "console.hpp"
#include "jsonl.hpp"
#include "kitty_keys.hpp"
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

  CHECK(to_jsonl(OutEvent::transport(static_cast<std::uint16_t>(TransportState::kPlaying), 5)) ==
        R"({"ev":"transport","state":"playing","@":5})");
  CHECK(to_jsonl(OutEvent::transport(static_cast<std::uint16_t>(TransportState::kStopped), 5)) ==
        R"({"ev":"transport","state":"stopped","@":5})");
  CHECK(to_jsonl(OutEvent::transport(static_cast<std::uint16_t>(TransportState::kPaused), 5)) ==
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

  // H3 enrichment: note lines carry the note name; ch10 (0-based 9) shows the
  // GM drum name, with a scientific-pitch fallback for notes without one.
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 0)), "C4"));
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::note_on(9, 36, 100), 0)), "Kick"));
  CHECK(has(to_human(OutEvent::midi(0, MidiMessage::note_on(9, 60, 100), 0)), "C4"));
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
    for (const OutEvent& e : events) {
      if (e.kind == OutEvent::Kind::kMidi) {
        ++n;
      }
    }
    return n;
  }
};

bool block_contains(const std::vector<std::string>& lines, const std::string& needle) {
  for (const std::string& line : lines) {
    if (line.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

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
  for (const OutEvent& e : f.events) {
    if (e.kind == OutEvent::Kind::kMidi && e.msg.status == midi::kClock) {
      ++clocks;
    }
  }
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
  CHECK(!f.run("route kbd:0 -> synth"));   // channels are 1-based
  CHECK(!f.run("route kbd:17 -> synth"));  // above 16
  CHECK(!f.run("route kbd:x -> synth"));   // not a number
  CHECK(f.run("route kbd -> synth:16"));   // boundary is valid
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
  CHECK(f.run("track step drum 1 36 110"));  // numeric note, default gate
  CHECK(f.run("track step drum 3 F#1"));     // default vel+gate
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
  CHECK(!f.run("track step bass 0 C2"));        // steps are 1-based
  CHECK(!f.run("track step bass 65 C2"));       // beyond kMaxStepsPerTrack
  CHECK(!f.run("track step bass 1 H2"));        // no such note letter
  CHECK(!f.run("track step bass 1 C2 0"));      // vel out of range
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
  CHECK(!f.run("track step t 1 G#9"));   // above 127
  CHECK(!f.run("track step t 1 Cb-1"));  // below 0
  CHECK(f.run("track step t 1 C"));      // octave optional: defaults to 4
  CHECK(f.shell.engine().timeline().track(0)->steps[0].note == 60);
  CHECK(!f.run("track step t 1 128"));  // numeric out of range
}

void test_shell_chord_commands() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("chord out synth:1"));
  CHECK(f.run("key C major"));
  CHECK(!f.shell.prefer_flats());
  CHECK(f.run("play D"));  // Dm7: chord event + 4 note-ons
  int chords = 0;
  for (const OutEvent& o : f.events) {
    if (o.kind == OutEvent::Kind::kChord) {
      ++chords;
    }
  }
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
  CHECK(f.run("play C E Bb"));     // multi-note completion
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
  const OutEvent ev =
      OutEvent::chord(0, 1, static_cast<std::uint8_t>(ChordQuality::kMin7), 62, 4, 100, 0);
  CHECK(to_jsonl(ev) == R"({"ev":"chord","in":"D4","out":"Dm7","deg":"ii","@":0})");
  // Flat spelling: Bb root.
  const OutEvent bb =
      OutEvent::chord(0, 0, static_cast<std::uint8_t>(ChordQuality::kMaj7), 70, 4, 100, 5);
  CHECK(to_jsonl(bb, true) == R"({"ev":"chord","in":"Bb4","out":"Bbmaj7","deg":"I","@":5})");
  CHECK(to_jsonl(bb, false) == R"({"ev":"chord","in":"A#4","out":"A#maj7","deg":"I","@":5})");
  // Half-diminished renders lowercase with m7b5, dominant uppercase.
  const OutEvent halfdim =
      OutEvent::chord(0, 6, static_cast<std::uint8_t>(ChordQuality::kHalfDim7), 71, 4, 100, 0);
  CHECK(to_jsonl(halfdim) == R"({"ev":"chord","in":"B4","out":"Bm7b5","deg":"viim7b5","@":0})");
  const std::string human = to_human(ev);
  CHECK(human.find("chord Dm7 (ii)") != std::string::npos);
  // Keyless modes render degree "-".
  const OutEvent keyless =
      OutEvent::chord(0, kNoDegree, static_cast<std::uint8_t>(ChordQuality::kMaj), 66, 3, 100, 0);
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
    for (const char* c = text; *c; ++c) {
      last = ed.feed(static_cast<std::uint8_t>(*c));
    }
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

void test_console_geometry() {
  // panel_rows() is the pane the grid paints into: everything above the fixed
  // status + input pair. An inactive console keeps the default 24 rows.
  Console console;  // never init()'d: stays inactive, so no terminal writes
  CHECK(console.panel_rows() == 24 - Console::kStatusInputRows);

  // The uniform grid tiles to EXACTLY the requested rows (no bands, no scroll
  // region), and an all-hidden manager clears the pane (empty block).
  PanelManager pm;
  CHECK(pm.combined_lines(80, 12).empty());  // nothing open -> hidden
  pm.open(PanelId::kEvents);
  for (const int rows : {5, 8, 12, 24, 40}) {
    CHECK(static_cast<int>(pm.combined_lines(80, rows).size()) == rows);
  }

  // Scrolling panels keep a bounded backlog and render only their cell tail, so
  // a long log reads like a scroll without a real scroll region.
  for (int i = 0; i < 40; ++i) {
    pm.append_line(PanelId::kEvents, "line" + std::to_string(i));
  }
  const std::vector<std::string> grid = pm.combined_lines(80, 6);  // title + 5 content rows
  CHECK(static_cast<int>(grid.size()) == 6);
  CHECK(block_contains(grid, "line39"));  // newest tail line is shown
  CHECK(!block_contains(grid, "line0"));  // oldest is scrolled off
}

void test_help_command() {
  ShellFixture f;
  CHECK(f.run("help"));
  CHECK(f.run("help chord"));
  CHECK(f.run("help seq"));
  CHECK(f.run("help style"));
  CHECK(f.run("help track"));
  CHECK(f.run("help midi"));
  CHECK(f.run("help panel"));
  CHECK(f.run("help piano"));
  CHECK(f.run("help notes"));
  CHECK(f.run("help nonsense"));  // unknown topic falls back to the overview
  CHECK(!f.run("help close"));    // lifecycle moved: migration hint errors out
  CHECK(!f.run("help open"));
  CHECK(f.events.empty());  // help never touches the engine
}

void test_help_panel_hook() {
  ShellFixture f;
  std::vector<std::string> panel{"sentinel"};
  int calls = 0;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    ++calls;
    return true;
  });
  // help <topic> fills and opens the help panel; the hook receives the
  // combined block: title rule first, topic content after it.
  CHECK(f.run("help chord"));
  CHECK(calls == 1 && panel.size() >= 2);
  CHECK(panel[0].find("-- menu") != std::string::npos);  // the panel is the contextual MENU
  CHECK(block_contains(panel, "help: chord"));
  // Lifecycle now lives under `panel ...`.
  CHECK(f.run("panel close help"));
  CHECK(calls == 2 && panel.empty());
  CHECK(f.run("panel open help"));
  CHECK(calls == 3 && block_contains(panel, "help: chord"));  // content survives close
  // A different topic replaces the content.
  CHECK(f.run("help midi"));
  CHECK(block_contains(panel, "help: midi"));
  // Opening the help panel before any topic was shown -> overview.
  ShellFixture fresh;
  std::vector<std::string> fresh_panel;
  fresh.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    fresh_panel = lines;
    return true;
  });
  CHECK(fresh.run("panel open help"));
  CHECK(block_contains(fresh_panel, "help  (help <topic>"));
}

void test_panel_commands() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });

  // Index of the first line whose text contains `needle` (or size() if none).
  auto line_index = [&](const char* needle) {
    for (std::size_t i = 0; i < panel.size(); ++i) {
      if (panel[i].find(needle) != std::string::npos) {
        return i;
      }
    }
    return panel.size();
  };

  // Coexistence: menu and piano share the grid; menu is higher in the order so
  // it paints above the piano (earlier in the top-to-bottom block).
  CHECK(f.run("panel open piano"));
  CHECK(block_contains(panel, "-- piano"));
  CHECK(f.run("help chord"));
  CHECK(block_contains(panel, "-- menu") && block_contains(panel, "-- piano"));
  CHECK(line_index("-- menu") < line_index("-- piano"));

  // Rename + alias: `panel open menu` is canonical, `panel open help` still
  // targets the same panel, and the title renders "-- menu ...".
  CHECK(f.run("panel close all"));
  CHECK(f.run("panel open menu"));
  CHECK(block_contains(panel, "-- menu"));
  CHECK(f.run("panel close all"));
  CHECK(f.run("panel open help"));  // backward-compatible alias
  CHECK(block_contains(panel, "-- menu"));
  CHECK(f.run("panel open piano"));  // restore the state the toggle test expects

  // toggle / close all.
  CHECK(f.run("panel toggle piano"));
  CHECK(!block_contains(panel, "-- piano"));
  CHECK(f.run("panel toggle piano"));
  CHECK(block_contains(panel, "-- piano"));
  CHECK(f.run("panel close all"));
  CHECK(panel.empty());

  // Focus: focusing opens, the title carries the '*' marker, repl clears it.
  CHECK(f.run("panel focus piano"));
  CHECK(block_contains(panel, "-- piano* "));
  CHECK(f.run("panel focus repl"));
  CHECK(block_contains(panel, "-- piano") && !block_contains(panel, "-- piano* "));
  CHECK(f.run("panel focus next"));
  CHECK(block_contains(panel, "-- piano* "));
  CHECK(f.run("panel focus next"));  // past the last visible -> back to repl
  CHECK(!block_contains(panel, "-- piano* "));

  // Errors: unknown panel / unknown subcommand / missing argument.
  CHECK(!f.run("panel open nonsense"));
  CHECK(!f.run("panel nonsense"));
  CHECK(!f.run("panel open"));
  CHECK(!f.run("panel"));

  // panel list / status are informational and never touch the engine.
  CHECK(f.run("panel list"));
  CHECK(f.run("panel status"));
  CHECK(f.run("panel help"));
  CHECK(f.events.empty());
}

void test_piano_commands() {
  ShellFixture f;

  // Octave: absolute, relative, clamped range.
  CHECK(f.run("piano octave 3"));
  CHECK(f.shell.piano_state().base_octave == 3);
  CHECK(f.run("piano octave up"));
  CHECK(f.shell.piano_state().base_octave == 4);
  CHECK(f.run("piano octave down"));
  CHECK(f.shell.piano_state().base_octave == 3);
  CHECK(!f.run("piano octave 10"));  // out of -1..9
  CHECK(!f.run("piano octave -2"));
  CHECK(!f.run("piano octave nonsense"));
  CHECK(f.run("piano octave 9"));
  CHECK(!f.run("piano octave up"));  // clamp rejects loudly at the edge
  CHECK(f.run("piano octave -1"));
  CHECK(!f.run("piano octave down"));

  // Channel: user-facing 1-based, internal 0-based.
  CHECK(f.run("piano channel 2"));
  CHECK(f.shell.piano_state().channel == 1);
  CHECK(!f.run("piano channel 0"));
  CHECK(!f.run("piano channel 17"));

  // Velocity.
  CHECK(f.run("piano velocity 100"));
  CHECK(f.shell.piano_state().velocity == 100);
  CHECK(!f.run("piano velocity 0"));
  CHECK(!f.run("piano velocity 128"));

  // Keymap and views.
  CHECK(f.run("piano keymap default"));
  CHECK(!f.run("piano keymap qwertz"));
  CHECK(f.run("piano view active-notes"));
  CHECK(f.shell.piano_state().view == PianoView::kActiveNotes);
  CHECK(f.run("piano view event-log"));
  CHECK(f.run("piano view keyboard"));
  CHECK(!f.run("piano view nonsense"));

  // No piano command may touch the engine's MIDI output.
  CHECK(f.midi_count() == 0);
}

void test_notes_names_commands() {
  ShellFixture f;
  CHECK(f.shell.piano_state().note_naming == NoteNaming::kCde);
  CHECK(f.run("notes names doremi"));
  CHECK(f.shell.piano_state().note_naming == NoteNaming::kDoReMi);
  CHECK(f.run("notes names cde"));
  CHECK(f.shell.piano_state().note_naming == NoteNaming::kCde);
  CHECK(f.run("notes names toggle"));
  CHECK(f.shell.piano_state().note_naming == NoteNaming::kDoReMi);
  CHECK(!f.run("notes names nonsense"));
  CHECK(!f.run("notes"));
}

void test_filter_view_commands() {
  ShellFixture f;

  CHECK(f.run("filter channel 2"));
  CHECK(f.run("filter port 0"));
  CHECK(f.run("filter event note-on"));
  CHECK(f.run("filter event note-off"));
  CHECK(f.run("filter clear"));
  CHECK(!f.run("filter channel 17"));
  CHECK(!f.run("filter port 99"));
  CHECK(!f.run("filter event nonsense"));
  CHECK(!f.run("filter"));

  CHECK(f.run("view show note-names off"));
  CHECK(f.run("view show note-numbers off"));
  CHECK(f.run("view show velocity off"));
  CHECK(f.run("view show channel on"));
  CHECK(f.run("view show port on"));
  CHECK(f.run("view show-octaves boundary"));
  CHECK(f.run("view show-octaves all"));
  CHECK(f.run("view show-octaves none"));
  CHECK(f.run("view show drum-names off"));
  CHECK(f.run("view clear"));
  CHECK(!f.run("view show nonsense on"));
  CHECK(!f.run("view show velocity maybe"));
  CHECK(!f.run("view"));

  // H3 filters: drums / melodic / velocity floor.
  CHECK(f.run("filter drums"));
  CHECK(f.run("filter melodic"));
  CHECK(f.run("filter velocity >= 80"));
  CHECK(!f.run("filter velocity >= 999"));
  CHECK(!f.run("filter velocity 80"));  // missing >=
  CHECK(f.run("filter clear"));
  CHECK(f.midi_count() == 0);  // filters never touch the engine
}

void test_theme_colors_layout_commands() {
  ShellFixture f;

  // theme: list / current / set, with a clear rejection of an unknown name.
  CHECK(f.run("theme list"));
  CHECK(f.run("theme current"));
  CHECK(f.run("theme set mono"));
  CHECK(f.shell.ui_style().theme_name() == "mono");
  CHECK(f.run("theme set dark"));
  CHECK(f.run("theme set high-contrast"));
  CHECK(!f.run("theme set nonexistent"));
  CHECK(f.shell.ui_style().theme_name() == "high-contrast");  // unchanged
  CHECK(!f.run("theme bogus"));
  CHECK(!f.run("theme"));

  // colors: on / off / toggle.
  CHECK(f.run("colors on"));
  CHECK(f.shell.ui_style().colors_enabled());
  CHECK(f.run("colors off"));
  CHECK(!f.shell.ui_style().colors_enabled());
  CHECK(f.run("colors toggle"));
  CHECK(f.shell.ui_style().colors_enabled());
  CHECK(!f.run("colors maybe"));
  CHECK(!f.run("colors"));

  // panel layout: 1 / 2 / toggle (panels per row).
  CHECK(f.run("panel layout 2"));
  CHECK(f.shell.panels().per_row() == 2);
  CHECK(f.run("panel layout 1"));
  CHECK(f.shell.panels().per_row() == 1);
  CHECK(f.run("panel layout toggle"));
  CHECK(f.shell.panels().per_row() == 2);
  CHECK(!f.run("panel layout diagonal"));
  CHECK(!f.run("panel layout"));

  CHECK(f.midi_count() == 0);
}

void test_contextual_panel() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });

  // Opening the help panel with no topic shows the contextual overview plus
  // the always-present navigation footer.
  CHECK(f.run("panel open help"));
  CHECK(block_contains(panel, "nav: TAB focus"));
  CHECK(block_contains(panel, "help  (help <topic>"));

  // An explicit topic pins: it survives re-renders in the same mode.
  CHECK(f.run("help chord"));
  CHECK(block_contains(panel, "help: chord"));
  CHECK(f.run("panel open piano"));  // a re-render, still REPL mode
  CHECK(block_contains(panel, "help: chord"));

  // Entering piano play mode unpins and the contextual content follows.
  CHECK(f.run("panel focus piano"));
  CHECK(block_contains(panel, "white: A S D F G H J K L"));
  CHECK(block_contains(panel, "nav: TAB focus"));

  // Back to the REPL: contextual overview returns.
  CHECK(f.run("panel focus repl"));
  CHECK(block_contains(panel, "help  (help <topic>"));
}

void test_ctrl_p_play_stop() {
  ShellFixture f;
  constexpr std::uint8_t kCtrlP = 0x10;
  CHECK(!f.shell.engine().transport().playing());

  CHECK(f.shell.handle_ui_key(kCtrlP));  // -> play
  CHECK(f.shell.engine().transport().playing());
  CHECK(f.shell.handle_ui_key(kCtrlP));  // -> stop
  CHECK(!f.shell.engine().transport().playing());

  // Global: works with piano focus too, and never leaks a note.
  CHECK(f.run("panel focus piano"));
  f.events.clear();
  CHECK(f.shell.handle_ui_key(kCtrlP));
  CHECK(f.shell.engine().transport().playing());
  CHECK(f.midi_count() == 0);
}

void test_styles_panel_chooser() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("style load basic"));
  constexpr std::uint8_t kBacktick = 0x60;      // focus shortcut for the styles panel
  constexpr std::uint8_t kCtrlApplyNow = 0x1C;  // CTRL+backslash: apply now
  constexpr std::uint8_t kEnter = 0x0D;

  // Backtick (`) is a FOCUS SHORTCUT: it gives focus to the styles panel and
  // opens it (the chooser dissolved into the always-present styles panel).
  CHECK(!f.shell.styles_focused());
  CHECK(f.shell.handle_ui_key(kBacktick));
  CHECK(f.shell.styles_focused());
  CHECK(f.shell.panels().visible(PanelId::kStyles));

  // The styles panel renders the chooser: style:/section:/hint + a `key:` line.
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "-- styles"));
  CHECK(block_contains(panel, "ENTER next-bar"));
  CHECK(block_contains(panel, "key:"));

  // A digit feeds the filter only while the styles panel is focused.
  CHECK(f.shell.handle_ui_key('0'));
  CHECK(f.shell.chooser().filter() == "0");
  CHECK(f.shell.handle_ui_key(0x7F));  // backspace clears the filter
  CHECK(f.shell.chooser().filter().empty());

  // Move the section highlight, then CTRL+\ applies immediately (transport
  // stopped) and KEEPS focus on the styles panel.
  f.shell.chooser_nav_section(+1);
  const SectionType want = f.shell.chooser().selected_section();
  f.events.clear();
  CHECK(f.shell.handle_ui_key(kCtrlApplyNow));
  CHECK(f.shell.styles_focused());  // applying keeps focus
  CHECK(f.shell.engine().arranger().current() == want);
  bool saw_section = false;
  for (const OutEvent& e : f.events) {
    saw_section = saw_section || e.kind == OutEvent::Kind::kSection;
  }
  CHECK(saw_section);

  // ENTER also applies and stays focused.
  f.shell.chooser_nav_section(+1);
  const SectionType want2 = f.shell.chooser().selected_section();
  CHECK(f.shell.handle_ui_key(kEnter));
  CHECK(f.shell.styles_focused());
  CHECK(f.shell.engine().arranger().current() == want2);

  // ESC (chooser_cancel) drops focus back to the REPL without a switch.
  f.events.clear();
  f.shell.chooser_cancel();
  CHECK(!f.shell.styles_focused());
  CHECK(f.events.empty());

  // Backtick from piano focus jumps straight to the styles panel.
  CHECK(f.run("panel focus piano"));
  CHECK(f.shell.handle_ui_key(kBacktick));
  CHECK(f.shell.styles_focused());
}

void test_ctrl_z_layout() {
  // CTRL+Z (0x1A) toggles the grid between 1 and 2 panels per row (global).
  ShellFixture f;
  constexpr std::uint8_t kCtrlZ = 0x1A;
  CHECK(f.shell.panels().per_row() == 1);
  CHECK(f.shell.handle_ui_key(kCtrlZ));
  CHECK(f.shell.panels().per_row() == 2);
  CHECK(f.shell.handle_ui_key(kCtrlZ));
  CHECK(f.shell.panels().per_row() == 1);
}

void test_styles_key_line() {
  // The styles panel carries a live `key:` line read from the chord engine.
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open styles"));
  CHECK(f.run("key F major"));
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "key: F major"));
  CHECK(f.run("key A minor"));
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "key: A minor"));
}

void test_tab_number_focus() {
  // TAB followed by a digit focuses the panel at that 1-based grid position.
  ShellFixture f;
  CHECK(f.run("panel open piano"));   // grid position 1 (bottom)
  CHECK(f.run("panel open styles"));  // grid position 2
  // TAB arms the jump; the next digit selects the panel.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.handle_ui_key('2'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kPanel);
  CHECK(f.shell.panels().focused_panel() == PanelId::kStyles);
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.handle_ui_key('1'));
  CHECK(f.shell.panels().focused_panel() == PanelId::kPiano);
}

// Byte values of the four stepping keys (mirrors the shell's constexprs).
constexpr std::uint8_t kStepSectionPrev = 0x2D;  // '-'
constexpr std::uint8_t kStepSectionNext = 0x3D;  // '='
constexpr std::uint8_t kStepStylePrev = 0x5F;    // '_'
constexpr std::uint8_t kStepStyleNext = 0x2B;    // '+'

void test_style_section_stepping() {
  // Stepping the variation (section) while the styles panel is focused advances
  // a PENDING selection with a debounced apply — no wrap at either end. Derives
  // first/last from the loaded style so it holds for any section vocabulary.
  ShellFixture f;
  CHECK(f.run("style load basic"));
  const auto& sections = styles::kBuiltins[0]->sections;
  const SectionType first = sections[0].type;
  const SectionType last = sections[sections.size() - 1].type;
  const int section_count = static_cast<int>(sections.size());

  // Focusing the styles panel seeds the chooser from the arranger.
  CHECK(f.run("panel focus styles"));
  CHECK(f.shell.styles_focused());
  CHECK(!f.shell.style_step_pending());
  const std::uint32_t gen0 = f.shell.style_step_gen();

  // '=' next: pending advances and the gen bumps, but the arranger does NOT
  // switch yet (the apply is debounced).
  const SectionType before = f.shell.engine().arranger().current();
  CHECK(f.shell.handle_ui_key(kStepSectionNext));
  CHECK(f.shell.style_step_pending());
  CHECK(f.shell.style_step_gen() == gen0 + 1);
  CHECK(f.shell.engine().arranger().current() == before);

  // Keep stepping past the end: '=' CLAMPS at the last section (no wrap).
  for (int i = 0; i < section_count + 2; ++i) {
    CHECK(f.shell.handle_ui_key(kStepSectionNext));
  }
  CHECK(f.shell.engine().arranger().current() == before);  // still unapplied

  // Apply: transport stopped -> immediate; pending clears; lands on the last.
  f.shell.apply_style_step();
  CHECK(!f.shell.style_step_pending());
  CHECK(f.shell.engine().arranger().current() == last);

  // '-' steps back; repeated '-' CLAMPS at the first section.
  for (int i = 0; i < section_count + 5; ++i) {
    CHECK(f.shell.handle_ui_key(kStepSectionPrev));
  }
  f.shell.apply_style_step();
  CHECK(f.shell.engine().arranger().current() == first);

  // apply_style_step with nothing pending is a no-op (idempotent).
  CHECK(!f.shell.style_step_pending());
  f.shell.apply_style_step();
  CHECK(f.shell.engine().arranger().current() == first);
}

void test_style_stepping_and_reclamp() {
  // '_'/'+' step the STYLE (clamped, no wrap); a style step preserves the
  // selected variation by type into the new style (see the StyleChooser unit
  // test for the preserve-by-type guarantee itself).
  ShellFixture f;
  CHECK(f.run("style load basic"));  // builtin index 0
  CHECK(f.run("panel focus styles"));
  CHECK(f.shell.engine().arranger().current_style() == styles::kBuiltins[0]);

  // '_' at the first style CLAMPS (stays basic).
  CHECK(f.shell.handle_ui_key(kStepStylePrev));
  f.shell.apply_style_step();
  CHECK(f.shell.engine().arranger().current_style() == styles::kBuiltins[0]);

  // '+' walks to the last builtin, then clamps; unapplied until apply.
  for (std::uint8_t i = 0; i < styles::kBuiltinCount + 2; ++i) {
    CHECK(f.shell.handle_ui_key(kStepStyleNext));
  }
  CHECK(f.shell.engine().arranger().current_style() == styles::kBuiltins[0]);  // not applied
  f.shell.apply_style_step();
  CHECK(f.shell.engine().arranger().current_style() ==
        styles::kBuiltins[styles::kBuiltinCount - 1]);

  // Step the style back one: whatever section the chooser shows is exactly what
  // gets applied, and the style lands one before the last.
  CHECK(f.shell.handle_ui_key(kStepStylePrev));
  const SectionType shown = f.shell.chooser().selected_section();
  f.shell.apply_style_step();
  CHECK(f.shell.engine().arranger().current() == shown);
  CHECK(f.shell.engine().arranger().current_style() ==
        styles::kBuiltins[styles::kBuiltinCount - 2]);
}

void test_step_mirrors_chooser() {
  // Stepping while the styles panel is focused drives the chooser highlight and
  // feeds the debounced pending selection; applying switches to exactly what is
  // shown.
  ShellFixture f;
  CHECK(f.run("style load basic"));
  CHECK(f.run("panel focus styles"));
  CHECK(f.shell.styles_focused());
  const std::uint32_t gen0 = f.shell.style_step_gen();

  const SectionType before = f.shell.engine().arranger().current();
  CHECK(f.shell.handle_ui_key(kStepSectionNext));
  CHECK(f.shell.styles_focused());
  CHECK(f.shell.style_step_pending());
  CHECK(f.shell.style_step_gen() == gen0 + 1);
  CHECK(f.shell.engine().arranger().current() == before);  // debounced, not yet applied

  const SectionType want = f.shell.chooser().selected_section();
  f.shell.apply_style_step();
  CHECK(!f.shell.style_step_pending());
  CHECK(f.shell.engine().arranger().current() == want);
}

void test_theme_switch_restyles_titles() {
  // With colors on, a coloured theme wraps panel titles in SGR; with colors
  // off the same titles are plain — proving the switch is coherent.
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("colors on"));
  CHECK(f.run("theme set default"));
  CHECK(f.run("panel open piano"));

  bool any_escape = false;
  for (const std::string& line : panel) {
    any_escape = any_escape || line.find('\x1b') != std::string::npos;
  }
  CHECK(any_escape);

  CHECK(f.run("colors off"));
  bool still_escape = false;
  for (const std::string& line : panel) {
    still_escape = still_escape || line.find('\x1b') != std::string::npos;
  }
  CHECK(!still_escape);  // colors off -> no escapes anywhere
}

// A fixture with the default thru wiring, so piano input becomes visible
// output (in0 -> router -> out0), exactly like the live default setup.
struct PianoFixture : ShellFixture {
  PianoFixture() {
    CHECK(run("port open in in0"));
    CHECK(run("port open out out0"));
    CHECK(run("thru in0 out0"));
    CHECK(run("panel focus piano"));
    events.clear();
  }

  int note_ons() const {
    int n = 0;
    for (const OutEvent& e : events) {
      if (e.kind == OutEvent::Kind::kMidi && e.msg.type() == midi::kNoteOn && e.msg.d2 > 0) {
        ++n;
      }
    }
    return n;
  }
  int note_offs() const {
    int n = 0;
    for (const OutEvent& e : events) {
      if (e.kind == OutEvent::Kind::kMidi &&
          (e.msg.type() == midi::kNoteOff || (e.msg.type() == midi::kNoteOn && e.msg.d2 == 0))) {
        ++n;
      }
    }
    return n;
  }
};

void test_piano_key_dispatch() {
  PianoFixture f;

  // 'a' (case-insensitive) = C of octave 4 = MIDI 60, through the normal path.
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.note_ons() == 1);
  CHECK(f.events.back().msg.d1 == 60);
  CHECK(f.events.back().msg.type() == midi::kNoteOn);

  // Toggle policy: same key again = note-off for the same note.
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.note_offs() == 1);
  CHECK(f.events.back().msg.d1 == 60);

  // Black key 'w' = C#4 = 61; velocity/channel follow the piano state.
  CHECK(f.run("piano channel 2"));
  CHECK(f.run("piano velocity 100"));
  CHECK(f.shell.handle_ui_key('w'));
  CHECK(f.events.back().msg.d1 == 61);
  CHECK(f.events.back().msg.channel() == 1);  // wire 0-based for user channel 2
  CHECK(f.events.back().msg.d2 == 100);
  CHECK(f.shell.handle_ui_key('w'));  // release before the next checks

  // Active notes reach the monitor via the wrapped sink.
  CHECK(f.shell.handle_ui_key('h'));  // A4
  CHECK(f.shell.monitor().active_notes().size() == 1);
  CHECK(f.run("piano panic"));
  CHECK(f.shell.monitor().active_notes().size() == 0);

  // 'P' is the D#5 black key = base C4 (60) + 15 = 75, through the normal path.
  CHECK(f.run("piano channel 1"));
  f.events.clear();
  CHECK(f.shell.handle_ui_key('p'));
  CHECK(f.note_ons() == 1);
  CHECK(f.events.back().msg.d1 == 75);
  CHECK(f.shell.handle_ui_key('p'));  // toggle release

  // Out-of-range: octave 9, ' = +17 semitones -> 137 -> rejected, no event.
  CHECK(f.run("piano octave 9"));
  const int before = f.note_ons();
  CHECK(f.shell.handle_ui_key('\''));
  CHECK(f.note_ons() == before);

  // TAB is the way out: focus returns to the REPL, the panel stays open.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kRepl);
  CHECK(f.shell.panels().visible(PanelId::kPiano));

  // With REPL focus musical keys are no longer intercepted.
  CHECK(!f.shell.handle_ui_key('a'));
}

void test_piano_focus_shortcuts() {
  PianoFixture f;

  // N toggles note naming, V cycles views, C clears the monitor.
  CHECK(f.shell.handle_ui_key('n'));
  CHECK(f.shell.piano_state().note_naming == NoteNaming::kDoReMi);
  CHECK(f.shell.handle_ui_key('v'));
  CHECK(f.shell.piano_state().view == PianoView::kActiveNotes);
  CHECK(f.shell.handle_ui_key('v'));
  CHECK(f.shell.piano_state().view == PianoView::kEventLog);
  CHECK(f.shell.handle_ui_key('v'));
  CHECK(f.shell.piano_state().view == PianoView::kKeyboard);

  // . / change octave (moved off [ ] so they never shadow a musical key).
  CHECK(f.shell.handle_ui_key('.'));
  CHECK(f.shell.piano_state().base_octave == 3);
  CHECK(f.shell.handle_ui_key('/'));
  CHECK(f.shell.piano_state().base_octave == 4);

  // C clears monitor buffers.
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.shell.monitor().active_notes().size() == 1);
  CHECK(f.shell.handle_ui_key('c'));
  CHECK(f.shell.monitor().active_notes().size() == 0);

  // TAB cycles focus among visible panels and back to the REPL.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kRepl);

  // Stray keys are swallowed under piano focus, never leak to the editor.
  CHECK(f.run("panel focus piano"));
  CHECK(f.shell.handle_ui_key('q'));
  CHECK(f.shell.handle_ui_key('1'));
}

void test_kitty_key_parser() {
  using Type = KittyKeyEvent::Type;

  // Bare code = press (event type defaults to 1).
  auto a = parse_kitty_key("97");
  CHECK(a.has_value() && a->code == 97 && a->type == Type::kPress);

  // Explicit press / repeat / release event types.
  auto press = parse_kitty_key("97;1u");  // trailing 'u' tolerated
  CHECK(press.has_value() && press->code == 97 && press->type == Type::kPress);
  auto repeat = parse_kitty_key("97;1:2");
  CHECK(repeat.has_value() && repeat->type == Type::kRepeat);
  auto release = parse_kitty_key("97;1:3");
  CHECK(release.has_value() && release->code == 97 && release->type == Type::kRelease);

  // Extra key/text sub-fields are skipped; the base code still parses.
  auto shifted = parse_kitty_key("59:58;2:3");  // ';' key, shift modifier, release
  CHECK(shifted.has_value() && shifted->code == 59 && shifted->type == Type::kRelease);

  // Modifiers are decoded as the wire value minus one: 5 -> ctrl, 2 -> shift,
  // 1 (or absent) -> none. This is what lets Ctrl chords be told apart.
  auto ctrl_p = parse_kitty_key("112;5u");  // CTRL+P
  CHECK(ctrl_p.has_value() && ctrl_p->code == 112 && (ctrl_p->modifiers & kitty::kModCtrl) != 0);
  CHECK((shifted->modifiers & kitty::kModShift) != 0);
  auto plain_a = parse_kitty_key("97;1u");
  CHECK(plain_a.has_value() && plain_a->code == 97 && plain_a->modifiers == 0);
  auto bare = parse_kitty_key("97");  // no section -> no modifiers
  CHECK(bare.has_value() && bare->modifiers == 0);

  // Malformed / non-events -> nullopt (a plain letter is not a kitty event).
  CHECK(!parse_kitty_key("a").has_value());
  CHECK(!parse_kitty_key("").has_value());
  CHECK(!parse_kitty_key(";1").has_value());      // no key code
  CHECK(!parse_kitty_key("97;1:9").has_value());  // unknown event type
}

void test_control_byte_for() {
  const std::uint8_t ctrl = kitty::kModCtrl;

  // The three chords that were being mis-read as musical keys.
  auto p = control_byte_for('p', ctrl);
  CHECK(p.has_value() && *p == 0x10);  // CTRL+P -> play/stop byte
  auto space = control_byte_for(' ', ctrl);
  CHECK(space.has_value() && *space == 0x00);
  auto backslash = control_byte_for('\\', ctrl);
  CHECK(backslash.has_value() && *backslash == 0x1c);

  // Case-insensitive: 'P' and 'p' map to the same control byte.
  auto upper_p = control_byte_for('P', ctrl);
  CHECK(upper_p.has_value() && *upper_p == 0x10);

  // No ctrl bit -> not a control chord; a ctrl-digit has no control byte.
  CHECK(!control_byte_for('a', 0).has_value());
  CHECK(!control_byte_for('a', kitty::kModShift).has_value());
  CHECK(!control_byte_for('1', ctrl).has_value());
}

void test_momentary_lock() {
  PianoFixture f;  // starts in piano focus
  CHECK(f.shell.momentary_available());
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // With momentary available, SPACE cycles momentary <-> toggle freely.
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // Locking momentary out downgrades the live mode to toggle at once...
  f.shell.set_momentary_available(false);
  CHECK(!f.shell.momentary_available());
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);

  // ...and SPACE can no longer switch back to momentary.
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);

  // Re-enabling restores the SPACE switch.
  f.shell.set_momentary_available(true);
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);
}

void test_piano_momentary_mode() {
  PianoFixture f;
  // Default is momentary: true press/release drive note-on/off.
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // 'A' pressed = note-on for MIDI 60 through the normal feed_midi path.
  CHECK(f.shell.piano_key_event('A', true));
  CHECK(f.note_ons() == 1);
  CHECK(f.events.back().msg.d1 == 60);
  CHECK(f.events.back().msg.type() == midi::kNoteOn);
  CHECK(f.shell.monitor().active_notes().size() == 1);

  // Re-press without release (autorepeat) must NOT double note-on.
  CHECK(f.shell.piano_key_event('A', true));
  CHECK(f.note_ons() == 1);

  // Release = matching note-off.
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.note_offs() == 1);
  CHECK(f.events.back().msg.d1 == 60);
  CHECK(f.shell.monitor().active_notes().size() == 0);

  // A release with nothing held is a no-op, not a spurious note-off.
  const int offs = f.note_offs();
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.note_offs() == offs);

  // Polyphony: two distinct keys held together.
  CHECK(f.shell.piano_key_event('A', true));  // 60
  CHECK(f.shell.piano_key_event('S', true));  // 62
  CHECK(f.shell.monitor().active_notes().size() == 2);
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.shell.piano_key_event('S', false));
  CHECK(f.shell.monitor().active_notes().size() == 0);
}

void test_piano_space_toggles_mode() {
  PianoFixture f;
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // SPACE flips the mode and is never a musical note.
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);
  CHECK(f.note_ons() == 0);
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);
  CHECK(f.note_ons() == 0);

  // In kToggle, piano_key_event presses toggle and releases are ignored.
  CHECK(f.shell.handle_ui_key(' '));  // -> kToggle
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);
  CHECK(f.shell.piano_key_event('A', true));  // note-on
  CHECK(f.note_ons() == 1);
  CHECK(f.shell.piano_key_event('A', false));  // release ignored in toggle mode
  CHECK(f.note_offs() == 0);
  CHECK(f.shell.piano_key_event('A', true));  // same key again = note-off
  CHECK(f.note_offs() == 1);
}

void test_piano_plain_bytes_always_toggle() {
  // The plain-byte path is toggle even in momentary mode, so a terminal
  // without the kitty protocol still plays (graceful degradation).
  PianoFixture f;
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);
  CHECK(f.shell.handle_ui_key('a'));  // press = note-on
  CHECK(f.note_ons() == 1);
  CHECK(f.shell.handle_ui_key('a'));  // same key again = note-off
  CHECK(f.note_offs() == 1);
}

void test_tab_enters_piano_from_repl() {
  ShellFixture f;
  CHECK(f.run("panel open piano"));
  // Merely opening a panel leaves focus on the REPL.
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kRepl);

  // TAB from the REPL drops into the only visible panel = piano play mode.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kPanel);
  CHECK(f.shell.panels().focused_panel() == PanelId::kPiano);

  // A musical key now sounds through the normal path.
  CHECK(f.run("port open in in0"));
  CHECK(f.run("port open out out0"));
  CHECK(f.run("thru in0 out0"));
  f.events.clear();
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.midi_count() >= 1);

  // With nothing open, TAB falls through to the line editor.
  ShellFixture g;
  CHECK(!g.shell.handle_ui_key('\t'));
}

void test_panel_manager_state() {
  PanelManager pm;
  CHECK(!pm.any_visible());
  CHECK(pm.focus_kind() == PanelFocus::kRepl);

  // Open + content + grid composition.
  pm.set_content(PanelId::kHelp, {"h1", "h2"});
  pm.open(PanelId::kHelp);
  pm.open(PanelId::kPiano);
  pm.set_content(PanelId::kPiano, {"p1"});
  CHECK(pm.visible(PanelId::kHelp) && pm.visible(PanelId::kPiano));

  // The grid tiles to EXACTLY the requested rows and carries both titles and
  // their content; menu is higher in the order so it paints above the piano.
  constexpr int kWide = 100;
  constexpr int kRows = 20;
  const std::vector<std::string> grid = pm.combined_lines(kWide, kRows);
  CHECK(static_cast<int>(grid.size()) == kRows);
  CHECK(block_contains(grid, "-- menu"));
  CHECK(block_contains(grid, "-- piano"));
  CHECK(block_contains(grid, "h1") && block_contains(grid, "p1"));

  // Panel numbers appear in the titles (1-based grid position, bottom-to-top:
  // piano is [1], menu is [2]).
  CHECK(block_contains(grid, "[1]") && block_contains(grid, "[2]"));
  CHECK(pm.panel_number(PanelId::kPiano) == 1);
  CHECK(pm.panel_number(PanelId::kHelp) == 2);

  // TAB+number: focus_number targets the visible panel at that grid slot.
  CHECK(pm.focus_number(2));
  CHECK(pm.focus_kind() == PanelFocus::kPanel && pm.focused_panel() == PanelId::kHelp);
  CHECK(!pm.focus_number(9));  // out of range: unchanged

  // Closing the focused panel returns focus to the REPL.
  pm.focus(PanelId::kPiano);
  CHECK(pm.focus_kind() == PanelFocus::kPanel && pm.focused_panel() == PanelId::kPiano);
  pm.close(PanelId::kPiano);
  CHECK(pm.focus_kind() == PanelFocus::kRepl);

  // Name round-trip. "menu" is canonical; "help" stays a backward-compat alias.
  PanelId id{};
  CHECK(parse_panel_name("filter", id) && id == PanelId::kFilter);
  CHECK(parse_panel_name("menu", id) && id == PanelId::kHelp);
  CHECK(parse_panel_name("help", id) && id == PanelId::kHelp);
  CHECK(parse_panel_name("styles", id) && id == PanelId::kStyles);
  CHECK(parse_panel_name("events", id) && id == PanelId::kEvents);
  CHECK(!parse_panel_name("bogus", id));
}

void test_warn_names_complete() {
  // Every WarnCode has a wire name (the jsonl table static_asserts the
  // count; this pins the spellings of the once-forgotten half).
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kNotInKey, 0)) ==
        R"({"ev":"warn","code":"not_in_key","@":0})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kTrackTableFull, 0)) ==
        R"({"ev":"warn","code":"track_table_full","@":0})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kSeqTableFull, 0)) ==
        R"({"ev":"warn","code":"seq_table_full","@":0})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kSeqEmpty, 0)) ==
        R"({"ev":"warn","code":"seq_empty","@":0})");
  CHECK(to_jsonl(OutEvent::warn(WarnCode::kUnsupported, 0)) ==
        R"({"ev":"warn","code":"unsupported","@":0})");
}

void test_shell_name_tables_never_diverge() {
  // The 17th track must fail in the SHELL, before a name is registered for
  // an index the core refused.
  ShellFixture f;
  CHECK(f.run("port open out s"));
  for (int i = 0; i < static_cast<int>(kMaxTracks); ++i) {
    CHECK(f.run(("track new t" + std::to_string(i) + " s:1").c_str()));
  }
  CHECK(!f.run("track new overflow s:1"));
  CHECK(!f.run("track step overflow 1 C4"));  // the name was never registered
  for (int i = 0; i < static_cast<int>(kMaxChordSequences); ++i) {
    CHECK(f.run(("seq new q" + std::to_string(i)).c_str()));
  }
  CHECK(!f.run("seq new overflow"));
  CHECK(!f.run("seq use overflow"));
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

void test_shell_style_listing() {
  ShellFixture f;
  std::vector<std::string> out;
  f.shell.set_print_hook([&](const std::string& line) { out.push_back(line); });
  auto listed = [&](const char* needle) {
    for (const std::string& line : out) {
      if (line.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  };

  // `style list` names every builtin.
  CHECK(f.run("style list"));
  CHECK(listed("basic"));

  // `style section list` lists the current style's sections and marks the
  // active one (varA right after load).
  out.clear();
  CHECK(f.run("style load basic"));
  CHECK(f.run("style section list"));
  CHECK(listed("varA"));
  CHECK(listed("varB"));
  CHECK(listed("fillA"));
  CHECK(listed("ending1"));
  CHECK(listed("(current)"));  // varA is active right after load

  // The marker follows the active section (immediate while stopped).
  out.clear();
  CHECK(f.run("style section varB"));
  CHECK(f.shell.engine().arranger().current() == SectionType::kVarB);
  CHECK(f.run("style section list"));
  CHECK(listed("varB"));
  CHECK(listed("(current)"));

  // `style <name> section list` lists a named builtin (case-insensitive).
  out.clear();
  CHECK(f.run("style BASIC section list"));
  CHECK(listed("varA"));
  CHECK(listed("ending1"));

  // Unknown style name errors.
  CHECK(!f.run("style nope section list"));

  // Listing never emits MIDI.
  CHECK(f.midi_count() == 0);
}

void test_shell_view_external_keys() {
  // No view-options accessor is exposed (shell.hpp is owned elsewhere), so the
  // test pins the command grammar: on/off flip the overlay, everything else is
  // a clear error.
  ShellFixture f;
  CHECK(f.run("view external-keys off"));
  CHECK(f.run("view external-keys on"));
  CHECK(f.run("view external-keys off"));  // idempotent re-toggle
  CHECK(!f.run("view external-keys maybe"));
  CHECK(!f.run("view external-keys"));  // missing on|off
  CHECK(f.midi_count() == 0);           // a view toggle never touches the engine
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
  test_shell_style_listing();
  test_shell_view_external_keys();
  test_jsonl_section_rendering();
  test_jsonl_chord_rendering();
  test_console_geometry();
  test_help_command();
  test_help_panel_hook();
  test_panel_commands();
  test_panel_manager_state();
  test_piano_commands();
  test_notes_names_commands();
  test_filter_view_commands();
  test_theme_colors_layout_commands();
  test_contextual_panel();
  test_ctrl_p_play_stop();
  test_ctrl_z_layout();
  test_styles_key_line();
  test_tab_number_focus();
  test_styles_panel_chooser();
  test_style_section_stepping();
  test_style_stepping_and_reclamp();
  test_step_mirrors_chooser();
  test_theme_switch_restyles_titles();
  test_piano_key_dispatch();
  test_piano_focus_shortcuts();
  test_kitty_key_parser();
  test_control_byte_for();
  test_momentary_lock();
  test_piano_momentary_mode();
  test_piano_space_toggles_mode();
  test_piano_plain_bytes_always_toggle();
  test_tab_enters_piano_from_repl();
  test_warn_names_complete();
  test_shell_name_tables_never_diverge();
  test_line_editor();
  test_alsa_null_state_is_safe();
  test_shell_pending_order_same_tick();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_host: all OK\n");
  }
  return arrangrr::test::failures();
}

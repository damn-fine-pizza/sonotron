// Host-layer unit tests: JSONL/human encoders (every branch) and the shell
// (command parsing, @tick queue, error paths). Links hostrt.

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "alsa_midi.hpp"
#include "arrangrr/arranger/arranger.hpp"
#include "gm_program.hpp"
#include "runtime/transport.hpp"
#include "console.hpp"
#include "jsonl.hpp"
#include "kitty_keys.hpp"
#include "shell.hpp"
#include "test.hpp"
#include "uds_server.hpp"

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

  // P0-2: the transport heartbeat, bar/beat/pulse packed purely numerically.
  CHECK(to_jsonl(OutEvent::beat(3, 2, 5, 1200)) ==
        R"({"ev":"beat","bar":3,"beat":2,"pulse":5,"@":1200})");
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
  CHECK(has(to_human(OutEvent::beat(3, 2, 5, 1200)), "beat 3.2.5"));
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

void test_shell_bpm_command() {
  ShellFixture f;
  std::vector<std::string> out;
  f.shell.set_print_hook([&](const std::string& line) { out.push_back(line); });
  auto printed = [&](const char* needle) {
    for (const std::string& line : out) {
      if (line.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  };

  // `bpm <N>` sets the tempo and confirms it.
  CHECK(f.run("bpm 120"));
  CHECK(f.shell.engine().transport().bpm() == 12000);
  CHECK(printed("tempo set to 120.00 bpm"));

  // `bpm` with no argument reports the current tempo and changes nothing.
  out.clear();
  CHECK(f.run("bpm"));
  CHECK(f.shell.engine().transport().bpm() == 12000);
  CHECK(printed("bpm 120.00"));

  // A bad number errors and leaves the tempo untouched.
  out.clear();
  CHECK(!f.run("bpm abc"));
  CHECK(!f.err.empty());
  CHECK(f.shell.engine().transport().bpm() == 12000);
}

// Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
// scope.md): `transpose <-12..12>`, the live global transpose.
void test_shell_transpose_command() {
  ShellFixture f;
  CHECK(f.run("transpose 5"));
  CHECK(f.shell.engine().arranger().master_transpose() == 5);
  CHECK(f.shell.engine().chords().master_transpose() == 5);

  CHECK(f.run("transpose -12"));
  CHECK(f.shell.engine().arranger().master_transpose() == -12);

  // Out of the shell's own [-12, +12] parse bound: rejected, state unchanged.
  CHECK(!f.run("transpose 13"));
  CHECK(!f.err.empty());
  CHECK(f.shell.engine().arranger().master_transpose() == -12);

  // Unparsable token: rejected the same way.
  CHECK(!f.run("transpose banana"));
  CHECK(!f.err.empty());
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
  CHECK(!f.run("chord detect maybe"));  // detect wants on|off
}

// docs/design/orchestrator-pipeline-extraction.md §17.3a: the `note` L1 verb
// is a pure client's wire-safe equivalent of surface_send_note() -- parse ->
// Command{Param::kNoteRaw, ...} -> the SAME 3-byte MIDI message, fed through
// feed_midi(). Verified end to end via the resulting kMidi OutEvent bytes.
void test_note_verb() {
  ShellFixture f;
  CHECK(f.run("port open in in0"));
  CHECK(f.run("port open out out0"));
  CHECK(f.run("thru in0 out0"));
  f.events.clear();

  // Error paths: unknown port, bad on/off, bad note, bad velocity.
  CHECK(!f.run("note bogus on 60 100"));
  CHECK(!f.run("note in0 maybe 60 100"));
  CHECK(!f.run("note in0 on notanote 100"));
  CHECK(!f.run("note in0 on 60 999"));
  CHECK(f.midi_count() == 0);

  // note-on: default velocity 100 when omitted.
  CHECK(f.run("note in0 on 60"));
  CHECK(f.midi_count() == 1);
  {
    const OutEvent& ev = f.events.back();
    CHECK(ev.kind == OutEvent::Kind::kMidi);
    CHECK(ev.port == 0);
    CHECK(ev.msg.status == midi::kNoteOn);  // channel 1 (0-based 0), no ":ch" given
    CHECK(ev.msg.d1 == 60);
    CHECK(ev.msg.d2 == 100);
  }

  // note-off: default (release) velocity when omitted.
  f.events.clear();
  CHECK(f.run("note in0 off 60"));
  CHECK(f.midi_count() == 1);
  {
    const OutEvent& ev = f.events.back();
    CHECK(ev.msg.status == midi::kNoteOff);
    CHECK(ev.msg.d1 == 60);
    CHECK(ev.msg.d2 == 64);
  }

  // Explicit channel suffix (":2" -> 0-based channel 1) and velocity.
  f.events.clear();
  CHECK(f.run("note in0:2 on 61 127"));
  CHECK(f.midi_count() == 1);
  {
    const OutEvent& ev = f.events.back();
    CHECK(ev.msg.status == (midi::kNoteOn | 1));
    CHECK(ev.msg.d1 == 61);
    CHECK(ev.msg.d2 == 127);
  }
}

void test_shell_chord_detect_panel() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open chords"));
  CHECK(block_contains(panel, "detect: off"));
  // The followed-chord NAME moved to the styles panel (current key:); here we
  // assert the DETECTION result via the engine state instead. None latched yet.
  CHECK(!f.shell.engine().chords().state().valid);

  CHECK(f.run("chord detect on"));
  CHECK(block_contains(panel, "detect: on"));

  // Two-zone topology (Dxx): detection observes the HARMONY surface port
  // (kHarmonyInputPort = 1), which `chord detect on` wires as the detect source
  // — NOT the piano/melody port (0). Holding a C major triad on the harmony port
  // (silent, but observed) re-harmonizes the band, and the chords panel names it.
  const std::uint8_t on[9] = {0x90, 60, 100, 0x90, 64, 100, 0x90, 67, 100};
  f.shell.feed_midi(1, Span<const std::uint8_t>(on, sizeof(on)));  // harmony port
  CHECK(f.run("chord detect on"));  // idempotent toggle repaints the panel
  CHECK(f.shell.engine().chords().state().valid);
  CHECK(f.shell.engine().chords().state().root_pc == 0);  // C, detected on the harmony port

  // Chord memory: releasing the keys leaves the last chord named.
  const std::uint8_t off[9] = {0x80, 60, 0, 0x80, 64, 0, 0x80, 67, 0};
  f.shell.feed_midi(1, Span<const std::uint8_t>(off, sizeof(off)));  // harmony port
  CHECK(f.run("chord detect on"));
  CHECK(f.shell.engine().chords().state().root_pc == 0);  // chord memory holds C after release

  // The PIANO/melody port (0) must NOT steer: a full D minor triad played there
  // sounds but leaves the followed chord untouched — the panel still names C,
  // never D. This pins the two-zone split: only the harmony surface steers.
  // (Steering is by PORT/zone here: feed_midi bypasses UI focus, so this is the
  // detect-source split, distinct from the global focus-steer of note-letters.)
  const std::uint8_t dmin[9] = {0x90, 62, 100, 0x90, 65, 100, 0x90, 69, 100};
  f.shell.feed_midi(0, Span<const std::uint8_t>(dmin, sizeof(dmin)));  // piano port
  CHECK(f.run("chord detect on"));
  CHECK(f.shell.engine().chords().state().root_pc == 0);  // still C
  CHECK(f.shell.engine().chords().state().root_pc != 2);  // never D: the piano port does not steer

  CHECK(f.run("chord detect off"));
  CHECK(block_contains(panel, "detect: off"));
}

// The GLOBAL chord-steer contract (the user's actual bug, inverted by the
// harmony-global-steer refactor): with ANY panel focused a musical LETTER key
// steers the band SILENTLY through the harmony surface — the piano panel now
// steers too, uniformly with chords/parts/etc — and the REPL never steers (a
// note-letter typed at the command line moves nothing). This drives the REAL
// live path (handle_ui_key), the exact chain a user's keystroke follows.
void test_note_letters_steer_from_every_panel_but_repl() {
  ShellFixture f;
  f.shell.configure_default_surfaces();  // detect on, single-finger, kHarmony steer port
  CHECK(f.run("chord mode single"));     // one letter key = the scale-aware triad

  // THE inverted truth: the PIANO panel now STEERS. Focus it, press 'f' (=F4):
  // in C major single-finger that is the IV triad, so the band follows F. Nothing
  // sounds (the harmony surface is output-suppressed) — steering is the effect.
  // Lowercase = IMMEDIATE steer; an UPPERCASE letter SHIFT-stages to the next bar
  // (D53), so the immediate-commit assertions here deliberately use lowercase.
  CHECK(f.run("panel focus piano"));
  CHECK(f.shell.handle_ui_key('f'));
  CHECK(f.shell.engine().chords().state().valid);
  CHECK(f.shell.engine().chords().state().root_pc == 5);  // F
  CHECK(f.shell.harmony_held_count() == 1);               // held on the harmony surface

  // Re-steer from the SAME piano panel: release F (plain-byte toggle), press 'g'
  // — the band MOVES to G (proves a live re-steer, not a one-shot latch).
  CHECK(f.shell.handle_ui_key('f'));  // toggle F off
  CHECK(f.shell.harmony_held_count() == 0);
  CHECK(f.shell.handle_ui_key('g'));
  CHECK(f.shell.engine().chords().state().root_pc == 7);  // G
  CHECK(f.shell.handle_ui_key('g'));                      // release before switching

  // The CHORDS panel steers too (its historical role). 'a' letter = C4 = the I
  // triad: the band follows C.
  CHECK(f.run("panel focus chords"));
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.shell.engine().chords().state().root_pc == 0);  // C
  CHECK(f.shell.handle_ui_key('a'));                      // release

  // EVERY focused panel steers — even a non-harmony one like the parts mixer:
  // the note-letter is consumed by the global choke BEFORE the parts key handler.
  // 'h' letter = A4 = the vi triad, so the band follows A.
  CHECK(f.run("panel focus parts"));
  CHECK(f.shell.handle_ui_key('h'));
  CHECK(f.shell.engine().chords().state().root_pc == 9);  // A
  CHECK(f.shell.handle_ui_key('h'));                      // release

  // The REPL is the ONE surface that must NOT steer: a note-letter falls through
  // to the line editor (handle_ui_key returns false) and the followed chord is
  // left exactly where it was — typing 's' does not move the band to D.
  CHECK(f.run("panel focus repl"));
  CHECK(!f.shell.handle_ui_key('s'));
  CHECK(f.shell.engine().chords().state().root_pc == 9);  // still A, never moved
  CHECK(f.shell.harmony_held_count() == 0);               // nothing captured at the REPL
}

// Permanent-transpose contract (user-required, root-cause behavior): a pressed
// chord STAYS the followed chord across bars — there is NO auto-revert after the
// key is released. Chord memory holds the band on it until the NEXT chord is
// pressed. This is the exact guarantee the user asked to be pinned.
void test_permanent_transpose_persists_across_bars() {
  ShellFixture f;
  f.shell.configure_default_surfaces();
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel focus chords"));

  // Steer to F (IV) with a lowercase = IMMEDIATE press, then RELEASE the key —
  // memory must keep the band on F.
  CHECK(f.shell.handle_ui_key('f'));
  CHECK(f.shell.engine().chords().state().root_pc == 5);
  CHECK(f.shell.handle_ui_key('f'));  // release
  CHECK(f.shell.harmony_held_count() == 0);
  CHECK(f.shell.engine().chords().state().root_pc == 5);  // still F after release

  // Run the transport across several bars: the followed chord must NOT drift or
  // auto-revert to the home key — it stays exactly on F, bars later.
  CHECK(f.run("transport start"));
  std::string err;
  CHECK(f.shell.advance_by(4 * kTicksPerBar, err));
  CHECK(f.shell.engine().chords().state().valid);
  CHECK(f.shell.engine().chords().state().root_pc == 5);  // still F

  // A NEW press moves it (proves it is memory, not a stuck latch) and the new
  // chord likewise persists across bars with no revert.
  CHECK(f.shell.handle_ui_key('g'));
  CHECK(f.shell.engine().chords().state().root_pc == 7);  // G
  CHECK(f.shell.handle_ui_key('g'));  // release
  CHECK(f.shell.advance_by(2 * kTicksPerBar, err));
  CHECK(f.shell.engine().chords().state().root_pc == 7);  // stays on G
}

// Owner's live finding, folded into this investigation: "sometimes pressing A
// then S produces the SAME chord." A and S are DIFFERENT white keys
// (piano_binding_for: A=C4 semitone 0, S=D4 semitone 2), so each single-finger
// press must yield a DIFFERENT root. Lowercase bytes are used throughout
// (deliberately, NOT 'A'/'S') to keep this probe isolated from the SEPARATE
// uppercase/shift-quantize ambiguity already pinned red elsewhere in this file
// (test_note_letters_steer_from_every_panel_but_repl,
// test_permanent_transpose_persists_across_bars): byte case doubles as the
// shift signal in surface_musical_key, so an uppercase letter here would
// confound root-collision with stage-vs-commit.
void test_pressing_a_then_s_yields_different_roots_when_properly_released() {
  std::printf("\n==== A/S control: 'a' alone, then 'a' released, then 's' alone ====\n");
  ShellFixture f;
  f.shell.configure_default_surfaces();
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel focus chords"));

  CHECK(f.shell.handle_ui_key('a'));  // A alone: single-finger root C (pc 0)
  std::printf("held=%zu root_pc=%u quality=%d\n", f.shell.harmony_held_count(),
              f.shell.engine().chords().state().root_pc,
              static_cast<int>(f.shell.engine().chords().state().quality));
  CHECK(f.shell.engine().chords().state().valid);
  const std::uint8_t r_a = f.shell.engine().chords().state().root_pc;
  CHECK(r_a == 0);  // C

  CHECK(f.shell.handle_ui_key('a'));  // release A (plain-byte toggle off)
  CHECK(f.shell.harmony_held_count() == 0);

  CHECK(f.shell.handle_ui_key('s'));  // S alone, A fully released first
  std::printf("held=%zu root_pc=%u quality=%d\n", f.shell.harmony_held_count(),
              f.shell.engine().chords().state().root_pc,
              static_cast<int>(f.shell.engine().chords().state().quality));
  const std::uint8_t r_s = f.shell.engine().chords().state().root_pc;
  CHECK(r_s == 2);      // D
  CHECK(r_s != r_a);    // a genuinely different chord from a genuinely different key
}

// The REAL-WORLD gesture: a player moving a finger from one key straight to
// the next WITHOUT an explicit double-tap to release the first (a plain TTY
// has no true key-up, so this is what "press A then S" means to a human). In
// single-finger, one key == one chord, so the new key must REPLACE the previous
// single-finger note instead of accumulating {A, S} -- otherwise the detector
// roots on the lowest held note (A) and the root never moves to the key just
// pressed. Regression guard for the owner's "A then S give the same chord"
// report; the fix lives in Shell::toggle_surface_key.
void test_single_finger_new_key_replaces_previous_root() {
  std::printf("\n==== single-finger A->S replaces (no release between) ====\n");
  ShellFixture f;
  f.shell.configure_default_surfaces();
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel focus chords"));

  CHECK(f.shell.handle_ui_key('a'));  // A alone: root C (pc 0)
  const std::uint8_t r_a = f.shell.engine().chords().state().root_pc;
  CHECK(r_a == 0);
  CHECK(f.shell.harmony_held_count() == 1);

  CHECK(f.shell.handle_ui_key('s'));  // S pressed with A still held (no release between)
  std::printf("held=%zu root_pc=%u quality=%d\n", f.shell.harmony_held_count(),
              f.shell.engine().chords().state().root_pc,
              static_cast<int>(f.shell.engine().chords().state().quality));
  // S REPLACES A: exactly one held note, and the root moves to S (D, pc 2) --
  // no {A, S} accumulation, no lowest-note-wins collapse onto A.
  CHECK(f.shell.harmony_held_count() == 1);
  const std::uint8_t root_after_s = f.shell.engine().chords().state().root_pc;
  CHECK(root_after_s == 2);
  CHECK(root_after_s != r_a);
}

// Same defect, the owner's second report: pressing G then H (adjacent white keys,
// no release) always resolved to Em because {G, A} accumulated and the detector
// rooted on the lowest note. With single-finger replace, H replaces G and the
// root moves to A (pc 9) -- never Em (pc 4).
void test_single_finger_g_then_h_does_not_collapse_to_em() {
  std::printf("\n==== single-finger G->H replaces (no Em collapse) ====\n");
  ShellFixture f;
  f.shell.configure_default_surfaces();
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel focus chords"));

  CHECK(f.shell.handle_ui_key('g'));  // G: single-finger root G (pc 7)
  CHECK(f.shell.engine().chords().state().root_pc == 7);

  CHECK(f.shell.handle_ui_key('h'));  // H (=A) replaces G
  CHECK(f.shell.harmony_held_count() == 1);
  const std::uint8_t root_after_h = f.shell.engine().chords().state().root_pc;
  CHECK(root_after_h == 9);  // A, not E (Em would be pc 4)
  CHECK(root_after_h != 4);
}

void test_shell_parts_command_and_panel() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open parts"));
  CHECK(block_contains(panel, "Drums"));
  CHECK(block_contains(panel, "Bass"));
  CHECK(block_contains(panel, "Pad"));
  CHECK(block_contains(panel, "Arp"));
  // Mute/solo via the CLI (the panel keys share this path).
  CHECK(f.run("part bass mute on"));
  CHECK(f.run("part drums solo on"));
  CHECK(f.run("part pad mute off"));
  // Errors.
  CHECK(!f.run("part nope mute on"));  // unknown role
  CHECK(!f.run("part bass flip on"));  // bad subcommand
  CHECK(!f.run("part bass"));          // too few args
}

// Parts-panel SOLO migrated off 's' (now a band-steering note-letter) onto 'i'
// ("isolate"). This pins the migration: 'i' toggles solo on the selected part,
// 's' does NOT touch solo — it steers the band instead — and 'm' still mutes.
void test_parts_solo_migrated_to_i_key() {
  ShellFixture f;
  f.shell.configure_default_surfaces();  // detect on, single-finger: 's' will steer
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel focus parts"));
  CHECK(f.shell.parts_focused());
  const TrackRole role = TrackRole::kDrums;  // row 0 (Drums) is selected by default
  CHECK(!f.shell.engine().arranger().soloed(role));

  // 'i' toggles solo on the selected part, and again toggles it back off.
  CHECK(f.shell.handle_ui_key('i'));
  CHECK(f.shell.engine().arranger().soloed(role));
  CHECK(f.shell.handle_ui_key('i'));
  CHECK(!f.shell.engine().arranger().soloed(role));

  // 's' is a NOTE-letter (D4) now: the global steer choke consumes it BEFORE the
  // parts key handler, so it never toggles solo — it steers the band to D (ii).
  CHECK(f.shell.handle_ui_key('s'));
  CHECK(!f.shell.engine().arranger().soloed(role));       // solo untouched
  CHECK(f.shell.engine().chords().state().valid);
  CHECK(f.shell.engine().chords().state().root_pc == 2);  // D
  CHECK(f.shell.handle_ui_key('s'));                       // release the steered note

  // 'm' is not a note-letter, so it survives the choke and still mutes the part.
  CHECK(!f.shell.engine().arranger().muted(role));
  CHECK(f.shell.handle_ui_key('m'));
  CHECK(f.shell.engine().arranger().muted(role));
}

void test_shell_arp_command_and_panel() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open arp"));
  CHECK(block_contains(panel, "rate"));
  CHECK(block_contains(panel, "direction"));
  CHECK(f.run("arp on"));
  CHECK(f.run("arp rate 1/8"));
  CHECK(block_contains(panel, "1/8"));
  CHECK(f.run("arp dir updown"));
  CHECK(block_contains(panel, "up-down"));
  CHECK(f.run("arp octaves 3"));
  CHECK(f.run("arp gate 50"));
  CHECK(f.run("arp latch on"));
  CHECK(f.run("port open out synth"));
  CHECK(f.run("arp out synth:2"));
  CHECK(f.run("arp off"));
  // Errors.
  CHECK(!f.run("arp rate 1/3"));      // bad rate
  CHECK(!f.run("arp dir sideways"));  // bad direction
  CHECK(!f.run("arp nope 1"));        // unknown field
}

void test_shell_groove_command_and_panel() {
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open groove"));
  CHECK(block_contains(panel, "swing"));
  CHECK(block_contains(panel, "accent"));
  CHECK(f.run("groove swing 60"));
  CHECK(block_contains(panel, "60%"));
  CHECK(f.run("groove humanize-t 30"));
  CHECK(f.run("groove accent 100"));
  CHECK(f.run("groove grid 16"));
  CHECK(f.run("groove quantize 75"));  // new field reachable end-to-end
  CHECK(block_contains(panel, "quantize"));
  CHECK(block_contains(panel, "75%"));
  CHECK(!f.run("groove nope 10"));  // unknown field
  CHECK(!f.run("groove swing"));    // missing value
}

void test_gm_program_parsing() {
  CHECK(parse_gm_program("0") == 0);
  CHECK(parse_gm_program("127") == 127);
  CHECK(parse_gm_program("128") == -1);        // out of range
  CHECK(parse_gm_program("-1") == -1);
  CHECK(parse_gm_program("trumpet") == 56);    // exact name
  CHECK(parse_gm_program("Trumpet") == 56);    // case-insensitive
  CHECK(parse_gm_program("acoustic-grand-piano") == 0);  // hyphen-normalized
  CHECK(parse_gm_program("Electric Piano 1") == 4);      // spaces normalized
  CHECK(parse_gm_program("finger") == 33);     // unique substring: Electric Bass (finger)
  CHECK(parse_gm_program("violin") == 40);     // exact single name
  CHECK(parse_gm_program("pad") == -1);        // ambiguous (Pad 1..8) -> reject
  CHECK(parse_gm_program("piano") == -1);      // ambiguous family -> reject, use a number
  CHECK(parse_gm_program("zzznope") == -1);    // unknown
  CHECK(std::string(gm_program_name(56)) == "Trumpet");
}

void test_shell_program_command() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("program synth 0"));                 // by number, default channel 1
  CHECK(f.run("program synth:2 trumpet"));         // name + explicit channel
  CHECK(f.run("program synth electric piano 1"));  // multi-word GM name
  CHECK(!f.run("program synth 200"));              // out of range
  CHECK(!f.run("program synth zzznope"));          // unknown voice
  CHECK(!f.run("program nowhere 0"));              // unknown port
  int programs = 0;
  int trumpet_ch2 = 0;
  for (const OutEvent& o : f.events) {
    if (o.kind == OutEvent::Kind::kMidi && (o.msg.status & 0xF0) == 0xC0) {
      ++programs;
      if (o.msg.d1 == 56 && (o.msg.status & 0x0F) == 1) {  // trumpet on channel 2 (0-based 1)
        ++trumpet_ch2;
      }
    }
  }
  CHECK(programs == 3);
  CHECK(trumpet_ch2 == 1);
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

// Phase 2b integrated-mode gap (docs report): Shell::resolve_style_index()/
// resolve_track_role() are the two public pure name-resolution seams exposed
// so a caller building its own Command POD (in_process_brain_session's L1-
// text translator) resolves names EXACTLY as cmd_style()'s "load" verb and
// cmd_part() do, without reaching into hostrt's private shell_internal.hpp.
// No Shell instance is needed -- both are static, no-state lookups.
void test_shell_style_and_role_name_resolution() {
  CHECK(Shell::resolve_style_index("basic") == 0);
  CHECK(Shell::resolve_style_index("BASIC") == 0);  // case-insensitive, matches cmd_style (D26)
  CHECK(Shell::resolve_style_index("pop") == 1);
  CHECK(Shell::resolve_style_index("funkytown") < 0);  // unknown name -> -1

  TrackRole role = TrackRole::kDrums;
  CHECK(Shell::resolve_track_role("lead", role));
  CHECK(role == TrackRole::kLead);
  CHECK(Shell::resolve_track_role("bass", role));
  CHECK(role == TrackRole::kBass);
  CHECK(!Shell::resolve_track_role("wizard", role));  // unknown role -> false
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
  // region) — including when nothing is open: a blank pane is still `rows` lines,
  // so the contract holds without leaning on the caller to clear.
  PanelManager pm;
  const std::vector<std::string> hidden = pm.combined_lines(80, 12);
  CHECK(static_cast<int>(hidden.size()) == 12);
  for (const std::string& line : hidden) {
    CHECK(line.empty());
  }
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

  // Narrow 2-per-row falls back to one-per-row: a wide terminal pairs the two
  // panels (a " | " gutter) but one too narrow for two min-width cells stacks
  // them, and no composed line overruns the width (colours off -> size == cols).
  pm.open(PanelId::kConsole);
  pm.set_per_row(2);
  CHECK(block_contains(pm.combined_lines(80, 8), " | "));
  const std::vector<std::string> narrow = pm.combined_lines(18, 8);
  CHECK(!block_contains(narrow, " | "));
  for (const std::string& line : narrow) {
    CHECK(line.size() <= 18);
  }

  // Short terminal (more panels than rows): keep the bottom-priority rows, each
  // still showing at least its title, tiling to EXACTLY rows — never the blind
  // tail-truncation that used to eat the bottom (most important) panels.
  pm.set_per_row(1);
  pm.open(PanelId::kStyles);
  const std::vector<std::string> tight = pm.combined_lines(80, 2);
  CHECK(static_cast<int>(tight.size()) == 2);
  for (const std::string& line : tight) {
    CHECK(line.find("-- ") != std::string::npos);
  }
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
  CHECK(calls == 2 && !block_contains(panel, "-- "));  // pane cleared (no titles)
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
  CHECK(!block_contains(panel, "-- "));  // nothing visible -> a blank pane

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

  // The styles panel renders the chooser: style:/section:/hint + the fixed
  // `home key:` reference line (the song tonic chord).
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "-- styles"));
  CHECK(block_contains(panel, "ENTER next-bar"));
  CHECK(block_contains(panel, "original key:"));

  // A digit feeds the filter only while the styles panel is focused.
  CHECK(f.shell.handle_ui_key('0'));
  CHECK(f.shell.chooser().filter() == "0");
  CHECK(f.shell.handle_ui_key(0x7F));  // backspace clears the filter
  CHECK(f.shell.chooser().filter().empty());

  // Move the section highlight, then CTRL+\ applies immediately (transport
  // stopped) and KEEPS focus on the styles panel.
  f.shell.chooser_nav_section(+1);
  const SectionType want = static_cast<SectionType>(f.shell.chooser().selected_section());
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
  const SectionType want2 = static_cast<SectionType>(f.shell.chooser().selected_section());
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
  // The styles panel carries the fixed reference line: `original key:` — the
  // song's tonic CHORD (single_finger_quality of the key root), a fixed reference
  // that does NOT move when the band is steered. `scale`/`key` set the tonic; F
  // major -> "original key: F", A minor -> "original key: Am". The `scale` command
  // is a `key` alias, so both spellings still drive the tonic.
  ShellFixture f;
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("panel open styles"));
  CHECK(f.run("key F major"));
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "original key: F"));
  CHECK(f.run("scale A minor"));  // `scale` alias of `key`: minor tonic -> "Am"
  f.shell.refresh_panels();
  CHECK(block_contains(panel, "original key: Am"));
}

void test_tab_number_focus() {
  // TAB followed by a digit focuses the panel at that 1-based grid position.
  ShellFixture f;
  CHECK(f.run("panel open piano"));   // paints at the bottom -> grid number 2
  CHECK(f.run("panel open styles"));  // paints above the piano -> grid number 1
  // TAB arms the jump; the next digit selects the panel (numbered top-to-bottom).
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.handle_ui_key('1'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kPanel);
  CHECK(f.shell.panels().focused_panel() == PanelId::kStyles);
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.handle_ui_key('2'));
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
  const SectionType shown = static_cast<SectionType>(f.shell.chooser().selected_section());
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

  const SectionType want = static_cast<SectionType>(f.shell.chooser().selected_section());
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

// Owner follow-up to roadmap 11410: the keyboard's committed (green) harmony
// overlay must light ONLY when the followed chord is genuinely ACTIVE —
// transport playing or a producer explicitly steered it — never for the
// passive home-tonic default `establish_default()` seeds on style load. This
// is the real production gate in Shell::refresh_piano_content, exercised end
// to end (not the pure render seam, which cannot see the gate).
void test_piano_harmony_gate_at_rest() {
  ShellFixture f;
  f.shell.set_width_provider([] { return 200; });  // wide: the grid tier, single-glyph keys
  std::vector<std::string> panel;
  f.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel = lines;
    return true;
  });
  CHECK(f.run("colors on"));
  CHECK(f.run("panel open piano"));

  const UiStyle& style = f.shell.ui_style();
  const std::string green_a = style.apply(UiRole::kMidiNoteOn, "A");  // C4, the tonic root key
  auto has_green = [&] {
    for (const std::string& line : panel) {
      if (line.find(green_a) != std::string::npos) {
        return true;
      }
    }
    return false;
  };

  // Loading a style seeds the home tonic via establish_default: the followed
  // chord becomes valid (C major) but NOT explicit, and transport is stopped —
  // this is the "at rest" state the owner called out. No green. (The piano
  // panel is only repainted on demand — main.cpp's redraw timer does this in
  // production — so the test drives it explicitly via refresh_panels(),
  // exactly like `chord play`/`transport start` below.)
  CHECK(f.run("style load basic"));
  f.shell.refresh_panels();
  CHECK(f.shell.engine().chords().state().valid);        // home tonic seeded
  CHECK(!f.shell.engine().chords().explicit_set());      // but never steered
  CHECK(!f.shell.engine().transport().playing());        // and not playing
  CHECK(!has_green());                                   // -> overlay stays off

  // An explicit steer while STILL stopped activates the overlay (part a).
  CHECK(f.run("chord play C"));
  f.shell.refresh_panels();
  CHECK(f.shell.engine().chords().explicit_set());
  CHECK(has_green());

  // Transport playing also activates the overlay on its own — a second, fresh
  // fixture that is never explicitly steered, just running.
  ShellFixture g;
  g.shell.set_width_provider([] { return 200; });
  std::vector<std::string> panel2;
  g.shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    panel2 = lines;
    return true;
  });
  CHECK(g.run("colors on"));
  CHECK(g.run("panel open piano"));
  CHECK(g.run("style load basic"));
  CHECK(g.run("transport start"));
  g.shell.refresh_panels();
  CHECK(g.shell.engine().transport().playing());
  CHECK(!g.shell.engine().chords().explicit_set());
  bool has_green2 = false;
  for (const std::string& line : panel2) {
    if (line.find(green_a) != std::string::npos) {
      has_green2 = true;
    }
  }
  CHECK(has_green2);
}

// Harmony-global-steer fixture: the piano panel is now a STEERING surface — its
// note keys drive the SILENT harmony port (kHarmony, output-suppressed), so a
// press re-harmonizes the band with NO audible note. There is therefore nothing
// on an out-port to count; configure_default_surfaces() arms detection +
// single-finger so a pressed key shows up as a followed chord and a held note on
// the harmony surface, which is what these tests observe (held-set + chord
// engine) in place of the old sounding-note counts.
struct PianoFixture : ShellFixture {
  PianoFixture() {
    shell.configure_default_surfaces();  // detect on, single-finger, kHarmony steer port
    CHECK(run("panel focus piano"));      // the piano panel steers the band
    events.clear();
  }

  // Notes the harmony surface currently holds (the silent equivalent of the old
  // "how many notes are sounding" — nothing sounds now, so we watch the held-set).
  std::size_t held() const { return shell.harmony_held_count(); }
  bool steering() const { return shell.engine().chords().state().valid; }
  int steered_root() const { return static_cast<int>(shell.engine().chords().state().root_pc); }
};

void test_piano_key_dispatch() {
  PianoFixture f;

  // 'a' (case-insensitive) = C of octave 4 (pc 0). In C-major single-finger that
  // is the I triad, so the piano panel STEERS the band to C and holds one note
  // on the (silent) harmony surface. No audible note is emitted anymore.
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.held() == 1);
  CHECK(f.steering());
  CHECK(f.steered_root() == 0);  // C

  // Toggle policy: same key again releases the held note. Chord MEMORY keeps the
  // band on C after release (it persists until the next chord is pressed).
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.held() == 0);
  CHECK(f.steered_root() == 0);  // still following C from memory

  // Black key 'w' = C#4 (pc 1): a chromatic root in C major snaps to major, so
  // the band follows C#. The key->note binding is proven through the steered
  // root now that no wire note-on carries it. (piano channel/velocity still parse
  // but no longer colour an audible note — the harmony surface is silent.)
  CHECK(f.run("piano channel 2"));
  CHECK(f.run("piano velocity 100"));
  CHECK(f.shell.handle_ui_key('w'));
  CHECK(f.held() == 1);
  CHECK(f.steered_root() == 1);       // C#
  CHECK(f.shell.handle_ui_key('w'));  // release before the next checks
  CHECK(f.held() == 0);

  // 'h' = A4 (pc 9) = the vi triad -> the band follows A, one note held.
  CHECK(f.shell.handle_ui_key('h'));
  CHECK(f.held() == 1);
  CHECK(f.steered_root() == 9);  // A
  // `piano panic` flushes BOTH surfaces' held sets (melody + harmony).
  CHECK(f.run("piano panic"));
  CHECK(f.held() == 0);

  // 'P' is the D#5 black key = base C4 (60) + 15 = 75 (pc 3) -> chromatic -> D#.
  CHECK(f.run("piano channel 1"));
  CHECK(f.shell.handle_ui_key('p'));
  CHECK(f.held() == 1);
  CHECK(f.steered_root() == 3);       // D#
  CHECK(f.shell.handle_ui_key('p'));  // toggle release
  CHECK(f.held() == 0);

  // Out-of-range: octave 9, ' = +17 semitones -> 137 -> rejected: no note is
  // captured on the harmony surface, so the held-set does not grow.
  CHECK(f.run("piano octave 9"));
  CHECK(f.shell.handle_ui_key('\''));
  CHECK(f.held() == 0);

  // TAB is the way out: focus returns to the REPL, the panel stays open.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kRepl);
  CHECK(f.shell.panels().visible(PanelId::kPiano));

  // With REPL focus musical keys are no longer intercepted (they fall through to
  // the line editor) and nothing is captured on the harmony surface.
  CHECK(!f.shell.handle_ui_key('a'));
  CHECK(f.held() == 0);
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

  // C clears the monitor's active notes. Piano keys are silent now, so seed the
  // monitor through a real SOUNDING path (an external note on a routed port) and
  // prove the 'C' shortcut empties it.
  CHECK(f.run("port open in mon_in"));
  CHECK(f.run("port open out mon_out"));
  CHECK(f.run("thru mon_in mon_out"));
  const std::uint8_t note_on[3] = {0x90, 60, 100};
  f.shell.feed_midi(0, Span<const std::uint8_t>(note_on, sizeof(note_on)));
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
  // Default is momentary: true press/release drive the harmony surface's held-set.
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // 'A' pressed = C4 captured on the harmony surface; the band steers to C.
  CHECK(f.shell.piano_key_event('A', true));
  CHECK(f.held() == 1);
  CHECK(f.steering());
  CHECK(f.steered_root() == 0);  // C

  // Re-press without release (autorepeat) must NOT double the held note.
  CHECK(f.shell.piano_key_event('A', true));
  CHECK(f.held() == 1);

  // Release = the held note leaves the surface.
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.held() == 0);

  // A release with nothing held is a no-op, not a spurious extra release.
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.held() == 0);

  // Polyphony: two distinct keys held together on the harmony surface.
  CHECK(f.shell.piano_key_event('A', true));  // C4
  CHECK(f.shell.piano_key_event('S', true));  // D4
  CHECK(f.held() == 2);
  CHECK(f.shell.piano_key_event('A', false));
  CHECK(f.shell.piano_key_event('S', false));
  CHECK(f.held() == 0);
}

void test_piano_space_toggles_mode() {
  PianoFixture f;
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // SPACE flips the harmony key mode and is never a musical note (nothing held).
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);
  CHECK(f.held() == 0);
  CHECK(f.shell.handle_ui_key(' '));
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);
  CHECK(f.held() == 0);

  // In kToggle, piano_key_event presses toggle the held note and releases are
  // ignored, so a press-release-press cycle ends with the note released.
  CHECK(f.shell.handle_ui_key(' '));  // -> kToggle
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);
  CHECK(f.shell.piano_key_event('A', true));  // capture
  CHECK(f.held() == 1);
  CHECK(f.shell.piano_key_event('A', false));  // release ignored in toggle mode
  CHECK(f.held() == 1);
  CHECK(f.shell.piano_key_event('A', true));  // same key again = release
  CHECK(f.held() == 0);
}

void test_piano_plain_bytes_always_toggle() {
  // The plain-byte path is toggle even in momentary mode, so a terminal without
  // the kitty protocol still steers (graceful degradation): press captures the
  // note on the harmony surface, the same key again releases it.
  PianoFixture f;
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);
  CHECK(f.shell.handle_ui_key('a'));  // press = capture
  CHECK(f.held() == 1);
  CHECK(f.shell.handle_ui_key('a'));  // same key again = release
  CHECK(f.held() == 0);
}

void test_piano_toggle_autorepeat_debounce() {
  // The bug: in toggle mode (the plain-TTY fallback, no key-release), holding a
  // piano key makes the OS auto-repeat the keystroke — a stream of identical
  // bytes. Without a debounce each byte flips the note on/off/on/off, a flood
  // "a manetta" with no rhythm. The fix ignores toggles that land within the
  // debounce window of the key's previous toggle, driven by an injected clock.
  constexpr std::uint64_t kDebounceUs = 300'000;  // mirrors kToggleAutoRepeatDebounceUs

  PianoFixture f;
  CHECK(f.shell.handle_ui_key(' '));  // momentary -> toggle
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kToggle);

  // First press captures MIDI 60 ('a' at octave 4) on the harmony surface.
  std::uint64_t t = 1'000'000;  // any non-zero base
  f.shell.set_input_time_us(t);
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.held() == 1);

  // A sustained auto-repeat burst (~30 ms cadence) inside the window is fully
  // swallowed: no machine-gun. The note simply stays held for as long as the
  // key is down, because every repeat slides the window forward.
  for (int i = 0; i < 30; ++i) {
    t += 30'000;
    f.shell.set_input_time_us(t);
    CHECK(f.shell.handle_ui_key('a'));
  }
  CHECK(f.held() == 1);  // still exactly one held note, never toggled off

  // A genuine re-tap AFTER the window is quiet long enough to be a fresh
  // keystroke, so it toggles the note off.
  t += kDebounceUs + 1;
  f.shell.set_input_time_us(t);
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.held() == 0);

  // ...and once more captures it again: intentional re-tapping still works.
  t += kDebounceUs + 1;
  f.shell.set_input_time_us(t);
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.held() == 1);

  // The debounce is per-key: a DIFFERENT key held at the same instant captures
  // its own note, not suppressed by the first key's window. In single-finger the
  // new key REPLACES the previous one, so the held count stays 1 but the steered
  // root moves to S (D, pc 2) -- proof that S registered rather than being eaten
  // by A's debounce window.
  CHECK(f.shell.handle_ui_key('s'));  // MIDI 62, same timestamp t
  CHECK(f.held() == 1);
  CHECK(f.shell.engine().chords().state().root_pc == 2);
}

void test_piano_kitty_repeat_no_double_fire() {
  // Momentary mode (kitty key protocol): a held key emits kPress then a stream
  // of kRepeat events. Neither the shell's held-check nor the live loop's
  // kRepeat drop may let a held key fire a second note-on.
  PianoFixture f;
  CHECK(f.shell.piano_key_mode() == PianoKeyMode::kMomentary);

  // The live loop drops kRepeat for MUSICAL keys; the predicate it relies on.
  CHECK(f.shell.piano_is_musical_key(static_cast<std::uint8_t>('a')));
  CHECK(f.shell.piano_is_musical_key(static_cast<std::uint8_t>('A')));
  CHECK(!f.shell.piano_is_musical_key(static_cast<std::uint8_t>('\t')));
  CHECK(!f.shell.piano_is_musical_key(static_cast<std::uint8_t>(' ')));

  // Press then autorepeat (both arrive as piano_key_event(pressed=true) when a
  // repeat is not dropped): exactly one held note, released on key-up.
  CHECK(f.shell.piano_key_event('A', true));  // press -> capture
  CHECK(f.held() == 1);
  for (int i = 0; i < 10; ++i) {
    CHECK(f.shell.piano_key_event('A', true));  // autorepeat re-press: idempotent
  }
  CHECK(f.held() == 1);
  CHECK(f.shell.piano_key_event('A', false));  // release -> note leaves the surface
  CHECK(f.held() == 0);
}

void test_arp_live_stays_on_grid() {
  // Sanity: with the transport PLAYING and the arp on over a held chord, the
  // output lands on the rate grid (one step per 1/8), NOT a flood — proving the
  // "a manetta" bug was the input path, never the rhythmic arp engine.
  ShellFixture f;
  CHECK(f.run("port open in in0"));
  CHECK(f.run("port open out out0"));
  CHECK(f.run("arp out out0:1"));
  CHECK(f.run("arp rate 1/8"));
  CHECK(f.run("arp on"));

  auto note_ons = [&]() {
    int n = 0;
    for (const OutEvent& e : f.events) {
      if (e.kind == OutEvent::Kind::kMidi && e.msg.type() == midi::kNoteOn && e.msg.d2 > 0) {
        ++n;
      }
    }
    return n;
  };

  // Hold a C major triad on the piano input port; the arp captures it while
  // playing (push_midi_in), so raw held notes do not pass straight through.
  CHECK(f.run("transport start"));
  const std::uint8_t on[9] = {0x90, 60, 100, 0x90, 64, 100, 0x90, 67, 100};
  f.shell.feed_midi(0, Span<const std::uint8_t>(on, sizeof(on)));

  // Advance exactly one bar and count arp note-ons. A 1/8 grid is ~8 steps per
  // bar: a small bounded number. A flood ("a manetta") would emit on the order
  // of one note per tick (hundreds per bar), so the bound below fails loudly if
  // the rhythm ever breaks.
  const int before = note_ons();
  std::string err;
  CHECK(f.shell.advance_by(kTicksPerBar, err));
  const int fired = note_ons() - before;
  CHECK(fired > 0);
  CHECK(fired < static_cast<int>(kTicksPerBar) / 10);  // decisively on the grid, not a flood
}

void test_tab_enters_piano_from_repl() {
  ShellFixture f;
  f.shell.configure_default_surfaces();  // arm steering so the captured key is observable
  CHECK(f.run("chord mode single"));
  CHECK(f.run("panel open piano"));
  // Merely opening a panel leaves focus on the REPL.
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kRepl);

  // TAB from the REPL drops into the only visible panel = piano play mode.
  CHECK(f.shell.handle_ui_key('\t'));
  CHECK(f.shell.panels().focus_kind() == PanelFocus::kPanel);
  CHECK(f.shell.panels().focused_panel() == PanelId::kPiano);

  // A musical key is now CAPTURED by the piano panel and steers the band (the
  // global-steer choke consumes it; nothing sounds — the harmony surface is silent).
  CHECK(f.shell.handle_ui_key('a'));
  CHECK(f.shell.harmony_held_count() == 1);
  CHECK(f.shell.engine().chords().state().valid);
  CHECK(f.shell.engine().chords().state().root_pc == 0);  // C

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

  // Panel numbers appear in the titles (1-based grid position, TOP-to-bottom so
  // they match reading order: menu paints above the piano, so menu is [1], the
  // piano [2]).
  CHECK(block_contains(grid, "[1]") && block_contains(grid, "[2]"));
  CHECK(pm.panel_number(PanelId::kHelp) == 1);
  CHECK(pm.panel_number(PanelId::kPiano) == 2);

  // TAB+number: focus_number targets the visible panel at that grid slot.
  CHECK(pm.focus_number(1));
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

void test_line_buffer_framing() {
  LineBuffer lb;
  std::vector<std::string> lines;

  // A line split across two feed() calls reassembles correctly.
  CHECK(lb.feed("hel", 3, lines));
  CHECK(lines.empty());
  CHECK(lb.feed("lo\n", 3, lines));
  CHECK(lines.size() == 1 && lines[0] == "hello");
  CHECK(lb.pending().empty());

  // Multiple lines in one chunk, plus a trailing partial line that stays
  // pending until the next feed() supplies its terminator.
  lines.clear();
  const std::string chunk = "one\ntwo\nthr";
  CHECK(lb.feed(chunk.data(), chunk.size(), lines));
  CHECK(lines.size() == 2 && lines[0] == "one" && lines[1] == "two");
  CHECK(lb.pending() == "thr");
  lines.clear();
  CHECK(lb.feed("ee\n", 3, lines));
  CHECK(lines.size() == 1 && lines[0] == "three");

  // Empty lines (blank input) are reported as empty strings, not dropped.
  lines.clear();
  CHECK(lb.feed("\n\nx\n", 4, lines));
  CHECK(lines.size() == 3);
  CHECK(lines[0].empty() && lines[1].empty() && lines[2] == "x");

  // CRLF framing: the trailing '\r' is stripped from the reassembled line.
  lines.clear();
  CHECK(lb.feed("crlf\r\n", 6, lines));
  CHECK(lines.size() == 1 && lines[0] == "crlf");
}

void test_line_buffer_overflow() {
  // A partial line that never terminates and grows past kMaxLineLength
  // reports overflow so the caller can drop a broken/hostile client instead
  // of buffering it without bound.
  LineBuffer lb;
  std::vector<std::string> lines;
  const std::string huge(LineBuffer::kMaxLineLength + 1, 'x');
  CHECK(!lb.feed(huge.data(), huge.size(), lines));
  CHECK(lines.empty());  // never terminated, so nothing was extracted
  CHECK(lb.pending().size() == huge.size());
}

void test_uds_server_start_errors() {
  // A path longer than sockaddr_un::sun_path is rejected before any syscall
  // that could otherwise misbehave on truncation.
  UdsServer too_long;
  CHECK(!too_long.start(std::string(200, 'x')));
  CHECK(!too_long.enabled());
  CHECK(too_long.listen_fd() < 0);

  // bind() failing (a directory component that does not exist) is reported
  // and leaves the adapter disabled rather than crashing.
  UdsServer bad_dir;
  CHECK(!bad_dir.start("/no/such/directory/arrangrr.sock"));
  CHECK(!bad_dir.enabled());
}

void test_uds_server_end_to_end() {
  // A real AF_UNIX socket exercised synchronously: no threads, no sleeps.
  // connect() on a unix stream socket completes as soon as the kernel queues
  // the new peer into the listen backlog (there is no real handshake), so
  // writing from the client before the server's accept() runs is safe and
  // deterministic — the bytes sit in the accepted socket's receive buffer.
  const std::string path =
      "/tmp/arrangrr_test_uds_" + std::to_string(static_cast<long>(::getpid())) + ".sock";

  UdsServer server;
  CHECK(server.start(path));
  CHECK(server.enabled());

  std::vector<std::pair<int, std::string>> received;
  server.set_line_handler(
      [&](int fd, const std::string& line) { received.emplace_back(fd, line); });

  const int client = ::socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(client >= 0);
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
  CHECK(::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

  // Two command lines in one write, split by the server's LineBuffer —
  // exercises the whole inbound path (accept -> read -> frame -> dispatch).
  const std::string wire = "transport start\nbpm 120\n";
  CHECK(::write(client, wire.data(), wire.size()) == static_cast<ssize_t>(wire.size()));

  server.handle_listen_readable();
  CHECK(server.client_fds().size() == 1);
  const int server_side_fd = server.client_fds()[0];
  server.handle_client_readable(server_side_fd);

  CHECK(received.size() == 2);
  CHECK(received[0].first == server_side_fd);
  CHECK(received[0].second == "transport start");
  CHECK(received[1].second == "bpm 120");

  // Outbound: broadcast() reaches the connected peer verbatim + newline.
  server.broadcast(R"({"ev":"warn","code":"unsupported","@":0})");
  char buf[256];
  const ssize_t n = ::read(client, buf, sizeof(buf));
  CHECK(n > 0);
  CHECK(std::string(buf, static_cast<std::size_t>(n)) ==
        "{\"ev\":\"warn\",\"code\":\"unsupported\",\"@\":0}\n");

  // send_error() reaches the same client as a JSON error object.
  server.send_error(server_side_fd, "unknown command: bogus", "bogus");
  const ssize_t n2 = ::read(client, buf, sizeof(buf));
  CHECK(n2 > 0);
  const std::string got2(buf, static_cast<std::size_t>(n2));
  CHECK(got2.find(R"("error":"unknown command: bogus")") != std::string::npos);
  CHECK(got2.find(R"("cmd":"bogus")") != std::string::npos);

  // JSON escaping of characters we did not generate ourselves (quotes,
  // backslash, newline, a raw control byte) — send_error()'s message and cmd
  // are arbitrary text, never host-controlled like jsonl.cpp's own strings.
  server.send_error(server_side_fd, "bad \"quote\"\\slash\nline", "cmd\x01x");
  const ssize_t n3 = ::read(client, buf, sizeof(buf));
  CHECK(n3 > 0);
  const std::string got3(buf, static_cast<std::size_t>(n3));
  CHECK(got3.find(R"(bad \"quote\"\\slash\nline)") != std::string::npos);
  CHECK(got3.find("cmd\\u0001x") != std::string::npos);

  CHECK(server.listen_fd() >= 0);

  // EOF: closing the client and letting the server observe it drops the
  // connection cleanly (no crash, client_fds() shrinks).
  ::close(client);
  server.handle_client_readable(server_side_fd);
  CHECK(server.client_fds().empty());
  // `server` going out of scope closes the listen fd and unlinks `path`.
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
  test_shell_bpm_command();
  test_shell_transpose_command();
  test_shell_transport_and_clock();
  test_shell_route_channel_remap();
  test_shell_error_paths();
  test_shell_parse_edges();
  test_shell_track_commands();
  test_note_name_parsing();
  test_shell_chord_commands();
  test_note_verb();
  test_shell_chord_detect_panel();
  test_note_letters_steer_from_every_panel_but_repl();
  test_permanent_transpose_persists_across_bars();
  test_pressing_a_then_s_yields_different_roots_when_properly_released();
  test_single_finger_new_key_replaces_previous_root();
  test_single_finger_g_then_h_does_not_collapse_to_em();
  test_gm_program_parsing();
  test_shell_program_command();
  test_shell_parts_command_and_panel();
  test_parts_solo_migrated_to_i_key();
  test_shell_groove_command_and_panel();
  test_shell_arp_command_and_panel();
  test_shell_seq_commands();
  test_shell_chord_modes_cli();
  test_shell_style_commands();
  test_shell_style_and_role_name_resolution();
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
  test_piano_harmony_gate_at_rest();
  test_piano_key_dispatch();
  test_piano_focus_shortcuts();
  test_kitty_key_parser();
  test_control_byte_for();
  test_momentary_lock();
  test_piano_momentary_mode();
  test_piano_space_toggles_mode();
  test_piano_plain_bytes_always_toggle();
  test_piano_toggle_autorepeat_debounce();
  test_piano_kitty_repeat_no_double_fire();
  test_arp_live_stays_on_grid();
  test_tab_enters_piano_from_repl();
  test_warn_names_complete();
  test_shell_name_tables_never_diverge();
  test_line_editor();
  test_alsa_null_state_is_safe();
  test_shell_pending_order_same_tick();
  test_line_buffer_framing();
  test_line_buffer_overflow();
  test_uds_server_start_errors();
  test_uds_server_end_to_end();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_host: all OK\n");
  }
  return arrangrr::test::failures();
}

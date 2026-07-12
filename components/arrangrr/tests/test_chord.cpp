#include <initializer_list>

#include "arrangrr/chord/chord_engine.hpp"
#include "chorddet/theory.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

void test_theory_scales_and_degrees() {
  // All seven modes keep 7 in-scale pitch classes.
  for (std::uint8_t m = 0; m < kModeCount; ++m) {
    const Key key{0, static_cast<Mode>(m)};
    int in_scale = 0;
    for (std::uint8_t pc = 0; pc < 12; ++pc) {
      if (theory::degree_of(key, pc) >= 0) {
        ++in_scale;
      }
    }
    CHECK(in_scale == 7);
  }
  // G major: F# in scale, F natural out.
  const Key g_major{7, Mode::kMajor};
  CHECK(theory::degree_of(g_major, 6) == 6);   // F#
  CHECK(theory::degree_of(g_major, 5) == -1);  // F
  // Dorian: major sixth (ii of C is D dorian... check D dorian has B).
  const Key d_dorian{2, Mode::kDorian};
  CHECK(theory::degree_of(d_dorian, 11) == 5);  // B natural
}

void test_theory_smart_qualities_all_degrees_major() {
  using theory::smart_quality;
  const ChordQuality expected[7] = {
      ChordQuality::kMaj7, ChordQuality::kMin7, ChordQuality::kMin7,     ChordQuality::kMaj7,
      ChordQuality::kDom7, ChordQuality::kMin7, ChordQuality::kHalfDim7,
  };
  for (int d = 0; d < 7; ++d) {
    CHECK(smart_quality(Mode::kMajor, d) == expected[d]);
  }
}

void test_theory_minor_harmonic_v() {
  using theory::smart_quality;
  CHECK(smart_quality(Mode::kMinor, 4) == ChordQuality::kDom7);   // harmonic exception
  CHECK(smart_quality(Mode::kDorian, 4) == ChordQuality::kMin7);  // other modes: pure stack
}

// shape_of returns the chord tones for every explicit quality (runtime call so
// each switch arm is covered, not just the compile-time self-tests).
void test_theory_shape_of_all_qualities() {
  using theory::shape_of;
  const ChordShape maj = shape_of(ChordQuality::kMaj);
  CHECK(maj.count == 3 && maj.offsets[0] == 0 && maj.offsets[1] == 4 && maj.offsets[2] == 7);
  const ChordShape min = shape_of(ChordQuality::kMin);
  CHECK(min.count == 3 && min.offsets[1] == 3 && min.offsets[2] == 7);
  const ChordShape dim = shape_of(ChordQuality::kDim);
  CHECK(dim.count == 3 && dim.offsets[1] == 3 && dim.offsets[2] == 6);
  const ChordShape aug = shape_of(ChordQuality::kAug);
  CHECK(aug.count == 3 && aug.offsets[1] == 4 && aug.offsets[2] == 8);
  const ChordShape maj7 = shape_of(ChordQuality::kMaj7);
  CHECK(maj7.count == 4 && maj7.offsets[3] == 11);
  const ChordShape min7 = shape_of(ChordQuality::kMin7);
  CHECK(min7.count == 4 && min7.offsets[1] == 3 && min7.offsets[3] == 10);
  const ChordShape dom7 = shape_of(ChordQuality::kDom7);
  CHECK(dom7.count == 4 && dom7.offsets[1] == 4 && dom7.offsets[3] == 10);
  const ChordShape hd7 = shape_of(ChordQuality::kHalfDim7);
  CHECK(hd7.count == 4 && hd7.offsets[2] == 6 && hd7.offsets[3] == 10);
  const ChordShape dim7 = shape_of(ChordQuality::kDim7);
  CHECK(dim7.count == 4 && dim7.offsets[2] == 6 && dim7.offsets[3] == 9);
  const ChordShape sus2 = shape_of(ChordQuality::kSus2);
  CHECK(sus2.count == 3 && sus2.offsets[1] == 2 && sus2.offsets[2] == 7);
  const ChordShape sus4 = shape_of(ChordQuality::kSus4);
  CHECK(sus4.count == 3 && sus4.offsets[1] == 5 && sus4.offsets[2] == 7);
}

// degree_to_semitones with floor division: positive degrees wrap up an octave
// every 7 steps, negative degrees wrap down (the index<0 correction branch).
void test_theory_degree_to_semitones_runtime() {
  using theory::degree_to_semitones;
  CHECK(degree_to_semitones(Mode::kMajor, 0) == 0);
  CHECK(degree_to_semitones(Mode::kMajor, 4) == 7);
  CHECK(degree_to_semitones(Mode::kMajor, 7) == 12);   // tonic, octave up
  CHECK(degree_to_semitones(Mode::kMajor, 8) == 14);
  CHECK(degree_to_semitones(Mode::kMajor, -1) == -1);  // leading tone below tonic
  CHECK(degree_to_semitones(Mode::kMajor, -7) == -12);
  CHECK(degree_to_semitones(Mode::kMajor, -8) == -13);
  CHECK(degree_to_semitones(Mode::kMinor, 2) == 3);
  CHECK(degree_to_semitones(Mode::kMinor, -1) == -2);
}

// complete_shell decision tree: drive every branch with an interval set that
// selects exactly that arm (runtime call for coverage).
void test_theory_complete_shell_all_branches() {
  using theory::complete_shell;
  auto q = [](std::initializer_list<std::uint8_t> ivs) {
    std::uint8_t iv[3] = {0, 0, 0};
    std::uint8_t n = 0;
    for (std::uint8_t v : ivs) {
      iv[n++] = v;
    }
    return complete_shell(iv, n);
  };
  CHECK(q({3, 6, 10}) == ChordQuality::kHalfDim7);  // min3 && dim5 && min7
  CHECK(q({3, 6}) == ChordQuality::kDim);           // min3 && dim5, no 7
  CHECK(q({4, 8}) == ChordQuality::kAug);           // maj3 && aug5
  CHECK(q({3, 10}) == ChordQuality::kMin7);         // min7 with min3
  CHECK(q({4, 10}) == ChordQuality::kDom7);         // min7 with maj3 -> dominant
  CHECK(q({10}) == ChordQuality::kDom7);            // bare min7 -> dominant
  CHECK(q({4, 11}) == ChordQuality::kMaj7);         // maj7 present
  CHECK(q({11}) == ChordQuality::kMaj7);            // bare maj7
  CHECK(q({3}) == ChordQuality::kMin);              // just a minor third
  CHECK(q({4}) == ChordQuality::kMaj);              // just a major third
  CHECK(q({5}) == ChordQuality::kSus4);             // suspended fourth
  CHECK(q({2}) == ChordQuality::kSus2);             // suspended second
  CHECK(q({7}) == ChordQuality::kMaj);              // bare fifth -> major (default arm)
  CHECK(q({}) == ChordQuality::kMaj);               // bare root -> major
}

// complete_shell_full folds in the diminished-seventh (bb7) case, otherwise
// delegates to complete_shell.
void test_theory_complete_shell_full() {
  using theory::complete_shell_full;
  auto q = [](std::initializer_list<std::uint8_t> ivs) {
    std::uint8_t iv[3] = {0, 0, 0};
    std::uint8_t n = 0;
    for (std::uint8_t v : ivs) {
      iv[n++] = v;
    }
    return complete_shell_full(iv, n);
  };
  CHECK(q({3, 6, 9}) == ChordQuality::kDim7);       // min3 && dim5 && bb7 -> dim7
  CHECK(q({3, 6, 10}) == ChordQuality::kHalfDim7);  // no bb7 -> delegate
  CHECK(q({4}) == ChordQuality::kMaj);              // delegate, plain major
  CHECK(q({9}) == ChordQuality::kMaj);              // bb7 alone (no min3/dim5) -> delegate
}

using Events = StaticVector<OutEvent, 64>;

struct ChordFixture {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command;
    command.op = op;
    command.param = p;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void play(std::uint8_t note, std::int8_t quality = -1, std::uint8_t vel = 100) {
    cmd(Param::kChordPlay, note, quality, vel);
  }
  StaticVector<std::uint8_t, 8> notes_of(std::uint8_t type) const {
    StaticVector<std::uint8_t, 8> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == type) {
        CHECK(out.push_back(o.msg.d1));
      }
    }
    return out;
  }
};

void test_play_d_in_c_major_is_dm7() {
  // The G1 spec: key C major, play D4 (62) -> Dm7 = 62 65 69 72.
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
  f.play(62);
  const OutEvent& chord = f.ev[0];
  CHECK(chord.kind == OutEvent::Kind::kChord);
  CHECK((chord.code & 0xFF) == 1);  // ii
  CHECK((chord.code >> 8) == static_cast<std::uint16_t>(ChordQuality::kMin7));
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 4 && ons[0] == 62 && ons[1] == 65 && ons[2] == 69 && ons[3] == 72);
}

void test_chord_change_releases_previous_first() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.play(62);  // Dm7
  f.ev.clear();
  f.play(67);  // G7
  // Same tick: all four NoteOffs (previous chord) precede the NoteOns (D29).
  int seen_on = 0, offs_after_on = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind != OutEvent::Kind::kMidi) {
      continue;
    }
    if (o.msg.type() == midi::kNoteOn) {
      ++seen_on;
    }
    if (o.msg.type() == midi::kNoteOff && seen_on > 0) {
      ++offs_after_on;
    }
  }
  CHECK(offs_after_on == 0);
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 4 && ons[0] == 67 && ons[3] == 77);  // G7: 67 71 74 77
}

void test_chord_stop_and_out_of_key() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.play(62);
  f.ev.clear();
  f.cmd(Param::kChordStop);
  CHECK(f.notes_of(midi::kNoteOff).size() == 4);
  CHECK(!f.e.chords().sounding());
  f.ev.clear();
  f.play(61);  // C# is chromatic in C major -> warn, no sound (D20)
  CHECK(f.ev.size() == 1);
  CHECK(f.ev[0].kind == OutEvent::Kind::kWarn);
  CHECK(f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kNotInKey));
}

void test_quality_override_and_output_channel() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kChordOut, 1 | (4 << 8), 0, 0, Op::kSet);  // port 1, channel 5
  f.play(60, static_cast<std::int8_t>(ChordQuality::kSus4));
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 3 && ons[0] == 60 && ons[1] == 65 && ons[2] == 67);
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kMidi) {
      CHECK(o.port == 1 && o.msg.channel() == 4);
    }
  }
}

void test_minor_key_v_is_dominant() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 9, static_cast<std::int32_t>(Mode::kMinor), 0, Op::kSet);  // A minor
  f.play(64);  // E4 -> E7 (harmonic V), not Em7
  const OutEvent& chord = f.ev[0];
  CHECK((chord.code >> 8) == static_cast<std::uint16_t>(ChordQuality::kDom7));
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 4 && ons[0] == 64 && ons[1] == 68 && ons[2] == 71 && ons[3] == 74);
}

void test_range_clamp_and_bad_args() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.play(127);  // G9, top of range: tones above 127 are dropped, no crash
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() >= 1 && ons.size() < 4);
  f.ev.clear();
  f.play(62, 99);  // bogus quality override
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
  f.ev.clear();
  f.cmd(Param::kKeySet, 15, 0, 0, Op::kSet);  // bad root pc
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
  f.ev.clear();
  f.cmd(Param::kChordOut, 9, 0, 0, Op::kSet);  // bad port
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
}

void test_single_finger_mode() {
  ChordFixture f;
  f.cmd(Param::kChordMode, 1, 0, 0, Op::kSet);
  f.play(66);  // F#4: chromatic anywhere, allowed in absolute mode
  const OutEvent& chord = f.ev[0];
  CHECK(chord.kind == OutEvent::Kind::kChord);
  CHECK((chord.code & 0xFF) == kNoDegree);
  CHECK((chord.code >> 8) == static_cast<std::uint16_t>(ChordQuality::kMaj));
  const auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 3 && ons[0] == 66 && ons[1] == 70 && ons[2] == 73);
  f.ev.clear();
  f.play(60, static_cast<std::int8_t>(ChordQuality::kMin7));  // override still wins
  const auto ons2 = f.notes_of(midi::kNoteOn);
  CHECK(ons2.size() == 4 && ons2[1] == 63);
}

void test_shell_mode_completion() {
  ChordFixture f;
  f.cmd(Param::kChordMode, 2, 0, 0, Op::kSet);
  // C4 + E4 + Bb4 packed -> C7 completed (60 64 67 70).
  f.cmd(Param::kChordPlay, 60 | (64 << 8) | (70 << 16), -1, 100);
  const OutEvent& chord = f.ev[0];
  CHECK((chord.code >> 8) == static_cast<std::uint16_t>(ChordQuality::kDom7));
  auto ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 4 && ons[0] == 60 && ons[1] == 64 && ons[2] == 67 && ons[3] == 70);
  f.ev.clear();
  // D4 + F4 -> Dm completed; lowest note wins as root even if unordered.
  f.cmd(Param::kChordPlay, 65 | (62 << 8), -1, 100);
  ons = f.notes_of(midi::kNoteOn);
  CHECK(ons.size() == 3 && ons[0] == 62 && ons[1] == 65 && ons[2] == 69);
  f.ev.clear();
  // Bare note in shell mode falls back to major.
  f.cmd(Param::kChordPlay, 60, -1, 100);
  CHECK((f.ev[0].code >> 8) == static_cast<std::uint16_t>(ChordQuality::kMaj));
}

void test_mode_switch_and_bad_mode() {
  ChordFixture f;
  f.cmd(Param::kChordMode, 1, 0, 0, Op::kSet);
  f.cmd(Param::kChordMode, 0, 0, 0, Op::kSet);  // back to diatonic
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.ev.clear();
  f.play(61);  // chromatic again rejected in diatonic mode
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kNotInKey));
  f.ev.clear();
  f.cmd(Param::kChordMode, 9, 0, 0, Op::kSet);
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
}

void test_output_switch_releases_old_port() {
  // chord out while a chord sounds: the NoteOffs must go to the OLD port,
  // or the old destination is left with four stuck notes.
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.play(62);  // Dm7 on port 0
  f.ev.clear();
  f.cmd(Param::kChordOut, 1 | (0 << 8), 0, 0, Op::kSet);  // move to port 1
  int offs_on_old = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOff && o.port == 0) {
      ++offs_on_old;
    }
  }
  CHECK(offs_on_old == 4);
  CHECK(!f.e.chords().sounding());
  f.ev.clear();
  f.play(62);  // next chord sounds on the new port
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kMidi) {
      CHECK(o.port == 1);
    }
  }
}

void test_chord_hold_is_honestly_unsupported() {
  ChordFixture f;
  f.cmd(Param::kChordHold, 1, 0, 0, Op::kSet);
  CHECK(f.ev.size() == 1);
  CHECK(f.ev[0].kind == OutEvent::Kind::kWarn);
  CHECK(f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kUnsupported));
}

void test_panic_covers_chord_notes() {
  ChordFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.play(62);
  f.ev.clear();
  f.cmd(Param::kPanic);
  CHECK(f.notes_of(midi::kNoteOff).size() == 4);  // tracker saw the chord
}

}  // namespace

int main() {
  test_theory_scales_and_degrees();
  test_theory_smart_qualities_all_degrees_major();
  test_theory_minor_harmonic_v();
  test_theory_shape_of_all_qualities();
  test_theory_degree_to_semitones_runtime();
  test_theory_complete_shell_all_branches();
  test_theory_complete_shell_full();
  test_play_d_in_c_major_is_dm7();
  test_chord_change_releases_previous_first();
  test_chord_stop_and_out_of_key();
  test_quality_override_and_output_channel();
  test_minor_key_v_is_dominant();
  test_range_clamp_and_bad_args();
  test_single_finger_mode();
  test_shell_mode_completion();
  test_mode_switch_and_bad_mode();
  test_output_switch_releases_old_port();
  test_chord_hold_is_honestly_unsupported();
  test_panic_covers_chord_notes();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord: all OK\n");
  }
  return arrangrr::test::failures();
}

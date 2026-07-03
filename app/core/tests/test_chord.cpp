#include "arrangrr/chord/chord_engine.hpp"
#include "arrangrr/chord/theory.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

void test_theory_scales_and_degrees() {
  // All seven modes keep 7 in-scale pitch classes.
  for (std::uint8_t m = 0; m < kModeCount; ++m) {
    const Key key{0, static_cast<Mode>(m)};
    int in_scale = 0;
    for (std::uint8_t pc = 0; pc < 12; ++pc)
      if (theory::degree_of(key, pc) >= 0) ++in_scale;
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
      ChordQuality::kMaj7, ChordQuality::kMin7, ChordQuality::kMin7, ChordQuality::kMaj7,
      ChordQuality::kDom7, ChordQuality::kMin7, ChordQuality::kHalfDim7,
  };
  for (int d = 0; d < 7; ++d) CHECK(smart_quality(Mode::kMajor, d) == expected[d]);
}

void test_theory_minor_harmonic_v() {
  using theory::smart_quality;
  CHECK(smart_quality(Mode::kMinor, 4) == ChordQuality::kDom7);   // harmonic exception
  CHECK(smart_quality(Mode::kDorian, 4) == ChordQuality::kMin7);  // other modes: pure stack
}

using Events = StaticVector<OutEvent, 64>;

struct ChordFixture {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           Op op = Op::kDo) {
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
    for (const OutEvent& o : ev)
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == type) CHECK(out.push_back(o.msg.d1));
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
    if (o.kind != OutEvent::Kind::kMidi) continue;
    if (o.msg.type() == midi::kNoteOn) ++seen_on;
    if (o.msg.type() == midi::kNoteOff && seen_on > 0) ++offs_after_on;
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
  for (const OutEvent& o : f.ev)
    if (o.kind == OutEvent::Kind::kMidi) CHECK(o.port == 1 && o.msg.channel() == 4);
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
  test_play_d_in_c_major_is_dm7();
  test_chord_change_releases_previous_first();
  test_chord_stop_and_out_of_key();
  test_quality_override_and_output_channel();
  test_minor_key_v_is_dominant();
  test_range_clamp_and_bad_args();
  test_panic_covers_chord_notes();
  if (arrangrr::test::failures() == 0) std::printf("test_chord: all OK\n");
  return arrangrr::test::failures();
}

#include "arrangrr/chord/chord_sequence.hpp"
#include "arrangrr/chord/chord_sequencer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

void test_sequence_free_durations_and_edit() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  CHECK(s.append(1, -1, 100, 2 * kTicksPerBar));  // ii for 2 bars
  CHECK(s.append(4, -1, 100, kTicksPerBar));      // V for 1 bar
  CHECK(s.append(0, -1, 100, kTicksPerBar));      // I
  CHECK(s.count() == 3);
  CHECK(s.length() == 4 * kTicksPerBar);
  CHECK(s.step(1).start == 2 * kTicksPerBar);
  CHECK(!s.append(0, -1, 100, 0));  // zero duration refused
  // Removing the first step closes the gap.
  CHECK(s.remove(0));
  CHECK(s.count() == 2 && s.step(0).start == 0 && s.length() == 2 * kTicksPerBar);
  CHECK(!s.remove(9));
  s.clear();
  CHECK(s.count() == 0 && s.length() == 0);
}

void test_sequence_quantize_after() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  // Sloppy live starts near 0, 1, 2 bars.
  CHECK(s.record(ChordStep{130, 0, 1, -1, 100}));
  CHECK(s.record(ChordStep{kTicksPerBar - 90, 0, 4, -1, 100}));
  CHECK(s.record(ChordStep{2 * kTicksPerBar + 200, 3 * kTicksPerBar / 2, 0, -1, 100}));
  s.quantize();
  CHECK(s.step(0).start == 0);
  CHECK(s.step(1).start == kTicksPerBar);
  CHECK(s.step(2).start == 2 * kTicksPerBar);
  CHECK(s.step(0).duration == kTicksPerBar);
  CHECK(s.step(1).duration == kTicksPerBar);
  CHECK(s.step(2).duration == 2 * kTicksPerBar);  // 1.5 bars rounds up to 2
}

void test_transpose_re_derives() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  CHECK(s.append(1, -1, 100, kTicksPerBar));
  s.transpose_to(7, -1);  // to G major, degrees untouched
  CHECK(s.key.root_pc == 7 && s.key.mode == Mode::kMajor);
  CHECK(s.step(0).degree == 1);
  s.transpose_by(2);
  CHECK(s.key.root_pc == 9);
  s.transpose_by(-12);
  CHECK(s.key.root_pc == 9);
}

using Events = StaticVector<OutEvent, 256>;

struct SeqFixture {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           Op op = Op::kDo, std::uint16_t idx = 0) {
    Command command;
    command.op = op;
    command.param = p;
    command.idx = idx;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void add(std::uint8_t note, Tick dur, std::int8_t quality = -1, std::uint8_t vel = 100) {
    cmd(Param::kSeqAdd, note, (quality + 1) | (vel << 8), static_cast<std::int32_t>(dur));
  }
  StaticVector<std::uint16_t, 32> chords() const {
    StaticVector<std::uint16_t, 32> out;
    for (const OutEvent& o : ev)
      if (o.kind == OutEvent::Kind::kChord) CHECK(out.push_back(o.code));
    return out;
  }
  static std::uint8_t degree(std::uint16_t code) { return code & 0xFF; }
  static ChordQuality quality(std::uint16_t code) { return static_cast<ChordQuality>(code >> 8); }
};

void test_progression_playback_and_loop() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);  // D -> ii
  f.add(67, kTicksPerBar);  // G -> V
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(2 * kTicksPerBar);  // full cycle + wrap into step 0 again
  const auto ch = f.chords();
  CHECK(ch.size() == 3);
  CHECK(SeqFixture::degree(ch[0]) == 1 && SeqFixture::quality(ch[0]) == ChordQuality::kMin7);
  CHECK(SeqFixture::degree(ch[1]) == 4 && SeqFixture::quality(ch[1]) == ChordQuality::kDom7);
  CHECK(SeqFixture::degree(ch[2]) == 1);  // loop wrapped
}

void test_no_loop_stops_and_releases() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(60, kTicksPerBar);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(kTicksPerBar + 10);
  CHECK(!f.e.sequences().playing());
  // The voicing was released at the sequence end.
  int ons = 0, offs = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind != OutEvent::Kind::kMidi) continue;
    if (o.msg.type() == midi::kNoteOn) ++ons;
    if (o.msg.type() == midi::kNoteOff) ++offs;
  }
  CHECK(ons == 4 && offs == 4);
}

void test_transpose_to_g_replays_rederived() {
  // The WOW: | ii V | in C plays Dm7 G7; transposed to G it plays Am7 D7.
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);
  f.add(67, kTicksPerBar);
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(2 * kTicksPerBar - 1);
  f.cmd(Param::kSeqTranspose, 7, -1, 0, Op::kSet);  // to G major
  f.advance(2 * kTicksPerBar);
  // Roots: Dm7(62..) G7 | Am7 D7 — check the note-on roots per chord event order.
  StaticVector<std::uint8_t, 16> roots;
  for (const OutEvent& o : f.ev)
    if (o.kind == OutEvent::Kind::kChord) CHECK(roots.push_back(o.msg.status));
  CHECK(roots.size() == 4);
  CHECK(roots[0] == 62 && roots[1] == 67);  // C major: D, G
  CHECK(roots[2] == 69 && roots[3] == 62);  // G major: ii=A, V=D
}

void test_record_quantize_playback() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqRec);
  // Two live chords, sloppy timing (~1 bar apart), captured while sounding.
  f.cmd(Param::kChordPlay, 62, -1, 100);
  f.advance(kTicksPerBar + 70);
  f.cmd(Param::kChordPlay, 67, -1, 100);
  f.advance(kTicksPerBar - 40);
  f.cmd(Param::kSeqStop);  // quantize-after to whole bars
  const ChordSequence* seq = f.e.sequences().current();
  CHECK(seq->count() == 2);
  CHECK(seq->step(0).start == 0 && seq->step(0).duration == kTicksPerBar);
  CHECK(seq->step(1).start == kTicksPerBar && seq->step(1).duration == kTicksPerBar);
  CHECK(seq->step(0).degree == 1 && seq->step(1).degree == 4);
}

void test_seq_warns() {
  SeqFixture f;
  f.cmd(Param::kSeqPlay);  // nothing to play
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqEmpty));
  f.ev.clear();
  f.cmd(Param::kSeqAdd, 60, 1 | (100 << 8), 100);  // no sequence yet
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
  f.ev.clear();
  f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqAdd, 61, 0 | (100 << 8), kTicksPerBar);  // C# chromatic
  CHECK(f.ev.size() == 1 &&
        f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kNotInKey));
  f.ev.clear();
  for (std::size_t i = 1; i < kMaxChordSequences; ++i) f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqNew);
  CHECK(f.ev.size() == 1 &&
        f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqTableFull));
  f.ev.clear();
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 99);
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
}

}  // namespace

int main() {
  test_sequence_free_durations_and_edit();
  test_sequence_quantize_after();
  test_transpose_re_derives();
  test_progression_playback_and_loop();
  test_no_loop_stops_and_releases();
  test_transpose_to_g_replays_rederived();
  test_record_quantize_playback();
  test_seq_warns();
  if (arrangrr::test::failures() == 0) std::printf("test_chord_seq: all OK\n");
  return arrangrr::test::failures();
}

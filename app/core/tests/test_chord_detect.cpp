#include "arrangrr/chord/chord_detector.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

namespace {

using namespace arrangrr;

// --- pure ChordDetector: recognition mirrors shell-mode entry --------------

void test_detector_needs_a_triad() {
  ChordDetector d;
  ChordState s;
  d.note_on(60);  // C
  CHECK(!d.recognize(s));  // one note: no chord
  d.note_on(64);           // E
  CHECK(!d.recognize(s));  // two notes: an interval, not a chord
  d.note_on(67);           // G
  CHECK(d.recognize(s));   // C major triad
  CHECK(s.valid);
  CHECK(s.root_pc == 0);
  CHECK(s.quality == ChordQuality::kMaj);
}

void test_detector_lowest_note_is_root() {
  ChordDetector d;
  ChordState s;
  d.note_on(69);  // A3
  d.note_on(72);  // C4
  d.note_on(76);  // E4  -> A minor (root A, +3, +7)
  CHECK(d.recognize(s));
  CHECK(s.root_pc == 9);  // A
  CHECK(s.quality == ChordQuality::kMin);
}

void test_detector_seventh_qualities() {
  {  // C E G Bb -> dominant 7
    ChordDetector d;
    ChordState s;
    d.note_on(60);
    d.note_on(64);
    d.note_on(67);
    d.note_on(70);  // Bb
    CHECK(d.recognize(s));
    CHECK(s.root_pc == 0);
    CHECK(s.quality == ChordQuality::kDom7);
  }
  {  // C Eb G B -> minor/major mismatch folds to maj7 family per complete_shell
    ChordDetector d;
    ChordState s;
    d.note_on(60);
    d.note_on(63);  // Eb -> min3
    d.note_on(67);
    d.note_on(71);  // B  -> maj7
    CHECK(d.recognize(s));
    CHECK(s.quality == ChordQuality::kMaj7);
  }
}

void test_detector_octave_doubling_dedup() {
  // A doubled third across octaves must not fill the interval slots and hide
  // the seventh: C(60) E(64) G(67) E(76) Bb(70) still reads dominant 7.
  ChordDetector d;
  ChordState s;
  d.note_on(60);
  d.note_on(64);
  d.note_on(76);  // E an octave up (duplicate pitch class)
  d.note_on(67);
  d.note_on(70);  // Bb
  CHECK(d.recognize(s));
  CHECK(s.quality == ChordQuality::kDom7);
}

void test_detector_chord_memory_and_clear() {
  ChordDetector d;
  ChordState s;
  d.note_on(60);
  d.note_on(64);
  d.note_on(67);
  CHECK(d.recognize(s));           // C major
  d.note_off(64);                  // drop below a triad
  CHECK(d.held_count() == 2);
  CHECK(!d.recognize(s));          // no new chord: caller keeps the last one
  d.note_off(60);
  d.note_off(67);
  CHECK(d.held_count() == 0);
  CHECK(!d.recognize(s));
  // Idempotent set: a duplicate NoteOn / a NoteOff of an unheld note is a no-op.
  d.note_on(60);
  d.note_on(60);
  CHECK(d.held_count() == 1);
  d.note_off(72);
  CHECK(d.held_count() == 1);
  d.clear();
  CHECK(d.held_count() == 0);
}

// --- engine integration: held keys re-harmonize the running band -----------

using Events = StaticVector<OutEvent, 512>;

struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds a NoteOn (vel>0) or NoteOff (vel==0) on the detect port (0).
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>((vel > 0 ? 0x90 : 0x80));
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
  }
  int bass_ons(std::uint8_t note) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == 1 && o.msg.d1 == note) {
        ++n;
      }
    }
    return n;
  }
};

void test_live_keys_reharmonize_the_band() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detection on, port 0
  // Hold an A minor triad on the keyboard (A3 C4 E4).
  b.key(69, 100);
  b.key(72, 100);
  b.key(76, 100);
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  // Bass anchors at 36 + root_pc: A minor -> 36 + 9 = 45 (A2), never 36 (C2).
  CHECK(b.bass_ons(45) > 0);
  CHECK(b.bass_ons(36) == 0);
}

void test_chord_memory_holds_after_release() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.key(69, 100);
  b.key(72, 100);
  b.key(76, 100);       // A minor recognized
  b.key(69, 0);         // release every key
  b.key(72, 0);
  b.key(76, 0);
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  CHECK(b.bass_ons(45) > 0);  // band keeps playing on the last chord (memory)
}

void test_detection_off_leaves_band_chordless() {
  Band b;
  b.setup_basic();
  // Detection stays OFF: held keys must NOT steer the arranger.
  b.key(69, 100);
  b.key(72, 100);
  b.key(76, 100);
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  CHECK(b.bass_ons(45) == 0);  // tonal roles silent with no chord (NTT waits)
  CHECK(b.bass_ons(36) == 0);
}

}  // namespace

int main() {
  test_detector_needs_a_triad();
  test_detector_lowest_note_is_root();
  test_detector_seventh_qualities();
  test_detector_octave_doubling_dedup();
  test_detector_chord_memory_and_clear();
  test_live_keys_reharmonize_the_band();
  test_chord_memory_holds_after_release();
  test_detection_off_leaves_band_chordless();
  return 0;
}

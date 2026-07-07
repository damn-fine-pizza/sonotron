// Does pressing a chord actually change the NOTES the band plays?
//
// The other suites prove the followed CONTEXT changes (chords().state()) and
// that the arranger follows a chord set via `chord play` (test_arranger's
// test_ntt_resolution_follows_chord). This one closes the loop the user cares
// about: play a chord LIVE on the detect port (single-finger AND fingered), with
// the transport PLAYING and a real style loaded, and assert the arranger's
// EMITTED bass/chord notes re-harmonize to the pressed chord — and change when
// the chord changes. It prints the notes so the behaviour is visible, not just
// asserted.

#include <cstdio>

#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 1024>;

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
  // One NoteOn (vel>0) / NoteOff (vel==0) on an input port.
  void key(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // basic style: drums->ch9, bass->ch1, chord1->ch2. Detect on port 0.
  void setup() {
    cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
    cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detect on, source = port 0
  }
  // All note-on pitches emitted on a channel since the last clear, sorted-ish by
  // arrival, printed for visibility.
  StaticVector<std::uint8_t, 32> ons(std::uint8_t channel) const {
    StaticVector<std::uint8_t, 32> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == channel) {
        CHECK(out.push_back(o.msg.d1));
      }
    }
    return out;
  }
  bool played(std::uint8_t channel, std::uint8_t note) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == channel && o.msg.d1 == note) {
        return true;
      }
    }
    return false;
  }
};

void print_bar(const char* label, const Band& b) {
  std::printf("  %-22s bass(ch1):", label);
  for (std::uint8_t n : b.ons(1)) {
    std::printf(" %u", n);
  }
  std::printf("   chord(ch2):");
  for (std::uint8_t n : b.ons(2)) {
    std::printf(" %u", n);
  }
  std::printf("\n");
}

// SINGLE-FINGER: one key on the detect port sets the chord; the bass root must
// track it. C major bass root = 36 (C2); + pitch-class offset per chord.
void test_single_finger_steers_the_band() {
  Band b;
  b.setup();
  b.cmd(Param::kChordMode, static_cast<std::int32_t>(ChordMode::kSingle), 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);

  std::printf("single-finger (transport playing, style basic/VarA):\n");

  // Press C (60) -> C major. Advance one bar.
  b.key(0, 60, 100);
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar("press C -> bass ~36", b);
  const bool c_bass = b.played(1, 36);  // C2

  // Press F (65) -> F major. The band must move to F.
  b.key(0, 60, 0);  // release C
  b.key(0, 65, 100);
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar("press F -> bass ~41", b);
  const bool f_bass = b.played(1, 41);  // F2

  // Press G (67) -> G major.
  b.key(0, 65, 0);
  b.key(0, 67, 100);
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar("press G -> bass ~43", b);
  const bool g_bass = b.played(1, 43);  // G2

  CHECK(c_bass);  // C chord -> bass plays C2
  CHECK(f_bass);  // F chord -> bass MOVED to F2
  CHECK(g_bass);  // G chord -> bass MOVED to G2
}

// FINGERED (multi-finger): a full triad on the detect port.
void test_fingered_triad_steers_the_band() {
  Band b;
  b.setup();  // diatonic mode by default (fingered needs the triad)
  b.cmd(Param::kTransportStart);

  std::printf("fingered / multi-finger (transport playing):\n");

  // F major triad F-A-C (65,69,72).
  const std::uint8_t fmaj[9] = {0x90, 65, 100, 0x90, 69, 100, 0x90, 72, 100};
  b.e.push_midi_in(0, Span<const std::uint8_t>(fmaj, 9),
                   [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar("F triad -> bass ~41", b);
  const bool f_bass = b.played(1, 41);

  // G major triad G-B-D (67,71,74).
  const std::uint8_t fmaj_off[9] = {0x80, 65, 0, 0x80, 69, 0, 0x80, 72, 0};
  b.e.push_midi_in(0, Span<const std::uint8_t>(fmaj_off, 9),
                   [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  const std::uint8_t gmaj[9] = {0x90, 67, 100, 0x90, 71, 100, 0x90, 74, 100};
  b.e.push_midi_in(0, Span<const std::uint8_t>(gmaj, 9),
                   [&](const OutEvent& o) { CHECK(b.ev.push_back(o)); });
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar("G triad -> bass ~43", b);
  const bool g_bass = b.played(1, 43);

  CHECK(f_bass);
  CHECK(g_bass);
}

}  // namespace

int main() {
  test_single_finger_steers_the_band();
  test_fingered_triad_steers_the_band();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_harmony_steer: all OK\n");
  }
  return arrangrr::test::failures();
}

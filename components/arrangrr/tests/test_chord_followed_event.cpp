// Functional tests for the additive kChordFollowed OutEvent (node 0700): the
// engine announces the followed harmonic context (current + pending) whenever it
// changes, from ANY producer, so the host GUI harmony visualizer can track it.
//
// These drive the real ABI (binary commands + raw MIDI in) and assert on the
// OBSERVABLE kChordFollowed events: their decoded cur/next chord, the producer
// that caused the change, and the delta policy (exactly one event per genuine
// change, none on ticks where the followed chord did not move). The four emit
// sites are all covered here: manual play, the ChordSequencer, live detection,
// and the bar-boundary promotion of a shift-staged chord (honest src via the
// producer that staged it).

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

// The decoded payload of a kChordFollowed event (mirrors the packing in
// OutEvent::chord_followed: cur/next each ride one byte as root_pc|quality<<4,
// the valid latches and the 2-bit Producer share msg.d2).
struct Followed {
  std::uint8_t cur_root;
  ChordQuality cur_quality;
  bool cur_valid;
  std::uint8_t next_root;
  ChordQuality next_quality;
  bool next_valid;
  Producer src;
};

Followed decode(const OutEvent& o) {
  return Followed{
      .cur_root = static_cast<std::uint8_t>(o.msg.status & 0x0F),
      .cur_quality = static_cast<ChordQuality>(o.msg.status >> 4),
      .cur_valid = (o.msg.d2 & 0x1) != 0,
      .next_root = static_cast<std::uint8_t>(o.msg.d1 & 0x0F),
      .next_quality = static_cast<ChordQuality>(o.msg.d1 >> 4),
      .next_valid = (o.msg.d2 & 0x2) != 0,
      .src = static_cast<Producer>((o.msg.d2 >> 2) & 0x3),
  };
}

struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           std::uint16_t idx = 0) {
    Command command{.op = op, .param = p, .idx = idx, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }

  // C major band, chord voicing routed to channel 5. idx=0 => immediate manual
  // play; idx=1 => SHIFT-quantized (staged to the next bar).
  void setup() {
    cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
    cmd(Param::kChordOut, 0 | (5 << 8), 0, 0, Op::kSet);
  }
  void play(std::uint8_t note, std::uint16_t shift = 0) {
    cmd(Param::kChordPlay, note, /*smart*/ -1, /*vel*/ 100, Op::kDo, shift);
  }

  int followed_count() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChordFollowed) {
        ++n;
      }
    }
    return n;
  }
  // The last kChordFollowed emitted (the test asserts on the freshest change).
  Followed last_followed() const {
    Followed f{};
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChordFollowed) {
        f = decode(o);
      }
    }
    return f;
  }
  void clear() { ev.clear(); }
};

// A manual `chord play C` announces exactly one change: cur = Cmaj7 (I of C
// major), no pending, src = manual.
void test_manual_immediate_announces_once() {
  Band b;
  b.setup();
  b.play(60);  // C4 -> Cmaj7
  CHECK(b.followed_count() == 1);
  const Followed f = b.last_followed();
  CHECK(f.cur_valid);
  CHECK(f.cur_root == 0);
  CHECK(f.cur_quality == ChordQuality::kMaj7);
  CHECK(!f.next_valid);
  CHECK(f.src == Producer::kManual);
}

// Replaying the SAME chord emits no new event (delta policy); a DIFFERENT chord
// does. Proves "no chord-followed on ticks where the chord did not change".
void test_delta_suppresses_repeats() {
  Band b;
  b.setup();
  b.play(60);  // Cmaj7 -> one event
  CHECK(b.followed_count() == 1);
  b.play(60);  // identical -> suppressed
  CHECK(b.followed_count() == 1);
  b.play(62);  // D4 -> Dm7, a genuine change
  CHECK(b.followed_count() == 2);
  const Followed f = b.last_followed();
  CHECK(f.cur_root == 2);
  CHECK(f.cur_quality == ChordQuality::kMin7);
  CHECK(f.src == Producer::kManual);
}

// The ChordSequencer's step announces the change with src = sequencer.
void test_sequencer_announces() {
  Band b;
  b.setup();
  b.cmd(Param::kSeqNew);
  b.cmd(Param::kSeqAdd, 67, (/*smart*/ -1 + 1) | (100 << 8),
        static_cast<std::int32_t>(kTicksPerBar));  // G -> V (G7)
  b.cmd(Param::kSeqPlay);
  b.clear();  // isolate the events produced from transport start onward
  b.cmd(Param::kTransportStart);
  b.advance(1);  // the armed step fires on the first played tick
  bool saw = false;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kChordFollowed) {
      const Followed f = decode(o);
      CHECK(f.src == Producer::kSequencer);
      CHECK(f.cur_valid && f.cur_root == 7 && f.cur_quality == ChordQuality::kDom7);
      saw = true;
    }
  }
  CHECK(saw);
}

// Live piano->chord detection announces the change with src = detect.
void test_detect_announces() {
  Band b;
  b.setup();
  b.cmd(Param::kChordDetect, 1, /*port*/ 0, 0, Op::kSet);
  b.key(69, 100);  // A
  b.key(72, 100);  // C
  b.clear();
  b.key(76, 100);  // E -> completes A minor, grows the held set -> recognize
  const Followed f = b.last_followed();
  CHECK(b.followed_count() >= 1);
  CHECK(f.src == Producer::kDetect);
  CHECK(f.cur_valid && f.cur_root == 9 && f.cur_quality == ChordQuality::kMin);
}

// A SHIFT-staged manual chord is announced twice: once as a PENDING (next) at
// stage time, then again at the next bar boundary as the promoted current — the
// bar-promote carries the honest src of the producer that staged it (manual).
void test_bar_promote_uses_staging_producer() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);  // start playing, land inside the first bar
  b.clear();
  b.play(67, /*shift*/ 1);  // stage G7 for the next bar
  const Followed staged = b.last_followed();
  CHECK(staged.next_valid && staged.next_root == 7 && staged.next_quality == ChordQuality::kDom7);
  CHECK(staged.src == Producer::kManual);
  const int after_stage = b.followed_count();

  // Advance to the next bar boundary: the staged chord promotes into `current`.
  b.advance(kTicksPerBar);
  CHECK(b.followed_count() > after_stage);  // the promotion produced a new event
  const Followed promoted = b.last_followed();
  CHECK(promoted.cur_valid && promoted.cur_root == 7 &&
        promoted.cur_quality == ChordQuality::kDom7);
  CHECK(!promoted.next_valid);  // the pending slot was cleared
  CHECK(promoted.src == Producer::kManual);
}

}  // namespace

int main() {
  test_manual_immediate_announces_once();
  test_delta_suppresses_repeats();
  test_sequencer_announces();
  test_detect_announces();
  test_bar_promote_uses_staging_producer();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_followed_event: all OK\n");
  }
  return arrangrr::test::failures();
}

#include "arrangrr/arp/arpeggiator.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

struct Cap {
  std::uint8_t note;
  std::uint8_t vel;
  TickOffset gate;
};
using Caps = StaticVector<Cap, 64>;

void collect(ArpeggiatorEngine& a, Tick from, Tick to, Caps& out) {
  for (Tick t = from; t < to; ++t) {
    a.on_tick(t, [&](std::uint8_t n, std::uint8_t v, TickOffset g) {
      CHECK(out.push_back(Cap{.note = n, .vel = v, .gate = g}));
    });
  }
}

ArpeggiatorParams make(ArpDirection dir, std::uint8_t octaves = 1, std::uint8_t gate = 50) {
  ArpeggiatorParams p;
  p.rate = ArpRate::kSixteenth;  // 240 ticks/step
  p.direction = dir;
  p.octaves = octaves;
  p.gate = gate;
  return p;
}

void test_arp_up_and_gate() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 1, 50));
  a.note_on(60, 100);
  a.note_on(64, 90);
  a.note_on(67, 80);
  Caps c;
  collect(a, 0, 240 * 4, c);  // four 16th steps
  CHECK(c.size() == 4);
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 67 && c[3].note == 60);  // wraps
  CHECK(c[0].vel == 100 && c[1].vel == 90);
  CHECK(c[0].gate == 120);  // 240 * 50%
}

void test_arp_down() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kDown));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c;
  collect(a, 0, 240 * 3, c);
  CHECK(c[0].note == 67 && c[1].note == 64 && c[2].note == 60);
}

void test_arp_octaves() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 2));
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 5, c);
  // up over 2 octaves: 60 64 72 76 then wrap to 60
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 72 && c[3].note == 76 && c[4].note == 60);
}

void test_arp_updown() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUpDown));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c;
  collect(a, 0, 240 * 5, c);
  // 60 64 67 64 then back to 60 (period 4)
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 67 && c[3].note == 64 && c[4].note == 60);
}

void test_arp_as_played() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kAsPlayed));
  a.note_on(67, 100);  // played high first
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 3, c);
  CHECK(c[0].note == 67 && c[1].note == 60 && c[2].note == 64);  // insertion order preserved
}

void test_arp_only_on_grid() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(60, 100);
  Caps c;
  collect(a, 1, 240, c);  // between grid boundaries: nothing fires
  CHECK(c.size() == 0);
  collect(a, 240, 241, c);  // the next boundary fires once
  CHECK(c.size() == 1 && c[0].note == 60);
}

void test_arp_latch() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_off(60);
  a.note_off(64);
  CHECK(a.active());  // latched: still playing after release
  CHECK(a.held_count() == 2);
  a.note_on(62, 100);  // a fresh key starts a new chord
  CHECK(a.held_count() == 1);
  Caps c;
  collect(a, 0, 240, c);
  CHECK(c[0].note == 62);
}

void test_arp_no_latch_release_stops() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(60, 100);
  a.note_off(60);
  CHECK(!a.active());
  Caps c;
  collect(a, 0, 240 * 2, c);
  CHECK(c.size() == 0);
}

void test_arp_random_deterministic() {
  ArpeggiatorParams p = make(ArpDirection::kRandom);
  p.seed = 42;
  ArpeggiatorEngine a;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c1;
  collect(a, 0, 240 * 8, c1);

  ArpeggiatorEngine b;
  b.set_params(p);
  b.note_on(60, 100);
  b.note_on(64, 100);
  b.note_on(67, 100);
  Caps c2;
  collect(b, 0, 240 * 8, c2);

  CHECK(c1.size() == c2.size() && c1.size() == 8);
  bool same = true;
  bool any_off_root = false;
  for (std::size_t i = 0; i < c1.size(); ++i) {
    same = same && (c1[i].note == c2[i].note);
    any_off_root = any_off_root || (c1[i].note != 60);
  }
  CHECK(same);         // same seed -> identical sequence
  CHECK(any_off_root);  // and it actually varies the note
}

// Live keyboard arp through the full engine: held input notes are captured and
// replayed rhythmically on the arp's output route while the transport runs.
void test_arp_engine_live() {
  Engine e;
  StaticVector<OutEvent, 256> ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0 | (1 << 8), 0);  // out port 0, channel 2 (0-based 1)
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  auto feed = [&](std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    const std::uint8_t b[3] = {status, d1, d2};
    e.push_midi_in(0, Span<const std::uint8_t>(b, 3), sink);
  };
  feed(0x90, 60, 100);  // hold C E G on input port 0
  feed(0x90, 64, 100);
  feed(0x90, 67, 100);
  cmd(Param::kTransportStart, 0, 0);
  e.advance_ticks(480 * 4, sink);

  int arp_ons = 0;
  std::uint8_t first = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn && o.msg.channel() == 1) {
      if (arp_ons == 0) {
        first = o.msg.d1;
      }
      ++arp_ons;
    }
  }
  CHECK(arp_ons >= 3);  // rhythmic sequence, one note per 1/8
  CHECK(first == 60);   // up direction starts on the lowest held note
}

}  // namespace

int main() {
  test_arp_up_and_gate();
  test_arp_down();
  test_arp_octaves();
  test_arp_updown();
  test_arp_as_played();
  test_arp_only_on_grid();
  test_arp_latch();
  test_arp_no_latch_release_stops();
  test_arp_random_deterministic();
  test_arp_engine_live();
  return 0;
}

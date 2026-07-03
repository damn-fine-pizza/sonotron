#include "arrangrr/arranger/arranger.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
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
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0,
        Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0,
        Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
  }
  int ons(std::uint8_t channel, std::uint8_t note = 255) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind != OutEvent::Kind::kMidi || o.msg.type() != midi::kNoteOn) continue;
      if (o.msg.channel() != channel) continue;
      if (note != 255 && o.msg.d1 != note) continue;
      ++n;
    }
    return n;
  }
  StaticVector<std::uint16_t, 16> sections() const {
    StaticVector<std::uint16_t, 16> out;
    for (const OutEvent& o : ev)
      if (o.kind == OutEvent::Kind::kSection) CHECK(out.push_back(o.code));
    return out;
  }
};

void test_drums_play_without_chord_but_tonal_roles_wait() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  CHECK(b.ons(9) > 0);   // drums are fixed: they groove chord-less
  CHECK(b.ons(1) == 0);  // bass silent until a chord exists (NTT needs one)
  CHECK(b.ons(2) == 0);
}

void test_ntt_resolution_follows_chord() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // Cmaj7 in C major (I)
  b.ev.clear();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar / 2 - 1);  // stop short of step 8 (root repeats there)
  // VarA bass: root (36 + 0 = C2 = 36) at step 0, fifth (36+7=43) at step 4.
  CHECK(b.ons(1, 36) == 1);
  CHECK(b.ons(1, 43) == 1);
  // Chord comp at step 0: Cmaj7 stack anchored at 60: 60 64 67 71.
  CHECK(b.ons(2, 60) == 1 && b.ons(2, 64) == 1 && b.ons(2, 67) == 1 && b.ons(2, 71) == 1);
  // Change chord to Dm7 (ii) and check the next bar's downbeat.
  b.cmd(Param::kChordPlay, 62, -1, 100);
  b.ev.clear();
  b.advance(kTicksPerBar);
  CHECK(b.ons(1, 38) >= 1);  // bass root D2 = 38
  CHECK(b.ons(2, 62) >= 1 && b.ons(2, 65) >= 1);  // Dm7 comp: 62 65 69 72
}

void test_quantized_variation_switch() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.ev.clear();
  b.advance(kTicksPerBar);  // switch lands on the next bar boundary
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
}

void test_fill_one_shot_returns_to_variation() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kFillA));
  b.ev.clear();
  b.advance(2 * kTicksPerBar + 10);
  const auto sec = b.sections();
  CHECK(sec.size() == 2);
  CHECK(sec[0] == static_cast<std::uint16_t>(SectionType::kFillA));
  CHECK(sec[1] == static_cast<std::uint16_t>(SectionType::kVarA));  // returned
}

void test_intro_leads_to_variation() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kIntro1));
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(kTicksPerBar + 10);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarA));
}

void test_ending_stops_transport() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kEnding1));
  b.advance(3 * kTicksPerBar);
  CHECK(!b.e.transport().playing());
  bool saw_stop = false;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kTransport &&
        o.code == static_cast<std::uint16_t>(TransportState::kStopped))
      saw_stop = true;
  }
  CHECK(saw_stop);
}

void test_triad_wrap_and_route_gating() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  b.cmd(Param::kStyleLoad, 0);
  // Only bass routed: chord/drums silent.
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0,
        Op::kSet);
  // Sus4 triad (3 tones): VarB bass uses tone 3 -> wraps to root +1 octave.
  b.cmd(Param::kChordPlay, 60, static_cast<std::int8_t>(ChordQuality::kSus4), 100);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(kTicksPerBar - 1);
  CHECK(b.ons(9) == 0 && b.ons(2) == 0);  // unrouted roles gated off
  // 48 fires twice: step 10 is the root an octave up ({10,0,1}) and step 12
  // is tone 3 wrapping past the sus4 triad {0,5,7} -> 36 + 0 + 12 = 48.
  CHECK(b.ons(1, 48) == 2);
}

void test_style_warns() {
  Band b;
  b.cmd(Param::kStyleLoad, 7);  // no such builtin
  b.cmd(Param::kStyleSection, 2);  // no style loaded
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleSection, 99);  // bogus section id
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarC));  // absent
  b.cmd(Param::kStyleRoute, 99, 0, 0, Op::kSet);
  b.cmd(Param::kStyleRoute, 0, 9, 0, Op::kSet);  // bad port
  int warns = 0;
  for (const OutEvent& o : b.ev)
    if (o.kind == OutEvent::Kind::kWarn) ++warns;
  CHECK(warns == 6);
}

void test_immediate_switch_when_stopped() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

}  // namespace

int main() {
  test_drums_play_without_chord_but_tonal_roles_wait();
  test_ntt_resolution_follows_chord();
  test_quantized_variation_switch();
  test_fill_one_shot_returns_to_variation();
  test_intro_leads_to_variation();
  test_ending_stops_transport();
  test_triad_wrap_and_route_gating();
  test_style_warns();
  test_immediate_switch_when_stopped();
  if (arrangrr::test::failures() == 0) std::printf("test_arranger: all OK\n");
  return arrangrr::test::failures();
}

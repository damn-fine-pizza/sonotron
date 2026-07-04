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

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
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
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
  }
  int ons(std::uint8_t channel, std::uint8_t note = 255) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind != OutEvent::Kind::kMidi || o.msg.type() != midi::kNoteOn) {
        continue;
      }
      if (o.msg.channel() != channel) {
        continue;
      }
      if (note != 255 && o.msg.d1 != note) {
        continue;
      }
      ++n;
    }
    return n;
  }
  StaticVector<std::uint16_t, 16> sections() const {
    StaticVector<std::uint16_t, 16> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kSection) {
        CHECK(out.push_back(o.code));
      }
    }
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
  CHECK(b.ons(1, 38) >= 1);                       // bass root D2 = 38
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
        o.code == static_cast<std::uint16_t>(TransportState::kStopped)) {
      saw_stop = true;
    }
  }
  CHECK(saw_stop);
}

void test_triad_wrap_and_route_gating() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  b.cmd(Param::kStyleLoad, 0);
  // Only bass routed: chord/drums silent.
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
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
  b.cmd(Param::kStyleLoad, 7);     // no such builtin
  b.cmd(Param::kStyleSection, 2);  // no style loaded
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleSection, 99);                                             // bogus section id
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarC));  // absent
  b.cmd(Param::kStyleRoute, 99, 0, 0, Op::kSet);
  b.cmd(Param::kStyleRoute, 0, 9, 0, Op::kSet);  // bad port
  int warns = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kWarn) {
      ++warns;
    }
  }
  CHECK(warns == 6);
}

namespace twobar {
// A 2-bar section: the review found bars 2..N were unreachable because the
// section clock restarted at every bar boundary.
constexpr StyleEvent kDrums[] = {
    {0, 38, 0, 90, 120},    // bar 1, step 0
    {16, 36, 0, 100, 120},  // bar 2, step 16 — must actually play
};
constexpr StylePattern kPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kDrums)},
};
constexpr StyleSection kSections[] = {
    {SectionType::kVarA, 2, Span<const StylePattern>(kPatterns)},
};
constexpr Style kStyle{"twobar", Span<const StyleSection>(kSections)};
}  // namespace twobar

void test_multibar_section_plays_bar_two() {
  Arranger a;
  CHECK(a.load_style(&twobar::kStyle));
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  a.on_transport_start();
  const ChordState no_chord{};
  StaticVector<std::uint32_t, 8> hits36, hits38;
  for (Tick t = 0; t < 4 * kTicksPerBar; ++t) {
    a.on_tick(t, no_chord, [&](std::uint8_t, TickOffset delay, const MidiMessage& msg) {
      if (msg.type() != midi::kNoteOn || delay != 0) {
        return;
      }
      if (msg.d1 == 36) {
        CHECK(hits36.push_back(t));
      }
      if (msg.d1 == 38) {
        CHECK(hits38.push_back(t));
      }
    });
  }
  // Two full cycles of a 2-bar section over 4 bars.
  CHECK(hits38.size() == 2 && hits38[0] == 0 && hits38[1] == 2 * kTicksPerBar);
  CHECK(hits36.size() == 2);
  CHECK(hits36[0] == kTicksPerBar);      // bar 2 fires
  CHECK(hits36[1] == 3 * kTicksPerBar);  // and again on the wrap
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

void test_seamless_style_switch() {
  Arranger a;
  CHECK(a.load_style(&styles::basic::kStyle));
  a.on_transport_start();
  const ChordState no_chord{};
  auto sink = [](std::uint8_t, TickOffset, const MidiMessage&) {};

  // Queue a live switch to a different style (both define varA). It must land
  // TOGETHER on the next bar downbeat, never mid-bar.
  CHECK(a.request_style(&twobar::kStyle, SectionType::kVarA, false));

  bool changed_before_bar = false;
  bool changed_at_bar = false;
  for (Tick t = 0; t <= kTicksPerBar; ++t) {
    const Arranger::TickResult r = a.on_tick(t, no_chord, sink);
    if (r.style_changed) {
      if (t < kTicksPerBar) {
        changed_before_bar = true;
      } else {
        changed_at_bar = true;
      }
    }
  }
  CHECK(!changed_before_bar);  // seamless: not a mid-bar cut
  CHECK(changed_at_bar);       // applied on the downbeat

  // Immediate switch + section fallback: basic has no varC, so it lands varA.
  Arranger f;
  CHECK(f.load_style(&twobar::kStyle));
  CHECK(f.request_style(&styles::basic::kStyle, SectionType::kVarC, true));
  CHECK(f.current() == SectionType::kVarA);

  // A null style is refused.
  CHECK(!f.request_style(nullptr, SectionType::kVarA, true));
}

void test_style_switch_immediate_when_stopped() {
  Band b;
  b.setup_basic();  // basic is builtin 0, loaded on varA
  b.ev.clear();
  // Combined switch to (style 0, VarB): stopped -> immediate regardless of c.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0);
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

void test_style_switch_next_bar_when_playing() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.ev.clear();
  // c = 0 while playing -> quantized to the next bar, no immediate section event.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0);
  CHECK(b.sections().empty());  // nothing landed yet
  CHECK(b.e.arranger().current() == SectionType::kVarA);
  b.ev.clear();
  b.advance(kTicksPerBar);  // crosses the bar boundary
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
}

void test_style_switch_immediate_while_playing() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.ev.clear();
  // c != 0 while playing -> hard cut now.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 1);
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

void test_style_switch_bad_index_warns() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.cmd(Param::kStyleSwitch, 7, static_cast<std::int32_t>(SectionType::kVarA), 1);   // no style 7
  b.cmd(Param::kStyleSwitch, -1, static_cast<std::int32_t>(SectionType::kVarA), 1);  // negative
  b.cmd(Param::kStyleSwitch, 0, 99, 1);                                              // bad section
  int warns = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kWarn &&
        o.code == static_cast<std::uint16_t>(WarnCode::kBadArgument)) {
      ++warns;
    }
  }
  CHECK(warns == 3);
  // No crash, and the arranger stayed on its loaded section.
  CHECK(b.e.arranger().current() == SectionType::kVarA);
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
  test_multibar_section_plays_bar_two();
  test_immediate_switch_when_stopped();
  test_seamless_style_switch();
  test_style_switch_immediate_when_stopped();
  test_style_switch_next_bar_when_playing();
  test_style_switch_immediate_while_playing();
  test_style_switch_bad_index_warns();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_arranger: all OK\n");
  }
  return arrangrr::test::failures();
}

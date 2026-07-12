// Phase 3b (docs/design/orchestrator-pipeline-extraction.md §17.3b/§17.5):
// the kParamState wire encoding (param_state_wire.hpp) and the client-side
// mirror it feeds (param_state_mirror.hpp). Two things are proven here,
// entirely in-process, before any socket code exists (§17.5's sequencing):
//   1. Round-trip correctness of the wire encode/decode for all 9 Param
//      domains the echo carries, plus a couple of malformed-line rejections.
//   2. Parity: driving a live Shell through a script and folding its OWN
//      emitted kParamState events (encoded, then decoded back) into a
//      ParamStateMirror reproduces EXACTLY the same values the Shell's own
//      refresh_*_content() methods read straight off the live engine.

#include "param_state_mirror.hpp"
#include "param_state_wire.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"
#include "shell.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

void test_round_trip_groove() {
  const OutEvent ev = OutEvent::param_state(
      Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwing), 42, 0, 7);
  CHECK(param_state_to_jsonl(ev) ==
        R"({"ev":"param","param":"groove","sub":0,"v0":42,"v1":0,"@":7})");
  ParamStateWire w;
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(ev), w));
  CHECK(w.param == Param::kGroove);
  CHECK(w.sub == static_cast<std::uint8_t>(GrooveField::kSwing));
  CHECK(w.v0 == 42);
  CHECK(w.v1 == 0);
  CHECK(w.tick == 7);
}

void test_round_trip_arp() {
  const OutEvent ev =
      OutEvent::param_state(Param::kArp, static_cast<std::uint8_t>(ArpField::kGate), 60, 0, 100);
  ParamStateWire w;
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(ev), w));
  CHECK(w.param == Param::kArp);
  CHECK(w.sub == static_cast<std::uint8_t>(ArpField::kGate));
  CHECK(w.v0 == 60);
}

void test_round_trip_part_mute_solo() {
  const OutEvent mute =
      OutEvent::param_state(Param::kPartMute, static_cast<std::uint8_t>(TrackRole::kBass), 1, 0, 0);
  ParamStateWire w;
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(mute), w));
  CHECK(w.param == Param::kPartMute);
  CHECK(w.sub == static_cast<std::uint8_t>(TrackRole::kBass));
  CHECK(w.v0 == 1);

  const OutEvent solo =
      OutEvent::param_state(Param::kPartSolo, static_cast<std::uint8_t>(TrackRole::kPad), 1, 0, 0);
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(solo), w));
  CHECK(w.param == Param::kPartSolo);
  CHECK(w.sub == static_cast<std::uint8_t>(TrackRole::kPad));
}

void test_round_trip_style_load() {
  // 16-bit LE index: v0 = low byte, v1 = high byte.
  const OutEvent ev = OutEvent::param_state(Param::kStyleLoad, 0, 300 & 0xFF, (300 >> 8) & 0xFF, 0);
  ParamStateWire w;
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(ev), w));
  CHECK(w.param == Param::kStyleLoad);
  CHECK((static_cast<int>(w.v0) | (static_cast<int>(w.v1) << 8)) == 300);
}

void test_round_trip_chord_detect_follow_mode_key() {
  ParamStateWire w;

  const OutEvent detect = OutEvent::param_state(Param::kChordDetect, 2, 1, 0, 0);
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(detect), w));
  CHECK(w.param == Param::kChordDetect);
  CHECK(w.sub == 2 && w.v0 == 1);

  const OutEvent follow = OutEvent::param_state(
      Param::kChordFollow, 0, static_cast<std::uint8_t>(ChordFollow::kManual), 0, 0);
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(follow), w));
  CHECK(w.param == Param::kChordFollow);
  CHECK(w.v0 == static_cast<std::uint8_t>(ChordFollow::kManual));

  const OutEvent mode = OutEvent::param_state(Param::kChordMode, 0,
                                              static_cast<std::uint8_t>(ChordMode::kShell), 0, 0);
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(mode), w));
  CHECK(w.param == Param::kChordMode);
  CHECK(w.v0 == static_cast<std::uint8_t>(ChordMode::kShell));

  const OutEvent key =
      OutEvent::param_state(Param::kKeySet, 0, 7, static_cast<std::uint8_t>(Mode::kMixolydian), 0);
  CHECK(parse_param_state_jsonl(param_state_to_jsonl(key), w));
  CHECK(w.param == Param::kKeySet);
  CHECK(w.v0 == 7);
  CHECK(w.v1 == static_cast<std::uint8_t>(Mode::kMixolydian));
}

void test_non_param_state_kind_renders_empty() {
  CHECK(param_state_to_jsonl(OutEvent::transport(0, 0)).empty());
}

void test_malformed_lines_rejected() {
  ParamStateWire w;
  CHECK(!parse_param_state_jsonl("", w));
  CHECK(!parse_param_state_jsonl(R"({"ev":"transport","state":"playing","@":0})", w));
  CHECK(!parse_param_state_jsonl(
      R"({"ev":"param","param":"not-a-real-domain","sub":0,"v0":0,"v1":0,"@":0})", w));
}

// --- Parity: Shell's own live state vs. the same events round-tripped
// through the wire and folded into a ParamStateMirror. ---

struct Fixture {
  std::vector<OutEvent> events;
  Shell shell{[this](const OutEvent& ev) { events.push_back(ev); }};

  bool run(const std::string& line) {
    std::string error;
    const bool ok = shell.exec_line(line, error);
    CHECK(ok);
    return ok;
  }

  // Folds every captured kParamState event through the wire (encode then
  // decode) into `mirror` -- exactly what a real UDS client would do with
  // the lines a control-plane broadcast delivers.
  ParamStateMirror wire_mirror() const {
    ParamStateMirror mirror;
    for (const OutEvent& ev : events) {
      if (ev.kind != OutEvent::Kind::kParamState) {
        continue;
      }
      const std::string line = param_state_to_jsonl(ev);
      CHECK(!line.empty());
      ParamStateWire w;
      CHECK(parse_param_state_jsonl(line, w));
      mirror.apply(w);
    }
    return mirror;
  }
};

void test_parity_groove_and_arp() {
  Fixture f;
  f.run("groove swing 42");
  f.run("groove humanize-t 10");
  f.run("groove humanize-v 20");
  f.run("groove accent 30");
  f.run("groove grid 16");
  f.run("groove quantize 55");
  f.run("arp on");
  f.run("arp rate 1/8");
  f.run("arp dir down");
  f.run("arp octaves 2");
  f.run("arp gate 60");
  f.run("arp latch on");

  const ParamStateMirror mirror = f.wire_mirror();
  const GrooveParams& live_groove = f.shell.engine().arranger().groove_params();
  CHECK(mirror.groove.swing == live_groove.swing);
  CHECK(mirror.groove.humanize_timing == live_groove.humanize_timing);
  CHECK(mirror.groove.humanize_velocity == live_groove.humanize_velocity);
  CHECK(mirror.groove.accent == live_groove.accent);
  CHECK(mirror.groove.swing_grid == live_groove.swing_grid);
  CHECK(mirror.groove.quantize == live_groove.quantize);

  const ArpeggiatorParams& live_arp = f.shell.engine().arp().params();
  CHECK(mirror.arp_enabled == f.shell.engine().arp_enabled());
  CHECK(mirror.arp.rate == static_cast<std::uint8_t>(live_arp.rate));
  CHECK(mirror.arp.direction == static_cast<std::uint8_t>(live_arp.direction));
  CHECK(mirror.arp.octaves == live_arp.octaves);
  CHECK(mirror.arp.gate == live_arp.gate);
  CHECK(mirror.arp.latch == live_arp.latch);
}

void test_parity_parts_mixer() {
  Fixture f;
  f.run("part bass mute on");
  f.run("part pad solo on");

  const ParamStateMirror mirror = f.wire_mirror();
  const auto bass_index = static_cast<std::size_t>(TrackRole::kBass);
  const auto pad_index = static_cast<std::size_t>(TrackRole::kPad);
  CHECK(mirror.part_muted[bass_index] ==
        f.shell.engine().arranger().part_info(TrackRole::kBass).muted);
  CHECK(mirror.part_soloed[pad_index] ==
        f.shell.engine().arranger().part_info(TrackRole::kPad).soloed);
}

void test_parity_style_chord_key() {
  Fixture f;
  f.run("style load basic");
  f.run("chord detect on");
  f.run("chord follow manual");
  f.run("chord mode shell");
  f.run("key G mixolydian");

  const ParamStateMirror mirror = f.wire_mirror();
  CHECK(mirror.style_index == 0);  // "basic" == styles::kBuiltins[0]
  CHECK(mirror.chord_detect == f.shell.engine().chord_detect());
  CHECK(mirror.chord_follow == static_cast<std::uint8_t>(f.shell.engine().chord_follow()));
  CHECK(mirror.chord_mode == static_cast<std::uint8_t>(f.shell.engine().chords().mode()));
  CHECK(mirror.key_root_pc == f.shell.engine().chords().key().root_pc);
  CHECK(mirror.key_mode == static_cast<std::uint8_t>(f.shell.engine().chords().key().mode));
}

}  // namespace

int main() {
  test_round_trip_groove();
  test_round_trip_arp();
  test_round_trip_part_mute_solo();
  test_round_trip_style_load();
  test_round_trip_chord_detect_follow_mode_key();
  test_non_param_state_kind_renders_empty();
  test_malformed_lines_rejected();
  test_parity_groove_and_arp();
  test_parity_parts_mixer();
  test_parity_style_chord_key();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_param_state_wire: all OK\n");
  }
  return arrangrr::test::failures();
}

// docs/design/orchestrator-pipeline-extraction.md §17.1/§17.3a/§17.5 Phase
// 3b: TuiClient (the client-side presentation half) folds inbound wire
// lines into the core-free mirror, and its gesture translators build the
// exact L1 verb the existing TUI's cmd_* grammar already accepts. This test
// builds wire lines the same way the real server does (OutEvent::param_
// state() -> param_state_to_jsonl(), the identical helpers test_param_state
// _wire.cpp already proves round-trip correctly) and drives them through
// TuiClient -- but TuiClient itself, and the file under test, never touch
// an arrangrr core header (client_event.hpp/tui_client.hpp are arrangrr-
// free by construction).

#include "tui_client.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"   // test-only: builds realistic wire text, mirrors
#include "param_state_wire.hpp"  // test_param_state_wire.cpp's own precedent
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

struct ClientFixture {
  std::vector<std::string> sent;
  TuiClient client{[this](const std::string& line) { sent.push_back(line); }};

  void feed(const OutEvent& ev) { client.on_wire_line(param_state_to_jsonl(ev)); }
};

void test_groove_fold_and_last_change() {
  ClientFixture f;
  f.feed(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwing), 42,
                               0, 7));
  CHECK(f.client.params().groove.swing == 42);
  CHECK(f.client.last_param_change() == "groove.swing=42");

  f.feed(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwingGrid),
                               16, 0, 8));
  CHECK(f.client.params().groove.swing_grid == 16);
  CHECK(f.client.last_param_change() == "groove.grid=16");
}

void test_arp_fold() {
  ClientFixture f;
  f.feed(
      OutEvent::param_state(Param::kArp, static_cast<std::uint8_t>(ArpField::kEnabled), 1, 0, 0));
  CHECK(f.client.params().arp_enabled);
  CHECK(f.client.last_param_change() == "arp.enabled=1");

  f.feed(OutEvent::param_state(Param::kArp, static_cast<std::uint8_t>(ArpField::kRate), 2, 0, 0));
  CHECK(f.client.params().arp.rate == 2);
  CHECK(f.client.last_param_change() == "arp.rate=1/16");
}

void test_part_mute_solo_fold() {
  ClientFixture f;
  f.feed(OutEvent::param_state(Param::kPartMute, static_cast<std::uint8_t>(TrackRole::kBass), 1, 0,
                               0));
  CHECK(f.client.params().part_muted[static_cast<std::size_t>(TrackRole::kBass)]);
  CHECK(f.client.last_param_change() == "part-mute.bass=on");

  f.feed(OutEvent::param_state(Param::kPartSolo, static_cast<std::uint8_t>(TrackRole::kDrums), 1, 0,
                               0));
  CHECK(f.client.params().part_soloed[static_cast<std::size_t>(TrackRole::kDrums)]);
  CHECK(f.client.last_param_change() == "part-solo.drums=on");
}

void test_key_and_chord_fold() {
  ClientFixture f;
  f.feed(OutEvent::param_state(Param::kKeySet, 0, 2, static_cast<std::uint8_t>(Mode::kDorian), 0));
  CHECK(f.client.params().key_root_pc == 2);
  CHECK(f.client.params().key_mode == static_cast<std::uint8_t>(Mode::kDorian));

  f.feed(OutEvent::param_state(Param::kChordDetect, 1, 1, 0, 0));
  CHECK(f.client.params().chord_detect);
  CHECK(f.client.params().chord_detect_port == 1);
}

// A malformed / unrecognized line must never throw and must leave the
// mirror untouched (additive-only ABI growth: a future field a client
// build does not know yet is ignored, not fatal).
void test_unknown_line_is_ignored() {
  ClientFixture f;
  f.client.on_wire_line("not json at all");
  f.client.on_wire_line(R"({"ev":"beat","bar":1,"beat":1,"pulse":0,"@":0})");
  CHECK(f.client.last_param_change().empty());
}

void test_send_note() {
  ClientFixture f;
  f.client.send_note("in0", 0, 60, 100, true);
  CHECK(f.sent.size() == 1);
  CHECK(f.sent[0] == "note in0 on 60 100");

  f.client.send_note("in0", 2, 60, 64, false);
  CHECK(f.sent.size() == 2);
  CHECK(f.sent[1] == "note in0:2 off 60 64");
}

void test_groove_adjust_sends_absolute_value() {
  ClientFixture f;
  // Starts at the GrooveViewParams default (swing = 0); +1 step = +10.
  f.client.groove_adjust(0, +1);
  CHECK(f.sent.size() == 1);
  CHECK(f.sent[0] == "groove swing 10");

  // Fold the server's own echo of that mutation back in (as the real round
  // trip would), then adjust again from the new current value.
  f.feed(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwing), 10,
                               0, 1));
  f.client.groove_adjust(0, +1);
  CHECK(f.sent.size() == 2);
  CHECK(f.sent[1] == "groove swing 20");

  // grid (row 4) toggles between 8 and 16 regardless of delta sign.
  f.client.groove_adjust(4, -1);
  CHECK(f.sent.back() == "groove grid 16");
}

void test_arp_adjust_sends_matching_verb_shape() {
  ClientFixture f;
  f.client.arp_adjust(0, +1);  // enabled row: toggles on/off as a bare verb
  CHECK(f.sent.back() == "arp on");

  f.client.arp_adjust(1, +1);  // rate row: default rate index 2 ("1/16") + 1
  CHECK(f.sent.back() == "arp rate 1/32");

  f.client.arp_adjust(2, +1);  // direction row: default 0 ("up") + 1
  CHECK(f.sent.back() == "arp dir down");

  f.client.arp_adjust(3, +1);  // octaves row: default 1 + 1
  CHECK(f.sent.back() == "arp octaves 2");

  f.client.arp_adjust(4, +1);  // gate row: default 75 + 10, clamped at 100
  CHECK(f.sent.back() == "arp gate 85");

  f.client.arp_adjust(5, 0);  // latch row: toggles regardless of delta
  CHECK(f.sent.back() == "arp latch on");
}

void test_part_toggle_sends_flipped_value() {
  ClientFixture f;
  f.client.part_toggle_mute(2);  // role index 2 = bass
  CHECK(f.sent.back() == "part bass mute on");

  f.feed(OutEvent::param_state(Param::kPartMute, static_cast<std::uint8_t>(TrackRole::kBass), 1, 0,
                               0));
  f.client.part_toggle_mute(2);
  CHECK(f.sent.back() == "part bass mute off");

  f.client.part_toggle_solo(0);  // role index 0 = drums
  CHECK(f.sent.back() == "part drums solo on");
}

}  // namespace

int main() {
  test_groove_fold_and_last_change();
  test_arp_fold();
  test_part_mute_solo_fold();
  test_key_and_chord_fold();
  test_unknown_line_is_ignored();
  test_send_note();
  test_groove_adjust_sends_absolute_value();
  test_arp_adjust_sends_matching_verb_shape();
  test_part_toggle_sends_flipped_value();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_tui_client: all OK\n");
  }
  return arrangrr::test::failures();
}

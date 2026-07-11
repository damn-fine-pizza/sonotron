// Unit tests for PartsModel: the Parts/Mixer per-part mute/solo state
// (ux-workstation.md §4.6). No GPU, no display, no core -- muted/soloed are
// local optimistic hints, not a readback (§11.4 gap), which is what these
// tests pin.

#include "src/parts_model.hpp"

#include "test.hpp"

using sonotron::PartsModel;

namespace {

void test_defaults_all_unmuted_unsoloed_unknown_gm() {
  PartsModel model;
  CHECK(PartsModel::kPartCount == 9);
  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    CHECK(!model.part(i).muted);
    CHECK(!model.part(i).soloed);
    CHECK(model.part(i).gm_program == -1);
  }
}

void test_labels_and_wire_tokens() {
  PartsModel model;
  CHECK(model.part_label(0) == "Drums");
  CHECK(model.part_wire_token(0) == "drums");
  CHECK(model.part_label(3) == "Chord1");
  CHECK(model.part_wire_token(3) == "chord1");
  CHECK(model.part_label(8) == "Lead");
  CHECK(model.part_wire_token(8) == "lead");
}

void test_toggle_mute_and_solo_are_independent_per_part() {
  PartsModel model;
  model.toggle_mute(2);
  CHECK(model.part(2).muted);
  CHECK(!model.part(2).soloed);
  CHECK(!model.part(0).muted);  // neighbour untouched

  model.toggle_solo(2);
  CHECK(model.part(2).soloed);

  model.toggle_mute(2);
  CHECK(!model.part(2).muted);
  CHECK(model.part(2).soloed);  // solo unaffected by the mute toggle
}

}  // namespace

int main() {
  test_defaults_all_unmuted_unsoloed_unknown_gm();
  test_labels_and_wire_tokens();
  test_toggle_mute_and_solo_are_independent_per_part();
  return sonotron::test::failures();
}

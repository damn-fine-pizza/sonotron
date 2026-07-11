// Unit tests for SeqEditModel: the Sequence Edit toolbar/selection state
// (ux-workstation.md §4.5/§6). No GPU, no display, no core.

#include "src/seqedit_model.hpp"

#include "test.hpp"

using sonotron::SeqEditModel;
using sonotron::SeqEditView;

namespace {

void test_defaults() {
  SeqEditModel model;
  CHECK(model.part_index() == 0);
  CHECK(model.part_label() == "Drums");
  CHECK(model.clip_label() == "-");
  CHECK(!model.record_armed());
  CHECK(model.grid_division() == 16);
  CHECK(model.view() == SeqEditView::kPianoRoll);
}

void test_set_part_index_clamps() {
  SeqEditModel model;
  model.set_part_index(2);
  CHECK(model.part_index() == 2);
  CHECK(model.part_label() == "Bass");

  model.set_part_index(999);  // out of range: clamps to the last role
  CHECK(model.part_index() == 8);
  CHECK(model.part_label() == "Lead");
}

void test_clip_label_and_record_arm() {
  SeqEditModel model;
  model.set_clip_label("wlk");
  CHECK(model.clip_label() == "wlk");

  model.set_record_armed(true);
  CHECK(model.record_armed());
  model.set_record_armed(false);
  CHECK(!model.record_armed());
}

void test_grid_division_and_view() {
  SeqEditModel model;
  model.set_grid_division(8);
  CHECK(model.grid_division() == 8);

  model.set_view(SeqEditView::kStep);
  CHECK(model.view() == SeqEditView::kStep);
  model.set_view(SeqEditView::kPianoRoll);
  CHECK(model.view() == SeqEditView::kPianoRoll);
}

}  // namespace

int main() {
  test_defaults();
  test_set_part_index_clamps();
  test_clip_label_and_record_arm();
  test_grid_division_and_view();
  return sonotron::test::failures();
}

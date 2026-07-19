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
  CHECK(model.role_visible(0));
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

void test_role_visibility() {
  SeqEditModel model;

  // Default: all 9 roles visible
  for (std::size_t i = 0; i < sonotron::kTrackRoleCount; ++i) {
    CHECK(model.role_visible(i));
  }

  // Toggle a single role off
  model.set_role_visible(3, false);
  CHECK(!model.role_visible(3));
  // Confirm a different role is untouched
  CHECK(model.role_visible(4));

  // Toggle back on
  model.set_role_visible(3, true);
  CHECK(model.role_visible(3));

  // Out-of-range read returns true (safe default)
  CHECK(model.role_visible(999));

  // Out-of-range write is a silent no-op: confirm all in-range roles still visible
  model.set_role_visible(999, false);
  for (std::size_t i = 0; i < sonotron::kTrackRoleCount; ++i) {
    CHECK(model.role_visible(i));
  }
}

void test_all_tracks_visible_bulk_action() {
  SeqEditModel model;

  // Default: everything visible, and the model reports that state.
  CHECK(model.all_tracks_shown());

  // Soloing role 3: only role 3 stays visible, every other role hidden.
  model.set_part_index(3);
  model.set_all_tracks_visible(false);
  CHECK(!model.all_tracks_shown());
  CHECK(model.role_visible(3));
  CHECK(!model.role_visible(0));
  CHECK(!model.role_visible(4));
  CHECK(!model.role_visible(8));

  // Flipping back to "all tracks": every role visible again.
  model.set_all_tracks_visible(true);
  CHECK(model.all_tracks_shown());
  for (std::size_t i = 0; i < sonotron::kTrackRoleCount; ++i) {
    CHECK(model.role_visible(i));
  }
}

}  // namespace

int main() {
  test_defaults();
  test_set_part_index_clamps();
  test_clip_label_and_record_arm();
  test_grid_division_and_view();
  test_role_visibility();
  test_all_tracks_visible_bulk_action();
  return sonotron::test::failures();
}

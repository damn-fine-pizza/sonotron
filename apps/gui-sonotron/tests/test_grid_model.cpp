// Unit tests for GridModel: the Repeat Zone matrix/scene shape
// (ux-workstation.md §4.4/§5). No GPU, no display, no core -- real LAUNCH is
// wired to the core clip primitive now (kGridLaunchWired == true, Phase-5
// Item #2); this test pins that constant rather than a fake launch result.

#include "src/grid_model.hpp"

#include "test.hpp"

using sonotron::GridCellKind;
using sonotron::GridModel;

namespace {

void test_default_shape() {
  GridModel grid;
  CHECK(grid.part_count() == 9);
  CHECK(grid.scene_count() == GridModel::kDefaultSceneCount);
  CHECK(grid.part_label(0) == "Drums");
  CHECK(grid.part_label(2) == "Bass");
  CHECK(grid.part_label(8) == "Lead");
}

void test_cells_start_empty() {
  GridModel grid;
  for (std::size_t part = 0; part < grid.part_count(); ++part) {
    for (std::size_t scene = 0; scene < grid.scene_count(); ++scene) {
      CHECK(grid.cell(part, scene).kind == GridCellKind::kEmpty);
      CHECK(grid.cell(part, scene).label.empty());
    }
  }
}

void test_set_and_clear_cell() {
  GridModel grid;
  grid.set_cell(2, 1, GridCellKind::kStyleSection, "funk");
  CHECK(grid.cell(2, 1).kind == GridCellKind::kStyleSection);
  CHECK(grid.cell(2, 1).label == "funk");
  // A neighbour is untouched.
  CHECK(grid.cell(2, 0).kind == GridCellKind::kEmpty);
  CHECK(grid.cell(1, 1).kind == GridCellKind::kEmpty);

  grid.clear_cell(2, 1);
  CHECK(grid.cell(2, 1).kind == GridCellKind::kEmpty);
  CHECK(grid.cell(2, 1).label.empty());
}

void test_add_scene_preserves_existing_cells_and_grows_shape() {
  GridModel grid;
  const std::size_t before = grid.scene_count();
  grid.set_cell(0, 0, GridCellKind::kChordSequence, "verse-prog");
  grid.set_cell(3, before - 1, GridCellKind::kStepTrack, "wlk");

  grid.add_scene();
  CHECK(grid.scene_count() == before + 1);
  // Existing content survived the reindex.
  CHECK(grid.cell(0, 0).kind == GridCellKind::kChordSequence);
  CHECK(grid.cell(0, 0).label == "verse-prog");
  CHECK(grid.cell(3, before - 1).kind == GridCellKind::kStepTrack);
  CHECK(grid.cell(3, before - 1).label == "wlk");
  // The new column starts empty.
  CHECK(grid.cell(0, before).kind == GridCellKind::kEmpty);
}

void test_add_scene_caps_at_max() {
  GridModel grid;
  for (std::size_t i = grid.scene_count(); i < GridModel::kMaxSceneCount + 3; ++i) {
    grid.add_scene();
  }
  CHECK(grid.scene_count() == GridModel::kMaxSceneCount);
}

void test_scene_names_default_to_bare_numbers() {
  GridModel grid;
  CHECK(grid.scene_name(0) == "1");
  CHECK(grid.scene_name(2) == "3");
  // Every index up to kMaxSceneCount is valid even before that many scene
  // columns exist -- the default 3-scene grid still answers for index 7.
  CHECK(grid.scene_name(GridModel::kMaxSceneCount - 1) ==
        std::to_string(GridModel::kMaxSceneCount));
}

void test_set_scene_name_and_bounds() {
  GridModel grid;
  grid.set_scene_name(1, "Chorus");
  CHECK(grid.scene_name(1) == "Chorus");
  // A neighbour is untouched.
  CHECK(grid.scene_name(0) == "1");
  CHECK(grid.scene_name(2) == "3");

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_name(GridModel::kMaxSceneCount, "should not stick");
  grid.set_scene_name(GridModel::kMaxSceneCount + 10, "should not stick either");

  // Out-of-range get returns an empty view, not garbage.
  CHECK(grid.scene_name(GridModel::kMaxSceneCount).empty());
  CHECK(grid.scene_name(GridModel::kMaxSceneCount + 10).empty());
}

void test_add_scene_preserves_scene_names() {
  GridModel grid;
  grid.set_scene_name(0, "Intro");
  const std::size_t before = grid.scene_count();
  grid.add_scene();
  // Renaming is decoupled from the cell-growth reindex -- add_scene() only
  // touches m_cells, never m_scene_names.
  CHECK(grid.scene_name(0) == "Intro");
  CHECK(grid.scene_count() == before + 1);
}

void test_launch_wired_is_lit() {
  // Pinned true: the core clip primitive (Phase-5 Item #2, docs/design/
  // clip-primitive-design.md) shipped -- launch/stop/scene-quantize are
  // real L1 verbs and the wire carries a real `clip` event.
  CHECK(sonotron::kGridLaunchWired == true);
}

}  // namespace

int main() {
  test_default_shape();
  test_cells_start_empty();
  test_set_and_clear_cell();
  test_add_scene_preserves_existing_cells_and_grows_shape();
  test_add_scene_caps_at_max();
  test_scene_names_default_to_bare_numbers();
  test_set_scene_name_and_bounds();
  test_add_scene_preserves_scene_names();
  test_launch_wired_is_lit();
  return sonotron::test::failures();
}

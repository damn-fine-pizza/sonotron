// Unit tests for GridModel: the Repeat Zone matrix/scene shape
// (ux-workstation.md §4.4/§5). No GPU, no display, no core -- real LAUNCH is
// an honest placeholder (kGridLaunchWired == false) until the core clip
// primitive lands; this test pins that constant rather than a fake launch
// result.

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

void test_launch_wired_is_honest_placeholder() {
  // Pinned false: flipping this to true is the exact seam the core clip
  // primitive (ux-workstation.md §11.3) lights up when it lands.
  CHECK(sonotron::kGridLaunchWired == false);
}

}  // namespace

int main() {
  test_default_shape();
  test_cells_start_empty();
  test_set_and_clear_cell();
  test_add_scene_preserves_existing_cells_and_grows_shape();
  test_add_scene_caps_at_max();
  test_launch_wired_is_honest_placeholder();
  return sonotron::test::failures();
}

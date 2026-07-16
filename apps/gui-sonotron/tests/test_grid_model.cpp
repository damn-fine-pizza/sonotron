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

// SLICE 4a (docs/proposals/repeat-zone-real-contract.md): every scene column
// defaults to kDefaultSectionType (SectionType::kVarA == 2).
void test_scene_sections_default_to_var_a() {
  GridModel grid;
  CHECK(grid.scene_section(0) == GridModel::kDefaultSectionType);
  CHECK(grid.scene_section(2) == GridModel::kDefaultSectionType);
  CHECK(grid.scene_section(GridModel::kMaxSceneCount - 1) == GridModel::kDefaultSectionType);
}

void test_set_scene_section_and_bounds() {
  GridModel grid;
  grid.set_scene_section(1, 3);  // kVarB
  CHECK(grid.scene_section(1) == 3);
  // A neighbour is untouched.
  CHECK(grid.scene_section(0) == GridModel::kDefaultSectionType);
  CHECK(grid.scene_section(2) == GridModel::kDefaultSectionType);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_section(GridModel::kMaxSceneCount, 5);
  grid.set_scene_section(GridModel::kMaxSceneCount + 10, 5);

  // Out-of-range get returns the default, not garbage.
  CHECK(grid.scene_section(GridModel::kMaxSceneCount) == GridModel::kDefaultSectionType);
  CHECK(grid.scene_section(GridModel::kMaxSceneCount + 10) == GridModel::kDefaultSectionType);
}

void test_add_scene_preserves_scene_sections() {
  GridModel grid;
  grid.set_scene_section(0, 10);  // kBreak
  const std::size_t before = grid.scene_count();
  grid.add_scene();
  // Section storage is decoupled from the cell-growth reindex, exactly like
  // scene names -- add_scene() only touches m_cells.
  CHECK(grid.scene_section(0) == 10);
  CHECK(grid.scene_count() == before + 1);
}

// section_wire_name mirrors in_process_brain_session.cpp's own
// parse_section_name spellings exactly (both directions of the same table).
void test_section_wire_name_matches_known_spellings() {
  CHECK(sonotron::section_wire_name(0) == "intro1");
  CHECK(sonotron::section_wire_name(2) == "varA");
  CHECK(sonotron::section_wire_name(3) == "varB");
  CHECK(sonotron::section_wire_name(10) == "break");
  CHECK(sonotron::section_wire_name(12) == "ending2");
  // Out of range: an empty view, not garbage.
  CHECK(sonotron::section_wire_name(13).empty());
  CHECK(sonotron::section_wire_name(255).empty());
}

void test_launch_wired_is_lit() {
  // Pinned true: the core clip primitive (Phase-5 Item #2, docs/design/
  // clip-primitive-design.md) shipped -- launch/stop/scene-quantize are
  // real L1 verbs and the wire carries a real `clip` event.
  CHECK(sonotron::kGridLaunchWired == true);
}

// SLICE 4b (docs/proposals/repeat-zone-real-contract.md): next_scene_to_launch
// is the pure auto-song advance decision, GUI-driven, no core involvement.
using sonotron::next_scene_to_launch;

void test_next_scene_auto_song_off_stays() {
  // auto_song OFF: nullopt regardless of how far past the section boundary.
  CHECK(!next_scene_to_launch(/*auto_song=*/false, /*playing=*/true, /*active_scene=*/0,
                              /*scene_count=*/5, /*bars_elapsed_in_scene=*/10,
                              /*active_scene_section_bars=*/1)
             .has_value());
}

void test_next_scene_not_playing_stays() {
  // Transport not playing: nullopt even with auto_song ON and past boundary.
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/false, /*active_scene=*/0,
                              /*scene_count=*/5, /*bars_elapsed_in_scene=*/10,
                              /*active_scene_section_bars=*/1)
             .has_value());
}

void test_next_scene_mid_scene_stays() {
  // On and playing, but the active section hasn't finished yet.
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/2,
                              /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                              /*active_scene_section_bars=*/4)
             .has_value());
}

void test_next_scene_advances_at_boundary() {
  // bars_elapsed_in_scene == active_scene_section_bars EXACTLY: advances.
  const auto next = next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/1,
                                         /*scene_count=*/5, /*bars_elapsed_in_scene=*/4,
                                         /*active_scene_section_bars=*/4);
  CHECK(next.has_value());
  CHECK(*next == 2);
}

void test_next_scene_advances_past_boundary() {
  // bars_elapsed_in_scene PAST the boundary (a missed poll/late frame):
  // still advances -- ">=", not "==".
  const auto next = next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/1,
                                         /*scene_count=*/5, /*bars_elapsed_in_scene=*/9,
                                         /*active_scene_section_bars=*/4);
  CHECK(next.has_value());
  CHECK(*next == 2);
}

void test_next_scene_wraps_at_last_scene() {
  // The song loops around the scene sequence: the last scene column (index
  // scene_count - 1) advances back to scene 0, not out of range.
  const auto next = next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/4,
                                         /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                                         /*active_scene_section_bars=*/1);
  CHECK(next.has_value());
  CHECK(*next == 0);
}

void test_next_scene_zero_scene_count_stays() {
  // Defensive: no scene to wrap into.
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/0,
                              /*scene_count=*/0, /*bars_elapsed_in_scene=*/5,
                              /*active_scene_section_bars=*/1)
             .has_value());
}

// bar_just_advanced: the once-per-crossing guard.
using sonotron::bar_just_advanced;

void test_bar_just_advanced_fires_once_per_distinct_bar() {
  int last = -1;
  CHECK(bar_just_advanced(0, last));  // first ever call at bar 0: fires
  CHECK(last == 0);
  CHECK(!bar_just_advanced(0, last));  // same bar again (next frame): does not fire
  CHECK(!bar_just_advanced(0, last));  // and again: still does not fire
  CHECK(bar_just_advanced(1, last));   // the bar actually changed: fires
  CHECK(last == 1);
  CHECK(!bar_just_advanced(1, last));  // settles again until the next change
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
  test_scene_sections_default_to_var_a();
  test_set_scene_section_and_bounds();
  test_add_scene_preserves_scene_sections();
  test_section_wire_name_matches_known_spellings();
  test_launch_wired_is_lit();
  test_next_scene_auto_song_off_stays();
  test_next_scene_not_playing_stays();
  test_next_scene_mid_scene_stays();
  test_next_scene_advances_at_boundary();
  test_next_scene_advances_past_boundary();
  test_next_scene_wraps_at_last_scene();
  test_next_scene_zero_scene_count_stays();
  test_bar_just_advanced_fires_once_per_distinct_bar();
  return sonotron::test::failures();
}

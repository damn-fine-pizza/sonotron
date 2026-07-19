// Unit tests for GridModel: the Repeat Zone matrix/scene shape
// (ux-workstation.md §4.4/§5). No GPU, no display, no core -- real LAUNCH is
// wired to the core clip primitive now (kGridLaunchWired == true, Phase-5
// Item #2); this test pins that constant rather than a fake launch result.

#include "src/grid_model.hpp"

#include <cmath>

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

// Task #6: scene_bars/set_scene_bars is the always-visible per-scene LENGTH
// stepper's own model storage -- every scene defaults to kDefaultSceneBars,
// bounds-checked exactly like scene_name/scene_section above, and the setter
// clamps to [1, kMaxSceneBars] (a sensible stepper ceiling, not a hard engine
// limit).
void test_scene_bars_defaults_to_default_scene_bars() {
  GridModel grid;
  CHECK(grid.scene_bars(0) == GridModel::kDefaultSceneBars);
  CHECK(grid.scene_bars(2) == GridModel::kDefaultSceneBars);
  CHECK(grid.scene_bars(GridModel::kMaxSceneCount - 1) == GridModel::kDefaultSceneBars);
}

void test_set_scene_bars_and_bounds() {
  GridModel grid;
  grid.set_scene_bars(1, 3);
  CHECK(grid.scene_bars(1) == 3);
  // A neighbour is untouched.
  CHECK(grid.scene_bars(0) == GridModel::kDefaultSceneBars);
  CHECK(grid.scene_bars(2) == GridModel::kDefaultSceneBars);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_bars(GridModel::kMaxSceneCount, 3);
  grid.set_scene_bars(GridModel::kMaxSceneCount + 10, 3);

  // Out-of-range get returns the default, not garbage.
  CHECK(grid.scene_bars(GridModel::kMaxSceneCount) == GridModel::kDefaultSceneBars);
  CHECK(grid.scene_bars(GridModel::kMaxSceneCount + 10) == GridModel::kDefaultSceneBars);
}

void test_set_scene_bars_clamps_floor_at_one() {
  GridModel grid;
  grid.set_scene_bars(0, 0);
  CHECK(grid.scene_bars(0) == 1);
  grid.set_scene_bars(0, -5);
  CHECK(grid.scene_bars(0) == 1);
}

void test_set_scene_bars_clamps_ceiling_at_max() {
  GridModel grid;
  grid.set_scene_bars(0, GridModel::kMaxSceneBars + 1);
  CHECK(grid.scene_bars(0) == GridModel::kMaxSceneBars);
  grid.set_scene_bars(0, 1000);
  CHECK(grid.scene_bars(0) == GridModel::kMaxSceneBars);
}

void test_add_scene_preserves_scene_bars() {
  GridModel grid;
  grid.set_scene_bars(0, 4);
  const std::size_t before = grid.scene_count();
  grid.add_scene();
  // Storage is decoupled from the cell-growth reindex, exactly like
  // scene_name/scene_section above -- add_scene() only touches m_cells.
  CHECK(grid.scene_bars(0) == 4);
  CHECK(grid.scene_count() == before + 1);
}

// Task #5: scene_repeat/set_scene_repeat is the always-visible per-scene
// REPEAT-COUNT stepper's own model storage -- every scene defaults to
// kDefaultSceneRepeat (1, "play once"), bounds-checked exactly like
// scene_bars above, and the setter clamps to [1, kSceneRepeatInfinite] (one
// past kMaxSceneRepeat, the "hold forever" sentinel).
void test_scene_repeat_defaults_to_default_scene_repeat() {
  GridModel grid;
  CHECK(grid.scene_repeat(0) == GridModel::kDefaultSceneRepeat);
  CHECK(grid.scene_repeat(2) == GridModel::kDefaultSceneRepeat);
  CHECK(grid.scene_repeat(GridModel::kMaxSceneCount - 1) == GridModel::kDefaultSceneRepeat);
}

void test_set_scene_repeat_and_bounds() {
  GridModel grid;
  grid.set_scene_repeat(1, 3);
  CHECK(grid.scene_repeat(1) == 3);
  // A neighbour is untouched.
  CHECK(grid.scene_repeat(0) == GridModel::kDefaultSceneRepeat);
  CHECK(grid.scene_repeat(2) == GridModel::kDefaultSceneRepeat);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_repeat(GridModel::kMaxSceneCount, 3);
  grid.set_scene_repeat(GridModel::kMaxSceneCount + 10, 3);

  // Out-of-range get returns the default, not garbage.
  CHECK(grid.scene_repeat(GridModel::kMaxSceneCount) == GridModel::kDefaultSceneRepeat);
  CHECK(grid.scene_repeat(GridModel::kMaxSceneCount + 10) == GridModel::kDefaultSceneRepeat);
}

void test_set_scene_repeat_clamps_floor_at_one() {
  GridModel grid;
  grid.set_scene_repeat(0, 0);
  CHECK(grid.scene_repeat(0) == 1);
  grid.set_scene_repeat(0, -5);
  CHECK(grid.scene_repeat(0) == 1);
}

void test_set_scene_repeat_clamps_ceiling_at_infinite_sentinel() {
  GridModel grid;
  grid.set_scene_repeat(0, GridModel::kSceneRepeatInfinite + 1);
  CHECK(grid.scene_repeat(0) == GridModel::kSceneRepeatInfinite);
  grid.set_scene_repeat(0, 1000);
  CHECK(grid.scene_repeat(0) == GridModel::kSceneRepeatInfinite);
}

void test_add_scene_preserves_scene_repeat() {
  GridModel grid;
  grid.set_scene_repeat(0, 4);
  const std::size_t before = grid.scene_count();
  grid.add_scene();
  // Storage is decoupled from the cell-growth reindex, exactly like
  // scene_bars/scene_name/scene_section above -- add_scene() only touches
  // m_cells.
  CHECK(grid.scene_repeat(0) == 4);
  CHECK(grid.scene_count() == before + 1);
}

// Song-mode Phase 2: scene_style_id/set_scene_style_id is the per-scene
// style override (kNoStyleOverride means inherit the base rig's style).
void test_scene_style_id_defaults_to_no_override() {
  GridModel grid;
  CHECK(grid.scene_style_id(0) == GridModel::kNoStyleOverride);
  CHECK(grid.scene_style_id(2) == GridModel::kNoStyleOverride);
  CHECK(grid.scene_style_id(GridModel::kMaxSceneCount - 1) == GridModel::kNoStyleOverride);
}

void test_set_scene_style_id_and_bounds() {
  GridModel grid;
  grid.set_scene_style_id(1, 5);
  CHECK(grid.scene_style_id(1) == 5);
  // A neighbour is untouched.
  CHECK(grid.scene_style_id(0) == GridModel::kNoStyleOverride);
  CHECK(grid.scene_style_id(2) == GridModel::kNoStyleOverride);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_style_id(GridModel::kMaxSceneCount, 10);
  grid.set_scene_style_id(GridModel::kMaxSceneCount + 10, 10);

  // Out-of-range get returns kNoStyleOverride, not garbage.
  CHECK(grid.scene_style_id(GridModel::kMaxSceneCount) == GridModel::kNoStyleOverride);
  CHECK(grid.scene_style_id(GridModel::kMaxSceneCount + 10) == GridModel::kNoStyleOverride);
}

void test_set_scene_style_id_negative_clears_to_no_override() {
  GridModel grid;
  grid.set_scene_style_id(0, 5);
  CHECK(grid.scene_style_id(0) == 5);
  grid.set_scene_style_id(0, -1);
  CHECK(grid.scene_style_id(0) == GridModel::kNoStyleOverride);
  // Try a more negative value too.
  grid.set_scene_style_id(0, 5);
  grid.set_scene_style_id(0, -99);
  CHECK(grid.scene_style_id(0) == GridModel::kNoStyleOverride);
}

// Song-mode Phase 2: scene_groove_override/scene_groove/set_scene_groove
// is the per-scene groove override, with a single atomic flag gating all
// six fields together.
void test_scene_groove_defaults_to_no_override() {
  GridModel grid;
  CHECK(grid.scene_groove_override(0) == false);
  CHECK(grid.scene_groove_override(2) == false);
  CHECK(grid.scene_groove_override(GridModel::kMaxSceneCount - 1) == false);
  // Defaults match SceneGroove{}'s own initializers.
  CHECK(grid.scene_groove(0).swing == 0);
  CHECK(grid.scene_groove(0).humanize_timing == 0);
  CHECK(grid.scene_groove(0).humanize_velocity == 0);
  CHECK(grid.scene_groove(0).accent == 0);
  CHECK(grid.scene_groove(0).swing_grid == 8);
  CHECK(grid.scene_groove(0).quantize == 0);
}

void test_set_scene_groove_and_bounds() {
  GridModel grid;
  GridModel::SceneGroove g{.swing = 40,
                           .humanize_timing = 10,
                           .humanize_velocity = 20,
                           .accent = 30,
                           .swing_grid = 16,
                           .quantize = 50};
  grid.set_scene_groove(1, true, g);
  CHECK(grid.scene_groove_override(1) == true);
  CHECK(grid.scene_groove(1).swing == 40);
  CHECK(grid.scene_groove(1).humanize_timing == 10);
  CHECK(grid.scene_groove(1).humanize_velocity == 20);
  CHECK(grid.scene_groove(1).accent == 30);
  CHECK(grid.scene_groove(1).swing_grid == 16);
  CHECK(grid.scene_groove(1).quantize == 50);
  // A neighbour is untouched.
  CHECK(grid.scene_groove_override(0) == false);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_groove(GridModel::kMaxSceneCount, true, g);
  grid.set_scene_groove(GridModel::kMaxSceneCount + 10, true, g);

  // Out-of-range get returns defaults, not garbage.
  CHECK(grid.scene_groove_override(GridModel::kMaxSceneCount) == false);
  CHECK(grid.scene_groove(GridModel::kMaxSceneCount).swing == 0);
  CHECK(grid.scene_groove(GridModel::kMaxSceneCount).swing_grid == 8);
}

// Song-mode Phase 2: scene_key_override/scene_key_root/scene_key_mode/
// set_scene_key is the per-scene key override (root pitch class + mode),
// with a single atomic override flag gating both fields together.
void test_scene_key_defaults_to_no_override() {
  GridModel grid;
  CHECK(grid.scene_key_override(0) == false);
  CHECK(grid.scene_key_override(2) == false);
  CHECK(grid.scene_key_override(GridModel::kMaxSceneCount - 1) == false);
  CHECK(grid.scene_key_root(0) == 0);
  CHECK(grid.scene_key_mode(0) == 0);
  CHECK(grid.scene_key_root(2) == 0);
  CHECK(grid.scene_key_mode(2) == 0);
}

void test_set_scene_key_and_bounds() {
  GridModel grid;
  grid.set_scene_key(1, true, 7, 2);
  CHECK(grid.scene_key_override(1) == true);
  CHECK(grid.scene_key_root(1) == 7);
  CHECK(grid.scene_key_mode(1) == 2);
  // A neighbour is untouched.
  CHECK(grid.scene_key_override(0) == false);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_key(GridModel::kMaxSceneCount, true, 5, 1);
  grid.set_scene_key(GridModel::kMaxSceneCount + 10, true, 5, 1);

  // Out-of-range get returns defaults, not garbage.
  CHECK(grid.scene_key_override(GridModel::kMaxSceneCount) == false);
  CHECK(grid.scene_key_root(GridModel::kMaxSceneCount) == 0);
  CHECK(grid.scene_key_mode(GridModel::kMaxSceneCount) == 0);
}

// Song-mode Phase 2: scene_tempo_x100/set_scene_tempo_x100 is the per-scene
// tempo override in tempo_x100 units (BPM * 100). kNoTempoOverride (0)
// means inherit the base rig's tempo.
void test_scene_tempo_defaults_to_no_override() {
  GridModel grid;
  CHECK(grid.scene_tempo_x100(0) == GridModel::kNoTempoOverride);
  CHECK(grid.scene_tempo_x100(2) == GridModel::kNoTempoOverride);
  CHECK(grid.scene_tempo_x100(GridModel::kMaxSceneCount - 1) == GridModel::kNoTempoOverride);
}

void test_set_scene_tempo_and_bounds() {
  GridModel grid;
  grid.set_scene_tempo_x100(1, 14000);
  CHECK(grid.scene_tempo_x100(1) == 14000);
  // A neighbour is untouched.
  CHECK(grid.scene_tempo_x100(0) == GridModel::kNoTempoOverride);
  CHECK(grid.scene_tempo_x100(2) == GridModel::kNoTempoOverride);

  // Out-of-range set is a no-op, not a crash or UB.
  grid.set_scene_tempo_x100(GridModel::kMaxSceneCount, 14000);
  grid.set_scene_tempo_x100(GridModel::kMaxSceneCount + 10, 14000);

  // Out-of-range get returns kNoTempoOverride, not garbage.
  CHECK(grid.scene_tempo_x100(GridModel::kMaxSceneCount) == GridModel::kNoTempoOverride);
  CHECK(grid.scene_tempo_x100(GridModel::kMaxSceneCount + 10) == GridModel::kNoTempoOverride);
}

void test_set_scene_tempo_clamps_to_valid_bpm_range() {
  GridModel grid;
  // Below floor clamps up.
  grid.set_scene_tempo_x100(0, 100);
  CHECK(grid.scene_tempo_x100(0) == GridModel::kMinBpmMirror);
  // Above ceiling clamps down.
  grid.set_scene_tempo_x100(0, 999999);
  CHECK(grid.scene_tempo_x100(0) == GridModel::kMaxBpmMirror);
  // Zero clears.
  grid.set_scene_tempo_x100(0, 14000);
  grid.set_scene_tempo_x100(0, 0);
  CHECK(grid.scene_tempo_x100(0) == GridModel::kNoTempoOverride);
  // Negative clears.
  grid.set_scene_tempo_x100(0, 14000);
  grid.set_scene_tempo_x100(0, -50);
  CHECK(grid.scene_tempo_x100(0) == GridModel::kNoTempoOverride);
}

void test_add_scene_preserves_scene_style_groove_key_tempo() {
  GridModel grid;
  // Set non-default values for each of the four new groups on scene 0.
  grid.set_scene_style_id(0, 3);
  GridModel::SceneGroove g{.swing = 25,
                           .humanize_timing = 15,
                           .humanize_velocity = 10,
                           .accent = 20,
                           .swing_grid = 12,
                           .quantize = 35};
  grid.set_scene_groove(0, true, g);
  grid.set_scene_key(0, true, 5, 1);
  grid.set_scene_tempo_x100(0, 12000);

  const std::size_t before = grid.scene_count();
  grid.add_scene();

  // All four values on scene 0 survive the grow unchanged.
  CHECK(grid.scene_style_id(0) == 3);
  CHECK(grid.scene_groove_override(0) == true);
  CHECK(grid.scene_groove(0).swing == 25);
  CHECK(grid.scene_groove(0).humanize_timing == 15);
  CHECK(grid.scene_groove(0).humanize_velocity == 10);
  CHECK(grid.scene_groove(0).accent == 20);
  CHECK(grid.scene_groove(0).swing_grid == 12);
  CHECK(grid.scene_groove(0).quantize == 35);
  CHECK(grid.scene_key_override(0) == true);
  CHECK(grid.scene_key_root(0) == 5);
  CHECK(grid.scene_key_mode(0) == 1);
  CHECK(grid.scene_tempo_x100(0) == 12000);
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

void test_next_scene_holds_at_last_scene() {
  // Song-form Option A: the last scene column does NOT wrap back to 0 --
  // it holds (nullopt), matching SceneChain::on_bar's own "last step holds"
  // precedent (scene_chain.hpp:121-124).
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/4,
                              /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                              /*active_scene_section_bars=*/1)
             .has_value());
}

void test_next_scene_zero_scene_count_stays() {
  // Defensive: no scene to wrap into.
  CHECK(!next_scene_to_launch(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/0,
                              /*scene_count=*/0, /*bars_elapsed_in_scene=*/5,
                              /*active_scene_section_bars=*/1)
             .has_value());
}

// auto_song_reached_song_end: song-form Option A's "cue the Ending" refinement
// on top of next_scene_to_launch's own nullopt -- mirrors its guard order
// exactly, then narrows to "AND it is the last column".
using sonotron::auto_song_reached_song_end;

void test_auto_song_reached_song_end_true_at_last_column_boundary() {
  CHECK(auto_song_reached_song_end(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/4,
                                   /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                                   /*active_scene_section_bars=*/1));
}

void test_auto_song_reached_song_end_false_not_last_column() {
  // Same boundary reached, but active_scene=1 is not the last column.
  CHECK(!auto_song_reached_song_end(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/1,
                                    /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                                    /*active_scene_section_bars=*/1));
}

void test_auto_song_reached_song_end_false_before_boundary() {
  // Last column, but not yet at the boundary (bars_elapsed < section_bars).
  CHECK(!auto_song_reached_song_end(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/4,
                                    /*scene_count=*/5, /*bars_elapsed_in_scene=*/0,
                                    /*active_scene_section_bars=*/1));
}

void test_auto_song_reached_song_end_false_auto_song_off() {
  CHECK(!auto_song_reached_song_end(/*auto_song=*/false, /*playing=*/true, /*active_scene=*/4,
                                    /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                                    /*active_scene_section_bars=*/1));
}

void test_auto_song_reached_song_end_false_not_playing() {
  CHECK(!auto_song_reached_song_end(/*auto_song=*/true, /*playing=*/false, /*active_scene=*/4,
                                    /*scene_count=*/5, /*bars_elapsed_in_scene=*/1,
                                    /*active_scene_section_bars=*/1));
}

void test_auto_song_reached_song_end_false_zero_scene_count() {
  CHECK(!auto_song_reached_song_end(/*auto_song=*/true, /*playing=*/true, /*active_scene=*/0,
                                    /*scene_count=*/0, /*bars_elapsed_in_scene=*/1,
                                    /*active_scene_section_bars=*/1));
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

// section_playhead_phase: beat-synchronized playhead phase for the launch-cell
// sweep bar, advancing 0->100% over the active scene's section length.
using sonotron::section_playhead_phase;

void test_section_playhead_phase_at_start_of_section() {
  // current_bar == active_scene_start_bar, at beat 1, pulse 0: phase ≈ 0.0
  const float phase = section_playhead_phase(
      /*current_bar=*/5, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(std::fabs(phase - 0.0F) < 1e-4F);
}

void test_section_playhead_phase_mid_section_whole_bar() {
  // 2 bars elapsed of a 4-bar section, at downbeat: phase ≈ 0.5
  const float phase = section_playhead_phase(
      /*current_bar=*/7, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(std::fabs(phase - 0.5F) < 1e-4F);
}

void test_section_playhead_phase_within_bar_beat_advance() {
  // 0 bars elapsed, beat_num=3 (at beat 3), pulse=0, bpb=4, section_bars=1.
  // within_bar = (max(3-1, 0) + 0/24) / 4 = 2/4 = 0.5
  // phase = (0 + 0.5) / 1 = 0.5
  const float phase = section_playhead_phase(
      /*current_bar=*/1, /*active_scene_start_bar=*/1, /*beat_num=*/3, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/1);
  CHECK(std::fabs(phase - 0.5F) < 1e-4F);
}

void test_section_playhead_phase_within_bar_pulse_advance() {
  // 0 bars elapsed, beat_num=1, pulse=12 (half a beat), bpb=4, section_bars=1.
  // within_bar = (0 + 12/24) / 4 = 0.5 / 4 = 0.125
  // phase = (0 + 0.125) / 1 = 0.125
  const float phase = section_playhead_phase(
      /*current_bar=*/2, /*active_scene_start_bar=*/2, /*beat_num=*/1, /*pulse=*/12,
      /*beats_per_bar=*/4, /*section_bars=*/1);
  CHECK(std::fabs(phase - 0.125F) < 1e-4F);
}

void test_section_playhead_phase_clamps_to_one_at_section_end() {
  // bars_elapsed == section_bars (at boundary): phase clamps to 1.0
  const float phase = section_playhead_phase(
      /*current_bar=*/9, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(std::fabs(phase - 1.0F) < 1e-4F);
}

void test_section_playhead_phase_clamps_beyond_section_end() {
  // bars_elapsed beyond section_bars: phase still clamps to 1.0
  const float phase = section_playhead_phase(
      /*current_bar=*/15, /*active_scene_start_bar=*/5, /*beat_num=*/2, /*pulse=*/12,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(phase <= 1.0F);
  CHECK(std::fabs(phase - 1.0F) < 1e-4F);
}

void test_section_playhead_phase_not_started_current_bar_zero() {
  // current_bar == 0 (transport parked): sentinel -1.0F
  const float phase = section_playhead_phase(
      /*current_bar=*/0, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(phase < 0.0F);
}

void test_section_playhead_phase_sentinel_section_bars_zero() {
  // section_bars == 0 (invalid): sentinel -1.0F
  const float phase = section_playhead_phase(
      /*current_bar=*/5, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/0);
  CHECK(phase < 0.0F);
}

void test_section_playhead_phase_sentinel_beats_per_bar_zero() {
  // beats_per_bar == 0 (invalid): sentinel -1.0F
  const float phase = section_playhead_phase(
      /*current_bar=*/5, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/0, /*section_bars=*/4);
  CHECK(phase < 0.0F);
}

void test_section_playhead_phase_rewind_bar_less_than_anchor() {
  // current_bar < active_scene_start_bar (bar rewind): sentinel -1.0F
  const float phase = section_playhead_phase(
      /*current_bar=*/3, /*active_scene_start_bar=*/5, /*beat_num=*/1, /*pulse=*/0,
      /*beats_per_bar=*/4, /*section_bars=*/4);
  CHECK(phase < 0.0F);
}

void test_section_playhead_phase_different_time_signature_3_4() {
  // bpb=3 (3/4 time), section_bars=2, beat_num=2, pulse=0
  // within_bar = (1 + 0) / 3 = 0.333...
  // phase = 0.333... / 2 ≈ 0.1667
  const float phase = section_playhead_phase(
      /*current_bar=*/1, /*active_scene_start_bar=*/1, /*beat_num=*/2, /*pulse=*/0,
      /*beats_per_bar=*/3, /*section_bars=*/2);
  CHECK(std::fabs(phase - (1.0F / 6.0F)) < 1e-4F);  // 1/6 ≈ 0.1667
}

// repeat_cycle_start_bar: repeat-local playhead anchor (owner task #3) -- the
// bar at which the CURRENT repeat cycle began, given the bar the WHOLE hold
// started and the section's own real length in bars.
using sonotron::repeat_cycle_start_bar;

void test_repeat_cycle_start_bar_first_repeat_is_the_scene_start() {
  // Still inside the FIRST repeat (bars_elapsed < repeat_length_bars): the
  // anchor is just the scene's own start bar, unchanged.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/6, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 5);
}

void test_repeat_cycle_start_bar_advances_exactly_at_a_repeat_boundary() {
  // Exactly one repeat_length_bars past the scene start: the SECOND repeat's
  // own cycle has just begun, so the anchor jumps forward by one full
  // repeat_length_bars.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/9, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 9);
}

void test_repeat_cycle_start_bar_mid_second_repeat() {
  // Two bars into the SECOND repeat (bars_elapsed == 6, repeat_length == 4):
  // one whole repeat (4 bars) has completed, so the anchor is scene_start +
  // 4, not scene_start itself.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/11, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 9);
}

void test_repeat_cycle_start_bar_third_repeat() {
  // Two whole repeats completed (bars_elapsed == 8, repeat_length == 4): the
  // THIRD repeat's own cycle has just begun.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/13, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 13);
}

void test_repeat_cycle_start_bar_degenerate_non_positive_length_falls_back() {
  // repeat_length_bars <= 0 (invalid): falls back to scene_start_bar
  // verbatim -- section_playhead_phase's own guards already turn this into
  // the honest "no playhead" sentinel, so there is nothing to compute.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/20, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/0) == 5);
  CHECK(repeat_cycle_start_bar(/*current_bar=*/20, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/-1) == 5);
}

void test_repeat_cycle_start_bar_rewind_falls_back_to_scene_start() {
  // current_bar < scene_start_bar (a bar rewind mid-hold, e.g. a
  // stop/restart cycle that has not yet re-anchored): falls back to
  // scene_start_bar verbatim, same degenerate-input discipline as above.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/3, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 5);
}

void test_repeat_cycle_start_bar_exactly_at_scene_start() {
  // current_bar == scene_start_bar: zero bars elapsed, still the first
  // repeat -- the anchor is the scene start itself.
  CHECK(repeat_cycle_start_bar(/*current_bar=*/5, /*scene_start_bar=*/5,
                               /*repeat_length_bars=*/4) == 5);
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
  test_scene_bars_defaults_to_default_scene_bars();
  test_set_scene_bars_and_bounds();
  test_set_scene_bars_clamps_floor_at_one();
  test_set_scene_bars_clamps_ceiling_at_max();
  test_add_scene_preserves_scene_bars();
  test_scene_repeat_defaults_to_default_scene_repeat();
  test_set_scene_repeat_and_bounds();
  test_set_scene_repeat_clamps_floor_at_one();
  test_set_scene_repeat_clamps_ceiling_at_infinite_sentinel();
  test_add_scene_preserves_scene_repeat();
  test_scene_style_id_defaults_to_no_override();
  test_set_scene_style_id_and_bounds();
  test_set_scene_style_id_negative_clears_to_no_override();
  test_scene_groove_defaults_to_no_override();
  test_set_scene_groove_and_bounds();
  test_scene_key_defaults_to_no_override();
  test_set_scene_key_and_bounds();
  test_scene_tempo_defaults_to_no_override();
  test_set_scene_tempo_and_bounds();
  test_set_scene_tempo_clamps_to_valid_bpm_range();
  test_add_scene_preserves_scene_style_groove_key_tempo();
  test_section_wire_name_matches_known_spellings();
  test_launch_wired_is_lit();
  test_next_scene_auto_song_off_stays();
  test_next_scene_not_playing_stays();
  test_next_scene_mid_scene_stays();
  test_next_scene_advances_at_boundary();
  test_next_scene_advances_past_boundary();
  test_next_scene_holds_at_last_scene();
  test_next_scene_zero_scene_count_stays();
  test_auto_song_reached_song_end_true_at_last_column_boundary();
  test_auto_song_reached_song_end_false_not_last_column();
  test_auto_song_reached_song_end_false_before_boundary();
  test_auto_song_reached_song_end_false_auto_song_off();
  test_auto_song_reached_song_end_false_not_playing();
  test_auto_song_reached_song_end_false_zero_scene_count();
  test_bar_just_advanced_fires_once_per_distinct_bar();
  test_section_playhead_phase_at_start_of_section();
  test_section_playhead_phase_mid_section_whole_bar();
  test_section_playhead_phase_within_bar_beat_advance();
  test_section_playhead_phase_within_bar_pulse_advance();
  test_section_playhead_phase_clamps_to_one_at_section_end();
  test_section_playhead_phase_clamps_beyond_section_end();
  test_section_playhead_phase_not_started_current_bar_zero();
  test_section_playhead_phase_sentinel_section_bars_zero();
  test_section_playhead_phase_sentinel_beats_per_bar_zero();
  test_section_playhead_phase_rewind_bar_less_than_anchor();
  test_section_playhead_phase_different_time_signature_3_4();
  test_repeat_cycle_start_bar_first_repeat_is_the_scene_start();
  test_repeat_cycle_start_bar_advances_exactly_at_a_repeat_boundary();
  test_repeat_cycle_start_bar_mid_second_repeat();
  test_repeat_cycle_start_bar_third_repeat();
  test_repeat_cycle_start_bar_degenerate_non_positive_length_falls_back();
  test_repeat_cycle_start_bar_rewind_falls_back_to_scene_start();
  test_repeat_cycle_start_bar_exactly_at_scene_start();
  return sonotron::test::failures();
}

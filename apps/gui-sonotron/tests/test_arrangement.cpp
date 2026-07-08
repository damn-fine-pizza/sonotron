// Unit tests for the Arrangement view-model (pure data — no ImGui): the mock
// arrangement's shape/invariants and the density_at / lane_has_ghost helpers.

#include "src/arrangement.hpp"

#include <cstring>

#include "test.hpp"

namespace {

void test_density_at_clamps_out_of_range() {
  sonotron::ArrangementLane lane{.name = "X", .density = {0.2F, 0.8F}, .muted = false, .ghost = ""};
  CHECK(sonotron::density_at(lane, -1) == 0.0F);
  CHECK(sonotron::density_at(lane, 0) > 0.19F && sonotron::density_at(lane, 0) < 0.21F);
  CHECK(sonotron::density_at(lane, 1) > 0.79F && sonotron::density_at(lane, 1) < 0.81F);
  CHECK(sonotron::density_at(lane, 2) == 0.0F);  // past the end
  CHECK(sonotron::density_at(lane, 99) == 0.0F);
}

void test_lane_has_ghost() {
  sonotron::ArrangementLane with{.name = "Pad", .density = {}, .muted = false, .ghost = "enters"};
  sonotron::ArrangementLane without{.name = "Drums", .density = {}, .muted = false, .ghost = ""};
  CHECK(sonotron::lane_has_ghost(with));
  CHECK(!sonotron::lane_has_ghost(without));
}

void test_mock_shape() {
  const sonotron::Arrangement arr = sonotron::mock_arrangement();
  CHECK(arr.total_bars == 16);
  CHECK(arr.now_bar > 7.9F && arr.now_bar < 8.1F);
  CHECK(arr.lanes.size() == 5);
  CHECK(!arr.sections.empty());

  // Every lane's density timeline spans the full bar count, and values stay
  // in [0,1] so the streamgraph never draws a negative/overflowing band.
  for (const sonotron::ArrangementLane& lane : arr.lanes) {
    CHECK(lane.density.size() == static_cast<std::size_t>(arr.total_bars));
    for (const float d : lane.density) {
      CHECK(d >= 0.0F && d <= 1.0F);
    }
  }
}

void test_mock_sections_tile_the_bar_axis() {
  const sonotron::Arrangement arr = sonotron::mock_arrangement();
  int covered = 0;
  for (const sonotron::Section& s : arr.sections) {
    CHECK(s.start_bar == covered);  // sections are contiguous, in order
    covered += s.length_bars;
  }
  CHECK(covered == arr.total_bars);  // and cover the whole song
}

void test_mock_ghost_lanes_are_silent_at_now() {
  // Pad and Arp are the Director's upcoming moves: they must carry a ghost
  // label AND be silent at NOW, so their bands only appear in the future.
  const sonotron::Arrangement arr = sonotron::mock_arrangement();
  const int now = static_cast<int>(arr.now_bar);
  for (const sonotron::ArrangementLane& lane : arr.lanes) {
    if (sonotron::lane_has_ghost(lane)) {
      CHECK(sonotron::density_at(lane, now) == 0.0F);
    }
  }
}

}  // namespace

int main() {
  test_density_at_clamps_out_of_range();
  test_lane_has_ghost();
  test_mock_shape();
  test_mock_sections_tile_the_bar_axis();
  test_mock_ghost_lanes_are_silent_at_now();
  return sonotron::test::failures();
}

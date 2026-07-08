#include "arrangement.hpp"

#include <cstddef>

namespace sonotron {

bool lane_has_ghost(const ArrangementLane& lane) {
  return lane.ghost != nullptr && lane.ghost[0] != '\0';
}

float density_at(const ArrangementLane& lane, int bar) {
  if (bar < 0 || static_cast<std::size_t>(bar) >= lane.density.size()) {
    return 0.0F;
  }
  return lane.density[static_cast<std::size_t>(bar)];
}

Arrangement mock_arrangement() {
  Arrangement arr;
  arr.total_bars = 16;
  arr.now_bar = 8.0F;  // pinned at ~1/3 from the left by the renderer

  arr.sections = {
      Section{.name = "Intro", .start_bar = 0, .length_bars = 2},
      Section{.name = "A", .start_bar = 2, .length_bars = 6},
      Section{.name = "B", .start_bar = 8, .length_bars = 4},
      Section{.name = "Chorus", .start_bar = 12, .length_bars = 4},
  };

  arr.lanes = {
      ArrangementLane{.name = "Drums",
                      .density = {0.35F, 0.35F, 0.60F, 0.60F, 0.60F, 0.65F, 0.70F, 0.70F, 0.85F,
                                  0.85F, 0.90F, 0.90F, 1.00F, 1.00F, 1.00F, 1.00F},
                      .muted = false,
                      .ghost = ""},
      ArrangementLane{.name = "Bass",
                      .density = {0.00F, 0.00F, 0.50F, 0.50F, 0.50F, 0.55F, 0.60F, 0.60F, 0.70F,
                                  0.70F, 0.70F, 0.75F, 0.80F, 0.80F, 0.85F, 0.85F},
                      .muted = false,
                      .ghost = ""},
      ArrangementLane{.name = "Keys",
                      .density = {0.00F, 0.00F, 0.55F, 0.65F, 0.70F, 0.75F, 0.80F, 0.80F, 0.20F,
                                  0.20F, 0.25F, 0.20F, 0.70F, 0.80F, 0.85F, 0.90F},
                      .muted = true,
                      .ghost = ""},
      ArrangementLane{.name = "Pad",
                      .density = {0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F,
                                  0.00F, 0.00F, 0.00F, 0.50F, 0.55F, 0.60F, 0.60F},
                      .muted = false,
                      .ghost = "enters -> 4"},
      ArrangementLane{.name = "Arp",
                      .density = {0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.00F,
                                  0.00F, 0.00F, 0.00F, 0.00F, 0.00F, 0.45F, 0.50F},
                      .muted = false,
                      .ghost = "off -> on"},
  };

  return arr;
}

}  // namespace sonotron

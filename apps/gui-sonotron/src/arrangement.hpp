#pragma once

#include <vector>

// Pure-data view model for the Arrangement zone — the big central surface: an
// observable time horizon of the whole song. NOT a DAW clip editor. Time runs
// left-to-right in bars; a fixed NOW playhead splits the PAST (what actually
// happened, solid) from the FUTURE (the Director's plan, ghosted). Each part
// is a lane whose per-bar activity is drawn as a flowing band (streamgraph).
//
// ImGui-free and core-free like intention.hpp; unit-testable. The ImGui
// drawing lives in arrangement_panel.hpp/.cpp. (Absorbs the earlier per-part
// Band model: a lane here is a part shown across time rather than at a single
// instant.)

namespace sonotron {

// A named span of the song form on the bar axis (e.g. "Intro" bars [0,2),
// "Chorus" bars [12,16)). `start_bar` is inclusive and 0-based.
struct Section {
  const char* name = "";
  int start_bar = 0;
  int length_bars = 0;
};

// One part's activity across the whole arrangement. `density` holds one value
// in [0,1] per bar (size == Arrangement::total_bars): how busy the part is in
// that bar, which sets the band's thickness. `muted` is the user's [M]
// override; `ghost` labels an upcoming Director move ("enters -> 4"), empty
// when there is none.
struct ArrangementLane {
  const char* name = "";
  std::vector<float> density;
  bool muted = false;
  const char* ghost = "";
};

// The observable horizon. `now_bar` is the fractional current position; the
// renderer pins it at a fixed fraction from the left so most of the surface
// shows the future ("a spyglass on the Director's plan").
struct Arrangement {
  int total_bars = 0;
  float now_bar = 0.0F;
  std::vector<Section> sections;
  std::vector<ArrangementLane> lanes;
};

// Fixed placeholder arrangement straight from the ux-concept mockup: drums
// building through Intro/A/B/Chorus, a bass and comping (muted) keys, and two
// lanes the Director is about to bring in (pad, arp) whose bands appear only
// in the future/ghost region.
Arrangement mock_arrangement();

// True when the lane has a pending Director move to show as a ghost.
bool lane_has_ghost(const ArrangementLane& lane);

// Linearly-clamped read of a lane's density at an integer bar (0 outside the
// lane's range), so the renderer can sample without bounds-checking.
float density_at(const ArrangementLane& lane, int bar);

}  // namespace sonotron

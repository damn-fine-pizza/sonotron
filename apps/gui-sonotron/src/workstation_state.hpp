#pragma once

#include "app_state.hpp"
#include "brain_session.hpp"
#include "browser_model.hpp"
#include "grid_model.hpp"
#include "parts_model.hpp"
#include "seqedit_model.hpp"
#include "v02_state.hpp"

// Bundles the mutable/const state each zone panel needs, threaded through
// render_layout -> layout_renderer.cpp's zone dispatch -> the individual
// *_panel.cpp render functions. Pure data (references only); no ImGui
// dependency, so this header does not widen who is allowed to include ImGui
// (see the *_panel/*_model split invariant).

namespace sonotron {

struct WorkstationState {
  AppState& app_state;
  BrainSession& brain_session;
  BrowserModel& browser;
  GridModel& grid;
  SeqEditModel& seqedit;
  PartsModel& parts;
  V02State& fx;  // v02 redesign: glow flag, frame clock, local intent surface
};

}  // namespace sonotron

// UI-AUTOMATION ACCEPTANCE TEST (task #6): the per-scene LENGTH stepper -- a
// small, always-visible "- <bars> +" control in every scene-header column
// (grid_panel.cpp's render_scene_header_cell), reading/writing GridModel::
// scene_bars directly. Two things need proving, both through REAL input
// injection and REAL model/engine readback (flow-verification-matrix-2026-
// 07.md §1 convention -- never a hand-written send() or a direct field write
// standing in for a click):
//
//   1. Clicking "+"/"-" for real changes model.scene_bars(i) by the expected
//      amount, read back from the real GridModel afterward -- never assumed.
//   2. Dialing a scene's length DOWN via real clicks, BEFORE Play, genuinely
//      changes how long auto-song holds that column -- i.e. the stepper's own
//      value is what gates the advance now (task #6's whole point,
//      grid_panel.cpp's active_style_section_bars), not a vestigial display
//      number the advance ignores (which is what the OLD style-section-
//      length * kDefaultSectionRepeats threshold would have used instead).
//
// LOCATE STRATEGY (mirrors test_repeat_zone_scene_header_next_bar_ui_
// automation.cpp's own proven caret-cluster technique, extended one more
// hop): the scene-header caret ("\xE2\x96\xB6", neon::u32(theme::kGreen),
// full alpha) is already locatable per-column via find_color_clusters
// filtered to a band just under grid_body's own top edge (same collision
// analysis as that sibling test: the ONLY other full-alpha kGreen paint in
// this test's own frame -- transport_panel's "Cm" key readout and the
// "chord" track-row label -- both land well outside this band). From a
// column's own caret rect, this test back-derives that column's own LEFT
// edge (hp0.x): the caret is drawn at hp0.x + cz - caret_ts.x - 3 (render_
// scene_header_cell), so its right edge sits ~3px shy of the column's own
// right edge, hp0.x + cz -- and `cz` itself is not guessed: UiState::
// cell_zoom is a public field this test constructs directly (default 52.0F,
// left untouched here).
//
// The stepper strip's own Y band is then real-located too: every stepper
// button (render_scene_header_cell's stepper_button helper) fills with the
// SAME exact color while unhovered -- neon::u32(theme::kFrameBg, 0.85F), a
// combination unique in this codebase (grid_latch's own M/S pair uses
// 0.9F/1.0F; every ImGui-owned FrameBg-colored widget renders at the theme's
// own baked-in 1.0F alpha) -- found via find_color_clusters, filtered to
// that column's own known x-extent and a generous band below the caret.
// Rather than trust find_color_clusters' draw-order-proximity grouping to
// cleanly separate "-" from "+" into two individual rects (a small, glyph-
// sized gap between two same-colored fills is not guaranteed to exceed its
// default index_gap), this test merges every matching cluster in that column
// into ONE combined bounding box for the Y band only, then clicks well
// inside the combined box's own LEFT and RIGHT few pixels -- landing
// resolutely on "-" (which only ever occupies the strip's own left edge) and
// "+" (only ever the right edge) without needing to trust that ambiguous
// per-button split at all.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
#include "src/parts_model.hpp"
#include "src/seqedit_model.hpp"
#include "src/theme.hpp"
#include "src/transport_panel.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// Same combined transport+grid frame shape every UI-automation test in this
// directory shares.
ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1280.0F, 800.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  sonotron::render_transport_panel(app_state, brain_session, fx);
  ImGui::Spacing();
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                     BrainSession& brain_session, AppState& app_state, UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
}

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

// The two real click points for one scene column's own "- N +" stepper --
// see this file's own header comment for the full locate-strategy rationale.
struct StepperClickPoints {
  ImVec2 minus_click;
  ImVec2 plus_click;
  bool found = false;
};

StepperClickPoints locate_scene_stepper(ImDrawData* locate, const th::Rect& grid_body_rect,
                                        std::size_t column, float cz) {
  StepperClickPoints out;

  const ImU32 caret_color = sonotron::neon::u32(sonotron::theme::kGreen);
  const std::vector<th::Rect> all_carets = th::find_color_clusters(locate, caret_color);
  std::vector<th::Rect> carets;
  for (const th::Rect& r : all_carets) {
    if (r.min.y >= grid_body_rect.min.y && r.max.y <= grid_body_rect.min.y + 60.0F) {
      carets.push_back(r);
    }
  }
  if (column >= carets.size()) {
    return out;
  }
  const th::Rect col_caret = carets[column];
  const float col_left = col_caret.max.x - cz + 3.0F;

  const ImU32 stepper_fill = sonotron::neon::u32(sonotron::theme::kFrameBg, 0.85F);
  const std::vector<th::Rect> stepper_clusters = th::find_color_clusters(locate, stepper_fill);
  // Task #5 added a SECOND stepper row (repeat count) directly below this
  // one, same fill color and flush against it (no vertical gap) -- so
  // find_color_clusters no longer returns two small per-row rects at all: the
  // "-" fill (and, separately, the "+" fill) of both rows now merges into ONE
  // tall rect spanning both rows, since cluster grouping only needs adjacent
  // same-colored pixels, not a shared row. This test targets the BARS
  // stepper specifically (task #6, the topmost row), so rather than trust any
  // merged rect's own extent, it locates ONLY that top edge (the minimum
  // min.y among matching clusters) and clicks a few pixels below it --
  // solidly inside the top row, clear of the second row's own territory.
  float top_row_y = -1.0F;
  for (const th::Rect& r : stepper_clusters) {
    if (r.min.x >= col_left - 5.0F && r.max.x <= col_left + cz + 5.0F &&
        r.min.y >= col_caret.min.y && r.max.y <= col_caret.min.y + 100.0F) {
      if (top_row_y < 0.0F || r.min.y < top_row_y) {
        top_row_y = r.min.y;
      }
    }
  }
  if (top_row_y < 0.0F) {
    return out;
  }
  const float click_y = top_row_y + 6.0F;
  out.minus_click = ImVec2(col_left + 6.0F, click_y);
  out.plus_click = ImVec2(col_left + cz - 6.0F, click_y);
  out.found = true;
  return out;
}

// -----------------------------------------------------------------------
// TEST 1: real click injection on "+"/"-" for one scene column changes
// model.scene_bars(i) by the expected amount, read back from the real
// GridModel -- never a direct field write standing in for the click. Also
// proves the two-separate-hit-regions fix (grid_panel.cpp's own header
// comment on this): clicking the stepper never also fires activate_scene_
// column, so fx.active_scene never leaves its untouched default (0).
// -----------------------------------------------------------------------
void test_stepper_clicks_change_scene_bars_and_never_launch_the_scene() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;
  InProcessBrainSession session;
  CHECK(session.start());

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  const th::Rect grid_body_rect = th::find_child_window_rect("grid_body");
  CHECK(grid_body_rect.found);
  const float cz = fx.cell_zoom;
  // Column 1, deliberately NOT column 0 (UiState::active_scene's own
  // untouched default) -- so an accidental scene launch from a mis-hit
  // "head" region would be unambiguously visible as fx.active_scene flipping
  // to 1.
  const StepperClickPoints stepper = locate_scene_stepper(locate, grid_body_rect, 1, cz);
  CHECK(stepper.found);
  if (!stepper.found) {
    ImGui::DestroyContext();
    return;
  }

  CHECK(model.scene_bars(1) == GridModel::kDefaultSceneBars);

  // Real click: one "-" press moves the real model down by exactly one.
  click_at(stepper.minus_click, model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(model.scene_bars(1) == GridModel::kDefaultSceneBars - 1);

  // Real clicks: enough "-" presses to floor at 1 (GridModel::set_scene_
  // bars' own clamp, never re-clamped here).
  for (int i = 0; i < 10; ++i) {
    click_at(stepper.minus_click, model, seqedit, parts, session, app_state, fx);
  }
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(model.scene_bars(1) == 1);

  // Real clicks: enough "+" presses to ceiling at kMaxSceneBars.
  for (int i = 0; i < 15; ++i) {
    click_at(stepper.plus_click, model, seqedit, parts, session, app_state, fx);
  }
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(model.scene_bars(1) == GridModel::kMaxSceneBars);

  // No accidental scene launch, the whole time: the "head" InvisibleButton's
  // own reduced hit region (grid_panel.cpp) never overlapped the stepper's
  // own separate one.
  CHECK(fx.active_scene == 0);

  session.stop();
  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// TEST 2: real timing respect. Dial scene 0's own length down to the
// minimum (1 bar) via real stepper clicks BEFORE Play, then prove auto-song
// (ON by default) advances off that column genuinely faster than the OLD
// style-section-length * kDefaultSectionRepeats threshold ever would have
// allowed -- i.e. the stepper's own value is what gates the advance, not a
// vestigial display number. "basic" style's own kVarA is 2 bars (test_
// preview.cpp), so the OLD threshold was 2 * kDefaultSectionRepeats(2) == 4
// bars; the deadline below sits well inside that OLD threshold's own
// wall-clock window at bpm 400 (~2.4s), comfortably above the NEW 1-bar
// threshold's own window (~0.6s).
// -----------------------------------------------------------------------
void test_stepper_min_length_makes_auto_song_advance_genuinely_faster() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  for (std::size_t i = 0; i < sonotron::kBuiltinStyleNames.size(); ++i) {
    if (sonotron::kBuiltinStyleNames[i] == "basic") {
      fx.active_style = static_cast<int>(i);
      break;
    }
  }
  session.send("bpm 400");  // shrink the wall-clock bar cadence

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  const th::Rect grid_body_rect = th::find_child_window_rect("grid_body");
  CHECK(grid_body_rect.found);
  const float cz = fx.cell_zoom;
  // Column 0: the column handle_master_play_launch forces active from, with
  // auto-song ON, the instant Play is observed playing (grid_panel.cpp).
  const StepperClickPoints stepper = locate_scene_stepper(locate, grid_body_rect, 0, cz);
  CHECK(stepper.found);
  if (!stepper.found) {
    ImGui::DestroyContext();
    return;
  }

  // Real clicks: dial scene 0 down to the minimum, BEFORE Play.
  for (int i = 0; i < 10; ++i) {
    click_at(stepper.minus_click, model, seqedit, parts, session, app_state, fx);
  }
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(model.scene_bars(0) == 1);

  // Real click: Play.
  ImDrawData* pre_play = render_one_frame(model, seqedit, parts, session, app_state, fx);
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(pre_play, play_color);
  CHECK(play_rect.found);
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);
  // Deliberately NOT asserting fx.active_scene == 0 here: with the stepper
  // dialed to its 1-bar minimum, the advance can legitimately land as early
  // as the very first non-zero bar this same wait loop is watching for --
  // exactly the fast timing this test exists to prove, not a race to avoid.
  // The window below tolerates fx.active_scene having already flipped by
  // this point (its own check covers "already 1", not only "flips next").

  // THE PIN: the real advance off scene 0 must land within a window that
  // could NEVER have been reached under the OLD 4-bar (2 * kDefaultSection
  // Repeats) threshold this same "basic" style used to gate on -- proving
  // the always-visible stepper's own value (dialed to 1 above) is what
  // actually gates the advance now, not a display-only number the advance
  // ignores.
  bool advanced = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (fx.active_scene != 0) {
        advanced = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  session.stop();

  CHECK(advanced);
  CHECK(fx.active_scene == 1);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_stepper_clicks_change_scene_bars_and_never_launch_the_scene();
  test_stepper_min_length_makes_auto_song_advance_genuinely_faster();
  return sonotron::test::failures();
}

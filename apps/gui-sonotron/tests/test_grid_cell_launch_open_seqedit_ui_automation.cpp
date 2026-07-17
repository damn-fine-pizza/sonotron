// UI-AUTOMATION functional test (Torquato QA pass, flow-verification-matrix-
// 2026-07.md §2c): "Click a FILLED cell -> real `launch clip <id> quantize
// <n>` + opens Sequence Edit" -- the matrix's own verdict was "NONE for this
// exact trigger" (only the pure next_scene_to_launch/model shape was
// covered, never the click handler itself). This closes that gap with REAL
// input injection through the REAL render_track_cell click handler
// (grid_panel.cpp), wired to a REAL InProcessBrainSession, asserting BOTH
// halves of the flow: the real per-cell launch readback (AppState::
// clip_state, the same core ClipMatrix round trip test_repeat_zone_playhead_
// ui_automation.cpp already proved reachable after dff4e9e's demo-clip-
// registration fix) AND the "open in Sequence Edit" bookkeeping
// (V02State::open_cell/open_row/open_section, SeqEditModel::part_index/
// clip_label) -- fields the existing playhead test never asserts on, since
// its own focus is the playhead primitive, not this click's OTHER effect.

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
#include "src/v02_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

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
using sonotron::V02State;
namespace th = sonotron::test_harness;

namespace {

ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, V02State& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  // Explicit size (matching main.cpp's own SetNextWindowSize(viewport->
  // WorkSize)): without it ImGui's small first-use default starves grid_
  // body's remaining height, ImGui sets window->SkipItems, and every track
  // row past ~y=115 never emits a single vertex -- found while writing this
  // test (also pinned as a pre-existing gap in the sibling test_repeat_
  // zone_playhead_ui_automation.cpp, fixed there too).
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
                     BrainSession& brain_session, AppState& app_state, V02State& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
}

void test_real_click_on_filled_cell_launches_and_opens_sequence_edit() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  SeqEditModel seqedit;
  PartsModel parts;
  V02State fx;
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

  // Warm-up + locate frame (seed_demo's own default open cell is bass/scene 0
  // -- fx.open_row == 1 -- so clicking a DIFFERENT cell below, drums/scene 0,
  // is a genuine change, not an already-true no-op).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);
  CHECK(fx.open_row == 1);

  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);

  // Drums/scene-0 cell rect: draw_cell's fill for a filled, not-hovered,
  // not-playing cell is neon::u32(track_color, 0.10F) (grid_panel.cpp's
  // `fill_a` decision) -- clusters[0] is always drums/scene 0 (draw order,
  // same reasoning test_repeat_zone_playhead_ui_automation.cpp's own header
  // comment already documents for this exact color/row).
  const ImU32 drums_not_playing = sonotron::neon::u32(sonotron::theme::kV02TrackColor[0], 0.10F);
  const std::vector<th::Rect> drums_cells = th::find_color_clusters(locate, drums_not_playing);
  CHECK(!drums_cells.empty());
  if (drums_cells.empty()) {
    ImGui::DestroyContext();
    return;
  }
  const th::Rect scene0_drums_cell = drums_cells.front();

  // Real click #1: Play.
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    std::vector<BrainEvent> events;
    while (std::chrono::steady_clock::now() < deadline) {
      events.clear();
      session.poll(events);
      for (const BrainEvent& ev : events) {
        app_state.apply(ev);
      }
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);

  // Real click #2: launch the drums/scene-0 cell for real (grid_panel.cpp's
  // REAL render_track_cell click handler -- `launch clip 0 quantize 1`,
  // `fx.open_cell = 0`, `seqedit.set_part_index(0)`, `seqedit.set_clip_label
  // ("A")` -- cell_id(role_index=0, scene=0, scene_count=5) == 0, seed_demo's
  // own curated label for this cell is "A").
  constexpr int kDrumsScene0ClipId = 0;
  click_at(scene0_drums_cell.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // THE "OPEN IN SEQUENCE EDIT" ASSERTIONS: these fields are set
  // SYNCHRONOUSLY inside the SAME click frame by the real click handler --
  // no engine round trip needed to observe them, only the real render loop's
  // own real state mutation.
  CHECK(fx.open_cell == kDrumsScene0ClipId);
  CHECK(fx.open_row == 0);
  CHECK(!fx.open_audio);  // drums (kRows[0].audio == false)
  CHECK(fx.open_section == model.scene_section(0));
  CHECK(seqedit.part_index() == 0);  // kRows[0].role_index
  CHECK(seqedit.clip_label() == "A");

  // Pump the REAL per-frame pipeline until the REAL ClipMatrix round trip
  // confirms the launch (bounded wall time, same discipline every other
  // real-backend test in this directory already uses).
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    std::vector<BrainEvent> events;
    while (std::chrono::steady_clock::now() < deadline) {
      events.clear();
      session.poll(events);
      for (const BrainEvent& ev : events) {
        app_state.apply(ev);
      }
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.clip_state(kDrumsScene0ClipId) != AppState::ClipLaunchState::kStopped) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  session.stop();

  // THE "REAL LAUNCH" ASSERTION: the click's `launch clip 0 quantize 1` must
  // have actually addressed a REGISTERED ClipMatrix clip (dff4e9e's fix) --
  // not warned into the void.
  CHECK(app_state.clip_state(kDrumsScene0ClipId) != AppState::ClipLaunchState::kStopped);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_click_on_filled_cell_launches_and_opens_sequence_edit();
  return sonotron::test::failures();
}

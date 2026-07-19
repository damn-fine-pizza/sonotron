// UI-AUTOMATION acceptance test (docs/proposals/browser-redesign-taxonomy.md
// Phase 1, deliverable 2): "Sections/Variations: click -> apply. Wire a plain
// click on a variation leaf to send `style section <name>` directly." This
// clicks a REAL variation leaf row (browser_panel.cpp's leaf_row, inside
// render_variations) through a REAL InProcessBrainSession, and asserts a
// REAL, engine-observable section change (AppState::section(), the
// authoritative kSection echo) -- never a spy on the sent wire text, the same
// discipline test_browser_style_switch_while_playing_ui_automation.cpp
// already established for style leaves.
//
// Click and drag are NOT mutually exclusive (repeat-zone-real-contract.md
// SLICE 4a, kept unchanged by this slice) -- this test only pins the click
// path.
//
// CORRECTED (Torquato QA, roadmap node 14120 investigation, 2026-07-19): this
// comment used to claim the drag-to-scene-header path "stays covered by
// grid_panel.cpp's own drop-target tests". That claim was FALSE -- no such
// file ever existed, and a grep across every test file for the real ImGui
// drag-drop payload IDs (kStyleDragPayloadId, kVariationDragPayloadId,
// BeginDragDropSource, AcceptDragDropPayload) found zero hits before this
// pass. The only pre-existing "drag" mention near grid_panel.cpp's drop
// targets was test_repeat_zone_playhead_ui_automation.cpp's own root-cause
// PROSE comment, which explains that the browser drag-drop handler is the
// only path that registers a real ClipMatrix clip -- it never drove a drag
// gesture itself, real or simulated. Real drag coverage (both the style and
// the variation drag source/target pairs, driven through a genuine multi-
// frame BeginDragDropSource/AcceptDragDropPayload gesture) now lives in
// test_browser_grid_drag_drop_ui_automation.cpp.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/browser_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
#include "src/theme.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <chrono>
#include <thread>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrowserCategory;
using sonotron::BrowserModel;
using sonotron::InProcessBrainSession;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// Renders JUST the browser panel inside its own named child window -- a
// TEST-only wrapper mirroring test_browser_style_switch_while_playing_ui_
// automation.cpp's own render_one_frame (render_browser_panel itself is
// unchanged). No transport panel here: this test never needs a Play click.
ImDrawData* render_one_frame(BrowserModel& model, BrainSession& brain_session, AppState& app_state,
                             UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1280.0F, 800.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("browser_area", ImVec2(360.0F, 600.0F));
  sonotron::render_browser_panel(model, brain_session, app_state, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, BrowserModel& model, BrainSession& brain_session,
                     AppState& app_state, UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, brain_session, app_state, fx);
}

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

// Pumps the REAL per-frame pipeline (bounded wall time) until AppState's own
// section echo reaches `target`, mirroring the sibling style-switch test's
// own wait loop shape.
bool wait_for_section(BrainSession& session, AppState& app_state, BrowserModel& model, UiState& fx,
                      const char* target, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    poll_once(session, app_state);
    render_one_frame(model, session, app_state, fx);
    if (app_state.section() == target) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return app_state.section() == target;
}

void test_real_variation_leaf_click_sends_style_section() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  BrowserModel model;
  UiState fx;
  AppState app_state;

  // Setup through BrowserModel's own public API -- the SAME precedent
  // test_browser_style_switch_while_playing_ui_automation.cpp's own
  // model.set_search_filter("rock") call already established for narrowing:
  // hide every Styles leaf (so the always-visible Styles section contributes
  // no theme::kTextSecondary rows to the scan below) and reveal ONLY the
  // Variations category, narrowed to its single "bridge" row -- the only
  // kVariations entry containing that substring (browser_panel.cpp).
  model.set_search_filter("zzz-no-style-match");  // category defaults kStyles
  model.set_category_visible(BrowserCategory::kVariations, true);
  model.set_category(BrowserCategory::kVariations);
  model.set_search_filter("bridge");

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  fx.active_style = 0;
  // Baseline section, established WHILE STOPPED (immediate echo -- the SAME
  // signal test_browser_style_switch_while_playing_ui_automation.cpp already
  // relies on for `style section`) -- distinct from "varD", the wire name the
  // "bridge" row's own click handler sends (kVariationWireNames[4]), so a
  // later flip to "varD" can only be attributed to the real click below.
  session.send("style section varA");
  CHECK(wait_for_section(session, app_state, model, fx, "varA", 6000));

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, session, app_state, fx);
  ImDrawData* frame = render_one_frame(model, session, app_state, fx);

  const th::Rect browser_tree = th::find_child_window_rect("browser_tree");
  CHECK(browser_tree.found);
  const ImU32 leaf_text_color = sonotron::neon::u32(sonotron::theme::kTextSecondary);
  const std::vector<th::Rect> leaf_clusters = th::find_color_clusters(frame, leaf_text_color);
  th::Rect target_row;
  for (const th::Rect& r : leaf_clusters) {
    if (r.min.y >= browser_tree.min.y && r.max.y <= browser_tree.max.y) {
      target_row = r;
      break;
    }
  }
  CHECK(target_row.found);

  // Real click: the "bridge" variation leaf. browser_panel.cpp's
  // render_variations sends `style section varD` on a plain click -- never
  // hand-written here.
  click_at(target_row.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  CHECK(wait_for_section(session, app_state, model, fx, "varD", 6000));
  // THE ASSERTION: only a real click on the "bridge" row's real handler could
  // have produced this section change -- nothing in this test ever sends
  // `style section varD` directly.
  CHECK(app_state.section() == "varD");

  session.stop();
  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_variation_leaf_click_sends_style_section();
  return sonotron::test::failures();
}

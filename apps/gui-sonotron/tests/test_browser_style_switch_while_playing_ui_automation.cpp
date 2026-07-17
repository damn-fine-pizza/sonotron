// UI-AUTOMATION functional test (Torquato QA pass, flow-verification-matrix-
// 2026-07.md §2b): "Click a style leaf while playing -> `style switch
// <name>` (live morph, quantized)" -- the matrix's own verdict was "NONE.
// This is a DIFFERENT code path from `style load` (mid-song morph vs hard
// reload) and has zero coverage, functional or unit."
//
// This test clicks a REAL style leaf row (browser_panel.cpp's leaf_row,
// inside render_styles) while the transport is REALLY playing, through a
// REAL InProcessBrainSession, and distinguishes "style switch" from "style
// load" via a REAL, ENGINE-OBSERVABLE difference (never a spy on the sent
// wire text): `style switch <name>` (in_process_brain_session.cpp) always
// targets `SectionType::kVarA` and rides Boundary::kNextBar -- while playing
// and already loaded, Engine::style_switch (engine.cpp) queues it
// (immediate == false) and it lands, one bar later, through the SAME
// fire_arranger `section_changed` -> `OutEvent::section(...)` path
// AppState::section() decodes. `style load <name>`, in contrast,
// (Engine::cmd_style's kStyleLoad case) NEVER emits an OutEvent::section at
// all, at any bar, playing or not -- so if this flow regressed to
// unconditionally sending `style load` (ignoring `fx.playing`), AppState::
// section() would simply never move off the pre-established baseline within
// this test's wall-clock budget, a real, observable divergence.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/browser_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
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
using sonotron::BrowserModel;
using sonotron::InProcessBrainSession;
using sonotron::V02State;
namespace th = sonotron::test_harness;

namespace {

// Renders the transport panel (for the real Play click) and the browser
// panel INSIDE its own named child window, so the browser's own rows can be
// spatially isolated from the transport panel's chrome (both use theme::
// kTextSecondary for some of their own text, e.g. the transport inset's
// "4/4" label) when scanning for color clusters below -- a TEST-only
// wrapper, not a product change (render_browser_panel itself is unchanged).
ImDrawData* render_one_frame(BrowserModel& model, BrainSession& brain_session, AppState& app_state,
                             V02State& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  // Explicit size (matching main.cpp's own SetNextWindowSize(viewport->
  // WorkSize)): without it ImGui's small first-use default leaves the outer
  // "test" window's own remaining content region too small, and the nested
  // "browser_area" child inherits window->SkipItems from that starved outer
  // layout even though ITS OWN requested size (360x600) is set explicitly --
  // found while writing this test (also pinned as a pre-existing gap in the
  // sibling test_repeat_zone_playhead_ui_automation.cpp, fixed there too).
  ImGui::SetNextWindowSize(ImVec2(1280.0F, 800.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  sonotron::render_transport_panel(app_state, brain_session, fx);
  ImGui::Spacing();
  ImGui::BeginChild("browser_area", ImVec2(360.0F, 600.0F));
  sonotron::render_browser_panel(model, brain_session, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, BrowserModel& model, BrainSession& brain_session,
                     AppState& app_state, V02State& fx) {
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

void test_real_style_leaf_click_while_playing_sends_switch_not_load() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  BrowserModel model;
  V02State fx;
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  fx.active_style = 0;  // "basic" is index 0 (kBuiltinStyleNames), same as main.cpp's own boot
  session.send("bpm 400");  // shrink the wall-clock bar cadence (~0.6s/bar vs ~2s at 120 BPM)
  // Baseline section, established WHILE STOPPED (immediate echo): distinct
  // from "varA", the fixed target `style switch` always requests
  // (in_process_brain_session.cpp), so a later flip to "varA" can only be
  // attributed to a real style-switch landing, never to this baseline.
  session.send("style section varB");

  // Warm-up + locate frame.
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, session, app_state, fx);

  const th::Rect browser_area = th::find_child_window_rect("browser_area");
  CHECK(browser_area.found);

  // Style rows sit shoulder to shoulder (no other-colored geometry drawn
  // between consecutive labels), so find_color_clusters' vertex-index-gap
  // heuristic (imgui_headless_harness.hpp, default index_gap=16) merges many
  // adjacent kTextSecondary rows into ONE blob instead of one cluster per
  // row (confirmed empirically while writing this test: clusters[0] alone
  // spanned ~13 rows' worth of Y). A dense, evenly-spaced list needs exact
  // layout math instead: "basic" (index 0, the pre-established active style)
  // is the ONLY style row painted in theme::kCyan (leaf_row's `active`
  // branch, browser_panel.cpp) plus its own left accent bar -- a real,
  // uniquely-colored anchor -- and every subsequent row sits exactly one
  // real ImGui::GetTextLineHeightWithSpacing() below the previous one
  // (leaf_row emits one Selectable per row, nothing else, so this is the
  // literal per-row advance the Selectable layout itself uses -- not a
  // guessed constant). theme::kCyan (full alpha) is ALSO used elsewhere in
  // THIS SAME frame (the "sonotron" title text and a same-row indicator, both
  // in the transport panel, confirmed empirically) -- find_single_color_rect
  // would merge those in too, so this filters clusters to the browser_area
  // child's own Y range first, exactly like the row-locating attempt above.
  const ImU32 active_style_text = sonotron::neon::u32(sonotron::theme::kCyan);
  const std::vector<th::Rect> cyan_clusters = th::find_color_clusters(locate, active_style_text);
  th::Rect basic_row;
  for (const th::Rect& r : cyan_clusters) {
    if (r.min.y >= browser_area.min.y && r.max.y <= browser_area.max.y) {
      basic_row = r;
      break;
    }
  }
  CHECK(basic_row.found);
  const float row_height = ImGui::GetTextLineHeightWithSpacing();
  constexpr int kTargetStyleIndex = 2;  // "rock" (kBuiltinStyleNames[2])
  th::Rect target_row;
  target_row.min = ImVec2(basic_row.min.x,
                          basic_row.min.y + static_cast<float>(kTargetStyleIndex) * row_height);
  target_row.max = ImVec2(basic_row.max.x, target_row.min.y + row_height);
  target_row.found = true;

  // Real click: Play.
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);
  click_at(play_rect.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.section() == "varB") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.section() == "varB");

  // Real click: the target style leaf, WHILE PLAYING (browser_panel.cpp's
  // REAL click handler -- `fx.playing ? "style switch " + name : "style
  // load " + name`, never hand-written here).
  click_at(target_row.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // The click's own immediate, GUI-side echo: confirms we actually clicked
  // the intended row (real production field, not an assumption).
  CHECK(fx.active_style == kTargetStyleIndex);

  // Pump the REAL per-frame pipeline (bounded wall time) until the section
  // readback flips to "varA" -- the fixed target ONLY a real `style switch`
  // ever lands (see this file's own header comment for why `style load`
  // could never produce this signal).
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, session, app_state, fx);
      if (app_state.section() == "varA") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  session.stop();

  // THE ASSERTION: only a real `style switch` (not `style load`) could have
  // produced this section-readback flip while the transport stayed playing
  // throughout.
  CHECK(app_state.section() == "varA");

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_style_leaf_click_while_playing_sends_switch_not_load();
  return sonotron::test::failures();
}

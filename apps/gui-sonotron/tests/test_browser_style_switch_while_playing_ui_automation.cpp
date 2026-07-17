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
// wire text).
//
// REWRITTEN for owner task #2 (preserve the active section across a live
// switch instead of always reverting to varA): the OLD version of this test
// asserted the section readback FLIPPED to "varA" after the switch -- that
// signal is gone by design now, because browser_panel.cpp's render_styles
// passes the engine's own CURRENT section (AppState::section()) as an
// explicit `style switch <name> section <current>` suffix
// (in_process_brain_session.cpp), so the switch lands on the SAME section it
// started from, not varA.
//
// The new, still-real signal: `style_switched=true` forces the arranger's own
// TickResult.section_changed=true regardless of whether the section VALUE
// actually changed (arranger.hpp's `if (next != m_current || style_switched)`
// -- confirmed by reading the arranger source, not assumed), so a genuine
// `OutEvent::section(...)` -> AppState::apply -> a NEW "section varB" line in
// AppState::log() (app_state.cpp's format_log_line) still fires one bar
// later, even though the section VALUE never moves off varB. `style load
// <name>` (Engine::cmd_style's kStyleLoad case), in contrast, NEVER emits an
// OutEvent::section at all, at any bar -- so counting "section varB" log-line
// OCCURRENCES before/after the click (rather than checking the live value,
// which never changes) still distinguishes a genuine switch from a load: a
// load would leave the count frozen forever; a switch adds a later
// occurrence.

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
#include "src/ui_state.hpp"

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
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// Renders the transport panel (for the real Play click) and the browser
// panel INSIDE its own named child window, so the browser's own rows can be
// spatially isolated from the transport panel's chrome (both use theme::
// kTextSecondary for some of their own text, e.g. the transport inset's
// "4/4" label) when scanning for color clusters below -- a TEST-only
// wrapper, not a product change (render_browser_panel itself is unchanged).
ImDrawData* render_one_frame(BrowserModel& model, BrainSession& brain_session, AppState& app_state,
                             UiState& fx) {
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

// `section_varb_events` counts every REAL kSection("varB") event this test
// observes, at the BrainEvent granularity, as it is decoded -- NOT via
// AppState::log() (app_state.hpp's own m_log is a display-only, CAPPED ring
// buffer, kMaxLog == 200 lines, and this test's own bpm-400 run floods it
// with far more than 200 "beat"/"midi-out" lines in the time it takes a
// queued style switch to land one bar later; the ORIGINAL baseline "section
// varB" line is long evicted by the time the switch's own line would land,
// which silently broke a first draft of this test's own log-line-count
// technique -- found and fixed while writing this test). Counting at the
// event source, before AppState ever caps or evicts anything, is immune to
// that.
void poll_once(BrainSession& session, AppState& app_state, int& section_varb_events) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    if (ev.kind == BrainEvent::Kind::kSection && ev.section_name == "varB") {
      ++section_varb_events;
    }
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
  UiState fx;
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  fx.active_style = 0;      // "basic" is index 0 (kBuiltinStyleNames), same as main.cpp's own boot
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

  // Family-grouped rendering (task #30, docs/proposals/style-browser-corpus-
  // scale.md) breaks the OLD arithmetic-offset locate strategy this test used
  // to rely on: "basic" (index 0, kOther) and "rock" (index 2,
  // kPopRockBallad) no longer render as two of three CONSECUTIVE, unbroken
  // rows -- they now sit under DIFFERENT family section headers, with other
  // families' headers (and rows) in between. Narrowing the browser's own
  // search filter to "rock" instead is precise and stable: browser_model.hpp
  // documents that "rock" is the only style name, AND the only family-label
  // fragment, that contains the substring "rock" among all 16 built-ins
  // (kPopRockBallad's own label was deliberately chosen as "Pop / Ballad",
  // NOT "Pop / Rock / Ballad", to keep exactly this kind of narrowing
  // precise) -- so after applying it, exactly one style leaf renders
  // anywhere in the whole browser tree (variations/kits share the same text
  // filter and neither list contains "rock" either). That lone leaf is not
  // the active style ("basic" is, and "basic" is now hidden by the filter),
  // so it paints in the same un-highlighted theme::kTextSecondary every
  // non-active leaf uses -- locate it exactly like this file used to locate
  // the cyan "basic" anchor (find_color_clusters, filtered to the
  // browser_area child's own Y range: theme::kTextSecondary is also used
  // elsewhere in this same frame, e.g. the transport inset's "4/4" label, so
  // the Y-range filter still matters here too). With only one leaf on
  // screen, find_color_clusters' vertex-index-gap merging heuristic (which
  // defeated a naive per-row-cluster approach when EVERY style rendered
  // shoulder to shoulder) is no longer a concern -- there is nothing
  // adjacent left to merge with.
  model.set_search_filter("rock");
  ImDrawData* filtered = render_one_frame(model, session, app_state, fx);
  const ImU32 leaf_text_color = sonotron::neon::u32(sonotron::theme::kTextSecondary);
  const std::vector<th::Rect> leaf_clusters = th::find_color_clusters(filtered, leaf_text_color);
  th::Rect target_row;
  for (const th::Rect& r : leaf_clusters) {
    if (r.min.y >= browser_area.min.y && r.max.y <= browser_area.max.y) {
      target_row = r;
      break;
    }
  }
  CHECK(target_row.found);
  constexpr int kTargetStyleIndex = 2;  // "rock" (kBuiltinStyleNames[2])

  // Real click: Play.
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);
  click_at(play_rect.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  int section_varb_events = 0;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state, section_varb_events);
      render_one_frame(model, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.section() == "varB") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.section() == "varB");
  // Baseline event count: exactly the one kSection("varB") event decoded
  // above while establishing the baseline (immediate echo, transport was
  // stopped at the time).
  CHECK(section_varb_events == 1);

  // Real click: the target style leaf, WHILE PLAYING (browser_panel.cpp's
  // REAL click handler -- `fx.playing ? "style switch " + name + " section "
  // + app_state.section() : "style load " + name`, never hand-written here).
  click_at(target_row.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // The click's own immediate, GUI-side echo: confirms we actually clicked
  // the intended row (real production field, not an assumption).
  CHECK(fx.active_style == kTargetStyleIndex);

  // Pump the REAL per-frame pipeline (bounded wall time) until a SECOND
  // kSection("varB") event is decoded -- only a real `style switch` (queued
  // to the next bar boundary, Engine::style_switch) forces the arranger's own
  // section_changed=true and re-emits the OutEvent::section(...) this test
  // observes; `style load` never emits one at all, at any bar (see this
  // file's own header comment).
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state, section_varb_events);
      render_one_frame(model, session, app_state, fx);
      if (section_varb_events > 1) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  session.stop();

  // THE ASSERTION: only a real `style switch` (not `style load`) could have
  // produced a second kSection("varB") event while the section's own VALUE
  // never moved -- the section-value-preserving fix (owner task #2) means the
  // live value alone is no longer an observable signal, so this counts the
  // genuine re-emission instead.
  CHECK(section_varb_events > 1);
  // Sanity: the section truly never moved off the baseline throughout.
  CHECK(app_state.section() == "varB");

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_style_leaf_click_while_playing_sends_switch_not_load();
  return sonotron::test::failures();
}

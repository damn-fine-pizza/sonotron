// UI-AUTOMATION functional test (Torquato QA pass, flow-verification-matrix-
// 2026-07.md §2c): "Auto-song bookkeeping surviving a transport stop/restart
// (bar rewinds to 0)" -- the matrix's own verdict was "No real-backend
// equivalent exists yet for the stop/restart case specifically (the
// real-backend test [test_grid_panel_auto_song_real_backend.cpp] only
// exercises the 'never stopped' happy path). Gap: hand to Torquato -- extend
// ... to send `transport stop` then a fresh `transport start` through the
// real backend and assert the anchor re-seats correctly."
//
// This test goes one step further than that gap description asks for real
// input fidelity: the Play/Stop/Play sequence below is driven through REAL
// mouse clicks on the REAL transport pad buttons (imgui_headless_harness.hpp)
// rather than hand-written `session.send("transport start")` calls, closing
// the residual click-injection gap the EXISTING real-backend test's own
// header comment leaves open ("press Play" there is still a literal send()
// call, not a click).
//
// UPDATED (Torquato QA, roadmap node 14120 investigation): this test used to
// carry a `click_arm_auto_song` helper that hand-wrote `fx.auto_song = true`
// to stand in for a header-toggle click, because the OLD per-frame FSM
// (grid_panel.cpp's now-fully-retired update_auto_song) never armed itself
// on its own. That helper is gone. Two things changed underneath it:
//   1. `auto_song` now DEFAULTS to true (ui_state.hpp, owner decision
//      2026-07-17) -- there is nothing left to "arm"; every fresh UiState
//      already starts in the state the old helper used to fake.
//   2. A real click on the header toggle when auto_song is ALREADY true
//      would TOGGLE IT OFF (grid_panel.cpp render_header's `fx.auto_song =
//      !fx.auto_song`), the opposite of what the old helper simulated -- so
//      converting the call into a real click would have made this test
//      click the WRONG direction, not a more faithful one.
// The CHECK(fx.auto_song) below now stands in the old helper's place: it
// documents and pins the precondition the rest of the test relies on
// (auto-song is armed from frame 0, no click required to reach that state)
// instead of hand-writing it.
//
// Root cause under test: the CURRENT mechanism (Song-mode Phase 1,
// grid_panel.cpp's handle_master_play_launch + reconcile_active_scene)
// rebuilds the whole SceneChain from scratch on every fresh Play
// (fx.master_play_launched re-arms the instant the transport is observed
// NOT playing) rather than carrying forward any stale bar-anchored
// bookkeeping across a stop/restart cycle -- this test proves the song
// genuinely advances AGAIN after a real stop/restart, through real clicks
// end to end, which is exactly what the old bar-rewind-guard FSM this test
// was originally written against also had to prove for its own (now
// retired) mechanism.

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

ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, UiState& fx) {
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

void test_auto_song_advances_again_after_a_real_stop_restart_cycle() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  model.set_scene_bars(0, 1);  // scene 0's own length: 1 bar, so the FIRST advance is fast
  model.set_scene_bars(1, 1);  // scene 1's own length: 1 bar, so the SECOND advance is fast too
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;
  // Song-mode Phase 1 precondition (see this file's own header comment):
  // auto-song starts ARMED by default -- nothing needs to click the header
  // toggle to reach this state, so this CHECK stands in for the old
  // click_arm_auto_song helper's field write.
  CHECK(fx.auto_song);

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
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);
  const ImU32 stop_border_color = sonotron::neon::u32(sonotron::theme::kTextSecondary, 0.6F);
  const th::Rect stop_rect = th::find_single_color_rect(locate, stop_border_color);
  CHECK(stop_rect.found);

  // Real click: Play (round 1).
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  bool armed = false;
  int max_bar_seen_round1 = 0;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      max_bar_seen_round1 = std::max(max_bar_seen_round1, app_state.bar());
      if (!armed && app_state.transport() == AppState::Transport::kPlaying) {
        // No click needed here: auto_song was already true from frame 0
        // (CHECK'd above), so the transport reporting "playing" is the only
        // real-world signal left to wait for -- handle_master_play_launch
        // (grid_panel.cpp) already built and launched the song on this same
        // transition, through production code, not this test.
        armed = true;
      }
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (armed && fx.active_scene != 0) {
        break;  // the FIRST advance landed
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(armed);
  CHECK(max_bar_seen_round1 > 0);
  // Sanity precondition for the real finding below: auto-song must actually
  // advance at least once before we can meaningfully test surviving a
  // restart.
  CHECK(fx.active_scene != 0);
  const int scene_after_round1 = fx.active_scene;

  // Real click: Stop (transport_panel.cpp's REAL button verb). Wait for BOTH
  // readbacks together, not just the first to flip: app_state.hpp's own
  // note_transport_sent(false) (the Stop button's own click verb) sets
  // m_transport = kStopped OPTIMISTICALLY the instant the click is drawn,
  // WITHOUT touching m_bar -- only the REAL kTransport("stopped") OutEvent's
  // reduction zeroes it. Breaking on transport()==kStopped alone races that
  // optimistic hint and fails spuriously before the real confirmation lands
  // (found and confirmed empirically while writing the sibling test_
  // transport_play_stop_ui_automation.cpp: the real event lands within one
  // extra poll cycle once actually waited for).
  click_at(stop_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kStopped && app_state.bar() == 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kStopped);
  CHECK(app_state.bar() == 0);  // app_state.cpp's own "stopped" reduction parks the bar at 0

  // Real click: Play again (round 2) -- the core's Transport rewinds its own
  // bar counter to 0 on every fresh Start (runtime/transport.hpp), which is
  // exactly the stale-anchor scenario update_auto_song's bar-rewind guard
  // exists for.
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  int max_bar_seen_round2 = 0;
  bool advanced_again = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      max_bar_seen_round2 = std::max(max_bar_seen_round2, app_state.bar());
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      // Wait for a REAL flip, not just any single frame's read: task #36's
      // fix (grid_panel.cpp's handle_master_play_launch, gated on
      // fx.auto_song) now resets fx.active_scene to 0 on the SAME frame Play
      // is processed, on EVERY fresh Play -- including this round 2 one. That
      // reset alone satisfies `fx.active_scene != scene_after_round1`
      // immediately, before poll_once has ever observed a single real bar
      // advance in round 2 -- breaking on that condition ALONE used to exit
      // this loop on its very first iteration with max_bar_seen_round2 still
      // 0, failing the bar-climbed assertion below for a reason that has
      // nothing to do with this test's own root cause (auto-song surviving a
      // stop/restart). Requiring max_bar_seen_round2 > 0 too -- the same
      // "wait for a real flip, not the earliest possible read" idiom
      // test_repeat_zone_scene_header_next_bar_ui_automation.cpp already
      // uses -- keeps this loop honestly waiting for the SECOND, auto-song-
      // driven advance this test actually pins, not the task #36 reset.
      if (fx.active_scene != scene_after_round1 && max_bar_seen_round2 > 0) {
        advanced_again = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  session.stop();

  // THE ASSERTION (owner symptom): the bar must genuinely climb again after
  // the restart (the core really did rewind and resume, not stay stuck).
  CHECK(max_bar_seen_round2 > 0);
  // THE PINNED ASSERTION: auto-song must advance AGAIN after a real
  // stop/restart cycle -- if `active_scene_start_bar`'s stale pre-stop anchor
  // were never re-seated, `bars_elapsed` would go deeply negative and stay
  // there, and the active scene would never move off `scene_after_round1`
  // for the rest of this run.
  CHECK(advanced_again);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_auto_song_advances_again_after_a_real_stop_restart_cycle();
  return sonotron::test::failures();
}

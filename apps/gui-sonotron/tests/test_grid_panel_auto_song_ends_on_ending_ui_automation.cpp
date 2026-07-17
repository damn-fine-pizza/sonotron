// UI-AUTOMATION ACCEPTANCE TEST (tasks #27/#12, docs/proposals/song-form-
// option-a-wiring-plan.md §2): song-form Option A -- a non-wrapping song that
// ends on an Ending, driven ENTIRELY by auto-song, with NO click on the
// dedicated ENDING transport pad at any point. This is the automatic
// counterpart to test_transport_ending_button_ui_automation.cpp's manual
// click path: that file proves a human pressing END cues the ending; this
// file proves the song reaching its own last authored column does the exact
// same thing by itself.
//
// The mechanism under test (grid_model.cpp/.hpp, grid_panel.cpp):
//   1. next_scene_to_launch's terminal case now HOLDS (nullopt) at the last
//      scene column instead of wrapping back to 0.
//   2. auto_song_reached_song_end mirrors that same guard order, narrowing
//      to "AND it is the last column", so update_auto_song's own nullopt
//      branch can tell "genuinely finished the song" apart from any of
//      next_scene_to_launch's other nullopt reasons.
//   3. update_auto_song then sends the bare `style section ending1` verb and
//      sets fx.ending_cued, exactly mirroring transport_panel.cpp's own
//      render_ending_pad (roadmap task #37) -- so from that point on, the
//      SAME already-tested core one-shot rule (Arranger::on_tick's
//      section_is_ending branch, proven by test_ending_stops_transport,
//      components/core/arrangrr/tests/test_arranger.cpp) stops the
//      transport by itself.
//
// GridModel(5) deliberately mirrors main.cpp's own demo boot (5 columns,
// Intro/VarA/VarB/VarC/VarD, no Ending column authored) -- this is
// DELIBERATE: the mechanism must cue the Ending regardless of what section
// the last authored column carries, not rely on content authoring (the demo
// grid's last column is a plain VarD, never an Ending itself).
//
// Real-vs-stubbed tag (flow-verification-matrix-2026-07.md §1 convention):
// `_ui_automation` == real click injection (Play only, here) + real
// InProcessBrainSession + real rendered-output/engine readback.

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
// directory shares (mirrors test_transport_ending_button_ui_automation.cpp).
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

// -----------------------------------------------------------------------
// Song-form Option A end-to-end: real Play click, no other click. Drives the
// demo grid through Intro->A->B->C->D purely via auto-song, asserts it holds
// on the last column (never wraps back to scene 0), asserts the automatic
// cue reaches "ending1" on the real engine readback, and asserts the core's
// own one-shot rule stops the transport by itself.
// -----------------------------------------------------------------------
void test_auto_song_holds_at_last_column_and_ends_on_ending_without_any_click() {
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

  // Real click: Play. This is the ONLY click in this whole test -- everything
  // downstream must be driven purely by auto-song.
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

  // GIVEN: auto_song is armed by default -- confirmed without clicking
  // anything (UiState::auto_song defaults to true, owner decision
  // 2026-07-17).
  CHECK(fx.auto_song);

  // Poll until the song has auto-advanced all the way to the last demo
  // column (index 4 of GridModel(5)) -- proves the automatic advance is
  // really moving the song forward on its own.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (fx.active_scene == 4) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(fx.active_scene == 4);

  // NO-WRAP REGRESSION GUARD: from the moment scene 4 was first observed,
  // never observe scene 0 again while the transport is still playing -- the
  // old (pre-Option-A) behavior would wrap right back to 0 here.
  bool saw_wrap_to_zero = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && fx.active_scene == 0) {
        saw_wrap_to_zero = true;
      }
      if (app_state.section() == "ending1") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  // THE ASSERTION (the automatic cue landed, purely from auto-song, no
  // click on the ENDING pad anywhere in this test): the real engine's own
  // section readback, never a mirror of a local flag.
  CHECK(app_state.section() == "ending1");
  CHECK(!saw_wrap_to_zero);

  // Wait for the core's own one-shot rule to stop the transport BY ITSELF
  // (already proven core-side by test_ending_stops_transport) -- never a
  // click, never a host-side send() of "transport stop".
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && fx.active_scene == 0) {
        saw_wrap_to_zero = true;
      }
      if (app_state.transport() == AppState::Transport::kStopped && app_state.bar() == 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  session.stop();

  CHECK(!saw_wrap_to_zero);
  // app_state.cpp's "stopped" reduction never touches m_section, so this
  // still reads "ending1" after the stop (same reasoning the sibling manual-
  // click test documents).
  CHECK(app_state.section() == "ending1");
  CHECK(app_state.transport() == AppState::Transport::kStopped);
  CHECK(app_state.bar() == 0);
  // The active-scene bookkeeping never got reset off the last column.
  CHECK(fx.active_scene == 4);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_auto_song_holds_at_last_column_and_ends_on_ending_without_any_click();
  return sonotron::test::failures();
}

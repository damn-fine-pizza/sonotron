// FIX PIN, now GREEN (Torquato QA, root-caused via a live
// SONOTRON_AUTOSONG_TRACE stderr trace, since removed): "scene 1 forever" bug
// in the live GUI was NOT a decision-logic bug -- fx.active_scene DID advance
// internally (0->1->2->3->4->0, confirmed by the trace). The real defect was
// that grid_panel.cpp's file-local update_auto_song advance branch sent ONLY
// `style section <name>` on advance -- it never sent `launch scene <n>
// quantize <q>`. So the next scene column's clips never actually fired: the
// section changed but no clip launch followed it, and the audio never
// changed. Contrast with the MANUAL scene-header ▶ launch
// (render_scene_header_cell), which correctly sends BOTH `style section
// <name>` AND `launch scene <s> quantize <bars>` -- auto-song's own advance
// now mirrors that exactly (Nazzareno's fix, grid_panel.cpp's
// update_auto_song).
//
// This test drives the REAL auto-song advance through the REAL
// render_grid_panel entry point (same headless-ImGui harness as
// test_grid_panel_auto_song.cpp: SpyBrainSession + render_one_frame +
// click_arm_auto_song, reproduced here rather than shared because this
// translation unit intentionally stays a single, self-contained repro of the
// missing-launch defect). It asserts that among the sent command lines there
// is one starting with "launch scene " after an auto-song advance fires.
//
// The active scene's own length is pinned short via GridModel::
// set_scene_bars (the new per-scene length the advance decision now reads,
// grid_model.hpp/.cpp -- NOT the old preview::section_bars style-section
// fallback this file used to rely on), so a single further bar deterministic-
// ally crosses the boundary without depending on any built-in style's own
// section length.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/parts_model.hpp"
#include "src/seqedit_model.hpp"
#include "src/v02_state.hpp"

#include "test.hpp"

#include <string>
#include <string_view>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrainSnapshot;
using sonotron::GridModel;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::V02State;

namespace {

// Spy BrainSession (test double, not a product dependency): records every
// sent command line instead of talking to a real transport, exactly like
// test_grid_panel_auto_song.cpp's own SpyBrainSession.
class SpyBrainSession : public BrainSession {
 public:
  void send(std::string_view command_line) override { sent.emplace_back(command_line); }
  void poll(std::vector<BrainEvent>&) override {}
  const BrainSnapshot& snapshot() const override { return m_snapshot; }
  Status status() const override { return Status::kConnected; }

  std::vector<std::string> sent;

 private:
  BrainSnapshot m_snapshot;
};

// One headless ImGui frame around render_grid_panel, mirroring the real
// per-frame shape (layout_renderer.cpp refreshes fx.playing from
// app_state.transport() right before the grid panel call).
void render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                      BrainSession& brain_session, const AppState& app_state, V02State& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::Begin("test");
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::EndFrame();
}

// Mimics the exact state mutation the "auto-song" header button click
// performs (grid_panel.cpp render_header): arms auto-song and resets both
// bookkeeping bars to the current live bar.
void click_arm_auto_song(V02State& fx, const AppState& app_state) {
  fx.auto_song = true;
  fx.active_scene_start_bar = app_state.bar();
  fx.auto_song_last_bar = app_state.bar();
}

bool any_sent_line_starts_with(const std::vector<std::string>& sent, std::string_view prefix) {
  for (const std::string& line : sent) {
    if (line.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

// THE FIXED BUG: auto-song advances the active scene column (this part
// already worked, per the live trace) and now ALSO fires a `launch scene`
// command for the newly-active column, so the next column's clips actually
// play. This reproduces the exact single-crossing advance already
// established by test_auto_song_advances_scene_after_section_elapses in
// test_grid_panel_auto_song.cpp (the active scene's length is pinned to 1 bar
// via set_scene_bars so a single further bar crosses the boundary
// deterministically), then adds the assertion that specifies the full
// contract: not just that `style section` was sent, but that a
// `launch scene <n> ...` command was ALSO sent for the new active scene.
void test_auto_song_advance_must_launch_next_scene_clips() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  // Headless font atlas build, same as test_grid_panel_auto_song.cpp: no
  // renderer backend is ever attached, no pixel is ever rendered.
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  SeqEditModel seqedit;
  PartsModel parts;
  SpyBrainSession brain;
  AppState app_state;
  V02State fx;

  // Scene 0 (the active scene at arm time) gets a short, explicit 1-bar
  // length so the advance boundary is crossed by exactly one further bar,
  // independent of any built-in style's own section length.
  model.set_scene_bars(0, 1);

  // Transport starts playing; first beat lands at bar 1.
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":1,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 1);
  CHECK(app_state.transport() == AppState::Transport::kPlaying);

  // Arm auto-song at bar 1, scene 0 active.
  click_arm_auto_song(fx, app_state);
  CHECK(fx.auto_song);
  CHECK(fx.active_scene == 0);

  // Same-bar render: nothing has elapsed yet, no advance, no send.
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(brain.sent.empty());

  // One further bar crosses the 1-bar section boundary for the active scene:
  // this must trigger exactly one advance.
  app_state.apply_line(R"({"ev":"beat","bar":2,"beat":0,"pulse":0,"@":500})");
  CHECK(app_state.bar() == 2);
  render_one_frame(model, seqedit, parts, brain, app_state, fx);

  // Half of the contract (already pinned green by
  // test_grid_panel_auto_song.cpp): the active scene column DOES advance,
  // and a `style section` send DOES fire for it.
  CHECK(fx.active_scene == 1);
  CHECK(any_sent_line_starts_with(brain.sent, "style section "));

  // THE MISSING HALF OF THE CONTRACT, THE ONE THAT ACTUALLY MAKES THE AUDIO
  // CHANGE: a `launch scene <n> quantize <q>` command must ALSO have been
  // sent for the newly-active scene column (1), exactly like the manual
  // scene-header ▶ launch does (render_scene_header_cell, grid_panel.cpp).
  // Without this send, the section changes silently but no clip in the new
  // column ever actually fires -- "scene 1 forever" from the listener's
  // point of view, even though fx.active_scene has moved. THIS IS THE
  // ASSERTION THAT FAILS ON THE CURRENT TREE.
  CHECK(any_sent_line_starts_with(brain.sent, "launch scene "));

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_auto_song_advance_must_launch_next_scene_clips();
  return sonotron::test::failures();
}

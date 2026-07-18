// FUNCTIONAL ACCEPTANCE TESTS (Nazzareno, task #5 Phase-1): hardens the
// per-section REPEAT COUNT contract added on top of song-mode-scenechain-
// adoption.md's own Phase 1 machinery -- host-only, ZERO ABI/core change.
// Mirrors test_song_mode_scenechain_contract.cpp's own real-backend, real-
// engine-thread, RecordingBrainSession-decorator shape exactly (same
// "real production code, thin recording wrapper around send()" discipline,
// same reason: apply_song_build (in_process_brain_session.cpp) is anonymous-
// namespace/engine-thread-local and has no seam a test translation unit can
// call directly, so the wire-level transcript + real bar-timing readback are
// the closest-to-the-metal observable proxies available). Also mirrors that
// sibling file's own GridModel(N) shape: `render_grid_panel`'s seed_demo()
// (grid_panel.cpp) unconditionally assigns scenes 0..4 the fixed sections
// intro1/varA/varB/varC/varD (and populates their cells) on the FIRST
// rendered frame -- so, exactly like that sibling test, this file sets
// scene_bars/scene_repeat directly (seed_demo never touches either) but
// lets seed_demo own scene_section/scene_name, and sizes each GridModel to
// only as many scenes as the test actually exercises.
//
// Three claims are proven here, each an ANTI-NO-OP pin against the
// expansion in apply_song_build genuinely being a real K-times/infinite-hold
// mechanism, not a field that is parsed and then ignored:
//   1. build_and_play_song (grid_panel.cpp) really serializes each populated
//      column's own GridModel::scene_repeat into the `song build ...` wire
//      line, as a plain decimal token for a finite count and the literal
//      "inf" token for GridModel::kSceneRepeatInfinite.
//   2. a scene with repeat=K genuinely holds for K*n_bars real bars (not
//      just n_bars) before the chain advances to the next scene -- proven by
//      real bar-timing readback, not just wire-text inspection: if apply_
//      song_build's expansion loop were a no-op (always emitting exactly one
//      kSceneAdd per scene regardless of `repeat`), this would fail at
//      bars_elapsed == 1, not >= 3.
//   3. an infinite scene (repeat="inf") holds forever -- the chain never
//      reaches the next scene even after many real bars -- until an explicit
//      resume gesture (the SAME `song build 1 <section> <bars> 1` line
//      activate_scene_column sends for a manual scene-header click,
//      grid_panel.cpp) rebuilds the chain and lands on the target section.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/parts_model.hpp"
#include "src/seqedit_model.hpp"
#include "src/ui_state.hpp"

#include "test.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrainSnapshot;
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::UiState;

namespace {

// Same thin recording DECORATOR as test_song_mode_scenechain_contract.cpp's
// own RecordingBrainSession: every send() is forwarded verbatim to a real
// InProcessBrainSession, and also appended to `sent`, giving the test a
// ground-truth transcript of every wire line the production code really
// sent.
class RecordingBrainSession : public BrainSession {
 public:
  explicit RecordingBrainSession(InProcessBrainSession& real) : m_real(real) {}

  void send(std::string_view command_line) override {
    sent.emplace_back(command_line);
    m_real.send(command_line);
  }
  void poll(std::vector<BrainEvent>& out) override { m_real.poll(out); }
  const BrainSnapshot& snapshot() const override { return m_real.snapshot(); }
  Status status() const override { return m_real.status(); }

  std::vector<std::string> sent;

 private:
  InProcessBrainSession& m_real;
};

void render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                      BrainSession& brain_session, const AppState& app_state, UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::Begin("test");
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::EndFrame();
}

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

// -----------------------------------------------------------------------
// Claim 1: the wire line really carries a repeat token per populated scene
// -- a plain decimal for a finite count, "inf" for GridModel::
// kSceneRepeatInfinite. GridModel(3): seed_demo assigns scenes 0/1/2 the
// fixed sections intro1/varA/varB and populates all three (kDemoCell
// pattern has at least one entry per scene index < 3).
// -----------------------------------------------------------------------
void test_song_build_wire_line_carries_repeat_tokens() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(3);
  model.set_scene_bars(0, 1);
  model.set_scene_repeat(0, 1);
  model.set_scene_bars(1, 1);
  model.set_scene_repeat(1, 3);
  model.set_scene_bars(2, 1);
  model.set_scene_repeat(2, GridModel::kSceneRepeatInfinite);
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession real_session;
  RecordingBrainSession session(real_session);
  CHECK(real_session.start());
  session.send("style load basic");

  session.send("transport start");
  app_state.note_transport_sent(true);
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  real_session.stop();

  std::string song_build_line;
  for (const std::string& line : session.sent) {
    if (line.rfind("song build ", 0) == 0) {
      song_build_line = line;
      break;
    }
  }
  CHECK(song_build_line == "song build 3 intro1 1 1 varA 1 3 varB 1 inf");

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// Claim 2: a scene with repeat=K really holds for K*n_bars real bars, not
// n_bars -- the ANTI-NO-OP pin against apply_song_build's own expansion loop
// silently degrading to "always exactly one kSceneAdd per scene". Measures
// the varA (scene 1) -> varB (scene 2) transition only -- both plain
// sections with no one-shot content override (unlike scene 0's own intro1,
// see test_song_mode_scenechain_contract.cpp's own header comment for why
// that one is excluded from timing math in this whole test directory).
// -----------------------------------------------------------------------
void test_finite_repeat_holds_for_repeat_times_n_bars() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(3);
  model.set_scene_bars(0, 1);
  model.set_scene_repeat(0, 1);  // scene 0 (intro1): pass through quickly
  model.set_scene_bars(1, 1);
  model.set_scene_repeat(1, 3);  // the expansion under test: 1*3 == 3 bars
  model.set_scene_bars(2, 1);
  model.set_scene_repeat(2, 1);
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  session.send("bpm 400");  // shrink the wall-clock bar cadence
  session.send("transport start");
  app_state.note_transport_sent(true);

  int scene1_start_bar = -1;
  int scene2_start_bar = -1;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);
  while (std::chrono::steady_clock::now() < deadline) {
    poll_once(session, app_state);
    render_one_frame(model, seqedit, parts, session, app_state, fx);
    if (scene1_start_bar < 0 && fx.active_scene == 1 && app_state.bar() > 0) {
      scene1_start_bar = app_state.bar();
    }
    if (scene1_start_bar >= 0 && scene2_start_bar < 0 && fx.active_scene == 2) {
      scene2_start_bar = app_state.bar();
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  session.stop();

  CHECK(scene1_start_bar >= 0);
  CHECK(scene2_start_bar >= 0);
  if (scene1_start_bar >= 0 && scene2_start_bar >= 0) {
    // THE PIN: scene 1 (repeat=3, n_bars=1) must hold for at least 3 real
    // bars before the chain advances to scene 2 -- a no-op expansion (K
    // always collapsed to 1) would advance after only 1 bar, failing this.
    CHECK(scene2_start_bar - scene1_start_bar >= 3);
  }

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// Claim 3: an infinite scene truncates the rest of the built chain -- the
// next scene is never reached even after many real bars -- and an explicit
// resume gesture (the same wire shape a manual scene-header click sends)
// lands on the target section.
// -----------------------------------------------------------------------
void test_infinite_repeat_truncates_chain_until_explicit_resume() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(3);
  model.set_scene_bars(0, 1);
  model.set_scene_repeat(0, 1);  // scene 0 (intro1): pass through quickly
  model.set_scene_bars(1, 1);
  model.set_scene_repeat(1, GridModel::kSceneRepeatInfinite);  // scene 1 (varA): holds forever
  model.set_scene_bars(2, 1);
  model.set_scene_repeat(2, 1);  // scene 2 (varB): never reached by the auto-song build
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  session.send("bpm 400");
  session.send("transport start");
  app_state.note_transport_sent(true);

  // Wait for the chain to genuinely reach scene 1 (the infinite one), then
  // keep polling for a bounded but generous window -- long enough that a
  // (buggy) finite hold would have long since advanced to scene 2.
  bool scene1_seen = false;
  const auto start_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
  while (std::chrono::steady_clock::now() < start_deadline) {
    poll_once(session, app_state);
    render_one_frame(model, seqedit, parts, session, app_state, fx);
    if (fx.active_scene == 1 && app_state.bar() > 0) {
      scene1_seen = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CHECK(scene1_seen);

  const int bar_at_scene1_seen = app_state.bar();
  const auto hold_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
  while (std::chrono::steady_clock::now() < hold_deadline) {
    poll_once(session, app_state);
    render_one_frame(model, seqedit, parts, session, app_state, fx);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  // THE TRUNCATION PIN: many real bars have elapsed (a finite repeat=1 hold
  // would have advanced after just 1), and the chain never reached scene 2.
  CHECK(app_state.bar() - bar_at_scene1_seen >= 3);
  CHECK(fx.active_scene == 1);

  // THE RESUME PIN: the exact wire shape a manual scene-header click sends
  // (activate_scene_column, grid_panel.cpp) -- `song build 1 <section>
  // <bars> 1` -- rebuilds the chain from scratch and genuinely lands on the
  // target section, proving the infinite hold is not a permanent dead end.
  session.send("song build 1 varB 1 1");
  bool resumed = false;
  const auto resume_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
  while (std::chrono::steady_clock::now() < resume_deadline) {
    poll_once(session, app_state);
    if (app_state.section() == "varB") {
      resumed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CHECK(resumed);

  session.stop();
  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_song_build_wire_line_carries_repeat_tokens();
  test_finite_repeat_holds_for_repeat_times_n_bars();
  test_infinite_repeat_truncates_chain_until_explicit_resume();
  return sonotron::test::failures();
}

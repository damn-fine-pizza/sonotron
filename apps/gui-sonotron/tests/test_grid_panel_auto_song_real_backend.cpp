// LIVE-BUG repro, UI-AUTOMATING flavor (owner steer, following up on
// test_in_process_brain_session_bar_advance.cpp's backend-only proof): that
// test showed the backend poll loop alone (session.poll() -> AppState::
// apply(), no ImGui in the loop) DOES advance AppState::bar() correctly over
// real wall-clock time. This test closes the remaining gap the owner
// flagged: test_grid_panel_auto_song.cpp -- the only place render_grid_panel
// (and its file-local update_auto_song) is exercised at all -- fakes BOTH
// ends of the seam: the "click" is a direct field write (its own
// click_arm_auto_song comment admits there is no click-injection seam,
// which stands and is reused verbatim below) AND the beats are hand-injected
// via app_state.apply_line(R"({"ev":"beat",...})") straight into AppState
// while its SpyBrainSession::poll() is a permanent no-op -- the REAL
// InProcessBrainSession production backend is never in that loop at all.
//
// This test AUTOMATES THE UI: it drives the SAME headless-ImGui render loop
// (no GPU, no window, no backend -- font atlas self-builds on first
// NewFrame(), identical technique to test_grid_panel_auto_song.cpp) but
// wires it to a REAL InProcessBrainSession end to end -- "press Play" is the
// literal verb transport_panel.cpp:55 sends (`brain_session.send("transport
// start")`), every frame drains session.poll() and reduces through a REAL
// AppState::apply(), exactly main.cpp's own frame shape (main.cpp:696-697),
// and render_grid_panel is the REAL production entry point, not a stand-in.
// The ONLY hand-written state mutation left is click_arm_auto_song (there is
// still no ImGui::SmallButton click-injection seam in this codebase) -- the
// bar, the beats, and the transport state all travel the real pipeline.

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
#include "src/v02_state.hpp"

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
using sonotron::V02State;

namespace {

// One headless ImGui frame around render_grid_panel, byte-for-byte the same
// helper as test_grid_panel_auto_song.cpp's own render_one_frame (fx.playing
// refreshed from app_state.transport() right BEFORE the call,
// layout_renderer.cpp:81-82/107-108) -- reused verbatim so a discrepancy
// between the two tests can never be blamed on a difference in this harness.
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

// Mimics the EXACT state mutation the "auto-song" header button click
// performs (grid_panel.cpp render_header's `if (ImGui::SmallButton(...))`
// block) -- verbatim copy of test_grid_panel_auto_song.cpp's own
// click_arm_auto_song: there is still no click-injection seam for an
// ImGui::SmallButton in this codebase (headless mouse-event simulation would
// test ImGui itself, not this feature).
void click_arm_auto_song(V02State& fx, const AppState& app_state) {
  fx.auto_song = true;
  fx.active_scene_start_bar = app_state.bar();
  fx.auto_song_last_bar = app_state.bar();
}

// THE PINNED BUG (owner report, Repeat Zone): "auto-song ON + transport
// PLAYING: the playhead bar does NOT scroll, the scenes do NOT advance --
// scene 1 repeats forever -- even though audio plays". Reproduced here with
// NO synthetic event anywhere in the loop: a real InProcessBrainSession
// (real engine thread, real OutEvent ring, real decode), the real "Play"
// verb, the real per-frame poll/apply/render pipeline, and the real
// render_grid_panel/update_auto_song production code.
void test_bar_and_auto_song_advance_through_real_backend_and_real_render_loop() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  // Headless (no renderer backend): force the default font atlas to build
  // now, exactly what a real backend's own NewFrame() would trigger --
  // otherwise ImFontAtlasUpdateNewFrame() asserts the first time ImGui::
  // NewFrame() runs below (same technique test_grid_panel_auto_song.cpp
  // uses). The returned pixel buffer is discarded: no texture is ever
  // uploaded, no pixel is ever rendered.
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  SeqEditModel seqedit;
  PartsModel parts;
  V02State fx;
  AppState app_state;

  // SOURCE-OF-TRUTH TRANSITION (owner task #3, see grid_panel.cpp's update_
  // auto_song header comment): the advance no longer reads GridModel::
  // scene_bars at all -- it reads the STYLE's own real section length
  // (preview::section_bars) times kDefaultSectionRepeats (2). With "basic"
  // loaded below and scene 0 defaulting to kVarA (GridModel::
  // kDefaultSectionType), that is section_bars(basic, kVarA) == 2 bars * 2
  // repeats == 4 bars -- at the DEFAULT 120 BPM that is ~8 real seconds,
  // which would blow the 6000ms deadline below, so this test now also bumps
  // the tempo (same technique the sibling stop/restart and browser-switch
  // real-backend tests already use) to keep the real-wall-clock advance
  // comfortably inside budget.

  InProcessBrainSession session;
  CHECK(session.start());

  // Real content, exactly main.cpp's own default-boot behavior
  // (main.cpp:653-661): `style load basic` through the SAME send() a browser
  // drop uses, and the SAME active_style echo main.cpp itself sets -- so the
  // real "basic" style is genuinely loaded and playing audio end to end,
  // even though the auto-song advance decision itself is now governed by
  // GridModel::scene_bars above, not this style's own section length.
  for (std::size_t i = 0; i < sonotron::kBuiltinStyleNames.size(); ++i) {
    if (sonotron::kBuiltinStyleNames[i] == "basic") {
      session.send("style load basic");
      fx.active_style = static_cast<int>(i);
      break;
    }
  }
  session.send("bpm 400");  // shrink the wall-clock bar cadence (see comment above)

  // "Press Play": the REAL button verb, transport_panel.cpp:55, byte for
  // byte (`brain_session.send("transport start")` followed by the same
  // optimistic `app_state.note_transport_sent(true)` hint) -- never a
  // hand-injected transport JSONL.
  session.send("transport start");
  app_state.note_transport_sent(true);

  // Pump the REAL per-frame pipeline (poll -> apply -> render, main.cpp's
  // own shape) across up to 6 REAL wall-clock seconds -- ample headroom at
  // 400 BPM (a 4/4 bar is well under a real second, and the advance
  // threshold above is 4 bars), bounded on WALL TIME (not iteration/frame
  // count) so this stays robust to scheduler jitter rather than flaky, per
  // the same discipline test_in_process_brain_session.cpp's own poll_until
  // already uses.
  const int first_bar = app_state.bar();
  int max_bar_seen = first_bar;
  bool armed = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
  std::vector<BrainEvent> events;
  while (std::chrono::steady_clock::now() < deadline) {
    events.clear();
    session.poll(events);
    for (const BrainEvent& ev : events) {
      app_state.apply(ev);
    }
    max_bar_seen = std::max(max_bar_seen, app_state.bar());

    // Arm auto-song the first frame the transport is confirmed playing
    // (mirrors a user who presses Play, sees it take, THEN clicks the
    // header button) -- exactly once, same one-shot shape as a real click.
    if (!armed && app_state.transport() == AppState::Transport::kPlaying) {
      click_arm_auto_song(fx, app_state);
      armed = true;
    }

    render_one_frame(model, seqedit, parts, session, app_state, fx);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  session.stop();

  CHECK(armed);  // the transport must have reported "playing" at some point
  // THE ASSERTION UNDER TEST (owner symptom #1 -- "the playhead bar does NOT
  // scroll"): the bar observed through the REAL render loop must have
  // advanced past its starting value, not stayed pinned/constant for 6 real
  // seconds.
  CHECK(max_bar_seen > first_bar);
  // THE ASSERTION UNDER TEST (owner symptom #2 -- "the scenes do NOT
  // advance -- scene 1 repeats forever"): with auto-song armed and the
  // transport genuinely playing through the real backend and real render
  // loop, the active scene column must have moved off scene 0 at some point.
  CHECK(fx.active_scene != 0);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_bar_and_auto_song_advance_through_real_backend_and_real_render_loop();
  return sonotron::test::failures();
}

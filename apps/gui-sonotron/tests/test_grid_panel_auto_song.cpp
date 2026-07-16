// Functional (integration) regression test for the Repeat-Zone auto-song
// advance (repeat-zone-real-contract.md SLICE 4b). Drives the REAL
// render_grid_panel entry point (grid_panel.hpp) inside a headless ImGui
// context -- not just the already-green pure next_scene_to_launch() unit
// (test_grid_model.cpp) -- because the owner-reported bug ("auto-song ON +
// transport PLAYING, but the active scene never advances") lives in the
// INTEGRATION seam (grid_panel.cpp's file-local update_auto_song: how it
// reads app_state.bar()/fx.playing/fx.active_style and drives the
// once-per-bar guard), which is anonymous-namespace and unreachable directly
// from a test translation unit. render_grid_panel is the only public seam
// that reaches it.
//
// No GPU, no window, no real renderer backend: ImGui's immediate-mode logic
// (widgets, ID stack, font metrics) runs headless -- the font atlas
// self-builds on the first NewFrame() (imgui.cpp's UpdateFontsNewFrame() ->
// GetDefaultFont() -> ImFontAtlasBuildMain()), and this test never calls
// ImGui::Render() or touches a backend, so it stays a pure host-only
// functional test with no new dependency.

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
// sent command line instead of talking to a real transport, so the test can
// assert on the exact `style section <name>` wire text auto-song's advance
// is supposed to fire.
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

// One headless ImGui frame around render_grid_panel, mirroring the REAL
// per-frame shape layout_renderer.cpp's render_layout() drives it with:
// fx.playing is refreshed from app_state.transport() right BEFORE the grid
// panel call (layout_renderer.cpp:81-82/107-108) -- reproduced here rather
// than left stale, so a failure in this test cannot be blamed on a
// frame-order artifact this harness introduced.
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
// performs (grid_panel.cpp render_header, the `if (ImGui::SmallButton(...))`
// block) -- arms auto-song and resets both bookkeeping bars to the CURRENT
// live bar. There is no click-injection seam for an ImGui::SmallButton in
// this codebase (headless mouse-event simulation would test ImGui itself,
// not this feature), so this reproduces the click's own state transition
// verbatim, exactly as the task's repro instructions call for.
void click_arm_auto_song(V02State& fx, const AppState& app_state) {
  fx.auto_song = true;
  fx.active_scene_start_bar = app_state.bar();
  fx.auto_song_last_bar = app_state.bar();
}

// Shared assertion helper (auto-song fix follow-up, Torquato QA): checks
// whether any recorded sent line starts with the given prefix, used below to
// assert the presence/ABSENCE of a `launch scene ` send across a per-scene
// cadence window.
bool any_sent_line_starts_with(const std::vector<std::string>& sent, std::string_view prefix) {
  for (const std::string& line : sent) {
    if (line.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

// THE PINNED BUG: auto-song armed while playing, section length elapsed ->
// the active scene column must advance (wrapping) and a real
// `style section <name>` send must fire. This is the exact behavior the
// owner reports as broken ("the active scene never advances -- it stays in
// the same scene forever").
void test_auto_song_advances_scene_after_section_elapses() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  // Headless (no renderer backend, no ImGuiBackendFlags_RendererHasTextures):
  // force the default font atlas to build now, exactly what a real backend's
  // own NewFrame() would trigger by calling this same accessor -- otherwise
  // ImFontAtlasUpdateNewFrame() asserts (imgui_draw.cpp) the first time
  // ImGui::NewFrame() runs below. The returned pixel buffer is discarded:
  // this test never uploads a texture or renders a pixel.
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

  // Scene 0 (the active scene at arm time) gets an explicit, short 1-bar
  // length via the new per-scene length GridModel::set_scene_bars -- the
  // advance decision now reads THIS, not preview::section_bars (the style's
  // own section length, which used to be the ONLY thing available and was
  // always 1 bar for every built-in style, the very reason auto-song used to
  // sprint one bar per scene regardless of what a column held). Pinning it
  // explicitly keeps this test's single-bar crossing deterministic and
  // independent of any built-in style.
  model.set_scene_bars(0, 1);

  // Transport starts playing; the first beat lands at bar 1 (kBeat's
  // beat_bar is a 1-based absolute bar counter, abi.hpp/app_state.cpp).
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":1,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 1);
  CHECK(app_state.transport() == AppState::Transport::kPlaying);

  // Click "auto-song" while playing, at bar 1. fx.active_scene defaults to
  // 0, whose length is now pinned to 1 bar above -- the active scene's own
  // length should therefore already be "done" after just one more bar.
  click_arm_auto_song(fx, app_state);
  CHECK(fx.auto_song);
  CHECK(fx.active_scene == 0);

  // A render at the SAME bar the arm happened must NOT fire yet -- nothing
  // has elapsed (bar_just_advanced's own once-per-crossing guard).
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(brain.sent.empty());

  // Scene 0's own length is pinned to 1 bar above, so a single further bar
  // (bar 2, one whole bar past the arm bar) is exactly one full boundary for
  // the CURRENT active scene: this is the realistic, single-crossing repro
  // (not a multi-bar loop, which would cross the same 1-bar boundary again
  // on every subsequent bar and cycle through several scenes -- a different,
  // already-covered case of the pure next_scene_to_launch()/bar_just_
  // advanced units).
  app_state.apply_line(R"({"ev":"beat","bar":2,"beat":0,"pulse":0,"@":500})");
  CHECK(app_state.bar() == 2);
  render_one_frame(model, seqedit, parts, brain, app_state, fx);

  // THE ASSERTION UNDER TEST: the section elapsed -- the active scene column
  // MUST have advanced (to scene 1, its wrap-forward neighbor) and a real
  // `style section` send MUST have fired for it. This is the exact
  // owner-reported behavior under test.
  CHECK(fx.active_scene == 1);
  bool sent_section = false;
  for (const std::string& line : brain.sent) {
    if (line == "style section varA") {
      sent_section = true;
    }
  }
  CHECK(sent_section);

  ImGui::DestroyContext();
}

// THE PINNED BUG (this is the one that reproduces the owner report): a
// transport STOP followed by a fresh START is a completely normal part of a
// live workflow (stop, tweak something, hit play again) -- and the CORE
// itself rewinds its absolute bar counter back to 0 on every fresh Start
// (arrangrr::Transport::start(): "m_bar_index = 0;", runtime/transport.hpp,
// "MIDI Start semantics: rewind to zero and play", pinned by the core's own
// test_start_rewinds_and_reseeds_the_bar_gate). AppState mirrors that: a
// "stopped" transport event parks app_state.bar() back to 0
// (app_state.cpp's kTransport case). But grid_panel.cpp's auto-song
// bookkeeping (`fx.active_scene_start_bar`/`fx.auto_song_last_bar`) is ONLY
// ever written by the arm-click and by a successful advance -- NOTHING
// re-anchors it to the just-reset bar on a stop/restart cycle. So once the
// live bar has climbed past a handful of bars before a stop, the anchor is
// left stale and HIGHER than the freshly-restarted bar count: `bars_elapsed
// = current_bar - active_scene_start_bar` goes deeply NEGATIVE and stays
// there (next_scene_to_launch's own `bars_elapsed_in_scene <
// active_scene_section_bars` guard, grid_model.cpp:100) until the new
// session's bar counter climbs all the way back up past the stale anchor --
// which, for a live user who just stopped and restarted, reads exactly as
// "the active scene never advances -- it stays in the same scene forever."
void test_auto_song_stuck_after_transport_stop_then_restart() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  SeqEditModel seqedit;
  PartsModel parts;
  SpyBrainSession brain;
  AppState app_state;
  V02State fx;

  // Every scene column gets an explicit, short 1-bar length (the new
  // per-scene length the advance decision reads, GridModel::set_scene_bars)
  // so the ten-bar loop below is guaranteed to cross a boundary on every
  // single bar as fx.active_scene cycles through all five columns -- proving
  // the stuck-after-restart bug stays fixed regardless of which column is
  // active at any given bar.
  for (std::size_t scene = 0; scene < model.scene_count(); ++scene) {
    model.set_scene_bars(scene, 1);
  }

  // First session: the user has already been playing for a while (bar 25)
  // before arming auto-song -- a perfectly ordinary sequence (play first,
  // arm auto-song later, mid-song).
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":25,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 25);
  click_arm_auto_song(fx, app_state);
  CHECK(fx.active_scene_start_bar == 25);
  CHECK(fx.auto_song_last_bar == 25);

  // The user stops the transport (Stop, tweak something, no re-arm). The
  // CORE's own bar counter is what will rewind on the NEXT Start -- here we
  // model only AppState's own mirrored reset (app_state.cpp's kTransport
  // "stopped" case), which is the one grid_panel.cpp's seam actually reads.
  app_state.apply_line(R"({"ev":"transport","state":"stopped","@":100})");
  CHECK(app_state.bar() == 0);
  // auto_song itself is never disarmed by a stop -- only the header button
  // toggles it -- and the stale anchor from the FIRST session survives too:
  // nothing in this codebase re-anchors it on a stop/restart cycle.
  CHECK(fx.auto_song);
  CHECK(fx.active_scene_start_bar == 25);

  // The user presses Play again (a fresh Start: the core rewinds its bar
  // counter to 0, so the next beats report bar 1, 2, 3, ... again, exactly
  // like the very first time the transport ever started).
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":200})");
  for (int bar = 1; bar <= 10; ++bar) {
    app_state.apply_line(R"({"ev":"beat","bar":)" + std::to_string(bar) +
                         R"(,"beat":0,"pulse":0,"@":)" + std::to_string(200 + bar * 500) + "}");
    render_one_frame(model, seqedit, parts, brain, app_state, fx);
  }
  CHECK(app_state.bar() == 10);

  // THE BUG, PINNED: ten full bars into the NEW session -- with auto_song
  // still ON, the transport PLAYING, and every scene column pinned to a
  // 1-bar length above -- that should have advanced the active scene column
  // NINE separate times over by now -- the active scene has not moved AT
  // ALL. A correct implementation must not get stuck this way after an
  // ordinary stop/restart.
  CHECK(fx.active_scene != 0);
  CHECK(!brain.sent.empty());

  ImGui::DestroyContext();
}

// POSITIVE COVERAGE (Torquato QA, auto-song fix follow-up): pins that
// GridModel::scene_bars actually GOVERNS the advance cadence, not just that
// an advance eventually fires. Scene 0 is given an explicit 4-bar length: the
// active scene column must NOT advance (and no `launch scene ` line may be
// sent) after 1, 2, or 3 bars have elapsed, and MUST advance (with a
// `launch scene <n> ...` send) exactly once 4 full bars have elapsed --
// proving the per-scene length, not some fixed 1-bar sprint, is what gates
// every crossing.
void test_scene_bars_governs_advance_cadence() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
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

  // Scene 0 (the active scene at arm time) gets an explicit 4-bar length.
  model.set_scene_bars(0, 4);
  CHECK(model.scene_bars(0) == 4);

  // Transport starts playing; the first beat lands at bar 1.
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":1,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 1);

  // Arm auto-song at bar 1, scene 0 active.
  click_arm_auto_song(fx, app_state);
  CHECK(fx.auto_song);
  CHECK(fx.active_scene == 0);
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(brain.sent.empty());

  // Bars 2, 3, 4 are 1, 2, and 3 bars past the arm bar -- all strictly less
  // than the pinned 4-bar length, so the active scene must stay put and no
  // `launch scene ` line may be sent yet, on any of these three bars.
  for (int bar = 2; bar <= 4; ++bar) {
    app_state.apply_line(R"({"ev":"beat","bar":)" + std::to_string(bar) +
                         R"(,"beat":0,"pulse":0,"@":)" + std::to_string(bar * 500) + "}");
    render_one_frame(model, seqedit, parts, brain, app_state, fx);
    CHECK(fx.active_scene == 0);
    CHECK(!any_sent_line_starts_with(brain.sent, "launch scene "));
  }

  // Bar 5 is exactly 4 bars past the arm bar -- the pinned length is now
  // fully elapsed: the active scene column MUST advance and a
  // `launch scene <n> ...` command MUST be sent for the newly-active column.
  app_state.apply_line(R"({"ev":"beat","bar":5,"beat":0,"pulse":0,"@":2500})");
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 1);
  CHECK(any_sent_line_starts_with(brain.sent, "style section "));
  CHECK(any_sent_line_starts_with(brain.sent, "launch scene "));

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_auto_song_advances_scene_after_section_elapses();
  test_auto_song_stuck_after_transport_stop_then_restart();
  test_scene_bars_governs_advance_cadence();
  return sonotron::test::failures();
}

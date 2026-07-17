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

  // SECOND source-of-truth transition (task #6, see grid_panel.cpp's update_
  // auto_song header comment for the full rationale): GridModel::scene_bars
  // is real again -- task #6's always-visible length stepper made it genuinely
  // user-editable, so the advance now reads it directly (no style-length,
  // no kDefaultSectionRepeats multiplier). Pinned here via an explicit
  // set_scene_bars(0, ...) call, restoring this test's own original 2-bar
  // cadence through the NEW real mechanism instead of task #3's now-obsolete
  // style-length-times-repeats one.
  constexpr int kExpectedThresholdBars = 2;
  model.set_scene_bars(0, kExpectedThresholdBars);

  // Transport starts playing; the first beat lands at bar 1 (kBeat's
  // beat_bar is a 1-based absolute bar counter, abi.hpp/app_state.cpp).
  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":1,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 1);
  CHECK(app_state.transport() == AppState::Transport::kPlaying);

  // Click "auto-song" while playing, at bar 1.
  click_arm_auto_song(fx, app_state);
  CHECK(fx.auto_song);
  CHECK(fx.active_scene == 0);

  // A render at the SAME bar the arm happened must NOT advance auto-song's
  // OWN scene yet (bar_just_advanced's own once-per-crossing guard). This
  // first render is also the ONE frame TWO OTHER, legitimate one-shot sends
  // fire on: seed_demo()'s `clip add ...` registrations (owner bug #1 fix)
  // AND handle_master_play_launch's own immediate launch of the CURRENTLY
  // active scene (0) -- issue (a), grid_panel.cpp -- the instant the
  // transport is observed to have started (already true, from the "playing"
  // apply_line above). Neither is the property under test here; what must
  // NOT have happened yet is auto-song's OWN advance to a DIFFERENT scene
  // (1), so the assertion is narrowed to that, not to "nothing was ever
  // sent".
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));

  // One bar past the arm bar (bars_elapsed == 1) is still strictly less than
  // the 2-bar threshold -- the active scene must not have advanced yet.
  app_state.apply_line(R"({"ev":"beat","bar":2,"beat":0,"pulse":0,"@":500})");
  CHECK(app_state.bar() == 2);
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));

  // Two bars past the arm bar (bars_elapsed == kExpectedThresholdBars): the
  // section has now played its one real repeat, the full threshold. This is
  // the realistic, single-crossing repro (not a multi-bar loop, which would
  // cross the same boundary again on every subsequent bar and cycle through
  // several scenes -- a different, already-covered case of the pure
  // next_scene_to_launch()/bar_just_advanced units).
  app_state.apply_line(R"({"ev":"beat","bar":3,"beat":0,"pulse":0,"@":1000})");
  CHECK(app_state.bar() == 1 + kExpectedThresholdBars);
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

  // NOTE (task #6 SECOND source-of-truth transition): these set_scene_bars
  // calls are load-bearing AGAIN -- task #6's always-visible length stepper
  // made GridModel::scene_bars genuinely user-editable, so the advance now
  // reads it directly once more (see grid_panel.cpp's update_auto_song header
  // comment). Every scene here is pinned to a 1-bar hold, an even faster
  // cadence than the style-length-based threshold this comment used to
  // describe -- the ten-bar loop below still gives several crossings, more
  // than enough to prove the stuck-after-restart bug stays fixed.
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
  // still ON and the transport PLAYING -- that should have advanced the
  // active scene column several times over by now (every kDefaultSectionRepeats
  // * preview::section_bars(-1, *) fallback == 2 bars, grid_panel.cpp's
  // update_auto_song -- the per-scene set_scene_bars(scene, 1) calls above are
  // vestigial now: the advance no longer consults them, see owner task #3) --
  // the active scene has not moved AT ALL. A correct implementation must not
  // get stuck this way after an ordinary stop/restart.
  CHECK(fx.active_scene != 0);
  CHECK(!brain.sent.empty());

  ImGui::DestroyContext();
}

// POSITIVE COVERAGE (Torquato QA, auto-song fix follow-up) -- REPURPOSED
// TWICE now. Owner task #3 first moved this test's own premise from
// GridModel::scene_bars to the style's own real section length (preview::
// section_bars) times kDefaultSectionRepeats, because at the time nothing let
// the user edit scene_bars. Task #6 (this pass) SECOND-transitions it right
// back: the always-visible length stepper makes scene_bars genuinely
// user-editable again, so grid_panel.cpp's update_auto_song reads it
// directly once more, no style-length, no repeat multiplier -- see that
// function's own header comment for the full history. This test is pinned
// via an explicit set_scene_bars(0, 4) call, restoring the EXACT SAME 4-bar
// threshold the bar-by-bar timeline below already exercised (the old
// preview::section_bars(0, kVarA)==2 * kDefaultSectionRepeats==2 threshold
// task #3 had introduced), so the timeline itself is unchanged; only the
// mechanism producing that threshold changed. fx.active_style is left at 0
// ("basic") -- no longer load-bearing for the threshold itself, but kept so
// this test's own name/history stays traceable. The active scene column must
// NOT advance (and no `launch scene ` line may be sent) after 1, 2, or 3 bars
// have elapsed, and MUST advance (with a `launch scene <n> ...` send) exactly
// once 4 full bars have elapsed.
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

  // 0 == "basic" (kBuiltinStyleNames[0]) -- kept for this test's own
  // traceability, no longer load-bearing for the threshold (see this
  // function's own header comment). scene 0's own LENGTH (task #6) is what
  // actually governs the advance now, pinned to 4 bars explicitly below.
  fx.active_style = 0;
  model.set_scene_bars(0, 4);

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
  // Same narrowing as test_auto_song_advances_scene_after_section_elapses
  // above: this first render also seeds the demo grid (owner bug #1 fix) AND
  // fires handle_master_play_launch's own immediate launch of the CURRENTLY
  // active scene (0, issue (a)) -- both legitimate, neither the property
  // under test here. The assertion is narrowed to "auto-song has not ALSO
  // advanced to a DIFFERENT scene (1) yet", the only unambiguous signature
  // of a premature auto-song crossing.
  CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));

  // Bars 2, 3, 4 are 1, 2, and 3 bars past the arm bar -- all strictly less
  // than the 4-bar threshold (basic's own kVarA length 2 * kDefaultSectionRepeats
  // 2), so the active scene must stay put and no `launch scene 1` (the
  // wrap-forward advance target) may be sent yet, on any of these three bars.
  for (int bar = 2; bar <= 4; ++bar) {
    app_state.apply_line(R"({"ev":"beat","bar":)" + std::to_string(bar) +
                         R"(,"beat":0,"pulse":0,"@":)" + std::to_string(bar * 500) + "}");
    render_one_frame(model, seqedit, parts, brain, app_state, fx);
    CHECK(fx.active_scene == 0);
    CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));
  }

  // Bar 5 is exactly 4 bars past the arm bar -- the threshold is now
  // fully elapsed: the active scene column MUST advance and a
  // `launch scene <n> ...` command MUST be sent for the newly-active column
  // (1). Checked against "launch scene 1" specifically (not a blanket
  // "launch scene " prefix): the master-play launch (issue a) already put a
  // "launch scene 0" in `brain.sent` back on the very first render, so only
  // the scene-1-specific send is unambiguous proof THIS advance actually
  // fired.
  app_state.apply_line(R"({"ev":"beat","bar":5,"beat":0,"pulse":0,"@":2500})");
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 1);
  CHECK(any_sent_line_starts_with(brain.sent, "style section "));
  CHECK(any_sent_line_starts_with(brain.sent, "launch scene 1"));

  ImGui::DestroyContext();
}

// REAL-PATH COVERAGE (owner "devo sempre cliccare a mano per far avanzare"
// bug): every test above arms auto-song by hand (click_arm_auto_song), so
// they prove the ADVANCE logic but NEVER the real user path -- where nothing
// armed auto-song and pressing Play advanced nothing (the near-invisible
// header toggle was the only thing that ever set it, and master Play never
// touched it). auto_song now DEFAULTS ON (v02_state.hpp, owner decision
// 2026-07-17). This test constructs a fresh V02State (the real startup
// state), NEVER toggles it, and drives everything through render_grid_panel
// (which runs both handle_master_play_launch and update_auto_song) -- so the
// scene must advance on its own with zero direct fx poking, exactly the path
// that used to be broken while every armed test stayed green.
void test_auto_song_armed_by_default_advances_without_manual_toggle() {
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

  // The whole point: NEVER call click_arm_auto_song. A default-constructed
  // V02State is exactly what the app boots with.
  CHECK(fx.auto_song);  // pins the owner default-ON decision itself
  CHECK(fx.active_scene == 0);

  // Task #6 (SECOND source-of-truth transition, see grid_panel.cpp's update_
  // auto_song header comment): GridModel::scene_bars is real again -- pinned
  // here to 2 bars explicitly, restoring this test's own original 2-bar
  // threshold through the NEW real mechanism (fx.active_style/preview::
  // section_bars are no longer load-bearing for this decision at all).
  model.set_scene_bars(0, 2);

  app_state.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  app_state.apply_line(R"({"ev":"beat","bar":1,"beat":0,"pulse":0,"@":0})");
  CHECK(app_state.bar() == 1);

  // First frame at bar 1: handle_master_play_launch fires (transport observed
  // playing), launches the active scene 0 and anchors active_scene_start_bar
  // to the live bar. auto-song must NOT yet have advanced to a DIFFERENT scene.
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));

  // One bar past the master-play anchor (bars_elapsed == 1) is still
  // strictly less than the 2-bar threshold -- the active scene must not have
  // advanced yet.
  app_state.apply_line(R"({"ev":"beat","bar":2,"beat":0,"pulse":0,"@":500})");
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 0);
  CHECK(!any_sent_line_starts_with(brain.sent, "launch scene 1"));

  // Two bars past the master-play anchor: the 2-bar threshold has now fully
  // elapsed, and the song must advance to scene 1 ON ITS OWN -- no toggle, no
  // manual state mutation, only real rendered frames.
  app_state.apply_line(R"({"ev":"beat","bar":3,"beat":0,"pulse":0,"@":1000})");
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(fx.active_scene == 1);
  CHECK(any_sent_line_starts_with(brain.sent, "launch scene 1"));

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_auto_song_advances_scene_after_section_elapses();
  test_auto_song_stuck_after_transport_stop_then_restart();
  test_scene_bars_governs_advance_cadence();
  test_auto_song_armed_by_default_advances_without_manual_toggle();
  return sonotron::test::failures();
}

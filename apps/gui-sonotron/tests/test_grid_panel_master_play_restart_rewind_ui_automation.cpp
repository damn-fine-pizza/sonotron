// UI-AUTOMATION FIX PIN (Torquato QA pass, roadmap task #36, owner-observed
// LIVE bug): "pressing master Play does NOT start the song from the
// beginning". Root-caused by the orchestrator before this test was written:
// handle_master_play_launch (grid_panel.cpp) used to launch `fx.active_scene`
// AS-IS -- whatever column auto-song (or a manual scene-header click) last
// left it at -- instead of rewinding to the FIRST scene column on a fresh
// Play. The core itself DOES rewind its own bar counter to 0 on every Start
// (MIDI Start semantics, runtime/transport.hpp, already exercised by the
// sibling test_grid_panel_auto_song_stop_restart_ui_automation.cpp), but the
// GUI's own "which scene is the song's current one" bookkeeping was never
// re-anchored to the start of the song, only to whatever bar the transport
// happens to resume at.
//
// STATUS: this test was authored RED-first against the pre-fix tree (see the
// FAIL trail below for the exact failing assertion and root cause observed
// at that point). Giotto's fix landed in grid_panel.cpp concurrently, in
// the same shared tree, while this test was being written and verified --
// `handle_master_play_launch` now does `if (fx.auto_song) { fx.active_scene =
// 0; fx.auto_song_last_bar = app_state.bar(); }` before computing which
// column to launch. Both tests below now pass GREEN against the current
// tree and are kept as the standing regression proof for that fix (same
// shape as test_preview_multibar_truncation_red.cpp's own "originally RED,
// now the GREEN acceptance gate" precedent in this same directory) -- do not
// weaken or remove either assertion.
//
// Scenario driven through REAL mouse clicks on the REAL transport pad
// buttons (imgui_headless_harness.hpp), a REAL InProcessBrainSession, and a
// REAL render_grid_panel/render_transport_panel loop -- never a hand-written
// UiState field standing in for a click, EXCEPT the two documented gaps this
// whole test suite already lives with (no click-injection seam yet for the
// auto-song header's own ImGui::SmallButton, alpha-0-at-rest background):
//   1. auto_song ON (now the shipped DEFAULT, ui_state.hpp), Play, and let
//      the REAL auto-song advance move the song mid-way to a non-zero scene
//      column (exactly test_grid_panel_auto_song_stop_restart_ui_
//      automation.cpp's own round 1, reused verbatim here).
//   2. Real click Stop (stopped, bar back to 0 -- the core's own real
//      rewind, already proven elsewhere).
//   3. Real click Play again. THE ASSERTION: the scene handle_master_play_
//      launch/activate_scene_column actually launches must be the FIRST scene
//      column (index 0, "the song start"), never the stale mid-song column.
//
// THE ASSERTION IS BEHAVIORAL, NOT A MIRROR OF THE BUG: rather than reading
// back `fx.active_scene` alone (which a buggy handle_master_play_launch would
// simply reassign to itself, making that check tautological), this test
// waits for the REAL engine's own confirmed section readback
// (AppState::section(), fed by the wire's real "section" OutEvent -- the
// SAME authoritative readback render_header's own "engine: %s" status row
// shows, grid_panel.cpp) to settle after the second Play, and asserts it
// equals scene 0's own section ("intro1", seed_demo's kDemoSections[0]) --
// the one and only fact that proves the FIRST scene column was genuinely
// re-launched end to end, wire and all. `fx.active_scene` is checked too, as
// a corroborating (not primary) signal.
//
// SCOPE DECISION, ENCODED AS A SECOND TEST BELOW: the rewind-to-first-scene
// behavior must be auto_song-ON only. With auto_song OFF, `ui_state.hpp`'s
// own comment calls this "a deliberate fixed single-scene loop" -- the active
// scene there is the user's own explicit manual selection (a scene-header
// click), not stale auto-song bookkeeping, and Play re-launching that exact
// selection is already the CORRECT, owner-intended behavior (grid_panel.cpp's
// handle_master_play_launch header comment, "owner task #4": "Deliberately
// UNCONDITIONAL on fx.auto_song ... with auto-song OFF ... the active scene
// must still be launched once"). A naive fix that rewinds to scene 0
// UNCONDITIONALLY (ignoring auto_song) would silently break that documented,
// owner-approved case -- the second test below locks the boundary so that
// regression cannot slip in unnoticed.

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
// directory shares (see test_grid_panel_auto_song_stop_restart_ui_
// automation.cpp's own copy for the "why explicit size" note).
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

// Shared setup: real style load + fast bpm + locate the real Play/Stop pads,
// then a real Play click and a real wait for auto-song to move the song
// mid-way to some non-zero scene column. Returns the scene index the song
// landed on (>0 on success; callers CHECK this).
struct MidSongFixture {
  th::Rect play_rect;
  th::Rect stop_rect;
  int scene_after_round1 = 0;
};

MidSongFixture drive_to_mid_song(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                                 InProcessBrainSession& session, AppState& app_state,
                                 UiState& fx) {
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
  MidSongFixture out;
  out.play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(out.play_rect.found);
  const ImU32 stop_border_color = sonotron::neon::u32(sonotron::theme::kTextSecondary, 0.6F);
  out.stop_rect = th::find_single_color_rect(locate, stop_border_color);
  CHECK(out.stop_rect.found);

  // Real click: Play (round 1).
  click_at(out.play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  bool armed = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      // Auto-song was already armed from frame 0 (CHECK'd in the caller) --
      // the transport reporting "playing" is the only real-world signal
      // left to wait for; handle_master_play_launch (grid_panel.cpp)
      // already built and launched the song on this same transition,
      // through production code.
      if (!armed && app_state.transport() == AppState::Transport::kPlaying) {
        armed = true;
      }
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (armed && fx.active_scene != 0) {
        break;  // the FIRST auto-song advance landed -- the song is now mid-way
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(armed);
  CHECK(fx.active_scene != 0);
  out.scene_after_round1 = fx.active_scene;
  return out;
}

// -----------------------------------------------------------------------
// TEST 1 (originally RED, now the GREEN acceptance gate -- see the file's own
// header comment): auto_song ON. Master Play, pressed a second time after a
// real Stop while the song sits mid-way, must restart the song from scene 0
// -- not re-launch the stale mid-song column.
// -----------------------------------------------------------------------
void test_master_play_restart_rewinds_to_first_scene_when_auto_song_on() {
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
  // Song-mode Phase 1 precondition (see this file's own header comment):
  // auto-song starts ARMED by default -- nothing needs to click the header
  // toggle to reach this state.
  CHECK(fx.auto_song);
  AppState app_state;
  InProcessBrainSession session;

  const MidSongFixture setup = drive_to_mid_song(model, seqedit, parts, session, app_state, fx);
  const int scene_after_round1 = setup.scene_after_round1;
  // seed_demo (grid_panel.cpp) has run by now (drive_to_mid_song's own warm-up
  // frames), so every scene column already carries its own distinct, real
  // section -- read straight off the model, never hardcoded, so this
  // assertion tracks seed_demo's own kDemoSections table instead of drifting
  // from it.
  const std::string expected_first_scene_section =
      std::string(sonotron::section_wire_name(model.scene_section(0)));
  CHECK(!expected_first_scene_section.empty());
  // The REAL engine's own settled section right before the restart (e.g.
  // "varA", scene_after_round1's own section) -- captured so the round-2 wait
  // below can tell a genuine POST-RESTART flip apart from this pre-existing
  // stale reading. app_state.cpp's "stopped" reduction never clears
  // m_section, so this value survives the Stop below unchanged; without
  // waiting for a real flip away from it, a naive "just wait for bar()>0"
  // loop can break on the very first post-restart bar while the flip is
  // still in flight and wrongly capture this stale value as if it were the
  // round-2 readback (found empirically while writing this test).
  const std::string section_before_restart = app_state.section();

  // Real click: Stop (transport_panel.cpp's REAL button verb). Wait for the
  // REAL confirmed stop (never the optimistic hint) -- same rationale as the
  // sibling stop/restart test's own comment on this exact race.
  click_at(setup.stop_rect.center(), model, seqedit, parts, session, app_state, fx);
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
  CHECK(app_state.bar() == 0);

  // Real click: Play again (round 2) -- THE OWNER'S OWN SCENARIO: master Play
  // pressed a second time, song sitting mid-way (fx.active_scene ==
  // scene_after_round1 at this exact moment, since nothing else touches it
  // between the break above and this click).
  click_at(setup.play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Wait for the transport to be genuinely observed playing again AND the
  // real engine section readback to actually FLIP away from its pre-restart
  // stale value (never just "non-empty" -- see section_before_restart's own
  // comment above for why that weaker condition can race a still-in-flight
  // commit and capture a stale reading). One real bar boundary is all
  // `style section`/`launch scene ... quantize 1` needs to commit
  // (kDefaultLaunchQuantizeBars == 1, grid_panel.cpp; see test_repeat_zone_
  // scene_header_next_bar_ui_automation.cpp's own header comment for the
  // exact next-bar-boundary contract), so this deadline is generous, not
  // tight.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0 &&
          app_state.section() != section_before_restart) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);

  session.stop();

  // THE PIN (real behavioral assertion, the actual downstream engine
  // readback, not a mirror of fx.active_scene): after a fresh master Play
  // following a real Stop, the song must have restarted from scene 0's own
  // section -- never stayed parked on the stale mid-song section, and never
  // flipped to anything else either.
  //
  // BEFORE THE FIX this read the STALE scene_after_round1's own section
  // instead -- in fact it never flipped away from section_before_restart at
  // all within the wait window above, so that loop timed out at its own
  // deadline -- because handle_master_play_launch (grid_panel.cpp) computed
  // active_scene_index straight from fx.active_scene without ever resetting
  // it to 0 first. Confirmed by hand against the pre-fix tree: `CHECK
  // app_state.section() == expected_first_scene_section` failed with
  // section() stuck on scene_after_round1's own wire name (e.g. "varA"
  // instead of "intro1"), and the corroborating `fx.active_scene == 0` check
  // below failed too (stuck at scene_after_round1).
  CHECK(app_state.section() == expected_first_scene_section);

  // Corroborating (not primary) signal: the SAME fx bookkeeping activate_
  // scene_column mutates every call site already relies on.
  CHECK(fx.active_scene == 0);
  CHECK(fx.active_scene != scene_after_round1 || scene_after_round1 == 0);

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// TEST 2 (scope lock, GREEN both before and after the fix): auto_song OFF.
// The active scene is the user's own explicit manual choice (a deliberate
// single-scene loop, ui_state.hpp's own comment) -- master Play must keep
// re-launching THAT scene, never rewind to scene 0. This nails down the
// boundary so a fix for TEST 1's bug does not silently regress into rewinding
// UNCONDITIONALLY (ignoring auto_song) -- Giotto's actual fix (see the
// file's own header comment) does gate the rewind on `fx.auto_song`, and this
// test is what proves that gate holds.
// -----------------------------------------------------------------------
void test_master_play_restart_preserves_selected_scene_when_auto_song_off() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  // Song-mode Phase 1 precondition (see this file's own header comment):
  // auto-song starts ARMED by default -- nothing needs to click the header
  // toggle to reach this state.
  CHECK(fx.auto_song);
  AppState app_state;
  InProcessBrainSession session;

  const MidSongFixture setup = drive_to_mid_song(model, seqedit, parts, session, app_state, fx);
  const int scene_after_round1 = setup.scene_after_round1;
  const std::string expected_selected_scene_section = std::string(sonotron::section_wire_name(
      model.scene_section(static_cast<std::size_t>(scene_after_round1))));
  CHECK(!expected_selected_scene_section.empty());

  // The user turns auto-song OFF right where they are -- a deliberate
  // "keep looping this scene" choice (documented gap: no click-injection
  // seam yet for the auto-song header's own alpha-0-at-rest SmallButton, same
  // as click_arm_auto_song's own precedent above for the ON direction).
  fx.auto_song = false;

  // Real click: Stop.
  click_at(setup.stop_rect.center(), model, seqedit, parts, session, app_state, fx);
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
  CHECK(app_state.bar() == 0);

  // Real click: Play again, auto_song still OFF.
  click_at(setup.play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0 &&
          !app_state.section().empty()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);

  session.stop();

  // THE SCOPE LOCK: with auto_song OFF, the manually-selected scene must
  // still be the one launched -- rewinding here would be an overreach beyond
  // the owner's actual bug report (which is specifically about auto_song's
  // own stale bookkeeping).
  CHECK(app_state.section() == expected_selected_scene_section);
  CHECK(fx.active_scene == scene_after_round1);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_master_play_restart_rewinds_to_first_scene_when_auto_song_on();
  test_master_play_restart_preserves_selected_scene_when_auto_song_off();
  return sonotron::test::failures();
}

// UI-AUTOMATION TIMING PIN (Torquato QA pass, task #19, owner report): "a
// manual next-scene click in Repeat Zone must take effect at the NEXT BAR" --
// the owner PERCEIVES the click as waiting until the whole cell/section ends
// before anything changes, rather than landing cleanly on the very next bar.
//
// This test clicks the REAL scene-header ▶ (grid_panel.cpp's render_scene_
// header_cell) while the transport is REALLY playing, through a REAL
// InProcessBrainSession, and measures -- in real bar units read back from the
// REAL engine's own "section"/"clip" OutEvents (never a V02State/AppState
// field write standing in for the click) -- how many bars elapse between the
// click and the FIRST observable readback flip. The pin: that gap must be
// <= 1 bar (the SAME bar boundary the click's own `style section` (Param::
// kStyleSection -> Arranger::request(section, immediate=false while playing),
// arranger.hpp's own "applied at the next bar boundary" contract,
// engine.cpp:635-642) and `launch clip ... quantize 1` (Boundary::kNextBar,
// in_process_brain_session.cpp's parse_quantize_suffix, clip_matrix.hpp's
// arm()/on_bar() due_bar_index math) are both individually documented to
// honor -- NOT `GridModel::kDefaultSceneBars` (8 bars, grid_model.hpp), which
// is a GUI-only bookkeeping length for auto-song's OWN auto-advance decision
// and the visual playhead sweep, and must never leak into how long a MANUAL
// click takes to land.
//
// Root-cause pointers this test's own assertions are keyed to (read, not
// assumed, before writing this file):
//   - grid_panel.cpp render_scene_header_cell's "go" branch: sends `style
//     section <name>` then `launch scene <s> quantize 1` (kDefaultLaunchQuantizeBars == 1).
//   - in_process_brain_session.cpp command_line_to_command: `style section`
//     carries no boundary/quantize suffix of its own (Engine::cmd_style reads
//     `!m_transport.playing()` as its OWN immediate flag); `launch scene ...
//     quantize 1` parses to Boundary::kNextBar via parse_quantize_suffix.
//   - engine.cpp Engine::cmd_style's kStyleSection case: `m_arranger.request
//     (section, !m_transport.playing())`.
//   - arranger.hpp Arranger::request(): "applied at the next bar boundary".
//   - arranger.hpp Arranger::on_tick(): the pending switch commits at ANY
//     `bar_boundary` (`pos % ticks_per_bar == 0`), not only at `section_end`
//     -- for every built-in style, section.bars == 1, so bar_boundary and
//     section_end always coincide for the STYLE side; the scene's OWN length
//     (GridModel::kDefaultSceneBars == 8) never enters this decision at all.
//   - clip_matrix.hpp ClipMatrix::arm()/on_bar(): `due_bar_index = bar_index_
//     now + (n_bars - 1)`; with n_bars == 1 this is the CURRENT bar count at
//     arm time, and Engine::fire_clips calls on_bar() with that SAME
//     bar_index_now (captured before Transport::advance_bar_tick()'s
//     increment) at the very next bar boundary -- so a clip armed with
//     quantize 1 mid-bar N is due at the boundary closing bar N, i.e. the
//     NEXT bar, never later.
//
// If this test goes RED, the bug is NOT where the above trace says it should
// be (the engine-level next-bar quantize looks correctly wired by inspection)
// -- see this file's own header comment in the QA report for the honest
// alternate theory this test is also positioned to catch: the PREVIOUS scene
// is never explicitly stopped when a new one launches (no `stop clip`/`stop
// scene` is ever sent, grid_panel.cpp), which is a real but DIFFERENT defect
// from a next-bar-quantize violation.

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
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::V02State;
namespace th = sonotron::test_harness;

namespace {

// Same combined transport+grid frame shape as test_repeat_zone_playhead_ui_
// automation.cpp's own render_one_frame (both panels this flow spans).
ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, V02State& fx) {
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
                     BrainSession& brain_session, AppState& app_state, V02State& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
}

// Drains every pending real OutEvent into `app_state` once (a single real
// poll cycle) -- used both for a plain "refresh" and inside the timing loop
// below.
void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

void test_manual_scene_launch_click_flips_readback_within_one_bar_while_playing() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  SeqEditModel seqedit;
  PartsModel parts;
  V02State fx;
  // Pinned OFF (Torquato QA re-pass, world change: auto_song now DEFAULTS TO
  // true, commit 7113a90 "auto-song DEFAULT ON", landed AFTER this test's own
  // base commit 59660fd). Left armed, update_auto_song's own periodic
  // re-evaluation (grid_panel.cpp) stays live for the whole lifetime of this
  // test's two bounded wall-clock wait windows below -- a SEPARATE, real
  // advance mechanism from the ONE manual scene-header click this test
  // measures. If a wait window ever ran long (host contention, CI slowness),
  // an unrelated auto-fired `style section`/`launch scene` could land inside
  // one of those windows and confound "which change produced this observed
  // flip". Pinning it false here removes that possibility, isolating the one
  // thing this test pins: the manual click's own next-bar timing. This does
  // NOT touch handle_master_play_launch (grid_panel.cpp) -- that one-shot
  // Play-triggered launch is UNCONDITIONAL (no auto_song gate at all), so it
  // still fires below; see the wait-loop comment further down for how this
  // test now honestly accounts for its real effect.
  fx.auto_song = false;
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
  // A baseline section, established WHILE STOPPED (immediate=true, engine.cpp's
  // kStyleSection case echoes OutEvent::section right away) -- deliberately NOT
  // "varA", the target scene-header click below will request (kDemoSections
  // maps scene 1 -> SectionType::kVarA, grid_panel.cpp's seed_demo). This send
  // is NOT expected to survive past the Play click below: handle_master_play_
  // launch (grid_panel.cpp, added in commit 9a67254 AFTER this test's own base
  // commit) unconditionally fires the instant Play is observed playing and
  // sends its OWN `style section <scene 0's section>` ("intro1", seed_demo's
  // kDemoSections[0]) -- see the wait-loop comment below for how this test
  // accounts for that honestly instead of assuming "varC" survives untouched.
  session.send("style section varC");
  // Speed up the real wall-clock bar cadence (400 BPM, the engine's own
  // accepted max, shell_parse.cpp) so this test's bounded wall-clock windows
  // stay short without touching iteration/frame counts -- a bar is ~0.6s at
  // 400 BPM/4/4 versus ~2s at the 120 BPM default.
  session.send("bpm 400");

  // Warm-up + locate frame (transport still stopped, nothing playing yet --
  // the scene-header caret's own paint color, neon::u32(theme::kGreen), is
  // unique in this frame only while no chord-row (role_index 3, also
  // theme::kV02TrackColor[kGreen]) cell is rendering a `playing==true`
  // border at the SAME full alpha; capturing the header rects BEFORE Play is
  // ever clicked keeps that collision structurally impossible).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  // Scene-header carets: render_scene_header_cell paints "\xE2\x96\xB6" in
  // neon::u32(theme::kGreen) (default alpha 1.0) at every column,
  // unconditionally -- but theme::kGreen at THIS SAME full alpha is NOT
  // exclusive to the caret in this frame: transport_panel.cpp:90's
  // unconditional `ImGui::TextColored(theme::kGreen, "Cm")` key readout, and
  // grid_panel.cpp's render_track_label row-name text for the "chord" track
  // row (role_index 3, theme::kV02TrackColor[2] == kGreen, drawn at full
  // alpha whenever that row isn't muted/dimmed), both land in the SAME color
  // bucket -- confirmed empirically while re-deriving this geometry (a naive
  // find_color_clusters over the whole frame returns 7 clusters, not 5: one
  // at y~438 ["Cm"], the real 5 scene headers, and one further down [the
  // "chord" row label]).
  //
  // ANCHOR, NOT A MAGIC CONSTANT (re-derived by Torquato QA pass after the
  // header grew two new UNCONDITIONALLY-reserved status rows, commit 9a67254
  // "header layout-shift fix" -- landed AFTER this test's own base commit
  // 59660fd -- which pushed the header's on-screen Y down by ~34px,
  // invalidating the previous hardcoded band [490,510]; NOTE this shift is
  // NOT caused by the "auto-song"/"song" toggle label width as first
  // suspected -- that toggle sits on one SameLine() row and never changes
  // row height -- it is caused by the two placeholder Dummy rows 9a67254
  // added below the title, which are reserved at a FIXED height regardless of
  // fx.playing/fx.auto_song, i.e. the shift is permanent, not state-
  // dependent): rather than re-measure and hardcode a new absolute pixel
  // band (which the NEXT header addition would silently invalidate again),
  // anchor to the "grid_body" child window's own real on-screen top edge
  // (find_child_window_rect, read-only ImGui window-list introspection, no
  // product change) -- render_scene_header_row is the FIRST thing drawn
  // inside that child, so the caret row always lands within a small margin
  // below its top. The 60px margin comfortably covers the header row's own
  // max height (cz*0.5 at minimum zoom up to a few wrapped-name text lines)
  // while staying far clear of the next same-colored cluster below it (the
  // "chord" row label, ~140px further down in this frame) -- confirmed
  // empirically while re-deriving this anchor.
  const ImU32 caret_color = sonotron::neon::u32(sonotron::theme::kGreen);
  const std::vector<th::Rect> all_carets = th::find_color_clusters(locate, caret_color);
  const th::Rect grid_body_rect = th::find_child_window_rect("grid_body");
  CHECK(grid_body_rect.found);
  std::vector<th::Rect> carets;
  for (const th::Rect& r : all_carets) {
    if (r.min.y >= grid_body_rect.min.y && r.max.y <= grid_body_rect.min.y + 60.0F) {
      carets.push_back(r);
    }
  }
  CHECK(carets.size() >= 2);
  if (carets.size() < 2) {
    ImGui::DestroyContext();
    return;
  }
  const th::Rect scene1_header = carets[1];  // column 1 -> SectionType::kVarA ("varA")

  // Real click: Play (transport_panel.cpp's REAL button verb).
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Wait (bounded real wall-clock, not iteration count) until the transport
  // genuinely reports playing with at least one real beat landed AND the
  // baseline section has SETTLED to something other than "varA" -- both real
  // engine readbacks, never the optimistic hint.
  //
  // NOT "== varC" (post-7113a90 finding): the "varC" sent above while stopped
  // does NOT survive Play. handle_master_play_launch (grid_panel.cpp) is
  // UNCONDITIONAL (no auto_song gate) and fires the instant Play is observed
  // playing, sending its OWN `style section intro1` (seed_demo's scene 0 ->
  // SectionType::kIntro1) + `launch scene 0 quantize 1` -- quantized to the
  // SAME next-bar boundary this loop's own `bar() > 0` is waiting to cross.
  // Asserting "== varC" here would be pinning a race between two independent
  // next-bar-quantized commits (confirmed by hand: 30/30 runs settled on
  // "intro1", never observed "varC" survive), not the behavior this test
  // actually cares about. What this test's own intent requires (see the
  // baseline comment above) is only that the settled baseline is NOT already
  // "varA" -- so a later flip to "varA" is unambiguously attributable to the
  // manual scene-header click under test, regardless of which non-varA
  // section real Play happens to settle into.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0 &&
          !app_state.section().empty() && app_state.section() != "varA") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(!app_state.section().empty());
  CHECK(app_state.section() != "varA");

  // Refresh the bar reading as close to the click as possible, then click:
  // scene-header 1's REAL ▶ (`style section varA` + `launch scene 1
  // quantize 1`, never hand-written here).
  poll_once(session, app_state);
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  const int bar_at_click = app_state.bar();
  constexpr std::size_t kDrumsScene1ClipId = 1;  // cell_id(role=0, scene=1, scene_count=5)
  const AppState::ClipLaunchState clip_before = app_state.clip_state(kDrumsScene1ClipId);

  click_at(scene1_header.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Tight real-time poll (bounded wall clock, fine-grained sleep) recording
  // the FIRST bar at which each real readback flips.
  int section_flip_bar = -1;
  int clip_flip_bar = -1;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (section_flip_bar < 0 && app_state.section() == "varA") {
        section_flip_bar = app_state.bar();
      }
      if (clip_flip_bar < 0 && app_state.clip_state(kDrumsScene1ClipId) != clip_before) {
        clip_flip_bar = app_state.bar();
      }
      if (section_flip_bar >= 0 && clip_flip_bar >= 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  session.stop();

  // Sanity: both readbacks must have flipped at all within the wall-clock
  // budget, or the timing assertions below are meaningless.
  CHECK(section_flip_bar >= 0);
  CHECK(clip_flip_bar >= 0);

  // THE PIN: a manual scene-header click while playing must take effect at
  // the NEXT bar (bar_at_click, or bar_at_click + 1 if the click landed just
  // after that bar's own boundary already closed) -- NEVER after
  // GridModel::kDefaultSceneBars (8) bars, which is what "waiting until the
  // cell ends" would look like if the bug the owner reports is real.
  if (section_flip_bar >= 0) {
    CHECK(section_flip_bar - bar_at_click <= 1);
  }
  if (clip_flip_bar >= 0) {
    CHECK(clip_flip_bar - bar_at_click <= 1);
  }

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_manual_scene_launch_click_flips_readback_within_one_bar_while_playing();
  return sonotron::test::failures();
}

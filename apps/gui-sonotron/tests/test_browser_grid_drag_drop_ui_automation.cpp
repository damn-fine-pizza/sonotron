// UI-AUTOMATION acceptance test (Torquato QA pass, roadmap node 14120): node
// 14120 claims "real search + real drag-and-drop already ship" as its own
// headline justification. Search-then-click-to-apply IS covered
// (test_browser_variation_click_apply_ui_automation.cpp). Drag was NOT: a
// grep across every test file for the real ImGui drag-drop payload IDs
// (kStyleDragPayloadId, kVariationDragPayloadId, BeginDragDropSource,
// AcceptDragDropPayload) found ZERO hits before this file -- no test ever
// drove a genuine multi-frame drag gesture through BeginDragDropSource/
// AcceptDragDropPayload. This file closes that gap for real.
//
// INVESTIGATION FINDING (owner-requested check on this pass): test_browser_
// variation_click_apply_ui_automation.cpp's own header comment (lines 11-14
// at the time of writing) claimed "the pre-existing drag-to-scene-header path
// ... stays covered by grid_panel.cpp's own drop-target tests". That claim is
// FALSE -- there is no "grid_panel.cpp's own drop-target tests" file at all.
// The only pre-existing hit for the word "drag" near grid_panel.cpp's drop
// targets is test_repeat_zone_playhead_ui_automation.cpp's ROOT-CAUSE COMMENT
// (it explains, in prose, that the browser drag-drop handler is the only path
// that registers a real ClipMatrix clip) -- it never drives a drag gesture
// itself, real or simulated; it only launches a cell seed_demo() already
// registered via a hand-written `clip add` string. That stale comment is
// corrected as part of this pass (see the diff to that file) to point here
// instead of to a test that never existed.
//
// Two scenarios, both driven end to end through imgui_headless_harness.hpp's
// new drag_to() helper (real io.AddMousePosEvent/AddMouseButtonEvent
// injection across real multi-frame NewFrame/Render cycles -- see that
// helper's own header comment for exactly why each frame in the cadence is
// required by ImGui's own drag-and-drop state machine, not guessed):
//
//   1. Style drag (browser_panel.cpp:177-178 BeginDragDropSource/
//      SetDragDropPayload(kStyleDragPayloadId) -> grid_panel.cpp:1453-1454
//      AcceptDragDropPayload(kStyleDragPayloadId), render_track_cell's own
//      drop-target block): drags the "rock" style leaf onto a genuinely
//      EMPTY launch cell (perc/scene-0 -- never touched by seed_demo()'s own
//      12-cell demo pattern), then proves the drag was REAL by launching that
//      SAME cell with a real click and reading AppState::clip_state() flip
//      away from kStopped -- the SAME real ClipMatrix round trip test_
//      repeat_zone_playhead_ui_automation.cpp's own root-cause comment
//      describes, this time actually exercised through the real drag path
//      that comment was only ever describing in prose.
//
//   2. Variation drag (browser_panel.cpp:311-313 BeginDragDropSource/
//      SetDragDropPayload(kVariationDragPayloadId) -> grid_panel.cpp:1086-
//      1087 AcceptDragDropPayload(kVariationDragPayloadId), render_scene_
//      header_cell's own drop-target block): drags the "chorus" variation
//      leaf onto scene column 2's own header, then proves the drag was REAL
//      by clicking that SAME header (a real, separate click gesture,
//      activate_scene_column's own `song build 1 <section> ...` verb) and
//      reading AppState::section() -- the authoritative kSection echo, the
//      SAME signal test_browser_variation_click_apply_ui_automation.cpp
//      already established -- actually flip from the pre-drag baseline
//      ("varB", seed_demo()'s own default for scene 2) to the dragged
//      variation's wire name ("varC"). Never a spied GridModel/UiState field:
//      the drop target itself only mutates host-side GridModel state (no
//      wire send, per render_scene_header_cell's own header comment -- "the
//      section only takes musical effect once the header's own play is
//      applied"), so the REAL proof this test needs is that a LATER, entirely
//      separate real click reads back the value the drag set, through the
//      real engine.
//
// Both panels render together in the SAME ImGui window/frame (mirroring
// test_grid_panel_auto_song_stop_restart_ui_automation.cpp's own combined-
// render pattern) -- a drag needs its source (browser_panel) and its target
// (grid_panel) both live in one frame, and ImGui's own BeginDragDropTarget()
// requires the target's window to share the drag source's RootWindow, which
// only holds if both panels are children of the SAME outer window.
//
// Neither scenario needs the transport running: Engine::clip_request emits
// its own "clip" OutEvent synchronously the instant a quantized launch is
// processed (engine.cpp, `LaunchState::kArmed`, sunk unconditionally, not
// gated on Transport::kPlaying), and Engine::scene_play's first step applies
// its Performance (and therefore its section) synchronously too
// (SceneChain::play()'s own immediate callback) -- confirmed by reading both
// call paths directly rather than assumed.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/browser_panel.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
#include "src/parts_model.hpp"
#include "src/seqedit_model.hpp"
#include "src/theme.hpp"
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
using sonotron::BrowserCategory;
using sonotron::BrowserModel;
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// Renders BOTH real panels side by side inside ONE window -- browser_panel in
// its own 360px-wide child (mirroring test_browser_variation_click_apply_ui_
// automation.cpp's own "browser_area" wrapper) and grid_panel filling the
// rest, so a drag gesture's source and target are both live in the SAME
// frame/RootWindow (see this file's own header comment for why that matters
// to ImGui's own BeginDragDropTarget()).
ImDrawData* render_one_frame(BrowserModel& browser, GridModel& grid, SeqEditModel& seqedit,
                             PartsModel& parts, BrainSession& brain_session, AppState& app_state,
                             UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1280.0F, 800.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("browser_area", ImVec2(360.0F, 800.0F));
  sonotron::render_browser_panel(browser, brain_session, app_state, fx);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("grid_area", ImVec2(0.0F, 800.0F));
  sonotron::render_grid_panel(grid, seqedit, parts, brain_session, app_state, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, BrowserModel& browser, GridModel& grid, SeqEditModel& seqedit,
                     PartsModel& parts, BrainSession& brain_session, AppState& app_state,
                     UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(browser, grid, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(browser, grid, seqedit, parts, brain_session, app_state, fx);
}

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

// Pumps the REAL per-frame pipeline (bounded wall time), rendering every
// cycle, until `predicate(app_state)` is true. Shared shape both scenarios
// below use for their own post-action readback wait.
template <typename Predicate>
bool pump_until(BrainSession& session, BrowserModel& browser, GridModel& grid,
                SeqEditModel& seqedit, PartsModel& parts, AppState& app_state, UiState& fx,
                Predicate&& predicate, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    poll_once(session, app_state);
    render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);
    if (predicate(app_state)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate(app_state);
}

// Scenario 1: a REAL drag of the "rock" style leaf onto a genuinely EMPTY
// launch cell (perc/scene-0), proved real by launching that same cell for
// real afterward.
void test_drag_real_style_leaf_onto_empty_grid_cell_registers_and_launches() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  BrowserModel browser;
  GridModel grid(5);  // same scene count main.cpp actually boots with
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

  // Narrow the browser to EXACTLY one style leaf ("rock" -- deliberately NOT
  // "basic", the already-active style, so a real drag registering it is an
  // unambiguous change), the same single-leaf narrowing discipline test_
  // browser_variation_click_apply_ui_automation.cpp already uses.
  browser.set_category(BrowserCategory::kStyles);
  browser.set_search_filter("rock");

  // Locate frame: mouse parked off-screen. Also the frame render_grid_panel's
  // own seed_demo() seeds the demo grid on -- the drums/scene-0 ("A") and
  // bass/scene-0 ("wlk") cells now exist to be found by their own rendered
  // geometry (a brand-new auto-sized window hides its own very first frame
  // entirely, the same well-known ImGui quirk every sibling test in this
  // directory already works around with a throwaway warm-up frame).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);

  // Source: the "rock" leaf row (leaf_row's own theme::kTextSecondary text
  // color -- unique within browser_tree once every other style is filtered
  // out and Variations/Voices/Kits stay hidden, the default).
  const th::Rect browser_tree = th::find_child_window_rect("browser_tree");
  CHECK(browser_tree.found);
  const ImU32 leaf_text_color = sonotron::neon::u32(sonotron::theme::kTextSecondary);
  const std::vector<th::Rect> leaf_clusters = th::find_color_clusters(locate, leaf_text_color);
  th::Rect rock_leaf;
  for (const th::Rect& r : leaf_clusters) {
    if (r.min.y >= browser_tree.min.y && r.max.y <= browser_tree.max.y) {
      rock_leaf = r;
      break;
    }
  }
  CHECK(rock_leaf.found);

  // Target: the empty perc/scene-0 launch cell. Not directly locatable by
  // color -- EVERY empty cell in the grid paints the identical theme::
  // kInsetBg fill, so color alone cannot distinguish one empty cell from
  // another. Instead: perc (kRows position 1, grid_panel.cpp's own
  // launch_rows.hpp) sits, by construction, exactly ONE row below drums
  // (position 0) and ONE row above bass (position 2) -- both real, FILLED,
  // uniquely-colored demo cells at this SAME scene column (seed_demo's own
  // "A"/"wlk" cells, at theme::kTrackColor[0]/[2] respectively). Interpolating
  // the midpoint between their two REAL rendered centers gives perc/scene-0's
  // own real center, using only rendered geometry -- this harness's own
  // "never a hard-coded pixel position" discipline -- never grid_panel.cpp's
  // internal cz/kCellGap layout constants.
  const ImU32 drums_fill = sonotron::neon::u32(sonotron::theme::kTrackColor[0], 0.10F);
  const std::vector<th::Rect> drums_cells = th::find_color_clusters(locate, drums_fill);
  CHECK(!drums_cells.empty());
  const ImU32 bass_fill = sonotron::neon::u32(sonotron::theme::kTrackColor[2], 0.10F);
  const std::vector<th::Rect> bass_cells = th::find_color_clusters(locate, bass_fill);
  CHECK(!bass_cells.empty());
  if (drums_cells.empty() || bass_cells.empty() || !rock_leaf.found) {
    ImGui::DestroyContext();
    return;
  }
  const ImVec2 drums_c = drums_cells.front().center();
  const ImVec2 bass_c = bass_cells.front().center();
  const ImVec2 perc_scene0_target((drums_c.x + bass_c.x) * 0.5F, (drums_c.y + bass_c.y) * 0.5F);

  // perc/scene-0 must genuinely start empty -- never in seed_demo()'s own
  // 12-cell pattern -- so a real change below can only be attributed to the
  // drag this test drives.
  CHECK(grid.cell(1, 0).kind == sonotron::GridCellKind::kEmpty);

  // THE REAL DRAG: press on the "rock" leaf, drag across into the grid
  // panel, release over the empty perc/scene-0 cell -- io.AddMousePosEvent/
  // AddMouseButtonEvent injection across real multi-frame NewFrame/Render
  // cycles, through the REAL BeginDragDropSource()/AcceptDragDropPayload()
  // state machine, never a direct field write.
  th::drag_to(rock_leaf.center(), perc_scene0_target,
              [&]() { render_one_frame(browser, grid, seqedit, parts, session, app_state, fx); });
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);

  // Host-side effect, synchronous within the drop frame (render_track_cell's
  // own AcceptDragDropPayload block calls model.set_cell() directly): the
  // cell fills for real.
  CHECK(grid.cell(1, 0).kind == sonotron::GridCellKind::kStyleSection);

  // cell_id(role_index=1 [perc], scene=0, scene_count=5) == 1*5 + 0 == 5
  // (grid_panel.cpp's own cell_id formula, hand-applied here since that
  // function is file-local to grid_panel.cpp and not exported).
  constexpr int kPercScene0ClipId = 5;

  // Real click on the now-filled cell: render_track_cell's own click handler
  // only sends `launch clip <id> quantize <n>` for a FILLED cell -- an empty
  // one is a deliberate no-op (owner bug #2, grid_panel.cpp). If the drag
  // above had never really reached AcceptDragDropPayload (e.g. a broken
  // drag_to() cadence engaging BeginDragDropSource too early/late), the cell
  // would still read empty here and this click would be a real no-op, so the
  // launch readback below can only succeed because the drag genuinely
  // delivered the payload.
  click_at(perc_scene0_target, browser, grid, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  const bool launched = pump_until(
      session, browser, grid, seqedit, parts, app_state, fx,
      [](const AppState& app) {
        return app.clip_state(kPercScene0ClipId) != AppState::ClipLaunchState::kStopped;
      },
      3000);

  session.stop();

  // THE REAL ENGINE-OBSERVABLE ASSERTION: the click's `launch clip 5
  // quantize <n>` must have addressed a clip THIS drag genuinely registered
  // with the core's ClipMatrix (the drag's own `clip add ... id 5` reaching
  // the real engine) -- not warned into the void the way an unregistered id
  // always is (Engine::clip_request's own `m_clips.get(id) == nullptr`
  // branch, engine.cpp).
  CHECK(launched);
  CHECK(app_state.clip_state(kPercScene0ClipId) != AppState::ClipLaunchState::kStopped);

  ImGui::DestroyContext();
}

// Scenario 2: a REAL drag of the "chorus" variation leaf onto scene column
// 2's own header, proved real by clicking that same header afterward and
// reading the engine's own section echo change from the pre-drag baseline.
void test_drag_real_variation_leaf_onto_scene_header_changes_launched_section() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  BrowserModel browser;
  GridModel grid(5);
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

  // Narrow the browser to EXACTLY one variation leaf ("chorus" ->
  // kVariationSections[3] == kVarC == wire "varC") -- hide every Styles leaf
  // first (test_browser_variation_click_apply_ui_automation.cpp's own
  // precedent for isolating a single leaf's kTextSecondary color), then
  // reveal Variations narrowed to "chorus". Deliberately NOT "bridge"
  // (varD, the sibling test's own pick) and deliberately NOT seed_demo()'s
  // own scene-2 default (varB) -- a real change to "varC" below can only be
  // attributed to this test's own drag.
  browser.set_search_filter("zzz-no-style-match");  // category defaults kStyles
  browser.set_category_visible(BrowserCategory::kVariations, true);
  browser.set_category(BrowserCategory::kVariations);
  browser.set_search_filter("chorus");

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);

  const th::Rect browser_tree = th::find_child_window_rect("browser_tree");
  CHECK(browser_tree.found);
  const ImU32 leaf_text_color = sonotron::neon::u32(sonotron::theme::kTextSecondary);
  const std::vector<th::Rect> leaf_clusters = th::find_color_clusters(locate, leaf_text_color);
  th::Rect chorus_leaf;
  for (const th::Rect& r : leaf_clusters) {
    if (r.min.y >= browser_tree.min.y && r.max.y <= browser_tree.max.y) {
      chorus_leaf = r;
      break;
    }
  }
  CHECK(chorus_leaf.found);

  // Target: scene column 2's own header drop zone. render_scene_header_cell
  // paints ONE 2px underline per scene column at neon::u32(theme::kCyan,
  // 0.25F) (grid_panel.cpp:1122) -- the only place in this combined render
  // that exact color/alpha pair appears (browser_panel.cpp never uses it;
  // section_header() uses theme::kText, leaf_row() uses theme::kCyan at full
  // alpha only when `active`, never 0.25F). render_scene_header_row iterates
  // scene columns 0..scenes-1 in ascending order (grid_panel.cpp), and this
  // harness's own header comment documents that this codebase's row-major
  // render loops always produce draw order == left-to-right screen order --
  // clusters[2] is therefore scene column 2's own underline. Its rect sits
  // fully inside the "head" InvisibleButton's own hit region (the underline
  // is drawn at `hp0.y + name_area_h - 2.0F`, strictly above the bottom edge
  // of that same InvisibleButton's height, per render_scene_header_cell's own
  // geometry), so its center is a valid click/drop point for "head" itself.
  const ImU32 header_underline = sonotron::neon::u32(sonotron::theme::kCyan, 0.25F);
  const std::vector<th::Rect> header_clusters = th::find_color_clusters(locate, header_underline);
  CHECK(header_clusters.size() >= 3);
  if (header_clusters.size() < 3 || !chorus_leaf.found) {
    ImGui::DestroyContext();
    return;
  }
  const ImVec2 scene2_header_target = header_clusters[2].center();

  // Baseline click: scene 2's own section, BEFORE any drag, is seed_demo()'s
  // own default ("varB", kDemoSections[2]) -- click the header for real
  // (activate_scene_column's own `song build 1 varB ...` verb) and wait for
  // the REAL engine section echo to confirm it, establishing an unambiguous
  // starting point for the change this test actually pins.
  click_at(scene2_header_target, browser, grid, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(pump_until(
      session, browser, grid, seqedit, parts, app_state, fx,
      [](const AppState& app) { return app.section() == "varB"; }, 6000));
  CHECK(app_state.section() == "varB");

  // THE REAL DRAG: press on the "chorus" leaf, drag across into the grid
  // panel, release over scene column 2's own header. render_scene_header_
  // cell's own AcceptDragDropPayload(kVariationDragPayloadId) block only
  // ever calls model.set_scene_section() -- HOST-SIDE state, no wire send
  // (see this file's own header comment) -- so this alone cannot yet be
  // observed through AppState; the real proof comes from the second click
  // below.
  th::drag_to(chorus_leaf.center(), scene2_header_target,
              [&]() { render_one_frame(browser, grid, seqedit, parts, session, app_state, fx); });
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(browser, grid, seqedit, parts, session, app_state, fx);

  // Real click #2: a SEPARATE gesture (fresh mouse-down/up, its own frames),
  // re-firing activate_scene_column against scene 2's now-updated section.
  // Nothing in this test ever calls model.set_scene_section() or
  // session.send("style section varC") directly -- the ONLY way "varC" can
  // reach the engine below is through the drag actually having delivered the
  // "chorus" payload to grid_panel.cpp's real drop-target handler.
  click_at(scene2_header_target, browser, grid, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  const bool section_changed = pump_until(
      session, browser, grid, seqedit, parts, app_state, fx,
      [](const AppState& app) { return app.section() == "varC"; }, 6000);

  session.stop();

  // THE REAL ENGINE-OBSERVABLE ASSERTION.
  CHECK(section_changed);
  CHECK(app_state.section() == "varC");

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_drag_real_style_leaf_onto_empty_grid_cell_registers_and_launches();
  test_drag_real_variation_leaf_onto_scene_header_changes_launched_section();
  return sonotron::test::failures();
}

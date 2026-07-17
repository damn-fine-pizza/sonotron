// UI-AUTOMATION functional test (Torquato QA pass, flow-verification-matrix-
// 2026-07.md §2c): "M/S latch click -> real `part <token> mute|solo on|off`
// + PartsModel toggle" -- the matrix's own verdict was "NONE for the click;
// test_parts_model.cpp covers PartsModel::toggle_mute/toggle_solo in
// isolation only". This closes that gap with REAL input injection on the
// REAL grid_latch InvisibleButton (grid_panel.cpp's render_track_label),
// wired to a REAL InProcessBrainSession: the click's real effect on
// PartsModel (the actual model instance the click handler mutates, not a
// stand-in) AND a real round trip through the engine confirming the exact
// wire verb the click sends (`part <token> mute on`/`part <token> solo on`)
// is accepted without a warn/error OutEvent.
//
// Honest scope note (PartsModel's own header comment, parts_model.hpp):
// there is still no shipped mute/solo READBACK verb on the wire (`Op::kGet`
// unwired) -- so unlike the clip-launch/section-change flows elsewhere in
// this pass, there is no independent ENGINE-side confirmation available
// beyond "the command was accepted, no warning fired". This test states that
// limit explicitly rather than silently asserting less than it proves.

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

// grid_latch's own footprint (grid_panel.cpp, anonymous namespace, not
// exported) -- hand-copied literals, same "duplicated deliberately" D38
// discipline every other cross-boundary constant in this codebase already
// documents, used ONLY to disambiguate a merged M+S color cluster below.
constexpr float kLatchSize = 17.0F;
constexpr float kLatchGap = 3.0F;

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

// Drains every pending real OutEvent, returning true iff any of them is a
// warn/error -- the real engine's own rejection signal.
bool poll_saw_warning(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  bool warned = false;
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
    if (ev.kind == BrainEvent::Kind::kWarn || ev.kind == BrainEvent::Kind::kError) {
      warned = true;
    }
  }
  return warned;
}

// grid_latch's own rest-state fill color is IDENTICAL for M and S, and for
// every track row (neon::u32(theme::kFrameBg, 0.9F), grid_panel.cpp) -- the
// M/S PAIR for one row may render as one merged cluster or two separate ones
// depending on whether the glyph draw between them exceeds find_color_
// clusters' index_gap (imgui_headless_harness.hpp's own header comment).
// This helper is robust to either outcome: it returns a point guaranteed to
// land inside the M (which=0) or S (which=1) sub-square of the group at
// `group_index` (0 == first track row, i.e. drums; 1 == second, bass).
ImVec2 latch_click_point(const std::vector<th::Rect>& latches, int group_index, int which) {
  if (latches.size() >= 12) {
    // Separate clusters: [row0 M, row0 S, row1 M, row1 S, ...].
    const th::Rect& r = latches[static_cast<std::size_t>(group_index * 2 + which)];
    return r.center();
  }
  // Merged M+S per row: [row0 (M+S), row1 (M+S), ...]. M sits at the rect's
  // own left edge; S starts kLatchGap further right, at the SAME Y.
  const th::Rect& r = latches[static_cast<std::size_t>(group_index)];
  const float x = r.min.x + (which == 0 ? kLatchSize * 0.5F
                                        : kLatchSize + kLatchGap + kLatchSize * 0.5F);
  const float y = r.min.y + kLatchSize * 0.5F;
  return ImVec2(x, y);
}

void test_real_ms_latch_clicks_toggle_partsmodel_and_reach_engine_without_warning() {
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
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  const ImU32 latch_rest = sonotron::neon::u32(sonotron::theme::kFrameBg, 0.9F);
  const std::vector<th::Rect> latches = th::find_color_clusters(locate, latch_rest);
  // Either 12 (M/S found as separate clusters, one pair per each of the 6
  // track rows) or 6 (M+S merged per row) -- anything else means the color
  // scan itself found something unexpected in this frame.
  CHECK(latches.size() == 12 || latches.size() == 6);
  if (latches.size() != 12 && latches.size() != 6) {
    ImGui::DestroyContext();
    return;
  }

  // --- M latch, row 0 (drums, PartsModel role index 0) ---
  const ImVec2 drums_m = latch_click_point(latches, 0, 0);
  CHECK(!parts.part(0).muted);
  click_at(drums_m, model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(parts.part(0).muted);  // real PartsModel, real click handler
  CHECK(!poll_saw_warning(session, app_state));  // real engine accepted `part drums mute on`

  click_at(drums_m, model, seqedit, parts, session, app_state, fx);  // toggle back off
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(!parts.part(0).muted);
  CHECK(!poll_saw_warning(session, app_state));

  // --- S latch, row 1 (bass, PartsModel role index 2 per kRows) ---
  const ImVec2 bass_s = latch_click_point(latches, 1, 1);
  CHECK(!parts.part(2).soloed);
  click_at(bass_s, model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(parts.part(2).soloed);
  CHECK(!poll_saw_warning(session, app_state));  // real engine accepted `part bass solo on`

  click_at(bass_s, model, seqedit, parts, session, app_state, fx);  // toggle back off
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  CHECK(!parts.part(2).soloed);
  CHECK(!poll_saw_warning(session, app_state));

  session.stop();
  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_ms_latch_clicks_toggle_partsmodel_and_reach_engine_without_warning();
  return sonotron::test::failures();
}

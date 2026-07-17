// UI-AUTOMATION PIN (originally Torquato QA pass, owner bug #2: "Scene cell
// preview is wrong: the launch-cell mini-preview does not match the
// Sequence Edit canvas (which the owner confirms is correct) for the same
// clip." -- the original column-crop root cause below was fixed in dff4e9e;
// this pin now also covers owner bug #3, the one-bar TRUNCATION both panels
// used to share: neither view drew a multi-bar section's bar 2 at all).
//
// Drives the REAL render_grid_panel AND the REAL render_seqedit_panel
// entry points (grid_panel.cpp / seqedit_panel.cpp) inside one headless
// ImGui frame, opens a REAL launch cell with a REAL click (imgui_headless_
// harness.hpp's click injection -- io.AddMousePosEvent/AddMouseButtonEvent,
// never a direct fx.open_cell/fx.open_row field write), and asserts on the
// ACTUAL RENDERED DRAW DATA of BOTH panels for the SAME clip: how many
// note-bar quads did each panel actually paint. No AppState/UiState field
// is compared -- only what ImGui really drew.
//
// Fixture, already independently proven by test_preview.cpp's own
// test_known_value_fixed_role_drums(): style "basic", section kVarA (a
// 2-bar section, Wave-1 style-depth), role_index 0 (drums) resolves to a
// REAL note (GM closed hi-hat, pitch 42, opening to pitch 46 on the bar-2
// turnaround) at steps {0,2,4,6,8,10,12,14} in bar 1 AND {16,18,20,22,24,
// 26,28,30} in bar 2 -- 16 real notes total across the full 2-bar window.
// Both the grid mini-preview crop (fixed, dff4e9e) and the one-bar
// truncation (fixed alongside this task) used to make grid_panel.cpp and
// seqedit_panel.cpp structurally diverge from each other and from the real
// content; this pin now asserts BOTH views paint every real note, in every
// bar, and agree with each other.
//
// grid_panel.cpp's own seed_demo() already seeds exactly this fixture on
// boot with zero custom setup: row 0 (drums, kRows[0].role_index == 0),
// scene 1 is filled ("B") and scene 1's section defaults to kVarA (the demo
// table's `{1, preview::Section::kVarA, "Var A"}"} -- see grid_panel.cpp's
// own seed_demo comment) -- so this test only needs to set fx.active_style
// to "basic" and click that one, already-seeded cell for real.

#include "imgui.h"
#include "imgui_internal.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/neon_widgets.hpp"
#include "src/parts_model.hpp"
#include "src/preview.hpp"
#include "src/seqedit_model.hpp"
#include "src/seqedit_panel.hpp"
#include "src/theme.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <cstddef>
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
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// Spy BrainSession (test double, not a product dependency, same shape as
// test_grid_panel_auto_song.cpp's own): this test only cares about RENDERED
// OUTPUT, never what gets sent on launch, so a real InProcessBrainSession
// (and its engine thread) is not needed here at all -- keeping this test
// fast and fully deterministic.
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

// One headless ImGui frame around BOTH real panels this bug spans: the
// Repeat Zone grid (the real launch cell + its real mini-preview) and
// Sequence Edit (the real canvas for whatever cell is open) -- laid out one
// under the other inside a single wide "test" window, matching main.cpp's
// own vertically-stacked zone layout in spirit (ux-workstation.md §3: Repeat
// Zone above Sequence Edit) closely enough that the two panels' own draw
// regions never overlap.
ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, UiState& fx) {
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1200.0F, 700.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("repeat_zone", ImVec2(0, 340.0F), ImGuiChildFlags_None);
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::EndChild();
  ImGui::Spacing();
  ImGui::BeginChild("sequence_edit", ImVec2(0, 0), ImGuiChildFlags_None);
  sonotron::render_seqedit_panel(seqedit, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

void test_grid_cell_preview_matches_sequence_edit_across_all_bars() {
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
  UiState fx;
  // Maximum zoom (grid_panel.cpp's own -/+ clamp tops out at 88px, render_
  // header) -- gives the 8-column-cropped mini-preview as much room as this
  // UI can ever give it, so this pin cannot be dismissed as "too small a
  // cell to show more notes"; the crop is a hard-coded COLUMN COUNT
  // (ClipPattern::kCellSteps), not a pixel-size limitation.
  fx.cell_zoom = 88.0F;

  for (std::size_t i = 0; i < sonotron::kBuiltinStyleNames.size(); ++i) {
    if (sonotron::kBuiltinStyleNames[i] == "basic") {
      fx.active_style = static_cast<int>(i);
      break;
    }
  }

  // Frame 1: seed_demo() latches (fx.seeded), populating drums/scene1 = "B"
  // at section kVarA -- verify the fixture this pin depends on independently
  // via the SAME library call render_track_cell itself uses (preview::
  // preview_for), before trusting any rendered geometry.
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  CHECK(model.scene_section(1) == static_cast<std::uint8_t>(sonotron::preview::Section::kVarA));
  const sonotron::preview::PreviewPattern pp =
      sonotron::preview::preview_for(fx.active_style, sonotron::preview::Section::kVarA,
                                     /*role_index=*/0);
  CHECK(pp.approx ==
        false);         // kFixed drums role: literal, not resolved against a placeholder chord
  CHECK(pp.bars == 2);  // basic::kVarA is a 2-bar section (Wave-1 style-depth)
  // Column occupancy (a STEP counts once if ANY voice slot is real content),
  // matching count_occupied_columns_in_band()'s own per-column counting
  // below -- steps 0,4,8,12,16,20,24,28 actually carry TWO simultaneous
  // voices each (kick/snare + hat, owner bug #13), but they still paint as
  // ONE occupied column each, same as the hat-only steps. Scanned across
  // BOTH bars (owner bug #2/#3: previewing only bar 1 used to silently drop
  // bar 2's real content) -- `total_steps` bounds the scan to exactly the
  // bars this section actually has, never a hardcoded one-bar window.
  const int total_steps = pp.bars * sonotron::preview::kSteps;
  int real_note_count = 0;
  for (int step = 0; step < total_steps; ++step) {
    bool step_has_content = false;
    for (int v = 0; v < sonotron::preview::kMaxVoicesPerStep; ++v) {
      if (pp.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(v)] >= 0) {
        step_has_content = true;
        break;
      }
    }
    if (step_has_content) {
      ++real_note_count;
    }
  }
  // 16 real notes: bar 1's steps 0,2,4,6,8,10,12,14 (8) PLUS bar 2's steps
  // 16,18,20,22,24,26,28,30 (8, the same backbeat groove repeated verbatim,
  // the hat only turning to an open hit on the last one) -- independently
  // pinned by test_preview.cpp's own test_known_value_fixed_role_drums(). If
  // this ever fails, the fixture itself changed (e.g. a Wave-1 style edit)
  // and this test's own root-cause reasoning needs re-deriving, not
  // silencing.
  CHECK(real_note_count == 16);

  // Locate the drums/scene-1 cell ("B") for real: row 0's fill color at
  // rest (grid_panel.cpp's draw_cell, non-hovered/non-playing fill_a ==
  // 0.10F) -- draw order gives scene 0 ("A") first, scene 1 ("B") second
  // (grid_panel.cpp's render_track_row iterates scene columns left to
  // right, cluster[0]/[1] preserve that draw order, see imgui_headless_
  // harness.hpp's own find_color_clusters comment for why this is index-gap
  // clustering, not X-distance clustering).
  ImGui::GetIO().MousePos = ImVec2(-100.0F, -100.0F);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  ImDrawData* locate = render_one_frame(model, seqedit, parts, brain, app_state, fx);
  const ImU32 drums_not_playing = sonotron::neon::u32(sonotron::theme::kTrackColor[0], 0.10F);
  const std::vector<th::Rect> drums_cells = th::find_color_clusters(locate, drums_not_playing);
  CHECK(drums_cells.size() >= 2);
  if (drums_cells.size() < 2) {
    ImGui::DestroyContext();
    return;
  }
  const th::Rect scene1_cell = drums_cells[1];

  // Real click: open this cell in Sequence Edit for real (render_track_
  // cell's REAL click handler -- filled cell -> sets fx.open_cell/fx.
  // open_row/fx.open_section and seqedit.set_part_index/set_clip_label --
  // never hand-written by this test).
  th::queue_mouse_down(scene1_cell.center());
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  th::queue_mouse_up(scene1_cell.center());
  render_one_frame(model, seqedit, parts, brain, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  ImDrawData* final_draw_data = render_one_frame(model, seqedit, parts, brain, app_state, fx);

  CHECK(fx.open_cell >= 0);  // sanity: the click really opened something

  // Re-locate the (now possibly re-tinted-on-hover-during-click, but stable
  // in POSITION) drums/scene-1 cell rect once more on the settled frame, so
  // the note-bar count below is scoped to the SAME cell regardless of any
  // fill-color/hover change the click itself caused.
  const std::vector<th::Rect> drums_cells_final =
      th::find_color_clusters(final_draw_data, drums_not_playing);
  th::Rect grid_cell_rect = scene1_cell;
  if (drums_cells_final.size() >= 2) {
    grid_cell_rect = drums_cells_final[1];
  }

  // The grid cell's own inner preview band -- grid_panel.cpp's draw_cell,
  // verbatim: `in0(p0.x+6, p0.y+size*0.22); in1(p1.x-6, p1.y-14)`. Read
  // directly off this file's own header comment (a stable, documented
  // constant, not a guess) -- `size` is fx.cell_zoom, set above.
  th::Rect grid_band;
  grid_band.min = ImVec2(grid_cell_rect.min.x + 6.0F, grid_cell_rect.min.y + fx.cell_zoom * 0.22F);
  grid_band.max = ImVec2(grid_cell_rect.max.x - 6.0F, grid_cell_rect.max.y - 14.0F);
  grid_band.found = true;

  // The Sequence Edit canvas rect -- ImGui's own child-window bookkeeping
  // (imgui_headless_harness.hpp's find_child_window_rect), not a guess:
  // seqedit_panel.cpp's own canvas child is named "seq_canvas".
  const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
  CHECK(canvas_rect.found);

  // Both panels paint their note bars with the EXACT SAME fill color/alpha
  // (grid_panel.cpp's clip_preview_pianoroll and seqedit_panel.cpp's own
  // per-step loop both call `neon::u32(track_color, 0.85F)`) -- only the
  // two panels' own disjoint screen regions (grid_band vs canvas_rect,
  // discovered above, never overlapping) distinguish which one painted
  // which bar. Both bands are sampled at a divisor SCALED by `pp.bars`
  // (owner bug #2/#3: the fixed-resolution one-bar divisors used to
  // silently discard bar 2's real content from this comparison, the same
  // truncation the product code itself used to have) -- the grid mini-cell
  // keeps its own coarser sampling relative to the canvas (kCellSteps :
  // kSteps == 1:2, unchanged), just scaled by the section's real bar count.
  const ImU32 note_color = sonotron::neon::u32(sonotron::theme::kTrackColor[0], 0.85F);
  const int grid_notes_shown = th::count_occupied_columns_in_band(
      final_draw_data, note_color, grid_band, pp.bars * sonotron::neon::ClipPattern::kCellSteps);
  const int seqedit_notes_shown =
      canvas_rect.found
          ? th::count_occupied_columns_in_band(final_draw_data, note_color, canvas_rect,
                                               pp.bars * sonotron::neon::ClipPattern::kSteps)
          : -1;

  // Diagnostic (not an assertion): the actual rendered note-bar counts, so a
  // test-log reader sees the concrete divergence (if any) without having to
  // re-derive it from the CHECK line below.
  std::printf("grid_notes_shown=%d seqedit_notes_shown=%d\n", grid_notes_shown,
              seqedit_notes_shown);

  // Sanity (should PASS): Sequence Edit really does paint every real note of
  // this clip, across BOTH bars (owner bug #2/#3 fix), not just bar 1.
  CHECK(seqedit_notes_shown == real_note_count);

  // THE RENDERED-OUTPUT PIN: the SAME clip's launch-cell mini-preview must
  // show the SAME note content Sequence Edit shows -- that is the whole
  // premise of grid_panel.cpp's own render_track_cell comment ("the SAME
  // preview_for(...) the opened cell's Sequence Edit canvas uses"). Before
  // the owner bug #2 crop fix (dff4e9e) this used to structurally diverge;
  // now both panels draw the identical (pitch, bars) content, so this holds
  // for every bar, not just bar 1.
  CHECK(grid_notes_shown == seqedit_notes_shown);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_grid_cell_preview_matches_sequence_edit_across_all_bars();
  return sonotron::test::failures();
}

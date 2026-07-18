// UI-AUTOMATION functional test (owner-reported live bug, 2026-07-18:
// "Sequence Edit still doesn't show all tracks"). Root cause: seqedit_
// panel.cpp's draw_role_pattern used to be called once per visible role but
// drew EVERY role into the SAME shared p0..p1 rectangle on the SAME pitch
// axis, so all visible roles mutually occluded each other in one lane --
// the model correctly marked every role visible (SeqEditModel::role_visible
// defaults true), but the RENDER put them all on top of one another. The fix
// (seqedit_panel.cpp's draw_piano_roll_lanes) partitions the canvas into one
// horizontal LANE per visible role.
//
// This test drives the REAL render_seqedit_panel entry point (no click
// injection needed -- this pin is about RENDERED OUTPUT for a given model
// state, the click path itself is already covered by test_grid_cell_launch_
// open_seqedit_ui_automation.cpp) and asserts on the ACTUAL RENDERED DRAW
// DATA: two visible roles with independently known, non-empty note content
// (style "basic", section kVarA -- drums role_index 0 and bass role_index 2,
// both pinned by test_preview.cpp's own test_known_value_fixed_role_drums/
// test_known_value_resolved_role_bass) must paint their note quads into
// TWO DISJOINT vertical (Y) ranges, never overlapping -- the rendered proof
// that these are separate lanes, not one shared occluded rect.

#include "imgui.h"
#include "src/neon_widgets.hpp"
#include "src/preview.hpp"
#include "src/seqedit_model.hpp"
#include "src/seqedit_panel.hpp"
#include "src/theme.hpp"
#include "src/track_roles.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <cstddef>
#include <cstdint>

using sonotron::SeqEditModel;
using sonotron::SeqEditView;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

ImDrawData* render_one_frame(SeqEditModel& seqedit, UiState& fx) {
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1200.0F, 700.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("sequence_edit", ImVec2(0, 0), ImGuiChildFlags_None);
  sonotron::render_seqedit_panel(seqedit, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

void test_piano_roll_draws_visible_roles_in_disjoint_lanes() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  SeqEditModel seqedit;
  UiState fx;

  // Fixture: style 0 ("basic"), section kVarA, drums (role_index 0) opened.
  // Both drums and bass (role_index 2) have independently pinned, non-empty
  // note content for this exact (style, section) pair (test_preview.cpp),
  // so a non-empty rendered rect for each is a real assertion, never
  // vacuously true.
  fx.active_style = 0;
  fx.open_cell = 0;
  fx.open_row = 0;  // -> track_color == theme::kTrackColor[0] == kCyan
  fx.open_section = static_cast<std::uint8_t>(sonotron::preview::Section::kVarA);
  fx.open_audio = false;
  fx.open_wav = false;
  seqedit.set_part_index(0);  // drums opened -- draws with full track_color/0.85F/glow
  seqedit.set_clip_label("A");
  seqedit.set_view(SeqEditView::kPianoRoll);
  // Only drums (opened) and bass visible -- isolates the two lanes this
  // test cares about; every other role's dim overlay would just add noise
  // to the color scan below, not change what this test proves.
  for (std::size_t role = 0; role < sonotron::kTrackRoleCount; ++role) {
    seqedit.set_role_visible(role, role == 0 || role == 2);
  }

  ImDrawData* draw_data = render_one_frame(seqedit, fx);

  const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
  CHECK(canvas_rect.found);

  // Drums (opened role, lane 0): full track_color at 0.85F alpha -- the same
  // exact color/alpha draw_role_pattern always used for the opened role,
  // before and after this fix (see seqedit_panel.cpp's draw_piano_roll_lanes).
  const ImU32 drums_color = sonotron::neon::u32(sonotron::theme::kTrackColor[0], 0.85F);
  // Bass (not opened, lane 1): dimmed role tint, no glow -- kRoleTint[2] is
  // kBlue, distinct from drums' kCyan, so the two colors can never collide.
  const ImU32 bass_color = sonotron::neon::u32(sonotron::theme::kRoleTint[2], 0.45F);

  const th::Rect drums_rect = th::find_single_color_rect(draw_data, drums_color);
  const th::Rect bass_rect = th::find_single_color_rect(draw_data, bass_color);

  // Sanity: both roles actually painted real note content this frame (not a
  // vacuous "nothing overlapped because nothing was drawn").
  CHECK(drums_rect.found);
  CHECK(bass_rect.found);
  if (!drums_rect.found || !bass_rect.found) {
    ImGui::DestroyContext();
    return;
  }

  // THE LANE-PARTITION PIN: drums (lane 0, drawn first) and bass (lane 1,
  // drawn second) must occupy DISJOINT vertical ranges -- one lane's own
  // note quads never reach into the other lane's Y range. Before this fix,
  // both roles drew into the SAME p0..p1 rect on the SAME pitch axis, so
  // their Y ranges would have overlapped (both spanning the same band,
  // differing only by which of the 5 pitch rows a given note landed on).
  const bool disjoint =
      drums_rect.max.y <= bass_rect.min.y + 1.0F || bass_rect.max.y <= drums_rect.min.y + 1.0F;
  CHECK(disjoint);

  // Both lanes stay within the canvas's own rect (no stray content drawn
  // outside the child window) -- with only 2 roles visible, 2*kLaneH is
  // comfortably under this test window's avail height, so no scrolling is
  // needed here and both lanes render fully on-screen.
  CHECK(drums_rect.min.y >= canvas_rect.min.y - 1.0F);
  CHECK(bass_rect.max.y <= canvas_rect.max.y + 1.0F);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_piano_roll_draws_visible_roles_in_disjoint_lanes();
  return sonotron::test::failures();
}

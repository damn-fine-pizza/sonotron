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
//
// Fabrizio review (2026-07-18, track-set + content divergence from the
// Repeat Zone): render_seqedit_panel now also takes a GridModel& (the SAME
// one the Repeat Zone renders), and every lane's content is resolved through
// launch_rows.hpp's shared resolve_track_cell_preview() against a REAL
// GridCell instead of a static style-table lookup keyed only by role -- see
// each test's own fixture-setup comment for the GridModel cells it seeds.

#include "imgui.h"
#include "imgui_internal.h"  // ImGuiWindow/ImGuiWindowFlags_Popup -- locating the "+ track" popup below
#include "src/grid_model.hpp"
#include "src/launch_rows.hpp"
#include "src/neon_widgets.hpp"
#include "src/preview.hpp"
#include "src/seqedit_model.hpp"
#include "src/seqedit_panel.hpp"
#include "src/theme.hpp"
#include "src/track_roles.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using sonotron::GridCellKind;
using sonotron::GridModel;
using sonotron::SeqEditModel;
using sonotron::SeqEditView;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

ImDrawData* render_one_frame(GridModel& grid, SeqEditModel& seqedit, UiState& fx) {
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1200.0F, 700.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("sequence_edit", ImVec2(0, 0), ImGuiChildFlags_None);
  sonotron::render_seqedit_panel(seqedit, fx, grid);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

// Seeds a real, non-empty kStyleSection cell for `role_index` at scene 0,
// section kVarA (the SAME (style, section) pair test_preview.cpp's own
// test_known_value_fixed_role_drums/test_known_value_resolved_role_bass
// already pin for role 0/2) -- so draw_piano_roll_lanes' shared resolve_
// track_cell_preview() call resolves REAL content for that lane, not the
// empty-cell blank default.
void seed_style_section_cell(GridModel& grid, std::size_t role_index) {
  grid.set_scene_section(0, static_cast<std::uint8_t>(sonotron::preview::Section::kVarA));
  grid.set_cell(role_index, 0, GridCellKind::kStyleSection, "A");
}

void test_piano_roll_draws_visible_roles_in_disjoint_lanes() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel grid;
  SeqEditModel seqedit;
  UiState fx;

  // Fixture: style 0 ("basic"), section kVarA, drums (role_index 0) opened.
  // Both drums and bass (role_index 2) have independently pinned, non-empty
  // note content for this exact (style, section) pair (test_preview.cpp), so
  // a non-empty rendered rect for each is a real assertion, never vacuously
  // true. Both roles' own GridModel cells are seeded as real kStyleSection
  // content at scene 0 -- draw_piano_roll_lanes now resolves each lane's
  // content from `grid`, not a bare (style, role) style-table lookup.
  fx.active_style = 0;
  fx.open_cell = 0;
  fx.open_row = 0;  // -> track_color == theme::kTrackColor[0] == kCyan
  fx.open_scene = 0;
  fx.open_section = static_cast<std::uint8_t>(sonotron::preview::Section::kVarA);
  fx.open_audio = false;
  fx.open_wav = false;
  seqedit.set_part_index(0);  // drums opened -- draws with full track_color/0.85F/glow
  seqedit.set_clip_label("A");
  seqedit.set_view(SeqEditView::kPianoRoll);
  seed_style_section_cell(grid, 0);  // drums
  seed_style_section_cell(grid, 2);  // bass
  // Only drums (opened) and bass visible -- isolates the two lanes this
  // test cares about; every other role's dim overlay would just add noise
  // to the color scan below, not change what this test proves.
  for (std::size_t role = 0; role < sonotron::kTrackRoleCount; ++role) {
    seqedit.set_role_visible(role, role == 0 || role == 2);
  }

  ImDrawData* draw_data = render_one_frame(grid, seqedit, fx);

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

// UI-AUTOMATION functional test (owner decision, 2026-07-18: the role name
// used to render TWICE -- once in the now-deleted left-hand sidebar, once on
// the lane itself -- so the sidebar, including its bulk "all tracks"/"last
// track" toggle and per-role checkbox list, is gone entirely; each lane's
// own header now carries a small "x" HIDE button, and a NEW "+ track" popup
// at the top of the panel re-shows whatever is currently hidden). This test
// drives BOTH real controls through REAL input injection (no direct model
// mutation): clicking a lane's own "x" button must hide that lane (both the
// model flag AND the lane's own rendered content disappearing), and picking
// that role back from the "+ track" popup must restore it -- the two ends of
// the replacement show/hide affordance, proven end to end.
//
// Fixture note: this uses SeqEditModel's TRUE DEFAULT visibility (every
// role, including the 3 non-kRows roles, starts visible=true) rather than
// pre-hiding roles, so hiding bass leaves EXACTLY ONE kRows role hidden --
// the "+ track" popup's candidate set is launch_rows.hpp's kRows (6 roles),
// not all 9 (Fabrizio review, 2026-07-18), so with the true default fixture
// the popup renders exactly one MenuItem ("Bass") after this test's single
// hide-click, and a click anywhere inside the (AlwaysAutoResize) popup rect
// reliably hits it.
void test_lane_hide_button_and_add_track_menu_toggle_visibility() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel grid;
  SeqEditModel seqedit;
  UiState fx;

  fx.active_style = 0;
  fx.open_cell = 0;
  fx.open_row = 0;
  fx.open_scene = 0;
  fx.open_section = static_cast<std::uint8_t>(sonotron::preview::Section::kVarA);
  fx.open_audio = false;
  fx.open_wav = false;
  seqedit.set_part_index(0);
  seqedit.set_clip_label("A");
  seqedit.set_view(SeqEditView::kPianoRoll);
  seed_style_section_cell(grid, 0);  // drums
  seed_style_section_cell(grid, 2);  // bass
  // No visibility overrides: SeqEditModel's constructor already defaults
  // every role visible, so all 6 kRows lanes (drums, bass, chord1, pad, arp,
  // lead, in that order) render this frame.

  // Both the "+ track" header button and every lane's own "x" hide button
  // are plain, unstyled ImGui::SmallButton()s (no ImGuiCol_Button push,
  // unlike mode_tab's own explicitly-tinted buttons) -- they all paint the
  // SAME rest-state fill color, queried straight off the live style rather
  // than a guessed constant.
  const ImU32 button_color = ImGui::GetColorU32(ImGuiCol_Button);
  const ImU32 bass_color = sonotron::neon::u32(sonotron::theme::kRoleTint[2], 0.45F);

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(grid, seqedit,
                   fx);  // warm-up, mirrors the sibling click-driven tests' own idiom
  ImDrawData* locate = render_one_frame(grid, seqedit, fx);

  CHECK(seqedit.role_visible(2));
  CHECK(th::find_single_color_rect(locate, bass_color).found);

  const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
  CHECK(canvas_rect.found);

  // Classify every button-colored cluster by whether it sits above the
  // canvas (the header's own "+ track" button) or inside it (one lane's own
  // "x" each) -- 6 lanes (all of kRows, default-visible) means 6 in-canvas
  // buttons plus the header's "+ track" button, 7 total. Sorted by Y since
  // draw order and screen order coincide for this top-to-bottom lane stack,
  // matching kRows' own iteration order (drums, bass, chord1, pad, arp,
  // lead) -- bass is therefore the SECOND lane from the top, index 1.
  const std::vector<th::Rect> buttons = th::find_color_clusters(locate, button_color);
  CHECK(buttons.size() == sonotron::kRows.size() + 1);
  th::Rect track_button;
  std::vector<th::Rect> in_canvas_buttons;
  for (const th::Rect& r : buttons) {
    if (r.center().y < canvas_rect.min.y) {
      track_button = r;
    } else {
      in_canvas_buttons.push_back(r);
    }
  }
  CHECK(track_button.found);
  CHECK(in_canvas_buttons.size() == sonotron::kRows.size());
  if (!track_button.found || in_canvas_buttons.size() < 2) {
    ImGui::DestroyContext();
    return;
  }
  std::sort(in_canvas_buttons.begin(), in_canvas_buttons.end(),
            [](const th::Rect& a, const th::Rect& b) { return a.min.y < b.min.y; });
  const th::Rect bass_hide_button = in_canvas_buttons[1];  // kRows[1] == bass, 2nd from top

  // Real click #1: hide the bass lane via its own "x" button. The model
  // mutates synchronously on the mouse-up (release) frame, but that SAME
  // frame's canvas already computed its lane list BEFORE the click fired
  // (compute_piano_roll_lanes runs before draw_piano_roll_lanes reaches
  // bass' own "x" button in the loop) -- so bass still paints once more
  // that frame, and only the FOLLOWING settle frame's canvas actually drops
  // the now-hidden lane. Assert the rendered-content proof against that
  // settle frame, not the click frame itself.
  th::queue_mouse_down(bass_hide_button.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_up(bass_hide_button.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  ImDrawData* after_hide = render_one_frame(grid, seqedit, fx);

  CHECK(!seqedit.role_visible(2));
  CHECK(!th::find_single_color_rect(after_hide, bass_color).found);

  // Real click #2: reopen the "+ track" popup (its rect is unaffected by
  // bass' lane disappearing -- the header layout above the canvas never
  // changes shape from a lane hide/show). A freshly-opened AlwaysAutoResize
  // popup establishes its final size on the SAME frame it opens but is not
  // yet stably hit-testable for a synthetic click until one settle frame
  // later, so render one extra frame before reading its rect below.
  th::queue_mouse_down(track_button.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_up(track_button.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(grid, seqedit, fx);

  // Locate the popup window Dear ImGui just opened (ImGuiWindowFlags_Popup)
  // -- read-only introspection of the already-vendored imgui_internal.h,
  // same discipline find_child_window_rect above already uses. With exactly
  // one hidden kRows role (bass), the popup renders exactly one MenuItem
  // ("Bass"), so clicking anywhere inside the (AlwaysAutoResize) popup rect
  // hits it.
  th::Rect popup_rect;
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  for (ImGuiWindow* w : ctx->Windows) {
    if (w != nullptr && (w->Flags & ImGuiWindowFlags_Popup) != 0) {
      const ImRect wr = w->Rect();
      popup_rect.min = wr.Min;
      popup_rect.max = wr.Max;
      popup_rect.found = true;
    }
  }
  CHECK(popup_rect.found);
  if (!popup_rect.found) {
    ImGui::DestroyContext();
    return;
  }

  // Real click #3: pick "Bass" from the popup -- re-shows it.
  th::queue_mouse_down(popup_rect.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_up(popup_rect.center());
  render_one_frame(grid, seqedit, fx);

  CHECK(seqedit.role_visible(2));

  ImGui::DestroyContext();
}

// Fabrizio review (2026-07-18, track-set divergence): the Repeat Zone's own
// launch grid only ever has 6 real rows (launch_rows.hpp's kRows -- drums,
// bass, chord1, pad, arp, lead); the other 3 TrackRole values (Perc,
// Chord2, Phrase) have no corresponding launch-grid row at all. Before this
// fix, Sequence Edit's lane set walked ALL 9 TrackRole values (filtered only
// by SeqEditModel::role_visible, which defaults every role -- including
// those 3 -- to true), so those 3 phantom lanes rendered with no matching
// Repeat-Zone row to represent. This test proves BOTH halves of the fix: (1)
// the lane set is capped at kRows even though the 3 non-kRows roles report
// role_visible() == true (the default), and (2) hiding a non-kRows role is a
// harmless no-op for the rendered lane set (it was never a candidate),
// while hiding a REAL kRows role does shrink it -- so the test cannot be
// satisfied by an implementation that is merely insensitive to role_visible
// in general.
void test_lane_set_is_capped_to_launch_rows_never_all_nine_roles() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel grid;
  SeqEditModel seqedit;
  UiState fx;

  fx.active_style = 0;
  fx.open_cell = 0;
  fx.open_row = 0;
  fx.open_scene = 0;
  fx.open_section = static_cast<std::uint8_t>(sonotron::preview::Section::kVarA);
  fx.open_audio = false;
  fx.open_wav = false;
  seqedit.set_part_index(0);
  seqedit.set_clip_label("A");
  seqedit.set_view(SeqEditView::kPianoRoll);
  seed_style_section_cell(grid, 0);  // drums, the only real content needed here

  const ImU32 button_color = ImGui::GetColorU32(ImGuiCol_Button);

  auto count_in_canvas_x_buttons = [&](ImDrawData* draw_data) -> std::size_t {
    const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
    CHECK(canvas_rect.found);
    std::size_t count = 0;
    for (const th::Rect& r : th::find_color_clusters(draw_data, button_color)) {
      if (r.center().y >= canvas_rect.min.y) {
        ++count;
      }
    }
    return count;
  };

  // Frame 1: every role at SeqEditModel's true default (all 9 visible,
  // including Perc/Chord2/Phrase). The lane set (one "x" button per lane)
  // must still be exactly kRows.size() (6), never kTrackRoleCount (9).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(grid, seqedit, fx);
  ImDrawData* all_default = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_x_buttons(all_default) == sonotron::kRows.size());

  // Frame 2: hide all 3 non-kRows roles (Perc=1, Chord2=4, Phrase=7). None of
  // them was ever a lane candidate, so the lane count must stay UNCHANGED at
  // kRows.size() -- a real regression to "walk all 9 roles" would instead
  // drop 3 lanes here.
  seqedit.set_role_visible(1, false);
  seqedit.set_role_visible(4, false);
  seqedit.set_role_visible(7, false);
  ImDrawData* non_launch_hidden = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_x_buttons(non_launch_hidden) == sonotron::kRows.size());

  // Frame 3: now hide one REAL kRows role (Arp, role_index 6). The lane
  // count must drop to kRows.size() - 1, proving this test is not simply
  // insensitive to role_visible altogether.
  seqedit.set_role_visible(6, false);
  ImDrawData* arp_hidden = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_x_buttons(arp_hidden) == sonotron::kRows.size() - 1);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_piano_roll_draws_visible_roles_in_disjoint_lanes();
  test_lane_hide_button_and_add_track_menu_toggle_visibility();
  test_lane_set_is_capped_to_launch_rows_never_all_nine_roles();
  return sonotron::test::failures();
}

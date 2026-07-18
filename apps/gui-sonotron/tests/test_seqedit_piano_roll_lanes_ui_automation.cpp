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
  // Only drums (opened) and bass have bars visible -- isolates the two
  // lanes this test cares about; every other kRows lane still renders its
  // own name+checkbox (draw_piano_roll_lanes always renders all 6 kRows
  // lanes now, regardless of role_visible), but with no seeded content and
  // no bars, it adds no note-color noise to the color scan below.
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

  // THE LANE-PARTITION PIN: drums (lane 0, fixed kRows order) and bass (lane
  // 1, fixed kRows order) must occupy DISJOINT vertical ranges -- one lane's
  // own note quads never reach into the other lane's Y range. Before this
  // fix, both roles drew into the SAME p0..p1 rect on the SAME pitch axis,
  // so their Y ranges would have overlapped (both spanning the same band,
  // differing only by which of the 5 pitch rows a given note landed on).
  const bool disjoint =
      drums_rect.max.y <= bass_rect.min.y + 1.0F || bass_rect.max.y <= drums_rect.min.y + 1.0F;
  CHECK(disjoint);

  // Both lanes stay within the canvas's own rect (no stray content drawn
  // outside the child window) -- with only 6 lanes total (always kRows.size()
  // now), 6*kLaneH is comfortably under this test window's avail height, so
  // no scrolling is needed here and both lanes render fully on-screen.
  CHECK(drums_rect.min.y >= canvas_rect.min.y - 1.0F);
  CHECK(bass_rect.max.y <= canvas_rect.max.y + 1.0F);

  ImGui::DestroyContext();
}

// UI-AUTOMATION functional test (owner decision, 2026-07-18: the previous
// per-lane "x" SmallButton HID THE WHOLE LANE, name included, and a "+
// track" popup re-shows whatever is currently hidden -- the owner rejected
// this outright, a lane's NAME must never disappear. Replaced with a real
// ImGui::Checkbox() placed directly UNDER each lane's own name label, in a
// left column (kLaneLabelW wide), that toggles ONLY whether that lane's
// note BARS are drawn; the name+checkbox pair is now ALWAYS rendered for
// all 6 kRows lanes, in fixed kRows order, regardless of role_visible --
// there is no popup and no lane-hiding affordance left at all). This test
// drives the REAL checkbox through REAL input injection (no direct model
// mutation): clicking a lane's checkbox must hide that lane's BARS ONLY --
// the lane's own name, its checkbox, and the total lane/checkbox count must
// never change -- and clicking the SAME checkbox again must restore the
// bars.
//
// Fixture note: this uses SeqEditModel's TRUE DEFAULT visibility (every
// role, including the 3 non-kRows roles, starts visible=true) rather than
// pre-hiding roles, since there is no popup and no "currently hidden"
// candidate list to interact with any more -- the checkbox is a direct,
// always-present per-lane toggle.
void test_lane_checkbox_toggles_bars_visibility_lane_stays() {
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
  // lead, in that order) render this frame, bars and all.

  // Every lane's own bars-visibility checkbox is a plain, unstyled
  // ImGui::Checkbox(). Dear ImGui's own Checkbox() (imgui_widgets.cpp) paints
  // its frame in ONE OF TWO colors depending on state: ImGuiCol_FrameBg when
  // UNCHECKED, ImGuiCol_CheckboxSelectedBg when CHECKED (a vendored-ImGui
  // detail found empirically while writing this test -- neither color alone
  // identifies "a checkbox", only "a checkbox in that particular state").
  // Since role_visible() defaults every role to true (checked), a fresh
  // SeqEditModel's checkboxes all paint CheckboxSelectedBg at first; a
  // checkbox flips to FrameBg only once its own bars are hidden. Every
  // in-canvas cluster helper below therefore counts BOTH colors combined --
  // that sum is the true "how many checkbox widgets exist" regardless of
  // which are currently checked.
  const ImU32 checkbox_checked_color = ImGui::GetColorU32(ImGuiCol_CheckboxSelectedBg);
  const ImU32 checkbox_unchecked_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
  const ImU32 bass_color = sonotron::neon::u32(sonotron::theme::kRoleTint[2], 0.45F);

  auto in_canvas_checkbox_count = [&](ImDrawData* draw_data,
                                      const th::Rect& canvas) -> std::size_t {
    std::size_t count = 0;
    for (const th::Rect& r : th::find_color_clusters(draw_data, checkbox_checked_color)) {
      if (r.center().y >= canvas.min.y) {
        ++count;
      }
    }
    for (const th::Rect& r : th::find_color_clusters(draw_data, checkbox_unchecked_color)) {
      if (r.center().y >= canvas.min.y) {
        ++count;
      }
    }
    return count;
  };

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(grid, seqedit,
                   fx);  // warm-up, mirrors the sibling click-driven tests' own idiom
  ImDrawData* locate = render_one_frame(grid, seqedit, fx);

  CHECK(seqedit.role_visible(2));
  CHECK(th::find_single_color_rect(locate, bass_color).found);

  const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
  CHECK(canvas_rect.found);

  // Locate every checkbox cluster. Every role starts checked (visible), so
  // at this point every cluster paints checkbox_checked_color -- assert the
  // in-canvas count is exactly kRows.size() (6): one checkbox per fixed
  // kRows lane, never more, never fewer. Sorted by Y since draw order and
  // screen order coincide for this top-to-bottom lane stack, matching
  // kRows' own iteration order (drums, bass, chord1, pad, arp, lead) --
  // bass is therefore the SECOND lane from the top, index 1.
  std::vector<th::Rect> in_canvas_checkboxes;
  for (const th::Rect& r : th::find_color_clusters(locate, checkbox_checked_color)) {
    if (r.center().y >= canvas_rect.min.y) {
      in_canvas_checkboxes.push_back(r);
    }
  }
  CHECK(in_canvas_checkboxes.size() == sonotron::kRows.size());
  CHECK(in_canvas_checkbox_count(locate, canvas_rect) == sonotron::kRows.size());
  if (in_canvas_checkboxes.size() < 2) {
    ImGui::DestroyContext();
    return;
  }
  std::sort(in_canvas_checkboxes.begin(), in_canvas_checkboxes.end(),
            [](const th::Rect& a, const th::Rect& b) { return a.min.y < b.min.y; });
  const th::Rect bass_checkbox = in_canvas_checkboxes[1];  // kRows[1] == bass, 2nd from top

  // Real click #1: hide bass' bars via its own checkbox.
  th::queue_mouse_down(bass_checkbox.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_up(bass_checkbox.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  ImDrawData* after_hide = render_one_frame(grid, seqedit, fx);

  CHECK(!seqedit.role_visible(2));
  CHECK(!th::find_single_color_rect(after_hide, bass_color).found);

  // THE CRUX OF THE WHOLE REWRITE: the checkbox cluster count (checked +
  // unchecked combined) is STILL exactly kRows.size() (6) after hiding
  // bass' bars -- the lane, its name, and its checkbox never disappeared,
  // only the bars did (bass' own checkbox simply flipped from
  // checkbox_checked_color to checkbox_unchecked_color). This is what the
  // old "x" SmallButton design could never prove, since hiding a lane there
  // hid the checkbox (and the name) along with the bars.
  CHECK(in_canvas_checkbox_count(after_hide, canvas_rect) == sonotron::kRows.size());

  // Real click #2: the checkbox never moved (the lane never disappeared),
  // so clicking the SAME screen position again re-shows bass' bars.
  th::queue_mouse_down(bass_checkbox.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_up(bass_checkbox.center());
  render_one_frame(grid, seqedit, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  ImDrawData* after_reshow = render_one_frame(grid, seqedit, fx);

  CHECK(seqedit.role_visible(2));
  CHECK(th::find_single_color_rect(after_reshow, bass_color).found);

  ImGui::DestroyContext();
}

// Fabrizio review (2026-07-18, track-set + content divergence from the
// Repeat Zone): the Repeat Zone's own launch grid only ever has 6 real rows
// (launch_rows.hpp's kRows -- drums, bass, chord1, pad, arp, lead); the
// other 3 TrackRole values (Perc, Chord2, Phrase) have no corresponding
// launch-grid row at all. draw_piano_roll_lanes therefore always renders
// exactly kRows.size() (6) lane name+checkbox pairs, in fixed kRows order,
// regardless of SeqEditModel::role_visible -- role_visible only gates
// whether a lane's own note BARS are drawn, it never gates whether the lane
// itself (name + checkbox) exists. This test proves BOTH halves of the new
// contract: (1) the lane/checkbox count is capped at kRows.size() even
// though the 3 non-kRows roles report role_visible() == true (the default),
// and stays capped there when those 3 are explicitly hidden (they were
// never lane candidates to begin with), and (2) hiding a REAL kRows role
// (Chord, role_index 3 -- style "basic"'s own kVarAPatterns table
// (components/core/arrangrr/include/arrangrr/arranger/styles/basic.hpp)
// only authors VarA content for Drums/Bass/Chord, never Arp, so Chord is
// used here rather than Arp for a non-vacuous bar-color assertion) leaves
// the checkbox count UNCHANGED at kRows.size() -- the lane NEVER disappears
// -- while Chord's own note bars stop being drawn. This is the semantic
// flip from the pre-checkbox design, which used to shrink the counted lane
// set by one here; now the count never shrinks, only a given lane's own
// bars toggle.
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
  seed_style_section_cell(grid, 0);  // drums
  seed_style_section_cell(grid, 3);  // chord -- needs real content so its own
                                     // bar-color assertion below is never
                                     // vacuous, before or after hiding it
                                     // (Arp has none for style "basic"/VarA,
                                     // see this test's own header comment)

  // Dear ImGui's own Checkbox() paints its frame in ImGuiCol_FrameBg when
  // UNCHECKED, ImGuiCol_CheckboxSelectedBg when CHECKED (see the sibling
  // test's own header comment for the empirical detail) -- roles default
  // checked, so both colors must be counted together to get the true
  // "how many checkbox widgets exist" regardless of individual state.
  const ImU32 checkbox_checked_color = ImGui::GetColorU32(ImGuiCol_CheckboxSelectedBg);
  const ImU32 checkbox_unchecked_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
  const ImU32 chord_color = sonotron::neon::u32(sonotron::theme::kRoleTint[3], 0.45F);

  auto count_in_canvas_checkboxes = [&](ImDrawData* draw_data) -> std::size_t {
    const th::Rect canvas_rect = th::find_child_window_rect("seq_canvas");
    CHECK(canvas_rect.found);
    std::size_t count = 0;
    for (const th::Rect& r : th::find_color_clusters(draw_data, checkbox_checked_color)) {
      if (r.center().y >= canvas_rect.min.y) {
        ++count;
      }
    }
    for (const th::Rect& r : th::find_color_clusters(draw_data, checkbox_unchecked_color)) {
      if (r.center().y >= canvas_rect.min.y) {
        ++count;
      }
    }
    return count;
  };

  // Frame 1: every role at SeqEditModel's true default (all 9 visible,
  // including Perc/Chord2/Phrase). The lane set (one checkbox per lane)
  // must be exactly kRows.size() (6), never kTrackRoleCount (9).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(grid, seqedit, fx);
  ImDrawData* all_default = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_checkboxes(all_default) == sonotron::kRows.size());

  // Frame 2: hide all 3 non-kRows roles (Perc=1, Chord2=4, Phrase=7). None of
  // them was ever a lane candidate, so the checkbox count must stay
  // UNCHANGED at kRows.size() -- a real regression to "walk all 9 roles"
  // would instead drop 3 lanes here. Chord (a real kRows role, still
  // visible) must still paint its own bars this frame.
  seqedit.set_role_visible(1, false);
  seqedit.set_role_visible(4, false);
  seqedit.set_role_visible(7, false);
  ImDrawData* non_launch_hidden = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_checkboxes(non_launch_hidden) == sonotron::kRows.size());
  CHECK(th::find_single_color_rect(non_launch_hidden, chord_color).found);

  // Frame 3: now hide one REAL kRows role (Chord, role_index 3). The
  // checkbox count must STAY at kRows.size() (6) -- the lane itself never
  // disappears -- while Chord's own bars are no longer drawn. THE SEMANTIC
  // FLIP: the pre-checkbox design used to drop the counted lane set to
  // kRows.size() - 1 here.
  seqedit.set_role_visible(3, false);
  ImDrawData* chord_hidden = render_one_frame(grid, seqedit, fx);
  CHECK(count_in_canvas_checkboxes(chord_hidden) == sonotron::kRows.size());
  CHECK(!th::find_single_color_rect(chord_hidden, chord_color).found);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_piano_roll_draws_visible_roles_in_disjoint_lanes();
  test_lane_checkbox_toggles_bars_visibility_lane_stays();
  test_lane_set_is_capped_to_launch_rows_never_all_nine_roles();
  return sonotron::test::failures();
}

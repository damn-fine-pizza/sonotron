// UI-AUTOMATION acceptance test (docs/proposals/browser-redesign-taxonomy.md
// Phase 1, deliverable 3): "Voices/Sounds tab... Clicking a voice sends
// `program <port>[:ch] <voice>`... [to] a destination picker (port
// [:channel])."
//
// There is no per-part program readback on the wire (parts_model.hpp;
// browser_model.hpp's own m_last_voice_sent comment) -- a real click cannot
// be proven via a wire-CONFIRMED voice state. Instead this test exploits a
// REAL, engine-observable signal that already exists for exactly this
// situation: InProcessBrainSession::send() synchronously translates every L1
// line through the SAME production command_line_to_command() translator the
// real app uses (in_process_brain_session.cpp), and an unresolvable
// destination surfaces as a genuine BrainEvent::kError on the next poll(),
// carrying the EXACT command line that was sent
// (`note.cmd = std::string(command_line)`). Deliberately setting an invalid
// destination PORT (never touched by the click itself -- only
// BrowserModel::build_program_verb reads it at click time) turns "did the
// click send the CURRENT destination plus the CORRECT voice name, all the
// way through the real translator" into an assertable, non-spied fact: the
// decoded error's own `cmd` field.
//
// Destination is set through BrowserModel's own public API before the click
// (the SAME precedent test_browser_style_switch_while_playing_ui_automation.
// cpp's own model.set_search_filter("rock") call already established for
// narrowing) -- the click itself, on the REAL voice leaf row, is the one
// thing actually simulated via real mouse input.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/browser_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
#include "src/theme.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <cstddef>
#include <string>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrowserCategory;
using sonotron::BrowserModel;
using sonotron::InProcessBrainSession;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

ImDrawData* render_one_frame(BrowserModel& model, BrainSession& brain_session, AppState& app_state,
                             UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(1280.0F, 800.0F), ImGuiCond_Always);
  ImGui::Begin("test");
  ImGui::BeginChild("browser_area", ImVec2(360.0F, 600.0F));
  sonotron::render_browser_panel(model, brain_session, app_state, fx);
  ImGui::EndChild();
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, BrowserModel& model, BrainSession& brain_session,
                     AppState& app_state, UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, brain_session, app_state, fx);
}

void test_real_voice_leaf_click_sends_program_with_current_destination() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  BrowserModel model;
  UiState fx;
  AppState app_state;

  // "Kalimba" is the only one of the 128 kGmVoiceNames containing that
  // substring (browser_model.hpp) -- narrows the Voices list to exactly one
  // row, the same "narrow with the model's own filter so exactly one leaf
  // renders" technique test_browser_style_switch_while_playing_ui_automation.
  // cpp already established for style leaves.
  std::size_t target_index = model.voice_count();
  for (std::size_t i = 0; i < model.voice_count(); ++i) {
    if (model.voice_name(i) == "Kalimba") {
      target_index = i;
      break;
    }
  }
  CHECK(target_index < model.voice_count());

  model.set_search_filter("zzz-no-style-match");  // category defaults kStyles
  model.set_category_visible(BrowserCategory::kVoices, true);
  model.set_category(BrowserCategory::kVoices);
  model.set_search_filter("Kalimba");

  // A deliberately unresolvable port ("zz" is neither "out0" nor a bare port
  // index, in_process_brain_session.cpp's own `program` translation) -- the
  // one thing that turns "the click sent the CURRENT destination" into an
  // observable fact (see this file's header comment).
  model.set_voice_port("zz");
  model.set_voice_channel(1);

  InProcessBrainSession session;
  CHECK(session.start());

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, session, app_state, fx);
  ImDrawData* frame = render_one_frame(model, session, app_state, fx);

  const th::Rect browser_tree = th::find_child_window_rect("browser_tree");
  CHECK(browser_tree.found);
  const ImU32 leaf_text_color = sonotron::neon::u32(sonotron::theme::kTextSecondary);
  const std::vector<th::Rect> leaf_clusters = th::find_color_clusters(frame, leaf_text_color);
  th::Rect target_row;
  for (const th::Rect& r : leaf_clusters) {
    if (r.min.y >= browser_tree.min.y && r.max.y <= browser_tree.max.y) {
      target_row = r;
      break;
    }
  }
  CHECK(target_row.found);

  // Real click: the "Kalimba" voice leaf. browser_panel.cpp's render_voices
  // sends `model.build_program_verb(name)` and marks the row as the local
  // "last sent" echo -- never hand-written here.
  click_at(target_row.center(), model, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // The click's own immediate, GUI-side echo: confirms we actually clicked
  // the intended row (real production field), NEVER presented as
  // wire-confirmed (browser_model.hpp's own m_last_voice_sent comment).
  CHECK(model.last_voice_sent() == static_cast<int>(target_index));

  const std::string expected_cmd = model.build_program_verb("Kalimba");
  CHECK(expected_cmd == "program zz:1 Kalimba");

  std::vector<BrainEvent> events;
  session.poll(events);
  bool found_error = false;
  for (const BrainEvent& ev : events) {
    if (ev.kind == BrainEvent::Kind::kError && ev.cmd == expected_cmd) {
      found_error = true;
      CHECK(ev.error.find("unknown output port") != std::string::npos);
    }
  }
  // THE ASSERTION: the real production translator (in_process_brain_session.
  // cpp's command_line_to_command, the SAME path the real app runs) actually
  // decoded a command line equal to what the click was supposed to send --
  // the CURRENT destination ("zz:1") plus the CLICKED voice name
  // ("Kalimba"), never a hand-typed string.
  CHECK(found_error);

  session.stop();
  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_voice_leaf_click_sends_program_with_current_destination();
  return sonotron::test::failures();
}

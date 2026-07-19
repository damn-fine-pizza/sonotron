// FUNCTIONAL ACCEPTANCE TESTS (Nazzareno, Song-mode Phase 2): hardens the
// per-scene STYLE/GROOVE/KEY/TEMPO override contract added on top of
// song-mode-scenechain-adoption.md's own Phasing section ("Phase 2 -- per-
// scene editors ... The Phase-1 capture-and-override picks the fields up
// automatically"). Host-only, ZERO ABI/core change: the four new override
// groups ride the SAME `song build` wire line the repeat-count contract
// (test_song_mode_repeat_count_contract.cpp) already hardens, just with four
// new trailing per-scene tokens (`<style> <groove> <key> <tempo>`, each `-`
// for "no override").
//
// Two claims are proven here:
//   1. ENCODE: build_and_play_song (grid_panel.cpp) really serializes each
//      populated column's own GridModel Phase-2 override state into the
//      wire line's trailing four tokens -- a scene with no overrides sends
//      `- - - -`, a scene with every override set sends the real values.
//   2. DECODE: in_process_brain_session.cpp's parse_song_build_scene_
//      overrides genuinely validates each of the four tokens -- a malformed
//      style/groove/key token is rejected with a clear per-field error
//      (kind == kError, not silently accepted or silently dropped), and a
//      fully well-formed line with real override values parses with NO
//      error at all (proving the happy path is not accidentally rejected by
//      the same validation that catches the malformed cases).
//
// This file does not attempt to prove the override actually retunes the
// live engine end-to-end: gui-contract-map.md's own JSONL event set (kMidi
// Out/kChord/kSection/kTransport/kWarn/kChordFollowed/kBeat/kClip/kLoop/
// kError) has no generic param_state/tempo/groove/key readback the GUI wire
// layer decodes today (transport_panel.cpp's own bpm/key fields are
// documented as "local-only display, BrainSnapshot fields are unwired") --
// so there is no observable proxy available to this test suite for that
// deeper claim, honestly out of scope here.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/grid_model.hpp"
#include "src/grid_panel.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/parts_model.hpp"
#include "src/seqedit_model.hpp"
#include "src/ui_state.hpp"

#include "test.hpp"

#include <string>
#include <string_view>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::UiState;

namespace {

// Same thin recording DECORATOR as the sibling contract tests: every send()
// is forwarded verbatim to a real InProcessBrainSession, and also appended
// to `sent`, giving the test a ground-truth transcript of every wire line
// the production code really sent.
class RecordingBrainSession : public BrainSession {
 public:
  explicit RecordingBrainSession(InProcessBrainSession& real) : m_real(real) {}

  void send(std::string_view command_line) override {
    sent.emplace_back(command_line);
    m_real.send(command_line);
  }
  void poll(std::vector<BrainEvent>& out_events) override { m_real.poll(out_events); }
  const sonotron::BrainSnapshot& snapshot() const override { return m_real.snapshot(); }
  Status status() const override { return m_real.status(); }

  std::vector<std::string> sent;

 private:
  InProcessBrainSession& m_real;
};

void render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                      BrainSession& brain_session, const AppState& app_state, UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::Begin("test");
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::EndFrame();
}

bool any_event_is_error_containing(const std::vector<BrainEvent>& events, std::string_view needle) {
  for (const BrainEvent& ev : events) {
    if (ev.kind == BrainEvent::Kind::kError && ev.error.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool any_event_is_error(const std::vector<BrainEvent>& events) {
  for (const BrainEvent& ev : events) {
    if (ev.kind == BrainEvent::Kind::kError) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------
// Claim 1 (ENCODE): a scene with no Phase-2 overrides sends "- - - -"; a
// scene with every override group set sends the real, correctly-shaped
// tokens (style decimal, groove 6 comma-joined decimals, key root:mode,
// tempo decimal).
// -----------------------------------------------------------------------
void test_song_build_wire_line_carries_phase2_override_tokens() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(2);
  model.set_scene_bars(0, 1);
  model.set_scene_repeat(0, 1);
  // Scene 0: every Phase-2 override group set.
  model.set_scene_style_id(0, 3);
  model.set_scene_groove(0, true,
                         GridModel::SceneGroove{.swing = 40,
                                                .humanize_timing = 10,
                                                .humanize_velocity = 20,
                                                .accent = 30,
                                                .swing_grid = 16,
                                                .quantize = 50});
  model.set_scene_key(0, true, 7, 2);
  model.set_scene_tempo_x100(0, 14000);
  // Scene 1: left at every Phase-2 default (no overrides).
  model.set_scene_bars(1, 1);
  model.set_scene_repeat(1, 1);

  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession real_session;
  RecordingBrainSession session(real_session);
  CHECK(real_session.start());
  session.send("style load basic");

  session.send("transport start");
  app_state.note_transport_sent(true);
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  real_session.stop();

  std::string song_build_line;
  for (const std::string& line : session.sent) {
    if (line.rfind("song build ", 0) == 0) {
      song_build_line = line;
      break;
    }
  }
  CHECK(song_build_line ==
        "song build 2 intro1 1 1 3 40,10,20,30,16,50 7:2 14000 varA 1 1 - - - -");

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// Claim 2 (DECODE): a malformed override token on any of the four groups is
// rejected with a clear, field-specific error -- it is neither silently
// accepted (corrupting the built chain) nor crashes the parser.
// -----------------------------------------------------------------------
void test_malformed_style_override_token_is_rejected_with_clear_error() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("song build 1 varA 1 1 bogus - - -");
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(any_event_is_error_containing(events, "style override"));
  session.stop();
}

void test_malformed_groove_override_token_wrong_field_count_is_rejected() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("song build 1 varA 1 1 - 1,2,3 - -");  // only 3 of 6 required fields
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(any_event_is_error_containing(events, "groove override"));
  session.stop();
}

void test_malformed_groove_override_token_out_of_range_field_is_rejected() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("song build 1 varA 1 1 - 1,2,3,4,5,999 - -");  // 999 > 255
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(any_event_is_error_containing(events, "groove override"));
  session.stop();
}

void test_malformed_key_override_token_bad_root_is_rejected() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("song build 1 varA 1 1 - - 99:2 -");  // root 99 is out of [0,11]
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(any_event_is_error_containing(events, "key override"));
  session.stop();
}

void test_malformed_tempo_override_token_is_rejected() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("song build 1 varA 1 1 - - - notanumber");
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(any_event_is_error_containing(events, "tempo override"));
  session.stop();
}

// -----------------------------------------------------------------------
// Claim 2b: the happy path is not accidentally rejected by the same
// validation that catches the malformed cases above -- a fully well-formed
// line with real override values on every one of the four groups produces
// NO kError event.
// -----------------------------------------------------------------------
void test_well_formed_phase2_overrides_parse_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  session.send("song build 1 varA 1 1 0 10,20,30,40,8,50 3:1 12000");
  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(!any_event_is_error(events));
  session.stop();
}

}  // namespace

int main() {
  test_song_build_wire_line_carries_phase2_override_tokens();
  test_malformed_style_override_token_is_rejected_with_clear_error();
  test_malformed_groove_override_token_wrong_field_count_is_rejected();
  test_malformed_groove_override_token_out_of_range_field_is_rejected();
  test_malformed_key_override_token_bad_root_is_rejected();
  test_malformed_tempo_override_token_is_rejected();
  test_well_formed_phase2_overrides_parse_without_error();
  return sonotron::test::failures();
}

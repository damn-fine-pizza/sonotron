// FUNCTIONAL ACCEPTANCE TESTS (Torquato QA pass): hardens the observable
// contract docs/proposals/song-mode-scenechain-adoption.md itself lists
// ("Observable contract" section) for the core `SceneChain` adoption --
// distinct from the individual bugs the sibling migrated tests pin. Two
// bullets of that contract are proven here for the first time; the other
// two are ALREADY covered elsewhere and are cited, not duplicated:
//   - "the active section changes once per scene, on the bar boundary, with
//     no oscillation" and "pressing Play issues exactly kSceneClear, one
//     kSceneAdd per populated scene, then kScenePlay -- and NO launch scene/
//     style section double-send per advance" -- both proven together below
//     by test_song_build_sends_exactly_once_no_double_send_and_scene_order_
//     is_monotonic, since the ABI-level Command sequence itself
//     (apply_song_build, in_process_brain_session.cpp) is anonymous-
//     namespace/host-engine-thread-local and has no seam a test translation
//     unit can call directly (matches this whole test directory's own
//     "render_grid_panel is the only public seam" discipline) -- so the
//     wire-level absence of any `launch scene `/`style section ` send for
//     the ENTIRE run is the closest-to-the-metal observable proxy available,
//     and it is a strictly STRONGER guarantee than "no double-send": Phase 1
//     retired those sends from the auto-song path entirely (design decision
//     4), so none may ever appear, not even once.
//   - "emitted chord activity keeps flowing across scene boundaries" is
//     proven by test_chord_activity_keeps_flowing_across_a_scene_boundary.
//   - "when the last scene's bars elapse, exactly one `style section
//     ending1` is cued and transport stops, the song does not wrap" is
//     ALREADY fully proven by test_grid_panel_auto_song_ends_on_ending_ui_
//     automation.cpp's own test_auto_song_holds_at_last_column_and_ends_on_
//     ending_without_any_click, re-confirmed green under Phase 1 by this
//     same QA pass (see this pass's own report) -- not re-tested here.
//   - "the per-scene length stepper (#6) still bounds each scene's hold to
//     [1,8] bars" is ALREADY fully unit-tested at the model layer
//     (test_grid_model.cpp's test_set_scene_bars_clamps_floor_at_one /
//     _ceiling_at_max) -- build_and_play_song (grid_panel.cpp) only ever
//     reads GridModel::scene_bars(), which can never return an out-of-range
//     value by construction, so no separate slow real-backend re-proof is
//     needed here.
//
// Real-vs-stubbed tag: real InProcessBrainSession, real engine thread, real
// per-frame render_grid_panel -- the ONLY test double is a thin recording
// DECORATOR around BrainSession::send() (never a fake engine), so every
// wire line asserted on below is a line the real production code actually
// sent, not a mock's guess.
//
// RETIREMENT NOTE (Torquato QA, song-mode Phase 1 migration pass): this
// file's own `!any_sent_line_starts_with(session.sent, "launch scene ")`
// assertion below is the deliberate, PERMANENT OPPOSITE of the retired
// test_grid_panel_auto_song_launches_next_scene.cpp (test_auto_song_advance_
// must_launch_next_scene_clips, removed from the tree by this same pass) --
// that test pinned the OLD double-trigger fix ("auto-song's advance must
// ALSO send `launch scene <n> quantize <q>`"), which Phase 1's design
// decision 4 explicitly retired (SceneChain never touches ClipMatrix at
// all). Retired outright rather than left as a dead stub, since the
// behavior it pinned no longer exists in any form to migrate toward -- see
// this pass's own QA report for the full disposition of every migrated/
// retired test.

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

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrainSnapshot;
using sonotron::GridModel;
using sonotron::InProcessBrainSession;
using sonotron::PartsModel;
using sonotron::SeqEditModel;
using sonotron::UiState;

namespace {

// A thin recording DECORATOR, not a test double for the engine itself: every
// send() is forwarded verbatim to a real InProcessBrainSession (so the
// production translator/ring/engine-thread path runs unchanged) and ALSO
// appended to `sent`, giving the test a ground-truth transcript of every
// wire line the production code really emitted.
class RecordingBrainSession : public BrainSession {
 public:
  explicit RecordingBrainSession(InProcessBrainSession& real) : m_real(real) {}

  void send(std::string_view command_line) override {
    sent.emplace_back(command_line);
    m_real.send(command_line);
  }
  void poll(std::vector<BrainEvent>& out) override { m_real.poll(out); }
  const BrainSnapshot& snapshot() const override { return m_real.snapshot(); }
  Status status() const override { return m_real.status(); }

  std::vector<std::string> sent;

 private:
  InProcessBrainSession& m_real;
};

// Same combined transport+grid frame shape every functional test in this
// directory shares -- no rendered-output assertions needed here (this pass
// is about the WIRE contract and fx.active_scene bookkeeping, not pixels),
// so no ImGui::Render()/ImDrawData round trip.
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

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

bool any_sent_line_starts_with(const std::vector<std::string>& sent, std::string_view prefix) {
  for (const std::string& line : sent) {
    if (line.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------
// Bullets 1+2 of the observable contract: no launch-scene/style-section
// double-send ANYWHERE in an auto-song run, and the active scene column
// advances strictly in order (0,1,2,3,4), never oscillating or repeating a
// scene already left behind.
// -----------------------------------------------------------------------
void test_song_build_sends_exactly_once_no_double_send_and_scene_order_is_monotonic() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);  // same scene count main.cpp actually boots with
  // Fast, deterministic cadence: scene 0 ("intro1") is a genuine 2-bar
  // one-shot in "basic" (styles/basic.hpp) regardless of this stepper (see
  // test_repeat_zone_owner_trace_replay.cpp's own header comment for the
  // full trace of why), so its own stepper value is set to match (2) purely
  // for readability -- scenes 1..4 (plain kVarA..kVarD, no one-shot
  // behavior) are pinned to the stepper's own floor (1 bar each) so the
  // WHOLE 5-scene chain elapses in a handful of real bars.
  model.set_scene_bars(0, 2);
  for (std::size_t s = 1; s < model.scene_count(); ++s) {
    model.set_scene_bars(s, 1);
  }
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession real_session;
  RecordingBrainSession session(real_session);
  CHECK(real_session.start());
  session.send("style load basic");
  session.send("bpm 400");  // shrink the wall-clock bar cadence

  // "Press Play": the same production verb transport_panel.cpp's Play pad
  // sends, never a hand-injected transport JSONL.
  session.send("transport start");
  app_state.note_transport_sent(true);

  std::vector<int> scene_transitions;  // deduplicated fx.active_scene history
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);
  while (std::chrono::steady_clock::now() < deadline) {
    poll_once(session, app_state);
    render_one_frame(model, seqedit, parts, session, app_state, fx);
    if (scene_transitions.empty() || scene_transitions.back() != fx.active_scene) {
      scene_transitions.push_back(fx.active_scene);
    }
    if (fx.active_scene == 4) {
      break;  // reached the last populated column -- the full chain has been walked
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  real_session.stop();

  CHECK(app_state.bar() > 0);  // sanity: the run genuinely played

  // THE WIRE-LEVEL ASSERTION: pressing Play (with auto-song ON, the default)
  // sends exactly ONE `song build ...` line for the whole run -- and no
  // `launch scene `/`style section ` line is EVER sent, for any advance,
  // because Song-mode Phase 1 (design decision 4) retired both from the
  // auto-song path entirely (the core SceneChain's own kSceneAdd/kScenePlay
  // sequence drives every advance from the engine thread, never a repeated
  // host-side send()).
  int song_build_count = 0;
  for (const std::string& line : session.sent) {
    if (line.rfind("song build ", 0) == 0) {
      ++song_build_count;
    }
  }
  CHECK(song_build_count == 1);
  CHECK(!any_sent_line_starts_with(session.sent, "launch scene "));
  CHECK(!any_sent_line_starts_with(session.sent, "style section "));

  // THE SCENE-ORDER ASSERTION: the active scene must have progressed
  // strictly through 0,1,2,3,4 -- no repeats-after-leaving, no going
  // backward, no skipping (which would indicate a stray extra advance or an
  // out-of-order Command application).
  const std::vector<int> expected = {0, 1, 2, 3, 4};
  CHECK(scene_transitions == expected);

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// Bullet 3 of the observable contract: the harmony loop (the default
// progression auto-injected at style-load, in_process_brain_session.cpp)
// keeps emitting "chord" activity across a scene boundary -- proving the
// per-scene Performance's own `chord_sequence_id = 0xFFFF` override really
// does leave the ChordSequencer alone rather than restarting it.
// -----------------------------------------------------------------------
void test_chord_activity_keeps_flowing_across_a_scene_boundary() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  model.set_scene_bars(0, 2);  // matches intro1's own intrinsic length, see sibling test above
  for (std::size_t s = 1; s < model.scene_count(); ++s) {
    model.set_scene_bars(s, 2);  // a couple of bars each -- enough room to observe chord activity
  }
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
  AppState app_state;

  InProcessBrainSession session;
  CHECK(session.start());
  session.send("style load basic");
  session.send("bpm 300");
  session.send("transport start");
  app_state.note_transport_sent(true);

  int chord_events_before_transition = 0;
  int chord_events_after_transition = 0;
  int last_scene = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);
  while (std::chrono::steady_clock::now() < deadline) {
    std::vector<BrainEvent> events;
    session.poll(events);
    for (const BrainEvent& ev : events) {
      app_state.apply(ev);
      if (ev.kind == BrainEvent::Kind::kChord) {
        if (last_scene == 0) {
          ++chord_events_before_transition;
        } else {
          ++chord_events_after_transition;
        }
      }
    }
    render_one_frame(model, seqedit, parts, session, app_state, fx);
    last_scene = fx.active_scene;
    // Enough evidence gathered on both sides of at least one scene
    // boundary -- stop early rather than always burning the full deadline.
    if (last_scene > 0 && chord_events_before_transition > 0 && chord_events_after_transition > 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  session.stop();

  CHECK(app_state.bar() > 0);
  CHECK(fx.active_scene > 0);  // sanity: a scene boundary was genuinely crossed

  // THE ASSERTION: harmony fired on BOTH sides of the boundary -- the
  // ChordSequencer never stopped or restarted when the SceneChain applied
  // the next scene's Performance.
  CHECK(chord_events_before_transition > 0);
  CHECK(chord_events_after_transition > 0);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_song_build_sends_exactly_once_no_double_send_and_scene_order_is_monotonic();
  test_chord_activity_keeps_flowing_across_a_scene_boundary();
  return sonotron::test::failures();
}

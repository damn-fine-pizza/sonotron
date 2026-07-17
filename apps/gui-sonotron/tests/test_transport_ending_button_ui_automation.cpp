// UI-AUTOMATION ACCEPTANCE TEST (Torquato QA pass, roadmap task #37, design
// doc docs/proposals/song-form-option-a-wiring-plan.md §3/§4): a dedicated
// ENDING transport button, distinct from the existing Stop pad. Per the
// design doc's own §3.3 finding, the whole gesture costs ZERO new core/ABI
// surface -- it is a host-only reuse of two ALREADY-TESTED core mechanisms:
//   1. `style section ending1`, sent while playing, is bar-quantized for free
//      (Engine::cmd_style's kStyleSection case, engine.cpp:635-641:
//      `!m_transport.playing()` is false while playing, so the request is
//      QUEUED, landing at the next bar boundary, engine.hpp's fire_arranger).
//   2. Once the Ending section's own authored bars run out, the Arranger's
//      one-shot rule stops the transport BY ITSELF (arranger.hpp's
//      section_is_ending branch, already proven host-agnostic by
//      test_ending_stops_transport, components/core/arrangrr/tests/
//      test_arranger.cpp:617-633).
// So the GUI-side contract under test here is narrow and entirely
// behavioral: does clicking the button really cue `ending1` (never anything
// else, never paired with a `launch scene` that could force it immediate),
// does the REAL engine's own section readback (AppState::section()) actually
// reach "ending1", does the transport then genuinely stop BY ITSELF (never a
// second click), and -- the one genuine race this feature introduces --
// does auto-song's own advance stay OUT of the way while the cued Ending is
// still playing out (rather than firing `style section <next>` at the next
// section boundary and silently overriding the cue before the one-shot rule
// ever gets to fire, the exact failure mode task #37's own §4 item 4 flags).
//
// STATUS AT AUTHORING TIME: RED-first, by design. `transport_panel.cpp` (the
// file Nazzareno is implementing the button in, in parallel, in the same
// shared tree) had NO ending pad at all when this test was written -- only
// the pre-existing Play/Stop/Panic three (see this file's own header comment
// in transport_panel.hpp). What HAS already landed in the tree (uncommitted,
// alongside this test) is the OTHER half of the contract this test also
// pins: `V02State::ending_cued` (v02_state.hpp) and grid_panel.cpp's
// update_auto_song suppression guard (`if (fx.ending_cued) { return; }`),
// both carrying a header comment that already documents EXACTLY what the
// button is expected to do: "set the instant the button sends `style section
// ending1` while playing... cleared the instant the transport is observed
// NOT playing". This test exercises that already-landed guard for real (not
// just trusts the comment) the moment the button itself exists to drive it.
//
// LOCATE STRATEGY, AND THE ONE ASSUMPTION IT ENCODES (flagged, not hidden):
// this test cannot know Nazzareno's exact accent color/glyph choice for the
// new pad ahead of time, so rather than guessing a specific ImU32 (which
// would silently rot the moment his real choice differs), it locates the
// button POSITIONALLY: the gap between the Panic pad's own rect and the
// "tempo_inset" child window that currently sits immediately to its right
// (both already real, locatable anchors -- Panic by its unique border color,
// the tempo inset by ImGui's own child-window list, imgui_headless_harness.
// hpp's find_child_window_rect). Whatever widget lands in that gap, in
// whatever color, is taken to be the new Ending pad -- this only requires
// Nazzareno to group it with the other three pad buttons (matching this
// exact file's own "Play/Stop/Panic neon pad buttons" comment block), not to
// pick any specific visual. If he places it somewhere else entirely, this
// locate step itself will fail (found=false) and needs reconciling once his
// code lands -- flagged in this pass's own report, not guessed silently.
//
// Real-vs-stubbed tag (flow-verification-matrix-2026-07.md §1 convention):
// `_ui_automation` == real click injection + real InProcessBrainSession +
// real rendered-output/engine readback, never a hand-written send() or a
// V02State field standing in for a click -- EXCEPT auto_song's own arming,
// which needs no click at all here: V02State::auto_song now defaults to true
// (owner decision 2026-07-17, v02_state.hpp), so "auto_song ON" is simply
// the fixture's starting state, not a gap.

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

// Same combined transport+grid frame shape every UI-automation test in this
// directory shares.
ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, V02State& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
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

void poll_once(BrainSession& session, AppState& app_state) {
  std::vector<BrainEvent> events;
  session.poll(events);
  for (const BrainEvent& ev : events) {
    app_state.apply(ev);
  }
}

// Shared fixture: real style load ("basic" -- every built-in style authors a
// real, non-zero kEnding1, and "basic"'s own is exactly 1 bar, design doc
// §1 item 4 / §2.6 -- so once cued, the ending resolves fast), fast bpm, a
// real Play click, and the real Play/Stop/Panic pad rects plus the "tempo_
// inset" anchor this test's own locate strategy needs for the Ending pad.
struct Fixture {
  th::Rect play_rect;
  th::Rect stop_border_rect;
  th::Rect panic_rect;
  th::Rect tempo_rect;
};

Fixture setup_and_play(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       InProcessBrainSession& session, AppState& app_state, V02State& fx) {
  CHECK(session.start());
  session.send("style load basic");
  for (std::size_t i = 0; i < sonotron::kBuiltinStyleNames.size(); ++i) {
    if (sonotron::kBuiltinStyleNames[i] == "basic") {
      fx.active_style = static_cast<int>(i);
      break;
    }
  }
  session.send("bpm 400");  // shrink the wall-clock bar cadence

  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  Fixture out;
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  out.play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(out.play_rect.found);
  const ImU32 stop_border_color = sonotron::neon::u32(sonotron::theme::kTextSecondary, 0.6F);
  out.stop_border_rect = th::find_single_color_rect(locate, stop_border_color);
  CHECK(out.stop_border_rect.found);
  const ImU32 panic_border_color = sonotron::neon::u32(sonotron::theme::kPink, 0.6F);
  out.panic_rect = th::find_single_color_rect(locate, panic_border_color);
  CHECK(out.panic_rect.found);
  out.tempo_rect = th::find_child_window_rect("tempo_inset");
  CHECK(out.tempo_rect.found);

  // Real click: Play.
  click_at(out.play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);
  return out;
}

// -----------------------------------------------------------------------
// TEST 1 (RED-first, per this file's own header comment): while playing,
// with auto-song ON (the default) genuinely advancing the song, clicking the
// ENDING pad must (a) cue `style section ending1` alone (real engine
// readback, never a mirror of a local flag), (b) let it play out and stop
// the transport BY ITSELF, and (c) never have auto-song's own advance
// override the cue first.
// -----------------------------------------------------------------------
void test_ending_button_cues_ending_then_engine_stops_transport_without_autosong_override() {
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
  AppState app_state;
  InProcessBrainSession session;

  const Fixture setup = setup_and_play(model, seqedit, parts, session, app_state, fx);
  CHECK(fx.auto_song);  // GIVEN: auto-song ON (the shipped default, not a click this test owes)

  // GIVEN: let the song genuinely advance at least once before cueing the
  // ending -- proves auto-song was actually live and moving, not merely
  // armed-but-idle, before the manual cue below.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (fx.active_scene != 0) {
        break;  // the real auto-song advance landed -- the song is mid-way
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK(fx.active_scene != 0);
  const int scene_at_ending_click = fx.active_scene;

  // Locate the Ending pad (see this file's own header comment for the exact,
  // explicitly-flagged assumption this positional strategy encodes): the
  // widget that lands in the gap between Panic's rect and the tempo inset's
  // own child-window rect, background excluded dynamically via the current
  // ImGui style rather than a guessed literal.
  th::Rect probe;
  probe.min = ImVec2(setup.panic_rect.max.x + 2.0F, setup.panic_rect.min.y - 2.0F);
  probe.max = ImVec2(setup.tempo_rect.min.x - 2.0F, setup.panic_rect.max.y + 2.0F);
  probe.found = true;
  ImDrawData* pre_click_frame = render_one_frame(model, seqedit, parts, session, app_state, fx);
  const ImU32 window_bg = ImGui::GetColorU32(ImGuiCol_WindowBg);
  const th::Rect ending_rect =
      th::find_rect_in_region_excluding_colors(pre_click_frame, probe, {window_bg});
  // Was RED at authoring time (no button existed in the Panic<->tempo-inset
  // gap yet); GREEN now that Nazzareno's render_ending_pad (transport_panel.
  // cpp) has landed at exactly this position.
  CHECK(ending_rect.found);
  const ImVec2 ending_click_pos = ending_rect.found ? ending_rect.center() : probe.center();

  // Real click: Ending.
  click_at(ending_click_pos, model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Wait for the REAL engine section readback to reach "ending1" (bar-
  // quantized cueing, design doc §3.3 item 1) -- and, over the SAME window,
  // watch for the one regression this feature specifically guards against:
  // auto-song firing its own advance and moving fx.active_scene off the
  // scene it was on when Ending was cued (which would mean update_auto_song's
  // `ending_cued` suppression latch failed to hold).
  bool autosong_overrode_the_cue = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (fx.active_scene != scene_at_ending_click) {
        autosong_overrode_the_cue = true;
      }
      if (app_state.section() == "ending1") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  // THE RED PIN (Torquato QA finding, roadmap task #37): this is where the
  // test currently fails, and it pins a genuine engine-side race, NOT a bug
  // in the button handler itself (fx.ending_cued does flip true, and `style
  // section ending1` IS sent -- confirmed by direct instrumentation during
  // this investigation). The active scene column has real clips registered
  // in the core ClipMatrix (grid_panel.cpp's demo-clip registration, commit
  // dff4e9e). The auto-song advance that lands the click's OWN starting
  // scene (activate_scene_column, grid_panel.cpp) always sends a paired
  // `launch scene <n> quantize 1` alongside `style section <n>` -- and per
  // that function's own header comment, `launch scene`'s fan-out
  // (Engine::clip_scene_launch -> Engine::clip_request -> Engine::
  // apply_clip_content) arms every one of that column's registered clips
  // with ClipMatrix::arm(..., n_bars=1). Those arms are promoted at the very
  // next bar boundary (ClipMatrix::on_bar), and EACH promoted clip's
  // apply_clip_content calls `Arranger::request(section, /*immediate=*/
  // true)` unconditionally -- which, per arranger.hpp's own contract,
  // BOTH sets the live section AND silently clears whatever `style section`
  // itself had queued (Arranger::m_pending_valid), with no awareness of
  // `fx.ending_cued` at all (that GUI-side latch only stops NEW `launch
  // scene` sends -- it cannot un-arm a clip the CORE already armed a moment
  // earlier, before the Ending click, on the engine's own bar clock). If the
  // Ending pad is clicked while that column's PRIOR scene-launch arm is
  // still in flight (has not yet reached its own 1-bar-quantized promotion),
  // the promotion lands AFTER the click, reasserts the OLD section (here,
  // "varA") via an IMMEDIATE Arranger::request, and clobbers the QUEUED
  // "ending1" pending switch before the arranger ever applies it -- the
  // Ending is cued, then silently un-cued by an unrelated, already-in-flight
  // clip promotion. This is exactly the interaction the design doc's own
  // §3.3 comment anticipated in the abstract ("at worst the queued one is
  // silently superseded... onto the SAME value") but did not cover for THIS
  // case, where the superseding value is NOT the same as what was queued.
  // THE ASSERTION (the cue landed): the real engine's own section readback,
  // never a mirror of a local "ending_cued" flag -- a flag a buggy button
  // handler could set without ever actually sending the verb.
  CHECK(app_state.section() == "ending1");
  // THE ASSERTION (no auto-song override raced ahead of the cue): if
  // update_auto_song's suppression guard ever regresses, this flips true and
  // the section above would most likely never have settled on "ending1" at
  // all (it would show whatever auto-song's own advance sent instead).
  CHECK(!autosong_overrode_the_cue);

  // Wait for the engine's own one-shot rule to stop the transport BY ITSELF
  // (arranger.hpp's section_is_ending branch / engine.hpp's fire_arranger,
  // already proven core-side by test_ending_stops_transport) -- never a
  // second click, never a host-side send() of "transport stop".
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (fx.active_scene != scene_at_ending_click) {
        autosong_overrode_the_cue = true;
      }
      if (app_state.transport() == AppState::Transport::kStopped && app_state.bar() == 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  session.stop();

  // THE ASSERTION (the song genuinely ends by itself): the real transport
  // readback, parked exactly the way app_state.cpp's own "stopped" reduction
  // parks it.
  CHECK(app_state.transport() == AppState::Transport::kStopped);
  CHECK(app_state.bar() == 0);
  // THE ASSERTION (still no override, all the way to the stop): re-checked
  // after the second wait window too.
  CHECK(!autosong_overrode_the_cue);
  // The engine's own section readback must still read "ending1" at the
  // moment of the stop -- app_state.cpp's "stopped" reduction never touches
  // m_section, so if auto-song HAD overridden the cue with some other
  // section, this would still show that other section's name, never
  // "ending1", even after the (wrong) stop.
  CHECK(app_state.section() == "ending1");
  // Corroborating (not primary) signal: the SAME fx bookkeeping every scene
  // launch already mutates never moved off the scene active when Ending was
  // cued.
  CHECK(fx.active_scene == scene_at_ending_click);

  ImGui::DestroyContext();
}

// -----------------------------------------------------------------------
// TEST 2 (regression guard, independent of the Ending pad's own existence):
// the Stop pad must keep doing an IMMEDIATE cold stop -- never cue an ending
// -- so a future refactor that merges the two behaviors (e.g. routing Stop
// through the same cue-then-stop path Ending uses) is caught here, not
// silently shipped. Mirrors test_transport_play_stop_ui_automation.cpp's own
// Play/Stop scenario, with the added "ending1" negative check this task
// specifically calls for.
// -----------------------------------------------------------------------
void test_stop_button_still_does_immediate_cold_stop() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  SeqEditModel seqedit;
  PartsModel parts;
  V02State fx;
  AppState app_state;
  InProcessBrainSession session;

  const Fixture setup = setup_and_play(model, seqedit, parts, session, app_state, fx);

  // Real click: Stop (never Ending).
  click_at(setup.stop_border_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  bool saw_ending_section = false;
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < deadline) {
      poll_once(session, app_state);
      if (app_state.section() == "ending1") {
        saw_ending_section = true;
      }
      render_one_frame(model, seqedit, parts, session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kStopped && app_state.bar() == 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  session.stop();

  // THE ASSERTION (immediate, cold stop, unchanged): the real transport
  // readback settles to stopped/bar-0 well within Ending1's own bar-quantized
  // cue-then-play-out window (design doc §3.2: Stop stays "immediate, no
  // cueing of any kind" -- a genuinely different gesture from Ending).
  CHECK(app_state.transport() == AppState::Transport::kStopped);
  CHECK(app_state.bar() == 0);
  // THE ASSERTION (the two controls stay distinct): Stop must never cue
  // "ending1" -- if it ever did, this would be the merged-behavior
  // regression this test exists to catch.
  CHECK(!saw_ending_section);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_ending_button_cues_ending_then_engine_stops_transport_without_autosong_override();
  test_stop_button_still_does_immediate_cold_stop();
  return sonotron::test::failures();
}

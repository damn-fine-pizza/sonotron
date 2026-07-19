// UI-AUTOMATION functional test (Torquato QA pass, flow-verification-matrix-
// 2026-07.md §2a): "Press Play -> transport starts" and "Press Stop ->
// transport stops, bar parks to 0" -- the matrix's own verdict for both rows
// was "the BUTTON->send() call site itself is exercised only by grep-level
// trust" (Play) / "No functional coverage of Stop through a real backend
// exists today" (Stop). This test closes both gaps at once with REAL input
// injection (imgui_headless_harness.hpp's click_at()-equivalent helpers)
// through the REAL render_transport_panel entry point, wired to a REAL
// InProcessBrainSession (real engine thread, real ring, real decode) -- never
// a hand-written `brain_session.send("transport start")` call, never a
// UiState/AppState field write standing in for a click.
//
// Real-vs-stubbed tag (docs/proposals/flow-verification-matrix-2026-07.md §1
// naming convention): `_ui_automation` == real click injection + real
// backend + real rendered-output/engine readback, the highest fidelity this
// project's harness currently offers.

#include "imgui.h"
#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/in_process_brain_session.hpp"
#include "src/neon_widgets.hpp"
#include "src/theme.hpp"
#include "src/transport_panel.hpp"
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <chrono>
#include <thread>
#include <vector>

using sonotron::AppState;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::InProcessBrainSession;
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

// One headless frame around the real transport panel, mirroring main.cpp's
// own per-frame shape (fx.playing refreshed from app_state.transport() right
// before drawing).
void render_one_frame(BrainSession& brain_session, AppState& app_state, UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::Begin("test");
  sonotron::render_transport_panel(app_state, brain_session, fx);
  ImGui::End();
  ImGui::Render();
}

// A real two-frame press+release click at `pos` (ImGuiButtonFlags_
// PressedOnClickRelease, ImGui's InvisibleButton default), mirroring every
// other UI-automation test's own click_at().
void click_at(ImVec2 pos, BrainSession& brain_session, AppState& app_state, UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  render_one_frame(brain_session, app_state, fx);
}

void test_real_play_click_starts_transport_and_climbs_bar_then_real_stop_click_parks_it() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  UiState fx;
  AppState app_state;
  InProcessBrainSession session;
  CHECK(session.start());

  // Warm-up + locate frame: the transport panel's Play/Stop pads are always
  // rendered (never conditionally hidden), so ONE locate frame is enough for
  // both buttons -- their screen rect never moves across a transport state
  // change.
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(session, app_state, fx);
  render_one_frame(session, app_state, fx);
  ImDrawData* locate = ImGui::GetDrawData();

  // Play pad: pad_button("play", ..., kCyan, filled=true, ...) paints fill
  // AND border with the EXACT SAME ImU32 at rest, neon::u32(kCyan, 0.9F)
  // (neon_widgets.cpp's pad_button, filled=false-vs-true branch) -- same
  // color the sibling test_repeat_zone_playhead_ui_automation.cpp already
  // locates the Play pad by.
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);

  // Stop pad: pad_button("stop", ..., kTextSecondary, filled=false, ...) --
  // filled=false makes the FILL color kStopDark regardless of accent (shared
  // with the Panic pad, which also renders filled=false), so the fill alone
  // cannot disambiguate Stop from Panic. The BORDER, however, is always
  // neon::u32(accent, 0.6F) at rest -- accent is kTextSecondary for Stop and
  // kPink for Panic, so this color is unique to the Stop pad in this frame.
  const ImU32 stop_border_color = sonotron::neon::u32(sonotron::theme::kTextSecondary, 0.6F);
  const th::Rect stop_rect = th::find_single_color_rect(locate, stop_border_color);
  CHECK(stop_rect.found);

  // Real content + real click #1: press Play (transport_panel.cpp's REAL
  // button verb -- `brain_session.send("transport start")` +
  // `app_state.note_transport_sent(true)` -- never hand-written here).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(session, app_state, fx);
  click_at(play_rect.center(), session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Pump the REAL per-frame pipeline (poll -> apply -> render) across up to 6
  // REAL wall-clock seconds, bounded on WALL TIME (not iteration/frame
  // count), same discipline every other real-backend test in this directory
  // already uses, until the transport genuinely reports playing AND at least
  // one real "beat" heartbeat has landed.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    std::vector<BrainEvent> events;
    while (std::chrono::steady_clock::now() < deadline) {
      events.clear();
      session.poll(events);
      for (const BrainEvent& ev : events) {
        app_state.apply(ev);
      }
      render_one_frame(session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  // THE ASSERTION (Play): the real click must have made the transport
  // genuinely report playing, with the beat heartbeat actually climbing --
  // not merely the optimistic note_transport_sent() hint (which flips
  // instantly regardless of whether the engine thread is even alive).
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);

  // Real click #2: press Stop (transport_panel.cpp's REAL button verb --
  // `brain_session.send("transport stop")` + `app_state.note_transport_
  // sent(false)`).
  click_at(stop_rect.center(), session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Wait for BOTH readbacks to agree, not just the first one to flip:
  // app_state.hpp's own note_transport_sent(false) (transport_panel.cpp's
  // Stop-button verb) sets m_transport = kStopped OPTIMISTICALLY, the instant
  // the click is drawn -- WITHOUT touching m_bar/m_beat/m_pulse. Only the
  // REAL kTransport OutEvent's "stopped" reduction (app_state.cpp) zeroes the
  // bar. Breaking on transport()==kStopped alone (this test's first-draft
  // shape) races the optimistic hint and fails spuriously on bar()==0 before
  // the real confirmation has had a chance to arrive -- confirmed empirically
  // by instrumenting this exact loop: the real kTransport("stopped") event
  // (and the bar->0 reduction with it) lands within a single extra poll
  // cycle once actually waited for. This was a TEST-DESIGN gap, not a product
  // bug: the loop's own break condition must require both signals together.
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    std::vector<BrainEvent> events;
    while (std::chrono::steady_clock::now() < deadline) {
      events.clear();
      session.poll(events);
      for (const BrainEvent& ev : events) {
        app_state.apply(ev);
      }
      render_one_frame(session, app_state, fx);
      if (app_state.transport() == AppState::Transport::kStopped && app_state.bar() == 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  session.stop();

  // THE ASSERTION (Stop): a real Stop click must genuinely stop the
  // transport AND park the bar/beat/pulse readout back to 0 (app_state.cpp's
  // kTransport "stopped" reduction) -- proven here through the REAL engine's
  // own "transport" OutEvent, not just the optimistic hint.
  CHECK(app_state.transport() == AppState::Transport::kStopped);
  CHECK(app_state.bar() == 0);

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_real_play_click_starts_transport_and_climbs_bar_then_real_stop_click_parks_it();
  return sonotron::test::failures();
}

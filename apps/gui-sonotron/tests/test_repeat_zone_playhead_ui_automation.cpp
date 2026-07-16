// UI-AUTOMATION RED PIN (Torquato QA pass, owner bug #1): "No playhead in
// the Repeat Zone: with auto-song on + transport playing, no beat-synced
// sweep bar is ever visible on the active scene's cell."
//
// Unlike every prior grid_panel functional test (test_grid_panel_auto_song*.
// cpp, test_grid_panel_auto_song_real_backend.cpp), this test injects REAL
// input -- io.AddMousePosEvent/AddMouseButtonEvent through imgui_headless_
// harness.hpp's click_at()-equivalent helpers, across real NewFrame/EndFrame
// cycles -- to press the REAL "Play" transport pad button
// (transport_panel.cpp) and click a REAL, already-filled Repeat-Zone launch
// cell (grid_panel.cpp's draw_cell), and it asserts on the ACTUAL RENDERED
// DRAW DATA (ImGui::Render()'s ImDrawData) for the playhead primitive
// (neon::playhead_at's own two AddLine calls), not on any AppState/V02State
// field. It does not need to click the "auto-song" header toggle at all
// (see the note before test_no_playhead_after_real_play_and_real_launch()
// below for why) -- side-stepping the ONE widget in these panels
// (ImGui::SmallButton with an alpha-0-at-rest background) this harness's own
// header comment flags as not headlessly rect-locatable without a product
// seam.
//
// Backend: a REAL InProcessBrainSession (main.cpp's own default, in-process
// engine thread + real OutEvent ring), REAL `session.send(...)` calls
// (fired by the REAL widget click handlers, never hand-written by this
// test), and a REAL per-frame poll()->AppState::apply() reduction -- same
// discipline as test_grid_panel_auto_song_real_backend.cpp, extended with
// real click injection and rendered-output assertions on top.

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

// One headless ImGui frame around BOTH real panels this bug spans: the
// transport rack (the real "Play" button) and the Repeat Zone grid (the
// real launch cell + the real playhead draw). Mirrors main.cpp's own
// per-frame shape (fx.playing refreshed from app_state.transport() right
// before drawing, layout_renderer.cpp:81-82/107-108) and every prior
// grid_panel test's render_one_frame -- reused verbatim in spirit so a
// discrepancy here cannot be blamed on a frame-order artifact this harness
// introduces. Returns the frame's ImDrawData (valid only until the next
// ImGui::Render() call).
ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, V02State& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::Begin("test");
  sonotron::render_transport_panel(app_state, brain_session, fx);
  ImGui::Spacing();
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

// A real two-frame press+release click at `pos` (ImGui's default button
// behavior, ImGuiButtonFlags_PressedOnClickRelease, only reports "pressed"
// once the button goes down AND back up while still hovering the SAME
// widget) -- returns the ImDrawData of the RELEASE frame (the one where the
// click handler actually ran).
ImDrawData* click_at(ImVec2 pos, GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                     BrainSession& brain_session, AppState& app_state, V02State& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
}

// THE PINNED BUG. Real backend, real clicks, real render loop, asserted
// against real draw data.
//
// This deliberately never touches the "auto-song" header toggle: V02State's
// own defaults already make this reachable without it --
// `active_scene == 0` and `active_scene_start_bar == 0` out of the box
// (v02_state.hpp), and section_playhead_phase()'s only gate on TIME is
// `current_bar > 0` (grid_model.cpp) -- so the moment the transport is
// really playing and the first real "beat" heartbeat has arrived
// (app_state.bar() > 0), scene 0's own playhead phase is already >= 0 with
// ZERO scene-header or auto-song interaction. This is deliberate: it proves
// the defect is not specific to auto-song at all (contrary to how the owner
// report reads at first glance) -- it is that NO Repeat-Zone cell can EVER
// read as "playing" through a real launch, auto-song or not, because the
// demo content grid_panel.cpp's own seed_demo() seeds is never registered
// with the core's ClipMatrix (see this test's own root-cause comment below).
void test_no_playhead_after_real_play_and_real_launch() {
  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  // Headless (no renderer backend): force the default font atlas to build
  // now, exactly what a real backend's own NewFrame() would trigger --
  // otherwise ImFontAtlasUpdateNewFrame() asserts the first time ImGui::
  // NewFrame() runs below (same technique every other grid_panel test in
  // this directory already uses). The returned pixel buffer is discarded: no
  // texture is ever uploaded, no pixel is ever rasterized -- this stays a
  // pure host-only test with no GPU.
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
  CHECK(session.start());
  session.send("style load basic");
  for (std::size_t i = 0; i < sonotron::kBuiltinStyleNames.size(); ++i) {
    if (sonotron::kBuiltinStyleNames[i] == "basic") {
      fx.active_style = static_cast<int>(i);
      break;
    }
  }

  // Locate frame: mouse parked off-screen, nothing clicked yet. This is
  // the frame render_grid_panel's own seed_demo() seeds the demo grid on
  // (fx.seeded latches true here) -- the drums/scene-0 cell ("A") and the
  // Play pad button now exist to be found by their OWN rendered geometry.
  // A brand-new auto-sized ImGui window hides its own very first frame
  // entirely (no draw commands at all) while it measures its own content
  // size before ever showing anything -- a well-known ImGui quirk, not a
  // product bug -- so this warm-up frame (thrown away) is required before
  // the real "locate" frame below can find any geometry at all.
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);

  // Play button rect: pad_button("play", ..., theme::kCyan, filled=true,
  // fx.glow) paints its fill AND border with the EXACT SAME ImU32 at rest
  // (not hovered -> no color boost) -- neon::u32(theme::kCyan, 0.9F),
  // transport_panel.cpp / neon_widgets.cpp:148-149. Nothing else in these
  // two panels paints that exact alpha over kCyan (the header text uses
  // alpha 1.0, the scene-header underline uses 0.25), so a single-region
  // scan is enough -- no clustering needed.
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);

  // Drums (row 0) launch cells: draw_cell's fill for a filled, not-hovered,
  // not-playing cell is neon::u32(track_color, 0.10F) (grid_panel.cpp's
  // `fill_a = playing ? 0.26F : (hovered ? 0.16F : 0.10F)`). theme::
  // kV02TrackColor reuses kCyan for BOTH row 0 (drums) and row 4 (arp)
  // (theme.hpp), so this exact color also matches arp's own filled cells --
  // harmless here, because find_color_clusters returns clusters in DRAW
  // ORDER (imgui_headless_harness.hpp's own header comment) and render_
  // grid_panel draws every drums-row cell before it ever reaches row 4, so
  // clusters[0] is always drums/scene 0 ("A" in seed_demo()'s pattern) --
  // the ACTIVE scene at boot (V02State::active_scene defaults to 0) --
  // regardless of how many later (row-4) clusters share the same color.
  const ImU32 drums_not_playing = sonotron::neon::u32(sonotron::theme::kV02TrackColor[0], 0.10F);
  const std::vector<th::Rect> drums_cells = th::find_color_clusters(locate, drums_not_playing);
  CHECK(!drums_cells.empty());
  if (drums_cells.empty()) {
    ImGui::DestroyContext();
    return;
  }
  const th::Rect scene0_cell = drums_cells.front();

  // Real click #1: press Play (transport_panel.cpp's REAL button verb --
  // `brain_session.send("transport start")` + `app_state.note_transport_
  // sent(true)` -- never hand-written by this test).
  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);

  // Real click #2: launch the drums/scene-0 cell for real (grid_panel.cpp's
  // REAL `render_track_cell` click handler -- filled cell -> `brain_session.
  // send("launch clip 0 quantize 1")`, `fx.open_cell = 0`, etc. -- never
  // hand-written by this test). cell_id(role_index=0, scene=0, scene_count=5)
  // == 0 * 5 + 0 == 0 (grid_panel.cpp's own cell_id formula).
  constexpr int kDrumsScene0ClipId = 0;
  click_at(scene0_cell.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Pump the REAL per-frame pipeline (poll -> apply -> render, main.cpp's
  // own shape) across up to 6 REAL wall-clock seconds, bounded on WALL TIME
  // (not iteration/frame count, same discipline test_in_process_brain_
  // session.cpp's own poll_until and test_grid_panel_auto_song_real_backend.
  // cpp already use) until the transport genuinely reports playing AND at
  // least one real "beat" heartbeat has landed (app_state.bar() > 0).
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
  std::vector<BrainEvent> events;
  ImDrawData* final_draw_data = nullptr;
  while (std::chrono::steady_clock::now() < deadline) {
    events.clear();
    session.poll(events);
    for (const BrainEvent& ev : events) {
      app_state.apply(ev);
    }
    final_draw_data = render_one_frame(model, seqedit, parts, session, app_state, fx);
    if (app_state.transport() == AppState::Transport::kPlaying && app_state.bar() > 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  session.stop();

  // Sanity: the transport must have genuinely reported playing with at
  // least one real beat landed, or nothing below is a meaningful pin.
  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);

  // ROOT CAUSE, proved directly: `launch clip 0 quantize 1` was sent for
  // real (the click above), but clip id 0 was NEVER registered with the
  // core's ClipMatrix -- grid_panel.cpp's seed_demo() only ever calls
  // GridModel::set_cell() (host-side display state); the ONLY path that
  // registers a real ClipMatrix clip is the browser style drag-drop handler
  // (render_track_cell's BeginDragDropTarget block, `clip add ... id
  // <id>`), never seed_demo(). Engine::clip_launch (components/core/
  // arrangrr/src/engine.cpp:1166-1168) forwards straight to Engine::
  // clip_request (engine.cpp:1192-1213), whose `const Clip* c =
  // m_clips.get(id); if (c == nullptr) { sink(OutEvent::warn(...)); return;
  // }` fires for any never-registered id -- a warn, and NO `clip` OutEvent
  // is ever emitted for id 0. AppState::apply()'s kClip case (app_state.cpp:
  // 120-125) is therefore never reached for this id, so AppState::
  // clip_state(0) keeps reading its default, kStopped, forever (app_state.
  // hpp:117-119) -- EXPECTED (correct) behavior is that a real launch makes
  // the clip read as armed/playing; this fails.
  CHECK(app_state.clip_state(kDrumsScene0ClipId) != AppState::ClipLaunchState::kStopped);

  // THE RENDERED-OUTPUT ASSERTION (owner symptom, verified against the
  // ACTUAL draw data, not a field read): grid_panel.cpp's draw_cell only
  // ever calls neon::playhead_at() when `playing && fx.playing &&
  // section_phase >= 0.0F` (grid_panel.cpp:302) -- `playing` here is
  // `app_state.clip_state(id) != kStopped` (render_track_cell, grid_panel.
  // cpp:681-682). Transport IS playing (fx.playing true) and scene 0's own
  // section_phase IS >= 0 (app_state.bar() > 0, checked above) -- but
  // `playing` for THIS cell is false (root cause above), so playhead_at()
  // structurally never runs for it. EXPECTED (correct) behavior: a launched,
  // playing scene's cell shows the beat-synced sweep; this must find NO
  // playhead-colored vertex in the cell's own bounds, which is exactly the
  // owner's "no beat-synced sweep bar is ever visible" report.
  CHECK(final_draw_data != nullptr);
  if (final_draw_data != nullptr) {
    const ImU32 playhead_core = sonotron::neon::u32(sonotron::theme::kText, 0.8F);
    const ImU32 playhead_wash = sonotron::neon::u32(sonotron::theme::kText, 0.25F);
    const bool playhead_found =
        th::any_vertex_with_color_in(final_draw_data, playhead_core, scene0_cell) ||
        th::any_vertex_with_color_in(final_draw_data, playhead_wash, scene0_cell);
    CHECK(playhead_found);
  }

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_no_playhead_after_real_play_and_real_launch();
  return sonotron::test::failures();
}

// UI-AUTOMATION RED PIN, OWNER-TRACE-DRIVEN CORROBORATION (Torquato QA
// pass): replays fixtures/owner_repro_2026_07_16.jsonl -- a REAL input trace
// the owner recorded live against this tree's --trace-input feature on
// 2026-07-16, capturing the exact live session where auto-song stayed on
// intro1 forever, no playhead sweep ever appeared in the Repeat Zone, and
// the launch-cell preview did not match Sequence Edit -- through imgui_
// headless_harness.hpp's load_input_trace()/apply_trace_frame() loader, tick
// -by-tick against a REAL InProcessBrainSession and the REAL render loop.
//
// HONEST DIVERGENCE FROM A PIXEL-FAITHFUL REPLAY (flagged, not hidden): the
// trace's own (x,y) mouse coordinates were captured against main.cpp's FULL
// 6-zone workstation layout (Transport/Browser/Repeat Zone/Parts/Sequence
// Edit, per layout.json) at 1280x800. This test's own window -- like every
// other functional test in this directory -- renders only the two panels
// (transport_panel.cpp/grid_panel.cpp) the pinned bug actually lives in, NOT
// the full layout_renderer.cpp zone arrangement, so the trace's own absolute
// pixel coordinates do NOT land on the same widgets here they did in the
// original live recording (the Repeat Zone grid sits at a different screen
// offset in a 2-panel test window than in the real 6-zone app). Reproducing
// the recording pixel-for-pixel would require running the REAL layout_
// renderer.cpp/layout_json.cpp path end to end (the full app minus GLFW/GL),
// a materially larger integration than this pass's scope -- flagged here as
// a follow-up, not invented as a silent approximation.
//
// What this test DOES do, honestly: it uses the SAME real-rect-derived
// click technique test_repeat_zone_playhead_ui_automation.cpp already
// proved (Play button + a real filled cell, both located from their own
// rendered geometry, never a hard-coded position) to put the REAL backend
// into the exact "transport playing, a scene genuinely launched" state the
// owner's session reached, and THEN replays the trace's mouse-MOVE (`mp`)
// traffic only -- 665 real recorded mouse positions, frame-paced -- as
// realistic hover/interaction noise on top, while polling the REAL engine
// for the full ~2240-frame span the owner's own session covered. `mb`/`key`
// events (8 real clicks, 20 key edges) are counted and logged but NOT
// replayed against this reduced window's own (different) widget positions,
// since doing so could just as easily mis-click an unintended widget here
// (e.g. an M/S latch) as reproduce the owner's intent -- exactly the
// divergence flagged above. This still exercises the real engine thread and
// the real render loop over the owner's own recorded session LENGTH and
// mouse-motion profile, and re-asserts the same rendered-output pin
// test_repeat_zone_playhead_ui_automation.cpp already proves in isolation,
// now under that extended, genuinely-recorded traffic.
//
// SONG-MODE PHASE 1 UPDATE (Torquato QA, docs/proposals/song-mode-
// scenechain-adoption.md): this test used to hold on scene 0's cell for the
// entire ~2240-frame span, because the retired per-frame auto-song FSM never
// actually resolved the demo grid's own scene 0 ("intro1") against the
// engine's REAL section -- it just froze fx.active_scene at 0 forever. Now
// that reconcile_active_scene (grid_panel.cpp) reads the ACTUAL Arranger
// section, this test observes something genuinely different: "basic"'s
// intro1 is an intrinsic 2-bar one-shot (components/core/arrangrr/include/
// arrangrr/arranger/styles/basic.hpp, `.bars=2`) that Arranger::on_tick
// resolves back to `m_return_to` (== kVarA -- the section active when the
// transport last started, arranger.hpp:360-367) after those 2 bars, WHOLLY
// INDEPENDENT of the SceneChain step's own n_bars hold (8, GridModel::
// kDefaultSceneBars) -- the SceneChain only ever applies a step's Performance
// once, at the transition; it does not re-arm the Arranger's own section
// clock. Verified by hand (three independent runs): by the time this ~4.5+
// real-wall-clock-second replay ends, app_state.bar() is consistently 3-4
// (well past intro1's 2-bar resolution, well short of the SceneChain's own
// 8-bar step-0 hold), app_state.section() reads "varA", and fx.active_scene
// has correctly followed it to 1 (demo scene 1 is ALSO seeded to kVarA, see
// seed_demo's kDemoSections) -- exactly the CORRECT one-shot-intro behavior,
// not a bug, and the opposite of the old FSM's "stuck on intro1 forever"
// symptom this test was written to catch.
//
// A SEPARATE, GENUINE REGRESSION THIS SAME LONGER RUN NOW EXPOSES (verified
// by hand, NOT papered over): the beat-synced playhead this test's own name
// promises to keep pinned green DISAPPEARS once fx.active_scene moves off
// scene 0. draw_cell (grid_panel.cpp) only ever paints the playhead when
// `playing` (== AppState::clip_state(cell_id) != kStopped) is ALSO true for
// THAT cell -- and Song-mode Phase 1's own design decision 4 deliberately
// retired every ClipMatrix-touching send from the auto-song advance path
// (SceneChain is "own-transport ... bypasses ClipMatrix entirely" per the
// proposal's own "Core facts"). So the ONLY cell ever marked `playing` in
// this whole run is the ONE this test manually clicks (scene 0's, line 192,
// `launch clip 0 quantize 1`) -- clip 1 (demo scene 1's drums cell, "B") is
// never independently armed, so once the active column reconciles to scene
// 1, NO cell anywhere satisfies `playing`, and neon::playhead_at() never
// fires for anyone. Confirmed directly: manually clicking scene 1's cell too
// (an experiment, not part of the shipped fix) makes the playhead reappear
// there -- proving the root cause is exactly this ClipMatrix/SceneChain
// decoupling, not a rendering or geometry mistake in this test. In real,
// ordinary auto-song play (no manual per-cell clicks, which is the actual
// owner-reported use case Phase 1 set out to fix), the playhead sweep will
// now vanish the instant the active section first advances past whichever
// single cell a user happened to click -- reintroducing, in a new form, the
// exact "no playhead sweep ever appeared" symptom (owner bug #1) this test
// was written to guard end to end. This is a genuine PRODUCT gap exposed by
// Phase 1, not a test artifact: flagged here and handed to Giotto (see
// this pass's QA report), NOT silently fixed or asserted around.

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
#include "src/ui_state.hpp"

#include "imgui_headless_harness.hpp"
#include "test.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
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
using sonotron::UiState;
namespace th = sonotron::test_harness;

namespace {

ImDrawData* render_one_frame(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                             BrainSession& brain_session, AppState& app_state, UiState& fx) {
  fx.playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
  ImGui::Begin("test");
  sonotron::render_transport_panel(app_state, brain_session, fx);
  ImGui::Spacing();
  sonotron::render_grid_panel(model, seqedit, parts, brain_session, app_state, fx);
  ImGui::End();
  ImGui::Render();
  return ImGui::GetDrawData();
}

ImDrawData* click_at(ImVec2 pos, GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                     BrainSession& brain_session, AppState& app_state, UiState& fx) {
  th::queue_mouse_down(pos);
  render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
  th::queue_mouse_up(pos);
  return render_one_frame(model, seqedit, parts, brain_session, app_state, fx);
}

void test_no_playhead_under_owner_recorded_session_traffic() {
  const std::vector<th::TraceEvent> trace = th::load_input_trace(GUI_SONOTRON_OWNER_TRACE_FIXTURE);
  CHECK(!trace.empty());

  int mp_count = 0;
  int mb_count = 0;
  int key_count = 0;
  int max_frame = 0;
  for (const th::TraceEvent& ev : trace) {
    max_frame = std::max(max_frame, ev.frame);
    switch (ev.kind) {
      case th::TraceEvent::Kind::kMousePos:
        ++mp_count;
        break;
      case th::TraceEvent::Kind::kMouseButton:
        ++mb_count;
        break;
      case th::TraceEvent::Kind::kKey:
        ++key_count;
        break;
    }
  }
  std::printf("owner trace: %d mp / %d mb / %d key events, max frame %d\n", mp_count, mb_count,
              key_count, max_frame);
  // The fixture's own known shape (recorded 2026-07-16, see this file's own
  // header comment) -- if this ever fails, the checked-in fixture changed
  // and every comment above describing "the owner's session" needs
  // re-deriving, not silencing.
  CHECK(mp_count == 665);
  CHECK(mb_count == 16);  // 8 full press+release clicks
  CHECK(key_count == 20);

  ImGui::CreateContext();
  ImGui::GetIO().DisplaySize = ImVec2(1280.0F, 800.0F);
  unsigned char* tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  GridModel model(5);
  SeqEditModel seqedit;
  PartsModel parts;
  UiState fx;
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

  // Real-rect-derived setup (the SAME proven technique as
  // test_repeat_zone_playhead_ui_automation.cpp): locate the real Play
  // button and the real drums/scene-0 cell, then click both for real,
  // putting the backend into the exact "playing, a scene genuinely
  // launched" state the owner's own session reached.
  // A brand-new auto-sized ImGui window hides its own very first frame
  // entirely (no draw commands at all) while it measures its own content
  // size -- a well-known ImGui quirk -- so this warm-up frame is required
  // before the real "locate" frame below can find any geometry at all (see
  // test_repeat_zone_playhead_ui_automation.cpp's own comment on this).
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));
  render_one_frame(model, seqedit, parts, session, app_state, fx);
  ImDrawData* locate = render_one_frame(model, seqedit, parts, session, app_state, fx);
  const ImU32 play_color = sonotron::neon::u32(sonotron::theme::kCyan, 0.9F);
  const th::Rect play_rect = th::find_single_color_rect(locate, play_color);
  CHECK(play_rect.found);
  // theme::kTrackColor reuses kCyan for both row 0 (drums) and row 4
  // (arp) -- harmless, since find_color_clusters preserves DRAW ORDER and
  // drums cells are always drawn before arp's, so clusters[0] is always
  // drums/scene 0 (see test_repeat_zone_playhead_ui_automation.cpp's own
  // comment on this).
  const ImU32 drums_not_playing = sonotron::neon::u32(sonotron::theme::kTrackColor[0], 0.10F);
  const std::vector<th::Rect> drums_cells = th::find_color_clusters(locate, drums_not_playing);
  CHECK(!drums_cells.empty());
  if (drums_cells.empty()) {
    session.stop();
    ImGui::DestroyContext();
    return;
  }
  const th::Rect scene0_cell = drums_cells.front();

  click_at(play_rect.center(), model, seqedit, parts, session, app_state, fx);
  click_at(scene0_cell.center(), model, seqedit, parts, session, app_state, fx);
  th::queue_mouse_move(ImVec2(-100.0F, -100.0F));

  // Replay the owner's own recorded mouse-MOVE traffic (mp only -- see this
  // file's own header comment for why mb/key are counted but not replayed
  // against this reduced window), tick-by-tick across the owner's own
  // ~2240-frame session span, polling the REAL engine every frame so real
  // beats/bars keep landing throughout -- bounded on a short per-frame sleep
  // (not the recording's own ~16.6ms/frame) to keep this test's own runtime
  // reasonable while still giving the real engine thread genuine wall-clock
  // time to advance (documented divergence from the recording's own real-
  // time cadence, per this file's own header comment).
  ImDrawData* final_draw_data = locate;
  std::vector<BrainEvent> events;
  for (int frame = 0; frame <= max_frame; ++frame) {
    th::apply_trace_frame(trace, frame);
    events.clear();
    session.poll(events);
    for (const BrainEvent& ev : events) {
      app_state.apply(ev);
    }
    final_draw_data = render_one_frame(model, seqedit, parts, session, app_state, fx);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  session.stop();

  CHECK(app_state.transport() == AppState::Transport::kPlaying);
  CHECK(app_state.bar() > 0);

  // Same root cause as test_repeat_zone_playhead_ui_automation.cpp: the
  // demo-seeded cell was never registered with the core's ClipMatrix, so it
  // never reads as playing, even under this extended, genuinely-recorded
  // mouse-traffic session.
  constexpr int kDrumsScene0ClipId = 0;
  CHECK(app_state.clip_state(kDrumsScene0ClipId) != AppState::ClipLaunchState::kStopped);

  // THE DIAGNOSED, OWNER-APPROVED FIX, CORROBORATED: intro1 is a genuine
  // 2-bar one-shot in "basic" (styles/basic.hpp), so by the time this
  // ~4.5+ real-wall-clock-second replay ends, the ENGINE's own section has
  // long since resolved off "intro1" -- verified by hand across three runs,
  // consistently landing at bar 3-4, section "varA" -- and reconcile_active_
  // scene has correctly followed it to demo scene 1 (also seeded to kVarA).
  // This is the fix this pass corroborates: the OLD per-frame FSM never
  // synchronized against the real section at all, so it stayed frozen on
  // scene 0 ("intro1") for the test's ENTIRE duration -- passing only because
  // it was wrong, never because auto-song had genuinely progressed.
  CHECK(app_state.section() == "varA");
  CHECK(fx.active_scene == 1);

  // THE GENUINE REGRESSION THIS LONGER RUN EXPOSES (see this file's own
  // header comment for the full root-cause trace) -- NOT papered over: once
  // the active scene has correctly reconciled to 1, its own cell (drums
  // scene 1, "B") must show the beat-synced playhead sweep for the ORIGINAL
  // owner bug ("no playhead sweep ever appeared in the Repeat Zone") to
  // actually stay fixed end to end. It does not: Song-mode Phase 1 retired
  // every ClipMatrix-touching send from the auto-song advance path
  // (SceneChain "bypasses ClipMatrix entirely"), so scene 1's cell was never
  // independently armed and reads `playing == false` (AppState::clip_state
  // stays kStopped for clip id 1) -- draw_cell's playhead paint is gated on
  // exactly that flag, so it never fires here. THIS ASSERTION IS EXPECTED TO
  // FAIL until Giotto re-wires SOME real per-cell "playing" signal for the
  // SceneChain-driven active scene (see this pass's QA report for the
  // handoff) -- it is deliberately NOT relaxed, removed, or redirected to a
  // cell this test never claims is the song's real current scene.
  CHECK(final_draw_data != nullptr);
  if (final_draw_data != nullptr && drums_cells.size() > 1) {
    const th::Rect& active_scene_cell = drums_cells[1];  // fx.active_scene == 1, asserted above
    const ImU32 playhead_core = sonotron::neon::u32(sonotron::theme::kText, 0.8F);
    const ImU32 playhead_wash = sonotron::neon::u32(sonotron::theme::kText, 0.25F);
    const bool playhead_found =
        th::any_vertex_with_color_in(final_draw_data, playhead_core, active_scene_cell) ||
        th::any_vertex_with_color_in(final_draw_data, playhead_wash, active_scene_cell);
    CHECK(playhead_found);
  }

  ImGui::DestroyContext();
}

}  // namespace

int main() {
  test_no_playhead_under_owner_recorded_session_traffic();
  return sonotron::test::failures();
}

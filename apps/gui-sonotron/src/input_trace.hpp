#pragma once

// INPUT RECORD/REPLAY (owner ask: close the "green tests, broken app" gap).
// The owner reproduces live bugs by clicking around; this pair lets
// gui-sonotron RECORD those input events to a JSONL file as they are
// consumed each frame, and REPLAY the exact same file back later to trigger
// the exact same effect -- either headlessly (SONOTRON_GUI_MAX_FRAMES) or in
// the normal, VISIBLE windowed app, so the owner can literally watch the
// session replay live and attach a debugger at the frame where a bug shows
// (`gdb --args ./gui-sonotron --replay-input trace.jsonl`; nothing here
// installs signal handlers or otherwise fights a debugger attaching).
//
// Wire format (one JSON object per line, newline-terminated):
//   {"t":"meta","w":<int>,"h":<int>}                      -- once, at start
//   {"f":<int>,"t":"mp","x":<float>,"y":<float>}           -- mouse position
//   {"f":<int>,"t":"mb","b":<int>,"d":<0|1>}               -- mouse button
//   {"f":<int>,"t":"key","k":<int>,"d":<0|1>}              -- key up/down
// `f` is the app's own frame counter (main.cpp's `frame`, the same one
// SONOTRON_GUI_MAX_FRAMES counts against), so record and replay agree on
// framing without a wall-clock timestamp. `w`/`h` are the GLFW logical
// window size (glfwGetWindowSize) at the moment recording starts -- the
// same size ImGuiIO::DisplaySize is derived from (imgui_impl_glfw.cpp),
// since replay needs to hit the same widgets at the same coordinates.
//
// Capture/feed timing (see main.cpp's render_frame() for the exact call
// site): io.AddMousePosEvent/AddMouseButtonEvent/AddKeyEvent only QUEUE an
// event; Dear ImGui applies the queue to io.MousePos/io.MouseDown/
// io.KeysData inside ImGui::NewFrame() itself. So:
//   - InputTraceRecorder::capture_frame() must be called AFTER
//     ImGui::NewFrame() (reading those io fields any earlier would see last
//     frame's stale values) -- it then reads exactly what ImGui goes on to
//     act on this frame, not a re-derivation of the raw platform events (the
//     alternative capture point the task allowed).
//   - InputTraceReplayer::feed_frame() must be called AFTER
//     ImGui_ImplGlfw_NewFrame() (so it is queued after, and therefore wins
//     over, any real-cursor value that backend's own UpdateMouseData()
//     fallback injects -- see feed_frame()'s own doc comment) and BEFORE
//     ImGui::NewFrame() applies the queue.
//
// Determinism caveat: replay fidelity assumes the same DisplaySize and the
// same app-state evolution (layout.json, scenes.json, style loaded, etc.)
// as when the trace was recorded. This is a repro tool, not a
// cryptographic replay -- good enough to turn "I clicked X and Y happened"
// into a script that reliably reproduces Y, not a guarantee against any
// possible environment drift.

#include <cstdio>
#include <string>
#include <vector>

struct ImGuiIO;

namespace sonotron {

// Appends one JSONL line per changed input field to a file, active only
// when start() succeeds.
class InputTraceRecorder {
 public:
  InputTraceRecorder() = default;
  ~InputTraceRecorder();
  InputTraceRecorder(const InputTraceRecorder&) = delete;
  InputTraceRecorder& operator=(const InputTraceRecorder&) = delete;

  // Opens `path` for writing (truncating any existing file) and writes the
  // one-shot {"t":"meta",...} line. Returns false (recorder stays inactive)
  // if the file cannot be opened; logs the failure to stderr itself.
  bool start(const std::string& path, int width, int height);

  bool is_active() const { return m_file != nullptr; }

  // Diffs `io`'s current mouse position/buttons and named-key down state
  // against what was captured on the previous call, and appends one JSONL
  // line per field that changed, tagged with `frame`. No-op if
  // !is_active(). MUST be called after ImGui::NewFrame() -- see the header
  // comment above.
  void capture_frame(int frame, const ImGuiIO& io);

 private:
  std::FILE* m_file = nullptr;
  bool m_have_prev_mouse = false;
  float m_prev_x = 0.0F;
  float m_prev_y = 0.0F;
  bool m_prev_button[5] = {false, false, false, false, false};
  std::vector<bool> m_prev_key_down;  // lazily sized to ImGuiKey_NamedKey_COUNT
};

// One recorded event, frame-tagged, as loaded from a trace file.
struct ReplayEvent {
  enum class Kind { kMousePos, kMouseButton, kKey };

  int frame = 0;
  Kind kind = Kind::kMousePos;
  float x = 0.0F;
  float y = 0.0F;
  int code = 0;  // button index (kMouseButton) or ImGuiKey (kKey)
  bool down = false;
};

// Loads a JSONL trace (as written by InputTraceRecorder, or hand-written
// for a test) and feeds it back into ImGui frame-by-frame. Meant to be
// paired with ImGui_ImplGlfw_InitForOpenGL(window,
// /*install_callbacks=*/false) for the whole run so real mouse-button/key
// events never reach ImGui at all (the backend only ever feeds those
// through the callbacks this disables, no polling fallback exists for
// them). Real mouse POSITION is a partial exception -- see feed_frame()'s
// own doc comment for how this class compensates for it.
class InputTraceReplayer {
 public:
  // Reads `path` line by line; a malformed line is skipped (best-effort --
  // a hand-written smoke-test trace need not be a byte-perfect
  // re-serialization of the writer's own output). Unknown/"meta"/"hit"
  // lines are silently ignored. Returns false and logs to stderr if the
  // file cannot be opened; the replayer is left with zero events either
  // way, so feed_frame() is always safe to call.
  bool load(const std::string& path);

  // Feeds every event recorded for a frame <= `frame` that has not been
  // consumed yet into `io` via AddMousePosEvent/AddMouseButtonEvent/
  // AddKeyEvent, in file order, then advances past them. Assumes
  // non-decreasing frame numbers in the file (true both of this file's own
  // writer and of any reasonable hand-written trace).
  //
  // Real-cursor leak mitigation: with install_callbacks=false,
  // imgui_impl_glfw.cpp's UpdateMouseData() has no way to tell "replay is
  // driving the mouse on purpose" from "callbacks legitimately were never
  // wired up" -- it unconditionally re-queries and re-queues the REAL OS
  // cursor position on every focused frame as a fallback. Calling this
  // AFTER ImGui_ImplGlfw_NewFrame() (see input_trace.hpp's header comment
  // for the exact call site) means any position event we queue here is
  // LAST for the frame and therefore wins once ImGui::NewFrame() applies
  // the queue. To also win on frames where the trace itself has no new
  // "mp" line, this method re-asserts the last position it ever fed, every
  // frame, once at least one has been fed -- otherwise the leak would show
  // through on every frame the trace does not explicitly touch the mouse.
  // (Known residual gap: a trace with zero "mp" lines at all never
  // establishes a position to re-assert, so the real cursor would show
  // through from frame 1 until the first recorded move -- an accepted
  // limit of not patching the vendored backend for this repro tool.)
  void feed_frame(int frame, ImGuiIO& io);

  int events_consumed() const { return m_consumed; }
  std::size_t event_count() const { return m_events.size(); }

 private:
  std::vector<ReplayEvent> m_events;
  std::size_t m_next = 0;
  int m_consumed = 0;
  bool m_have_last_pos = false;
  float m_last_x = 0.0F;
  float m_last_y = 0.0F;
};

}  // namespace sonotron

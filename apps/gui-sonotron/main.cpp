// sonotron GUI — the 6-zone workstation screen (node 11600), G0-G3.
//
// The window shows the 6-zone workstation screen (Transport across the top;
// Browser | Repeat Zone | the Intention-over-Parts right rail across the
// middle; Sequence Edit across the bottom), laid out from a JSON layout
// file, with a File/Edit/View/Transport/Help menu bar and every zone
// dispatched to its live panel (G3, docs/design/gui-fase2-mechanical-plan.md).
// A UdsBrainSession connects (if given `--control <path>`) as a pure client
// and drives AppState; the core-dependent surfaces (real playhead, the
// live harmonic visualizer, real clip launch) stay honest placeholders
// until their core work lands (§11) — see docs/design/ux-workstation.md §3
// for the screen this lays out, and src/layout_model.hpp / layout_json.hpp
// / layout_renderer.hpp for the three-way split (pure-data model / JSON
// reader-writer / ImGui renderer).
//
// Pure client (D38): this file links neither arrangrr_core nor hostrt and
// includes zero core headers — only the vendored toolkit
// (third_party/imgui, third_party/glfw), system OpenGL, and this app's own
// src/ files.
//
// DPI/font pass: text is rendered with a vendored monospace TTF (JetBrains
// Mono NL, see assets/fonts/ARRGRR_VENDOR.md) rasterized at the window's
// GLFW content scale instead of ImGui's small built-in bitmap font, so the
// dashboard stays crisp on HiDPI displays. See content_scale_for(),
// font_path(), load_font() below. A font FILE is a data asset, not a code
// dependency — no new third-party code library was added for this.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/app_state.hpp"
#include "src/brain_event.hpp"
#include "src/brain_session.hpp"
#include "src/browser_model.hpp"
#include "src/grid_model.hpp"
#include "src/layout_json.hpp"
#include "src/layout_model.hpp"
#include "src/layout_renderer.hpp"
#include "src/parts_model.hpp"
#include "src/screenshot.hpp"
#include "src/seqedit_model.hpp"
#include "src/uds_brain_session.hpp"
#include "src/workstation_state.hpp"

namespace {

void glfw_error_callback(int error, const char* description) {
  std::fprintf(stderr, "sonotron: GLFW error %d: %s\n", error, description);
}

// The layout file lives in the user's config dir by default
// ($XDG_CONFIG_HOME/sonotron/layout.json, falling back to
// $HOME/.config/sonotron/layout.json) so it survives independently of
// wherever the binary happens to be launched from. SONOTRON_LAYOUT_PATH
// overrides it — used by the headless verification path below, exactly
// like SONOTRON_GUI_MAX_FRAMES is; the shipped app is never launched with
// either variable set.
std::string layout_path() {
  if (const char* override_path = std::getenv("SONOTRON_LAYOUT_PATH"); override_path != nullptr) {
    return override_path;
  }
  if (const char* xdg_config = std::getenv("XDG_CONFIG_HOME"); xdg_config != nullptr) {
    return std::string(xdg_config) + "/sonotron/layout.json";
  }
  if (const char* home = std::getenv("HOME"); home != nullptr) {
    return std::string(home) + "/.config/sonotron/layout.json";
  }
  return "sonotron-layout.json";
}

// Resolves the control-socket path the GUI connects to as a pure client
// (docs/design/gui-contract-map.md, ux-workstation.md §9): `--control
// <path>` (the same flag `cli-arrangrr --control` takes) wins, then the
// SONOTRON_CONTROL_PATH env var (same override pattern as
// SONOTRON_LAYOUT_PATH/SONOTRON_FONT_PATH above). Neither set means "run
// disconnected" -- there is no default socket path (Edit > Preferences,
// ux-workstation.md §12, is a later milestone for setting this from inside
// the GUI).
std::string control_path_from_args(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--control") {
      return argv[i + 1];
    }
  }
  if (const char* env = std::getenv("SONOTRON_CONTROL_PATH"); env != nullptr) {
    return env;
  }
  return "";
}

// Headless-smoke escape hatch: if SONOTRON_GUI_MAX_FRAMES=N is set, render
// exactly N frames then exit instead of waiting for the window to be
// closed by the user. Used only by the build/verification path (there is
// no display-less way to prove a GUI app's frame loop otherwise); the
// shipped app is never launched with this variable set.
int max_frames_from_env() {
  const char* value = std::getenv("SONOTRON_GUI_MAX_FRAMES");
  if (value == nullptr) {
    return -1;
  }
  return std::atoi(value);
}

// Headless-verification escape hatch, sibling to SONOTRON_GUI_MAX_FRAMES:
// if SONOTRON_GUI_SCREENSHOT=<path> is set, the final rendered frame is
// captured to that PNG (see capture_screenshot below). The shipped app is
// never launched with this variable set.
const char* screenshot_path_from_env() { return std::getenv("SONOTRON_GUI_SCREENSHOT"); }

// Reads the current back buffer and writes it to `path` as a PNG. Must be
// called AFTER the frame is drawn but BEFORE glfwSwapBuffers, while the
// rendered image still lives in the (default GL_BACK) read buffer.
void capture_screenshot(int width, int height, const char* path) {
  if (width <= 0 || height <= 0) {
    return;
  }
  std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  if (sonotron::write_png_rgba_bottom_up(path, width, height, pixels.data())) {
    std::fprintf(stdout, "sonotron: screenshot written to %s (%dx%d)\n", path, width, height);
  } else {
    std::fprintf(stderr, "sonotron: failed to write screenshot to %s\n", path);
  }
}

GLFWwindow* create_window() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // Ask the platform to report an accurate content (DPI) scale and to size
  // the window in scaled pixels right away, instead of only finding out
  // about HiDPI after a content-scale-changed event fires post-creation.
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
  return glfwCreateWindow(1280, 800, "sonotron", nullptr, nullptr);
}

// Queries the window's content (DPI) scale via GLFW. GLFW can report a
// zero/degenerate scale on some platforms before the window is mapped;
// guard against that by falling back to 1x rather than rasterizing a
// zero-sized (or negative-sized) font atlas.
float content_scale_for(GLFWwindow* window) {
  float xscale = 1.0F;
  float yscale = 1.0F;
  glfwGetWindowContentScale(window, &xscale, &yscale);
  if (!(xscale > 0.0F)) {
    xscale = 1.0F;
  }
  return xscale;
}

// The vendored monospace TTF lives at apps/gui-sonotron/assets/fonts/
// alongside the source (see assets/fonts/ARRGRR_VENDOR.md for provenance
// and license). SONOTRON_FONT_PATH overrides it — used by the
// verification path below to exercise the missing-file fallback without
// touching the real vendored asset, exactly like SONOTRON_LAYOUT_PATH.
std::string font_path() {
  if (const char* override_path = std::getenv("SONOTRON_FONT_PATH"); override_path != nullptr) {
    return override_path;
  }
  return std::string(SONOTRON_ASSETS_DIR) + "/fonts/JetBrainsMonoNL-Regular.ttf";
}

// Resolves the *logical* (1x DPI) base font size to use: `layout.font_size_px`
// if it is in the sane range, kDefaultFontSizePx otherwise (absent-from-JSON
// already lands on the default via Layout's in-class initializer; this is
// the defense-in-depth layer for a Layout built by other means, e.g. a
// future in-app editor writing a bad value directly).
float resolved_base_font_size_px(const sonotron::Layout& layout) {
  if (sonotron::is_valid_font_size_px(layout.font_size_px)) {
    return layout.font_size_px;
  }
  return sonotron::kDefaultFontSizePx;
}

// Loads the vendored TTF rasterized at content_scale * base_font_size_px so
// text stays crisp on HiDPI displays. If the file is missing or unreadable,
// falls back to ImGui's built-in bitmap font at the same pixel size rather
// than crashing or leaving the app without any font at all.
void load_font(ImGuiIO& io, float content_scale, float base_font_size_px) {
  const std::string path = font_path();
  const float size_px = base_font_size_px * content_scale;

  ImFontConfig config;
  config.Flags |= ImFontFlags_NoLoadError;  // we check the return value ourselves below
  ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size_px, &config);
  if (font == nullptr) {
    std::fprintf(stderr, "sonotron: could not load font '%s' - falling back to built-in font\n",
                 path.c_str());
    ImFontConfig fallback_config;
    fallback_config.SizePixels = size_px;
    io.Fonts->AddFontDefault(&fallback_config);
  }
}

// Finds a zone by id for the View-menu visibility toggles below. Returns
// nullptr if the layout (e.g. a stale hand-edited layout.json) has no zone
// with that id — the caller then just skips the toggle rather than crashing.
sonotron::Zone* find_zone(sonotron::Layout& layout, std::string_view id) {
  for (sonotron::Zone& zone : layout.zones) {
    if (zone.id == id) {
      return &zone;
    }
  }
  return nullptr;
}

// The menu bar (ux-workstation.md §4.1): File / Edit / View / Transport /
// Help, chrome outside the JSON-driven zone grid. Only View's
// Intention/Parts toggles and Transport's mirrors are wired to real
// behaviour this slice; the rest render as (mostly disabled) placeholders
// so the bar is discoverable without inventing functionality that is not
// there. `quit_requested` is set true on File > Quit — the GUI window only,
// NEVER a bare `quit` on the socket (that tears down the shared host for
// every client, gui-contract-map.md §0; BrainSession::send() blacklists it
// anyway as defense-in-depth, but the menu never even attempts it).
void render_menu_bar(sonotron::Layout& layout, sonotron::BrainSession& brain_session,
                     bool& quit_requested) {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  if (ImGui::BeginMenu("File")) {
    ImGui::MenuItem("New set", nullptr, false, false);
    ImGui::MenuItem("Open set...", nullptr, false, false);
    ImGui::MenuItem("Save set", nullptr, false, false);
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
      quit_requested = true;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Edit")) {
    ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
    ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
    ImGui::Separator();
    ImGui::MenuItem("Preferences...", nullptr, false, false);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    if (sonotron::Zone* intention = find_zone(layout, "intention")) {
      ImGui::MenuItem("Intention", nullptr, &intention->visible);
    }
    if (sonotron::Zone* parts = find_zone(layout, "parts")) {
      ImGui::MenuItem("Parts", nullptr, &parts->visible);
    }
    ImGui::MenuItem("Layout density...", nullptr, false, false);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Transport")) {
    if (ImGui::MenuItem("Start", "Ctrl+P")) {
      brain_session.send("transport start");
    }
    if (ImGui::MenuItem("Stop")) {
      brain_session.send("transport stop");
    }
    if (ImGui::MenuItem("Continue")) {
      brain_session.send("transport continue");
    }
    if (ImGui::MenuItem("Panic")) {
      brain_session.send("panic");
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    ImGui::MenuItem("About sonotron", nullptr, false, false);
    ImGui::MenuItem("Key bindings...", nullptr, false, false);
    ImGui::MenuItem("Contract / version", nullptr, false, false);
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
}

void render_frame(sonotron::Layout& layout, sonotron::WorkstationState& state,
                  bool& quit_requested) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  render_menu_bar(layout, state.brain_session, quit_requested);

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("sonotron", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  sonotron::render_layout(layout, state);
  ImGui::End();

  ImGui::Render();
}

// `screenshot_path` is non-null only on the frame that should be captured
// (the verification path); the capture happens after the draw and before the
// buffer swap, then normal presentation continues.
void present_frame(GLFWwindow* window, const char* screenshot_path) {
  int display_w = 0;
  int display_h = 0;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.10F, 0.10F, 0.12F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  if (screenshot_path != nullptr) {
    capture_screenshot(display_w, display_h, screenshot_path);
  }
  glfwSwapBuffers(window);
}

}  // namespace

int main(int argc, char** argv) {
  glfwSetErrorCallback(glfw_error_callback);
  if (glfwInit() == GLFW_FALSE) {
    std::fprintf(stderr, "sonotron: glfwInit failed\n");
    return 1;
  }

  GLFWwindow* window = create_window();
  if (window == nullptr) {
    std::fprintf(stderr, "sonotron: glfwCreateWindow failed\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // No layout persistence yet (that is a later, deliberate milestone) - do
  // not let ImGui write an imgui.ini next to wherever this is launched from.
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();

  // Layout is loaded before the font because it carries the configurable
  // logical font-size knob (Layout::font_size_px, the JSON "font_size"
  // key) that load_font() below needs.
  const std::string path = layout_path();
  sonotron::Layout layout;
  std::string layout_error;
  if (!sonotron::load_or_create_default(path, layout, layout_error)) {
    std::fprintf(stderr, "sonotron: failed to load layout from %s: %s\n", path.c_str(),
                 layout_error.c_str());
    layout = sonotron::default_layout();
  }
  std::fprintf(stdout, "sonotron: layout loaded from %s (%zu zones)\n", path.c_str(),
               layout.zones.size());

  // DPI awareness: rasterize the font atlas at the monitor's real content
  // scale (crisp on 2x/HiDPI, not upscaled-blurry) and scale the rest of
  // the style metrics (padding, spacing, borders) to match. This is a
  // one-shot query at startup, not a live per-monitor-move rescale — the
  // app does not yet react to a mid-session content-scale-changed event.
  const float content_scale = content_scale_for(window);
  const float base_font_size_px = resolved_base_font_size_px(layout);
  load_font(io, content_scale, base_font_size_px);
  ImGui::GetStyle().ScaleAllSizes(content_scale);
  std::fprintf(stdout,
               "sonotron: content scale %.2fx, logical font size %.1fpx, effective %.1fpx\n",
               static_cast<double>(content_scale), static_cast<double>(base_font_size_px),
               static_cast<double>(base_font_size_px * content_scale));

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  std::fprintf(stdout, "sonotron: window open, GL renderer: %s\n",
               reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

  // The GUI as a pure client of the headless core (docs/design/
  // gui-contract-map.md, ux-workstation.md §9): UdsBrainSession is the first
  // concrete BrainSession, connecting (if a control path was given at all)
  // to `cli-arrangrr --control <path>`. AppState holds no truth of its own;
  // it is only ever reduced from the events UdsBrainSession::poll() decodes.
  const std::string control_path = control_path_from_args(argc, argv);
  sonotron::UdsBrainSession brain_session;
  sonotron::AppState app_state;

  // The G3 zone panels' models (docs/design/gui-fase2-mechanical-plan.md):
  // pure data, owned here, threaded into the frame loop through
  // WorkstationState. The core-dependent surfaces they back (grid launch,
  // the sequence-edit note canvas) stay honest placeholders — see each
  // model/panel pair's own header comment for the exact gap.
  sonotron::BrowserModel browser_model;
  sonotron::GridModel grid_model;
  sonotron::SeqEditModel seqedit_model;
  sonotron::PartsModel parts_model;
  sonotron::WorkstationState workstation_state{.app_state = app_state,
                                               .brain_session = brain_session,
                                               .browser = browser_model,
                                               .grid = grid_model,
                                               .seqedit = seqedit_model,
                                               .parts = parts_model};

  if (!control_path.empty()) {
    if (brain_session.connect_to(control_path)) {
      std::fprintf(stdout, "sonotron: connected to control socket %s\n", control_path.c_str());
    } else {
      std::fprintf(stderr, "sonotron: could not connect to control socket %s: %s\n",
                   control_path.c_str(), brain_session.last_error().c_str());
    }
  } else {
    std::fprintf(stdout,
                 "sonotron: no control socket given (--control <path> or "
                 "SONOTRON_CONTROL_PATH) - running disconnected\n");
  }

  const int max_frames = max_frames_from_env();
  const char* screenshot_path = screenshot_path_from_env();
  int frame = 0;
  std::vector<sonotron::BrainEvent> brain_events;
  bool quit_requested = false;

  while (glfwWindowShouldClose(window) == GLFW_FALSE && !quit_requested) {
    if (max_frames >= 0 && frame >= max_frames) {
      break;
    }
    ++frame;

    // Capture only the last frame of a bounded (max-frames) run, once the UI
    // has settled — one PNG, not one per frame.
    const bool capture_this_frame =
        screenshot_path != nullptr && max_frames >= 0 && frame == max_frames;

    glfwPollEvents();

    // Poll-in-frame (gui-contract-map.md §1): drain the non-blocking socket
    // once, reduce every decoded event into AppState, then render from that
    // state. No background reader thread.
    brain_events.clear();
    brain_session.poll(brain_events);
    for (const sonotron::BrainEvent& event : brain_events) {
      app_state.apply(event);
      // No dedicated log/console zone exists yet in the §3 workstation
      // wireframe (that is G3+ territory); until one lands, the scrolling
      // event log is observable on stdout.
      std::fprintf(stdout, "sonotron: %s\n", app_state.log().back().c_str());
    }
    const bool brain_connected =
        brain_session.status() == sonotron::BrainSession::Status::kConnected;
    app_state.set_connected(brain_connected);

    render_frame(layout, workstation_state, quit_requested);
    present_frame(window, capture_this_frame ? screenshot_path : nullptr);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  // Persist on exit. There is no in-app editing of the layout yet (zones are
  // static frames, no drag-resize splitters), so this mainly keeps the
  // on-disk file in the writer's canonical form after a hand-edit; it is
  // the seam a future layout editor will use to save "on change".
  std::string save_error;
  if (!sonotron::save_layout(path, layout, save_error)) {
    std::fprintf(stderr, "sonotron: failed to save layout to %s: %s\n", path.c_str(),
                 save_error.c_str());
  }

  std::fprintf(stdout, "sonotron: closed cleanly after %d frame(s)\n", frame);
  return 0;
}

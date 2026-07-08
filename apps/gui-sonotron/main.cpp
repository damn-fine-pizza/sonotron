// sonotron GUI — general layout step (node 11600, JSON-driven zones).
//
// The window now shows the 5-zone primary-screen dashboard (Transport ·
// Seed / Intention | Band / Harmony | Structure) as EMPTY, titled frames,
// laid out from a JSON layout file. No socket, no brain connection, no live
// content in any zone yet — see docs/design/ux-concept.md for the mockup
// this lays out, and src/layout_model.hpp / layout_json.hpp /
// layout_renderer.hpp for the three-way split (pure-data model / JSON
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

#include "src/layout_json.hpp"
#include "src/layout_model.hpp"
#include "src/layout_renderer.hpp"

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

void render_frame(const sonotron::Layout& layout) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("sonotron", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  sonotron::render_layout(layout);
  ImGui::End();

  ImGui::Render();
}

void present_frame(GLFWwindow* window) {
  int display_w = 0;
  int display_h = 0;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.10F, 0.10F, 0.12F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
}

}  // namespace

int main() {
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

  const int max_frames = max_frames_from_env();
  int frame = 0;

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    if (max_frames >= 0 && frame >= max_frames) {
      break;
    }
    ++frame;

    glfwPollEvents();
    render_frame(layout);
    present_frame(window);
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

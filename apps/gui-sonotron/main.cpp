// sonotron GUI — bootstrap skeleton (node 11600 infrastructure step).
//
// This is deliberately NOT the real UI: it opens one ImGui window, renders
// a trivial frame (a label + the stock ImGui demo toggle), and closes
// cleanly. No socket, no brain connection, no view/panel model yet.
//
// Pure client (D38): this file links neither arrangrr_core nor hostrt and
// includes zero core headers — only the vendored toolkit
// (third_party/imgui, third_party/glfw) and system OpenGL.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

namespace {

void glfw_error_callback(int error, const char* description) {
  std::fprintf(stderr, "sonotron: GLFW error %d: %s\n", error, description);
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
  return glfwCreateWindow(1280, 800, "sonotron", nullptr, nullptr);
}

void render_frame(bool& show_demo_window) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("sonotron");
  ImGui::Text("GUI infrastructure bootstrap - no brain connection yet.");
  ImGui::Checkbox("Show ImGui demo", &show_demo_window);
  ImGui::End();

  if (show_demo_window) {
    ImGui::ShowDemoWindow(&show_demo_window);
  }

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

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  std::fprintf(stdout, "sonotron: window open, GL renderer: %s\n",
               reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

  bool show_demo_window = false;
  const int max_frames = max_frames_from_env();
  int frame = 0;

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    if (max_frames >= 0 && frame >= max_frames) {
      break;
    }
    ++frame;

    glfwPollEvents();
    render_frame(show_demo_window);
    present_frame(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  std::fprintf(stdout, "sonotron: closed cleanly after %d frame(s)\n", frame);
  return 0;
}

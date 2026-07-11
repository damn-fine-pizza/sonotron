# gui-sonotron: HOST-ONLY desktop frontend, a PURE CLIENT (D38).
#
# This target links neither arrangrr_core nor hostrt, and includes zero core
# headers — see docs/design/gui-contract-map.md. It links only the vendored
# toolkit (third_party/imgui, which pulls in third_party/glfw + OpenGL) plus
# this app's own dependency-free src/ (JSON-driven general layout, the
# brain-session seam, the zone models, and the zone panels -- node 11600,
# G0-G3).
#
# Wired only from the non-firmware branch of the top-level CMakeLists.txt —
# never built for arm-none-eabi.

# Pure-data zone models (G3, docs/design/gui-fase2-mechanical-plan.md,
# ux-workstation.md §4): Browser/Grid/SeqEdit state feeding the workstation
# zone panels below. Dependency-free beyond STL -- no ImGui, no GLFW -- so
# they stay fully host-only unit-testable without a GPU or display, same
# split as gui_sonotron_brain. (PartsModel/track_roles.hpp are header-only
# and need no source file here; their declarations reach every consumer
# through the shared include directory.)
add_library(gui_sonotron_models STATIC
  src/browser_model.cpp
  src/grid_model.cpp
  src/seqedit_model.cpp
)
target_include_directories(gui_sonotron_models PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(gui_sonotron_models PRIVATE -Wall -Wextra -Werror)

# Pure-logic "brain" seam (ux-workstation.md §9/§13, node 11600): line
# framing + the flat-JSON decoder (brain_event), the non-blocking UDS
# transport implementing BrainSession (uds_brain_session), and the
# client-side view state reduced from the event stream (app_state).
# Dependency-free beyond POSIX + STL -- no ImGui, no GLFW -- so it stays
# fully host-only unit-testable without a GPU or display, same split as
# gui_sonotron_models above.
add_library(gui_sonotron_brain STATIC
  src/brain_event.cpp
  src/uds_brain_session.cpp
  src/app_state.cpp
)
target_include_directories(gui_sonotron_brain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(gui_sonotron_brain PRIVATE -Wall -Wextra -Werror)

# Layout engine + zone panels. The zone panels (G3) are ImGui-rendering code
# (transport/browser/grid/seqedit/parts/intention), so they live in this
# ImGui-linked library alongside layout_renderer.cpp -- the *_panel/*_model
# split invariant is about which .cpp files include ImGui, not which CMake
# target compiles them (docs/design/gui-fase2-mechanical-plan.md's
# Invariants). Needs gui_sonotron_models for the model types the panels
# render/mutate (BrowserModel/GridModel/SeqEditModel); AppState/BrainSession
# usage stays header-only (inline getters, a pure-virtual interface) so no
# link dependency on gui_sonotron_brain is required.
add_library(gui_sonotron_layout STATIC
  src/layout_model.cpp
  src/layout_json.cpp
  src/layout_renderer.cpp
  src/transport_panel.cpp
  src/browser_panel.cpp
  src/grid_panel.cpp
  src/seqedit_panel.cpp
  src/parts_panel.cpp
  src/intention_panel.cpp
)
target_include_directories(gui_sonotron_layout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gui_sonotron_layout PUBLIC imgui gui_sonotron_models)
target_compile_options(gui_sonotron_layout PRIVATE -Wall -Wextra -Werror)

# Host-only screenshot helper: a thin wrapper over the stb_image_write.h
# already vendored under third_party/glfw/deps (no new dependency). Kept in
# its own target so the not-warning-clean stb implementation can be compiled
# with warnings off (-w) without relaxing -Werror anywhere else, and so the
# layout tests never link it. Only the executable uses it (SONOTRON_GUI_
# SCREENSHOT verification path in main.cpp).
add_library(gui_sonotron_screenshot STATIC src/screenshot.cpp)
target_include_directories(gui_sonotron_screenshot
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/glfw/deps)
target_compile_options(gui_sonotron_screenshot PRIVATE -w)

add_executable(gui_sonotron main.cpp)
set_target_properties(gui_sonotron PROPERTIES OUTPUT_NAME gui-sonotron)
target_link_libraries(gui_sonotron PRIVATE
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_screenshot imgui)
target_compile_options(gui_sonotron PRIVATE -Wall -Wextra -Werror)
# Bakes the source-tree assets/ path into the binary so the vendored TTF
# (see assets/fonts/ARRGRR_VENDOR.md) is found when running straight out of
# the build tree during development. SONOTRON_FONT_PATH (env) overrides this
# at runtime — same escape hatch used for SONOTRON_LAYOUT_PATH. Packaging a
# real install layout is a later milestone, not part of this font/DPI pass.
target_compile_definitions(gui_sonotron PRIVATE
  SONOTRON_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Layout-model/JSON unit tests (host-only; this whole subtree is added only
# in the non-firmware branch of the top-level CMakeLists, where
# enable_testing() is in effect).
add_subdirectory(tests)

# gui-sonotron: HOST-ONLY desktop frontend, a PURE CLIENT (D38).
#
# This target links neither arrangrr_core nor hostrt, and includes zero core
# headers — see docs/design/gui-contract-map.md. It links only the vendored
# toolkit (third_party/imgui, which pulls in third_party/glfw + OpenGL) plus
# this app's own dependency-free src/ (JSON-driven general layout: node
# 11600 layout step). The socket client to the brain and live zone content
# are later milestones — zones are empty, titled frames for now.
#
# Wired only from the non-firmware branch of the top-level CMakeLists.txt —
# never built for arm-none-eabi.

add_library(gui_sonotron_layout STATIC
  src/layout_model.cpp
  src/layout_json.cpp
  src/layout_renderer.cpp
)
target_include_directories(gui_sonotron_layout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gui_sonotron_layout PUBLIC imgui)
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
target_link_libraries(gui_sonotron PRIVATE gui_sonotron_layout gui_sonotron_screenshot imgui)
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

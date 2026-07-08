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

add_executable(gui_sonotron main.cpp)
set_target_properties(gui_sonotron PROPERTIES OUTPUT_NAME gui-sonotron)
target_link_libraries(gui_sonotron PRIVATE gui_sonotron_layout imgui)
target_compile_options(gui_sonotron PRIVATE -Wall -Wextra -Werror)

# Layout-model/JSON unit tests (host-only; this whole subtree is added only
# in the non-firmware branch of the top-level CMakeLists, where
# enable_testing() is in effect).
add_subdirectory(tests)

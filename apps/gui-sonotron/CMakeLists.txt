# gui-sonotron: HOST-ONLY desktop frontend, a PURE CLIENT (D38).
#
# This target links neither arrangrr_core nor hostrt, and includes zero core
# headers — see docs/design/gui-contract-map.md. It links only the vendored
# toolkit (third_party/imgui, which pulls in third_party/glfw + OpenGL).
#
# Bootstrap scope only (node 11600 infrastructure step): open a window and
# render one trivial ImGui frame. The socket client to the brain, the real
# view/panel model, and JSON persistence are later milestones.
#
# Wired only from the non-firmware branch of the top-level CMakeLists.txt —
# never built for arm-none-eabi.

add_executable(gui_sonotron main.cpp)
set_target_properties(gui_sonotron PROPERTIES OUTPUT_NAME gui-sonotron)
target_link_libraries(gui_sonotron PRIVATE imgui)
target_compile_options(gui_sonotron PRIVATE -Wall -Wextra -Werror)

# Dear ImGui, vendored (see ARRGRR_VENDOR.md for the pinned tag/commit).
# This CMakeLists.txt is ours — upstream Dear ImGui ships no build system.
# Host-only: this directory is only ever add_subdirectory()'d from the
# non-firmware branch of the top-level CMakeLists.txt.

add_library(imgui STATIC
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  backends/imgui_impl_glfw.cpp
  backends/imgui_impl_opengl3.cpp
)

# SYSTEM: vendored headers are exempt from our own -Wall/-Wextra/-Werror and
# from clang-tidy (its HeaderFilterRegex already excludes third_party/, this
# just also keeps compiler warnings quiet for consumers).
target_include_directories(imgui SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

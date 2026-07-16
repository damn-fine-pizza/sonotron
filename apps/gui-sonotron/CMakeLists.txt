# gui-sonotron: HOST-ONLY desktop frontend.
#
# Phase 2b (docs/design/sonotron-server-phase2-brief.md) relaxed D38 for this
# milestone (owner-decided): the GUI binary now hosts the engine in-process
# on a dedicated thread (the "integrated" mode, no external `sonotron-server`
# needed), so it DOES link hostrt/runtime/arrangrr — through the
# gui_sonotron_engine library below. A second, narrowly-scoped library,
# gui_sonotron_preview (repeat-zone-real-contract.md "cell preview made
# real" pass), ALSO includes arrangrr/ headers, to resolve real note content
# for the launch-grid/Sequence Edit preview through the SAME NTT kernel the
# engine plays. These two are the ONLY libraries that do. Every other
# library in this directory (gui_sonotron_models/brain/layout/screenshot)
# stays exactly as core-free as it always was — see each target's own
# comment below and docs/design/gui-contract-map.md for the discipline that
# continues to apply at THIS granularity (compilation units and
# target_link_libraries edges) now that the OS-level process boundary that
# used to enforce it is gone. `--control <path>` (a pure client of an
# external `sonotron-server`, unchanged) is still supported side by side with
# the integrated default -- see main.cpp.
#
# Wired only from the non-firmware branch of the top-level CMakeLists.txt —
# never built for arm-none-eabi.

find_package(Threads REQUIRED)

# Pure-data zone models (G3, docs/design/gui-fase2-mechanical-plan.md,
# ux-workstation.md §4): Browser/Grid/SeqEdit state feeding the workstation
# zone panels below. Dependency-free beyond STL -- no ImGui, no GLFW -- so
# they stay fully host-only unit-testable without a GPU or display, same
# split as gui_sonotron_brain. (PartsModel/track_roles.hpp are header-only
# and need no source file here; their declarations reach every consumer
# through the shared include directory.)
# scenes_json.cpp (repeat-zone-real-contract.md §4/§8b decision 3): a
# scenes.json sibling to layout.json persisting GridModel's host-only scene
# names. Sits in this library (not gui_sonotron_layout, alongside
# layout_json.cpp) because it only needs GridModel + STL file I/O, no ImGui
# -- same host-only, GPU-free unit-testability as grid_model.cpp itself.
add_library(gui_sonotron_models STATIC
  src/browser_model.cpp
  src/grid_model.cpp
  src/scenes_json.cpp
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

# Real-content cell preview (repeat-zone-real-contract.md, "cell preview made
# real" pass): a SECOND library allowed to include arrangrr/ headers (Corelli
# §15 review resolution item 1's original grant was ONE library,
# gui_sonotron_engine below; this pass extends the same discipline to a
# second, narrowly-scoped one). preview.cpp resolves a role's StylePattern
# against a canonical placeholder harmony through arrangrr::Arranger::resolve
# (hoisted PUBLIC for exactly this reuse) so the launch-cell thumbnail and
# the Sequence Edit canvas draw the SAME real note data the engine would
# play, never a re-implementation. preview.hpp (the header gui_sonotron_
# layout's panels actually include) names ZERO arrangrr/core types -- the
# same pimpl-by-convention discipline in_process_brain_session.hpp already
# uses -- so linking this library does not reopen the core-free invariant
# gui_sonotron_layout/grid_panel.cpp/seqedit_panel.cpp otherwise hold to.
add_library(gui_sonotron_preview STATIC src/preview.cpp)
target_include_directories(gui_sonotron_preview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gui_sonotron_preview PRIVATE arrangrr)
target_compile_options(gui_sonotron_preview PRIVATE -Wall -Wextra -Werror)

# Layout engine + zone panels + the sonotron theme. The zone panels (G3) are
# ImGui-rendering code (transport/browser/grid/seqedit/parts/intention), so
# they live in this ImGui-linked library alongside layout_renderer.cpp -- the
# *_panel/*_model split invariant is about which .cpp files include ImGui,
# not which CMake target compiles them (docs/design/gui-fase2-mechanical-
# plan.md's Invariants). theme.cpp (src/theme.hpp) is pure ImGuiStyle/ImVec4
# data with no widget calls and no BrainSession sends, so it sits here too
# rather than needing a target of its own. Needs gui_sonotron_models for the
# model types the panels render/mutate (BrowserModel/GridModel/SeqEditModel)
# and for track_roles.hpp (theme.hpp's per-role tint ramp); AppState/
# BrainSession usage stays header-only (inline getters, a pure-virtual
# interface) so no link dependency on gui_sonotron_brain is required.
# gui_sonotron_preview is linked PRIVATE (repeat-zone-real-contract.md,
# "cell preview made real" pass): grid_panel.cpp/seqedit_panel.cpp call its
# preview_for() to draw REAL resolved note content instead of a label-hash,
# but its own public header (preview.hpp) names zero arrangrr/core types, so
# this stays a pure library-boundary dependency, not a core-free violation --
# see preview.hpp's own header comment and gui_sonotron_preview's target
# comment above.
# input_trace.cpp (INPUT RECORD/REPLAY, owner ask: close the "green tests,
# broken app" gap -- see src/input_trace.hpp) sits here rather than being
# compiled straight into the gui_sonotron executable: it only needs ImGuiIO
# + STL, but main.cpp's OWN translation unit inherits -fno-exceptions
# transitively (through gui_sonotron_engine -> hostrt, the core's
# no-exceptions discipline), which would forbid the try/catch its
# hand-rolled JSONL number parser uses. Living in this core-free library
# instead (same discipline as theme.cpp/neon_widgets.cpp above) keeps that
# parser free to use ordinary exception-based std::stod.
add_library(gui_sonotron_layout STATIC
  src/theme.cpp
  src/neon_widgets.cpp
  src/input_trace.cpp
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
target_link_libraries(gui_sonotron_layout PRIVATE gui_sonotron_preview)
target_compile_options(gui_sonotron_layout PRIVATE -Wall -Wextra -Werror)

# Engine-linking half of the GUI binary (Phase 2b, docs/design/
# sonotron-server-phase2-brief.md): D38 is retired for THIS milestone
# (owner-decided, Corelli §15 review resolution item 1) -- this library, and
# gui_sonotron_preview above, are the ONLY two libraries that include
# arrangrr/runtime/hostrt headers directly. This one links hostrt/ALSA
# directly and owns the dedicated engine thread
# (in_process_brain_session.cpp: Runtime + arrangrr Stage + AlsaMidi, driven
# by the Command ring, producing the OutEvent ring) and the in-process
# OutEvent -> BrainEvent decode (brain_event_from_outevent.cpp), which shares
# components/hostrt/event_labels.hpp with jsonl.cpp so the ring's decode and
# the socket's JSONL wire cannot drift into two independently-maintained
# label tables. Depends on gui_sonotron_brain only for the BrainSession
# interface + BrainEvent shape (both stay exactly as core-free as before);
# every OTHER library in this directory is untouched by this addition.
# audio_ring (components/platform/audio, promoted out of this app per docs/
# proposals/components-restructure-move-plan.md §2/§3 Step 5) supplies the
# SPSC ring template + the shared AudioMidiEvent POD this library's producer
# side (in_process_brain_session.cpp) needs, without linking melodd/
# miniaudio.
add_library(gui_sonotron_engine STATIC
  src/brain_event_from_outevent.cpp
  src/in_process_brain_session.cpp
)
target_include_directories(gui_sonotron_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gui_sonotron_engine PUBLIC gui_sonotron_brain audio_ring hostrt
  Threads::Threads)
target_compile_options(gui_sonotron_engine PRIVATE -Wall -Wextra -Werror)

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
  gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine audio
  gui_sonotron_screenshot imgui sonotron_version)
target_compile_options(gui_sonotron PRIVATE -Wall -Wextra -Werror)
# Bakes the source-tree assets/ path into the binary so the vendored TTF
# (see assets/fonts/ARRGRR_VENDOR.md) is found when running straight out of
# the build tree during development. SONOTRON_FONT_PATH (env) overrides this
# at runtime — same escape hatch used for SONOTRON_LAYOUT_PATH. Packaging a
# real install layout is a later milestone, not part of this font/DPI pass.
target_compile_definitions(gui_sonotron PRIVATE
  SONOTRON_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  # Apple's own sanctioned escape hatch for the OpenGL-deprecated-since-10.14
  # -Wdeprecated-declarations error (Apple clang, -Werror): main.cpp calls
  # glPixelStorei/glReadPixels/glViewport/glClearColor/glClear/glGetString
  # directly. APPLE-only; no effect on Linux/Windows.
  $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>)

# Layout-model/JSON unit tests (host-only; this whole subtree is added only
# in the non-firmware branch of the top-level CMakeLists, where
# enable_testing() is in effect).
add_subdirectory(tests)

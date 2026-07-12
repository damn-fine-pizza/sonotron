# components/runtime: the dual-target, freestanding, no-heap kernel
# (Transport + OutScheduler, moved byte-for-byte from components/arrangrr,
# plus the NEW Stage port / Runtime driver). Depends ONLY on components/
# common — NEVER on components/arrangrr (Decision A: that edge is exactly the
# cycle this component exists to break).
#
# Header-only/INTERFACE: Transport/OutScheduler/Stage/Runtime are all
# templates or inline classes (docs/design/orchestrator-pipeline-extraction.md
# §14.5 point 8 — the template/concept Stage shape removes the forcing reason
# for a ceremony .cpp that a virtual-class Stage would have needed).
add_library(runtime INTERFACE)
target_include_directories(runtime INTERFACE include)
target_link_libraries(runtime INTERFACE common)
target_compile_options(runtime INTERFACE
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)

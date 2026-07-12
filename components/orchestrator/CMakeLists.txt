# components/orchestrator: HOST-ONLY. Names and composes the concrete
# multi-stage pipelines (today: Accompany, roadmap 9310) out of the
# dual-target `runtime::Pipeline<StageTs...>` mechanism plus one host-only
# stage (midisrc's MIDI-source Stage adapter) -- it does NOT reimplement
# time or the D29 total order (docs/design/orchestrator-pipeline-
# extraction.md §16.3/§16.6/§16.8).
#
# Header-only/INTERFACE, same shape as components/runtime/chorddet: the
# Accompany pipeline is a plain type alias over templates already defined
# elsewhere, no forcing reason for a ceremony .cpp.
#
# HOST-ONLY: links midisrc (file I/O, std::vector/std::string) -- must NEVER
# be added on the arm-none-eabi (firmware) branch of the top-level
# CMakeLists.txt.
add_library(orchestrator INTERFACE)
target_include_directories(orchestrator INTERFACE include)
target_link_libraries(orchestrator INTERFACE common runtime chorddet arrangrr midisrc)

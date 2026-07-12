# components/midisrc: HOST-ONLY MIDI-source material — the hand-rolled SMF
# reader (`smf.{hpp,cpp}`) + the generic `Diagnostics` batch it reports
# through (co-promoted from apps/tools/arrstyle-converter, Phase 4c,
# docs/design/orchestrator-pipeline-extraction.md §16.5), plus the
# MIDI-source Stage adapter (`midi_source_stage.hpp`) implementing
# `runtime::StageLike`. Links common+runtime only — NEVER arrangrr (D43: a
# pipeline stage does not know its neighbours).
#
# HOST-ONLY: this whole component ports std::vector/std::string/file I/O and
# must NEVER be added on the arm-none-eabi (firmware) branch of the top-level
# CMakeLists.txt.
add_library(midisrc STATIC
  src/diagnostics.cpp
  src/smf.cpp
  src/file_io.cpp
  src/midi_source_stage.cpp
)
target_include_directories(midisrc PUBLIC include)
target_link_libraries(midisrc PUBLIC common runtime)
target_compile_options(midisrc PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)

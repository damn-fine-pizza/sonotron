# components/common: the base, dependency-free, header-only layer both
# components/runtime and components/arrangrr depend on (Decision A, docs/
# design/orchestrator-pipeline-extraction.md §14.1) — Tick/BpmX100/
# TickAccumulator/the 3 bar constants, ARR_ASSERT, and MidiMessage + midi::*.
# Pure data/constant/macro layer, no branches: excluded from the unit-branch
# coverage gate (scripts/coverage.sh), same as tests/ directories.
add_library(common INTERFACE)
target_include_directories(common INTERFACE include)
target_compile_options(common INTERFACE
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)

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
  src/smf_write.cpp
  src/file_io.cpp
  src/midi_source_stage.cpp
)
target_include_directories(midisrc PUBLIC include)
target_link_libraries(midisrc PUBLIC common runtime)
target_compile_options(midisrc PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)

# Phase-5 Item G (docs/strategy/phase5-proposals.md #4): a libFuzzer harness
# around parse_smf, the untrusted-parser entry point `midi-source load`
# reaches from an arbitrary user file. OFF by default -- this option must
# never appear on the default host/arm build surface or in ci.sh; it exists
# purely for a developer/CI job that opts in explicitly. Pre-decided
# dependency choice: clang's BUILT-IN libFuzzer only (no AFL++, no new
# dependency). Compiled from source directly (not linked against the
# `midisrc` static library) so the sanitizer/coverage instrumentation covers
# the parser itself, not just the harness.
option(SONOTRON_FUZZ "Build the midisrc SMF-parser libFuzzer harness (clang, host-only, dev/CI opt-in)" OFF)

if(SONOTRON_FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
      "SONOTRON_FUZZ requires clang++ (libFuzzer is a clang built-in); "
      "reconfigure with -DCMAKE_CXX_COMPILER=clang++")
  endif()

  add_executable(midisrc_fuzz_smf
    fuzz/fuzz_smf.cpp
    src/smf.cpp
    src/diagnostics.cpp
  )
  target_include_directories(midisrc_fuzz_smf PRIVATE include)
  target_compile_options(midisrc_fuzz_smf PRIVATE
    -Wall -Wextra -Werror
    -fsanitize=fuzzer,address,undefined
    -g -O1
  )
  target_link_options(midisrc_fuzz_smf PRIVATE -fsanitize=fuzzer,address,undefined)

  # A bounded smoke run over the seed corpus, wired into ctest ONLY when
  # SONOTRON_FUZZ is explicitly requested -- a normal `ctest --preset host`
  # never touches this. libFuzzer treats its (single) corpus-directory
  # argument as BOTH read AND write target: it drops every newly-interesting
  # mutation straight back into that directory. Pointing it at the tracked
  # fuzz/seeds/ directly would silently pollute the curated seed corpus with
  # fuzzer-generated garbage on every test run (caught during manual
  # verification -- do not repeat that mistake). So: copy the curated seeds
  # into a disposable scratch directory under the build tree at configure
  # time, and run the smoke test against THAT copy; fuzz/seeds/ itself is
  # never opened for writing by ctest.
  set(midisrc_fuzz_corpus_scratch "${CMAKE_CURRENT_BINARY_DIR}/fuzz_smf_corpus_scratch")
  file(MAKE_DIRECTORY "${midisrc_fuzz_corpus_scratch}")
  file(GLOB midisrc_fuzz_seed_files "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds/*")
  file(COPY ${midisrc_fuzz_seed_files} DESTINATION "${midisrc_fuzz_corpus_scratch}")

  add_test(NAME midisrc_fuzz_smf_smoke
           COMMAND midisrc_fuzz_smf -max_len=4096 -runs=20000
                   "${midisrc_fuzz_corpus_scratch}")
  set_tests_properties(midisrc_fuzz_smf_smoke PROPERTIES LABELS fuzz)
endif()

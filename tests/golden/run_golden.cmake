# Runs one golden test: CLI --script SCRIPT, canonical JSONL diffed vs GOLDEN.
# WORKDIR is optional (unset for every pre-Accompany golden, which does no
# file I/O of its own): the Accompany category's `midi-source load <path>`
# verb resolves its path relative to the process CWD, so its golden pins
# WORKDIR to the repo root so the relative fixture path in the .acmd is
# independent of wherever ctest itself was invoked from.
if(WORKDIR)
  execute_process(
    COMMAND ${CLI} --script ${SCRIPT}
    OUTPUT_FILE ${OUT}
    WORKING_DIRECTORY ${WORKDIR}
    RESULT_VARIABLE rc
  )
else()
  execute_process(
    COMMAND ${CLI} --script ${SCRIPT}
    OUTPUT_FILE ${OUT}
    RESULT_VARIABLE rc
  )
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "script failed (rc=${rc}): ${SCRIPT}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT} ${GOLDEN}
  RESULT_VARIABLE diff
)
if(NOT diff EQUAL 0)
  execute_process(COMMAND diff -u ${GOLDEN} ${OUT} OUTPUT_VARIABLE delta)
  message(FATAL_ERROR "golden mismatch for ${SCRIPT}:\n${delta}")
endif()

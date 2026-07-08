# Runs one golden test: CLI --script SCRIPT, canonical JSONL diffed vs GOLDEN.
execute_process(
  COMMAND ${CLI} --script ${SCRIPT}
  OUTPUT_FILE ${OUT}
  RESULT_VARIABLE rc
)
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

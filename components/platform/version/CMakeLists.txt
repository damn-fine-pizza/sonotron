# components/platform/version: HOST-ONLY release VERSION + build-number
# generation. MAJOR.MINOR comes from the repo-root VERSION file (the single
# source of truth, bumped only via scripts/bump-version.sh); the BUILD
# number is the total commit count reachable from HEAD (`git rev-list
# --count HEAD` -- deterministic, monotonic, identical locally and in CI),
# plus the short commit SHA. All four are configure_file()'d into a
# generated version.hpp under the build tree and exposed through the
# `sonotron_version` INTERFACE target so apps can
# `#include "version/version.hpp"` without vendoring the generated header
# into the source tree.
#
# HOST-ONLY: never added on the arm-none-eabi (firmware) branch of the
# top-level CMakeLists.txt -- see the ARRANGRR_FIRMWARE guard there. Never
# link this target from components/core/**.

set(SONOTRON_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
file(STRINGS "${SONOTRON_VERSION_FILE}" SONOTRON_VERSION_MAJOR_MINOR LIMIT_COUNT 1)
string(STRIP "${SONOTRON_VERSION_MAJOR_MINOR}" SONOTRON_VERSION_MAJOR_MINOR)

# Fallback when git is unavailable (a source tarball with no .git
# directory/worktree file) -- the owner-decided caveat, this is the whole
# fallback, no partial-degradation path.
set(SONOTRON_BUILD_NUMBER 0)
set(SONOTRON_GIT_SHA "unknown")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SONOTRON_BUILD_NUMBER_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE SONOTRON_GIT_COUNT_RESULT
    ERROR_QUIET)
  if(SONOTRON_GIT_COUNT_RESULT EQUAL 0 AND NOT SONOTRON_BUILD_NUMBER_OUT STREQUAL "")
    set(SONOTRON_BUILD_NUMBER "${SONOTRON_BUILD_NUMBER_OUT}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SONOTRON_GIT_SHA_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE SONOTRON_GIT_SHA_RESULT
    ERROR_QUIET)
  if(SONOTRON_GIT_SHA_RESULT EQUAL 0 AND NOT SONOTRON_GIT_SHA_OUT STREQUAL "")
    set(SONOTRON_GIT_SHA "${SONOTRON_GIT_SHA_OUT}")
  endif()
endif()

set(SONOTRON_VERSION_STRING "${SONOTRON_VERSION_MAJOR_MINOR}.${SONOTRON_BUILD_NUMBER}")
set(SONOTRON_VERSION_FULL "${SONOTRON_VERSION_STRING}+${SONOTRON_GIT_SHA}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/version/version.hpp"
  @ONLY
)

add_library(sonotron_version INTERFACE)
target_include_directories(sonotron_version INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Re-run configure whenever VERSION is bumped, so a stale MAJOR.MINOR can
# never survive a plain `cmake --build` (a fresh configure recomputes the
# BUILD number/SHA regardless, but this keeps MAJOR.MINOR edits picked up
# without the caller having to remember to reconfigure by hand too).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONOTRON_VERSION_FILE}")

#!/usr/bin/env bash
# Unit-test coverage: instrumented host build, full test suite (unit +
# golden — golden runs exercise the CLI binary, so host code is measured
# too), report via gcovr. Test code itself is excluded from the metric, as
# are ARR_ASSERT trap branches (never taken by design).
#
# PROJECT GATE: every milestone must keep lines, functions AND branches
# at >= 80% before merging to main.
set -euo pipefail
cd "$(dirname "$0")/.."

GCOVR=(gcovr)
if ! command -v gcovr >/dev/null 2>&1; then
  if command -v uvx >/dev/null 2>&1; then
    GCOVR=(uvx gcovr)
  else
    echo "gcovr not found: install it (dnf install gcovr) or install uv" >&2
    exit 2
  fi
fi

# Full clean: stale .gcno/.gcda from previous source layouts confuse gcovr's
# merge; a coverage run is about correctness, not speed.
rm -rf build/coverage
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage

mkdir -p build/coverage/report
"${GCOVR[@]}" \
  --root . \
  --filter 'app/core/' \
  --filter 'app/platform/' \
  --exclude 'app/core/tests/' \
  --exclude 'app/tests/' \
  --object-directory build/coverage \
  --exclude-branches-by-pattern '.*ARR_ASSERT.*' \
  --exclude-throw-branches \
  --print-summary \
  --sort uncovered-percent \
  --txt build/coverage/report/coverage.txt \
  --html-details build/coverage/report/coverage.html

echo
echo "text report:  build/coverage/report/coverage.txt"
echo "html report:  build/coverage/report/coverage.html"

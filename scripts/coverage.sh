#!/usr/bin/env bash
# Unit-test coverage: instrumented host build, full test suite (unit +
# golden — golden runs exercise the CLI binary, so host code is measured
# too), report via gcovr. Test code itself is excluded from the metric.
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

cmake --preset coverage
cmake --build --preset coverage
# Reset counters from previous runs, then execute the suite.
find build/coverage -name '*.gcda' -delete
ctest --preset coverage

mkdir -p build/coverage/report
"${GCOVR[@]}" \
  --root . \
  --filter 'app/core/' \
  --filter 'app/platform/' \
  --exclude 'app/core/tests/' \
  --exclude 'app/tests/' \
  --object-directory build/coverage \
  --print-summary \
  --sort uncovered-percent \
  --txt build/coverage/report/coverage.txt \
  --html-details build/coverage/report/coverage.html

echo
echo "text report:  build/coverage/report/coverage.txt"
echo "html report:  build/coverage/report/coverage.html"

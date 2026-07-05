#!/usr/bin/env bash
# Unit-test coverage: instrumented host build, full test suite (unit +
# golden — golden runs exercise the CLI binary, so host code is measured
# too), report via gcovr. Test code itself is excluded from the metric, as
# are ARR_ASSERT trap branches (never taken by design).
#
# PROJECT GATE (scoped, 2026-07-05): the >= 80% lines/functions/branches gate
# is ENFORCED on the core (`app/core/`) — the STM32-critical, freestanding,
# deterministically-testable code. The host layer (`app/platform/`) is
# REPORTED for visibility but NOT branch-gated: its terminal-rendering,
# resize, ALSA-failure and socket OS-error (EAGAIN/EPIPE/bind) branches are
# not reachable from a deterministic unit test without fault injection — the
# same rationale that already excludes ARR_ASSERT trap branches. Host lines
# and functions still matter and are watched via the full report below.
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

# ENFORCED GATE — core only. Non-zero exit if the core drops below 80% on any
# of lines/functions/branches. Host (app/platform/) is intentionally not part
# of this gate (see header); it is covered by the full report above.
echo
echo "=== ENFORCED GATE: core (app/core/) >= 80% lines/functions/branches ==="
"${GCOVR[@]}" \
  --root . \
  --filter 'app/core/' \
  --exclude 'app/core/tests/' \
  --object-directory build/coverage \
  --exclude-branches-by-pattern '.*ARR_ASSERT.*' \
  --exclude-throw-branches \
  --fail-under-line 80 \
  --fail-under-function 80 \
  --fail-under-branch 80 \
  --print-summary

#!/usr/bin/env bash
# Three-metric coverage (project memory `coverage-metrics`). One instrumented
# host build, then per-CTest-label subsets measured separately:
#
#   metric 1  unit        production code hit by UNIT tests (`-L unit`).
#                         ENFORCED GATE: core (app/core/) >= 80% on
#                         lines/functions/branches. This is THE gate.
#   metric 2  functional  production code hit by FUNCTIONAL / interaction /
#                         engine-level / golden / integration tests
#                         (`-L functional`). REPORT-ONLY for now.
#   metric 3  regression  the anti-regression tests written for FIXED bugs
#                         (`-L regression`). Measured as a CENSUS, not a line %:
#                         N tests, all-green yes/no, meant to be monotonically
#                         non-decreasing. REPORT-ONLY / informational.
#
# WHY per-category isolation works with our STATIC-lib layout: every test binary
# links the SAME static `arrangrr_core`, so all runs write to the SAME shared
# .gcda files under build/coverage/app/core/CMakeFiles/arrangrr_core.dir/ and
# gcov ACCUMULATES counts across binaries. Wiping every .gcda before each
# category's `ctest -L <cat>` therefore cleanly isolates that category's
# coverage — the accumulation we normally fight is exactly what we exploit here.
#
# The host layer (app/platform/) is REPORTED but NOT branch-gated: its terminal/
# resize/ALSA-failure/socket OS-error branches are not deterministically
# reachable without fault injection (same rationale that excludes ARR_ASSERT
# trap branches). Coverage/instrumentation is HOST-ONLY and never touches the
# arm cross-build.
set -euo pipefail
cd "$(dirname "$0")/.."

# gcovr is PINNED to 8.6 (owner decision under the host-dep policy). Rationale:
# gcovr's 8.x line/branch accounting has shifted across point-releases and the
# branch gate runs only ~18 branches above the 80% floor, so an unpinned gcovr
# could move the gate silently on an upstream release. `uvx gcovr==8.6` fixes
# the version deterministically with no local install. Coverage/instrumentation
# is HOST-ONLY and never touches the arm cross-build.
GCOVR=(uvx gcovr==8.6)
if ! command -v uvx >/dev/null 2>&1; then
  echo "uvx not found: install uv (coverage pins gcovr==8.6 via uvx)" >&2
  exit 2
fi

# Full clean: stale .gcno/.gcda from previous source layouts confuse gcovr's
# merge; a coverage run is about correctness, not speed.
rm -rf build/coverage
cmake --preset coverage
cmake --build --preset coverage

REPORT_DIR=build/coverage/report
mkdir -p "${REPORT_DIR}"

# Common gcovr scope for the CORE gate metric.
CORE_ARGS=(
  --root .
  --filter 'app/core/'
  --exclude 'app/core/tests/'
  --exclude 'app/tests/'
  --object-directory build/coverage
  --exclude-branches-by-pattern '.*ARR_ASSERT.*'
  --exclude-throw-branches
)

reset_gcda() {
  find build/coverage -name '*.gcda' -delete 2>/dev/null || true
}

# Run one category's tests; tolerate non-zero (a red test must not abort the
# summary — we report it). Echoes the ctest exit code on stdout.
run_category() {
  local cat="$1"
  set +e
  ctest --preset coverage -L "${cat}" >"${REPORT_DIR}/ctest_${cat}.log" 2>&1
  local rc=$?
  set -e
  echo "${rc}"
}

# Extract "<metric>: NN.N%" from a gcovr --print-summary capture.
pct() { # <summary-file> <lines|functions|branches>
  grep -iE "^${2}:" "$1" | grep -oE '[0-9]+\.[0-9]+%' | head -1
}

echo "=== instrumented build ready; measuring per CTest label ==="

# --- metric 1: unit -------------------------------------------------------
reset_gcda
UNIT_RC=$(run_category unit)
"${GCOVR[@]}" "${CORE_ARGS[@]}" \
  --print-summary --sort uncovered-percent \
  --txt "${REPORT_DIR}/coverage.txt" \
  --html-details "${REPORT_DIR}/coverage.html" \
  >"${REPORT_DIR}/summary_unit.txt" 2>&1 || true
# Also give the host layer visibility (report-only, not gated).
"${GCOVR[@]}" \
  --root . --filter 'app/core/' --filter 'app/platform/' \
  --exclude 'app/core/tests/' --exclude 'app/tests/' \
  --object-directory build/coverage \
  --exclude-branches-by-pattern '.*ARR_ASSERT.*' --exclude-throw-branches \
  --html-details "${REPORT_DIR}/coverage_full.html" >/dev/null 2>&1 || true

# --- metric 2: functional -------------------------------------------------
reset_gcda
FUNC_RC=$(run_category functional)
"${GCOVR[@]}" "${CORE_ARGS[@]}" --print-summary \
  >"${REPORT_DIR}/summary_functional.txt" 2>&1 || true

# --- metric 3: regression (census, not a %) -------------------------------
reset_gcda
# Test count for the label from ctest's own authoritative "Total Tests:" line.
REG_N=$(ctest --preset coverage -L regression -N 2>/dev/null \
  | sed -n 's/^Total Tests: *\([0-9][0-9]*\).*/\1/p' | tail -1)
REG_N=${REG_N:-0}
REG_RC=$(run_category regression)
if [ "${REG_RC}" -eq 0 ]; then REG_GREEN="yes"; else REG_GREEN="NO"; fi

# Soft monotonic-non-decreasing check against an OPTIONAL owner-committed
# baseline. This script does NOT create the baseline (owner opt-in): if
# scripts/coverage-regression-baseline.txt exists and the census dropped, warn.
REG_MONO="(no baseline; report-only)"
BASELINE=scripts/coverage-regression-baseline.txt
if [ -f "${BASELINE}" ]; then
  PREV=$(tr -dc '0-9' <"${BASELINE}")
  if [ "${REG_N}" -lt "${PREV:-0}" ]; then
    REG_MONO="REGRESSED (${PREV} -> ${REG_N})"
  else
    REG_MONO="ok (baseline ${PREV:-0} -> ${REG_N})"
  fi
fi

# --- 3-number summary -----------------------------------------------------
echo
echo "============================================================"
echo " THREE-METRIC COVERAGE SUMMARY (core scope: app/core/)"
echo "------------------------------------------------------------"
printf ' metric 1  unit        lines %-7s functions %-7s branches %-7s\n' \
  "$(pct "${REPORT_DIR}/summary_unit.txt" lines)" \
  "$(pct "${REPORT_DIR}/summary_unit.txt" functions)" \
  "$(pct "${REPORT_DIR}/summary_unit.txt" branches)"
printf ' metric 2  functional  lines %-7s functions %-7s branches %-7s\n' \
  "$(pct "${REPORT_DIR}/summary_functional.txt" lines)" \
  "$(pct "${REPORT_DIR}/summary_functional.txt" functions)" \
  "$(pct "${REPORT_DIR}/summary_functional.txt" branches)"
printf ' metric 3  regression  census: %s tests, all-green: %s, monotonic: %s\n' \
  "${REG_N}" "${REG_GREEN}" "${REG_MONO}"
echo "------------------------------------------------------------"
echo " metric 1 = ENFORCED gate | metrics 2 & 3 = report-only"
echo " text report: ${REPORT_DIR}/coverage.txt"
echo " html report: ${REPORT_DIR}/coverage.html (unit) / coverage_full.html"
echo "============================================================"
echo

# Fail loudly if any category's suite was red (green suites are a precondition
# for trusting the numbers). Metrics 2/3 are report-only, but a RED test is a
# hard failure regardless of category.
if [ "${UNIT_RC}" -ne 0 ] || [ "${FUNC_RC}" -ne 0 ] || [ "${REG_RC}" -ne 0 ]; then
  echo "FAIL: a labelled test suite was red (unit=${UNIT_RC} functional=${FUNC_RC} regression=${REG_RC}); see ${REPORT_DIR}/ctest_*.log" >&2
  exit 1
fi

# --- ENFORCED GATE: metric 1 (unit), core only ----------------------------
# Re-measure unit coverage in isolation and fail under 80% on any of
# lines/functions/branches. This is the ONLY metric that fails the build.
echo "=== ENFORCED GATE: metric 1 (unit) core (app/core/) >= 80% l/f/b ==="
reset_gcda
UNIT_RC2=$(run_category unit)
if [ "${UNIT_RC2}" -ne 0 ]; then
  echo "FAIL: unit suite red on the gate re-run; see ${REPORT_DIR}/ctest_unit.log" >&2
  exit 1
fi
"${GCOVR[@]}" "${CORE_ARGS[@]}" \
  --fail-under-line 80 \
  --fail-under-function 80 \
  --fail-under-branch 80 \
  --print-summary

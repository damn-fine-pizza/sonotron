#!/usr/bin/env bash
# Style enforcement (memory: cpp-style): clang-tidy naming + braces checks
# over all first-party C++ sources. Uses a clang compile database — the GCC
# one carries -fmodules-ts flags clang-tidy cannot parse.
# Usage: scripts/lint.sh [--fix]
set -euo pipefail
cd "$(dirname "$0")/.."

FIX=""
[ "${1:-}" = "--fix" ] && FIX="-fix"

cmake --preset tidy > /dev/null

# -co --exclude-standard: also lint NEW (not yet tracked) sources, or a fresh
# module could land unlinted (review finding, H1).
mapfile -t sources < <(git ls-files -co --exclude-standard 'apps/**/*.cpp' 'components/**/*.cpp')
run-clang-tidy -quiet -p build/tidy $FIX "${sources[@]}" 2>/dev/null | grep -v '^$' || true

# run-clang-tidy's exit code is unreliable across versions; re-check strictly.
fails=0
for f in "${sources[@]}"; do
  if ! clang-tidy -quiet -p build/tidy "$f" > /dev/null 2>&1; then
    echo "LINT FAIL: $f"
    fails=1
  fi
done
if [ "$fails" -ne 0 ]; then
  echo "lint: violations found (run scripts/lint.sh --fix)"
  exit 1
fi
echo "lint OK"

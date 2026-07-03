#!/usr/bin/env bash
# Local CI: both targets must be green on every commit (D3).
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== host: configure + build + test =="
cmake --preset host
cmake --build --preset host
ctest --preset host

echo "== arm: configure + cross-build (freestanding link gate) =="
cmake --preset arm
cmake --build --preset arm

echo "CI OK: host tests green, arm cross-build green"

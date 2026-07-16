#!/usr/bin/env bash
#
# scripts/gui-sonotron/launch-gui.sh — launch the gui-sonotron UI (host build).
#
# Usage:
#   launch-gui.sh [-- <args passed to gui-sonotron>]
#
# Builds the host target first if the binary is missing, then execs it. Any
# arguments after `--` (or all arguments) are forwarded to the app, e.g.:
#   launch-gui.sh --control /tmp/sonotron.sock
#   launch-gui.sh -- --trace-input /tmp/session.jsonl     # record a repro
#   launch-gui.sh -- --replay-input /tmp/session.jsonl     # replay it back
#
# A launch-gui.bat / .cmd sibling can be added the day we ship on Windows; for
# now this POSIX shell launcher covers the Linux and macOS hosts.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Prefer a release build if present, else the debug host build.
BIN=""
for candidate in \
  "$REPO_ROOT/build/host-release/apps/gui-sonotron/gui-sonotron" \
  "$REPO_ROOT/build/host/apps/gui-sonotron/gui-sonotron"; do
  if [ -x "$candidate" ]; then BIN="$candidate"; break; fi
done

if [ -z "$BIN" ]; then
  printf '\033[36m[launch]\033[0m gui-sonotron not built yet — building host target …\n'
  "$REPO_ROOT/scripts/gui-sonotron/build.sh" host
  BIN="$REPO_ROOT/build/host/apps/gui-sonotron/gui-sonotron"
fi

# Drop a leading `--` separator if the caller used one.
[ "${1:-}" = "--" ] && shift

printf '\033[36m[launch]\033[0m %s %s\n' "$BIN" "$*"
exec "$BIN" "$@"

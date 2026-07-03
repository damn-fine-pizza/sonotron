#!/usr/bin/env bash
# Pane-console integration: run the CLI under a pseudo-tty via script(1) so
# the TUI path engages headlessly. Types through the line editor (including
# arrows and history), checks pane markers, and verifies clean teardown.
set -u
CLI="$1"
command -v script >/dev/null 2>&1 || exit 77

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# \r submits (raw mode); arrows exercise the editor's escape parsing.
# Sequence: play D · recall it with Up and resubmit · help · quit.
printf 'play D\r\x1b[A\r\x1b[Bhelp\rquit\r' \
  | timeout 15 script -qec "$CLI --events human" "$tmp/typescript" > "$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 0 ] || { echo "CLI exited rc=$rc"; tail -5 "$tmp/out"; exit 1; }

grep -q $'\x1b\[7m' "$tmp/out" || { echo "status bar missing"; exit 1; }
count=$(grep -c '> play D' "$tmp/out" || true)
[ "$count" -ge 2 ] || { echo "history recall missing (got $count)"; exit 1; }
grep -q 'commands (help' "$tmp/out" || { echo "help output missing"; exit 1; }
grep -q 'stopped' "$tmp/out" || { echo "status text missing"; exit 1; }
pgrep -x arrangrr >/dev/null && { echo "leftover process"; exit 1; }
exit 0

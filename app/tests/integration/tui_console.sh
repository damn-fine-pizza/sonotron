#!/usr/bin/env bash
# Pane-console integration: run the CLI under a pseudo-tty via script(1) so
# the TUI path engages headlessly. Types through the line editor (including
# arrows and history), checks pane markers, and verifies clean teardown.
set -u
CLI="$1"
command -v script >/dev/null 2>&1 || exit 77

# A foreign arrangrr session (e.g. the user jamming) must never be touched:
# ALSA name-based resolution would hit it. Skip instead of interfering.
if pgrep -x arrangrr >/dev/null 2>&1; then
  echo "an arrangrr session is already running - skipping to leave it alone"
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Focus now starts on the piano panel (play mode), so SHIFT+TAB (CSI Z) steps
# back to the REPL before we type commands. We clear the crowded default grid
# and keep only the console (for the command echo) + help (so 'help' has the
# full height for its content, which the default 6-panel grid clips on 80x24).
# \r submits (raw mode); arrows exercise the editor's escape parsing.
# Sequence: SHIFT+TAB · clear · open console · play D · recall with Up · help · quit.
printf '\x1b[Zpanel close all\rpanel open console\rplay D\r\x1b[A\r\x1b[Bhelp\rquit\r' \
  | timeout 15 script -qec "$CLI --events human" "$tmp/typescript" > "$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 0 ] || { echo "CLI exited rc=$rc"; tail -5 "$tmp/out"; exit 1; }

grep -q $'\x1b\[7m' "$tmp/out" || { echo "status bar missing"; exit 1; }
# The uniform panel grid repaints with cursor positioning (few newlines), so the
# console echo of the typed + recalled command shows as repeated OCCURRENCES on
# the same physical line rather than distinct grep lines: count occurrences.
count=$(grep -o '> play D' "$tmp/out" | wc -l)
[ "$count" -ge 2 ] || { echo "history recall missing (got $count)"; exit 1; }
grep -q 'chord progressions' "$tmp/out" || { echo "help output missing"; exit 1; }
grep -q 'stopped' "$tmp/out" || { echo "status text missing"; exit 1; }
pgrep -x arrangrr >/dev/null && { echo "leftover process"; exit 1; }
exit 0

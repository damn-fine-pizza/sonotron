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
#
# What this test PROVES (and what it deliberately does NOT scrape):
# The uniform panel grid repaints by cursor positioning, so a typed command's
# console echo is placed cell-by-cell and never lands as a contiguous string —
# any occurrence count of it is a layout accident that silently shifts whenever a
# panel's line count changes (which is exactly what broke the old `> play D`
# count). And in this headless `script` PTY the status bar is only seeded once at
# startup (it refreshes on transport ticks, which do not fire here), so mid-session
# engine state is not scrapeable either. So we do NOT assert on scraped state.
# Instead we prove the END-TO-END CONSOLE WIRING survives a realistic session:
# after SHIFT+TAB, several submitted commands, and Up/Up/Enter/Down arrow escapes,
# the FINAL typed command `help` still parses and EXECUTES — its menu content
# renders (`chord progressions`). A corrupted arrow-escape parse or a dropped
# Enter would poison the buffer and garble that trailing command, so this single
# witness covers editor escape-parsing + line submission + command execution +
# panel render, plus a clean teardown. History-recall CORRECTNESS itself is owned
# deterministically by test_host's test_line_editor (up/down recall unit test);
# here the arrows only need to not break the session.
# Sequence: SHIFT+TAB · clear · open console · bpm 140 · bpm 90 · Up·Up·Enter
#           (recall an earlier line) · Down · help · quit.
printf '\x1b[Zpanel close all\rpanel open console\rbpm 140\rbpm 90\r\x1b[A\x1b[A\r\x1b[Bhelp\rquit\r' \
  | timeout 15 script -qec "$CLI --events human" "$tmp/typescript" > "$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 0 ] || { echo "CLI exited rc=$rc"; tail -5 "$tmp/out"; exit 1; }

grep -q $'\x1b\[7m' "$tmp/out" || { echo "status bar missing"; exit 1; }
# The trailing `help`, typed after every other command + the arrow escapes, must
# still execute and render its menu content: the layout-independent proof that
# the whole console path (editor escape-parsing -> submit -> exec -> render)
# stayed intact across the session.
grep -q 'chord progressions' "$tmp/out" || { echo "help output missing (console path broke)"; exit 1; }
grep -q 'stopped' "$tmp/out" || { echo "status text missing"; exit 1; }
pgrep -x arrangrr >/dev/null && { echo "leftover process"; exit 1; }
exit 0

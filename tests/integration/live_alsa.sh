#!/usr/bin/env bash
# Live integration: real ALSA sequencer round-trip through the arrangrr
# binary (aseqsend -> in0 -> thru -> out0 -> aseqdump), plus CLI argument
# and script-stdin paths. Skips (exit 77) when the ALSA seq device or the
# alsa-utils tools are unavailable (e.g. headless CI).
set -u
CLI="$1"
APP=""
DUMP=""

command -v aseqsend >/dev/null 2>&1 || exit 77
command -v aseqdump >/dev/null 2>&1 || exit 77
aseqdump -l >/dev/null 2>&1 || exit 77

# A foreign arrangrr session (e.g. the user jamming) must never be touched:
# ALSA name-based resolution would hit it. Skip instead of interfering.
if pgrep -x cli-arrangrr >/dev/null 2>&1; then
  echo "an arrangrr session is already running - skipping to leave it alone"
  exit 77
fi

tmp=$(mktemp -d)
cleanup() {
  [ -n "$APP" ] && kill "$APP" 2>/dev/null
  [ -n "$DUMP" ] && kill "$DUMP" 2>/dev/null
  rm -rf "$tmp"
}
trap cleanup EXIT

# --- argument-parsing branches -------------------------------------------
"$CLI" --help >/dev/null || { echo "--help failed"; exit 1; }
"$CLI" --bogus 2>/dev/null && { echo "--bogus accepted"; exit 1; }
"$CLI" --script /nonexistent 2>/dev/null && { echo "bad script accepted"; exit 1; }
"$CLI" --script 2>/dev/null && { echo "dangling --script accepted"; exit 1; }
"$CLI" --events 2>/dev/null && { echo "dangling --events accepted"; exit 1; }
printf 'bogus_command\n' | "$CLI" --script - 2>/dev/null && { echo "bad script line accepted"; exit 1; }

# --- live mode exits cleanly on stdin EOF ---------------------------------
timeout 5 "$CLI" --events jsonl </dev/null >/dev/null 2>&1 || { echo "EOF exit failed"; exit 1; }

# --- script mode from stdin, human events --------------------------------
printf 'port open out s\nclock out s\ntransport start\nadvance 40\ntransport stop\nquit\n' \
  | "$CLI" --script - --events human > "$tmp/human.log" || { echo "script - failed"; exit 1; }
grep -q "transport playing" "$tmp/human.log" || { echo "human output missing"; exit 1; }

# --- live ALSA round-trip -------------------------------------------------
mkfifo "$tmp/repl"
exec 9<>"$tmp/repl"
"$CLI" --events jsonl < "$tmp/repl" > "$tmp/live.log" 2>&1 &
APP=$!
sleep 0.6

aseqdump -l | grep -q arrangrr || { echo "arrangrr ALSA client not found"; exit 1; }

aseqdump -p "arrangrr:1" > "$tmp/dump.log" 2>&1 &
DUMP=$!
sleep 0.4
aseqsend -p "arrangrr:0" 90 3C 64 80 3C 40 || { echo "aseqsend failed"; exit 1; }
sleep 0.6

echo "quit" >&9
wait "$APP" 2>/dev/null
APP=""

grep -q "Note on .*note 60" "$tmp/dump.log" || { echo "note-on missing:"; cat "$tmp/dump.log"; exit 1; }
grep -q "Note off .*note 60" "$tmp/dump.log" || { echo "note-off missing:"; cat "$tmp/dump.log"; exit 1; }
grep -q '"msg":"noteon"' "$tmp/live.log" || { echo "monitor jsonl missing"; exit 1; }
exit 0

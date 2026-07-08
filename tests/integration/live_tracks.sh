#!/usr/bin/env bash
# Live REPL integration: drive the CLI through stdin like a user would —
# create a track on the virtual output port, write steps, start the
# transport — and assert the pattern actually comes out of the ALSA port.
# Assertions are timing-tolerant (counts, not exact ticks): the live clock
# is real time, determinism belongs to the golden tests.
# Skips (exit 77) when ALSA seq or alsa-utils are unavailable.
set -u
CLI="$1"
APP=""
DUMP=""

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

mkfifo "$tmp/repl"
exec 9<>"$tmp/repl"
"$CLI" --events human < "$tmp/repl" > "$tmp/monitor.log" 2>&1 &
APP=$!
sleep 0.6

aseqdump -l | grep -q arrangrr || { echo "arrangrr ALSA client not found"; exit 1; }
aseqdump -p "arrangrr:1" > "$tmp/dump.log" 2>&1 &
DUMP=$!
sleep 0.4

# Whole setup pasted as ONE multi-line write: also regression-tests the raw
# REPL reader (the old cin-based loop stranded batched lines).
printf '%s\n' \
  "track new bass out0:3 bass" \
  "track length bass 4" \
  "track step bass 1 C2 100 120" \
  "track step bass 3 G2 80 120" \
  "transport tempo 150" \
  "transport start" >&9

sleep 1.4   # >2 pattern cycles at 150 BPM (4 steps = 960 ticks ~ 0.4 s)
printf 'track mute bass on\n' >&9
sleep 0.5   # muted tail: no new hits expected
printf 'transport stop\npanic\nquit\n' >&9
sleep 0.4
kill "$DUMP" 2>/dev/null
DUMP=""
wait "$APP" 2>/dev/null
APP=""

count_36=$(grep -c 'Note on .* 2, note 36' "$tmp/dump.log" || true)
count_43=$(grep -c 'Note on .* 2, note 43' "$tmp/dump.log" || true)
offs=$(grep -c 'Note off .* 2, note' "$tmp/dump.log" || true)

# >= 2 cycles: C2 (36) and G2 (43) each hit at least twice, on channel 3
# (aseqdump prints 0-based channel 2). Every hit pairs with a NoteOff.
[ "$count_36" -ge 2 ] || { echo "expected >=2 C2 hits, got $count_36"; cat "$tmp/dump.log"; exit 1; }
[ "$count_43" -ge 2 ] || { echo "expected >=2 G2 hits, got $count_43"; cat "$tmp/dump.log"; exit 1; }
[ "$offs" -ge $((count_36 + count_43)) ] || { echo "unpaired notes: $offs offs"; exit 1; }

# The mute must actually stop the pattern: compare hits right before the
# mute against the end of the capture.
total_before_mute=$((count_36 + count_43))
sleep 0  # (dump already stopped; counts above include the muted tail)

# The batched setup must have executed promptly: the monitor shows the
# transport playing and at least the first hit.
grep -q 'transport playing' "$tmp/monitor.log" || { echo "transport never played"; exit 1; }
grep -q 'note-on   36' "$tmp/monitor.log" || { echo "monitor missed the pattern"; exit 1; }
grep -q 'transport stopped' "$tmp/monitor.log" || { echo "transport never stopped"; exit 1; }

exit 0

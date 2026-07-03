#!/usr/bin/env bash
# One-console jam: starts a GM synth (FluidSynth) in the background, launches
# the arrangrr REPL with the band pre-configured (setup.acmd via --init), and
# wires arrangrr's MIDI output to the synth. Quit the REPL (`quit`, Ctrl-D or
# Ctrl-C) and everything this script started is killed.
set -euo pipefail
cd "$(dirname "$0")/../.."

KILL_FIRST=0
for arg in "$@"; do
  case "$arg" in
    -k|--kill) KILL_FIRST=1 ;;
    -h|--help)
      echo "usage: demo/jam/start.sh [-k|--kill]"
      echo "  -k, --kill   kill leftover arrangrr/fluidsynth sessions before starting"
      exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 2 ;;
  esac
done

CLI=build/host/app/platform/host/arrangrr
SETUP="$(dirname "$0")/setup.acmd"
SYNTH_PID=""
WAITER_PID=""
CLI_PID=""

cleanup() {
  trap - EXIT INT TERM HUP
  set +e  # teardown must run to completion even when a kill target is gone
  if [ -n "$CLI_PID" ]; then
    kill "$CLI_PID" 2>/dev/null
  fi
  if [ -n "$WAITER_PID" ]; then
    kill "$WAITER_PID" 2>/dev/null
  fi
  if [ -n "$SYNTH_PID" ]; then
    kill "$SYNTH_PID" 2>/dev/null
  fi
  # Belt and braces: nothing this script spawned may survive it.
  pkill -P $$ 2>/dev/null
  wait 2>/dev/null
  return 0
}
trap cleanup EXIT INT TERM HUP

# --- safeguard: never start on top of leftover sessions ---------------------
if [ "$KILL_FIRST" -eq 1 ]; then
  echo "--kill: clearing previous sessions..."
  pkill -x arrangrr 2>/dev/null || true
  pkill -x fluidsynth 2>/dev/null || true
  sleep 0.4
fi
refuse() {
  echo "$1 is already running:"
  shift; "$@"
  echo "Stop it (or rerun with --kill) — this script refuses to double up."
  exit 2
}
if pgrep -x arrangrr >/dev/null 2>&1; then
  refuse "An 'arrangrr' process" pgrep -ax arrangrr
fi
if pgrep -x fluidsynth >/dev/null 2>&1; then
  refuse "A 'fluidsynth' process" pgrep -ax fluidsynth
fi
# Single-instance lock: atomic and immune to command-line lookalikes
# (a pgrep -f heuristic here once matched an unrelated shell command).
LOCK="${XDG_RUNTIME_DIR:-/tmp}/arrangrr-jam.lock"
exec 200>"$LOCK"
if ! flock -n 200; then
  echo "Another jam script already holds the lock ($LOCK)."
  echo "Quit that session first (or rerun with --kill after finding it)."
  exit 2
fi

# --- prerequisites ----------------------------------------------------------
if ! command -v fluidsynth >/dev/null 2>&1; then
  echo "fluidsynth is not installed. Get it with:"
  echo "  sudo dnf install fluidsynth fluid-soundfont-gm"
  exit 2
fi

SOUNDFONT=""
for sf in /usr/share/soundfonts/FluidR3_GM.sf2 /usr/share/soundfonts/default.sf2 \
          /usr/share/soundfonts/*.sf2; do
  [ -f "$sf" ] && SOUNDFONT="$sf" && break
done
if [ -z "$SOUNDFONT" ]; then
  echo "No GM soundfont found under /usr/share/soundfonts. Get one with:"
  echo "  sudo dnf install fluid-soundfont-gm"
  exit 2
fi

if [ ! -x "$CLI" ]; then
  echo "arrangrr binary not built yet — building (host preset)..."
  cmake --preset host >/dev/null
  cmake --build --preset host >/dev/null
fi

# --- synth in the background -------------------------------------------------
echo "Starting FluidSynth ($SOUNDFONT)..."
fluidsynth -a pipewire -i -s "$SOUNDFONT" >/dev/null 2>&1 &
SYNTH_PID=$!

for _ in $(seq 1 50); do
  aconnect -l 2>/dev/null | grep -q "FLUID Synth" && break
  sleep 0.1
done
if ! aconnect -l 2>/dev/null | grep -q "FLUID Synth"; then
  echo "FluidSynth's ALSA port never appeared."
  exit 1
fi

# --- wire arrangrr -> synth as soon as its port shows up ---------------------
(
  for _ in $(seq 1 50); do
    aconnect -l 2>/dev/null | grep -q "arrangrr" && break
    sleep 0.1
  done
  aconnect arrangrr:1 "FLUID Synth":0 2>/dev/null || true
) &
WAITER_PID=$!

cat <<'BANNER'
------------------------------------------------------------------
 arrangrr jam — the band is configured, the clock is yours.

   transport start          the band comes in on | C Am F G |
   style section varB       busier groove (lands on the bar)
   style section fillA      one-bar fill, then back
   seq transpose to G       whole progression re-derives in G
   play D  /  play E min7   solo chords on channel 4
   style section ending1    outro, stops by itself
   panic                    the red button
   quit                     leave (synth shuts down too)
------------------------------------------------------------------
BANNER

# The REPL has its own pane UI with native line editing on a terminal.
REPL=("$CLI" --events human --init "$SETUP")

# The REPL runs as a child and we wait on it: `wait` is interruptible, so
# INT/TERM reach the trap immediately (a foreground child would defer it) and
# the trap tears everything down. The explicit <&0 matters: POSIX gives a
# backgrounded command /dev/null as stdin, which made the REPL quit on the
# spot — redirecting from our fd 0 hands it the real terminal.
"${REPL[@]}" <&0 &
CLI_PID=$!
rc=0
wait "$CLI_PID" || rc=$?
CLI_PID=""
exit "$rc"

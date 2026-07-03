#!/usr/bin/env bash
# One-console jam: starts a GM synth (FluidSynth) in the background, launches
# the arrangrr REPL with the band pre-configured (setup.acmd via --init), and
# wires arrangrr's MIDI output to the synth. Quit the REPL (`quit`, Ctrl-D or
# Ctrl-C) and everything this script started is killed.
set -euo pipefail
cd "$(dirname "$0")/../.."

CLI=build/host/app/platform/host/arrangrr
SETUP="$(dirname "$0")/setup.acmd"
SYNTH_PID=""
WAITER_PID=""
CLI_PID=""

cleanup() {
  trap - EXIT INT TERM
  [ -n "$CLI_PID" ] && kill "$CLI_PID" 2>/dev/null
  [ -n "$WAITER_PID" ] && kill "$WAITER_PID" 2>/dev/null
  [ -n "$SYNTH_PID" ] && kill "$SYNTH_PID" 2>/dev/null
  # Belt and braces: nothing this script spawned may survive it.
  pkill -P $$ 2>/dev/null || true
  wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# --- safeguard: refuse to start on top of leftover processes ----------------
if pgrep -x arrangrr >/dev/null 2>&1; then
  echo "An 'arrangrr' process is already running:"
  pgrep -ax arrangrr
  echo "Stop it first (pkill -x arrangrr) or use that session."
  exit 2
fi
if pgrep -x fluidsynth >/dev/null 2>&1; then
  echo "A 'fluidsynth' process is already running:"
  pgrep -ax fluidsynth
  echo "Stop it first (pkill -x fluidsynth) — this script manages its own synth."
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

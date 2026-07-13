#!/usr/bin/env bash
# Shared demo launcher: FluidSynth in the background, arrangrr REPL in the
# foreground, ALSA wiring, single-instance lock, and a teardown that leaves
# nothing behind on quit/Ctrl-C/EOF/TERM/HUP (see the verified matrix in the
# git history). Sourced by apps/demo/*/start.sh wrappers, which set:
#   SETUP  — .acmd file fed via --init (may be /dev/null)
#   MOTD   — text file shown in the REPL's persistent panel (optional)
#   KILL_FIRST — 1 to clear leftover sessions before starting
set -euo pipefail

CLI=build/host/apps/tools/cli-arrangrr/cli-arrangrr
SYNTH_PID=""
WAITER_PID=""
CLI_PID=""

cleanup() {
  trap - EXIT INT TERM HUP
  set +e  # teardown must run to completion even when a kill target is gone
  if [ -n "$CLI_PID" ]; then
    kill "$CLI_PID" 2>/dev/null || true
  fi
  if [ -n "$WAITER_PID" ]; then
    kill "$WAITER_PID" 2>/dev/null || true
  fi
  if [ -n "$SYNTH_PID" ]; then
    kill "$SYNTH_PID" 2>/dev/null || true
  fi
  # Belt and braces: nothing this script spawned may survive it.
  pkill -P $$ 2>/dev/null || true
  wait 2>/dev/null || true
  return 0
}
trap cleanup EXIT INT TERM HUP

launch_jam() {
  # --- safeguard: never start on top of leftover sessions -------------------
  if [ "${KILL_FIRST:-0}" -eq 1 ]; then
    echo "--kill: clearing previous sessions..."
    pkill -x cli-arrangrr 2>/dev/null || true
    pkill -x fluidsynth 2>/dev/null || true
    sleep 0.4
  fi
  if pgrep -x cli-arrangrr >/dev/null 2>&1; then
    echo "A 'cli-arrangrr' process is already running:"
    pgrep -ax cli-arrangrr
    echo "Stop it (or rerun with --kill) — this script refuses to double up."
    exit 2
  fi
  if pgrep -x fluidsynth >/dev/null 2>&1; then
    echo "A 'fluidsynth' process is already running:"
    pgrep -ax fluidsynth
    echo "Stop it (or rerun with --kill) — this script manages its own synth."
    exit 2
  fi
  # Single-instance lock, atomic and immune to command-line lookalikes.
  local lock="${XDG_RUNTIME_DIR:-/tmp}/arrangrr-jam.lock"
  exec 200>"$lock"
  if ! flock -n 200; then
    echo "Another demo script already holds the lock ($lock)."
    echo "Quit that session first (or rerun with --kill after finding it)."
    exit 2
  fi

  # --- prerequisites ---------------------------------------------------------
  if ! command -v fluidsynth >/dev/null 2>&1; then
    echo "fluidsynth is not installed. Get it with:"
    echo "  sudo dnf install fluidsynth fluid-soundfont-gm"
    exit 2
  fi
  local soundfont=""
  local sf
  for sf in /usr/share/soundfonts/FluidR3_GM.sf2 /usr/share/soundfonts/default.sf2 \
            /usr/share/soundfonts/*.sf2; do
    if [ -f "$sf" ]; then
      soundfont="$sf"
      break
    fi
  done
  if [ -z "$soundfont" ]; then
    echo "No GM soundfont found under /usr/share/soundfonts. Get one with:"
    echo "  sudo dnf install fluid-soundfont-gm"
    exit 2
  fi
  if [ ! -x "$CLI" ]; then
    echo "cli-arrangrr binary not built yet — building (host preset)..."
    cmake --preset host >/dev/null
    cmake --build --preset host >/dev/null
  fi

  # --- synth + wiring ----------------------------------------------------------
  echo "Starting FluidSynth ($soundfont)..."
  # Match FluidSynth's render rate to PipeWire's (no resampling artefacts) and
  # give it a roomy period so integrated-audio underruns don't crackle during
  # sustained play. Bump period-size to 4096 if a crackle still slips through.
  local pw_rate=48000
  if command -v pw-metadata >/dev/null 2>&1; then
    local detected
    detected="$(pw-metadata -n settings 2>/dev/null | awk -F"'" '/clock\.rate/{print $4; exit}')" || true
    [ -n "$detected" ] && pw_rate="$detected"
  fi
  fluidsynth -a pipewire -o audio.period-size=2048 -o synth.sample-rate="$pw_rate" \
    -i -s "$soundfont" >/dev/null 2>&1 &
  SYNTH_PID=$!
  local i
  for i in $(seq 1 50); do
    if aconnect -l 2>/dev/null | grep -q "FLUID Synth"; then
      break
    fi
    sleep 0.1
  done
  if ! aconnect -l 2>/dev/null | grep -q "FLUID Synth"; then
    echo "FluidSynth's ALSA port never appeared."
    exit 1
  fi
  # Warm the audio path: the first note after FluidSynth starts crackles
  # while PipeWire opens the stream — swallow that with an inaudible note
  # (velocity 1 on channel 16) sent straight to the synth.
  if command -v aseqsend >/dev/null 2>&1; then
    aseqsend -p "FLUID Synth:0" 9F 00 01 8F 00 40 2>/dev/null || true
  fi

  (
    for i in $(seq 1 50); do
      if aconnect -l 2>/dev/null | grep -q "sonotron"; then
        break
      fi
      sleep 0.1
    done
    aconnect sonotron:1 "FLUID Synth":0 2>/dev/null || true
  ) &
  WAITER_PID=$!

  # --- REPL --------------------------------------------------------------------
  local args=(--events human --init "$SETUP")
  if [ -n "${MOTD:-}" ]; then
    args+=(--motd "$MOTD")
  fi
  # Waited child: `wait` is interruptible so signals reach the trap at once;
  # <&0 hands it our real stdin (POSIX gives backgrounded commands /dev/null).
  "$CLI" "${args[@]}" <&0 &
  CLI_PID=$!
  local rc=0
  wait "$CLI_PID" || rc=$?
  CLI_PID=""
  exit "$rc"
}

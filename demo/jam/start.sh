#!/usr/bin/env bash
# One-console jam: starts a GM synth (FluidSynth) in the background, launches
# the arrangrr REPL with the band pre-configured (setup.acmd), and wires
# arrangrr's MIDI output to the synth. Quit the REPL (`quit` or Ctrl-D) and
# everything shuts down.
set -euo pipefail
cd "$(dirname "$0")/../.."

CLI=build/host/app/platform/host/arrangrr
SETUP="$(dirname "$0")/setup.acmd"
SYNTH_PID=""

cleanup() {
  [ -n "$SYNTH_PID" ] && kill "$SYNTH_PID" 2>/dev/null
}
trap cleanup EXIT

# --- prerequisites ---------------------------------------------------------
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

# --- synth in the background ----------------------------------------------
echo "Starting FluidSynth ($SOUNDFONT)..."
fluidsynth -a pipewire -i -s "$SOUNDFONT" >/dev/null 2>&1 &
SYNTH_PID=$!

# Wait for the synth's ALSA client to appear.
for _ in $(seq 1 50); do
  aconnect -l 2>/dev/null | grep -q "FLUID Synth" && break
  sleep 0.1
done
if ! aconnect -l 2>/dev/null | grep -q "FLUID Synth"; then
  echo "FluidSynth's ALSA port never appeared."
  exit 1
fi

# --- wire arrangrr -> synth as soon as its port shows up --------------------
(
  for _ in $(seq 1 50); do
    aconnect -l 2>/dev/null | grep -q "arrangrr" && break
    sleep 0.1
  done
  aconnect arrangrr:1 "FLUID Synth":0 2>/dev/null || true
) &

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

# Feed the setup, then hand stdin over to you (same REPL, one console).
exec 3<"$SETUP"
{ cat <&3; exec cat; } | "$CLI" --events human

#!/usr/bin/env bash
# One-console jam: FluidSynth + the arrangrr REPL with the band
# pre-configured (setup.acmd) and armed — `transport start` counts it in.
# Shared lifecycle (safeguards, wiring, teardown) lives in demo/lib/launch.sh.
set -euo pipefail
cd "$(dirname "$0")/../.."

SETUP="demo/jam/setup.acmd"
MOTD="demo/jam/motd.txt"
KILL_FIRST=0
for arg in "$@"; do
  case "$arg" in
    -k|--kill) KILL_FIRST=1 ;;
    -h|--help)
      echo "usage: demo/jam/start.sh [-k|--kill]"
      echo "  -k, --kill   kill leftover arrangrr/fluidsynth sessions first"
      exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 2 ;;
  esac
done

source demo/lib/launch.sh
launch_jam

#!/usr/bin/env bash
# One-console jam: FluidSynth + the arrangrr REPL with the band
# pre-configured (setup.acmd) and armed — `transport start` counts it in.
# Shared lifecycle (safeguards, wiring, teardown) lives in apps/demo/lib/launch.sh.
set -euo pipefail
cd "$(dirname "$0")/../../.."

SETUP="apps/demo/jam/setup.acmd"
MOTD="apps/demo/jam/motd.txt"
KILL_FIRST=0
for arg in "$@"; do
  case "$arg" in
    -k|--kill) KILL_FIRST=1 ;;
    -h|--help)
      echo "usage: apps/demo/jam/start.sh [-k|--kill]"
      echo "  -k, --kill   kill leftover cli-arrangrr/fluidsynth sessions first"
      exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 2 ;;
  esac
done

source apps/demo/lib/launch.sh
launch_jam

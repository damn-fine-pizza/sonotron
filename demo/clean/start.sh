#!/usr/bin/env bash
# Clean-room demo: same launcher as demo/jam (synth, wiring, safeguards,
# bulletproof teardown) but nothing is pre-configured — you program the
# session yourself. -i/--init pre-arms an empty looping sequence so the only
# thing left to type is `seq add ...` and `transport start`.
set -euo pipefail
cd "$(dirname "$0")/../.."

HERE="demo/clean"
SETUP="$HERE/setup.acmd"
MOTD="$HERE/motd.txt"
KILL_FIRST=0
for arg in "$@"; do
  case "$arg" in
    -i|--init)
      SETUP="$HERE/armed.acmd"
      MOTD="$HERE/motd-armed.txt"
      ;;
    -k|--kill) KILL_FIRST=1 ;;
    -h|--help)
      echo "usage: demo/clean/start.sh [-i|--init] [-k|--kill]"
      echo "  -i, --init   pre-arm an empty looping sequence at 96 BPM"
      echo "               (then you only: seq add C ... + transport start)"
      echo "  -k, --kill   kill leftover arrangrr/fluidsynth sessions first"
      exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 2 ;;
  esac
done

source demo/lib/launch.sh
launch_jam

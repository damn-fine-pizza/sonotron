#!/usr/bin/env bash
# Phase 3 client-session smoke (docs/design/orchestrator-pipeline-
# extraction.md §17, the follow-up to Phase 3a's kParamState wire): a PURE
# `cli-arrangrr --connect` client exchanges a note gesture (the new `note`
# L1 verb, §17.3a) and a groove tweak with a live `sonotron-server
# --control` session, receives the kParamState echo over the wire (the new
# param_state_wire.hpp shape), and folds it into its own core-free mirror
# (TuiClient/ParamStateMirror) -- proving the client/server seam end to end,
# not just the in-process unit tests. Skips (exit 77) when ALSA seq is
# unavailable, mirroring server_socket.sh's own guard.
set -u
SERVER="$1"
CLIENT="$2"
SRV=""

command -v aseqdump >/dev/null 2>&1 || exit 77
aseqdump -l >/dev/null 2>&1 || exit 77

# A foreign session must never be touched: ALSA name-based resolution would
# hit it. Skip instead of interfering.
if pgrep -x sonotron-server >/dev/null 2>&1; then
  echo "a sonotron-server session is already running - skipping to leave it alone"
  exit 77
fi

tmp=$(mktemp -d)
cleanup() {
  [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
  rm -rf "$tmp"
}
trap cleanup EXIT

sock="$tmp/control.sock"

"$SERVER" --control "$sock" --events jsonl > "$tmp/server.log" 2>&1 &
SRV=$!

# Wait for the control socket to appear (server startup, including its
# default `port open in in0` / `port open out out0` / `thru in0 out0` --
# the note verb's target port already exists by the time this appears).
for _ in $(seq 1 50); do
  [ -S "$sock" ] && break
  sleep 0.1
done
[ -S "$sock" ] || { echo "control socket never appeared"; cat "$tmp/server.log"; exit 1; }

printf 'note in0 on 60 100\ngroove swing 50\nquit\n' \
  | "$CLIENT" --connect "$sock" > "$tmp/client.log" 2>&1
rc=$?

kill "$SRV" 2>/dev/null
wait "$SRV" 2>/dev/null
SRV=""

if [ "$rc" -ne 0 ]; then
  echo "cli-arrangrr --connect exited non-zero ($rc):"
  cat "$tmp/client.log"
  exit 1
fi

# 1. The note gesture round-tripped as a real MIDI-out event (never a raw
#    binary Command, never a direct feed_midi call from the client -- it
#    went over the wire as the `note` L1 line and came back as the SAME
#    canonical JSONL every other client sees).
grep -q '"ev":"midi-out"' "$tmp/client.log" \
  || { echo "note gesture did not round-trip as a midi-out event:"; cat "$tmp/client.log"; exit 1; }

# 2. The groove tweak's kParamState echo arrived on the wire...
grep -q '"ev":"param","param":"groove"' "$tmp/client.log" \
  || { echo "groove kParamState echo never arrived:"; cat "$tmp/client.log"; exit 1; }

# 3. ...AND the client's own core-free mirror (TuiClient/ParamStateMirror)
#    actually folded it to the right value -- not just "received bytes".
grep -q '\[mirror\] groove.swing=50' "$tmp/client.log" \
  || { echo "mirror did not fold the groove echo to the expected value:"; cat "$tmp/client.log"; exit 1; }

exit 0

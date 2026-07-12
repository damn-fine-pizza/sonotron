#!/usr/bin/env bash
# Phase 2a socket smoke (docs/design/orchestrator-pipeline-extraction.md §15
# gate 2): a client connects to `sonotron-server --control PATH` and
# round-trips one L1 line over the UDS control socket -- the same wire
# behaviour cli-arrangrr's --control already proves (test_host.cpp's
# test_uds_server_end_to_end), but exercised end-to-end through the new
# headless binary's own poll() loop, not just the UdsServer unit in isolation.
# Skips (exit 77) when ALSA seq / alsa-utils or python3 are unavailable.
set -u
SERVER="$1"
SRV=""

command -v aseqdump >/dev/null 2>&1 || exit 77
aseqdump -l >/dev/null 2>&1 || exit 77
command -v python3 >/dev/null 2>&1 || exit 77

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

# Wait for the control socket to appear (server startup).
for _ in $(seq 1 50); do
  [ -S "$sock" ] && break
  sleep 0.1
done
[ -S "$sock" ] || { echo "control socket never appeared"; cat "$tmp/server.log"; exit 1; }

python3 - "$sock" > "$tmp/client.log" <<'PY'
import socket
import sys
import time

path = sys.argv[1]
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(path)
s.sendall(b"transport start\n")
s.settimeout(3)

data = b""
deadline = time.time() + 3
while b'"ev":"transport"' not in data and time.time() < deadline:
    try:
        chunk = s.recv(4096)
    except socket.timeout:
        break
    if not chunk:
        break
    data += chunk
s.close()
sys.stdout.write(data.decode("utf-8", "replace"))
sys.exit(0 if b'"ev":"transport"' in data and b'"state":"playing"' in data else 1)
PY
rc=$?

kill "$SRV" 2>/dev/null
wait "$SRV" 2>/dev/null
SRV=""

if [ "$rc" -ne 0 ]; then
  echo "did not observe the transport-start round-trip:"
  cat "$tmp/client.log"
  exit 1
fi

grep -q '"ev":"transport","state":"playing"' "$tmp/client.log" \
  || { echo "unexpected broadcast payload:"; cat "$tmp/client.log"; exit 1; }

exit 0

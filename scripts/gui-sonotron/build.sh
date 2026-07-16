#!/usr/bin/env bash
#
# scripts/gui-sonotron/build.sh — painless (cross-)build of the gui-sonotron app.
#
# Usage:
#   build.sh [TARGET] [--clean-first] [--release]
#
#   TARGET   host (default, = whatever OS you are on) | linux | osx | windows
#   --clean-first   wipe the target's build dir first (fresh configure + build)
#   --release       build the release preset instead of debug (host/native only)
#
# Supported host→target matrix (per the owner's cases):
#   1. host linux  → linux (native, works today)
#                  → osx     (cross via osxcross toolchain — future macOS workstream)
#                  → windows (cross via mingw-w64 toolchain — future Windows workstream)
#   2. host osx    → osx     (native)
#   3. host windows→ windows (native, future)
#
# Cross targets whose platform support / toolchain is not in the tree yet
# degrade GRACEFULLY: a clear note + exit 0 (never a hard error / CI break),
# matching the owner rule "don't flag the osx build when the build host isn't
# osx". They light up automatically the day the platform workstream drops in
# the matching cmake/toolchains/<name>.cmake + the CoreMIDI/WinMM backend.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

# CMake target name (underscore) vs. the produced binary name (hyphen) —
# apps/gui-sonotron/CMakeLists.txt: add_executable(gui_sonotron ...) with
# OUTPUT_NAME gui-sonotron.
GUI_TARGET="gui_sonotron"
GUI_BIN="gui-sonotron"

usage() {
  cat <<'EOF'
build.sh — painless (cross-)build of the gui-sonotron app.

Usage:
  build.sh [TARGET] [--clean-first] [--release]

  TARGET          host (default, = whatever OS you are on) | linux | osx | windows
  --clean-first   wipe the target's build dir first (fresh configure + build)
  --release       build the release preset instead of debug (host/native only)

Host→target matrix:
  linux  → linux (native today) | osx (osxcross) | windows (mingw-w64)
  osx    → osx (native)
  windows→ windows (native, future)

Cross targets whose platform support / toolchain is not in the tree yet skip
gracefully (clear note + exit 0, never a hard error). They light up when the
platform workstream adds cmake/toolchains/<name>.cmake + the platform backend.
EOF
}

# --- parse args --------------------------------------------------------------
TARGET="host"
CLEAN_FIRST=0
RELEASE=0
for arg in "$@"; do
  case "$arg" in
    --clean-first)      CLEAN_FIRST=1 ;;
    --release)          RELEASE=1 ;;
    -h|--help)          usage; exit 0 ;;
    host|linux|osx|macos|windows|win) TARGET="$arg" ;;
    *) echo "build.sh: unknown argument '$arg'" >&2; usage >&2; exit 2 ;;
  esac
done

# --- detect the build host ---------------------------------------------------
case "$(uname -s)" in
  Linux)                 HOST_OS="linux" ;;
  Darwin)                HOST_OS="osx" ;;
  MINGW*|MSYS*|CYGWIN*)  HOST_OS="windows" ;;
  *)                     HOST_OS="unknown" ;;
esac

# resolve "host" and normalize aliases
[ "$TARGET" = "host" ] && TARGET="$HOST_OS"
case "$TARGET" in macos) TARGET="osx" ;; win) TARGET="windows" ;; esac

note()  { printf '\033[36m[build]\033[0m %s\n' "$*"; }
skip()  { printf '\033[33m[build] SKIP:\033[0m %s\n' "$*"; }
die()   { printf '\033[31m[build] error:\033[0m %s\n' "$*" >&2; exit 1; }

# --- native build (target == host) ------------------------------------------
build_native() {
  local preset="host" build_dir="$REPO_ROOT/build/host"
  if [ "$RELEASE" = 1 ]; then preset="host-release"; build_dir="$REPO_ROOT/build/host-release"; fi
  if [ "$CLEAN_FIRST" = 1 ]; then note "clean: removing $build_dir"; rm -rf "$build_dir"; fi
  note "configuring ($preset) …"
  cmake --preset "$preset"
  note "building target '$GUI_TARGET' …"
  cmake --build --preset "$preset" --target "$GUI_TARGET"
  note "done → $build_dir/apps/gui-sonotron/$GUI_BIN"
}

# --- cross build (target != host) -------------------------------------------
# Looks for cmake/toolchains/<toolchain>.cmake and the cross compiler; if either
# is missing the target is not ready yet → graceful skip (exit 0).
build_cross() {
  local toolchain_name="$1" probe_cmd="$2" future_note="$3"
  local toolchain="$REPO_ROOT/cmake/toolchains/${toolchain_name}.cmake"
  local build_dir="$REPO_ROOT/build/$TARGET"

  if [ ! -f "$toolchain" ] || ! command -v "$probe_cmd" >/dev/null 2>&1; then
    skip "host=$HOST_OS → target=$TARGET is not buildable yet."
    printf '       %s\n' "$future_note"
    [ -f "$toolchain" ] || printf '       missing toolchain file: %s\n' "cmake/toolchains/${toolchain_name}.cmake"
    command -v "$probe_cmd" >/dev/null 2>&1 || printf '       missing cross compiler: %s\n' "$probe_cmd"
    exit 0   # not an error — this target just isn't wired yet
  fi

  if [ "$CLEAN_FIRST" = 1 ]; then note "clean: removing $build_dir"; rm -rf "$build_dir"; fi
  note "configuring cross ($TARGET, $toolchain_name) …"
  cmake -S "$REPO_ROOT" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain"
  note "building target '$GUI_TARGET' …"
  cmake --build "$build_dir" --target "$GUI_TARGET"
  note "done → $build_dir/apps/gui-sonotron/$GUI_BIN"
}

# --- dispatch ----------------------------------------------------------------
[ "$HOST_OS" = "unknown" ] && die "unrecognized build host ($(uname -s))"

if [ "$TARGET" = "$HOST_OS" ]; then
  build_native
else
  case "$HOST_OS:$TARGET" in
    linux:osx)
      build_cross "osxcross" "o64-clang++" \
        "target osx from linux needs the macOS platform backend (CoreMIDI) + an osxcross toolchain — future macOS workstream." ;;
    linux:windows)
      build_cross "mingw-w64" "x86_64-w64-mingw32-g++" \
        "target windows from linux needs the Windows platform backend (WinMM/WinRT MIDI) + a mingw-w64 toolchain — future Windows workstream." ;;
    *)
      skip "host=$HOST_OS → target=$TARGET is not a supported combination."
      printf '       supported: linux→{linux,osx,windows}, osx→osx, windows→windows.\n'
      exit 0 ;;
  esac
fi

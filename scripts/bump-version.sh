#!/usr/bin/env bash
#
# scripts/bump-version.sh — bump the repo-root VERSION file's MAJOR.MINOR.
#
# Usage:
#   bump-version.sh [minor|major] [--no-commit]
#
#   minor         0.3 -> 0.4 (default)
#   major         0.3 -> 1.0 (resets MINOR to 0; there is no PATCH component)
#   --no-commit   rewrite VERSION only, skip `git add && git commit`
#
# The BUILD number (the third, `0.3.<N>` component) is never stored here --
# it is computed at CMake configure time from `git rev-list --count HEAD`
# (see components/platform/version/CMakeLists.txt), so bumping VERSION never
# touches it.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

note()  { printf '\033[36m[bump-version]\033[0m %s\n' "$*"; }
die()   { printf '\033[31m[bump-version] error:\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'EOF'
bump-version.sh — bump the repo-root VERSION file's MAJOR.MINOR.

Usage:
  bump-version.sh [minor|major] [--no-commit]

  minor         0.3 -> 0.4 (default)
  major         0.3 -> 1.0 (resets MINOR to 0; there is no PATCH component)
  --no-commit   rewrite VERSION only, skip `git add && git commit`

The BUILD number is never stored here -- it is computed at CMake configure
time from `git rev-list --count HEAD` (components/platform/version).
EOF
}

# --- parse args --------------------------------------------------------------
COMPONENT="minor"
NO_COMMIT=0
for arg in "$@"; do
  case "$arg" in
    minor|major)  COMPONENT="$arg" ;;
    --no-commit)  NO_COMMIT=1 ;;
    -h|--help)    usage; exit 0 ;;
    *) echo "bump-version.sh: unknown argument '$arg'" >&2; usage >&2; exit 2 ;;
  esac
done

VERSION_FILE="$REPO_ROOT/VERSION"
[ -f "$VERSION_FILE" ] || die "VERSION not found at $VERSION_FILE"

CURRENT="$(tr -d '[:space:]' < "$VERSION_FILE")"
[[ "$CURRENT" =~ ^([0-9]+)\.([0-9]+)$ ]] || die "VERSION does not hold a MAJOR.MINOR value: '$CURRENT'"
MAJOR="${BASH_REMATCH[1]}"
MINOR="${BASH_REMATCH[2]}"

case "$COMPONENT" in
  minor) MINOR=$((MINOR + 1)) ;;
  major) MAJOR=$((MAJOR + 1)); MINOR=0 ;;
esac

NEW="${MAJOR}.${MINOR}"
note "bumping $COMPONENT: $CURRENT -> $NEW"
printf '%s\n' "$NEW" > "$VERSION_FILE"

if [ "$NO_COMMIT" = 1 ]; then
  note "--no-commit: VERSION rewritten, no git action taken"
  exit 0
fi

git add "$VERSION_FILE"
git commit -m "chore: bump version to $NEW"
note "committed: chore: bump version to $NEW"

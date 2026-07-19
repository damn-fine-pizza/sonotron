# CLI Corrections (sonotron)

# Hand-curated from `rtk learn` output, keeping only verified, durable rules.
# NOTE: `rtk learn --write-rules` OVERWRITES this file with raw, mostly-noise
# pairings — do not re-run it blindly; re-curate if you do.

## Build & test (CMake presets exist)
- Configure/build via presets: `cmake --build --preset host` (see `CMakePresets.json`: host, host-release, coverage, tidy, arm, arm-release).
- Run tests via preset: `ctest --preset host`, or target a subset with `ctest --test-dir build/host -R "<name>" --output-on-failure`.
- Do NOT `cd build/host && ctest ...` — use `ctest --test-dir build/host ...` (or the preset) so the working directory is unaffected.
- GUI verification path: `scripts/gui-sonotron/build.sh` then `ctest`, always in the FOREGROUND.

## clang-format (repo enforces it)
- Check: `clang-format --dry-run --Werror <files>` (exit 1 just means "needs formatting" — it is a signal, not a broken command).
- Fix in place: `clang-format -i <files>`.

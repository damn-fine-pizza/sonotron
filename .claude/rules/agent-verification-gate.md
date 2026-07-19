# Agent Verification Gate (sonotron)

# How the orchestrator handles the build/test gate when delegating implementation
# to subagents. Encodes the fix for a recurring failure: a long ctest gets
# auto-backgrounded, the agent closes WITHOUT a result, and unverified work risks
# being committed. Independent of session memory — always loaded.

## Root cause
- The Bash tool auto-backgrounds any command past the 120s default timeout.
- The full GUI gate (`scripts/gui-sonotron/build.sh` + `ctest -E "live_alsa|live_tracks"`) is ~5-9 min.
- An agent that runs it at the default timeout loses the foreground; it cannot hold a long
  command past its turn, so it closes with a NON-result ("moved to background / I'll wait").
- A gate you never receive is not a gate.

## The gate belongs to the ORCHESTRATOR, not the implementing agent
- Default subagent brief = "implement + report the exact files/diff + compile + smoke check.
  Do NOT run the full ctest suite." The orchestrator runs the full gate.
- The orchestrator runs `build.sh` + `ctest` in the background (`run_in_background`) and is
  re-invoked on completion, so it never pause-loops.
- This is consistent with CLAUDE.md: "the owning lead independently verifies build and test claims."

## If an agent must run the full gate itself (rare)
- It MUST pass an explicit Bash `timeout` ≥ 540000 ms so the command stays foreground.
- Its final report MUST contain the literal pass/fail line (`X/Y tests passed`).
- Ending a turn with "I'll wait for the background task" is a FAILED turn, not a completed one.

## Acceptance guard (hard check before committing)
- Treat any agent completion whose report lacks the literal `X/Y tests passed` line as UNVERIFIED.
- Never commit on an unverified report. Resume the agent for the number, or run the gate yourself.

## Never corrupt the build dir
- NEVER run two builds/ctests concurrently against the same `build/host` (races → fake all-red,
  "Unable to find executable", everything failing in seconds).
- Use separate build dirs / worktrees, or serialize. When a gate looks impossibly red, suspect a
  corrupted/incomplete build dir first: `rm -rf build/host` and rebuild clean before trusting it.

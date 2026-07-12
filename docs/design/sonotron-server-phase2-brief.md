# Phase 2 brief — server as a LIBRARY; GUI + engine in one binary, two threads

Status: PREPARED, Corelli-reviewed (§15, APPROVED WITH CORRECTIONS — folded
below). Owner-decided the one open fork (D38 relaxed). Ready to dispatch to
Nazzareno once Phase 1 is committed.

Anchored to `orchestrator-pipeline-extraction.md` §4/§5, **reshaped** by the
owner's mid-milestone directive (this section supersedes the "separate
`sonotron-server` binary over UDS" reading of §4/§5 for Phase 2).

## The owner's reshape (verbatim intent)
> The server, for now, is a **library**. We are not building a distributed
> system, so GUI and server can live in the **same binary** that links two libs
> and runs **two threads** — no need for two processes.

So `sonotron-server` is NOT primarily a binary; it is a **library** (the
composed `components/runtime` + `arrangrr` stage + non-TUI hostrt glue) that two
consumers link.

## Topology
- **The server library** (`libsonotron_server`, or the composition of
  `components/runtime` + `components/arrangrr` + the non-TUI half of
  `components/hostrt`: `jsonl`, `uds_server`, `gm_program`, `note_names`,
  `alsa_midi`). Exposes the `Command`-in / `OutEvent`-out ports. Option A from
  the prior brief still holds: composes `Runtime` + arrangrr stage **directly**;
  no dedicated orchestrator component yet (deferred to Phase 4).
- **The GUI binary** links the server library and runs the engine on a
  **dedicated thread**, communicating with the render thread via in-process
  ring buffers (below). This is the primary consumer.
- **A thin headless `sonotron-server` binary** — just a `main()` around the
  library — retained ONLY for the golden `--script` mode and CLI/headless use,
  speaking the **UDS socket + JSONL** exactly as today.

## Thread-boundary mechanism (owner-confirmed): in-process POD ring buffers
NOT a socket between threads. A socket is the inter-**process** tool; between two
threads of one process it is pure overhead (kernel syscalls + serialization) for
zero benefit. The threads share the address space, so they hand data directly
through **two bounded SPSC ring buffers**, one per direction:

- **GUI thread → engine thread**: `Command` slots (user input).
- **engine thread → GUI thread**: `OutEvent` slots (what happened).

`push`/`pop` are atomic O(1) index moves; the engine drain is **wait-free** — it
never blocks and never allocates (respects the core's no-heap realtime
doctrine). A full ring drops / applies backpressure; the engine never stalls
behind the GUI's GPU/vsync. `OutEvent` (16 B) and `Command` (20 B) are fixed-size
PODs → fixed-slot rings, zero heap.

**Two pumps, each on its own clock, decoupled by the rings:**
- Engine pump (realtime tick cadence, 960 PPQN): drain `Command` ring → advance
  N ticks → push produced `OutEvent`s.
- GUI pump (~60 fps / vsync): drain `OutEvent` ring in batch → update
  `app_state`/view; on user action → push a `Command`.

Neither waits for the other; the rings absorb the rate mismatch. Same logical
shape as today's D38 socket (server broadcasts, client drains when it can) —
without kernel or serialization.

**Host-side glue only.** The rings + threading live in the GUI app (host), so
they may use host primitives freely — a simple `std::mutex`-guarded bounded
queue is acceptable for the first cut; lock-free SPSC is the clean target,
hardened later. The freestanding core stays untouched.

## Data on the ring: PODs (revises the prior brief's "JSONL on the queue")
The ring carries `OutEvent`/`Command` **PODs**, not JSONL. Consequence for the
GUI: its decode changes from `parse_brain_event(jsonl_string) → BrainEvent` to
`brain_event_from_outevent(OutEvent) → BrainEvent` — a new, more fundamental
decode (JSONL is just the serialized `OutEvent`). `AppState::apply(BrainEvent)`
is **unchanged**; only the decode source changes.

The **JSONL wire stays alive and unchanged** for the inter-process paths: the
headless `sonotron-server` over the socket, and the **18 goldens** via
`--script`. Same `Command`/`OutEvent` contract on both the ring and the socket →
they are interchangeable, and a future multi-process/distributed split is a swap
of ring-for-socket, no contract change.

## What does NOT happen in Phase 2 (deliberate)
- `hostrt::Shell` does **not** split — stays whole, linked by the server lib.
  The client/server Shell split is Phase 3.
- No ABI reshape. `test_abi_frozen` untouched.

## Test migration + green gate
- Goldens re-point `$<TARGET_FILE:cli_arrangrr>` → `$<TARGET_FILE:sonotron_server>`
  (the thin headless binary), one-liner per test.
- **18 goldens byte-identical** against `sonotron_server --script`.
- New: an in-process smoke — GUI-thread and engine-thread exchange one
  `Command`/`OutEvent` round-trip over the rings (no socket), proving the
  in-process pump works.
- Existing socket smoke still passes against the headless binary.
- `cli-arrangrr` still builds and works as today (untouched until Phase 3).

## Corelli §15 review — resolution (mandatory for Nazzareno)

Corelli reviewed this brief (design doc §15): **APPROVED WITH CORRECTIONS**.

1. **D38 collision — OWNER-DECIDED: relax D38 fully.** The brief's single binary
   means the GUI binary links the core; D38 ("the GUI never links/#includes the
   core") is **retired** for this milestone. The GUI may link
   `runtime`/`arrangrr`/`hostrt` and name `OutEvent` directly — no internal
   "engine-host" lib boundary is required. **Honest tradeoff the owner accepted**:
   the CMake wall that *guaranteed* GUI code can't recompute music theory (and
   drift from the engine's truth) becomes a convention, not an enforced barrier.
   Consequence: correction #4 below (shared labeling module) is now the PRIMARY
   safeguard against that drift and is kept mandatory. DESIGN.md's D38 entry must
   be updated to "relaxed/retired" when Phase 2 lands (Vasari's job).
2. **Ring — lock-free from the start, NOT mutex-first.** "Wait-free" and
   "mutex-first" are incompatible (real priority inversion, worsened because the
   `EventSink` fires one event per produced `OutEvent`, not one per tick). Use a
   bounded SPSC lock-free ring: atomic head/tail, cache-line padding,
   acquire/release ordering, one reserved slot. All host-side `std::atomic`, no
   new deps.
3. **Asymmetric overflow policy.** `OutEvent` (engine→GUI) MAY drop (existing D38
   precedent). `Command` (GUI→engine) must NEVER silently drop — generous ring +
   a `WarnCode` event on saturation.
4. **Double-decode drift — extract shared labeling module FIRST.** The label
   helpers (`quality_suffix`, `roman_degree`, `note_label`, `warn_name`, …) are
   private in `components/hostrt/jsonl.cpp`. Extract them into a shared module
   BEFORE writing `brain_event_from_outevent`, so `to_jsonl` and the new
   in-process decode call the same source. Thread the hidden `prefer_flats`
   parameter (from `Shell::m_prefer_flats`) through the new decode signature — the
   brief's original signature omitted it.
5. **`Runtime` ownership / thread-safety.** `Runtime::transport()`/`stage()` are
   public mutable accessors with no thread guarantee. Make them structurally
   unreachable from the GUI thread (not just a comment). Note: Phase 1 landed with
   the stage holding a `Transport&` reference — consistent with "only the engine
   thread runs the stage", but Phase 2 must enforce that structurally.

## Permanent constraints (carried into Nazzareno's intake)
Core no-heap dual-target (`runtime`/`arrangrr`/`common` stay freestanding; the
rings/threading are host-side glue); codebase-memory-mcp first for code;
rtk-proxied commands; NO commit/merge (owner commits after review); no new deps
without a flag.

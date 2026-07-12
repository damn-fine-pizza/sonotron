# Runtime extraction — Phase 1 move-plan (physical layout only)

Status: **MOVE-PLAN (Palladio, 2026-07-12), read-only proposal — no file has been moved.**
This document translates `docs/design/orchestrator-pipeline-extraction.md` §5 "Phase 1"
into an executable, ordered `git mv` / CMake / include plan. It does **not** redesign
anything: the Stage port shape (§3.6 of Corelli's doc) and the exact carving of
`Engine::advance_ticks` are out of my lane and are named, not resolved, below. This is a
handoff for the owner and for `nazzareno-cpp-implementor` to execute.

Companions: `docs/design/orchestrator-pipeline-extraction.md` (the design this
implements), `docs/design/project-structure.md` (the placement rules this obeys).

---

## 0. A precondition the design doc's "byte-for-byte" instruction does not survive as-is

Corelli's doc says `Transport` moves "byte-for-byte" from
`components/arrangrr/include/arrangrr/transport/transport.hpp` to `components/runtime`.
Taken literally this is **not buildable**: `transport.hpp` currently defines, inline,
three meter constants used **outside** `Transport` itself, by files that Corelli's own
§3.5 topology says must **stay** in `components/arrangrr`:

```
components/arrangrr/include/arrangrr/transport/transport.hpp:22-24
  inline constexpr std::uint32_t kBeatsPerBar = 4;
  inline constexpr std::uint32_t kTicksPerBeat = kPpqn;
  inline constexpr std::uint32_t kTicksPerBar = kBeatsPerBar * kTicksPerBeat;
```

Verified direct consumers of `kTicksPerBar`/`kTicksPerBeat`/`kBeatsPerBar` that are
**not** going anywhere (grep, full tree, `.claude/worktrees/*` excluded):

- `components/arrangrr/include/arrangrr/arranger/arranger.hpp:14` — `#include
  "arrangrr/transport/transport.hpp"  // kTicksPerBar` (comment confirms: constant-only,
  no `Transport` instance used in this header).
- `components/arrangrr/include/arrangrr/chord/chord_sequence.hpp:9` — same, feeds
  `quantize(Tick grid = kTicksPerBar)` (chord_sequence.hpp:82).
- `components/arrangrr/src/engine.cpp:407` — `m_seq.stop_record(m_now, ... : kTicksPerBar)`,
  an arrangrr-side `ChordSequencer` call, transitively via `engine.hpp`'s include.
- `components/arrangrr/src/engine_checks.cpp:35` — `engine.advance_ticks(kTicksPerBar,
  sink)` (this file itself relocates too, §5 below, but independently needs the constant).
- Transitively: `components/arrangrr/include/arrangrr/chord/chord_sequencer.hpp` (via its
  own include of `chord_sequence.hpp`, confirmed — it does **not** include
  `transport.hpp` directly).
- `components/hostrt/shell_io_commands.cpp`, `shell_parse.cpp`, `shell_music_commands.cpp`
  also reference these constants (bar-relative command parsing).
- A dozen `components/arrangrr/tests/*.cpp` files.

If `kBeatsPerBar`/`kTicksPerBeat`/`kTicksPerBar` travel with `Transport` into
`components/runtime`, then `arranger.hpp` and `chord_sequence.hpp` — files Corelli's own
topology keeps in `arrangrr` — would need a new `#include "runtime/..."`, i.e. **arrangrr
would depend on runtime**. That is exactly backwards: the whole point of the extraction is
that `runtime` drives `arrangrr` through the Stage port, not that the reduced arranger
stage reaches back into the kernel for basic meter math. This is a **physical placement
bug in the literal "byte-for-byte" reading**, not a re-litigation of Corelli's design —
the fix is mechanical and free:

**Pre-move edit (content, not a `git mv` — Nazzareno executes this verbatim edit before
any file move):** hoist the three constexpr lines above out of `transport.hpp` into
`components/arrangrr/include/arrangrr/common/time.hpp`, appended after the existing
`kGridPpqn`/`kTicksPerGridStep` block (`common/time.hpp:17-20`) — same file, same
"musical time constants" header comment (`common/time.hpp:5`) that already owns `kPpqn`,
`kGridPpqn`. Both `arranger.hpp:14` and `chord_sequence.hpp:9` **already** directly
`#include "arrangrr/common/time.hpp"` (arranger.hpp:12, chord_sequence.hpp:6) — so once
the constants live there, both files simply **delete** their `#include
"arrangrr/transport/transport.hpp"` line, with zero replacement include needed.
`engine.hpp`/`engine.cpp` also already include `common/time.hpp` directly (engine.hpp
top-of-file include block) — same zero-new-include outcome once `Transport` itself (not
just the constant) leaves `Engine`.

`Position` (transport.hpp:26-30, bar/beat/tick derived from `Transport`'s own state) has
no consumer outside `Transport`/`Engine` (confirmed by grep: only
`engine.hpp:323`/`test_engine.cpp:135` use it, both engine-adjacent) — it moves with
`Transport` cleanly, no split needed.

---

## 1. New `components/runtime` — target and tree

```
components/runtime/
├── CMakeLists.txt                              NEW
├── include/runtime/
│   ├── transport/transport.hpp                 MOVED (minus the 3 constants, §0)
│   ├── scheduler/out_scheduler.hpp             MOVED byte-for-byte
│   ├── stage.hpp                               NEW  — Stage interface + StageContext (§3.6)
│   └── runtime.hpp                             NEW  — Runtime driver (the relocated
│                                                        advance_ticks loop body lands here)
├── src/                                        (empty for Phase 1 — flag below)
└── tests/
    ├── CMakeLists.txt                          NEW  — runtime_test() function
    ├── test.hpp                                NEW  — duplicate of arrangrr/tests/test.hpp
    ├── test_scheduler.cpp                      MOVED + absorbs test_midi.cpp's 5 scheduler
    │                                            functions (§4, dedup flagged)
    ├── test_engine_fire_order.cpp               MOVED, harness rewritten to Runtime+Stage
    └── test_engine.cpp                         NEW  — the transport/clock-pulse subset
                                                        split out of arrangrr's test_engine.cpp
```

CMake regime for `components/runtime` (mirrors `components/arrangrr/CMakeLists.txt`
exactly — dual-target, no-heap, no external deps):

```cmake
add_library(runtime STATIC
  # Phase 1: header-only candidate (Transport/OutScheduler/Stage/Runtime are all
  # templates/inline classes today) — an empty add_library(... ) with no .cpp is
  # invalid CMake; Nazzareno needs at least one ceremony .cpp (mirroring arrangrr's
  # version.cpp/common_checks.cpp pattern) OR this becomes an INTERFACE library.
  # FLAGGED, not decided here (§6).
)
target_include_directories(runtime PUBLIC include)
target_compile_options(runtime PUBLIC
  -fno-exceptions -fno-rtti -fno-threadsafe-statics
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
```

**Flagged dependency (owner approval required, per my guardrails):** `runtime`'s
`transport.hpp` and `out_scheduler.hpp` both `#include "arrangrr/common/..."` headers
(`time.hpp`, `assert.hpp`, `midi/message.hpp`) — this is preserved from today's file
content, not introduced by me, but it means **`components/runtime`'s CMakeLists must gain
`target_link_libraries(runtime PUBLIC arrangrr)`** (or an equivalent
`target_include_directories` pointing at `components/arrangrr/include`) purely to resolve
those `#include` paths. This is a **new, first-time CMake dependency edge `runtime →
arrangrr`** that did not exist before (today `arrangrr`'s own CMakeLists proudly has
"zero external `target_link_libraries`", and nothing links the reverse direction either).
It does not violate the dual-target contract (arrangrr's `common/` is itself freestanding
and arm-clean) and it is the *sanctioned* direction (runtime consuming arrangrr's
low-level substrate, not arrangrr depending on runtime) — but it is a **new coupling** and
per my mandate I flag it rather than assume it. If the owner wants zero coupling even in
this narrow sense, the alternative is duplicating `Tick`/`BpmX100`/`kPpqn` into a new
runtime-local header — a real cost I do not recommend (duplication of the wire-adjacent
tick type is worse than one clean directed include edge), but the owner's call.

---

## 2. `git mv` list (ordered so the tree never sits in a broken intermediate state)

Order matters: create the new component's skeleton and CMake wiring FIRST (inert, not yet
referenced), then move files into it, then repoint consumers, then delete the vacated
`arrangrr` CMake entries. Every step below assumes the §0 precondition edit already
landed.

```
# Step A — scaffold (no moves yet, just new empty dirs + CMakeLists, SAFE)
mkdir -p components/runtime/include/runtime/transport
mkdir -p components/runtime/include/runtime/scheduler
mkdir -p components/runtime/src
mkdir -p components/runtime/tests
# (components/runtime/CMakeLists.txt, .../tests/CMakeLists.txt: new files, §5)

# Step B — the two byte-for-byte (post-§0-edit) header moves
git mv components/arrangrr/include/arrangrr/transport/transport.hpp \
       components/runtime/include/runtime/transport/transport.hpp
git mv components/arrangrr/include/arrangrr/scheduler/out_scheduler.hpp \
       components/runtime/include/runtime/scheduler/out_scheduler.hpp
# the now-empty components/arrangrr/include/arrangrr/transport/ and .../scheduler/
# directories are removed by git automatically (no tracked files left)

# Step C — test relocations (subject-under-test criterion, §4)
git mv components/arrangrr/tests/test_scheduler.cpp \
       components/runtime/tests/test_scheduler.cpp
git mv components/arrangrr/tests/test_engine_fire_order.cpp \
       components/runtime/tests/test_engine_fire_order.cpp

# Step D — the link-gate file changes HOME (its whole reason to live inside the
# arrangrr library evaporates once Engine sheds Transport/OutScheduler, §6)
git mv components/arrangrr/src/engine_checks.cpp \
       tests/arm-smoke/link_gate.cpp
```

Files that are **NOT** `git mv`'d but split (function-level, not file-level — I name the
destinations, Nazzareno performs the split, §4):

- `components/arrangrr/tests/test_midi.cpp` — 5 scheduler-testing functions extracted
  into `components/runtime/tests/test_scheduler.cpp` (merge, dedup flagged); the
  remainder (parser/router/note-tracker tests) **stays** at
  `components/arrangrr/tests/test_midi.cpp`, dropping its now-unneeded
  `#include "arrangrr/scheduler/out_scheduler.hpp"` (test_midi.cpp:7).
- `components/arrangrr/tests/test_engine.cpp` — SPLIT (§4): 7 functions move to the new
  `components/runtime/tests/test_engine.cpp`; the remaining 8 stay in
  `components/arrangrr/tests/test_engine.cpp`, harness rewritten to the two-object shape.

No files move under `apps/gui-sonotron/` — confirmed zero `m_engine`/`engine()` reference
anywhere in that tree (grep, full tree); its own `AppState::Transport` enum is a local,
unrelated type. D38 purity holds; this migration does not touch the GUI.

---

## 3. New files to create (Nazzareno writes the bodies; this is WHERE, at a header level WHAT)

- **`components/runtime/include/runtime/stage.hpp`** — the `StageContext` struct and the
  `Stage` abstract class exactly as Corelli's §3.6 illustrative shape (not frozen).
  `arrangrr`'s reduced `Engine` (or its renamed successor) implements this interface —
  this is the **one** sanctioned new include arrangrr-side code takes on `runtime/`
  (`#include "runtime/stage.hpp"`), narrower than depending on `runtime/transport.hpp` or
  `runtime/scheduler/out_scheduler.hpp` directly.
- **`components/runtime/include/runtime/runtime.hpp`** — the `Runtime` driver class. Houses
  the relocated `advance_ticks` loop body (today `engine.hpp:185-216`) and owns `Transport
  m_transport` + `OutScheduler<N> m_scheduler`. Drives exactly one `Stage` instance in
  Phase 1 (the arrangrr adapter). **Flagged seam, not mine to carve:** today's `flush()`
  (`engine.hpp:439-444`) pops `m_scheduler` **and** calls `m_tracker.observe(...)` — but
  `NoteTracker` is explicitly listed (Corelli §3.5) as staying **inside** the arrangrr
  stage, not moving to `runtime`. `flush()` as it stands straddles both new components and
  cannot relocate wholesale into `Runtime` without either (a) `Runtime` calling back into
  the Stage on every drained event, or (b) `NoteTracker` observation moving out of
  `flush()` into the Stage's own `on_tick`. This is exactly the "advance_ticks conflates
  both" seam Corelli's own §2.3 names — I flag it prominently rather than resolve it;
  hand to Nazzareno (and re-confirm with Corelli if the resolution isn't a pure carve).
  **Second flagged seam:** `push_command`/`push_midi_in` (staying "as-is" on the arrangrr
  side per the brief) call `schedule_or_warn` → `m_scheduler.schedule(...)` then
  `flush(sink)` (`engine.hpp:180-181`) for immediate-fire commands. If `m_scheduler` no
  longer exists on the reduced Engine/Stage object, this code cannot compile unchanged —
  "kept as-is" cannot mean byte-identical here. The Stage-adapter needs *some* channel back
  to `Runtime`'s scheduler (a reference held by the adapter, an extra `Stage` method, or a
  widened `EventSink`) — an interface decision squarely inside Nazzareno's/Corelli's remit,
  not a file-placement one. I name it so the plan is not silently broken.
- **New `components/runtime/tests/test_engine.cpp`** — receives, verbatim function bodies,
  the 7 transport/clock-pulse-driven functions split out of
  `components/arrangrr/tests/test_engine.cpp` (§4): `test_scheduled_events_fire_on_advance`,
  `test_events_fire_with_stopped_transport`, `test_transport_clock_emission`,
  `test_beat_heartbeat_cadence`, `test_transport_position_and_bounds`,
  `test_clock_on_two_ports_and_continue`, `test_schedule_at_already_due`. Harness becomes
  `Runtime` + the arrangrr Stage adapter (two objects), not a bare `Engine`.
- **`components/runtime/tests/test.hpp`** — a plain duplicate of
  `components/arrangrr/tests/test.hpp` (25 lines, `namespace arrangrr::test`,
  dependency-free `CHECK`/`failures()` harness). Deliberate small duplication, not a shared
  library — consistent with each component's `tests/` being self-contained
  (`project-structure.md`'s own component-local-test placement rule).

---

## 4. Test-relocation table (subject-under-test is the criterion — same rule the
   `arrangrr_test()` three-metric comments in `components/arrangrr/tests/CMakeLists.txt`
   already use: "Engine as harness for ONE producer" stays; "Engine's own cross-producer/
   cross-tick machinery as the subject" moves)

| File | Verdict | Reason |
|---|---|---|
| `test_scheduler.cpp` | **MOVE** → `components/runtime/tests/` | Subject is `OutScheduler` itself, in isolation. |
| `test_engine_fire_order.cpp` | **MOVE** → `components/runtime/tests/` | Subject is the D29 cross-**stage** fire order (`test_chord_seq_resolves_before_arranger_same_tick`, `test_bar_one_downbeat_is_c_root`) — named explicitly by Corelli as "now a genuinely runtime test." Rewrite harness to Runtime+Stage. |
| `test_midi.cpp` | **SPLIT** | Its 5 `test_scheduler_*` functions (lines ~129-160, ~275-296 region) are a lighter, already-overlapping duplicate of `test_scheduler.cpp`'s coverage — merge into `runtime/tests/test_scheduler.cpp`, **flag dedup** for Torquato (do not blindly concatenate near-identical cases). Parser/Router/NoteTracker functions **stay**, drop the now-dead `out_scheduler.hpp` include. |
| `test_engine.cpp` | **SPLIT** | 7 functions (`test_scheduled_events_fire_on_advance`, `test_events_fire_with_stopped_transport`, `test_transport_clock_emission`, `test_beat_heartbeat_cadence`, `test_transport_position_and_bounds`, `test_clock_on_two_ports_and_continue`, `test_schedule_at_already_due`) are genuine Transport/clock/scheduler-drain concerns → **new** `runtime/tests/test_engine.cpp`. The other 8 (`test_thru_note_in_note_out`, `test_engine_arp_setter_guards`, `test_engine_chord_detect_guard_and_nonnote`, `test_panic_via_engine`, `test_warn_on_bad_tempo_and_route`, `test_route_table_full_warns`, `test_input_port_out_of_range_ignored`, `test_warn_on_unknown_command`) exercise the Stage's own command dispatch/warn-code surface → **stay** in `components/arrangrr/tests/test_engine.cpp`, harness rewritten. |
| `test_arp.cpp`, `test_arranger.cpp`, `test_gesture.cpp`, `test_voicing.cpp` | **STAY**, harness rewrite | CMakeLists' own comment: "Engine is only the harness that lets it emit over ticks -> unit." Subject is one producer (Arp/Arranger); harness becomes Runtime+Stage. |
| `test_arp_functional.cpp` | **STAY**, harness rewrite, **flag for re-verification** | Subject is ArpeggiatorEngine's no-stuck-note invariant across transport stop/retrigger — an arrangrr-module correctness property, but exercised through transport-stop and scheduler-retrigger mechanics that are **both** moving to `runtime`. Keep the test in arrangrr, but its assertions should be re-checked once the two-object harness lands (not a pure mechanical rewrite — flagged, not resolved). |
| `test_timeline.cpp`, `test_step_locks.cpp` | **STAY**, harness rewrite | Subject is `Timeline` (an arrangrr module per Corelli §3.5); Engine/Runtime is scaffolding only. |
| `test_chord.cpp`, `test_chord_seq.cpp`, `test_chord_detect.cpp`, `test_chord_follow.cpp`, `test_chord_followed_event.cpp`, `test_followed_context.cpp`, `test_chord_context_diagnosis.cpp`, `test_chord_seq_vs_live_steer.cpp`, `test_harmony_steer.cpp`, `test_input_zone.cpp` | **STAY**, harness rewrite | All exercise the D47 `FollowedContext` gate / chord-producer arbitration — an **intra-arrangrr** concern (which producer wins the chord for the stage's own harmonic decision), not the Runtime's cross-stage fire order. Distinct from `test_engine_fire_order.cpp` (which is about the ORDER stages fire in, not which producer wins inside one stage). |
| `test_abi_frozen.cpp`, `test_common.cpp` | **STAY, untouched** | Neither constructs `Engine` nor touches `Transport`/`OutScheduler`. The ABI waiver is explicitly NOT spent in Phase 1. |

**`components/arrangrr/tests/CMakeLists.txt`** edit: remove the two `arrangrr_test(...)`
lines for `test_scheduler`/`test_engine_fire_order`; every other `arrangrr_test(...)` line
is unchanged (same names, same categories — only their `.cpp` bodies get a harness
rewrite, which is content, not placement).

**New `components/runtime/tests/CMakeLists.txt`**:

```cmake
function(runtime_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE runtime arrangrr)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

runtime_test(test_scheduler unit)
runtime_test(test_engine_fire_order functional)
runtime_test(test_engine functional)
```

Note `target_link_libraries(${name} PRIVATE runtime arrangrr)` — `test_engine_fire_order`
and the new `test_engine.cpp` instantiate the arrangrr Stage adapter too, so both libs are
needed; `test_scheduler` alone could link `runtime` only (a leaner variant is fine, flagged
as a minor optimization Nazzareno may take or leave).

---

## 5. `components/runtime/CMakeLists.txt` and the root `CMakeLists.txt` delta

Root `CMakeLists.txt` (today: `add_subdirectory(components/arrangrr)` unconditional, then
branches on `ARRANGRR_FIRMWARE`):

```cmake
add_subdirectory(components/arrangrr)
add_subdirectory(components/runtime)          # NEW — unconditional: both the arm-none-eabi
                                               # branch and the host branch need it (dual-target)

if(ARRANGRR_FIRMWARE)
  add_subdirectory(tests/arm-smoke)
else()
  enable_testing()
  add_subdirectory(components/hostrt)
  add_subdirectory(apps/tools/cli-arrangrr)
  add_subdirectory(components/arrangrr/tests)
  add_subdirectory(components/runtime/tests)   # NEW
  add_subdirectory(components/hostrt/tests)
  add_subdirectory(tests/golden)
  add_subdirectory(tests/integration)
  add_subdirectory(apps/tools/arrstyle-converter)
  add_subdirectory(third_party)
  add_subdirectory(apps/gui-sonotron)
endif()
```

`components/arrangrr/CMakeLists.txt` edit: remove `src/engine_checks.cpp` from the
`add_library(arrangrr STATIC ...)` sources list (that TU relocates to `tests/arm-smoke/`,
§6) — nothing else in this file changes; `arrangrr` keeps its "zero external
`target_link_libraries`" property.

`components/hostrt/CMakeLists.txt` edit:

```cmake
target_link_libraries(hostrt PUBLIC arrangrr runtime ALSA::ALSA)   # was: arrangrr ALSA::ALSA
```

`apps/tools/cli-arrangrr/CMakeLists.txt`: **no edit needed** — it links `hostrt` `PRIVATE`,
and `hostrt`'s `PUBLIC` link to `runtime` propagates transitively.

`tests/arm-smoke/CMakeLists.txt` edit:

```cmake
add_executable(firmware_stub main.cpp link_gate.cpp)    # was: main.cpp only
target_link_libraries(firmware_stub PRIVATE arrangrr runtime)   # was: arrangrr only
```

(`link_gate.cpp` is `engine_checks.cpp` relocated, §6 — its body must be rewritten by
Nazzareno to instantiate `runtime::Runtime` + the arrangrr Stage adapter together instead
of a bare `arrangrr::Engine`; I name the new home, not the new body.)

---

## 6. Why `engine_checks.cpp` changes HOME, not just content

`components/arrangrr/src/engine_checks.cpp` is compiled today straight into the
`arrangrr` static library (`components/arrangrr/CMakeLists.txt`'s `add_library(arrangrr
STATIC ... src/engine_checks.cpp)`). Its only job is the freestanding link-gate: "pull the
WHOLE core through the freestanding linker" (`tests/arm-smoke/main.cpp:14-16`'s own
comment). Once `Engine` sheds `Transport`/`OutScheduler`/the fire-loop, proving "the whole
runtime cross-builds and links" **requires instantiating both `runtime::Runtime` and the
arrangrr Stage together** — which means this TU needs to link `runtime`, a dependency
`arrangrr`-the-library must never carry (it would contaminate every consumer of
`libarrangrr.a`, host and firmware alike, with a `runtime` link even when they only want
the arranger). The file's *reason to exist* was always "the cross-component freestanding
gate," and `tests/arm-smoke/` is precisely the place `project-structure.md` already
sanctions for that role (today's own comment: "a freestanding image that pulls in the
core" — tomorrow, both components). This is a **wrong-target-directory finding**, not new
content, hence a `git mv` (Step D, §2) with a rename to `link_gate.cpp`, plus the CMake
edit in §5.

---

## 7. The `hostrt::Shell::advance_to` call-site and the `.transport()` accessor blast radius

Corelli's brief names one call-site: `Shell::advance_to`. Verified: it is actually **two**
lines inside that same function/region, both in `components/hostrt/shell.cpp`:

```
components/hostrt/shell.cpp:456   m_engine.advance_ticks(static_cast<std::uint32_t>(at - m_engine.now()), m_sink);
components/hostrt/shell.cpp:465   m_engine.advance_ticks(static_cast<std::uint32_t>(target - m_engine.now()), m_sink);
```

Both retarget to `m_runtime.advance_ticks(...)`. **`Shell` gains a new member** (`Runtime
m_runtime;` alongside its existing `Engine m_engine;`, `shell.hpp:380`) and a new accessor
(`Runtime& runtime()`), mirroring the existing `Engine& engine()` (`shell.hpp:245-246`) —
a Shell-internals decision for Nazzareno, named here only for its physical consequence:
**`components/hostrt/shell.hpp` and `shell.cpp` now `#include "runtime/runtime.hpp"`**, a
new include this component did not have before (sanctioned — `hostrt` already depends on
`arrangrr`, and `hostrt` is host-only so gaining a `runtime` include is unremarkable
compared to the `arrangrr↔runtime` question in §1).

Beyond the two `advance_ticks` lines, `Transport` moving out from under `Engine` breaks
every `.transport()` call currently reached through `Engine`, because `Transport` no
longer lives there once it moves to `Runtime`. Verified call sites (grep, full tree):

```
components/hostrt/shell_io_commands.cpp:377   m_engine.transport().bpm()
components/hostrt/shell_io_commands.cpp:396   m_engine.transport().bpm()
components/hostrt/shell_input.cpp:445          m_engine.transport().playing()
components/hostrt/shell.cpp:134                m_engine.transport().playing()  (chord_active gate)
```

All four become `m_runtime.transport()...`. And in tests:

```
components/hostrt/tests/test_host.cpp:146,148,168,174,181,250,1251,1254,1256,1262,1554,1577
  — 13 sites of `f.shell.engine().transport()...` → `f.shell.runtime().transport()...`
```

`components/hostrt/jsonl.cpp:8` includes `transport.hpp` **only** for the `TransportState`
enum (used in a local `transport_name()` switch, `jsonl.cpp:28-32`) — since `TransportState`
moves with `Transport` to `runtime`, this include repoints to
`"runtime/transport/transport.hpp"` (one-line path edit, no logic change). Same for
`components/hostrt/tests/test_host.cpp:17`.

`apps/gui-sonotron` — **zero impact**, confirmed (§2): its own `AppState::Transport` is an
unrelated local enum; `apps/gui-sonotron/CMakeLists.txt` links neither `arrangrr` nor
`hostrt` nor (now) `runtime`.

---

## 8. Namespace fork (flagged, not resolved)

`components/hostrt` already lives in `namespace arrangrr::host` (verified:
`shell.hpp:24`, `jsonl.hpp:10`, `jsonl.cpp:11`) — i.e. today's host layer is nested
*inside* `arrangrr`'s own namespace. This matters for where `Transport`/`OutScheduler`
land namespace-wise once they physically move to a *different* CMake component:

- **Option A — keep `namespace arrangrr`** for the moved `Transport`/`OutScheduler` (and
  the new `Stage`/`Runtime`, if desired). Zero find-and-replace for any consumer
  (`arrangrr::Transport`, `arrangrr::TransportState`, `arrangrr::OutScheduler<N>` all keep
  compiling unchanged) — minimal churn, matching Phase 1's own "no ABI reshape yet"
  spirit. **Cost:** a component named `runtime` whose public symbols live in a different
  component's (`arrangrr`'s) namespace is a name/location mismatch a future reader will
  trip on — physically two components, logically one namespace.
- **Option B — `namespace runtime`** for the moved types. Symbol names finally match their
  physical/CMake home. **Cost:** every one of the ~13 include sites in §7 plus every
  `Transport`/`OutScheduler<N>` reference across ~15 test files must also gain a qualifier
  edit (`arrangrr::Transport` → `runtime::Transport`), multiplying this phase's diff for a
  purely cosmetic (if honest) gain.

**My lean:** Option A for Phase 1 (minimize churn, consistent with the ABI-waiver-not-spent
discipline), with an explicit TODO for a later phase (once the Stage port stabilizes and a
second stage/consumer exists) to rename to `namespace runtime` in one dedicated pass. This
is a genuine physical-structure fork — I flag it for the owner rather than deciding it
unilaterally, since it trades off "component boundaries should read true" (my instinct)
against "minimize this phase's blast radius" (the design brief's own instinct).

---

## 9. Sub-folder fork (flagged)

Should `components/runtime/include/runtime/` mirror `arrangrr`'s convention of a
sub-folder per module (`runtime/transport/transport.hpp`, `runtime/scheduler/
out_scheduler.hpp`, as I've laid the tree out above), or go flat (`runtime/transport.hpp`,
`runtime/out_scheduler.hpp`) since the component starts with only two headers (plus
`stage.hpp`/`runtime.hpp` at the top level either way)? Arrangrr's sub-folder convention
earns its keep with 10+ modules; with 2, a flat layout is equally legible and slightly
less ceremony. **My lean:** flat for Phase 1 (`runtime/transport.hpp`,
`runtime/out_scheduler.hpp`) — revisit if/when a 3rd or 4th runtime-owned module appears
(e.g. if `chorddet`, §6 of Corelli's open forks, ever lands inside `runtime` rather than
as its own component). Flagged for the owner; I have laid out the tree above with
sub-folders as the more conservative (mirrors-precedent) default, but flat is my actual
recommendation.

---

## 10. Coverage-gate scope (must extend, or the kernel goes silently uncovered)

`scripts/coverage.sh` enforces the CORE gate on `components/arrangrr/` only
(`project-structure.md`'s own documented decision). Exact lines to extend:

```
scripts/coverage.sh:6    doc comment: "core (components/arrangrr/)" → "core
                          (components/arrangrr/ + components/runtime/)"
scripts/coverage.sh:18   doc comment: same addition, mention runtime's own .gcda tree
scripts/coverage.sh:53-61  CORE_ARGS array:
    --filter 'components/arrangrr/'         (existing)
  + --filter 'components/runtime/'          (NEW)
    --exclude 'components/arrangrr/tests/'  (existing)
  + --exclude 'components/runtime/tests/'   (NEW)
    --exclude 'tests/'                      (existing, unchanged — already covers both)
scripts/coverage.sh:94-99  the report-only extended view (arrangrr+hostrt) — add a third
                            --filter 'components/runtime/' alongside, symmetric treatment
scripts/coverage.sh:133,163  summary echo text: "core scope: components/arrangrr/" →
                            "core scope: components/arrangrr/ + components/runtime/"
```

Without this edit, `components/runtime`'s new templates (`Transport`, `OutScheduler`,
`Runtime`, `Stage`) would be silently exempt from the 80% unit-branch gate the moment they
physically leave `components/arrangrr/` — the exact "kernel goes uncovered" risk the task
brief calls out.

---

## 11. Phase-1 green-gate checklist (handoff, not something I ran)

- [ ] §0's pre-move constant hoist lands and `arrangrr` alone still builds/links host +
      arm-none-eabi with zero new includes in `arranger.hpp`/`chord_sequence.hpp`.
- [ ] All 18 `tests/golden/*.golden` files diff byte-identical against their `.acmd`
      scripts through `cli_arrangrr` (target name **unchanged** in Phase 1, per the brief).
- [ ] `components/arrangrr/tests/test_abi_frozen.cpp` compiles and passes **unedited**
      (the waiver is available, not spent — confirms `abi.hpp`'s shape truly didn't move).
- [ ] `tests/arm-smoke` cross-builds `runtime` + `arrangrr` together for `arm-none-eabi`
      and link-succeeds via the relocated `link_gate.cpp` (§6) — the smoke test's claim
      becomes "the runtime kernel + the arranger stage link freestanding together."
- [ ] Host `ctest` green across all labels (`unit`/`functional`/`regression`), including
      the new `components/runtime/tests/` binaries and every rewritten
      `components/arrangrr/tests/*.cpp` harness.
- [ ] `scripts/coverage.sh`'s CORE gate (§10) still enforces ≥80% unit branch coverage,
      now over `components/arrangrr/ + components/runtime/` combined — not silently
      narrowed or widened.
- [ ] No `#include "arrangrr/transport/..."` or `#include "arrangrr/scheduler/..."` string
      survives anywhere in the tree (grep-clean) — every consumer repoints to `runtime/`.
- [ ] `git grep -n 'm_engine\.transport\|m_engine\.advance_ticks'` returns empty in
      `components/hostrt/` (every site in §7 retargeted).

---

## 12. Open physical-structure forks for the owner (I do not resolve these)

1. **Namespace** (§8): keep `namespace arrangrr` for the moved `Transport`/`OutScheduler`
   (my lean, minimal churn) vs rename to `namespace runtime` now (honest, costlier).
2. **Sub-folder vs flat** under `runtime/include/runtime/` (§9): my lean is flat for two
   modules; the tree in §1 shows the sub-folder variant as the conservative default.
3. **Target name**: `runtime` (as Corelli named it and I've used throughout) vs
   `arrangrr_runtime` (would read consistently beside `arrangrr`/`hostrt`/`cli_arrangrr`'s
   existing naming, at the cost of a longer, slightly redundant name given the CMake
   target already lives in `components/runtime/`). I default to `runtime` per the owner's
   already-made decision (per the task brief: "owner chose this name") — flagging only
   that a target-name/directory-name split (`runtime` the CMake target vs
   `components/runtime` the directory) is the same pattern `arrangrr`/`arrangrr_core`
   already used before the 2026-07-07 restructure renamed the target to match the
   directory (`project-structure.md`'s own "Naming ... resolved by the restructure"
   section) — worth the same convergence here from day one rather than a later rename.
4. **`runtime`'s own `src/`**: header-only (all four new/moved headers are templates or
   inline classes) means `add_library(runtime STATIC)` needs at least one `.cpp` to be
   valid CMake (mirroring `arrangrr`'s `version.cpp`/`common_checks.cpp` ceremony
   pattern), or `runtime` should be declared `INTERFACE` instead of `STATIC`. This is a
   Nazzareno-level CMake-mechanics call I flag but do not make.
5. **The `runtime → arrangrr` CMake link** (§1): needed only so `transport.hpp`/
   `out_scheduler.hpp` can resolve `#include "arrangrr/common/..."`. Flagged for explicit
   owner sign-off since it is a first-time coupling in that direction; the alternative
   (duplicate the tick/tempo primitives into a runtime-local header) is worse, in my view,
   but is the owner's tradeoff to make, not mine.
6. **The `flush()`/`schedule_or_warn` seam** (§3): `push_command`/`push_midi_in` staying
   "as-is" cannot literally mean unchanged once the scheduler they call into moves off the
   object they're methods of. This is a logical-interface question (how the Stage reaches
   the Runtime-owned scheduler for immediate-fire commands) that I hand to Nazzareno and
   flag for Corelli's awareness — it is not a file-placement question, but a placement
   plan that ignored it would be dishonest about the blast radius.

# Move-plan: components/{core,platform} two-tier regime split

Status: PROPOSAL — read-only move-plan, not executed. Physical layout only
(directory/target/file placement, CMakeLists edits, include-path edits the
moves force). Palladio does not judge coupling/ABI/abstraction quality here,
does not design the `ISoundEngine` contract, and does not run any `git mv`.
This is a handoff for a human or `giotto-cpp-implementor`.

Owner-locked target (verbatim from the brief, not re-opened here):

```
components/
  core/       # platform-AGNOSTIC, freestanding-capable (compiles on STM32 M7)
    common/ runtime/ chorddet/ arrangrr/
  platform/   # platform-DEPENDENT, needs a hosted OS (FLAT)
    midisrc/ orchestrator/ hostrt/
    audio/                      # miniaudio device layer, promoted OUT of the GUI
    engines/soundfont-melodd/   # folds today's melodd/TSF (HYPHEN, not parenthetical)
    engines/physical/
    engines/analog/
```

---

## 0. FLAG BEFORE ANYTHING ELSE: this reverses a documented decision

`docs/architecture.md:29,47-56` (§3 "Componentization principles") currently
reads:

> **`components/` is flat; the target boundary is a *contract*, not a
> folder.** We do not split `components/` into `core/` vs `host/` sub-trees
> — that would force the same axis into every top-level dir and read as
> asymmetric. Instead each component **declares its target-regime**
> (freestanding/STM32-capable vs host-only) in its own CMake. […] A target
> specialization is a **sibling component**, created only when needed (e.g.
> `arrangrr-arm64/` beside `arrangrr/`), never a pervasive folder split.

This is the *opposite* of the two-tier `core/`/`platform/` regime this brief
locks. I am treating the brief as the current owner instruction and
proposing the move accordingly, but I did not write that sentence and I am
not the one who reversed it — **confirm with the owner that `docs/
architecture.md` §2/§3 is meant to be rewritten to match** (out of my
write-scope: I persist only this one proposal doc). If the reversal is not
actually intended, stop before step 1 below. This is the single highest-value
verification gate in this whole plan, cheaper to close now than after 8
directories move.

Interesting: §3's own escape hatch ("a target specialization is a sibling
component … e.g. `arrangrr-arm64/`") already establishes a **hyphenated
sibling-naming precedent** in this codebase, which is consistent with the
brief's `engines/soundfont-melodd` (hyphen, not parenthetical) — so the
*naming convention* the brief asks for is not foreign to the project, even
though the *tiering* it implies is the reversal above.

---

## 1. The actual current tree (mapped, cited)

Real, flat `components/` (no `core/`/`platform/` split exists today):

| Path | Target(s) | Regime (from its own CMakeLists) |
|---|---|---|
| `components/common/` (`include/common/{assert.hpp,time.hpp,midi/message.hpp}`) | `common` (INTERFACE) | dual-target, unconditional (`CMakeLists.txt:45`) |
| `components/runtime/` (`include/runtime/*.hpp`, `tests/`) | `runtime` (INTERFACE) | dual-target, unconditional (`CMakeLists.txt:46`) |
| `components/chorddet/` (`include/chorddet/*.hpp`) | `chorddet` (INTERFACE) | dual-target, unconditional (`CMakeLists.txt:51`) |
| `components/arrangrr/` (`include/arrangrr/**`, `src/*.cpp`, `tests/`) | `arrangrr` (STATIC) | dual-target, unconditional (`CMakeLists.txt:52`) |
| `components/midisrc/` (`include/midisrc/*.hpp`, `src/*.cpp`, `tests/`, `fuzz/`) | `midisrc` (STATIC) | HOST-ONLY, `else()` branch (`CMakeLists.txt:66`) |
| `components/orchestrator/` (`include/orchestrator/accompany.hpp`) | `orchestrator` (INTERFACE) | HOST-ONLY (`CMakeLists.txt:67`) |
| `components/hostrt/` (~40 flat `.cpp`/`.hpp`, `tests/`) | `hostrt` (STATIC) | HOST-ONLY (`CMakeLists.txt:68`) |
| `components/melodd/` (`include/melodd/*.hpp`, `src/*.cpp`, `tests/`) | `melodd` (STATIC) | HOST-ONLY, ordered before `apps/gui-sonotron` (`CMakeLists.txt:95-96`) |
| `components/samplrr/` | none — only `README.md`, **not** `add_subdirectory`'d anywhere | placeholder, unwired |

`gui_sonotron_audio` today is defined *inside* `apps/gui-sonotron/CMakeLists.txt:112-127`
(not its own component), backed by:
- `apps/gui-sonotron/src/audio_engine.{hpp,cpp}`
- `apps/gui-sonotron/src/audio_midi_event.hpp` (shared POD, also consumed by `gui_sonotron_engine`)
- `apps/gui-sonotron/src/spsc_ring.hpp` (the ring template, also consumed by `gui_sonotron_engine` via its own `gui_sonotron_ring` INTERFACE target, `CMakeLists.txt:28-29`)

Top-level wiring: `CMakeLists.txt:45-100` (`add_subdirectory` chain, both the
`ARRANGRR_FIRMWARE` branch and the host `else()` branch).

**A load-bearing fact that makes most of this move mechanically cheap:** I
grepped every `#include` in the tree for a literal `components/` path
(`grep -rn '#include.*"components/'`) — **zero hits**. Every component
reaches its own headers through `target_include_directories(X PUBLIC
include)` resolved relative to `CMAKE_CURRENT_SOURCE_DIR`, and every
consumer includes the *target's* public prefix (`"melodd/synth.hpp"`,
`"runtime/stage.hpp"`, `"arrangrr/…"`, `"chorddet/…"`, `"midisrc/…"`,
`"orchestrator/…"`, `"common/…"`), never a repo-rooted path. **Moving a
whole component directory one level deeper under `core/` or `platform/`
therefore requires editing zero `#include` lines anywhere in the tree** —
only the `add_subdirectory(...)` call sites and the few hard-coded absolute
paths enumerated in §3 below.

---

## 2. Diagnosi di collocazione (mismatches against the locked target)

- **No regime tiering exists on disk today** — `components/` is flat, matching
  the *current* `docs/architecture.md` doctrine (§0 above), not the newly
  locked one. Every core-vs-host distinction today lives only in each
  component's own `CMakeLists.txt` comment + which branch of the top-level
  `if(ARRANGRR_FIRMWARE)` adds it — exactly what §3 of that doc calls a
  "contract, not a folder." The brief asks to make it a folder too.
- **`apps/gui-sonotron/src/audio_engine.{hpp,cpp}` is host-only, arrangrr-free
  library code masquerading as app source** — confirmed by its own
  `CMakeLists.txt:112-123` comment ("HOST-ONLY, arrangrr-free… deliberately
  NOT hostrt/arrangrr/runtime"). It has no ImGui/GLFW coupling (verified: its
  own `#include` list is `<mutex> <string> <miniaudio.h> "audio_midi_event.hpp"
  "melodd/synth.hpp"` — nothing from `third_party/imgui` or `glfw`). Textbook
  "wrong target directory," already flagged once before (see §5).
- **`apps/gui-sonotron/src/audio_midi_event.hpp` and `spsc_ring.hpp` are
  *shared* between two libraries that must NOT depend on each other** — both
  are `#include`d directly by `apps/gui-sonotron/src/in_process_brain_session.{hpp,cpp}`
  (part of `gui_sonotron_engine`, staying in the app) **and** by
  `apps/gui-sonotron/src/audio_engine.{hpp,cpp}` (moving out). If only
  `audio_engine.{hpp,cpp}` moved and these two headers stayed in
  `apps/gui-sonotron/src/`, the new `components/platform/audio` library would
  have to reach back **into an app's private `src/`** for headers — components
  must never depend on apps. These two headers have to move together with
  `audio_engine.{hpp,cpp}` for the dependency direction to stay sane. This is
  a correction to the brief's literal one-line KEY FACT ("audio_engine.{hpp,cpp}
  … move"); the mechanics force two more files along.
- **The brief's "3 tests `test_audio_*.cpp` move" is only 1/3 accurate** —
  I read all three:
  - `test_audio_engine_smoke.cpp` links `gui_sonotron_audio` directly
    (`apps/gui-sonotron/tests/CMakeLists.txt:93-94`) and includes
    `"src/audio_engine.hpp"` + `"src/audio_midi_event.hpp"` — this is the one
    that actually tests `AudioEngine`. **Moves.**
  - `test_audio_port_gate.cpp` and `test_audio_primary_port_reachable.cpp`
    both link `gui_sonotron_engine` (`CMakeLists.txt:83,108`), both
    `#include "src/in_process_brain_session.hpp"` and only reach
    `"src/audio_midi_event.hpp"` as the shared POD type to inspect ring
    contents (verified: neither includes `"src/audio_engine.hpp"`). They test
    `InProcessBrainSession`'s OutEvent→AudioMidiEvent port filter, not
    `AudioEngine`. **Stay** in `apps/gui-sonotron/tests/`; only their
    `audio_midi_event.hpp` include path needs updating.
  (Note: a prior, more granular proposal — `docs/proposals/
  audio-engines-family-layout.md` §6 — already found and recorded this exact
  same 1-of-3 correction independently. Two independent passes agreeing is
  good evidence this is real, not a nitpick.)
- **`components/samplrr` doesn't fit the locked list at all.** It is an
  unwired, source-free placeholder (`README.md` only: "Host-only sampler
  engine"), documented in `docs/architecture.md:37,133,540` as a future
  audio-source **slot**, conceptually a sibling of the very
  `soundfont-melodd`/`physical`/`analog` engines this brief lists — but the
  brief never names it. Left as bare `components/samplrr/` after the move it
  would be the one component sitting outside the two-tier axis entirely,
  which is exactly the "no per-file exception" the owner just locked against.
  Flagged in §7, not silently resolved.
- **One hard-coded absolute component path already exists and will break
  silently if missed**: `components/melodd/CMakeLists.txt:28`
  `target_include_directories(melodd PUBLIC ${CMAKE_SOURCE_DIR}/components/common/include)`
  — the ONE place in the whole tree that reaches another component by a
  repo-rooted path instead of `target_link_libraries`. Its own comment
  explains why (avoiding `common`'s INTERFACE `-fno-exceptions/-fno-rtti`
  leaking onto melodd's TUs). This line must track both moves at once
  (`melodd` → `engines/soundfont-melodd`, `common` → `core/common`).
- **Two more hard-coded absolute paths, both to `midisrc`'s test fixture**:
  `apps/gui-sonotron/tests/CMakeLists.txt:74,85`
  (`GUI_SONOTRON_TEST_MIDI_FIXTURE="${CMAKE_SOURCE_DIR}/components/midisrc/tests/fixtures/tiny.mid"`).
- **Three golden scripts hard-code the same fixture path as a literal command
  argument, not a CMake variable** — `tests/golden/accompany_basic.acmd:13`,
  `accompany_melody_detect.acmd:27`, `accompany_restyle.acmd:30`, each:
  `midi-source load components/midisrc/tests/fixtures/tiny.mid` (resolved
  against `WORKDIR=${CMAKE_SOURCE_DIR}`, `tests/golden/CMakeLists.txt`'s
  `accompany_golden` function).
- **`scripts/coverage.sh` hard-codes `components/arrangrr/`, `components/
  runtime/`, `components/hostrt/` eight times** (lines 6-8, 23, 28, 58-59,
  64-68, 105-112, 146, 182) as `gcovr --filter`/`--exclude` arguments. These
  are not cosmetic: if left stale, `gcovr` filters on paths that contain zero
  files post-move and silently reports empty/zero coverage — a silent gate
  failure disguised as a passing (or trivially-0%) run, worse than a build
  break because nothing turns red.
- **`scripts/lint.sh:16` does *not* need editing** — I verified empirically
  (`git ls-files -co --exclude-standard 'components/**/*.cpp'` against a
  synthetic two-level-deep tree in a scratch repo) that git's pathspec `**`
  already matches arbitrarily nested paths (`components/core/common/foo.cpp`
  and `components/platform/hostrt/bar.cpp` both matched). No edit forced.
- **`tests/arm-smoke/CMakeLists.txt:3` and `tests/arm-smoke/link_gate.cpp`**
  carry a historical-comment mention of `components/arrangrr/src/
  engine_checks.cpp` — prose only (no live path used by the build), cosmetic,
  not blocking.
- **~86 files under `docs/`, `README.md`, and in-tree comments** mention a
  `components/<name>` path in prose (measured via
  `grep -rl` across `docs/ README.md apps/tools apps/gui-sonotron components`).
  The highest-value ones to refresh are `docs/architecture.md` (§2 tree,
  already flagged in §0) and the top-level `README.md:12,16,57`. This is
  documentation debt, not a build gate; I am not enumerating all 86 here (out
  of proportion for this move-plan) — whoever executes should `grep -rl` the
  same pattern afterward and sweep prose in a follow-up pass, not block the
  build-verification gate on it.

---

## 3. Piano di spostamento (ordered)

### Step 0 — scaffolding (SAFE, no git tracked until files land)

```
mkdir -p components/core components/platform/engines
```

### Step 1 — the four dual-target core components (SAFE, directory-level git mv)

```
git mv components/common       components/core/common
git mv components/runtime      components/core/runtime
git mv components/chorddet     components/core/chorddet
git mv components/arrangrr     components/core/arrangrr
```
No file inside any of these four needs an include edit (see §1's zero-hits
finding). `components/core/arrangrr/tests/` and `components/core/runtime/tests/`
move with their parent, no separate step.

### Step 2 — the three host-only platform components (SAFE, directory-level git mv)

```
git mv components/midisrc      components/platform/midisrc
git mv components/orchestrator components/platform/orchestrator
git mv components/hostrt       components/platform/hostrt
```
`components/platform/hostrt/tests/` moves with its parent.

### Step 3 — melodd → engines/soundfont-melodd (NEEDS-BUILD-EDIT: one absolute path)

```
git mv components/melodd components/platform/engines/soundfont-melodd
```
Then edit `components/platform/engines/soundfont-melodd/CMakeLists.txt:28`:
```diff
- target_include_directories(melodd PUBLIC ${CMAKE_SOURCE_DIR}/components/common/include)
+ target_include_directories(melodd PUBLIC ${CMAKE_SOURCE_DIR}/components/core/common/include)
```
**Naming decision I am flagging, not deciding:** the brief asks for the
*directory* `engines/soundfont-melodd` but says nothing about the CMake
*target* name. I recommend **keeping the target name `melodd` unchanged**
(zero edits to `apps/tools/melodd/CMakeLists.txt:12`'s
`target_link_libraries(melodd_bin PRIVATE melodd …)`, zero edits to
`components/platform/engines/soundfont-melodd/tests/CMakeLists.txt`'s
`melodd_test` function, zero edits to the new `components/platform/audio`'s
link line below) — this mirrors the project's own existing precedent of
directory-name (hyphenated, e.g. `apps/gui-sonotron`) vs. target-name
(underscored, e.g. `gui_sonotron`) divergence. Renaming the target too (e.g.
to `soundfont_melodd`) is a legitimate alternative with real blast radius
across those same three files — **NEEDS-DECISION** if the owner wants it.

### Step 4 — new placeholder engine slots (SAFE, new files only, nothing to move)

```
mkdir -p components/platform/engines/physical components/platform/engines/analog
```
Write `README.md` in each (mirrors `components/samplrr/README.md`'s own
one-line "SLOT — no code yet" shape) + a stub `CMakeLists.txt` with no
`add_library` yet (mirrors melodd's own pre-Phase-5 empty-slot state per
`docs/architecture.md:37`). These two are **new files**, not moves — nothing
in the current tree becomes `physical` or `analog` content.

### Step 5 — promote the audio device layer out of the GUI (NEEDS-BUILD-EDIT)

```
mkdir -p components/platform/audio/include/audio components/platform/audio/src components/platform/audio/tests

git mv apps/gui-sonotron/src/spsc_ring.hpp         components/platform/audio/include/audio/spsc_ring.hpp
git mv apps/gui-sonotron/src/audio_midi_event.hpp  components/platform/audio/include/audio/audio_midi_event.hpp
git mv apps/gui-sonotron/src/audio_engine.hpp      components/platform/audio/include/audio/audio_engine.hpp
git mv apps/gui-sonotron/src/audio_engine.cpp      components/platform/audio/src/audio_engine.cpp
git mv apps/gui-sonotron/tests/test_spsc_ring.cpp         components/platform/audio/tests/test_spsc_ring.cpp
git mv apps/gui-sonotron/tests/test_audio_engine_smoke.cpp components/platform/audio/tests/test_audio_engine_smoke.cpp
```
`test_audio_port_gate.cpp` and `test_audio_primary_port_reachable.cpp`
**do NOT move** (see §2's correction).

**Include edits this forces** (mechanical, one line each, verified by reading
every file's actual `#include`s — following the project's own convention,
confirmed by reading `components/melodd/tests/test_synth_smoke.cpp`, that
even a component's *own* tests include its public headers through the
`<component>/name.hpp` prefix, not a bare relative name):

| File (new path) | Before | After |
|---|---|---|
| `components/platform/audio/include/audio/audio_engine.hpp` | `#include "audio_midi_event.hpp"` | `#include "audio/audio_midi_event.hpp"` |
| `components/platform/audio/src/audio_engine.cpp` | `#include "audio_engine.hpp"` | `#include "audio/audio_engine.hpp"` |
| `components/platform/audio/include/audio/audio_midi_event.hpp` | `#include "spsc_ring.hpp"` | `#include "audio/spsc_ring.hpp"` |
| `components/platform/audio/tests/test_audio_engine_smoke.cpp` | `#include "src/audio_engine.hpp"` / `#include "src/audio_midi_event.hpp"` | `#include "audio/audio_engine.hpp"` / `#include "audio/audio_midi_event.hpp"` |
| `components/platform/audio/tests/test_spsc_ring.cpp` | `#include "src/spsc_ring.hpp"` | `#include "audio/spsc_ring.hpp"` |
| `apps/gui-sonotron/main.cpp:63` | `#include "src/audio_engine.hpp"` | `#include "audio/audio_engine.hpp"` |
| `apps/gui-sonotron/src/in_process_brain_session.hpp:8` | `#include "audio_midi_event.hpp"` | `#include "audio/audio_midi_event.hpp"` |
| `apps/gui-sonotron/src/in_process_brain_session.cpp:17` | `#include "spsc_ring.hpp"` | `#include "audio/spsc_ring.hpp"` |
| `apps/gui-sonotron/tests/test_audio_port_gate.cpp:35` | `#include "src/audio_midi_event.hpp"` | `#include "audio/audio_midi_event.hpp"` |
| `apps/gui-sonotron/tests/test_audio_primary_port_reachable.cpp:25` | `#include "src/audio_midi_event.hpp"` | `#include "audio/audio_midi_event.hpp"` |

`audio_engine.hpp`'s `#include "melodd/synth.hpp"` and `audio_engine.cpp`'s
`#include "melodd/dispatch.hpp"` and `audio_midi_event.hpp`'s `#include
"common/midi/message.hpp"` are **unchanged** — those components keep their
own public prefix regardless of which tier moved them.

**New `components/platform/audio/CMakeLists.txt`** (new file — content, not
placement, but shown here because it is exactly what the move forces
mechanically, mirroring `components/platform/engines/soundfont-melodd`'s own
precedent for reaching `common` by raw include path rather than
`target_link_libraries`):

```cmake
find_package(Threads REQUIRED)

# audio_ring: the SPSC ring template + the shared AudioMidiEvent POD, split
# into its own light INTERFACE target so gui_sonotron_engine (staying under
# apps/gui-sonotron, never touching audio hardware) can reach the shared
# header without linking melodd/miniaudio.
add_library(audio_ring INTERFACE)
target_include_directories(audio_ring INTERFACE include)
target_include_directories(audio_ring INTERFACE
  ${CMAKE_SOURCE_DIR}/components/core/common/include)

add_library(audio STATIC src/audio_engine.cpp)
target_include_directories(audio PUBLIC include)
target_link_libraries(audio PUBLIC audio_ring melodd miniaudio Threads::Threads)
target_compile_options(audio PRIVATE -Wall -Wextra -Werror)

add_subdirectory(tests)
```

**New `components/platform/audio/tests/CMakeLists.txt`** (labels/behavior
copied verbatim from the two registrations being removed from
`apps/gui-sonotron/tests/CMakeLists.txt`):

```cmake
add_executable(test_spsc_ring test_spsc_ring.cpp)
target_link_libraries(test_spsc_ring PRIVATE audio_ring Threads::Threads)
target_compile_options(test_spsc_ring PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_spsc_ring COMMAND test_spsc_ring)
set_tests_properties(test_spsc_ring PROPERTIES LABELS unit)

add_executable(test_audio_engine_smoke test_audio_engine_smoke.cpp)
target_link_libraries(test_audio_engine_smoke PRIVATE audio)
target_compile_options(test_audio_engine_smoke PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_audio_engine_smoke COMMAND test_audio_engine_smoke)
set_tests_properties(test_audio_engine_smoke PROPERTIES LABELS functional)
```

**`apps/gui-sonotron/CMakeLists.txt` edits:**
- DELETE the `gui_sonotron_ring` INTERFACE block (lines 20-29, keep the
  `find_package(Threads REQUIRED)` on line 20 — `gui_sonotron_engine` still
  needs `Threads::Threads` directly at line 109).
- DELETE the `gui_sonotron_audio` STATIC block (lines 112-127).
- Line 108: `target_link_libraries(gui_sonotron_engine PUBLIC gui_sonotron_brain gui_sonotron_ring hostrt Threads::Threads)` → replace `gui_sonotron_ring` with `audio_ring`.
- Lines 143-145: `target_link_libraries(gui_sonotron PRIVATE gui_sonotron_layout gui_sonotron_brain gui_sonotron_engine gui_sonotron_audio gui_sonotron_screenshot imgui)` → replace `gui_sonotron_audio` with `audio`.

**`apps/gui-sonotron/tests/CMakeLists.txt` edits:**
- Remove the `test_spsc_ring` registration block (lines ~44-49).
- Remove the `test_audio_engine_smoke` registration block (lines ~93-101).
- Lines 74 and 85: `${CMAKE_SOURCE_DIR}/components/midisrc/tests/fixtures/tiny.mid` → `${CMAKE_SOURCE_DIR}/components/platform/midisrc/tests/fixtures/tiny.mid` (forced by Step 2, not by the audio move — both `test_in_process_brain_session` and `test_audio_port_gate`'s fixture defines).
- `test_audio_port_gate`/`test_audio_primary_port_reachable`'s own
  `gui_sonotron_engine_test(...)` registrations **stay untouched**.

### Step 6 — top-level `CMakeLists.txt` (NEEDS-BUILD-EDIT, the seam that makes everything consistent again)

| Line(s) | Before | After |
|---|---|---|
| 45 | `add_subdirectory(components/common)` | `add_subdirectory(components/core/common)` |
| 46 | `add_subdirectory(components/runtime)` | `add_subdirectory(components/core/runtime)` |
| 51 | `add_subdirectory(components/chorddet)` | `add_subdirectory(components/core/chorddet)` |
| 52 | `add_subdirectory(components/arrangrr)` | `add_subdirectory(components/core/arrangrr)` |
| 66 | `add_subdirectory(components/midisrc)` | `add_subdirectory(components/platform/midisrc)` |
| 67 | `add_subdirectory(components/orchestrator)` | `add_subdirectory(components/platform/orchestrator)` |
| 68 | `add_subdirectory(components/hostrt)` | `add_subdirectory(components/platform/hostrt)` |
| 75 | `add_subdirectory(components/arrangrr/tests)` | `add_subdirectory(components/core/arrangrr/tests)` |
| 76 | `add_subdirectory(components/runtime/tests)` | `add_subdirectory(components/core/runtime/tests)` |
| 77 | `add_subdirectory(components/hostrt/tests)` | `add_subdirectory(components/platform/hostrt/tests)` |
| 95 | `add_subdirectory(components/melodd)` | `add_subdirectory(components/platform/engines/soundfont-melodd)` |
| new, before 96 | — | `add_subdirectory(components/platform/audio)` (must precede `apps/gui-sonotron`: it links `audio`/`audio_ring`, same ordering constraint melodd already documents at `CMakeLists.txt:90-95` for itself) |
| new, after Step 4 | — | `add_subdirectory(components/platform/engines/physical)` / `add_subdirectory(components/platform/engines/analog)` — only if their stub `CMakeLists.txt` do more than a no-op; if truly empty slots with no `add_library`, these two `add_subdirectory` calls are optional (harmless either way, but skipping them until real content lands avoids an empty-directory `add_subdirectory` that does nothing) |

Prose comments in this file (lines 41-52, 58-95) reference the old paths in
their rationale text — recommended to refresh for honesty, not build-blocking.

### Step 7 — `scripts/coverage.sh` (NEEDS-BUILD-EDIT — silent-failure risk if skipped)

All 8 occurrences of `components/arrangrr/` → `components/core/arrangrr/`,
`components/runtime/` → `components/core/runtime/`, `components/hostrt/` →
`components/platform/hostrt/` (lines 6-8, 23, 28, 58-59, 64-68, 105-112, 146,
182 — both the `CORE_ARGS` gcovr filter/exclude array and the second,
report-only gcovr invocation that additionally reports `hostrt`).

### Step 8 — `tests/golden/*.acmd` fixture paths (NEEDS-BUILD-EDIT)

`accompany_basic.acmd:13`, `accompany_melody_detect.acmd:27`,
`accompany_restyle.acmd:30`: `midi-source load
components/midisrc/tests/fixtures/tiny.mid` → `midi-source load
components/platform/midisrc/tests/fixtures/tiny.mid`. (`accompany_restyle.acmd:17`
is a comment mentioning the same path — cosmetic, not required for the
script to run.)

### Step 9 — build + verify (the single gate)

```
cmake --build --preset host   && ctest --preset host
cmake --build --preset arm
```
Then diff all 22 `tests/golden/*.golden` files byte-for-byte against the
freshly produced `.out` files (the golden harness already does this per-test
via `run_golden.cmake`; "22 goldens byte-identical" means: every
`arrangrr_golden`/`accompany_golden` registration in `tests/golden/
CMakeLists.txt` — I counted 18 `arrangrr_golden(...)` + `clip_launch` + 3
`accompany_golden(...)` = 22 — passes green).

---

## 4. Cost against the targets (dual-target check)

- Nothing in `components/core/{common,runtime,chorddet,arrangrr}` changes
  regime — they were already unconditional/dual-target and stay so, just one
  directory level deeper. `tests/arm-smoke`'s link gate (`firmware_stub`
  linking `arrangrr`+`runtime`) is untouched: it references those two
  targets by name, never by path.
- Nothing in `components/platform/{midisrc,orchestrator,hostrt,audio,
  engines/soundfont-melodd,engines/physical,engines/analog}` is reachable
  from the `if(ARRANGRR_FIRMWARE)` branch before or after — the `else()`
  placement discipline is preserved verbatim, just renamed/nested.
- `components/platform/audio`'s `audio_ring` INTERFACE target deliberately
  reaches `components/core/common/include` by a **raw include path**, not
  `target_link_libraries(... common)` — this is not a new dependency, it is
  the exact same avoidance-of-`common`'s-INTERFACE-compile-flags pattern
  `components/platform/engines/soundfont-melodd/CMakeLists.txt` already uses
  today, now with the `core/` prefix. No new host or core dependency is
  introduced by this move; `audio` links exactly what `gui_sonotron_audio`
  already linked (`melodd`, `miniaudio`, `Threads::Threads`), nothing added.

---

## 5. Dove va il nuovo (placement answers for open slots)

- **A new `ISoundEngine`-style contract header**, if/when Corelli/Giotto
  design one: this brief's locked layout does not name a location for it
  (unlike the earlier, more granular `docs/proposals/
  audio-engines-family-layout.md`, which put it at
  `components/audio-engines/core/`). Given the *current* locked tree only
  lists `engines/soundfont-melodd`, `engines/physical`, `engines/analog` as
  leaves with no shared-contract sibling, the placement question ("does a
  shared interface get its own `engines/core/` leaf, or does it live inside
  `components/platform/audio/include/audio/` since that's the one consumer
  today?") is a **logical** question about whether/how the family shares a
  contract — handed to Corelli, not resolved here.
- **`components/samplrr`**: given its own README's self-description ("host-only
  sampler engine") and `docs/architecture.md:37,133,540`'s framing as a
  peer slot to `melodd`, the physically consistent placement under the new
  regime would be `components/platform/engines/sampler/` (a fourth leaf next
  to `soundfont-melodd`/`physical`/`analog`) — **but the brief never named it,
  so I am not moving it**; flagging the gap for an owner decision instead
  (§7).

---

## 6. Ordered execution checklist (for whoever runs the git mv, gated on sign-off)

1. [ ] Owner confirms §0 (the `docs/architecture.md` §3 reversal is intended).
2. [ ] Owner decides §3's target-name question (keep `melodd`, or rename).
3. [ ] Owner decides §5's `components/samplrr` gap (fold into `engines/sampler`
   now, leave flat for a later pass, or explicitly drop the slot).
4. [ ] Step 0 — `mkdir -p components/core components/platform/engines`.
5. [ ] Step 1 — `git mv` the 4 core components.
6. [ ] Step 2 — `git mv` the 3 host platform components.
7. [ ] Step 3 — `git mv` melodd → `engines/soundfont-melodd` + the one absolute-path edit.
8. [ ] Step 4 — new `engines/physical`, `engines/analog` placeholders.
9. [ ] Step 5 — `git mv` the 6 audio files + write the 2 new CMakeLists.txt +
   the 10 include edits + the 2 `apps/gui-sonotron` CMakeLists edits.
10. [ ] Step 6 — top-level `CMakeLists.txt` (11 `add_subdirectory` edits + 1
    new line).
11. [ ] Step 7 — `scripts/coverage.sh` (8 path occurrences).
12. [ ] Step 8 — 3 `tests/golden/*.acmd` fixture-path edits.
13. [ ] Step 9 — build host (`-Werror`) + `ctest --preset host` all green +
    22 goldens byte-identical + build arm green. This is the ONE verification
    gate; nothing here is "done" before all three hold simultaneously.
14. [ ] Follow-up (not gating): sweep the ~86 prose files (`docs/`, `README.md`)
    for stale `components/<name>` mentions, starting with
    `docs/architecture.md` §2/§3 and `README.md:12,16,57`.

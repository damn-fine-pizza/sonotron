# Proposal: promote the host audio backend out of apps/gui-sonotron into a pluggable audio-engines family

Status: PROPOSAL — read-only move-plan, not executed. Physical layout only
(directory/target/file placement); the logical shape of the `ISoundEngine`
contract itself (method signatures, ownership, error handling) is Corelli's
and Nazzareno's call, not fixed here beyond what placement requires.

Owner decisions this proposal converges on (2026-07-14/15):
- Misfiling confirmed: `apps/gui-sonotron/src/audio_engine.{hpp,cpp}` has no
  ImGui/GLFW coupling and must leave the GUI app.
- Scope widened: not a single move, but a pluggable **family** of synthesis
  backends (`melodd`/TinySoundFont today, physical-models and analog later)
  behind one shared contract.
- Naming: nested grouping `components/audio-engines/<method>/`, not flat
  `components/engine-<method>`.
- Dual-target fork: **dual-target-ready, not now.** Implementation is
  HOST-ONLY today (excluded from the arm-none-eabi branch exactly like
  `components/melodd`); the seam is *shaped* so a later promotion to
  freestanding/no-heap does not force a redesign, but no no-heap constraint
  is paid now.

---

## 1. The family layout + naming

```
components/
  audio/                          # NEW — engine-agnostic host device layer
    include/audio/
      audio_engine.hpp             # <- apps/gui-sonotron/src/audio_engine.hpp
      spsc_ring.hpp                 # <- apps/gui-sonotron/src/spsc_ring.hpp
      audio_midi_event.hpp          # <- apps/gui-sonotron/src/audio_midi_event.hpp
    src/
      audio_engine.cpp              # <- apps/gui-sonotron/src/audio_engine.cpp
    tests/
      test_audio_engine_smoke.cpp   # <- apps/gui-sonotron/tests/test_audio_engine_smoke.cpp
      test_spsc_ring.cpp            # <- apps/gui-sonotron/tests/test_spsc_ring.cpp
    CMakeLists.txt

  audio-engines/                  # NEW — pure grouping dir, no target of its own
    CMakeLists.txt                 # just add_subdirectory(core/soundfont/physical-models/analog)
    core/                         # the shared ISoundEngine contract
      include/audio_engines/i_sound_engine.hpp
      CMakeLists.txt               # INTERFACE lib
    soundfont/                    # today's melodd, WRAPPED not absorbed (see §2)
      include/soundfont/engine.hpp
      src/engine.cpp
      tests/
      CMakeLists.txt
    physical-models/              # SLOT — no code yet (mirrors melodd's own original state)
      README.md
      CMakeLists.txt               # empty/commented, no add_library yet
    analog/                       # SLOT — no code yet; documented FUTURE core-capable candidate
      README.md
      CMakeLists.txt

  melodd/                         # UNCHANGED — stays exactly where it is (see §2)
  ...
```

**Why `components/audio/` as a sibling of `components/audio-engines/`, not
nested `audio-engines/host/`:** the device layer is not itself an engine —
it is engine-*agnostic*, holds one `ISoundEngine` chosen at runtime by the
composition root, and owns things no engine should ever touch (the
miniaudio device, the render/panic mutex, the GUI-thread↔audio-thread ring).
Nesting it under `audio-engines/host/` invites a reader to mistake it for a
fourth backend. A flat, short sibling name — `audio` — reads at a glance the
same way `common`/`runtime`/`hostrt`/`melodd` already do, and the two
entries `audio` + `audio-engines` sitting next to each other in a `ls
components/` self-explain the split: "the thing that plays" vs. "the things
that can be played."

**Why `audio-engines/core/` for the interface, not `components/audio-engines/`
itself as a target:** `components/` itself is a pure grouping folder with no
CMake target — only its children build. `audio-engines/` should mirror that
exact shape (its own `CMakeLists.txt` only chains `add_subdirectory`s) so the
"one target per leaf directory" invariant holds everywhere in the tree, not
just at the top level. `core/` is a leaf like any other engine, it just
happens to ship the shared header instead of an implementation.

**CMake target names (flat CMake namespace — leaf directory names alone are
not unique/greppable enough, e.g. plain `core` would collide/obscure):**

| Directory | Target |
|---|---|
| `components/audio/` | `audio` |
| `components/audio-engines/core/` | `audio_engine_core` (INTERFACE) |
| `components/audio-engines/soundfont/` | `audio_engine_soundfont` |
| `components/audio-engines/physical-models/` | `audio_engine_physical_models` (SLOT, not built yet) |
| `components/audio-engines/analog/` | `audio_engine_analog` (SLOT, not built yet) |

`components/melodd`'s own target stays `melodd`, untouched (§2).

---

## 2. Does melodd BECOME audio-engines/soundfont, or does soundfont WRAP melodd?

**Recommendation: WRAP.** `components/melodd` stays exactly where it is,
untouched — directory, target name `melodd`, README, tests, its three
existing consumers (`apps/tools/melodd`, and the new `components/audio-
engines/soundfont`) all keep working with zero edits to `melodd` itself.
`components/audio-engines/soundfont` is a NEW, thin component: one class
(`soundfont::SoundfontEngine` or similar) that implements `ISoundEngine` by
delegating every call to a `melodd::Synth` it owns. `target_link_libraries
(audio_engine_soundfont PUBLIC audio_engine_core melodd)`.

This is not a stylistic preference — it is the SAME dependency shape the
project already uses on purpose: `components/chorddet` stays its own
component; `components/arrangrr` (built later, the higher-level consumer)
depends on it rather than absorbing/renaming it (D43: "arrangrr depends on
chorddet, chorddet never depends on arrangrr" — `components/melodd/CMakeLists.
txt:18-27`, `docs/DESIGN.md` §16.1/§16.8). Absorbing melodd into
`audio-engines/soundfont` would force a target rename (`melodd` →
something else) with real blast radius — every `target_link_libraries(...
melodd ...)` site (`apps/tools/melodd/CMakeLists.txt:12`,
`components/melodd/tests/CMakeLists.txt`'s `melodd_test` function, the new
audio-engines/soundfont link) — for zero structural gain. `melodd` also has
its own, independent identity worth keeping stable: "the built-in GM audio
realizer," reusable outside the pluggable-family concept entirely (the
standalone `apps/tools/melodd` binary has no interest in `ISoundEngine` and
should keep linking `melodd` raw, unchanged).

---

## 3. The shared interface component (`audio_engine_core`)

Placement: `components/audio-engines/core/`, HOST-ONLY for now — added only
in the non-firmware (`else()`) branch of the top-level `CMakeLists.txt`,
same discipline as `components/melodd`/`hostrt`/`midisrc`/`orchestrator`
(`CMakeLists.txt:54-101`). NOT `components/common`: `common` is the base
layer BOTH the arm-none-eabi branch and the host branch add unconditionally
(`CMakeLists.txt:41-46`) — putting a host-only-for-now interface there would
silently widen its build surface onto the firmware target without owner
sign-off, exactly the kind of undeclared dependency my charter forbids me
from assuming. It stays include-only-reachable from `components/common`
(header for `arrangrr::MidiMessage` in the dispatch signature), mirroring
`components/melodd/CMakeLists.txt:19-28`'s own precedent: `target_include_
directories(... PUBLIC .../components/common/include)`, never `target_link_
libraries(... common)`, so `common`'s PUBLIC `-fno-exceptions -fno-rtti
-fno-threadsafe-statics` never leaks onto the family.

Dependency direction for the whole family (mirrors "arrangrr core stays pure
-MIDI upstream, engines are downstream, name-blind" — `docs/DESIGN.md` §0910:
"arrangrr … and a future host-only audio engine are peer modules wired by an
orchestrator, name-blind, talking only through the POD interface"):

```
audio_engine_core  (interface, no deps beyond common's MidiMessage header)
   ^        ^              ^
   |        |              |
soundfont  physical-models  analog     <- concrete engines, each depends
   |                                      DOWN on core only, NEVER on a
  melodd                                  sibling engine
   ^
   |
audio  (host device: miniaudio + the ring; depends on core ONLY,
        never on melodd/soundfont/any concrete engine)
   ^
   |
apps/gui-sonotron (composition root: links `audio` + the ONE concrete
                    engine it chooses, e.g. `audio_engine_soundfont`,
                    wires them together at construction time)
```

`components/audio` deliberately does NOT link any concrete engine. This is
the crux of "the host audio device layer picks ONE engine at runtime": the
choice of which backend to build lives at the composition root
(`apps/gui-sonotron`'s CMakeLists + `main.cpp`), not inside `components/
audio`. How `AudioEngine` receives its `ISoundEngine` (constructor
reference, `unique_ptr`, factory) is a logical/API question I hand to
Corelli/Nazzareno — I only fix that the LINK EDGE from `audio` to any
concrete engine must not exist.

---

## 4. Dual-target: host-only now, promotion path documented

**Now (built):** `audio_engine_core` is HOST-ONLY, added only in the
non-firmware branch. Its header may use STL/pragmatic host code freely — no
no-heap constraint paid today. Every family member (`soundfont`, and the
`physical-models`/`analog` slots when they land) is excluded from the
arm-none-eabi branch exactly like `components/melodd` is today.

**What would have to change to promote `audio_engine_core` (and a future
core-capable engine) to freestanding/no-heap — documented intent, NOT built:**

1. **The render-callback contract itself must avoid STL/heap in its
   signature.** Today's `AudioEngine::render(float* out, int frame_count)` is
   already shaped this way (caller-provided buffer, no owned return) — that
   shape must be preserved verbatim when `render`/`dispatch` become pure-
   virtual members of `ISoundEngine`; no `std::string`, `std::function`,
   `std::any`, or exception-throwing signatures in the interface.
2. **Engines split by nature, not by fiat:** `soundfont` (TSF file I/O,
   `std::string` paths, `std::filesystem`) and any DSP-table-loading
   `physical-models` engine are HOST-ONLY *forever* — no promotion path for
   them. `analog` (a from-scratch, formula/table-driven synthesizer with no
   file I/O) is the plausible FUTURE core-capable candidate IF its own
   implementation stays bounded/no-heap — this is why its SLOT is called out
   by name now, before any code exists, so the eventual implementer knows
   the bar to clear.
3. **`audio_engine_core` would move from the host-only `else()` branch to the
   UNCONDITIONAL block** at the top of the top-level `CMakeLists.txt`
   (alongside `components/common`/`runtime`/`chorddet`, `CMakeLists.txt:41-
   52`) — a physical relocation across the fork line, not a redesign, PROVIDED
   the contract already respects (1). This is exactly why getting the shape
   right now matters even though the placement stays host-only today.
4. **`components/audio` (the device layer) never promotes** — it owns a real
   miniaudio device and OS-level playback, inherently host-only regardless of
   which engine is plugged in. Promotion only ever concerns `audio_engine_core`
   + a future `analog` engine, never `components/audio` or `soundfont`/
   `physical-models`.

---

## 5. Concrete CMake sketch (illustrative, for Nazzareno — not applied here)

`components/audio-engines/CMakeLists.txt` (pure grouping, mirrors `components/`
itself):
```cmake
add_subdirectory(core)
add_subdirectory(soundfont)
add_subdirectory(physical-models)   # SLOT
add_subdirectory(analog)            # SLOT
```

`components/audio-engines/core/CMakeLists.txt`:
```cmake
add_library(audio_engine_core INTERFACE)
target_include_directories(audio_engine_core INTERFACE include)
target_include_directories(audio_engine_core INTERFACE
  ${CMAKE_SOURCE_DIR}/components/common/include)
```

`components/audio-engines/soundfont/CMakeLists.txt`:
```cmake
add_library(audio_engine_soundfont STATIC src/engine.cpp)
target_include_directories(audio_engine_soundfont PUBLIC include)
target_link_libraries(audio_engine_soundfont PUBLIC audio_engine_core melodd)
target_compile_options(audio_engine_soundfont PRIVATE -Wall -Wextra -Werror)
add_subdirectory(tests)
```

`components/audio/CMakeLists.txt`:
```cmake
find_package(Threads REQUIRED)
add_library(audio_ring INTERFACE)
target_include_directories(audio_ring INTERFACE include)
target_include_directories(audio_ring INTERFACE
  ${CMAKE_SOURCE_DIR}/components/common/include)

add_library(audio STATIC src/audio_engine.cpp)
target_include_directories(audio PUBLIC include)
target_link_libraries(audio PUBLIC audio_engine_core audio_ring miniaudio
  Threads::Threads)
target_compile_options(audio PRIVATE -Wall -Wextra -Werror)
add_subdirectory(tests)
```

Top-level `CMakeLists.txt` ordering (host-only `else()` branch, before
`apps/gui-sonotron`, mirroring melodd's own "ordered BEFORE apps/gui-sonotron"
comment at `CMakeLists.txt:90-95`):
```
...
add_subdirectory(components/melodd)          # unchanged
add_subdirectory(components/audio-engines)   # NEW
add_subdirectory(components/audio)           # NEW
add_subdirectory(apps/gui-sonotron)
add_subdirectory(apps/tools/melodd)          # unchanged
```

`apps/gui-sonotron/CMakeLists.txt` rewire:
- DELETE the local `gui_sonotron_ring` INTERFACE target and the local
  `gui_sonotron_audio` STATIC target (lines 20-29 minus the Threads
  `find_package`, and lines 111-126).
- `gui_sonotron_engine`'s link line (`CMakeLists.txt:107-108`) swaps
  `gui_sonotron_ring` → `audio_ring`.
- The executable's link line (`CMakeLists.txt:142-144`) swaps
  `gui_sonotron_audio` → `audio audio_engine_soundfont` (the composition
  root's explicit backend choice).
- `main.cpp`'s `#include "src/audio_engine.hpp"` → `#include "audio/
  audio_engine.hpp"`; construction gains an explicit step to build the
  `SoundfontEngine` before constructing `AudioEngine` (content-level wiring,
  Nazzareno's).

---

## 6. Test relocation

| Test | Currently links | Moves? | Why |
|---|---|---|---|
| `test_audio_engine_smoke.cpp` | `gui_sonotron_audio` | → `components/audio/tests/`, links `audio` (+ a fake/stub `ISoundEngine`, not `audio_engine_soundfont`, keeping the device-layer smoke test decoupled from TSF/melodd — matches its own header's "device-independent" framing) | Tests `AudioEngine` itself |
| `test_spsc_ring.cpp` | `gui_sonotron_ring` | → `components/audio/tests/`, links `audio_ring` | Tests the ring now living in `components/audio` |
| `test_audio_port_gate.cpp` | `gui_sonotron_engine` | **STAYS** in `apps/gui-sonotron/tests/` | Tests `InProcessBrainSession`'s OutEvent→AudioMidiEvent port filter — producer-side logic, not `AudioEngine`. Only needs the relocated header's include path updated (`src/audio_midi_event.hpp` → `audio/audio_midi_event.hpp`). |
| `test_audio_primary_port_reachable.cpp` | `gui_sonotron_engine` | **STAYS** | Same reason as above |

This corrects the initial framing (all three `test_audio_*.cpp` looked like
"the audio tests"): only `test_audio_engine_smoke.cpp` actually exercises
`AudioEngine`; the other two exercise `InProcessBrainSession` and use
`audio_midi_event.hpp` merely as the shared POD type to inspect ring
contents — verified by reading each file's `#include`s
(`apps/gui-sonotron/tests/test_audio_port_gate.cpp:28`, `test_audio_primary_
port_reachable.cpp:19` both include `src/in_process_brain_session.hpp`, not
`src/audio_engine.hpp`).

---

## 7. Migration order (safe sequence, no broken intermediate state)

1. `mkdir -p` the new directories (`components/audio/{include/audio,src,tests}`,
   `components/audio-engines/{core/include/audio_engines,soundfont/{include/soundfont,src,tests},physical-models,analog}`).
2. `git mv apps/gui-sonotron/src/spsc_ring.hpp components/audio/include/audio/spsc_ring.hpp`
3. `git mv apps/gui-sonotron/src/audio_midi_event.hpp components/audio/include/audio/audio_midi_event.hpp`
4. `git mv apps/gui-sonotron/src/audio_engine.hpp components/audio/include/audio/audio_engine.hpp`
5. `git mv apps/gui-sonotron/src/audio_engine.cpp components/audio/src/audio_engine.cpp`
6. `git mv apps/gui-sonotron/tests/test_spsc_ring.cpp components/audio/tests/`
7. `git mv apps/gui-sonotron/tests/test_audio_engine_smoke.cpp components/audio/tests/`
8. Write `components/audio-engines/core/include/audio_engines/i_sound_engine.hpp`
   (new file, Corelli/Nazzareno's contract design) + its `CMakeLists.txt`.
9. Write `components/audio-engines/soundfont/{include/soundfont/engine.hpp,
   src/engine.cpp}` (new adapter over `melodd::Synth`) + its `CMakeLists.txt`
   + `tests/`.
10. Write `components/audio-engines/{physical-models,analog}/README.md` +
    stub `CMakeLists.txt` (SLOT, no `add_library` yet — mirrors melodd's own
    original state per `docs/DESIGN.md` §0910).
11. Edit include paths inside every moved/new file (relative `"audio_midi_
    event.hpp"` → `"audio/audio_midi_event.hpp"`, etc., following the
    `melodd::` convention of `#include "melodd/synth.hpp"` from consumers).
12. Edit `apps/gui-sonotron/CMakeLists.txt` (delete the two local targets,
    rewire the two link lines) and `apps/gui-sonotron/main.cpp`'s include +
    construction site.
13. Edit `apps/gui-sonotron/tests/CMakeLists.txt` (drop the two moved test
    registrations; update the two staying tests' include path only).
14. Edit the top-level `CMakeLists.txt` (`add_subdirectory` chain per §5).
15. Build host: `cmake --build` + `ctest` — confirm all previously-green
    tests stay green, the two relocated tests pass from their new location,
    `apps/tools/melodd` and `components/melodd/tests` are untouched/green
    (zero edits there).
16. Build arm-none-eabi preset — confirm it is bit-for-bit unaffected (none
    of the new directories are reachable from the `if(ARRANGRR_FIRMWARE)`
    branch).
17. `tests/golden` — unaffected by construction (no golden exercises GUI
    audio), run anyway as a blast-radius sanity check, not because this
    change should touch it.

Labels: steps 1-10 SAFE (pure addition/move, nothing references the new
paths yet); step 11 SAFE (mechanical include-path edits); steps 12-14
NEEDS-BUILD-EDIT (the CMake/main.cpp edits that make the tree consistent
again); step 9's `ISoundEngine` adapter class shape and step 12's exact
`AudioEngine`-receives-`ISoundEngine` wiring are NEEDS-DECISION (Corelli/
Nazzareno, not a placement question).

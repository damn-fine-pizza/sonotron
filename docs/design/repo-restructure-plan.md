# Repo restructure — move-plan (arrangrr → sonotron)

Status: **move-plan, ready for review.** Produced by Palladio from
`docs/design/project-structure.md` (target + principles, owner-aligned
2026-07-07), for execution on branch `repo-restructure`. This document is a
**plan**, not an action: no file was moved, renamed, created, or deleted to
produce it. Every operation below is a `git mv` (or a stated CREATE/EDIT) for
a human or Nazzareno to run, in the given order, so the tree never sits in a
broken intermediate state.

Companions: `docs/design/project-structure.md` (authoritative target tree),
`docs/product-identity.md`, `docs/design/workstation-vision.md`.

Labels: **SAFE** (mechanical, no ambiguity) · **NEEDS-BUILD-EDIT** (the move
forces a CMake/script edit, itemized) · **NEEDS-DECISION** (owner must confirm
a naming/placement choice before execution).

---

## AMENDMENTS — owner-confirmed 2026-07-07 (these OVERRIDE the plan below)

1. **CLI binary name = `cli-arrangrr`** (NOT `arrangrr`). Step 20: `set_target_properties(cli_arrangrr PROPERTIES OUTPUT_NAME cli-arrangrr)`. Propagate to demo scripts: step 32 → `CLI=build/host/apps/tools/cli-arrangrr/cli-arrangrr`, and any `./arrangrr` invocation → `./cli-arrangrr`.
2. **arm gate → `tests/arm-smoke/`, NOT inside arrangrr.** Replaces step 10 and the `components/arrangrr/firmware` line in step 16: `git mv app/firmware/stub/main.cpp tests/arm-smoke/main.cpp` + its `CMakeLists.txt`; root CMake under the `ARRANGRR_FIRMWARE` branch → `add_subdirectory(tests/arm-smoke)`. It is a freestanding link smoke-build — not an app, not an stm32 dir. Target may keep `firmware_stub` or become `arm_smoke`.
3. **Create NOTHING for the future now:** no `apps/stm32`, no `apps/gui-sonotron`, no `apps/sonotron-host`, no `third_party/` (all arrive with the GUI spike / the orchestrator milestone). Slots created: only `components/{orchestrator,melodd,samplrr}` (Phase E unchanged).
4. **Confirmed defaults:** CLI CMake target = `cli_arrangrr`; coverage CORE gate stays scoped to `components/arrangrr/` only (step 28 default) for now.

---

## 0. Pre-flight (do first, not a move)

- Working tree is clean on `repo-restructure` (verified: `git status` →
  nothing to commit at plan time, aside from the pre-existing untracked docs
  under `docs/design/` from prior sessions).
- `build/`, `.cache/`, and the `compile_commands.json` symlink
  (`compile_commands.json -> build/tidy/compile_commands.json`) all encode the
  OLD tree layout inside generated CMake caches. They must be wiped and
  regenerated AFTER the move, not migrated:
  `rm -rf build/ .cache/` (the symlink itself is a tracked convenience link
  pointing at a build artifact; leave the symlink, it self-heals on the next
  `cmake --preset tidy`). — **SAFE**, mandatory, not optional.
- **Known future collision (flagged, not part of this plan):** branch
  `spikes/gui-skeleton` (commit `43fa02f`) adds `app/gui/` (ImGui+GLFW UDS-JSONL
  client) on top of a *different* parent than `repo-restructure`. It is not
  merged into `main` and not present in the tree this plan maps. When that
  spike lands, `app/gui/` → `apps/gui-sonotron/` + `third_party/{imgui,glfw}`
  wiring is a FOLLOW-UP move, not covered here — see `Cosa ho flaggato` in my
  reply.

---

## 1. La pianta attuale (mapped, with evidence)

```
CMakeLists.txt              project(arrangrr) — root wiring (read in full)
CMakePresets.json           host / host-release / coverage / tidy / arm — no
                             source-tree literal paths, unaffected by the move
cmake/toolchains/arm-cortex-m7.cmake
.clang-tidy                 HeaderFilterRegex: 'app/'
scripts/{ci.sh,lint.sh,coverage.sh}
app/
├── core/                   add_library(arrangrr_core STATIC …) — CMakeLists.txt
│   ├── include/arrangrr/   45 headers (abi.hpp, arp/, arranger/ (+styles/),
│   │                       chord/, common/, midi/, routing/, scheduler/,
│   │                       timeline/, transport/, config.hpp, engine.hpp,
│   │                       version.hpp)
│   ├── src/                4 .cpp (version, common_checks, engine, engine_checks)
│   └── tests/               CMakeLists.txt + 27 test_*.cpp + test.hpp
│                            — 22 are core-only (link arrangrr_core only);
│                            5 ALSO link arrangrr_host: test_host.cpp,
│                            test_panels.cpp, test_panel_nav.cpp,
│                            test_style_chooser.cpp, test_rc_config.cpp
│                            (evidence: app/core/tests/CMakeLists.txt:82-96)
├── platform/host/          add_library(arrangrr_host STATIC …) AND
│                           add_executable(arrangrr_cli main.cpp) in the SAME
│                           CMakeLists.txt/directory — 37 lib files (.cpp/.hpp,
│                           flat) + main.cpp + CMakeLists.txt
├── firmware/stub/          add_executable(firmware_stub main.cpp) — links
│                           arrangrr_core only, SUFFIX .elf
├── tools/
│   ├── arrstyle-converter/  add_library(arrstyle_lib) + add_executable
│   │                        (arrstyle_converter) + tests/ (own CMakeLists);
│   │                        self-contained, does NOT link arrangrr_core
│   │                        (evidence: CMakeLists.txt:4 comment + no
│   │                        target_link_libraries to arrangrr_core)
│   └── arrstyle-extractor/  extract_kb.py + kb-reference/*.md/*.json —
│                            NOT wired into any CMakeLists.txt anywhere
│                            (grepped; only arrstyle-converter is
│                            add_subdirectory'd from root)
└── tests/
    ├── golden/              CMakeLists.txt (arrangrr_golden() fn) + 17×
    │                        (.acmd+.golden) + run_golden.cmake — parameterized
    │                        via $<TARGET_FILE:arrangrr_cli>, no literal paths
    └── integration/         CMakeLists.txt + 3× *.sh — parameterized via
                             $<TARGET_FILE:arrangrr_cli>, no literal paths
demo/
├── clean/  {armed,setup}.acmd, motd*.txt, start.sh
├── jam/    setup.acmd, motd.txt, start.sh
└── lib/    launch.sh — CLI=build/host/app/platform/host/arrangrr (LITERAL,
                        mirrors the CMake source-subdir layout of the binary
                        output tree; breaks on the move — see §4)
```

**Reference graph, root `CMakeLists.txt` (today):**
```
add_subdirectory(app/core)
if(ARRANGRR_FIRMWARE)
  add_subdirectory(app/firmware/stub)
else()
  enable_testing()
  add_subdirectory(app/platform/host)
  add_subdirectory(app/core/tests)
  add_subdirectory(app/tests/golden)
  add_subdirectory(app/tests/integration)
  add_subdirectory(app/tools/arrstyle-converter)
endif()
```

**Target names in play** (grepped across every `CMakeLists.txt`/`*.cmake`):
`arrangrr_core` (lib), `arrangrr_host` (lib), `arrangrr_cli` (exe,
`OUTPUT_NAME arrangrr`), `firmware_stub` (exe), `arrstyle_lib` (lib),
`arrstyle_converter` (exe, `OUTPUT_NAME arrstyle-converter`).

**Intended target tree** (from `project-structure.md`): `apps/{gui-sonotron,
demo,tools/{cli-arrangrr,arrstyle-converter,arrstyle-extractor}}`,
`components/{arrangrr,hostrt,orchestrator,melodd,samplrr}` (flat, no
core/host subfolders), `third_party/`, `tests/{golden,integration}`,
`build/`, `cmake/`, `docs/`, `scripts/`.

---

## 2. Diagnosi di collocazione (why each thing moves)

- **`app/core` is the freestanding component, misnamed as "the umbrella."**
  Per `product-identity.md` (Naming, 2026-07-07): arrangrr is now *one*
  component among several under `sonotron`; `app/core` living directly under
  `app/` (a target-regime folder) reads as "the" core of a single product,
  which is no longer true. → `components/arrangrr/`.
- **`app/platform/host` conflates TWO roles in one directory: a library and
  an executable.** `add_library(arrangrr_host …)` and
  `add_executable(arrangrr_cli main.cpp)` share one `CMakeLists.txt`
  (evidence: `app/platform/host/CMakeLists.txt:4` and `:33`). Per the target
  tree, the library is a component (`components/hostrt`) and the executable
  is a deliverable (`apps/tools/cli-arrangrr`) — two different tree axes
  living in the same folder is exactly the "wrong room" Palladio exists to
  catch. → **split**.
- **`app/core/tests` mixes core-only tests with host-linking tests** — 5 of
  27 files `target_link_libraries(… PRIVATE arrangrr_host)` (evidence cited
  above). "Test beside the unit it proves": those 5 prove `hostrt` behaviour
  (Shell/PanelManager/UDS wiring through the ALSA-linked host lib), not the
  freestanding core, and today they physically live inside the core's own
  test directory although the core doesn't need ALSA or heap to build. →
  **split**: 22 stay with `arrangrr`, 5 move to `hostrt`'s own tests.
- **`app/firmware/stub` is a top-level target-regime folder for one
  five-line entrypoint.** Per `project-structure.md` Principle 2, firmware is
  explicitly NOT a top-level dir — it is "a thin entrypoint carried inside
  arrangrr." → folded into `components/arrangrr/firmware/`.
- **`app/tools/*` sit under `app/`, a target-regime folder, though tools are
  deliverables (binaries), not target-regime code.** → `apps/tools/*`.
- **`app/tests/{golden,integration}` exercise the CLI end-to-end
  (cross-component behaviour)** — the target tree's own criterion for
  `tests/` (cross-component, vs. unit tests living in-component). →
  `tests/{golden,integration}` at repo root.
- **`demo/` is a deliverable-adjacent demonstration app**, not core or host
  library code; the target tree names it explicitly under `apps/`. →
  `apps/demo/`.
- **Naming drift baked into target identifiers**: `arrangrr_core` and
  `arrangrr_host` both carry a "this-is-the-only-component" prefix that no
  longer holds once `hostrt`/`orchestrator`/`melodd`/`samplrr` are peers. The
  brief specifies the exact renames (`arrangrr_core→arrangrr`,
  `arrangrr_host→hostrt`); this plan applies them.
- **No file needs splitting along a NEW logical seam.** `app/core/src` (4
  files) and `app/core/include/arrangrr/**` (45 files) relocate as-is —
  arrangrr stays ONE library, no internal port introduced (this plan does
  not touch `sequencrr` extraction, which is explicitly future/Corelli's).

---

## 3. Piano di spostamento (ordered `git mv` + CREATE + EDIT)

### Phase A — whole-subtree moves (no internal split, mechanical) — SAFE

Run in any order relative to each other (they touch disjoint source paths),
but all before Phase B/C, since Phase B/C's CMake edits reference the NEW
paths.

1. `git mv app/tools/arrstyle-converter apps/tools/arrstyle-converter`
   (≈40 files: `src/*.{cpp,hpp}` ×26, `tests/*` ×14 incl. fixtures,
   `CMakeLists.txt` ×2, `README.md`, `DESIGN.md`). Internal includes are
   flat/relative (`#include "model.hpp"` etc.) — unaffected by relocation
   (verified: no `#include "../` anywhere in the subtree).
2. `git mv app/tools/arrstyle-extractor apps/tools/arrstyle-extractor`
   (7 files: `extract_kb.py`, `README.md`, `kb-reference/**`). Not wired into
   any `CMakeLists.txt` — zero build impact.
3. `git mv app/tests/golden tests/golden` (34 files + `CMakeLists.txt` +
   `run_golden.cmake`). Fully parameterized via CMake generator expressions
   — no literal paths inside.
4. `git mv app/tests/integration tests/integration` (3 `*.sh` +
   `CMakeLists.txt`). Same: parameterized via `$<TARGET_FILE:…>`.
5. `git mv demo/clean apps/demo/clean`
6. `git mv demo/jam apps/demo/jam`
7. `git mv demo/lib apps/demo/lib`
   (steps 5-7: `demo/` becomes empty and is implicitly removed by git once
   its last tracked file moves — confirm no stray untracked files are left
   behind under old `demo/` before deleting it manually if git leaves the
   empty dir on disk.)
8. `git mv app/core/include components/arrangrr/include`
   (45 headers, `#include "arrangrr/…"` paths are ABSOLUTE-from-include-root
   already — no header content changes; only the CMake `target_include_directories`
   base moves with the directory, see Phase C).
9. `git mv app/core/src components/arrangrr/src` (4 `.cpp`).

### Phase B — split moves (file-level, ordered so nothing is orphaned) — SAFE / NEEDS-DECISION (naming)

10. `git mv app/firmware/stub/main.cpp components/arrangrr/firmware/main.cpp`
    then `git mv app/firmware/stub/CMakeLists.txt components/arrangrr/firmware/CMakeLists.txt`.
    **NEEDS-DECISION**: subfolder name — I recommend `firmware/` (direct,
    minimal-churn continuation of `app/firmware/stub`'s identity; the design
    doc's own prose says "a thin arm entrypoint … inside arrangrr" without
    mandating a name). Alternative: `arm/` (regime-named, mirrors a possible
    future `arrangrr-arm64` sibling per Principle 1's escape hatch). Either
    is SAFE mechanically; only the name needs the owner's nod.
11. `git mv app/core/tests/test_abi_frozen.cpp components/arrangrr/tests/`
    … (repeat for the 21 other core-only test files + `test.hpp`: `test_arp.cpp`,
    `test_arp_functional.cpp`, `test_arranger.cpp`,
    `test_chord_context_diagnosis.cpp`, `test_chord.cpp`,
    `test_chord_detect.cpp`, `test_chord_follow.cpp`, `test_chord_seq.cpp`,
    `test_chord_seq_vs_live_steer.cpp`, `test_common.cpp`, `test_engine.cpp`,
    `test_engine_fire_order.cpp`, `test_followed_context.cpp`,
    `test_gesture.cpp`, `test_harmony_steer.cpp`, `test_input_zone.cpp`,
    `test_midi.cpp`, `test_scheduler.cpp`, `test_step_locks.cpp`,
    `test_timeline.cpp`, `test_voicing.cpp`, `test.hpp`) →
    `components/arrangrr/tests/`. 22 files total.
12. `git mv app/core/tests/test_host.cpp components/hostrt/tests/test_host.cpp`
    `git mv app/core/tests/test_panels.cpp components/hostrt/tests/test_panels.cpp`
    `git mv app/core/tests/test_panel_nav.cpp components/hostrt/tests/test_panel_nav.cpp`
    `git mv app/core/tests/test_style_chooser.cpp components/hostrt/tests/test_style_chooser.cpp`
    `git mv app/core/tests/test_rc_config.cpp components/hostrt/tests/test_rc_config.cpp`
    (5 files — the ones that `target_link_libraries(… arrangrr_host)` today).
13. **CREATE** `components/hostrt/tests/test.hpp` — duplicate of
    `components/arrangrr/tests/test.hpp` (17-line, dependency-free, no
    shared logic to preserve a single source of truth for). Duplicating a
    trivial leaf header beats introducing a cross-component test-support
    dependency (`hostrt/tests` → `arrangrr/tests`) for zero benefit. **SAFE**
    — flag to Corelli only if/when a real shared test-support lib is wanted
    later (out of scope here).
14. `git mv app/platform/host components/hostrt` (moves the WHOLE directory
    first: 37 lib files + `main.cpp` + `CMakeLists.txt`, all under one new
    name)
15. `git mv components/hostrt/main.cpp apps/tools/cli-arrangrr/main.cpp`
    (pulls the executable's entrypoint back OUT into its own deliverable
    dir — completes the split described in §2). `main.cpp`'s includes
    (`"alsa_midi.hpp"`, `"shell.hpp"`, `"arrangrr/common/time.hpp"`, etc.,
    verified via grep) resolve through `hostrt`'s PUBLIC include dir
    (`target_include_directories(hostrt PUBLIC .)`) propagated at link time
    — physical co-location was never load-bearing for these, only the CMake
    link edge is. **SAFE**, verified.

### Phase C — CMakeLists.txt content edits forced by the moves — NEEDS-BUILD-EDIT

16. **`CMakeLists.txt` (root)** — full rewrite:
    ```cmake
    cmake_minimum_required(VERSION 3.28)
    project(sonotron LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

    if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
      set(ARRANGRR_FIRMWARE ON)
      if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
        message(FATAL_ERROR "arm-none-eabi-g++ >= 14 required (found ${CMAKE_CXX_COMPILER_VERSION})")
      endif()
    else()
      set(ARRANGRR_FIRMWARE OFF)
    endif()

    add_subdirectory(components/arrangrr)

    if(ARRANGRR_FIRMWARE)
      add_subdirectory(components/arrangrr/firmware)
    else()
      enable_testing()
      add_subdirectory(components/hostrt)
      add_subdirectory(apps/tools/cli-arrangrr)
      add_subdirectory(components/arrangrr/tests)
      add_subdirectory(components/hostrt/tests)
      add_subdirectory(tests/golden)
      add_subdirectory(tests/integration)
      add_subdirectory(apps/tools/arrstyle-converter)
    endif()
    ```
    (`ARRANGRR_FIRMWARE` variable name kept as-is — it gates a build mode,
    not a component; renaming it is optional bikeshed, not required by the
    move. `project(sonotron …)` is the one substantive line per the design
    doc's own "Execution" note.)
17. **`components/arrangrr/CMakeLists.txt`** — rename target
    `arrangrr_core` → `arrangrr` everywhere (`add_library`, all 3
    `target_compile_options` calls, `target_include_directories`).
18. **`components/arrangrr/firmware/CMakeLists.txt`** — `target_link_libraries(firmware_stub PRIVATE arrangrr)`
    (was `arrangrr_core`).
19. **`components/hostrt/CMakeLists.txt`** — rewritten, DROP the
    `add_executable(arrangrr_cli …)` block entirely (moved out, step 21):
    ```cmake
    find_package(ALSA REQUIRED)

    add_library(hostrt STATIC
      shell.cpp shell_parse.cpp shell_io_commands.cpp shell_music_commands.cpp
      shell_input.cpp shell_view.cpp shell_chooser.cpp jsonl.cpp alsa_midi.cpp
      console.cpp kitty_keys.cpp note_names.cpp gm_program.cpp panel_manager.cpp
      piano_view.cpp parts_view.cpp groove_view.cpp arp_view.cpp midi_monitor.cpp
      ui_style.cpp style_chooser.cpp rc_config.cpp uds_server.cpp
    )
    target_include_directories(hostrt PUBLIC .)
    target_link_libraries(hostrt PUBLIC arrangrr ALSA::ALSA)
    target_compile_options(hostrt PRIVATE -Wall -Wextra -Werror)
    ```
20. **CREATE `apps/tools/cli-arrangrr/CMakeLists.txt`** (new file, the
    executable half of the split):
    ```cmake
    add_executable(cli_arrangrr main.cpp)
    set_target_properties(cli_arrangrr PROPERTIES OUTPUT_NAME arrangrr)
    target_link_libraries(cli_arrangrr PRIVATE hostrt)
    target_compile_options(cli_arrangrr PRIVATE -Wall -Wextra -Werror)
    ```
    **NEEDS-DECISION**: CMake target name `cli_arrangrr` (my recommendation,
    underscore form of the deliverable dir `cli-arrangrr`) vs. keeping
    `arrangrr_cli`. Either works; I recommend `cli_arrangrr` for
    grep-ability (`git grep cli_arrangrr` matches the dir). **The produced
    BINARY filename** (`OUTPUT_NAME arrangrr`) I recommend keeping
    UNCHANGED — demo scripts, muscle memory, and ad-hoc invocations
    (`./arrangrr …`) all assume the name `arrangrr`; renaming the binary
    itself is a separate, larger-blast-radius decision the owner should make
    explicitly, not one this restructure should force silently.
21. **`components/arrangrr/tests/CMakeLists.txt`** — remove the 5
    host-linking `arrangrr_test(...)` calls and their `target_link_libraries`
    lines (`test_host`, `test_panels`, `test_panel_nav`,
    `test_style_chooser`, `test_rc_config` — lines 82-96 of the original);
    rename `arrangrr_core` → `arrangrr` in the function body
    (`target_link_libraries(${name} PRIVATE arrangrr)`).
22. **CREATE `components/hostrt/tests/CMakeLists.txt`**:
    ```cmake
    function(hostrt_test name category)
      add_executable(${name} ${name}.cpp)
      target_link_libraries(${name} PRIVATE hostrt)
      add_test(NAME ${name} COMMAND ${name})
      set_tests_properties(${name} PROPERTIES LABELS ${category})
    endfunction()

    hostrt_test(test_host unit)
    hostrt_test(test_panels unit)
    hostrt_test(test_panel_nav functional)
    hostrt_test(test_style_chooser unit)
    hostrt_test(test_rc_config unit)
    ```
    (categories preserved verbatim from the original file.)
23. **`tests/golden/CMakeLists.txt`** — `$<TARGET_FILE:arrangrr_cli>` →
    `$<TARGET_FILE:cli_arrangrr>` (1 occurrence).
24. **`tests/integration/CMakeLists.txt`** — `$<TARGET_FILE:arrangrr_cli>` →
    `$<TARGET_FILE:cli_arrangrr>` (3 occurrences: `live_alsa`, `live_tracks`,
    `tui_console`).
25. **`apps/tools/arrstyle-converter/CMakeLists.txt`** and
    **`apps/tools/arrstyle-converter/tests/CMakeLists.txt`** — NO edit
    needed (target names `arrstyle_lib`/`arrstyle_converter` untouched,
    self-contained, relocated wholesale in Phase A step 1).

### Phase D — non-CMake edits forced by the moves — NEEDS-BUILD-EDIT

26. **`.clang-tidy`** — `HeaderFilterRegex: 'app/'` →
    `HeaderFilterRegex: 'apps/|components/'`.
27. **`scripts/lint.sh`** line 16 —
    `git ls-files -co --exclude-standard 'app/**/*.cpp'` →
    `git ls-files -co --exclude-standard 'apps/**/*.cpp' 'components/**/*.cpp'`.
28. **`scripts/coverage.sh`** — the CORE gate scope and comments hardcode
    `app/core/` / `app/platform/` / `app/tests/` / `app/core/tests/`:
    - line 55 `--filter 'app/core/'` → `--filter 'components/arrangrr/'`
    - line 56 `--exclude 'app/core/tests/'` →
      `--exclude 'components/arrangrr/tests/'`
    - line 57 `--exclude 'app/tests/'` → `--exclude 'tests/'`
    - line 95 `--filter 'app/core/' --filter 'app/platform/'` →
      `--filter 'components/arrangrr/' --filter 'components/hostrt/'`
    - line 96 `--exclude 'app/core/tests/' --exclude 'app/tests/'` →
      `--exclude 'components/arrangrr/tests/' --exclude 'components/hostrt/tests/' --exclude 'tests/'`
    - comment lines 6, 18, 23, 133, 163 — cosmetic, update text to match
      (`components/arrangrr/`, `components/hostrt/`).
    **NEEDS-DECISION**: should `components/hostrt/tests/` be excluded from
    the CORE gate scope the same way `app/core/tests/` was? I recommend YES
    (host tests were never part of the ENFORCED unit-coverage gate on the
    core; keep that boundary identical after the split) — flagged so the
    owner can override if the gate's scope should also grow to cover
    `hostrt` now that it's visibly separate.
29. **`apps/demo/clean/start.sh`** — `cd "$(dirname "$0")/../.."` →
    `cd "$(dirname "$0")/../../.."` (one more directory level: was
    `demo/clean` [2 deep], now `apps/demo/clean` [3 deep]). Same for
    **`apps/demo/jam/start.sh`**.
30. **`apps/demo/jam/start.sh`** — `SETUP="demo/jam/setup.acmd"` →
    `SETUP="apps/demo/jam/setup.acmd"`; `MOTD="demo/jam/motd.txt"` →
    `MOTD="apps/demo/jam/motd.txt"`; `source demo/lib/launch.sh` →
    `source apps/demo/lib/launch.sh`.
31. **`apps/demo/clean/start.sh`** — `HERE="demo/clean"` →
    `HERE="apps/demo/clean"`; `source demo/lib/launch.sh` →
    `source apps/demo/lib/launch.sh`; help text
    `usage: demo/clean/start.sh …` → `usage: apps/demo/clean/start.sh …`
    (cosmetic but user-facing).
32. **`apps/demo/lib/launch.sh`** line 11 —
    `CLI=build/host/app/platform/host/arrangrr` →
    `CLI=build/host/apps/tools/cli-arrangrr/arrangrr` (the build-tree output
    path mirrors the NEW source subdir of the `add_executable` call — this
    is the single highest-risk literal-path edit in the whole plan: miss it
    and every demo script silently fails to find the binary with a
    confusing "not built yet, rebuilding…" branch that then ALSO looks in
    the wrong place after rebuilding).
33. **`README.md`** — `[app/tools/arrstyle-converter/README.md]` →
    `[apps/tools/arrstyle-converter/README.md]`; same for the `DESIGN.md`
    link (`app/tools/arrstyle-converter/DESIGN.md` →
    `apps/tools/arrstyle-converter/DESIGN.md`).
34. **`apps/tools/arrstyle-converter/README.md`** — 3 literal-path
    mentions to update: `app/core/include/arrangrr/arranger/style.hpp` →
    `components/arrangrr/include/arrangrr/arranger/style.hpp`; `does not
    link \`arrangrr_core\`` → `does not link \`arrangrr\``; binary path
    `build/host/app/tools/arrstyle-converter/arrstyle-converter` →
    `build/host/apps/tools/arrstyle-converter/arrstyle-converter`.
35. **`apps/tools/arrstyle-converter/DESIGN.md`** — 4 occurrences of
    `app/core`/`app/platform`/`app/tools`/`app/tests` paths (grepped,
    contents not individually cited here — 387-line file, sweep on
    execution, not itemized to keep this plan reviewable rather than a full
    transcript).
36. **Wider documentation sweep (NOT executed by this plan, flagged as a
    follow-up)** — 14 more docs files carry 1-5 literal `app/…` path
    mentions each (28 occurrences total, grepped):
    `docs/reflections/style-data-format.md`,
    `docs/design/gui-contract-map.md`,
    `docs/reflections/hybrid-arranger-gap-analysis.md`,
    `docs/proposals/per-style-feel-swing-reauthoring.md`,
    `docs/design/gui-toolkit-decision.md`,
    `docs/reviews/followed-chord-context-ownership.md`,
    `docs/reflections/style-differentiation-and-generation.md`,
    `docs/research/yamaha-style-corpus-and-rules.md`,
    `docs/design/chord-following.md`, `docs/TUI_SPEC.md`,
    `docs/design/hooks-design-notes.md`,
    `docs/proposals/per-style-feel-values.md`, `docs/DESIGN.md`. These are
    prose references (design rationale, not build-load-bearing) — stale
    paths here degrade doc trustworthiness but do not break anything
    mechanical. Recommend a dedicated doc-sweep pass after the move lands,
    separate from this branch's build-correctness work.

### Phase E — slots (CREATE, no code) — SAFE

37. **CREATE** `components/orchestrator/README.md` — placeholder:
    "SLOT — no code yet. See `docs/design/project-structure.md` (the
    orchestrator + composable pipeline) for the design intent. Host-regime
    only (VST/audio hosting does not run on STM32)."
38. **CREATE** `components/melodd/README.md` — placeholder: "SLOT — no code
    yet. Host-only optional audio peer (D43); see
    `docs/product-identity.md` and `docs/design/workstation-vision.md`."
39. **CREATE** `components/samplrr/README.md` — placeholder: "SLOT — no
    code yet. Host-only sampler engine; see
    `docs/design/workstation-vision.md`."
    (Git does not track empty directories — each slot needs at least one
    tracked file to exist in the tree at all; a one-line README is the
    lightest such file and doubles as the "why is this empty" answer for
    anyone who opens it.)

**NOT created now**: `third_party/` and `apps/gui-sonotron/`. Per the task's
own instruction and verified above (§0), neither ImGui/GLFW nor any GUI code
exists in the tree this plan maps — they live only on unmerged branch
`spikes/gui-skeleton`. Creating an empty `third_party/` with nothing to
vendor is noise; I recommend deferring both until that spike (or its
replacement) is actually being merged. **NEEDS-DECISION** if the owner wants
the empty slots created preemptively anyway purely so the on-disk tree
matches the target diagram exactly today — trivial either way, stated so it
isn't silently skipped.

### Phase F — verification (after all moves + edits land)

40. `rm -rf build/` (stale caches from the old tree — mandatory, see §0).
41. `cmake --preset host && cmake --build --preset host && ctest --preset host`
    — full host build + all tests (unit + functional + regression labels)
    green.
42. `cmake --preset arm && cmake --build --preset arm` — freestanding
    cross-build link gate (confirms `components/arrangrr` still builds
    without heap/exceptions/RTTI for `arm-none-eabi`, and that
    `components/arrangrr/firmware` still links; confirms `hostrt`,
    `apps/tools/cli-arrangrr`, and the tools are correctly excluded from
    this preset by the `ARRANGRR_FIRMWARE` branch).
43. `cmake --preset tidy` — regenerates `build/tidy/compile_commands.json`,
    which the tracked root symlink `compile_commands.json` already points
    at; confirms `.clang-tidy`'s new `HeaderFilterRegex` matches real paths.
44. `scripts/lint.sh` — confirms the new glob in step 27 finds every
    first-party source under both `apps/**` and `components/**`.
45. `scripts/coverage.sh` — confirms the CORE gate (metric 1, ≥80%
    lines/functions/branches) still measures exactly
    `components/arrangrr/` after the filter edits in step 28, and that the
    5 split-off host tests still run and report (now under `hostrt`'s own
    `tests/` subdirectory, still discovered by the root's
    `add_subdirectory(components/hostrt/tests)`).
46. Run every `apps/demo/{clean,jam}/start.sh` manually (or at minimum
    `--help`) to confirm the `cd`/path edits in steps 29-32 are correct —
    these are the one class of edit with NO automated test coverage in this
    repo.

---

## 4. Dove va il nuovo (placement rules going forward)

- **A new arrangrr module** (new header/source inside the freestanding
  brain, e.g. a new MIDI-FX): `components/arrangrr/include/arrangrr/<module>/*.hpp`
  + `components/arrangrr/src/*.cpp` if it needs a translation unit (most of
  `arrangrr` is header-only; only 4 `.cpp` exist today for the ceremony
  files) — never `components/arrangrr/host/` or similar, the component
  stays flat and the target-regime is declared in `CMakeLists.txt`
  (`-fno-exceptions -fno-rtti`), not encoded in a subfolder.
- **A new host-only runtime facility** (new Shell command family, a new
  TUI panel, a new host adapter): `components/hostrt/<name>.{cpp,hpp}` —
  flat, same convention as today's `shell_*.cpp` files.
- **A new CLI-only concern** (argument parsing, a new subcommand's `main()`
  wiring): `apps/tools/cli-arrangrr/main.cpp` if it's part of the single
  entrypoint; a new file only if `main.cpp` (822 lines) is split — that
  split is a legitimate FUTURE Palladio candidate (oversized-file axis) but
  out of scope for this restructure.
- **A new dev/import tool** (another alien-format importer): its own
  `apps/tools/<name>/` sibling to `arrstyle-converter`, self-contained,
  dependency-free unless explicitly cleared with the owner (CLI-deps
  policy).
- **A new cross-component test** (drives the CLI end-to-end, or spans two
  components once `orchestrator` exists): `tests/golden/` (deterministic
  `.acmd`/`.golden` pair) or `tests/integration/` (OS-level, may SKIP) —
  never inside a component's own `tests/`.
- **A new component-local unit test**: beside its component, in that
  component's own `tests/` (`components/arrangrr/tests/` or
  `components/hostrt/tests/`), following the existing `arrangrr_test()` /
  `hostrt_test()` CMake function pattern.

---

## 5. Risks & flags (repeated here for the executor; see also the chat reply)

- **Highest-risk single edit**: `apps/demo/lib/launch.sh`'s `CLI=` literal
  path (step 32) — silent failure mode if missed (see step 32's note).
- **Split correctness**: the 5 host-linking tests (step 12) must be
  identified by their ACTUAL `target_link_libraries` graph, not by guessing
  from filename — verified here by reading `app/core/tests/CMakeLists.txt`
  lines 82-96, not inferred.
- **`test.hpp` duplication** (step 13) is a deliberate, small, reviewable
  divergence from strict DRY — flagged, not hidden.
- **Coverage gate scope** (step 28 decision) changes what "the core" means
  for the ENFORCED 80% gate the moment `hostrt` becomes visibly separate;
  needs an explicit owner nod even though the mechanical default (exclude
  `hostrt` same as before) is almost certainly right.
- **Binary/target naming** (steps 10, 20) are the two genuinely open
  naming questions in this plan; everything else is mechanical.
- **`spikes/gui-skeleton` collision** (§0) is out of scope but will need a
  second, smaller move-plan the day that branch is rebased onto the new
  tree — noted so it isn't a surprise.

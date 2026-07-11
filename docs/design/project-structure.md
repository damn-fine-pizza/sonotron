# Project structure — sonotron as a collection of components

Status: **REALIZED on-disk structure + componentization principles, owner-aligned 2026-07-07,
executed and committed 2026-07-11.** This document is now the canonical description of the current
repo layout (`project(sonotron)`, `apps/` · `components/` · `tests/` · `third_party/`) — no longer a
target. The restructure move-plan that carried the tree from `project(arrangrr)`/`app/` to here has
been executed and retired; its placement rules are absorbed below (see "Placement rules — where new
things go"). Companions: `product-identity.md` (arrangrr's identity),
`workstation-vision.md` (the product), `director-vocabulary.md`.

## What sonotron is

**sonotron** is the project: a **collection of musical-object components** plus the apps that
orchestrate them. The repo was once arrangrr, undifferentiated; the restructure made arrangrr
**one component among several** under the sonotron umbrella (`project(sonotron)`).

- **gui-sonotron** is the principal frontend — a **desktop app** (host, Dear ImGui) that is a
  **pure client** (D38): it talks to the backend over the UDS-JSONL socket and **never links the
  components**. It **exists now** as `apps/gui-sonotron/` (landed with the GUI spike; ImGui + GLFW
  vendored under `third_party/`). A separate backend (**sonotron-host**) links the components and
  serves that socket; it is still FUTURE (arrives with the orchestrator milestone), so today the
  socket-serving role is played by `cli-arrangrr`.
- The collection can spawn **other deliverables** (other apps, other orchestrations): `apps/` is
  extensible by construction. `cli-arrangrr` is one such — a dev/util frontend on the brain alone.

## Repo layout (current)

```
sonotron/                    project(sonotron)
├── apps/                    deliverables — orchestrators (link components) and pure clients (socket)
│   ├── gui-sonotron/        PRESENT · desktop frontend — ImGui pure client via socket (D38, no links)
│   ├── sonotron-host/       FUTURE · host backend — links the components + serves the socket
│   ├── demo/                demonstration apps (clean · jam · shared lib)
│   └── tools/               cli-arrangrr (host CLI + today's socket server) · arrstyle-converter · arrstyle-extractor
├── components/              OUR libraries — flat; target-regime is a declared property, not a folder
│   ├── arrangrr/            freestanding · the arranger (+ transport, + sequencing, + harmony,
│   │                        + midi-fx as modules)
│   ├── hostrt/              host · runtime + L0/L1 glue of arrangrr (UDS, sink, jsonl codec) — not a name-blind peer
│   ├── orchestrator/        host · SLOT — composes the component/source pipeline (design apart)
│   ├── melodd/              host · SLOT — audio engine
│   └── samplrr/             host · SLOT — sampler
├── third_party/             PRESENT · vendored external deps: imgui · glfw (host-only, off-limits to freestanding)
├── tests/                   integration + golden + arm-smoke (freestanding link gate); unit tests live in-component
├── build/                   linux-x64/ · arm64/  (per-target output)
└── cmake/ · docs/ · scripts/
```

## Principle 1 — `components/` is flat; the target boundary is a *contract*, not a folder

We do **not** split `components/` into `core/` vs `host/` sub-trees — that would force the same
axis into every top-level dir (apps/, tests/…) and read as asymmetric. Instead:

- Each component **declares its target-regime** (freestanding/STM32-capable vs host-only) in its own
  CMake. CI cross-builds the freestanding components for `arm-none-eabi` and **link-checks** them
  (`nosys.specs`). NOTE: today this proves compile+link, not symbolic heap-absence — a real no-heap
  symbol scan is outstanding hardening (flagged by Corelli's seam review), so the boundary is a
  contract to be *fully* verified, not yet a complete gate.
- The load-bearing "does it run on the chip?" boundary is therefore a **verified contract**, not a
  directory. It cannot silently erode even though the tree is flat.
- A **target specialization is a sibling component**, created only when needed — e.g.
  `arrangrr-arm64/` beside `arrangrr/` — never a pervasive folder split.

## Principle 2 — `apps/` are the deliverables (orchestrators)

An app either **links** a chosen set of components (an orchestrator/daemon) or is a **pure client**
over the socket (D38); a component is **name-blind** to its consumers (D43). So `apps/` holds:
**sonotron-host** (backend — links the components, serves the socket; future), **gui-sonotron**
(desktop frontend — pure client, links nothing; present), **cli-arrangrr** (host CLI on the brain,
also today's socket server), **demo/** (examples). **Neither firmware nor an stm32 app is a
top-level dir now** — the arm target is verified by a freestanding link gate under
`tests/arm-smoke/`; a real chip app can become `apps/stm32/` later.

## Principle 3 — `third_party/` is external and host-only

Vendored deps (Dear ImGui, GLFW) live in `third_party/`, distinct from `components/` (which is
*ours*). They are host-only by construction and never linked by a freestanding component — arrangrr
stays dependency-free.

## The component-vs-module criterion

A thing is its **own component** (a top-level library with a POD port + hooks) if it can be
**absent, substituted, or multiplied** across the deployment matrix or across a real runtime seam.
Otherwise it is a **module inside a component**. Applied today:

- **melodd, samplrr, orchestrator** → components (host, optional/absent on STM32). Slots for now.
- **harmony, midi-fx, arp, looper, groove** → modules inside `arrangrr`.
- **the sequencer** → see below (deferred split).

Corollary — **arrangrr is no longer the umbrella.** With the collection view, `arrangrr` is the
**arranger component** (decides *what* to play); the "MIDI brain" is now *the set of core
components* together, not a single lib. `product-identity.md` is to be updated accordingly.

## Sequencer — one transport, N sequencing-engines

"Sequencer" is two different things, and conflating them is what made the placement ambiguous:

1. **transport / clock master** — *the time*, the "when". There is **one** (subdivisions/polymeter
   layer on top). arrangrr cannot exist without it → **intrinsic, stays in arrangrr**.
2. **sequencing-engine (per lane)** — *what* is scheduled on a part (step, euclidean, generative…).
   These are **N** and composable. If you want two (or ten), the engine is a **component**
   (`sequencrr`), instantiated many times and wired by the orchestrator/pipeline — exactly the
   "pipeline composed of many sources" direction. The same holds for `samplrr`/`melodd` (N instances
   of one component in the pipeline).

**Owner's placement criterion:** the sequencer stays a *module in arrangrr* if arrangrr cannot
function without it; it becomes a *component* if arrangrr has reason to exist without it. **Not
now.** The move keeps transport + sequencing together inside arrangrr; `sequencrr` is extracted as a
component-engine in its **own milestone** (Corelli designs the transport↔engine port and the
N-instance model). The flat `components/` + `orchestrator/` slot are already **forward-compatible**:
adding `sequencrr` later needs no further restructure.

## The orchestrator + the composable pipeline

The orchestrator is generalized into a **component** that composes a **pipeline** of
components/sources — MIDI sequencers, sampler, virtual instruments, VST/CLAP/LV2. It has the *power*
of a DAW (routing, many sources, composable graph) but presents it through **precise workflows**,
never the linear-timeline free-for-all — that is sonotron's identity. Two constraints:

- **Host-regime.** VST/audio hosting does not run on STM32, so the rich orchestrator is a **host**
  component; on the chip the minimal wiring (arrangrr → MIDI out) is done by the arm entrypoint.
- **It is a design milestone, not a move.** The pipeline graph, the sources, the VST/audio adapters
  are architecture (Corelli/Prospero). Today that logic lives scattered in `cli-arrangrr`'s `main()`
  + the host runtime; extracting it into `components/orchestrator/` is later refactor. For the move
  we create the **slot**.

## The executed restructure (2026-07-11) — what moved, what is a slot

The restructure was planned by Palladio (a concrete `git mv` / CMake move-plan derived from this
document), seam-validated by Corelli, reviewed with the owner, and executed and committed on
2026-07-11. The one substantive CMake change was `project(arrangrr)` → `project(sonotron)`;
everything else was mechanical relocation of the `app/` tree into `apps/` · `components/` · `tests/`.
The former move-plan document has been retired; its forward-looking placement rules are absorbed in
the next section.

- **Moved** (existing code, relocated cleanly): `app/core` → `components/arrangrr`;
  `app/platform/host` lib → `components/hostrt` + its CLI exe → `apps/tools/cli-arrangrr` (target
  `cli_arrangrr`, `OUTPUT_NAME cli-arrangrr`); `app/tools/*` → `apps/tools/*`; `app/tests` →
  `tests/`; `demo/` → `apps/demo/`; `app/firmware/stub` → `tests/arm-smoke/` (freestanding link
  gate — not an app, not inside arrangrr).
- **Slots** (empty, no code yet): `components/orchestrator`, `components/melodd`, `components/samplrr`.
- **NOT touched:** the core lib was not shattered. No new internal port was introduced in the move
  (that is `sequencrr`'s future milestone).

## Placement rules — where new things go

For "which room does a new thing belong in?" These are the forward-looking rules absorbed from the
retired move-plan (§4). They are the operational corollary of Principles 1–3 above: `components/`
stays flat, `apps/` holds deliverables, cross-component tests live at the repo root.

- **A new arrangrr module** (new header/source inside the freestanding brain, e.g. a new MIDI-FX):
  `components/arrangrr/include/arrangrr/<module>/*.hpp` + `components/arrangrr/src/*.cpp` if it needs
  a translation unit (most of `arrangrr` is header-only; only 4 `.cpp` exist today for the ceremony
  files) — never `components/arrangrr/host/` or similar, the component stays flat and the
  target-regime is declared in `CMakeLists.txt` (`-fno-exceptions -fno-rtti`), not encoded in a
  subfolder.
- **A new host-only runtime facility** (new Shell command family, a new TUI panel, a new host
  adapter): `components/hostrt/<name>.{cpp,hpp}` — flat, same convention as today's `shell_*.cpp`
  files.
- **A new CLI-only concern** (argument parsing, a new subcommand's `main()` wiring):
  `apps/tools/cli-arrangrr/main.cpp` if it's part of the single entrypoint; a new file only if
  `main.cpp` is split — that split is a legitimate FUTURE Palladio candidate (oversized-file axis)
  but was out of scope for the restructure.
- **A new dev/import tool** (another alien-format importer): its own `apps/tools/<name>/` sibling to
  `arrstyle-converter`, self-contained, dependency-free unless explicitly cleared with the owner
  (CLI-deps policy).
- **A new cross-component test** (drives the CLI end-to-end, or spans two components once
  `orchestrator` exists): `tests/golden/` (deterministic `.acmd`/`.golden` pair) or
  `tests/integration/` (OS-level, may SKIP) — never inside a component's own `tests/`.
- **A new component-local unit test**: beside its component, in that component's own `tests/`
  (`components/arrangrr/tests/` or `components/hostrt/tests/`), following the existing
  `arrangrr_test()` / `hostrt_test()` CMake function pattern.

## Naming / coverage decisions resolved by the restructure

- **Target names:** `arrangrr` (freestanding lib, was `arrangrr_core`), `hostrt` (host lib, was
  `arrangrr_host`), `cli_arrangrr` (CLI exe target, dir `apps/tools/cli-arrangrr`).
- **Binary name:** the CLI exe carries `set_target_properties(cli_arrangrr PROPERTIES OUTPUT_NAME
  cli-arrangrr)`, so the produced binary is `cli-arrangrr` — the demo launch scripts
  (`apps/demo/lib/launch.sh`) invoke and `pgrep` it by that name.
- **CORE coverage gate scope:** the enforced unit-coverage gate measures only `components/arrangrr/`
  (with `components/arrangrr/tests/` and `tests/` excluded); `components/hostrt/tests/` is excluded
  from the CORE gate, preserving the pre-split boundary.

## Open / deferred
- Extracting `sequencrr` as a component-engine (transport↔engine port, N instances).
- Designing the orchestrator pipeline (sources, VST/audio adapters, the graph).
- Whether `hostrt` stays separate or folds into the orchestrator later.
- Standing up `apps/sonotron-host` (the socket-serving backend) so `gui-sonotron` no longer depends
  on `cli-arrangrr` for the socket role.

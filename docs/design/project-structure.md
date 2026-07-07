# Project structure — sonotron as a collection of components

Status: **target structure + componentization principles, owner-aligned 2026-07-07.**
Design altitude (principles + target tree), NOT yet realized in the repo. This document is the
input for Palladio's concrete move-plan (`git mv` / CMake) and Corelli's seam validation, to be
executed on branch `repo-restructure`. Companions: `product-identity.md` (arrangrr's identity),
`workstation-vision.md` (the product), `director-vocabulary.md`.

## What sonotron is

**sonotron** is the project: a **collection of musical-object components** plus the apps that
orchestrate them. Today the repo *is* arrangrr, undifferentiated; the restructure turns arrangrr
into **one component among several** under the sonotron umbrella (`project(sonotron)`).

- **gui-sonotron** is the principal frontend — a **desktop app** (host, Dear ImGui) that is a
  **pure client** (D38): it talks to the backend over the UDS-JSONL socket and **never links the
  components**. A separate backend (**sonotron-host**) links the components and serves that socket.
  Both are FUTURE (arrive with the GUI spike + the orchestrator); today the socket-serving role is
  played by `cli-arrangrr`.
- The collection can spawn **other deliverables** (other apps, other orchestrations): `apps/` is
  extensible by construction. `cli-arrangrr` is one such — a dev/util frontend on the brain alone.

## Repo layout (target)

```
sonotron/                    project(sonotron)
├── apps/                    deliverables — orchestrators (link components) and pure clients (socket)
│   ├── gui-sonotron/        FUTURE · desktop frontend — ImGui pure client via socket (D38, no links)
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
├── third_party/             external deps: dear imgui · glfw (host-only, off-limits to freestanding)
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
(desktop frontend — pure client, links nothing; future), **cli-arrangrr** (host CLI on the brain,
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

## What moves now vs. what is a slot

- **Moves now** (existing code, relocated cleanly): `app/core` → `components/arrangrr`;
  `app/platform/host` lib → `components/hostrt` + its CLI exe → `apps/tools/cli-arrangrr` (binary
  `cli-arrangrr`); `app/tools/*` → `apps/tools/*`; `app/tests` → `tests/`; `demo/` → `apps/demo/`;
  `app/firmware/stub` → `tests/arm-smoke/` (freestanding link gate — not an app, not inside arrangrr).
- **Slots** (empty, no code yet): `components/orchestrator`, `components/melodd`, `components/samplrr`.
- **NOT touched:** the core lib is not shattered. No new internal port is introduced in the move
  (that is `sequencrr`'s future milestone).

## Execution
1. Checkpoint today's design work on `main`.
2. Branch `repo-restructure`.
3. Palladio → concrete move-plan (`git mv` / CMake), reviewable, from this document; Corelli →
   validate the seams. No file moves until that plan is reviewed with the owner.

## Open / deferred
- Extracting `sequencrr` as a component-engine (transport↔engine port, N instances).
- Designing the orchestrator pipeline (sources, VST/audio adapters, the graph).
- Whether `hostrt` stays separate or folds into the orchestrator later.
- The on-disk repo-folder rename (cosmetic; `project(sonotron)` in CMake is the substantive change).

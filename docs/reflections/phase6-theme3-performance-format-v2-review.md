# `Performance format_version 2` — FX-chain + Router-scope snapshot (Phase-6 Theme 3, Item #3, node `8200`)

Corelli, DESIGN (2026-07-14). Pre-code architecture/design review, no product
code written or edited by this review. Companion piece to
`docs/phase6-design-reviews.md`, kept as a separate `docs/reflections/` file
per this reviewer's read-only-on-existing-docs discipline (mirrors
`docs/reflections/phase6-theme3-master-transpose-scope.md`'s own precedent).
Feeds an owner decision, then an implementation brief for
`nazzareno-cpp-implementor`.

## What was asked

Extend the persisted `Performance` format (`SNPF`, `arrangrr/perf/
performance.hpp`) so a recall restores, in addition to the base rig: (a) the
FX insert-chain state (`InsertChain`, per-role, Phase-5 Item #10) and (b) "the
general Router thru-matrix." Ground truth changes the shape of (b) — see
**Finding 0** below, the first and most load-bearing thing this review
establishes.

## Finding 0 — two different "Router" concepts exist; the brief's (b) is ambiguous between them

The tree has **two** distinct routing constructs, and the task brief's own
"GROUND IT" section names the first while its GOAL prose names the second:

1. **`Arranger::Route`** (`components/arrangrr/include/arrangrr/arranger/
   arranger.hpp:545-549`) — `{port, channel, enabled}`, one per `TrackRole`,
   `Arranger::m_routes[kRoleCount]` (`arranger.hpp:651`). This is the "per-role
   output route table." **It is already captured**, today, in v1: `Performance
   ::routes[10]` (`performance.hpp:79`, type `PerfRoute`), written by
   `Engine::capture_performance` (`engine.cpp:1104-1106`) and restored by
   `Engine::apply_performance` (`engine.cpp:1181-1187`). There is nothing new
   to do here.

2. **`arrangrr::Router`** (`components/arrangrr/include/arrangrr/routing/
   router.hpp:53-89`) — the general MIDI thru/soft-thru matrix: a *dynamic*,
   host-authored list of `Route{in_port, in_channel, out_port, out_channel,
   pass}` entries (`router.hpp:45-51`), bounded by `kMaxRoutes = 32`
   (`config.hpp:12`), owned by `Engine::m_router` (`engine.hpp:626`), mutated
   only through `Param::kRouteAdd`/`kRouteClear` (`engine.cpp:163-180`). **This
   is NOT captured today**, and it is not the same shape as (1): it is an
   unordered, variable-length, count-driven list, not a fixed 10-slot
   per-role table.

`performance.hpp`'s own header comment is explicit that this split is a
**prior, locked Corelli decision**, not an oversight: *"Scope: a Performance
snapshots the Arranger's per-role routing ... NOT the general Router
thru-matrix (routing/router.hpp) — the Corelli review's narrower, already-
backed v1 choice"* (`performance.hpp:20-23`). `docs/DESIGN.md` node `8200`
records the same thing as shipped fact: *"routing is per-role Arranger routes
only (not the general Router thru-matrix/Zones)"* (`docs/DESIGN.md:914`).

So the brief's (b), read literally against "the per-role output route table,"
describes work that is **already done**. Read against "the general Router
thru-matrix," it describes a **genuine scope reversal** of a locked decision.
I have treated it as the latter — the interesting, non-trivial question — and
flag the conflation explicitly so the owner is not surprised by which one
this document actually proposes.

## Cosa ho tracciato

**Decisions read:** `docs/DESIGN.md` §2 hard rules (Architectural Principles,
lines 55-66, esp. Principle #8 "one single versioned binary format ... `magic
+ version + CRC`"), line 588 ("Binary serialization ... Versioning with
migration *tool-side only* — the device reads its own version or refuses with
a message"), node `8200` (lines 897-916, the locked v1 scope + Corelli-
reviewed exclusion), §17 "Performance / Registration / Preset model" (lines
482-507, the ORIGINAL 2023-era field sketch), node `0800`/D4 (dependency-free
core, line 706), node `0700`/D26 (ABI discipline — append-only param IDs,
fixed-capacity `u16`-indexed collections, names host-only, line 697), node
`0400`/`12200`/D33 (≤512 KB envelope, `static_assert`-verified, lines 689,
1171). Also `docs/reflections/phase6-theme3-master-transpose-scope.md`
Decision 4 (lines 247-264), which explicitly deferred the `master_transpose`
`int16_t` widen to "alongside the `format_version 2` bump Theme 3 already
lists for the FX-chain/Router snapshot" — this review IS that bump.

**Code traced (include/call graph, not recalled):**
- `components/arrangrr/include/arrangrr/perf/performance.hpp` (full read,
  lines 1-353): `Performance` POD, `PerformanceStoreHeader{magic,
  format_version, count}` (already exists, lines 89-94), `kPerformanceMagic`
  = "SNPF" (99-101), `kPerformanceFormatVersion = 1` (111), explicit LE
  field-by-field `serialize()`/`deserialize()` + CRC32 (185-351),
  `PerformanceStore` bounded pool (130-158).
- `components/arrangrr/include/arrangrr/fx/insert_chain.hpp` (1-102, 267-444):
  `Insert{type, enabled, params}` = 6 B (`static_assert`, line 102);
  `InsertChain::m_inserts` is a `StaticVector<Insert,kMaxInserts>` that the
  constructor fills to full capacity (269-273) — i.e. **fixed cardinality**,
  not a variable-length list, despite the container type; `InsertChain::get
  (slot)` read accessor exists (381-383) but **no read forwarder exists on
  `Arranger`** — only `set_fx`/`set_fx_param`/`set_fx_enable`/`clear_fx`
  (`arranger.hpp:169-206`), all write-only.
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp:538` (`kRoleCount
  = 10`), `:651` (`Route m_routes[kRoleCount]`), `:662` (`InsertChain
  m_chain[kRoleCount]`).
- `components/arrangrr/include/arrangrr/routing/router.hpp` (full read):
  `Router::m_routes` = `StaticVector<Route,kMaxRoutes>` (line 88), `add`/
  `clear`/`count`/`routes()` (55-60).
- `components/arrangrr/include/arrangrr/config.hpp` (full read): `kMaxRoutes =
  32` (12), `kMaxPerformances = 16` (51), `kMaxChainFan = 8` (65) — irrelevant
  scratch buffer, not chain length.
- `components/arrangrr/include/arrangrr/abi.hpp`: `kMaxInserts = 8` (306),
  `Param::kPerformanceStore = 51`/`kPerformanceRecall = 52` (236, 240),
  `Param::kPadBankSelect = 58` (286, Item #4 already landed — confirms 59+ is
  free for any new param this item might need).
- `components/arrangrr/src/engine.cpp`: `capture_performance` (1091-1128),
  `apply_performance` (1156-1210ish), `cmd_routing`/`Param::kRouteAdd`/
  `kRouteClear` (153-180) — proves `m_router` is live, mutable, engine-owned
  state that `capture_performance` never touches today.
- `components/arrangrr/src/performance.cpp` (full read): `perf::validate` —
  the pure, Engine-free bounds-check pattern every new field must extend.
- `components/hostrt/shell_pad_commands.cpp` (1-246): `perf save`/`perf load`
  are the **only** callers of `serialize()`/`deserialize()` outside tests —
  `perf_load` (231-243) calls the core's `deserialize()` **directly**, no
  host-side translation layer exists today; `kPerfFileBufferSize` (82-84)
  sizes the *save*-side buffer off `kPerformanceRecordWireSize` symbolically
  (auto-follows a wire-size bump) — `perf_load`'s read path is unaffected
  since `read_binary_file` reads the whole file before `deserialize()` ever
  runs (233-237).
- `components/arrangrr/tests/test_performance_wire.cpp:261-273`
  (`test_deserialize_rejects_wrong_format_version`) — proves the **current**
  behavior a migrator must change: today ANY version ≠ 1 is a hard reject,
  verified by a test that flips the version byte and asserts `deserialize()`
  returns `false` untouched.

## L'architettura com'è costruita

**Confini/coupling.** `performance.hpp` is core, freestanding, dual-target by
its own header comment (lines 30-34: "the ON-DISK shape is target-agnostic
and host/firmware neutral; the file I/O ITSELF ... is HOST-ONLY"). `Insert`
and `InsertChain` (fx/insert_chain.hpp) are equally core/dual-target — no
host-only facility leaks in (no heap, no exceptions, `StaticVector` fixed
capacity throughout). `Router` (routing/router.hpp) is likewise core/dual-
target. So structurally, nothing here crosses the core/host seam wrongly —
the seam that IS crossed wrongly, or rather not yet drawn at all, is the one
Finding 3 below names: migration logic vs. the core/host boundary.

**Astrazione — asymmetric read/write on `InsertChain`.** `Arranger` exposes
four write-only forwarders onto `m_chain[role]` (`set_fx`, `set_fx_param`,
`set_fx_enable`, `clear_fx` — `arranger.hpp:169-206`) and **zero** read
forwarders, even though `InsertChain::get(slot)` (`insert_chain.hpp:381`)
already exists one layer down. This is a real, load-bearing gap for this
item: `Engine::capture_performance` cannot read a role's chain today without
a new accessor on `Arranger`. Not a defect in isolation (v1 never needed to
read a chain back), but it is exactly the kind of "abstraction that doesn't
exist where a seam is screaming for one" this review's brief asks me to
name — the FX-chain capture cannot be added without first widening
`Arranger`'s public surface with a read path.

**ABI discipline (D26/node 0700).** The existing wire discipline is textbook:
every `Performance` field is written byte-by-byte through `perf_wire::
put_u8/u16/u32` (`performance.hpp:215-247`), never a `memcpy`/
`reinterpret_cast` of the struct — the header comment explains why
(`GrooveParams` carries 2 B of implicit padding that must never cross the
wire, and host-GCC-vs-arm-none-eabi layout is not guaranteed identical,
`performance.hpp:51-57`, itself credited as a prior "Corelli fix"). `Insert`
(6 B, no internal padding on any conforming ABI given its `u8+u8+{4 B union}`
shape) is the *same class of risk*: even though it happens to be tight today,
the discipline says explicit field walk, not `sizeof`-trust, and any v2
serializer must apply it identically.

**Layering — the version discriminator already exists.** Contrary to what the
brief asked me to "determine precisely," `PerformanceStoreHeader::
format_version` (`performance.hpp:91`) and `kPerformanceFormatVersion = 1`
(`performance.hpp:111`) are **already wired end to end**: `serialize()`
writes it (`performance.hpp:300`), `deserialize()` reads and hard-checks it
(`performance.hpp:322-327`), and a dedicated test
(`test_deserialize_rejects_wrong_format_version`) pins the reject-on-mismatch
behavior. There is no "wire size is the only discriminator" ambiguity to
resolve — v1 already anticipated this exact bump. The header comment even
names it: *"A future version that adds the general Router snapshot bumps
this and gets an explicit migrator"* (`performance.hpp:106-107`) and the
`reserved[5]` field comment says the same (`performance.hpp:85`, *"future
growth → bump format_version, add a migrator"*). **This item is the tree
cashing a cheque it wrote to itself.**

## Deriva dalle decisioni

**Deriva 1 — §17's original shape for the Router field is "reference," the
brief's framing is "inline blob."** `docs/DESIGN.md` §17 (lines 482-507) is
explicit: *"It saves references + state, not heavy contents"* (line 484),
and its field sketch lists `routing_profile_id // in→out matrix + Zones`
(line 493) — an **ID into a table**, exactly the same shape as `style_id`
(`performance.hpp:65`), `chord_sequence_id` (77), and `controller_map_id`
(78), all three already implemented as `u16` IDs with `0xFFFF` = "none/keep
current." Embedding the live `Router::routes()` list inline (even bounded)
is a *different* shape than the one ever designed for this field — no
`RoutingProfileStore` (a bounded pool of NAMED router configurations,
mirroring `PerformanceStore`/`ChordSequencer`/`ClipMatrix`'s own established
"bounded pool, stable id" pattern) exists anywhere in the tree today. **Which
side should yield:** the code should follow §17's original shape, not
invent a new one under schedule pressure. A `RoutingProfileStore` is real,
separately-sized net-new work (its own bounded pool, its own author/select
ABI verbs) — it does not belong inside "bump a format version," and folding
it in now would be the kind of ceremony-vs-scope mismatch this review exists
to catch.

**Deriva 2 — node `8200`'s locked v1 scope exclusion.** Node `8200` records,
as shipped fact and *"Corelli-reviewed choice"*, that Performance v1
deliberately excludes "the general Router thru-matrix/Zones"
(`docs/DESIGN.md:914`). Extending Performance to cover it is not a bug fix or
a natural continuation — it is a **scope reversal of a locked decision**, and
per this reviewer's own mandate, that call belongs to the owner, not to an
implementation detail buried inside a "v2 bump." **Which side should
yield:** neither, automatically — this is a NEEDS-DECISION, not a drift to
silently correct. I flag it; I do not resolve it here.

**Deriva 3 — Principle #8 / line 588 vs. the brief's "migrator" ask.**
`docs/DESIGN.md` line 588 is a hard rule: *"Versioning with migration
**tool-side only** (the device reads its own version or refuses with a
message)."* The brief's deliverable #3 asks for "how a v1 record ... loads
under v2 code (defaults)" — i.e. v2 code must **succeed**, not refuse, on a
v1 buffer. The single natural seam to add that logic is inside
`performance.hpp::deserialize()` itself — but that function is core,
freestanding, dual-target (its own header comment says so,
`performance.hpp:30-34`), and it is the **same function firmware would call**
if firmware ever reads a Performance file directly. Adding v1→v2 upgrade
logic there makes the DEVICE migrate, which is exactly what line 588
forbids. **Which side should yield: the code.** The migrator must live
HOST-ONLY (`components/hostrt`, next to `Shell::perf_load` — traced at
`shell_pad_commands.cpp:231-243`, today a bare passthrough to `deserialize()`
with zero translation layer), not inside `arrangrr/perf/performance.hpp`.
The core's own `deserialize()` should keep doing exactly what it does today:
accept its own current version, refuse (return `false`) on anything else —
"refuses with a message" in firmware terms, "refuses, host retries via its
own upgrade path" in host terms.

## Proposte strutturali

**P1 — FX-chain snapshot: full, dense, per-role. [SHIPPABLE, no new dependency]**
Add `PerfInsert insert_chains[kRoleCount][kMaxInserts]` (a wire-mirrored,
explicit-LE `Insert`: `type: u8, enabled: u8, params: 4 raw bytes`, 6 B/slot,
same "never `memcpy`" discipline as the rest of the record). Cost: `kRoleCount
(10) × kMaxInserts (8) × 6 B = 480 B` per `Performance` record, wire and
in-memory alike (no padding either shape, `Insert`'s layout has none). This
mirrors the *already-shipped* `routes[10]`/mute/solo per-role pattern exactly
— fixed cardinality, no count prefix, no variable-length parsing — and keeps
`kPerformanceRecordWireSize` a compile-time constant, which the whole
`serialize()`/`deserialize()`/`kPerfFileBufferSize` machinery structurally
depends on (`needed = header + count × kPerformanceRecordWireSize + crc`,
`performance.hpp:293-294,328-330`; `shell_pad_commands.cpp:82-84`). A sparse/
compact "only populated slots" encoding was considered and rejected: at
`kMaxPerformances(16) × 480 B ≈ 7.5 KB` against a 512 KB envelope (1.5%), the
budget argument for compaction does not exist — the honest reason to reject
compaction is that it would require a variable-length sub-record inside an
otherwise fixed-size record, adding real parsing risk for zero budget
benefit. Requires a NEW read forwarder on `Arranger` (`const InsertChain&
chain(TrackRole) const noexcept`, or equivalent) — today write-only, per
Finding above; this is new, small, core-side surface, not a dependency.

**P2 — Router: do NOT inline-capture in v2; reserve the field. [NEEDS-DECISION]**
Add `std::uint16_t routing_profile_id = 0xFFFF;` (2 B) to `Performance`,
following §17's original shape and the sibling `style_id`/`chord_sequence_id`
/`controller_map_id` pattern — reserved, unbacked, `perf::validate()` accepts
only `0xFFFF` for now (exactly `controller_map_id`'s current treatment,
`performance.cpp` has no check on it today because it is unbuilt). This buys
the byte-slot and the version bump now, at zero design debt, and defers the
real work — a bounded `RoutingProfileStore` (pool of named `Router`
configurations) plus author/select ABI verbs — to its own future item, sized
and reviewed on its own terms. **Fork for the owner:** if the owner instead
wants LIVE `Router` state captured inline NOW (reversing node `8200`'s
locked exclusion), the structurally least-bad shape is a small FIXED slice —
e.g. `RouteSnap route_thru[8]` (8 of `Router`'s 32 possible entries, 5 B/each
= 40 B, sentinel `pass == 0` marks an inactive slot) — never the live
`Router::routes()` list directly, which is variable-length and would force
`serialize()`/`deserialize()`/`kPerfFileBufferSize` off the fixed-record-size
invariant they are built on. This fork is explicitly a scope call, not an
engineering default; I recommend the reserved-field option (byte-cost 2 B vs.
40 B is irrelevant next to the 512 KB envelope — the recommendation is about
respecting the locked scope and the reference-not-blob shape, not RAM).

**P3 — `master_transpose` widen to `std::int16_t`. [SHIPPABLE, bundle into this bump]**
Same byte width (2 B → 2 B), so zero wire-size cost. Formalizes what Item #1
already implemented as a low-byte reinterpret hack
(`performance.hpp:67-74`) into the field's real type, exactly as Ottorino's
own review pre-blessed: *"only worth it alongside the `format_version 2`
bump Theme 3 already lists for the FX-chain/Router snapshot"*
(`docs/reflections/phase6-theme3-master-transpose-scope.md:258-261`).
Recommend bundling — it is free once the migrator/version-branch code exists
regardless, and removes a wart.

**P4 — the v1→v2 migrator lives HOST-ONLY, next to `Shell::perf_load`. [SHIPPABLE, no dependency]**
Per Deriva 3: the core's `deserialize()` keeps its current, unchanged
contract — accept `format_version == kPerformanceFormatVersion` (now `2`),
refuse (return `false`) on anything else, byte-for-byte the same discipline
`test_deserialize_rejects_wrong_format_version` already locks in. A NEW
host-only function (e.g. `arrangrr::host::migrate_performance_v1_to_v2` in
`components/hostrt`) owns: parsing a v1 buffer with the OLD, frozen v1 stride
(`kPerformanceRecordWireSizeV1 = 94`, kept as a named historical constant,
never reused for anything live), constructing an in-memory `PerformanceStore`
with every v2-only field defaulted (`insert_chains` = all-default/inert
`Insert{}`, `routing_profile_id = 0xFFFF`), and handing that store to
`Shell::perf_load`'s existing success path. `Shell::perf_load` tries
`deserialize()` (native v2) first; on failure, tries the v1 migrator; on
that also failing, reports the existing "bad performance file" error
unchanged. No firmware code path gains version-branching it does not have
today — device-side recall stays "load a v2 project or don't," matching line
588 exactly.

## Cosa ho flaggato / cosa decide il proprietario

- **The (b) conflation itself (Finding 0).** Confirm whether "Router" in this
  item means the already-captured `Arranger::Route` (nothing to build) or the
  general `Router` thru-matrix (a locked-scope reversal). This review assumed
  the latter throughout; if the former was meant, Item #3 is FX-chain-only
  and P2/Deriva 1/Deriva 2 fall away entirely.
- **P2's fork (reserved field vs. inline `route_thru[8]`)** is an explicit
  scope call on reopening node `8200`'s locked exclusion — not mine to make.
- **Deriva 3 (host-only migrator placement)** is a structural correction I am
  confident in (it is a direct, cited application of DESIGN.md line 588), but
  it does change where `nazzareno-cpp-implementor` will need to write code —
  `components/hostrt`, not `arrangrr/perf/performance.hpp` — flagging so the
  implementation brief scopes the right component.
- **No new dependency anywhere in P1-P4.** Everything proposed stays within
  already-present core types (`StaticVector`, explicit LE wire helpers,
  `CRC32`) and the already-established host/core split — no CLI-deps flag
  needed.
- **P1's new `Arranger` read forwarder** is a small, uncontroversial ABI/API
  surface addition, but it IS a new public method on a core class — naming it
  here so the implementation brief doesn't treat it as a trivial oversight
  fix; it is net-new surface earning its own test.

## File toccati da questa review (solo lettura)

- `components/arrangrr/include/arrangrr/perf/performance.hpp`
- `components/arrangrr/include/arrangrr/fx/insert_chain.hpp`
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp`
- `components/arrangrr/include/arrangrr/routing/router.hpp`
- `components/arrangrr/include/arrangrr/config.hpp`
- `components/arrangrr/include/arrangrr/abi.hpp`
- `components/arrangrr/src/engine.cpp`
- `components/arrangrr/src/performance.cpp`
- `components/hostrt/shell_pad_commands.cpp`
- `components/arrangrr/tests/test_performance_wire.cpp`
- `docs/DESIGN.md`
- `docs/reflections/phase6-theme3-master-transpose-scope.md`

Nessun file di prodotto è stato modificato. Questo documento è l'unica
scrittura di questa review.

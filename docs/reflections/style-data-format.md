# Reflection — A pure-data (non-compiled) style format

Status: proposal / direction. Read-only branch context: `styles-modern-vocab`.
Author: Prospero (dual-axis critique). Scope: DIRECTION, not code.

**Owner decision (roadmapped, not now — see DESIGN D44):** keep BOTH
representations. Add a pure-data format for interchange/authoring/runtime-loading
AND a generator that emits the C++-style form from the data, so the STM32 target
can keep using compiled styles. Foreseen SW pieces: a style *inspector*,
*generators*, *serialize/deserialize*, a *style compiler* (data → `.cpp`), and a
*non-binary* interchange format (an on-device text parser stays refused). This
confirms Prospero's layered verdict and its "smallest first step" (teach
`arrstyle-converter` to emit the constexpr header from its `StyleModel`).

## Sul tavolo (the reflection, restated)

Today arranger STYLES are C++ source: `inline constexpr StyleEvent[]` tables
wired into `StylePattern` / `StyleSection` / `Style` (16 built-ins under
`app/core/include/arrangrr/arranger/styles/`). They are POD + `Span`, land in
flash memory-mapped on the STM32 target, cost zero RAM and zero parsing (D32/D33).
The question: does a NON-compiled, pure-data style representation make sense, to
unlock what, and in what format — given a freestanding, no-heap, no-filesystem
core that must remain zero-copy-from-flash. The likely answer is layered: an
authoring format, an on-device format, and the built-ins' baking policy are three
DIFFERENT layers and must be judged separately.

## The one fact that decides the whole format question

Only `StyleEvent` is serialisable as-is: it is 10 bytes of value fields, no
pointers (`static_assert(sizeof==10)`). Every container above it —
`StylePattern`, `StyleSection`, `Style` — carries a `Span<const T>` (a raw
pointer + `size_t`) and `Style::name` is a `const char*`. Spans are NOT
serialisable and NOT portable: their width differs host (16 B) vs arm32 (8 B),
and a stored pointer is meaningless after a load or across a flash image.

Therefore the format split is forced, and it matches DESIGN.md line 260 exactly
("references are indices/IDs into tables, not pointers — serialization is
memcpy-friendly and relocatable"): **leaf event data is blittable and shared;
the index structure is offset-based on disk and rebuilt into pointer-bearing
`Span`s at load.** This is not a preference. It is the only honest encoding.

---

## Payoffs — judged on both axes

### Tenere

- **Ingesting the 1010-style Yamaha corpus without a recompile.** Engineering:
  the corpus already lowers into the converter's host `StyleModel`; the only
  missing link is an emitter to a device-consumable artifact. Musical: this is
  the actual prize — a borrowed-chord, real-world accompaniment vocabulary an
  order of magnitude past 16 hand-authored styles. Intersection: the technical
  change (a data format) directly serves the musical ambition, and the ambition
  respects the machine (the event payload is small — `events * 10 B`, far below
  the ~20-28 KB of C++ *source* per style; 1010 blobs live comfortably on the
  SD/flash storage D33 already anticipates). This survives on both axes.

- **Runtime-loadable user styles.** Engineering: deliverable with a bounded,
  no-heap loader (below) and the storage HAL D33 already foresees. Musical: a
  player expects to bring their own grooves; a fixed factory set is a ceiling.
  Intersection: worth doing, but it is strictly downstream of the format+loader,
  so it is a *consequence* to keep, not the first step.

### Rilavorare

- **Hot-reload while authoring.** Half-right. On the HOST it is genuinely useful
  (edit JSON → re-emit → reload) and nearly free. On the DEVICE it is a mirage:
  it tempts an on-device parser/watcher that would allocate. What it must become:
  a host-only affordance in the converter/sim loop, explicitly NOT a device
  feature. Fails the engineering axis only if smuggled onto the target.

- **Separating musical content from the firmware build.** Half-right and worth
  banking, but do not overshoot. The correct form: musical content is authored
  as data and *mechanically* lowered; it no longer requires hand-writing C++.
  The wrong form it must NOT become: "nothing is baked, everything loads at
  runtime." The factory set staying baked (below) is a feature, not debt.

### Buttare

- **A style library / marketplace as a design driver.** Kill it as a premature
  scope. Engineering: designing the format around distribution/versioning/DRM
  fantasies now corrupts a decision that should be driven by the machine's
  constraints. Musical: irrelevant to whether a single style sounds right. The
  chosen blob enables a marketplace later for free; that is the correct amount of
  attention to pay it today — none.

---

## The format — a three-layer answer

The layers are distinct artifacts with distinct owners. Do not collapse them.

### Layer A — Authoring / interchange format (HOST-ONLY): JSON, already exists

`arrstyle-converter` already has a host `StyleModel` and a byte-stable JSON
writer (`serialize.hpp`, schema v1). That IS the authoring/interchange layer.
Keep it. Every importer (SFF/CASM, MIDI, ChordPro) already targets `StyleModel`;
JSON is its serialisation. A hand-authoring DSL/TOML is a *possible* future
sugar on top, but it is not required and must never gain an on-device reader.
Verdict: reuse, do not reinvent.

### Layer B — On-device format: a flat, mmap-able, offset-based binary blob

The device artifact. One file per style (working name `.arrsty`). Properties,
each justified:

- **Little-endian, fixed alignment (>= 4).** Host x86 and Cortex-M7 are both LE
  — declare LE and `static_assert`/reject otherwise. `StyleEvent` has `uint16`
  fields, so it needs 2-byte alignment; align section starts to 4. `packed` is
  refused on Cortex-M7 (DESIGN.md line 627: alignment/perf) — the blob is padded,
  not packed.

- **Versioned header carrying the invariants.** magic (`'A''S''T''Y'`),
  `format_version`, and `event_size` which MUST equal the in-RAM
  `sizeof(StyleEvent)` (== 10). The 10-byte invariant is thus enforced across the
  disk/RAM boundary: a blob built for a different event layout is rejected at
  load, not silently misread. The header also holds section count and the offset
  to the section table.

- **Offset-based index, blittable leaves.** The section table, pattern table and
  name are byte offsets from the blob base. The `StyleEvent[]` arrays are stored
  verbatim (blittable — this is where zero-copy is preserved). On-disk record vs
  in-RAM record: the LEAF (`StyleEvent`) is identical on disk and in RAM (shared,
  not copied); the INDEX structs (`Style`/`StyleSection`/`StylePattern`) are NOT
  serialised — they are reconstructed at load, because they hold `Span`s.

Sketch (illustrative, not a spec):

```
Header      : magic u32 | format_version u16 | event_size u16 (==10)
              | section_count u16 | _pad u16 | name_off u32 | section_table_off u32
SectionRec  : type u8 | bars u8 | pattern_count u16 | pattern_table_off u32
PatternRec  : role u8 | policy u8 | voicing u8 | _pad u8
              | gm_program i16 | _pad u16 | event_count u32 | event_off u32
Events      : StyleEvent[]  (verbatim, 10 B each, 4-aligned section start)
Name        : NUL-terminated UTF-8
```

### Layer C — Built-ins: STAY baked as constexpr

Keep `kBuiltins[16]` compiled into firmware. They cost zero RAM (flash mmap),
they are guaranteed present, and they CANNOT fail to load — the factory floor a
performer relies on. Longer term the same authoring JSON can emit BOTH the baked
constexpr header and the blob from one pipeline, so built-ins and user styles
share a source of truth; that convergence is desirable but is not step one.

---

## The loader story — no heap, no filesystem

The question "how does the device load pure data with no heap and no fs" has a
precise answer that keeps the fire loop untouched:

1. The blob arrives via the storage HAL (SD/internal flash, D33) or is
   memory-mapped from a flash region — its base address is known.
2. **Validate**: magic, `format_version`, `event_size == sizeof(StyleEvent)`,
   counts within the static caps. Reject → keep the current style; never stall
   the fire loop.
3. **Adapt-in-place into a pre-sized static pool.** Walk the section/pattern
   tables and build a `Style` + `StyleSection[]` + `StylePattern[]` in a
   `StaticVector` sized to `kMaxSections` / `kMaxPatterns` (D32 caps, verified by
   `static_assert`). Each rebuilt `Span<const StyleEvent>` points at
   `blob_base + event_off` — the event arrays are consumed ZERO-COPY straight
   from the mmap'd blob; only the lightweight index is materialised in RAM. Name
   pointer = `blob_base + name_off`. This is filling a pool, not allocating — the
   exact distinction D32 draws.
4. The arranger keeps holding `const Style*` and iterating `Span`s. **Zero fire-
   loop changes.** For seamless style switch, use two index pools (double buffer),
   consistent with the existing `m_pending_style` swap.

What happens to `Span` in a relocatable blob: it does not exist on disk. Pointers
are computed once, at load, as `base + offset`, and live only in the RAM index.
Endianness: LE, asserted. Alignment: padded to 4, never `packed`. Versioning: in
the header, and the 10-byte event invariant is a load-time gate, not a hope.

---

## Migration path and composition with the existing tooling

The codegen technique exists (`emit_canon_header` proves the "host program emits
a freestanding `<cstdint>`-only artifact" pattern). Reuse the *technique*, not the
canon-builder itself — canon-builder distils statistical genre aggregates, not
full `StyleEvent` tables, so it is a cousin, not the tool.

- **Step 0 (smallest, zero device risk) — the recommended first step.**
  Add to `arrstyle-converter` an emitter that lowers its host `StyleModel` into a
  `style_model.hpp`-shaped constexpr header (the exact form `bossa.hpp` has today).
  Effect: styles become authored-as-DATA (JSON) and mechanically lowered to the
  EXISTING baked format. This alone unlocks ingesting the 1010 corpus into
  built-ins and kills hand-authoring of C++ tables — with NO core change and NO
  new runtime code. It still requires a recompile, and that is acceptable for the
  first step because it de-risks the lowering (event resolution, section mapping,
  gate/PPQN units) against the compiler before any binary loader exists.

- **Step 1 — the real payoff.** Define Layer B (the flat blob), add the
  `StyleModel -> .arrsty` emitter (same lowering, different sink), and write the
  no-heap adapt-in-place loader into the static index pool. This is what removes
  the recompile and delivers runtime-loadable styles.

- **Step 2 — convergence.** Emit both baked header and blob from one JSON so
  built-ins and user styles share a source of truth. Optional.

## Non-goals / traps to refuse (explicit)

- An on-device JSON/TOML parser, or any human-readable format parsed on the
  target. Humans author on the HOST. On device this only buys allocation and
  parse latency on a path that may run during a live style switch. Refuse (D32).
- Any format whose load path allocates. Refuse (D32).
- Serialising the `Span`-based structs directly (raw pointers / target-width
  layout). Meaningless and non-portable. Refuse.
- `packed` structs on Cortex-M7. Refuse (DESIGN.md line 627).
- Growing `StyleEvent` past 10 bytes to carry on-disk metadata. Put metadata in
  the blob HEADER; keep the leaf invariant. Refuse the growth.
- Designing for a marketplace now. Refuse; the blob enables it later for free.
- Dropping the baked built-ins in favour of "everything loads." Refuse; the
  factory set's zero-RAM, cannot-fail guarantee is a feature.

## Verdetto

The reflection is sound at its core and the answer is unambiguously layered:
JSON stays the host authoring/interchange format (it already exists), the
built-ins stay baked constexpr (zero-RAM, cannot fail), and the genuinely new
thing is a flat, little-endian, offset-based on-device blob whose blittable
`StyleEvent` leaves are consumed zero-copy from flash while its pointer-bearing
index is rebuilt into a bounded static `Span` pool at load — no heap, no parser,
no filesystem dependency, fire loop untouched. The one sin to guard against is
the on-device text-parser fantasy: it would put allocation and parse latency onto
a path that can fire during a live style switch, trading a real musical timing
guarantee for a convenience nobody needs on the target. Smallest first step:
teach `arrstyle-converter` to emit the existing constexpr header shape from its
`StyleModel` — data authoring with zero device risk — then, and only then, add
the blob and its loader for the no-recompile payoff.

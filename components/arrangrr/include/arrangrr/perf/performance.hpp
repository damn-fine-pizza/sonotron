#pragma once

#include <cstddef>
#include <cstdint>

#include "arrangrr/arranger/groove.hpp"  // GrooveParams
#include "arrangrr/common/crc.hpp"       // crc32
#include "arrangrr/common/span.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"  // kMaxPerformances

// Performance (Phase-5 Item #9, docs/phase5-design-reviews.md "Pad/Scene live
// -> Performance"): the one-button full-state recall preset (DESIGN.md
// section 17, node 8200). Deliberately NOT the GUI's Repeat-Zone "Scene" (a
// clip-column, apps/gui-sonotron/src/grid_model.hpp) and NOT DESIGN.md
// section 7's Song-anchored timed Scene (node 8100, a separate, later
// concern) -- `Performance` is the only word this file uses for its own
// concept (locked decision).
//
// Scope: a Performance snapshots the Arranger's per-role routing
// (Arranger::m_routes / part_info()), NOT the general Router thru-matrix
// (routing/router.hpp) -- the Corelli review's narrower, already-backed v1
// choice.
//
// PerformanceStore is implicitly scoped to the LIVE project: a slot number is
// meaningful only within one running session / one loaded project file --
// there is no cross-project slot namespace (mirrors ChordSequencer's own
// single pool, chord_sequencer.hpp).
//
// This is the first real exercise of Architectural Principle #8 ("one single
// versioned binary format... magic + version + CRC"): the ON-DISK shape is
// target-agnostic and host/firmware neutral; the file I/O ITSELF (fopen/
// fstream) is HOST-ONLY (a HAL the core never touches, D-locked Principle #2)
// -- see components/hostrt's `perf save`/`perf load` L1 verbs.

namespace arrangrr {

// One routed destination for an Arranger part (mirrors Arranger::Route, but
// POD/u8-typed for the wire: `enabled` rides a full byte, not a bitfield/
// bool, so the explicit field-by-field (de)serializer below never has to
// reason about bit-packing).
struct PerfRoute {
  std::uint8_t port = 0;
  std::uint8_t channel = 0;
  std::uint8_t enabled = 0;
};

// Field order is alignment-optimal (widest members first): char[24], the two
// u32 masks, GrooveParams (alignof 4), the u16 cluster, the PerfRoute[10]
// table (alignof 1), then the trailing u8 cluster + reserved pad.
// sizeof(Performance) == 96 is a RAM/flash BUDGET check ONLY -- the ON-DISK
// format is the EXPLICIT field-by-field little-endian byte stream
// serialize()/deserialize() below produce, never a memcpy/reinterpret_cast of
// this struct: GrooveParams carries 2 bytes of implicit inter-field padding
// (between `quantize` and `seed`) whose value must never cross the wire, and
// host GCC vs arm-none-eabi struct layout is not guaranteed identical
// (Corelli fix).
struct Performance {
  char name[24]{};  // host-set display name (D26: the core ABI never carries a string
                    // itself -- see PerformanceStore::get()'s mutable overload)
  std::uint32_t track_mute_mask = 0;  // bit r = TrackRole r muted (the core's 10-role space,
                                      // NOT the GUI's 9-role display subset)
  std::uint32_t track_solo_mask = 0;  // bit r = TrackRole r soloed
  GrooveParams groove{};
  std::uint16_t style_id = 0xFFFF;  // builtin style index, or 0xFFFF = keep current
  std::uint16_t tempo_x100 = 0;
  std::uint16_t master_transpose = 0;  // RESERVED: no engine backing yet -- ALWAYS 0
                                       // (locked decision; do not wire without an
                                       // explicit owner/musical-scope decision)
  std::uint16_t pad_bank_id = 0;
  std::uint16_t chord_sequence_id = 0xFFFF;  // 0xFFFF = none
  std::uint16_t controller_map_id = 0xFFFF;  // 0xFFFF = none (unbuilt today; reserved)
  PerfRoute routes[10]{};                    // indexed by the core's TrackRole (kRoleCount == 10)
  std::uint8_t variation = 0;                // SectionType
  std::uint8_t chord_mode = 0;               // ChordMode
  std::uint8_t chord_follow = 0;             // ChordFollow
  std::uint8_t key_root = 0;                 // pitch class 0..11
  std::uint8_t key_mode = 0;                 // Mode
  std::uint8_t reserved[5]{};                // future growth -> bump format_version, add a migrator
};
static_assert(sizeof(Performance) == 96, "Performance RAM/flash budget pin (D33)");

struct PerformanceStoreHeader {
  std::uint32_t magic = 0;
  std::uint16_t format_version = 0;
  std::uint16_t count = 0;
};
static_assert(sizeof(PerformanceStoreHeader) == 8);

// "SNPF" (Sonotron Performance Format), stored little-endian byte-for-byte
// (byte 0 == 'S') so a hex dump of the file reads the mnemonic directly --
// the same convention as RIFF/PNG chunk magics.
inline constexpr std::uint32_t kPerformanceMagic =
    static_cast<std::uint32_t>('S') | (static_cast<std::uint32_t>('N') << 8) |
    (static_cast<std::uint32_t>('P') << 16) | (static_cast<std::uint32_t>('F') << 24);

// format_version 1 is the subset shipped now (Phase-5 Item #9): every field
// above. A future version that adds the general Router snapshot or a real
// master_transpose bumps this and gets an explicit migrator in deserialize()
// (Architectural Principle #8's own discipline -- a persisted format's
// version bump is visible and versioned, unlike the live wire ABI's
// same-recompile discipline).
inline constexpr std::uint16_t kPerformanceFormatVersion = 1;

// Explicit TIGHT (no padding) wire size of one Performance record: the sum of
// its LOGICAL field widths. Deliberately SMALLER than sizeof(Performance) ==
// 96 because it drops GrooveParams' 2 bytes of implicit in-memory padding
// (see the struct comment above) -- the wire format is not required to match
// the in-memory layout.
inline constexpr std::size_t kPerformanceRecordWireSize =
    24 /* name */ + 4 /* track_mute_mask */ + 4 /* track_solo_mask */ +
    10 /* groove: 6 u8 fields + u32 seed */ + 2 /* style_id */ + 2 /* tempo_x100 */ +
    2 /* master_transpose */ + 2 /* pad_bank_id */ + 2 /* chord_sequence_id */ +
    2 /* controller_map_id */ + 10 * 3 /* routes[10] */ + 1 /* variation */ + 1 /* chord_mode */ +
    1 /* chord_follow */ + 1 /* key_root */ + 1 /* key_mode */ + 5 /* reserved */;
static_assert(kPerformanceRecordWireSize == 94);

// Bounded pool of Performance slots, addressed directly by slot number (NOT a
// sequential push_back id like ClipMatrix/ChordSequencer -- a Performance
// slot is a stable, host-chosen "preset number", so store() overwrites in
// place or grows by exactly one).
class PerformanceStore {
 public:
  // Stores `perf` at `slot`: overwrites an existing slot in place, or grows
  // the pool by exactly one when `slot == size()`. Returns false for an
  // out-of-range or gap-leaving slot (slot > size()).
  [[nodiscard]] bool store(std::size_t slot, const Performance& perf) noexcept {
    if (slot < m_pool.size()) {
      m_pool[slot] = perf;
      return true;
    }
    if (slot == m_pool.size()) {
      return m_pool.push_back(perf);
    }
    return false;
  }
  const Performance* get(std::size_t slot) const noexcept {
    return slot < m_pool.size() ? &m_pool[slot] : nullptr;
  }
  // Mutable access: naming a slot (Performance::name) or a file-load restore
  // are host-only operations reached through this accessor -- the core ABI
  // never carries a string itself (D26).
  Performance* get(std::size_t slot) noexcept {
    return slot < m_pool.size() ? &m_pool[slot] : nullptr;
  }
  std::size_t size() const noexcept { return m_pool.size(); }

 private:
  StaticVector<Performance, kMaxPerformances> m_pool;
};

// QA gate restoration (Phase-5 Item #9 follow-up): the PURE bounds-check half
// of what used to be Engine::validate_performance's entire body, extracted so
// it is unit-testable in isolation, WITHOUT an Engine instance (before this
// split it was reachable only through functional Engine tests and did not
// count toward the enforced metric-1 unit-coverage gate).
namespace perf {

// Validates a Performance's referenced ids/fields against static bounds:
// style_id (0xFFFF or < the builtin style table size), variation (a real
// SectionType), the track mute/solo masks (fit the core's 10-role space),
// key_root/key_mode, chord_mode, chord_follow, and every route's port/
// channel. `chord_sequence_count` is passed in (the ONE value that needs
// live state -- ChordSequencer::count(), Engine-owned) so this function
// itself stays pure, freestanding, and Engine-free. Defined in
// arrangrr/src/performance.cpp, NOT inline here: the bounds it checks
// (styles::kBuiltinCount, ChordMode, ChordFollow, Mode) live behind headers
// (arranger/style.hpp pulls in all 16 constexpr style tables) this header
// would otherwise have to carry into every one of ITS includers -- engine.hpp
// among them -- for no benefit to callers that only want serialize()/
// deserialize() or the plain POD shapes above.
bool validate(const Performance& p, std::size_t chord_sequence_count) noexcept;

}  // namespace perf

namespace perf_wire {

// Little-endian byte writers/readers shared by serialize()/deserialize()
// below -- explicit field-by-field, never a memcpy of Performance/
// GrooveParams (see the struct's own header comment for why).
inline void put_u8(Span<std::uint8_t> out, std::size_t& off, std::uint8_t v) noexcept {
  out[off] = v;
  ++off;
}
inline void put_u16(Span<std::uint8_t> out, std::size_t& off, std::uint16_t v) noexcept {
  put_u8(out, off, static_cast<std::uint8_t>(v & 0xFFu));
  put_u8(out, off, static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}
inline void put_u32(Span<std::uint8_t> out, std::size_t& off, std::uint32_t v) noexcept {
  put_u16(out, off, static_cast<std::uint16_t>(v & 0xFFFFu));
  put_u16(out, off, static_cast<std::uint16_t>((v >> 16) & 0xFFFFu));
}
inline std::uint8_t get_u8(Span<const std::uint8_t> in, std::size_t& off) noexcept {
  return in[off++];
}
inline std::uint16_t get_u16(Span<const std::uint8_t> in, std::size_t& off) noexcept {
  const std::uint16_t lo = get_u8(in, off);
  const std::uint16_t hi = get_u8(in, off);
  return static_cast<std::uint16_t>(lo | static_cast<std::uint16_t>(hi << 8));
}
inline std::uint32_t get_u32(Span<const std::uint8_t> in, std::size_t& off) noexcept {
  const std::uint32_t lo = get_u16(in, off);
  const std::uint32_t hi = get_u16(in, off);
  return lo | (hi << 16);
}

inline void put_record(Span<std::uint8_t> out, std::size_t& off, const Performance& p) noexcept {
  for (char c : p.name) {
    put_u8(out, off, static_cast<std::uint8_t>(c));
  }
  put_u32(out, off, p.track_mute_mask);
  put_u32(out, off, p.track_solo_mask);
  put_u8(out, off, p.groove.swing);
  put_u8(out, off, p.groove.humanize_timing);
  put_u8(out, off, p.groove.humanize_velocity);
  put_u8(out, off, p.groove.accent);
  put_u8(out, off, p.groove.swing_grid);
  put_u8(out, off, p.groove.quantize);
  put_u32(out, off, p.groove.seed);
  put_u16(out, off, p.style_id);
  put_u16(out, off, p.tempo_x100);
  put_u16(out, off, p.master_transpose);
  put_u16(out, off, p.pad_bank_id);
  put_u16(out, off, p.chord_sequence_id);
  put_u16(out, off, p.controller_map_id);
  for (const PerfRoute& r : p.routes) {
    put_u8(out, off, r.port);
    put_u8(out, off, r.channel);
    put_u8(out, off, r.enabled);
  }
  put_u8(out, off, p.variation);
  put_u8(out, off, p.chord_mode);
  put_u8(out, off, p.chord_follow);
  put_u8(out, off, p.key_root);
  put_u8(out, off, p.key_mode);
  for (std::uint8_t b : p.reserved) {
    put_u8(out, off, b);
  }
}

inline void get_record(Span<const std::uint8_t> in, std::size_t& off, Performance& p) noexcept {
  for (char& c : p.name) {
    c = static_cast<char>(get_u8(in, off));
  }
  p.track_mute_mask = get_u32(in, off);
  p.track_solo_mask = get_u32(in, off);
  p.groove.swing = get_u8(in, off);
  p.groove.humanize_timing = get_u8(in, off);
  p.groove.humanize_velocity = get_u8(in, off);
  p.groove.accent = get_u8(in, off);
  p.groove.swing_grid = get_u8(in, off);
  p.groove.quantize = get_u8(in, off);
  p.groove.seed = get_u32(in, off);
  p.style_id = get_u16(in, off);
  p.tempo_x100 = get_u16(in, off);
  p.master_transpose = get_u16(in, off);
  p.pad_bank_id = get_u16(in, off);
  p.chord_sequence_id = get_u16(in, off);
  p.controller_map_id = get_u16(in, off);
  for (PerfRoute& r : p.routes) {
    r.port = get_u8(in, off);
    r.channel = get_u8(in, off);
    r.enabled = get_u8(in, off);
  }
  p.variation = get_u8(in, off);
  p.chord_mode = get_u8(in, off);
  p.chord_follow = get_u8(in, off);
  p.key_root = get_u8(in, off);
  p.key_mode = get_u8(in, off);
  for (std::uint8_t& b : p.reserved) {
    b = get_u8(in, off);
  }
}

}  // namespace perf_wire

// Serializes `store` into `out` as {header, count records, trailing CRC-32}
// (Architectural Principle #8's `magic + version + CRC` shape, computed over
// the header + every record). Returns the number of bytes written, or 0 when
// `out` is too small -- the caller must grow its buffer and retry; no partial
// write is implied by a nonzero-but-wrong length, since a 0 return is the
// ONLY failure signal.
inline std::size_t serialize(const PerformanceStore& store, Span<std::uint8_t> out) noexcept {
  const std::size_t count = store.size();
  const std::size_t needed =
      sizeof(PerformanceStoreHeader) + count * kPerformanceRecordWireSize + sizeof(std::uint32_t);
  if (out.size() < needed) {
    return 0;
  }
  std::size_t off = 0;
  perf_wire::put_u32(out, off, kPerformanceMagic);
  perf_wire::put_u16(out, off, kPerformanceFormatVersion);
  perf_wire::put_u16(out, off, static_cast<std::uint16_t>(count));
  for (std::size_t i = 0; i < count; ++i) {
    const Performance* p = store.get(i);
    perf_wire::put_record(out, off, *p);
  }
  const std::uint32_t crc = crc32(Span<const std::uint8_t>(out.data(), off));
  perf_wire::put_u32(out, off, crc);
  return off;
}

// Deserializes `in` into `store`: validates magic, format version, the
// record-count bound (<= kMaxPerformances), and the trailing CRC-32 BEFORE
// touching `store` -- a bad buffer leaves `store` untouched (returns false,
// no partial load), the same semantic-atomicity discipline
// Engine::apply_performance applies one layer up.
inline bool deserialize(Span<const std::uint8_t> in, PerformanceStore& store) noexcept {
  if (in.size() < sizeof(PerformanceStoreHeader) + sizeof(std::uint32_t)) {
    return false;
  }
  std::size_t off = 0;
  const std::uint32_t magic = perf_wire::get_u32(in, off);
  const std::uint16_t version = perf_wire::get_u16(in, off);
  const std::uint16_t count = perf_wire::get_u16(in, off);
  if (magic != kPerformanceMagic || version != kPerformanceFormatVersion ||
      count > kMaxPerformances) {
    return false;
  }
  const std::size_t needed = sizeof(PerformanceStoreHeader) +
                             static_cast<std::size_t>(count) * kPerformanceRecordWireSize +
                             sizeof(std::uint32_t);
  if (in.size() < needed) {
    return false;
  }
  const std::size_t crc_off = needed - sizeof(std::uint32_t);
  const std::uint32_t expected_crc = crc32(in.first(crc_off));
  std::size_t crc_read_off = crc_off;
  const std::uint32_t actual_crc = perf_wire::get_u32(in, crc_read_off);
  if (actual_crc != expected_crc) {
    return false;
  }
  PerformanceStore loaded;  // build off to the side; only commit on full success
  for (std::uint16_t i = 0; i < count; ++i) {
    Performance p;
    perf_wire::get_record(in, off, p);
    if (!loaded.store(i, p)) {
      return false;  // unreachable given count <= kMaxPerformances; defensive
    }
  }
  store = loaded;
  return true;
}

}  // namespace arrangrr

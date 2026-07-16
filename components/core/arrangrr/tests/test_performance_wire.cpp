// Unit tests for the Performance on-disk wire format (Phase-5 Item #9,
// docs/phase5-design-reviews.md "Pad/Scene live -> Performance"; format
// version 2, Phase-6 Theme 3 Item #3, docs/reflections/phase6-theme3-
// performance-format-v2-review.md): the explicit field-by-field little-endian
// serialize()/deserialize() pair, and PerformanceStore's own pool bookkeeping.
// Pure header-only code -- no Engine needed, mirroring test_common.cpp's own
// treatment of common/ primitives.

#include "arrangrr/perf/performance.hpp"

#include <cstring>
#include <vector>

#include "arrangrr/fx/insert_chain.hpp"  // kInsertTypeCount (Phase-6 Theme 4: now 6, was 4)
#include "test.hpp"

namespace {

using namespace arrangrr;

bool same_groove(const GrooveParams& a, const GrooveParams& b) {
  return a.swing == b.swing && a.humanize_timing == b.humanize_timing &&
         a.humanize_velocity == b.humanize_velocity && a.accent == b.accent &&
         a.swing_grid == b.swing_grid && a.quantize == b.quantize && a.seed == b.seed;
}

bool same_insert(const PerfInsert& a, const PerfInsert& b) {
  if (a.type != b.type || a.enabled != b.enabled) {
    return false;
  }
  for (int i = 0; i < 4; ++i) {
    if (a.params[i] != b.params[i]) {
      return false;
    }
  }
  return true;
}

bool same_routes(const Performance& a, const Performance& b) {
  for (int i = 0; i < 10; ++i) {
    if (a.routes[i].port != b.routes[i].port || a.routes[i].channel != b.routes[i].channel ||
        a.routes[i].enabled != b.routes[i].enabled) {
      return false;
    }
  }
  return true;
}

bool same_insert_chains(const Performance& a, const Performance& b) {
  for (int r = 0; r < 10; ++r) {
    for (int s = 0; s < 8; ++s) {
      if (!same_insert(a.insert_chains[r][s], b.insert_chains[r][s])) {
        return false;
      }
    }
  }
  return true;
}

// Split into small, single-purpose helpers above (Phase-6 Theme 3 Item #3's
// routes/insert_chains checks pushed this over the cognitive-complexity gate
// as one flat function) -- same discipline as perf::validate() itself.
bool same_performance(const Performance& a, const Performance& b) {
  if (std::memcmp(a.name, b.name, sizeof(a.name)) != 0) {
    return false;
  }
  if (a.track_mute_mask != b.track_mute_mask || a.track_solo_mask != b.track_solo_mask) {
    return false;
  }
  if (!same_groove(a.groove, b.groove)) {
    return false;
  }
  if (a.style_id != b.style_id || a.tempo_x100 != b.tempo_x100) {
    return false;
  }
  if (a.beats_per_bar != b.beats_per_bar) {  // Phase 7 (node T0)
    return false;
  }
  if (a.master_transpose != b.master_transpose || a.pad_bank_id != b.pad_bank_id) {
    return false;
  }
  if (a.chord_sequence_id != b.chord_sequence_id || a.controller_map_id != b.controller_map_id) {
    return false;
  }
  if (a.routing_profile_id != b.routing_profile_id) {
    return false;
  }
  if (!same_routes(a, b) || !same_insert_chains(a, b)) {
    return false;
  }
  if (a.variation != b.variation || a.chord_mode != b.chord_mode ||
      a.chord_follow != b.chord_follow) {
    return false;
  }
  if (a.key_root != b.key_root || a.key_mode != b.key_mode) {
    return false;
  }
  for (int i = 0; i < 2; ++i) {  // Phase 7 (node T0): reserved shrunk 3 -> 2
    if (a.reserved[i] != b.reserved[i]) {
      return false;
    }
  }
  return true;
}

Performance distinctive_performance(std::uint8_t salt) {
  Performance p;
  const char* nm = "SlotName";
  std::memcpy(p.name, nm, std::strlen(nm));
  p.track_mute_mask = 0x000002AAu + salt;
  p.track_solo_mask = 0x00000155u + salt;
  p.groove = GrooveParams{.swing = static_cast<std::uint8_t>(10 + salt),
                          .humanize_timing = 20,
                          .humanize_velocity = 30,
                          .accent = 40,
                          .swing_grid = 16,
                          .quantize = 50,
                          .seed = 0x11223344u + salt};
  p.style_id = static_cast<std::uint16_t>(3 + salt);
  p.tempo_x100 = static_cast<std::uint16_t>(9000 + salt);
  // Phase 7 (node T0): exercise a genuine non-4/4 value, bounded to
  // [kMinBeatsPerBar, kMaxBeatsPerBar].
  p.beats_per_bar =
      static_cast<std::uint8_t>(kMinBeatsPerBar + (salt % (kMaxBeatsPerBar - kMinBeatsPerBar + 1)));
  // Alternates sign so every distinctive_performance() exercises the full
  // two's-complement wire pattern, not just non-negative values.
  p.master_transpose = static_cast<std::int16_t>((salt % 2 == 0) ? (salt % 13) : -(salt % 13) - 1);
  p.pad_bank_id = salt;
  p.chord_sequence_id = salt == 0 ? std::uint16_t{0xFFFF} : static_cast<std::uint16_t>(salt);
  p.controller_map_id = 0xFFFF;
  p.routing_profile_id = 0xFFFF;  // the only value perf::validate() accepts today (P2)
  for (int i = 0; i < 10; ++i) {
    p.routes[i] = PerfRoute{.port = static_cast<std::uint8_t>(i % 4),
                            .channel = static_cast<std::uint8_t>((i + salt) % 16),
                            .enabled = static_cast<std::uint8_t>(i % 2)};
  }
  // Distinctive, non-default FX chains on every role/slot -- every byte
  // depends on role, slot, AND salt so no two records collide.
  for (int r = 0; r < 10; ++r) {
    for (int s = 0; s < 8; ++s) {
      PerfInsert& ins = p.insert_chains[r][s];
      ins.type = static_cast<std::uint8_t>((r + s + salt) % kInsertTypeCount);
      ins.enabled = static_cast<std::uint8_t>((r + s + salt) % 2);
      ins.params[0] = static_cast<std::uint8_t>(0x10 + r);
      ins.params[1] = static_cast<std::uint8_t>(0x20 + s);
      ins.params[2] = static_cast<std::uint8_t>(0x30 + salt);
      ins.params[3] = static_cast<std::uint8_t>(0x40 + ((r * 8 + s + salt) % 256));
    }
  }
  p.variation = static_cast<std::uint8_t>(2 + (salt % 3));
  p.chord_mode = static_cast<std::uint8_t>(salt % 3);
  p.chord_follow = static_cast<std::uint8_t>(salt % 5);
  p.key_root = static_cast<std::uint8_t>(salt % 12);
  p.key_mode = static_cast<std::uint8_t>(salt % 7);
  for (int i = 0; i < 2; ++i) {  // Phase 7 (node T0): reserved shrunk 3 -> 2
    p.reserved[i] = static_cast<std::uint8_t>(0xA0 + i + salt);
  }
  return p;
}

void test_single_record_round_trip() {
  PerformanceStore store;
  const Performance original = distinctive_performance(3);
  CHECK(store.store(0, original));

  std::vector<std::uint8_t> buf(4096, 0xCC);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n == sizeof(PerformanceStoreHeader) + kPerformanceRecordWireSize + sizeof(std::uint32_t));
  CHECK(buf[0] == 'S' && buf[1] == 'N' && buf[2] == 'P' && buf[3] == 'F');  // wire mnemonic

  PerformanceStore loaded;
  CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
  CHECK(loaded.size() == 1);
  CHECK(same_performance(*loaded.get(0), original));
}

void test_multi_record_round_trip() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  CHECK(store.store(1, distinctive_performance(2)));
  CHECK(store.store(2, distinctive_performance(3)));

  std::vector<std::uint8_t> buf(8192);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n ==
        sizeof(PerformanceStoreHeader) + 3 * kPerformanceRecordWireSize + sizeof(std::uint32_t));

  PerformanceStore loaded;
  CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
  CHECK(loaded.size() == 3);
  CHECK(same_performance(*loaded.get(0), distinctive_performance(1)));
  CHECK(same_performance(*loaded.get(1), distinctive_performance(2)));
  CHECK(same_performance(*loaded.get(2), distinctive_performance(3)));
}

// format_version 2 (Phase-6 Theme 3 Item #3): the record grew from 94 to 574
// tight bytes (480 of it the FX-chain snapshot, 2 the reserved
// routing_profile_id field) -- pinned here so a future accidental layout
// drift is caught immediately, exactly like v1's own 94-byte pin was.
// format_version 3 (Phase 7, node T0) added beats_per_bar funded by shrinking
// `reserved` by the same 1 B, so the total stays 574 -- still pinned here.
void test_wire_record_is_574_bytes_tight_no_struct_padding() {
  CHECK(kPerformanceRecordWireSize == 574);
  CHECK(kPerformanceRecordWireSize <
        sizeof(Performance));  // strictly tighter than in-memory layout
}

// Endianness/offset check: style_id and tempo_x100 (two adjacent u16 fields,
// right after GrooveParams on the wire) land at their documented byte
// offsets, little-endian, with NO gap between GrooveParams' 6 u8 fields + u32
// seed and style_id -- proving the wire drops GrooveParams' 2 bytes of
// in-memory inter-field padding (the struct comment in performance.hpp).
void test_explicit_little_endian_encoding_no_padding_gap() {
  PerformanceStore store;
  Performance p;
  p.groove = GrooveParams{.swing = 0,
                          .humanize_timing = 0,
                          .humanize_velocity = 0,
                          .accent = 0,
                          .swing_grid = 8,
                          .quantize = 0,
                          .seed = 0};
  p.style_id = 0x1234;
  p.tempo_x100 = 0xABCD;
  CHECK(store.store(0, p));

  std::vector<std::uint8_t> buf(1024, 0);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);

  // The record starts right after the 8-byte header. name[24] + mute(4) +
  // solo(4) + groove(6 u8 + u32 seed = 10 bytes) = 42 bytes in before style_id.
  const std::size_t rec = sizeof(PerformanceStoreHeader);
  const std::size_t style_id_off = rec + 24 + 4 + 4 + 10;
  CHECK(buf[style_id_off] == 0x34);      // low byte first (little-endian)
  CHECK(buf[style_id_off + 1] == 0x12);  // high byte second
  const std::size_t tempo_off = style_id_off + 2;
  CHECK(buf[tempo_off] == 0xCD);
  CHECK(buf[tempo_off + 1] == 0xAB);
}

// Phase-6 Theme 3 Item #1/#3: master_transpose is a real std::int16_t as of
// format_version 2 (P3) -- the FULL 16 bits are the two's-complement value
// now, not a low-byte-reinterpret-with-reserved-high-byte hack. Exercises
// the mandate's own adversarial-hardening value set (-1, -12, +12, +7, plus
// 0) at the wire's raw byte level, proving (a) both bytes carry the correct
// LE two's-complement pattern and (b) the neighboring fields (tempo_x100
// right before, pad_bank_id right after, on the wire) are never corrupted.
void test_master_transpose_wire_byte_pattern_signed_round_trip() {
  struct Case {
    std::int16_t semitones;
    std::uint8_t low_byte;
    std::uint8_t high_byte;
  };
  const Case cases[] = {
      {.semitones = 0, .low_byte = 0x00, .high_byte = 0x00},
      {.semitones = -1, .low_byte = 0xFF, .high_byte = 0xFF},
      {.semitones = -12, .low_byte = 0xF4, .high_byte = 0xFF},
      {.semitones = 12, .low_byte = 0x0C, .high_byte = 0x00},
      {.semitones = 7, .low_byte = 0x07, .high_byte = 0x00},
  };
  for (const Case& c : cases) {
    PerformanceStore store;
    Performance p;
    p.tempo_x100 = 0xBEEF;  // sentinel: proves the field right before is untouched
    p.master_transpose = c.semitones;
    p.pad_bank_id = 0xCAFE;  // sentinel: proves the field right after is untouched
    CHECK(store.store(0, p));

    std::vector<std::uint8_t> buf(1024, 0xAA);
    const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
    CHECK(n > 0);

    // Same field-offset arithmetic as test_explicit_little_endian_encoding_
    // no_padding_gap above, extended two u16 fields further (style_id,
    // tempo_x100) plus beats_per_bar's new 1 B (Phase 7, node T0) to reach
    // master_transpose.
    const std::size_t rec = sizeof(PerformanceStoreHeader);
    const std::size_t transpose_off = rec + 24 + 4 + 4 + 10 + 2 + 2 + 1;
    CHECK(buf[transpose_off] == c.low_byte);
    CHECK(buf[transpose_off + 1] == c.high_byte);

    PerformanceStore loaded;
    CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
    const Performance* got = loaded.get(0);
    CHECK(got != nullptr);
    CHECK(got->master_transpose == c.semitones);
    CHECK(got->tempo_x100 == p.tempo_x100);    // neighboring field untouched
    CHECK(got->pad_bank_id == p.pad_bank_id);  // neighboring field untouched
  }
}

// The wire itself does NOT clamp master_transpose to perf::validate()'s
// [-12, +12] product-level bound (that is validate()'s own, separate job --
// see test_performance_validate.cpp) -- it round-trips the FULL std::int16_t
// range losslessly, including values a corrupt/adversarial on-disk record
// might carry. Documented here as CURRENT, INTENTIONAL behavior, mirroring
// how the wire never clamps track_mute_mask/style_id/etc. either.
void test_master_transpose_wire_round_trips_full_int16_range_unclamped() {
  const std::int16_t values[] = {-32768, -13, 13, 32767};
  for (std::int16_t v : values) {
    PerformanceStore store;
    Performance p;
    p.master_transpose = v;
    CHECK(store.store(0, p));

    std::vector<std::uint8_t> buf(1024, 0);
    const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
    CHECK(n > 0);
    PerformanceStore loaded;
    CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
    CHECK(loaded.get(0)->master_transpose == v);
  }
}

// Phase-6 Theme 3 Item #3 (P1): the FX-chain snapshot's own byte-exactness --
// a non-default Insert on a specific (role, slot) lands at the EXACT computed
// wire offset (insert_chains starts right after routes[10], 6 bytes/slot),
// little-endian trivially satisfied since every PerfInsert field is a single
// byte or a raw byte array.
void test_insert_chain_wire_round_trip_is_byte_exact() {
  PerformanceStore store;
  Performance p;
  // role index 2, slot 3: a distinctive, fully non-default Insert.
  PerfInsert& target = p.insert_chains[2][3];
  target.type = 2;     // InsertType::kEcho
  target.enabled = 0;  // non-default (Insert{}'s own default is enabled == true)
  target.params[0] = 0x11;
  target.params[1] = 0x22;
  target.params[2] = 0x33;
  target.params[3] = 0x44;
  CHECK(store.store(0, p));

  std::vector<std::uint8_t> buf(4096, 0);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);

  // insert_chains starts right after routes[10] on the wire: name(24) +
  // masks(8) + groove(10) + beats_per_bar(1, Phase 7 node T0) + 7 u16 fields
  // (style/tempo/transpose/pad_bank/chord_seq/controller_map/
  // routing_profile, 14 B) + routes(30) = 87 bytes into the record, plus the
  // 8-byte header in front of the record itself.
  const std::size_t insert_chains_off = sizeof(PerformanceStoreHeader) + 87;
  const std::size_t target_off = insert_chains_off + (2 * 8 + 3) * 6;  // role 2, slot 3
  CHECK(buf[target_off] == 2);                                         // type
  CHECK(buf[target_off + 1] == 0);                                     // enabled
  CHECK(buf[target_off + 2] == 0x11);                                  // params[0]
  CHECK(buf[target_off + 3] == 0x22);                                  // params[1]
  CHECK(buf[target_off + 4] == 0x33);                                  // params[2]
  CHECK(buf[target_off + 5] == 0x44);                                  // params[3]

  PerformanceStore loaded;
  CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
  const Performance* got = loaded.get(0);
  CHECK(got != nullptr);
  const PerfInsert& got_target = got->insert_chains[2][3];
  CHECK(got_target.type == 2 && got_target.enabled == 0);
  CHECK(got_target.params[0] == 0x11 && got_target.params[1] == 0x22 &&
        got_target.params[2] == 0x33 && got_target.params[3] == 0x44);
  // Every OTHER slot stays the untouched Insert{} default (type 0, enabled 1,
  // params all-zero) -- proves this test isolates ONE slot, not a blanket fill.
  CHECK(got->insert_chains[0][0].type == 0 && got->insert_chains[0][0].enabled == 1);
  CHECK(got->insert_chains[0][0].params[0] == 0 && got->insert_chains[0][0].params[3] == 0);
}

// Torquato QA hardening (Phase-6 Theme 3 Item #3 independent verification):
// the CRC-32 trailer covers the FULL record, including the NEW 480-byte
// insert_chains region -- flipping a single byte strictly INSIDE that region
// (not the CRC field itself, not any pre-existing v1 field) must be caught by
// the same CRC check every other field's corruption already is. Proves the
// mandate's "CRC32 covers the new fields" property directly, rather than
// inferring it from the fact that the whole-buffer CRC already covers
// everything.
void test_deserialize_rejects_fx_region_byte_flip_via_crc() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(4)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);

  // insert_chains starts at header(8) + 87 bytes into the record (same offset
  // arithmetic as test_insert_chain_wire_round_trip_is_byte_exact above);
  // flip a byte squarely inside it -- role 5, slot 4's `type` byte.
  const std::size_t insert_chains_off = sizeof(PerformanceStoreHeader) + 87;
  const std::size_t victim_off = insert_chains_off + (5 * 8 + 4) * 6;
  CHECK(victim_off < n - sizeof(std::uint32_t));  // still inside the record, before the CRC
  buf[victim_off] ^= 0xFF;

  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));  // pre-populated: must survive untouched
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

void test_deserialize_rejects_wrong_magic() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  buf[0] ^= 0xFF;  // corrupt the magic
  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));  // pre-populated: must survive untouched
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

// format_version 3 (Phase 7, node T0): the core's own deserialize()
// hard-rejects ANY version != 3 -- a wrong/future version.
void test_deserialize_rejects_wrong_format_version() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  buf[4] = 4;  // format_version low byte (right after the 4-byte magic): neither 1 nor 3
  buf[5] = 0;
  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

// Phase-6 Theme 3 Item #3 (P4, docs/DESIGN.md line 588): the OLD v1 version
// number is likewise hard-rejected by the core -- the device NEVER migrates,
// only components/hostrt's migrate_performance_v1_to_v2 (host-only) does,
// tested separately in components/hostrt/tests/test_perf_migrate.cpp.
void test_deserialize_rejects_v1_format_version() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  buf[4] = 1;  // format_version low byte: the OLD v1 value
  buf[5] = 0;
  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

void test_deserialize_rejects_corrupted_crc() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  buf[n - 1] ^= 0xFF;  // flip a byte inside the trailing CRC-32
  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

void test_deserialize_rejects_truncated_buffer() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), n - 1), sentinel));  // one byte short
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));

  // Below the absolute minimum (header + trailing CRC).
  const std::uint8_t tiny[4] = {'S', 'N', 'P', 'F'};
  CHECK(!deserialize(Span<const std::uint8_t>(tiny, sizeof(tiny)), sentinel));
  CHECK(sentinel.size() == 1);
}

// Torquato QA finding, RED BY DESIGN (docs/reflections/phase6-theme3-
// performance-format-v2-review.md's own mandate: "a truncated/oversized v2
// buffer fails cleanly"): deserialize()'s size check is `in.size() < needed`
// -- strictly LESS-THAN -- so a buffer with EXTRA trailing bytes past a
// perfectly valid, correctly-CRC'd record is silently ACCEPTED, and the
// trailing bytes are silently ignored (never even inspected). This is
// PRE-EXISTING behavior (the exact same `<` shape existed in v1's own
// deserialize() before this bump -- format_version 2 changed field widths and
// constants, not this comparison operator), so it is not a NEW regression
// introduced by the v2 work, but it does violate the mandate's own stated
// invariant and is a real, reachable correctness gap: a `.snpf` file
// corrupted by an appended/partial second record (e.g. an interrupted
// resave, or a concatenation of two files) loads successfully as if nothing
// were wrong, silently discarding whatever data follows the first valid
// record instead of failing loudly. FIXED: deserialize() now requires an
// exact-length buffer (`in.size() != needed`), so this rejects cleanly.
void test_deserialize_rejects_oversized_buffer_with_trailing_garbage() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096, 0);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);
  // A buffer that is a byte-for-byte valid v2 record (correct magic, version,
  // count, and CRC over exactly the first `n` bytes) with two EXTRA garbage
  // bytes appended past the end -- neither truncated nor tampered inside the
  // record itself.
  std::vector<std::uint8_t> oversized(buf.begin(), buf.begin() + static_cast<long>(n));
  oversized.push_back(0xAB);
  oversized.push_back(0xCD);

  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));  // pre-populated: must survive untouched
  CHECK(!deserialize(Span<const std::uint8_t>(oversized.data(), oversized.size()), sentinel));
  CHECK(sentinel.size() == 1);
  CHECK(same_performance(*sentinel.get(0), distinctive_performance(9)));
}

void test_deserialize_rejects_count_beyond_pool_bound() {
  // Hand-craft a header claiming more records than kMaxPerformances, with a
  // matching CRC over just the (empty) header -- deserialize must reject on
  // the count bound before even looking for record bytes.
  std::vector<std::uint8_t> buf(sizeof(PerformanceStoreHeader) + sizeof(std::uint32_t), 0);
  std::size_t off = 0;
  perf_wire::put_u32(Span<std::uint8_t>(buf.data(), buf.size()), off, kPerformanceMagic);
  perf_wire::put_u16(Span<std::uint8_t>(buf.data(), buf.size()), off, kPerformanceFormatVersion);
  perf_wire::put_u16(Span<std::uint8_t>(buf.data(), buf.size()), off,
                     static_cast<std::uint16_t>(kMaxPerformances + 1));
  const std::uint32_t crc = crc32(Span<const std::uint8_t>(buf.data(), off));
  perf_wire::put_u32(Span<std::uint8_t>(buf.data(), buf.size()), off, crc);

  PerformanceStore sentinel;
  CHECK(sentinel.store(0, distinctive_performance(9)));
  CHECK(!deserialize(Span<const std::uint8_t>(buf.data(), buf.size()), sentinel));
  CHECK(sentinel.size() == 1);
}

void test_serialize_into_undersized_buffer_returns_zero() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::uint8_t tiny[4];
  CHECK(serialize(store, Span<std::uint8_t>(tiny, sizeof(tiny))) == 0);
  // Even a buffer one byte short of the exact need fails cleanly.
  std::vector<std::uint8_t> almost(sizeof(PerformanceStoreHeader) + kPerformanceRecordWireSize +
                                   sizeof(std::uint32_t) - 1);
  CHECK(serialize(store, Span<std::uint8_t>(almost.data(), almost.size())) == 0);
}

void test_serialize_empty_store_round_trips() {
  PerformanceStore store;  // no slots
  std::vector<std::uint8_t> buf(64);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n == sizeof(PerformanceStoreHeader) + sizeof(std::uint32_t));
  PerformanceStore loaded;
  CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
  CHECK(loaded.size() == 0);
}

// PerformanceStore pool bookkeeping (pure, unit-level -- no Engine).
void test_performance_store_slot_semantics() {
  PerformanceStore store;
  CHECK(store.size() == 0);
  CHECK(store.get(0) == nullptr);
  CHECK(store.store(0, distinctive_performance(1)));  // grows by exactly one
  CHECK(store.size() == 1);
  CHECK(!store.store(2, distinctive_performance(2)));  // gap-leaving: rejected
  CHECK(store.size() == 1);
  CHECK(store.store(1, distinctive_performance(2)));  // exact next slot: grows
  CHECK(store.size() == 2);
  CHECK(store.store(0, distinctive_performance(3)));  // overwrite in place
  CHECK(store.size() == 2);
  CHECK(same_performance(*store.get(0), distinctive_performance(3)));
  CHECK(store.get(5) == nullptr);
}

}  // namespace

int main() {
  test_single_record_round_trip();
  test_multi_record_round_trip();
  test_wire_record_is_574_bytes_tight_no_struct_padding();
  test_explicit_little_endian_encoding_no_padding_gap();
  test_master_transpose_wire_byte_pattern_signed_round_trip();
  test_master_transpose_wire_round_trips_full_int16_range_unclamped();
  test_insert_chain_wire_round_trip_is_byte_exact();
  test_deserialize_rejects_fx_region_byte_flip_via_crc();
  test_deserialize_rejects_wrong_magic();
  test_deserialize_rejects_wrong_format_version();
  test_deserialize_rejects_v1_format_version();
  test_deserialize_rejects_corrupted_crc();
  test_deserialize_rejects_truncated_buffer();
  test_deserialize_rejects_oversized_buffer_with_trailing_garbage();  // regression: exact-length
                                                                      // required above
  test_deserialize_rejects_count_beyond_pool_bound();
  test_serialize_into_undersized_buffer_returns_zero();
  test_serialize_empty_store_round_trips();
  test_performance_store_slot_semantics();
  return arrangrr::test::failures();
}

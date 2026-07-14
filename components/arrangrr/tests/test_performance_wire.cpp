// Unit tests for the Performance on-disk wire format (Phase-5 Item #9,
// docs/phase5-design-reviews.md "Pad/Scene live -> Performance"): the
// explicit field-by-field little-endian serialize()/deserialize() pair, and
// PerformanceStore's own pool bookkeeping. Pure header-only code -- no Engine
// needed, mirroring test_common.cpp's own treatment of common/ primitives.

#include "arrangrr/perf/performance.hpp"

#include <cstring>
#include <vector>

#include "test.hpp"

namespace {

using namespace arrangrr;

bool same_groove(const GrooveParams& a, const GrooveParams& b) {
  return a.swing == b.swing && a.humanize_timing == b.humanize_timing &&
         a.humanize_velocity == b.humanize_velocity && a.accent == b.accent &&
         a.swing_grid == b.swing_grid && a.quantize == b.quantize && a.seed == b.seed;
}

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
  if (a.master_transpose != b.master_transpose || a.pad_bank_id != b.pad_bank_id) {
    return false;
  }
  if (a.chord_sequence_id != b.chord_sequence_id || a.controller_map_id != b.controller_map_id) {
    return false;
  }
  for (int i = 0; i < 10; ++i) {
    if (a.routes[i].port != b.routes[i].port || a.routes[i].channel != b.routes[i].channel ||
        a.routes[i].enabled != b.routes[i].enabled) {
      return false;
    }
  }
  if (a.variation != b.variation || a.chord_mode != b.chord_mode ||
      a.chord_follow != b.chord_follow) {
    return false;
  }
  if (a.key_root != b.key_root || a.key_mode != b.key_mode) {
    return false;
  }
  for (int i = 0; i < 5; ++i) {
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
  p.master_transpose = 0;
  p.pad_bank_id = salt;
  p.chord_sequence_id = salt == 0 ? std::uint16_t{0xFFFF} : static_cast<std::uint16_t>(salt);
  p.controller_map_id = 0xFFFF;
  for (int i = 0; i < 10; ++i) {
    p.routes[i] = PerfRoute{.port = static_cast<std::uint8_t>(i % 4),
                            .channel = static_cast<std::uint8_t>((i + salt) % 16),
                            .enabled = static_cast<std::uint8_t>(i % 2)};
  }
  p.variation = static_cast<std::uint8_t>(2 + (salt % 3));
  p.chord_mode = static_cast<std::uint8_t>(salt % 3);
  p.chord_follow = static_cast<std::uint8_t>(salt % 5);
  p.key_root = static_cast<std::uint8_t>(salt % 12);
  p.key_mode = static_cast<std::uint8_t>(salt % 7);
  for (int i = 0; i < 5; ++i) {
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

void test_wire_record_is_94_bytes_tight_no_struct_padding() {
  CHECK(kPerformanceRecordWireSize == 94);
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

  std::vector<std::uint8_t> buf(256, 0);
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

// Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
// scope.md Decision 4a): the low byte of master_transpose reinterprets as a
// signed int8_t semitone offset. This exercises the RAW wire bytes across
// the mandate's own adversarial-hardening list (-1, -12, +12, +7, plus 0):
// (a) the low byte carries the correct two's-complement pattern, (b) the
// high byte stays 0 -- RESERVED, per performance.hpp's own field comment --
// for every value a live capture can ever produce, and (c) the neighboring
// fields (tempo_x100 right before, pad_bank_id right after, on the wire) are
// never corrupted by the reinterpret.
void test_master_transpose_wire_byte_pattern_signed_round_trip() {
  struct Case {
    std::int8_t semitones;
    std::uint8_t low_byte;
  };
  const Case cases[] = {
      {.semitones = 0, .low_byte = 0x00},   {.semitones = -1, .low_byte = 0xFF},
      {.semitones = -12, .low_byte = 0xF4}, {.semitones = 12, .low_byte = 0x0C},
      {.semitones = 7, .low_byte = 0x07},
  };
  for (const Case& c : cases) {
    PerformanceStore store;
    Performance p;
    p.tempo_x100 = 0xBEEF;  // sentinel: proves the field right before is untouched
    p.master_transpose = static_cast<std::uint16_t>(static_cast<std::uint8_t>(c.semitones));
    p.pad_bank_id = 0xCAFE;  // sentinel: proves the field right after is untouched
    CHECK(store.store(0, p));

    std::vector<std::uint8_t> buf(256, 0xAA);
    const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
    CHECK(n > 0);

    // Same field-offset arithmetic as test_explicit_little_endian_encoding_
    // no_padding_gap above, extended two u16 fields further (style_id,
    // tempo_x100) to reach master_transpose.
    const std::size_t rec = sizeof(PerformanceStoreHeader);
    const std::size_t transpose_off = rec + 24 + 4 + 4 + 10 + 2 + 2;
    CHECK(buf[transpose_off] == c.low_byte);
    CHECK(buf[transpose_off + 1] == 0x00);  // high byte: reserved, always 0 on a live capture

    PerformanceStore loaded;
    CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
    const Performance* got = loaded.get(0);
    CHECK(got != nullptr);
    const auto round_tripped = static_cast<std::int8_t>(got->master_transpose & 0xFFu);
    CHECK(round_tripped == c.semitones);
    CHECK(got->tempo_x100 == p.tempo_x100);    // neighboring field untouched
    CHECK(got->pad_bank_id == p.pad_bank_id);  // neighboring field untouched
  }
}

// The reserved high byte is NOT enforced to be zero by validate() -- the same
// convention as Performance::reserved[5] (never checked either): a nonzero
// high byte round-trips losslessly on the wire (a plain 16-bit copy) and is
// silently ignored at every point-of-use (perf::validate/apply_performance/
// capture_performance all mask to the low byte). Documented here as CURRENT,
// INTENTIONAL behavior (forward-compatible reserved bits), not a defect --
// a future format_version bump that gives the high byte real meaning would
// need to revisit this test, not silently break it.
void test_master_transpose_high_byte_is_not_enforced_reserved_zero() {
  PerformanceStore store;
  Performance p;
  // low byte = 5 (a valid, in-range transpose); high byte = 0x01 (garbage in
  // the still-reserved half of the field).
  p.master_transpose = 0x0105;
  CHECK(store.store(0, p));

  std::vector<std::uint8_t> buf(256, 0);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);
  PerformanceStore loaded;
  CHECK(deserialize(Span<const std::uint8_t>(buf.data(), n), loaded));
  CHECK(loaded.get(0)->master_transpose == 0x0105);  // the full 16 bits survive, garbage and all
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

void test_deserialize_rejects_wrong_format_version() {
  PerformanceStore store;
  CHECK(store.store(0, distinctive_performance(1)));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(store, Span<std::uint8_t>(buf.data(), buf.size()));
  buf[4] = 2;  // format_version low byte (right after the 4-byte magic)
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
  test_wire_record_is_94_bytes_tight_no_struct_padding();
  test_explicit_little_endian_encoding_no_padding_gap();
  test_master_transpose_wire_byte_pattern_signed_round_trip();
  test_master_transpose_high_byte_is_not_enforced_reserved_zero();
  test_deserialize_rejects_wrong_magic();
  test_deserialize_rejects_wrong_format_version();
  test_deserialize_rejects_corrupted_crc();
  test_deserialize_rejects_truncated_buffer();
  test_deserialize_rejects_count_beyond_pool_bound();
  test_serialize_into_undersized_buffer_returns_zero();
  test_serialize_empty_store_round_trips();
  test_performance_store_slot_semantics();
  return arrangrr::test::failures();
}

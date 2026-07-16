// Unit + functional tests for the Performance v1 -> v2 migrator (Phase-6
// Theme 3 Item #3, P4, docs/reflections/phase6-theme3-performance-format-v2-
// review.md): components/platform/hostrt::migrate_performance_v1_to_v2 and its wiring
// into Shell::perf_load's fallback path. Pure-unit coverage (no Shell) mirrors
// test_performance_wire.cpp's own style; the ONE functional test proves the
// end-to-end `perf load` fallback actually fires.

#include "perf_v1_migrate.hpp"

#include <string>
#include <vector>

#include "arrangrr/common/crc.hpp"
#include "midisrc/file_io.hpp"
#include "shell.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

// Hand-crafts ONE v1-format record (the OLD 94-byte field order: no
// insert_chains, no routing_profile_id, a 5-byte reserved tail, and
// master_transpose in v1's own low-byte-reinterpret encoding) directly into
// `out` at `off` -- deliberately NOT reusing arrangrr::perf_wire::put_record()
// (that writes the NEW v2 layout), mirroring perf_v1_migrate.cpp's own
// get_record_v1() in reverse.
void put_record_v1(Span<std::uint8_t> out, std::size_t& off, const char (&name)[24],
                   std::uint32_t mute_mask, std::uint32_t solo_mask, const GrooveParams& groove,
                   std::uint16_t style_id, std::uint16_t tempo_x100, std::int8_t transpose,
                   std::uint16_t pad_bank_id, std::uint16_t chord_sequence_id,
                   std::uint16_t controller_map_id, const PerfRoute (&routes)[10],
                   std::uint8_t variation, std::uint8_t chord_mode, std::uint8_t chord_follow,
                   std::uint8_t key_root, std::uint8_t key_mode) {
  using namespace perf_wire;
  for (char c : name) {
    put_u8(out, off, static_cast<std::uint8_t>(c));
  }
  put_u32(out, off, mute_mask);
  put_u32(out, off, solo_mask);
  put_u8(out, off, groove.swing);
  put_u8(out, off, groove.humanize_timing);
  put_u8(out, off, groove.humanize_velocity);
  put_u8(out, off, groove.accent);
  put_u8(out, off, groove.swing_grid);
  put_u8(out, off, groove.quantize);
  put_u32(out, off, groove.seed);
  put_u16(out, off, style_id);
  put_u16(out, off, tempo_x100);
  // v1's own encoding: low byte = signed int8_t semitone, high byte reserved/0.
  put_u16(out, off, static_cast<std::uint16_t>(static_cast<std::uint8_t>(transpose)));
  put_u16(out, off, pad_bank_id);
  put_u16(out, off, chord_sequence_id);
  put_u16(out, off, controller_map_id);
  for (const PerfRoute& r : routes) {
    put_u8(out, off, r.port);
    put_u8(out, off, r.channel);
    put_u8(out, off, r.enabled);
  }
  put_u8(out, off, variation);
  put_u8(out, off, chord_mode);
  put_u8(out, off, chord_follow);
  put_u8(out, off, key_root);
  put_u8(out, off, key_mode);
  for (int i = 0; i < 5; ++i) {
    put_u8(out, off, 0);  // v1's own reserved[5]
  }
}

// Builds a complete one-record v1 SNPF buffer (header + one 94-byte record +
// trailing CRC-32).
std::vector<std::uint8_t> build_v1_single_record_buffer(const char (&name)[24],
                                                        std::int8_t transpose,
                                                        std::uint16_t style_id) {
  std::vector<std::uint8_t> buf(
      sizeof(PerformanceStoreHeader) + kPerformanceRecordWireSizeV1 + sizeof(std::uint32_t), 0);
  Span<std::uint8_t> out(buf.data(), buf.size());
  std::size_t off = 0;
  perf_wire::put_u32(out, off, kPerformanceMagic);
  perf_wire::put_u16(out, off, kPerformanceFormatVersionV1);
  perf_wire::put_u16(out, off, 1);  // count
  PerfRoute routes[10];
  for (int i = 0; i < 10; ++i) {
    routes[i] = PerfRoute{.port = static_cast<std::uint8_t>(i % 4),
                          .channel = static_cast<std::uint8_t>(i % 16),
                          .enabled = static_cast<std::uint8_t>(i % 2)};
  }
  const GrooveParams groove{.swing = 11,
                            .humanize_timing = 22,
                            .humanize_velocity = 33,
                            .accent = 44,
                            .swing_grid = 16,
                            .quantize = 55,
                            .seed = 0xCAFEBABEu};
  put_record_v1(out, off, name, 0x2AAu, 0x155u, groove, style_id, /*tempo_x100=*/9500, transpose,
                /*pad_bank_id=*/3, /*chord_sequence_id=*/0xFFFF, /*controller_map_id=*/0xFFFF,
                routes, /*variation=*/2, /*chord_mode=*/1, /*chord_follow=*/0, /*key_root=*/4,
                /*key_mode=*/1);
  const std::uint32_t crc = crc32(Span<const std::uint8_t>(buf.data(), off));
  perf_wire::put_u32(out, off, crc);
  return buf;
}

// ---- pure unit coverage of migrate_performance_v1_to_v2 -------------------

void test_migrate_v1_buffer_preserves_shared_fields_and_defaults_v2_only_fields() {
  char name[24] = "V1Slot";
  const std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, /*transpose=*/-9,
                                                                      /*style_id=*/5);
  PerformanceStore store;
  CHECK(migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size()), store));
  CHECK(store.size() == 1);
  const Performance* p = store.get(0);
  CHECK(p != nullptr);
  CHECK(p->style_id == 5);
  CHECK(p->tempo_x100 == 9500);
  CHECK(p->pad_bank_id == 3);
  CHECK(p->variation == 2);
  CHECK(p->key_root == 4);
  CHECK(p->groove.swing == 11 && p->groove.seed == 0xCAFEBABEu);
  CHECK(p->routes[3].port == 3 % 4 && p->routes[3].channel == 3 && p->routes[3].enabled == 1);
  // v1's low-byte-reinterpret transpose lands, unchanged in VALUE, in v2's
  // real std::int16_t field.
  CHECK(p->master_transpose == -9);
  // v2-only fields: fully defaulted.
  CHECK(p->routing_profile_id == 0xFFFF);
  for (int r = 0; r < 10; ++r) {
    for (int s = 0; s < 8; ++s) {
      const PerfInsert& ins = p->insert_chains[r][s];
      CHECK(ins.type == 0 && ins.enabled == 1);
      CHECK(ins.params[0] == 0 && ins.params[1] == 0 && ins.params[2] == 0 && ins.params[3] == 0);
    }
  }
}

// The mandate's own adversarial-hardening value set (-1, -12, +12, +7, plus
// 0) for master_transpose, migrated through the v1 low-byte decode path.
void test_migrate_preserves_master_transpose_across_the_full_value_set() {
  const std::int8_t values[] = {0, -1, -12, 12, 7};
  for (std::int8_t v : values) {
    char name[24] = "T";
    const std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, v, 0);
    PerformanceStore store;
    CHECK(migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size()), store));
    CHECK(store.get(0)->master_transpose == v);
  }
}

void test_migrate_rejects_a_v2_buffer() {
  PerformanceStore native_store;
  Performance seed;
  CHECK(native_store.store(0, seed));
  std::vector<std::uint8_t> buf(4096);
  const std::size_t n = serialize(native_store, Span<std::uint8_t>(buf.data(), buf.size()));
  CHECK(n > 0);

  PerformanceStore sentinel;
  CHECK(sentinel.store(0, seed));  // pre-populated: must survive untouched
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), n), sentinel));
  CHECK(sentinel.size() == 1);
}

void test_migrate_rejects_garbage_buffer() {
  const std::uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3};
  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));
  CHECK(
      !migrate_performance_v1_to_v2(Span<const std::uint8_t>(garbage, sizeof(garbage)), sentinel));
  CHECK(sentinel.size() == 1);
}

void test_migrate_rejects_corrupted_crc() {
  char name[24] = "X";
  std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, 0, 0);
  buf[buf.size() - 1] ^= 0xFF;  // flip a byte inside the trailing CRC-32
  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size()), sentinel));
  CHECK(sentinel.size() == 1);
}

// Torquato QA hardening: the migrator's own version gate is as strict as the
// core deserialize()'s -- it accepts ONLY format_version == 1 (never a future
// version, never the current v2 either, already covered by test_migrate_
// rejects_a_v2_buffer above).
void test_migrate_rejects_future_format_version() {
  char name[24] = "X";
  std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, 0, 0);
  Span<std::uint8_t> out(buf.data(), buf.size());
  std::size_t off = 4;              // right after the 4-byte magic
  perf_wire::put_u16(out, off, 3);  // some future version, neither 1 nor 2
  // Recompute the trailing CRC so this is a version-gate rejection, not
  // incidentally also a CRC rejection.
  const std::size_t crc_off = buf.size() - sizeof(std::uint32_t);
  const std::uint32_t crc = crc32(Span<const std::uint8_t>(buf.data(), crc_off));
  std::size_t crc_write_off = crc_off;
  perf_wire::put_u32(out, crc_write_off, crc);

  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size()), sentinel));
  CHECK(sentinel.size() == 1);
}

// Mirrors the core deserialize()'s own count-bound rejection
// (test_deserialize_rejects_count_beyond_pool_bound, test_performance_wire.
// cpp) -- the migrator must reject a header claiming more records than
// kMaxPerformances BEFORE it ever walks record bytes, the same
// atomicity/bounds discipline applied on the v1 side.
void test_migrate_rejects_count_beyond_pool_bound() {
  std::vector<std::uint8_t> buf(sizeof(PerformanceStoreHeader) + sizeof(std::uint32_t), 0);
  Span<std::uint8_t> out(buf.data(), buf.size());
  std::size_t off = 0;
  perf_wire::put_u32(out, off, kPerformanceMagic);
  perf_wire::put_u16(out, off, kPerformanceFormatVersionV1);
  perf_wire::put_u16(out, off, static_cast<std::uint16_t>(kMaxPerformances + 1));
  const std::uint32_t crc = crc32(Span<const std::uint8_t>(buf.data(), off));
  perf_wire::put_u32(out, off, crc);

  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size()), sentinel));
  CHECK(sentinel.size() == 1);
}

// Parity fix with the core arrangrr::perf::deserialize()'s own hardening
// (test_deserialize_rejects_oversized_buffer_with_trailing_garbage,
// test_performance_wire.cpp): a byte-for-byte valid v1 record (correct
// magic, v1 version, count, and CRC over exactly its own bytes) with EXTRA
// garbage bytes appended past the end must be REJECTED, not silently
// accepted with the trailing bytes discarded -- a corrupted/concatenated
// legacy `.snpf` must not silently migrate.
void test_migrate_rejects_oversized_buffer_with_trailing_garbage() {
  char name[24] = "X";
  const std::vector<std::uint8_t> valid = build_v1_single_record_buffer(name, /*transpose=*/5,
                                                                        /*style_id=*/2);
  std::vector<std::uint8_t> oversized = valid;
  oversized.push_back(0xAB);
  oversized.push_back(0xCD);

  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));  // pre-populated: must survive untouched
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(oversized.data(), oversized.size()),
                                      sentinel));
  CHECK(sentinel.size() == 1);
}

void test_migrate_rejects_truncated_buffer() {
  char name[24] = "X";
  const std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, 0, 0);
  PerformanceStore sentinel;
  Performance seed;
  CHECK(sentinel.store(0, seed));
  CHECK(!migrate_performance_v1_to_v2(Span<const std::uint8_t>(buf.data(), buf.size() - 1),
                                      sentinel));  // one byte short
  CHECK(sentinel.size() == 1);
}

// ---- functional: Shell::perf_load falls back to the migrator --------------

// Mirrors test_pad_perf.cpp's own ShellFixture pattern.
struct ShellFixture {
  std::vector<OutEvent> events;
  Shell shell{[this](const OutEvent& ev) { events.push_back(ev); }};
  std::string err;

  bool run(const std::string& line) { return shell.exec_line(line, err); }
};

void test_perf_load_falls_back_to_v1_migrator() {
  char name[24] = "OldSlot";
  const std::vector<std::uint8_t> buf = build_v1_single_record_buffer(name, /*transpose=*/-3,
                                                                      /*style_id=*/1);
  const std::string path = "test_perf_migrate_v1_file.snpf";
  std::string write_err;
  CHECK(midisrc::write_binary_file(path, buf, write_err));

  ShellFixture f;
  CHECK(f.run("perf load " + path));
  CHECK(f.shell.engine().performances().size() == 1);
  const Performance* p = f.shell.engine().performances().get(0);
  CHECK(p != nullptr);
  CHECK(p->style_id == 1);
  CHECK(p->master_transpose == -3);
  CHECK(p->routing_profile_id == 0xFFFF);
  for (int r = 0; r < 10; ++r) {
    CHECK(p->insert_chains[r][0].type == 0 && p->insert_chains[r][0].enabled == 1);
  }
}

// A file that is neither a valid v2 record NOR a valid v1 record still
// produces the ORIGINAL "bad performance file" error, unchanged -- the
// migrator is a fallback, not a second chance for genuinely bad input.
void test_perf_load_still_rejects_a_garbage_file_after_migrator_fallback() {
  const std::string path = "test_perf_migrate_garbage_file.snpf";
  std::string write_err;
  const std::vector<std::uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3};
  CHECK(midisrc::write_binary_file(path, garbage, write_err));

  ShellFixture f;
  CHECK(!f.run("perf load " + path));
  CHECK(!f.err.empty());
  CHECK(f.shell.engine().performances().size() == 0);
}

}  // namespace

int main() {
  test_migrate_v1_buffer_preserves_shared_fields_and_defaults_v2_only_fields();
  test_migrate_preserves_master_transpose_across_the_full_value_set();
  test_migrate_rejects_a_v2_buffer();
  test_migrate_rejects_future_format_version();
  test_migrate_rejects_count_beyond_pool_bound();
  test_migrate_rejects_garbage_buffer();
  test_migrate_rejects_corrupted_crc();
  test_migrate_rejects_oversized_buffer_with_trailing_garbage();
  test_migrate_rejects_truncated_buffer();
  test_perf_load_falls_back_to_v1_migrator();
  test_perf_load_still_rejects_a_garbage_file_after_migrator_fallback();
  return arrangrr::test::failures();
}

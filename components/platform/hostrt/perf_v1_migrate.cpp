#include "perf_v1_migrate.hpp"

#include "arrangrr/common/crc.hpp"

// See perf_v1_migrate.hpp for the design rationale (Phase-6 Theme 3 Item #3,
// P4). This file owns the OLD v1 record layout ONLY -- it must never be
// edited to track format_version 2's own layout (that stays
// arrangrr/perf/performance.hpp's job); a future format_version 3 gets its
// OWN migrator function here, this one stays frozen at "v1 -> v2" forever.

namespace arrangrr::host {

namespace {

// Reads ONE v1 record (the OLD, 94-byte field order -- no insert_chains, no
// routing_profile_id, and a 5-byte `reserved` tail instead of v2's 3) into a
// freshly-defaulted v2 Performance `p`: every v1 field lands in the SAME
// v2 struct member (the two layouts share a field name for everything v1
// ever carried), and every v2-only member is left at `Performance{}`'s own
// default (insert_chains == inert, routing_profile_id == 0xFFFF) -- the
// caller is responsible for default-constructing `p` first.
void get_record_v1(Span<const std::uint8_t> in, std::size_t& off, Performance& p) noexcept {
  using namespace perf_wire;
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
  // v1's own encoding (Phase-6 Theme 3 Item #1, since-formalized by Item #3's
  // P3): the LOW byte is a signed int8_t semitone offset, the high byte was
  // reserved/unused. Decode it the same way v1's own capture_performance/
  // apply_performance did, then widen into v2's real std::int16_t field --
  // numerically identical, just no longer a byte-reinterpret hack.
  const std::uint16_t v1_transpose = get_u16(in, off);
  p.master_transpose = static_cast<std::int16_t>(static_cast<std::int8_t>(v1_transpose & 0xFFu));
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
  for (std::size_t i = 0; i < 5; ++i) {
    get_u8(in, off);  // v1's own reserved[5]: discarded, no v2 slot corresponds to it 1:1
  }
  // insert_chains / routing_profile_id: left at Performance{}'s own v2
  // defaults (the caller default-constructs `p` before calling this).
}

}  // namespace

bool migrate_performance_v1_to_v2(Span<const std::uint8_t> in, PerformanceStore& store) noexcept {
  if (in.size() < sizeof(PerformanceStoreHeader) + sizeof(std::uint32_t)) {
    return false;
  }
  std::size_t off = 0;
  const std::uint32_t magic = perf_wire::get_u32(in, off);
  const std::uint16_t version = perf_wire::get_u16(in, off);
  const std::uint16_t count = perf_wire::get_u16(in, off);
  if (magic != kPerformanceMagic || version != kPerformanceFormatVersionV1 ||
      count > kMaxPerformances) {
    return false;
  }
  const std::size_t needed = sizeof(PerformanceStoreHeader) +
                             static_cast<std::size_t>(count) * kPerformanceRecordWireSizeV1 +
                             sizeof(std::uint32_t);
  // EXACT length required (parity with arrangrr::perf::deserialize()'s own
  // Nazzareno fix, Torquato QA finding): a strict `<` only rejected a
  // too-SHORT buffer, silently accepting extra trailing bytes past a
  // perfectly valid v1 record (an interrupted resave or a file concatenation
  // would load as if nothing were wrong). Shell::perf_load hands this
  // function the same tight buffer it hands deserialize() -- the full
  // contents of the file, read verbatim by midisrc::read_binary_file, no
  // caller relies on slack -- so `!=` never rejects a well-formed legacy v1
  // record, only a truncated OR oversized one.
  if (in.size() != needed) {
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
    Performance p;  // default-constructed: insert_chains inert, routing_profile_id 0xFFFF
    get_record_v1(in, off, p);
    if (!loaded.store(i, p)) {
      return false;  // unreachable given count <= kMaxPerformances; defensive
    }
  }
  store = loaded;
  return true;
}

}  // namespace arrangrr::host

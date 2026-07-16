#pragma once

#include "arrangrr/perf/performance.hpp"

// Phase-6 Theme 3 Item #3 (Performance format_version 2, P4, docs/reflections/
// phase6-theme3-performance-format-v2-review.md): the v1 -> v2 migrator.
// HOST-ONLY (components/hostrt), never linked into the core/arm-none-eabi
// build -- docs/DESIGN.md line 588 ("Versioning with migration tool-side
// only -- the device reads its own version or refuses with a message") means
// the DEVICE's own arrangrr::deserialize() must keep hard-rejecting anything
// != kPerformanceFormatVersion (now 2, including the OLD version 1), exactly
// as it always has. Only the HOST tool-side load path (Shell::perf_load,
// shell_pad_commands.cpp) falls back to this function when the native
// deserialize() fails.

namespace arrangrr::host {

// The v1 record's own FROZEN wire stride (94 B): kept as a named historical
// constant, never reused for anything live -- it existed as
// kPerformanceRecordWireSize before format_version 2 appended
// insert_chains/routing_profile_id and shrank `reserved`.
inline constexpr std::size_t kPerformanceRecordWireSizeV1 = 94;
inline constexpr std::uint16_t kPerformanceFormatVersionV1 = 1;

// Parses `in` as a v1-format SNPF buffer (magic + format_version == 1 +
// count, `count` records of the OLD 94-byte stride, trailing CRC-32 -- the
// SAME magic/CRC discipline arrangrr::deserialize() itself uses, just the
// OLD record layout) and builds a v2 `PerformanceStore` in `store`: every
// field the v1 layout carried is preserved verbatim (master_transpose's
// value included, now living in its real std::int16_t field instead of a
// low-byte reinterpret), and every v2-only field is defaulted
// (insert_chains == inert `Insert{}` equivalents, routing_profile_id ==
// 0xFFFF). Returns false -- `store` left UNTOUCHED -- on a bad magic, a
// version other than 1, a record count beyond kMaxPerformances, a corrupted
// CRC, or a truncated buffer; the SAME validate-before-commit atomicity
// arrangrr::deserialize() itself applies.
bool migrate_performance_v1_to_v2(Span<const std::uint8_t> in, PerformanceStore& store) noexcept;

}  // namespace arrangrr::host

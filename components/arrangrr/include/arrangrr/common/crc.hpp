#pragma once

#include <cstdint>

#include "arrangrr/common/span.hpp"

// CRC-32 (ISO-HDLC / IEEE 802.3), table generated at compile time (D32:
// constexpr data tables). Used by the versioned binary project format (§21).

namespace arrangrr {

namespace detail {
consteval std::uint32_t crc32_entry(std::uint32_t i) {
  std::uint32_t c = i;
  for (int k = 0; k < 8; ++k) {
    c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
  }
  return c;
}

struct Crc32Table {
  std::uint32_t entries[256];
};

consteval Crc32Table make_crc32_table() {
  Crc32Table t{};
  for (std::uint32_t i = 0; i < 256; ++i) {
    t.entries[i] = crc32_entry(i);
  }
  return t;
}

inline constexpr Crc32Table kCrc32Table = make_crc32_table();
}  // namespace detail

constexpr std::uint32_t crc32(Span<const std::uint8_t> data,
                              std::uint32_t seed = 0xFFFFFFFFu) noexcept {
  std::uint32_t c = seed;
  for (std::uint8_t byte : data) {
    c = detail::kCrc32Table.entries[(c ^ byte) & 0xFFu] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

namespace detail {
consteval std::uint32_t crc32_selftest() {
  constexpr std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  return crc32(Span<const std::uint8_t>(check));
}
// Known check value for CRC-32/ISO-HDLC over "123456789".
static_assert(crc32_selftest() == 0xCBF43926u);
}  // namespace detail

}  // namespace arrangrr

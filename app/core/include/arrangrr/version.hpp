#pragma once

#include <cstdint>

namespace arrangrr {

inline constexpr std::uint16_t kVersionMajor = 0;
inline constexpr std::uint16_t kVersionMinor = 1;
inline constexpr std::uint16_t kVersionPatch = 0;

// L0 protocol version (DESIGN.md §29.3).
inline constexpr std::uint16_t kProtocolVersion = 1;

const char* version_string() noexcept;

}  // namespace arrangrr

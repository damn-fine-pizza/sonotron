#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "diagnostics.hpp"

// Yamaha SFF (Style File Format) — STUB. SFF is a reverse-engineered format: a
// .sty file is a Standard MIDI File with appended CASM/OTS/etc. chunks. We ship
// NO proprietary format internals and NO copyrighted style content; we only
// parse user-provided files. The MVP does inspect-only extraction of the
// embedded SMF header/tempo (which is public MIDI), and refuses `import` with
// an honest "not implemented".

namespace arrstyle {

// Inspect-only: prints an honest capability note plus any trivially-extractable
// SMF facts. Always returns 0-worthy success (records info/warnings only).
void inspect_sff(const std::vector<std::uint8_t>& bytes, const std::string& source,
                 std::ostream& out, Diagnostics& diag);

// Import is deliberately unimplemented: records an error so the CLI exits
// non-zero.
bool import_sff(const std::vector<std::uint8_t>& bytes, const std::string& source,
                Diagnostics& diag);

// True if the bytes look like a Yamaha style (embedded SMF + a CASM/Sff marker).
bool looks_like_sff(const std::vector<std::uint8_t>& bytes);

}  // namespace arrstyle

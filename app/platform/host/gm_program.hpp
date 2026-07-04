#pragma once

#include <cstdint>
#include <string>

// General MIDI program (voice) names, host-only display + parsing. The core
// speaks raw program numbers (0..127) over the ABI (kProgram); this gives the
// user friendly names ("trumpet", "fingered-bass") on the CLI. Pure data +
// lookup: no terminal, no engine dependency.

namespace arrangrr::host {

// GM program name for 0..127 (masked to 7 bits). Never null.
const char* gm_program_name(std::uint8_t program);

// Parses a user token into a GM program 0..127, or -1 when unresolved.
// Accepts a decimal number (0..127) OR a name matched case-insensitively,
// ignoring spaces/hyphens/underscores: exact normalized match first, then a
// unique substring match (e.g. "trumpet" -> 56, "fingered" -> 33). Ambiguous
// or unknown names return -1.
int parse_gm_program(const std::string& token);

}  // namespace arrangrr::host

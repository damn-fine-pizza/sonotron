#pragma once

#include <string>

#include "arrangrr/abi.hpp"

// L0 wire encoding, host side only (D26): the core emits POD OutEvents; this
// renders them as canonical JSONL lines. The golden harness diffs this output.

namespace arrangrr::host {

// One event -> one JSON line (no trailing newline).
std::string to_jsonl(const OutEvent& ev);

// Human-readable one-liner for the interactive monitor.
std::string to_human(const OutEvent& ev);

}  // namespace arrangrr::host

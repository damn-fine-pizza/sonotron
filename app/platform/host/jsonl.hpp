#pragma once

#include <string>

#include "arrangrr/abi.hpp"

// L0 wire encoding, host side only (D26): the core emits POD OutEvents; this
// renders them as canonical JSONL lines. The golden harness diffs this output.

namespace arrangrr::host {

// One event -> one JSON line (no trailing newline). `prefer_flats` selects
// enharmonic spelling for chord events (derived from the current key).
std::string to_jsonl(const OutEvent& ev, bool prefer_flats = false);

// Human-readable one-liner for the interactive monitor.
std::string to_human(const OutEvent& ev, bool prefer_flats = false);

}  // namespace arrangrr::host

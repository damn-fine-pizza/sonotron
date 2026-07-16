#pragma once

#include <cstdint>
#include <string>

#include "arrangrr/abi.hpp"

// Wire encoding/decoding for OutEvent::Kind::kParamState (docs/design/
// orchestrator-pipeline-extraction.md §17.3b, Phase 3b). Deliberately
// SEPARATE from jsonl.hpp's to_jsonl()/to_human(): those two render the
// canonical event stream the golden harness diffs byte-for-byte, and
// kParamState must stay INVISIBLE there forever (their kParamState case
// keeps returning an empty string on purpose, jsonl.cpp) -- giving it a real
// body there would make every golden .acmd that touches groove/arp/parts/
// style/chord-mode/chord-detect/chord-follow/key gain new output lines and
// break byte-identity. The functions here are meant to be wired ONLY into a
// control-plane broadcast (a UDS client's echo channel), never into
// script-mode stdout.
//
// param_state_to_jsonl() mirrors OutEvent::param_state()'s own packing 1:1
// (see abi.hpp's Kind::kParamState comment for the per-Param table): the
// wire carries `param` (the domain tag), `sub` (the per-Param role/field
// selector), `v0`/`v1` (the value bytes) and the tick, all numeric --
// consistent with every other to_jsonl() case's own "labels are a HOST/GUI
// concern" discipline (the mnemonic `param` string is the one exception,
// mirroring "ev"/"msg" tag strings elsewhere in jsonl.cpp).
// parse_param_state_jsonl() is the exact inverse.

namespace arrangrr::host {

struct ParamStateWire {
  Param param = Param::kNone;
  std::uint8_t sub = 0;
  std::uint8_t v0 = 0;
  std::uint8_t v1 = 0;
  Tick tick = 0;
};

// Renders a kParamState OutEvent as a JSON line (no trailing newline).
// Returns an empty string for any other Kind or for a Param this wire
// encoding does not (yet) know how to name -- mirroring jsonl.cpp's own
// "render nothing rather than lie" discipline for an unmodeled shape.
std::string param_state_to_jsonl(const OutEvent& ev);

// Parses a line produced by param_state_to_jsonl() back into its fields.
// Returns false when the line is not a recognized `"ev":"param"` line or
// names an unrecognized `param` domain -- exactly parse_brain_event's own
// "unmodeled line" convention (gui-sonotron's brain_event.cpp).
bool parse_param_state_jsonl(const std::string& line, ParamStateWire& out);

}  // namespace arrangrr::host

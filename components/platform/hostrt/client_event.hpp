#pragma once

#include <map>
#include <string>

// Pure-client wire decoder (docs/design/orchestrator-pipeline-extraction.md
// §17.5 Phase 3b): the JSONL -> event decode a socket client needs, WITHOUT
// linking arrangrr/abi.hpp -- mirrors apps/gui-sonotron/src/brain_event.hpp's
// own "pure-logic seam" discipline (that file's own header comment), reused
// here so cli-arrangrr's client half never sees a core enum either. Decodes
// the 7 shapes the host's to_jsonl()/to_human() render (jsonl.cpp) plus the
// per-client {"error":...} line. The 8th shape, kParamState's "ev":"param"
// line, is DELIBERATELY NOT decoded here -- it has its own dedicated codec
// (param_state_wire.hpp's parse_param_state_jsonl()) which already returns a
// typed ParamStateWire; a caller tries that parser first (or checks the "ev"
// field) and falls back to parse_client_event() for everything else.

namespace arrangrr::host {

// A parsed flat JSON object: string- and integer-valued keys, kept apart so
// a caller asks for exactly the type it expects. Nested objects/arrays are
// skipped during parsing (the JSONL grammar consumed here is flat) and
// booleans/null are ignored -- deliberately minimal, not a general JSON
// library, only enough to read the host's event lines without a dependency.
struct ClientJsonObject {
  std::map<std::string, std::string> strings;
  std::map<std::string, long> ints;

  bool has_string(const std::string& key) const { return strings.count(key) != 0; }
  bool has_int(const std::string& key) const { return ints.count(key) != 0; }

  std::string get_string(const std::string& key, const std::string& fallback = "") const;
  long get_int(const std::string& key, long fallback = 0) const;
};

// Parses one flat JSON object line into `out`. Returns false on any
// structural error (and NEVER throws or crashes on malformed input) -- the
// caller keeps the old state and simply ignores the line.
bool parse_client_json_object(const std::string& line, ClientJsonObject& out);

// One decoded inbound event. Only the fields relevant to `kind` are
// meaningful; the rest keep their defaults -- small POD payload, the client
// never sees raw text beyond what it explicitly asked to decode.
struct ClientEvent {
  enum class Kind {
    kUnknown,
    kMidiOut,
    kChord,
    kSection,
    kTransport,
    kWarn,
    kError,  // per-client {"error":...,"cmd":...} line (not an OutEvent)
    kChordFollowed,
    kBeat,
  };

  Kind kind = Kind::kUnknown;
  bool valid = false;
  long tick = 0;  // the "@" field, when present

  // midi-out
  int port = 0;
  std::string msg;

  // chord -- the only shipped "chord" event is the recorded-sequencer path:
  // {"in", "out", "deg"}, never a pitch-class mask.
  std::string chord_in;
  std::string chord_out;
  std::string chord_deg;

  // section
  std::string section_name;

  // transport
  std::string transport_state;  // "playing" | "paused" | "stopped"

  // warn
  std::string warn_code;

  // chord-followed: {"ev":"chord-followed","cur":"<label>","cur_pcs":<mask>,
  // "next":"<label>","next_pcs":<mask>,"src":"<producer>","@":tick}.
  std::string followed_current;
  int followed_current_pcs = 0;
  std::string followed_next;
  int followed_next_pcs = 0;
  std::string followed_source;

  // beat: {"ev":"beat","bar":N,"beat":M,"pulse":P,"@":tick}.
  int beat_bar = 0;
  int beat_index = 0;
  int beat_pulse = 0;

  // per-client error
  std::string error;
  std::string cmd;
};

// Parses one JSONL line into a ClientEvent. A line this client does not
// model, or a malformed line, yields ClientEvent{kind = kUnknown,
// valid = false} -- never a crash. A "ev":"param" line (kParamState's own
// shape) always yields kUnknown here -- decode it with
// parse_param_state_jsonl() instead (see the file header comment).
ClientEvent parse_client_event(const std::string& line);

}  // namespace arrangrr::host

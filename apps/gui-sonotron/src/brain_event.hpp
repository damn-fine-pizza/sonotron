#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// sonotron GUI wire layer (ux-workstation.md §9, gui-contract-map.md §3).
// This header is the PURE-LOGIC seam of the GUI: it depends on NOTHING but
// the C++ standard library -- no GLFW, no ImGui, no OpenGL, and above all NO
// arrangrr core headers. The GUI is a pure client of the headless core over
// the UDS-JSONL adapter; it never links the core and never sees a core enum.
// Everything here is unit-testable without a GPU or a display.
//
// The framing logic mirrors components/platform/hostrt/uds_server.hpp's LineBuffer (a
// deliberate OWN copy, not an #include of the host header -- the pure-client
// boundary rule forbids reusing host-side code) and the decoder understands
// the 8 JSONL event shapes the host currently emits (see gui-contract-map.md
// §3 for the original 5, plus the additive "chord-followed" shape landed by
// pipeline-p0-mechanical-plan.md P0-1, the additive "beat" shape landed by
// P0-2, plus the additive "clip" shape, Phase-5 Item #2): midi-out, chord,
// section, transport, warn, chord-followed, beat, clip, plus the per-client
// {"error":...} line. `kBeat` (P0-2, the transport heartbeat) is decoded:
// {"ev":"beat","bar":N,"beat":M,"pulse":P,"@":tick} -- bar/beat/pulse are
// purely numeric (core concern), a live playhead is a HOST/GUI rendering
// choice built on top of them. `kClip` (Phase-5 Item #2) is decoded:
// {"ev":"clip","id":N,"state":"stopped"|"armed"|"playing"|"queued_stop","@":
// tick} -- which Repeat-Zone cell is armed/playing/stopped.

namespace sonotron {

// Pure line-framing buffer: feed() it arbitrary byte chunks exactly as they
// arrive off a non-blocking recv(), and it emits every complete
// '\n'-terminated line (in arrival order), retaining any trailing partial
// line for the next feed(). Socket-free so the framing is testable without a
// real fd.
class LineBuffer {
 public:
  // Caps how large an unterminated (partial) line may grow before feed()
  // reports overflow -- protects the client from a broken or hostile peer
  // that never sends '\n'. JSONL event lines are short, so this is headroom.
  static constexpr std::size_t kMaxLineLength = 4096;

  // Appends `len` bytes from `data`, appending every complete line (the
  // trailing '\n', and a trailing '\r' if present, stripped) to `out_lines`.
  // Returns false when the still-pending partial line has grown past
  // kMaxLineLength; the caller should treat the connection as broken. Lines
  // already appended before the overflow remain valid.
  bool feed(const char* data, std::size_t len, std::vector<std::string>& out_lines);

  // Bytes accumulated so far for a not-yet-terminated line.
  const std::string& pending() const { return m_pending; }

 private:
  std::string m_pending;
};

// A parsed flat JSON object: string-valued keys and integer-valued keys, kept
// apart so a caller asks for exactly the type it expects. Nested objects and
// arrays are skipped during parsing (the JSONL grammar we consume is flat),
// and booleans/null are ignored. Deliberately minimal: this is not a general
// JSON library, only enough to read the host's event lines without a
// dependency.
struct JsonObject {
  std::map<std::string, std::string> strings;
  std::map<std::string, long> ints;

  bool has_string(const std::string& key) const { return strings.count(key) != 0; }
  bool has_int(const std::string& key) const { return ints.count(key) != 0; }

  std::string get_string(const std::string& key, const std::string& fallback = "") const;
  long get_int(const std::string& key, long fallback = 0) const;
};

// Parses one flat JSON object line into `out`. Returns false on any
// structural error (and NEVER throws or crashes on malformed input) -- the
// caller keeps the old state and simply ignores the line. Leading/trailing
// whitespace is tolerated; anything after the closing brace is ignored.
bool parse_json_object(const std::string& line, JsonObject& out);

// One decoded inbound event (ux-workstation.md §9's `BrainEvent`, JSONL
// parsed here). Only the fields relevant to `kind` are meaningful; the rest
// keep their defaults -- small POD payload, panels never see raw text.
struct BrainEvent {
  enum class Kind {
    kUnknown,
    kMidiOut,
    kChord,
    kSection,
    kTransport,
    kWarn,
    kError,          // per-client {"error":...,"cmd":...} line (not an OutEvent)
    kChordFollowed,  // additive, gap P0-1 (ux-workstation.md §11) -- decoded (pipeline-p0 P0-1)
    kBeat,           // additive, gap P0-2 (ux-workstation.md §11) -- decoded (pipeline-p0 P0-2)
    kClip,           // additive, Phase-5 Item #2 (docs/design/clip-primitive-design.md) -- decoded
  };

  Kind kind = Kind::kUnknown;
  bool valid = false;
  long tick = 0;  // the "@" field, when present

  // midi-out
  int port = 0;
  std::string msg;

  // chord -- the ONLY shipped "chord" event is the recorded-sequencer path
  // (gui-contract-map.md §3): {"in", "out", "deg"}, never a pitch-class mask.
  std::string chord_in;
  std::string chord_out;
  std::string chord_deg;

  // section
  std::string section_name;

  // transport
  std::string transport_state;  // "playing" | "paused" | "stopped"

  // warn
  std::string warn_code;

  // chord-followed (additive, gap P0-1): {"ev":"chord-followed","cur":"<label>",
  // "cur_pcs":<mask>,"next":"<label>","next_pcs":<mask>,"src":"<producer>","@":tick}.
  // Labels are the same rendering as the "chord" event's `out` field
  // (pitch class + quality suffix, e.g. "Cmaj7"), "-" when not valid; `_pcs`
  // is the 12-bit pitch-class mask (bit0=C .. bit11=B), 0 when not valid.
  std::string followed_current;  // "current": the chord followed this bar
  int followed_current_pcs = 0;
  std::string followed_next;  // "next": the shift-staged pending chord
  int followed_next_pcs = 0;
  std::string followed_source;  // "manual" | "detect" | "sequencer"

  // beat (additive, gap P0-2): {"ev":"beat","bar":N,"beat":M,"pulse":P,"@":
  // tick}. The transport heartbeat, one line per 24-PPQN pulse while playing
  // -- lets a client draw a moving playhead. `beat_index` (not `beat`, to
  // avoid clashing with the enclosing BrainEvent's own "kind of event" word)
  // is 1-based within the bar; `beat_pulse` is the 0..23 sub-beat index.
  int beat_bar = 0;
  int beat_index = 0;
  int beat_pulse = 0;

  // clip (additive, Phase-5 Item #2): {"ev":"clip","id":N,"state":"stopped"|
  // "armed"|"playing"|"queued_stop","@":tick}. `clip_id` addresses a
  // ClipMatrix slot (the Repeat-Zone cell id grid_panel.cpp sends);
  // `clip_state` is the label text verbatim (event_labels.hpp's
  // clip_state_name on the host side) -- a pure client renders it directly,
  // no core enum crosses this boundary.
  int clip_id = 0;
  std::string clip_state;

  // per-client error
  std::string error;
  std::string cmd;
};

// Parses one JSONL line into a BrainEvent. A line the GUI does not model, or
// a malformed line, yields BrainEvent{kind = kUnknown, valid = false} --
// never a crash.
BrainEvent parse_brain_event(const std::string& line);

}  // namespace sonotron

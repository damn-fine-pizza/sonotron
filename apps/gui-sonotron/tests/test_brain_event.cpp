// Unit tests for the GUI wire seam (ux-workstation.md §9): LineBuffer
// framing, the flat JSON-line parser, and BrainEvent decoding scoped to the 5
// shapes gui-contract-map.md §3 documents as originally shipped, plus the
// additive "chord-followed" shape (gap P0-1, pipeline-p0-mechanical-plan.md)
// and the per-client error line. No GPU, no display, no core.

#include "src/brain_event.hpp"

#include "test.hpp"

using sonotron::BrainEvent;
using sonotron::JsonObject;
using sonotron::LineBuffer;
using sonotron::parse_brain_event;
using sonotron::parse_json_object;

namespace {

void test_line_buffer() {
  LineBuffer buf;
  std::vector<std::string> lines;

  // Two lines in one chunk, split at '\n'.
  const std::string wire = "transport start\nbpm 120\n";
  CHECK(buf.feed(wire.data(), wire.size(), lines));
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "transport start");
  CHECK(lines[1] == "bpm 120");

  // Partial line retained across feeds; CRLF stripped.
  lines.clear();
  const std::string part1 = "hel";
  const std::string part2 = "lo\r\nworld";
  CHECK(buf.feed(part1.data(), part1.size(), lines));
  CHECK(lines.empty());
  CHECK(buf.feed(part2.data(), part2.size(), lines));
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "hello");
  CHECK(buf.pending() == "world");

  // Overflow: an unterminated line past the cap reports false.
  LineBuffer big;
  std::vector<std::string> none;
  const std::string flood(LineBuffer::kMaxLineLength + 1, 'x');
  CHECK(!big.feed(flood.data(), flood.size(), none));
  CHECK(none.empty());
}

void test_json_object() {
  JsonObject obj;
  CHECK(parse_json_object(R"({"a":"hi","n":-42,"@":1000})", obj));
  CHECK(obj.get_string("a") == "hi");
  CHECK(obj.get_int("n") == -42);
  CHECK(obj.get_int("@") == 1000);
  CHECK(!obj.has_string("missing"));
  CHECK(obj.get_int("missing", 7) == 7);

  // Escapes in a string value.
  JsonObject esc;
  CHECK(parse_json_object(R"({"s":"a\"b\\c\n"})", esc));
  CHECK(esc.get_string("s") == "a\"b\\c\n");

  // Nested object/array values are skipped, siblings still parse.
  JsonObject nested;
  CHECK(parse_json_object(R"({"obj":{"x":1},"arr":[1,2,3],"tail":5})", nested));
  CHECK(nested.get_int("tail") == 5);
  CHECK(!nested.has_int("obj"));

  // Booleans/null ignored, whitespace tolerated.
  JsonObject lit;
  CHECK(parse_json_object(R"(  { "b" : true , "z" : null , "k" : 2 } )", lit));
  CHECK(lit.get_int("k") == 2);

  // Malformed inputs: no crash, returns false.
  JsonObject bad;
  CHECK(!parse_json_object("not json at all", bad));
  CHECK(!parse_json_object(R"({"unterminated)", bad));
  CHECK(!parse_json_object(R"({"k":})", bad));
  CHECK(!parse_json_object("", bad));
}

void test_parse_midi_out() {
  const BrainEvent ev = parse_brain_event(
      R"({"ev":"midi-out","port":0,"msg":"noteon","ch":1,"note":60,"vel":100,"@":3})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kMidiOut);
  CHECK(ev.port == 0);
  CHECK(ev.msg == "noteon");
  CHECK(ev.tick == 3);
}

void test_parse_chord() {
  // The only shipped "chord" event: the recorded-sequencer path
  // (gui-contract-map.md §3) -- {in, out, deg}, never a pitch-class mask.
  const BrainEvent ev =
      parse_brain_event(R"({"ev":"chord","in":"C4","out":"Cmaj","deg":"I","@":384})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kChord);
  CHECK(ev.chord_in == "C4");
  CHECK(ev.chord_out == "Cmaj");
  CHECK(ev.chord_deg == "I");
  CHECK(ev.tick == 384);
}

void test_parse_section() {
  const BrainEvent ev = parse_brain_event(R"({"ev":"section","name":"varA","@":0})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kSection);
  CHECK(ev.section_name == "varA");
}

void test_parse_transport() {
  const BrainEvent ev = parse_brain_event(R"({"ev":"transport","state":"playing","@":1})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kTransport);
  CHECK(ev.transport_state == "playing");
}

void test_parse_warn() {
  const BrainEvent ev = parse_brain_event(R"({"ev":"warn","code":"unsupported","@":2})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kWarn);
  CHECK(ev.warn_code == "unsupported");
}

// The additive "chord-followed" shape (gap P0-1) -- exact wire shape ported
// from spikes/kchordfollowed and rendered by components/hostrt/jsonl.cpp.
void test_parse_chord_followed() {
  const BrainEvent ev = parse_brain_event(
      R"({"ev":"chord-followed","cur":"Cmaj7","cur_pcs":2193,"next":"-","next_pcs":0,)"
      R"("src":"manual","@":0})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kChordFollowed);
  CHECK(ev.followed_current == "Cmaj7");
  CHECK(ev.followed_current_pcs == 2193);
  CHECK(ev.followed_next == "-");
  CHECK(ev.followed_next_pcs == 0);
  CHECK(ev.followed_source == "manual");
  CHECK(ev.tick == 0);

  // A staged (pending) chord: both cur and next carry a real label. (G7 =
  // root pc 7, dom7 offsets {0,4,7,10} -> pcs {7,11,2,5} -> mask 2212.)
  const BrainEvent staged = parse_brain_event(
      R"({"ev":"chord-followed","cur":"Cmaj7","cur_pcs":2193,"next":"G7","next_pcs":2212,)"
      R"("src":"detect","@":42})");
  CHECK(staged.valid);
  CHECK(staged.followed_next == "G7");
  CHECK(staged.followed_next_pcs == 2212);
  CHECK(staged.followed_source == "detect");
}

void test_parse_error() {
  const BrainEvent ev = parse_brain_event(R"({"error":"unknown command: x","cmd":"x"})");
  CHECK(ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kError);
  CHECK(ev.error == "unknown command: x");
  CHECK(ev.cmd == "x");
}

void test_parse_malformed_and_unrecognized() {
  // Malformed / unknown -> invalid, never a crash.
  const BrainEvent ev = parse_brain_event("garbage{{");
  CHECK(!ev.valid);
  CHECK(ev.kind == BrainEvent::Kind::kUnknown);

  const BrainEvent ev2 = parse_brain_event(R"({"ev":"nope"})");
  CHECK(!ev2.valid);
  CHECK(ev2.kind == BrainEvent::Kind::kUnknown);
}

// The remaining additive shapes (ux-workstation.md §11: beat/clip) are NOT
// decoded yet -- pinned explicitly so a future slice adding them is a
// deliberate, reviewed change to this test, not a silent behaviour drift.
// "chord-followed" graduated out of this list -- see test_parse_chord_followed.
void test_additive_shapes_stay_undecoded() {
  const BrainEvent beat = parse_brain_event(R"({"ev":"beat","bar":3,"beat":2,"@":5})");
  CHECK(!beat.valid);
  CHECK(beat.kind == BrainEvent::Kind::kUnknown);

  const BrainEvent clip = parse_brain_event(R"({"ev":"clip","id":1,"state":"playing","@":6})");
  CHECK(!clip.valid);
  CHECK(clip.kind == BrainEvent::Kind::kUnknown);
}

}  // namespace

int main() {
  test_line_buffer();
  test_json_object();
  test_parse_midi_out();
  test_parse_chord();
  test_parse_section();
  test_parse_transport();
  test_parse_warn();
  test_parse_chord_followed();
  test_parse_error();
  test_parse_malformed_and_unrecognized();
  test_additive_shapes_stay_undecoded();
  return sonotron::test::failures();
}

// Unit tests for brain_event_from_outevent() -- the in-process OutEvent ->
// BrainEvent decode (Phase 2b, docs/design/sonotron-server-phase2-brief.md
// "Data on the ring", Corelli §15.3/§15.4). Each case mirrors what the
// EXISTING JSONL wire (components/hostrt/jsonl.cpp's to_jsonl(), and
// apps/gui-sonotron/tests/test_app_state.cpp's apply_line() cases) already
// asserts for the same OutEvent shape, so a drift between the two decode
// paths would show up here even though this test never touches a socket.

#include "src/brain_event_from_outevent.hpp"

#include "arrangrr/abi.hpp"
#include "arrangrr/chord/followed_context.hpp"
#include "runtime/transport.hpp"
#include "test.hpp"

using arrangrr::ChordQuality;
using arrangrr::ChordState;
using arrangrr::OutEvent;
using arrangrr::Producer;
using arrangrr::TransportState;
using arrangrr::WarnCode;
using sonotron::brain_event_from_outevent;
using sonotron::BrainEvent;

namespace {

void test_section_decode() {
  const OutEvent ev = OutEvent::section(2 /* kVarA */, 384);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.valid);
  CHECK(decoded.kind == BrainEvent::Kind::kSection);
  CHECK(decoded.section_name == "varA");
  CHECK(decoded.tick == 384);
}

void test_transport_decode() {
  const OutEvent playing =
      OutEvent::transport(static_cast<std::uint16_t>(TransportState::kPlaying), 1);
  CHECK(brain_event_from_outevent(playing, false).transport_state == "playing");

  const OutEvent stopped =
      OutEvent::transport(static_cast<std::uint16_t>(TransportState::kStopped), 2);
  CHECK(brain_event_from_outevent(stopped, false).transport_state == "stopped");
}

void test_warn_decode() {
  const OutEvent ev = OutEvent::warn(WarnCode::kSchedulerFull, 7);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.kind == BrainEvent::Kind::kWarn);
  CHECK(decoded.warn_code == "scheduler_full");
}

void test_beat_decode() {
  const OutEvent ev = OutEvent::beat(5, 2, 12, 999);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.kind == BrainEvent::Kind::kBeat);
  CHECK(decoded.beat_bar == 5);
  CHECK(decoded.beat_index == 2);
  CHECK(decoded.beat_pulse == 12);
}

void test_chord_decode_prefers_sharps_by_default() {
  // root_note=60 (C4), degree=0, quality=kMaj -- mirrors the golden's own
  // simplest "chord" shape (in="C4", out="C", deg="I").
  const OutEvent ev =
      OutEvent::chord(0, 0, static_cast<std::uint8_t>(ChordQuality::kMaj), 60, 3, 100, 10);
  const BrainEvent decoded = brain_event_from_outevent(ev, /*prefer_flats=*/false);
  CHECK(decoded.kind == BrainEvent::Kind::kChord);
  CHECK(decoded.chord_in == "C4");
  CHECK(decoded.chord_out == "C");
  CHECK(decoded.chord_deg == "I");
}

void test_chord_followed_decode() {
  const ChordState cur{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};   // C
  const ChordState next{.root_pc = 7, .quality = ChordQuality::kMin, .valid = true};  // Gm
  const OutEvent ev = OutEvent::chord_followed(cur, next, Producer::kDetect, 42);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.kind == BrainEvent::Kind::kChordFollowed);
  CHECK(decoded.followed_current == "C");
  CHECK(decoded.followed_next == "Gm");
  CHECK(decoded.followed_source == "detect");
  CHECK(decoded.followed_current_pcs != 0);
  CHECK(decoded.followed_next_pcs != 0);
}

void test_chord_followed_invalid_renders_dash() {
  const ChordState invalid{};  // valid == false
  const OutEvent ev = OutEvent::chord_followed(invalid, invalid, Producer::kManual, 1);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.followed_current == "-");
  CHECK(decoded.followed_current_pcs == 0);
  CHECK(decoded.followed_next == "-");
}

void test_midi_noteon_decode() {
  const arrangrr::MidiMessage msg{.status = 0x90, .d1 = 64, .d2 = 100};
  const OutEvent ev = OutEvent::midi(0, msg, 5);
  const BrainEvent decoded = brain_event_from_outevent(ev, false);
  CHECK(decoded.kind == BrainEvent::Kind::kMidiOut);
  CHECK(decoded.port == 0);
  CHECK(decoded.msg == "noteon");
}

}  // namespace

int main() {
  test_section_decode();
  test_transport_decode();
  test_warn_decode();
  test_beat_decode();
  test_chord_decode_prefers_sharps_by_default();
  test_chord_followed_decode();
  test_chord_followed_invalid_renders_dash();
  test_midi_noteon_decode();
  return sonotron::test::failures();
}

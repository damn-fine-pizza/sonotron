// D47 functional tests: the explicit chord-follow harmony-source selector.
//
// Three producers can update the arranger-followed chord context
// (Engine::m_chords, read back as e.chords().state()): live MIDI detection,
// the ChordSequencer, and manual `chord play`. The ChordFollow selector gates
// which of them may STEER the followed chord; the other side effects (sounding
// notes, emitting the chord event, tracking the held detect set) must always
// run. kAuto keeps the legacy last-writer-wins behavior.
//
// These are engine-level functional tests driven through the real ABI (binary
// commands + raw MIDI in), asserting on the OBSERVABLE followed chord and on
// the observable side effects (kChord events, note-on output, held count).

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

// The chord voicing is routed to a distinct channel so its note-ons can be
// isolated from the pass-through of the held detect keys (channel 0).
constexpr std::uint8_t kChordChannel = 5;

struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           std::uint16_t idx = 0) {
    Command command{.op = op, .param = p, .idx = idx, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds a NoteOn (vel>0) or NoteOff (vel==0) on the detect port (0).
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Appends a chord step to the current sequence (smart quality by default),
  // mirroring the kSeqAdd packing used by test_chord_seq.
  void seq_add(std::uint8_t note, Tick dur, std::int8_t quality = -1, std::uint8_t vel = 100) {
    cmd(Param::kSeqAdd, note, (quality + 1) | (vel << 8), static_cast<std::int32_t>(dur));
  }

  // Standard C major band with the chord voicing routed to kChordChannel.
  void setup() {
    cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
    cmd(Param::kChordOut, 0 | (kChordChannel << 8), 0, 0, Op::kSet);
  }

  const ChordState& followed() const { return e.chords().state(); }

  // Note-ons emitted by the chord voicing (isolated by channel).
  int chord_note_ons() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == kChordChannel && o.msg.d2 > 0) {
        ++n;
      }
    }
    return n;
  }
  // True if a chord event with the given root MIDI note was emitted (root_note
  // is carried in OutEvent::msg.status for kChord).
  bool saw_chord_event(std::uint8_t root_note) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChord && o.msg.status == root_note) {
        return true;
      }
    }
    return false;
  }
};

// Arms and plays a one-step V chord (G7 in C major): degree 4, root note 67,
// root_pc 7, smart quality Dom7. Non-looping. The step fires on transport start.
void arm_seq_G(Band& b) {
  b.cmd(Param::kSeqNew);
  b.seq_add(67, kTicksPerBar);  // G -> V
  b.cmd(Param::kSeqPlay);
}

// The reference roots/qualities used across the cases.
constexpr std::uint8_t kSeqRootPc = 7;                 // G (V of C)
constexpr ChordQuality kSeqQuality = ChordQuality::kDom7;
constexpr std::uint8_t kDetectRootPc = 9;              // A (A minor triad)
constexpr ChordQuality kDetectQuality = ChordQuality::kMin;
constexpr std::uint8_t kManualRootPc = 0;              // C (I of C)
constexpr ChordQuality kManualQuality = ChordQuality::kMaj7;

// Holds an A minor triad (A3 C4 E4) on the detect port.
void hold_A_minor(Band& b) {
  b.key(69, 100);
  b.key(72, 100);
  b.key(76, 100);
}

// --- Case 1: kAuto is legacy last-writer-wins ------------------------------
// All three producers steer in turn; the followed chord equals whoever wrote
// last, and each write is observable in program order.
void test_auto_is_legacy_last_writer_wins() {
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kAuto), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);

  // (a) detection steers.
  hold_A_minor(b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kDetectRootPc);
  CHECK(b.followed().quality == kDetectQuality);

  // (b) manual play steers over detection.
  b.cmd(Param::kChordPlay, 60, -1, 100);  // C in C major -> Cmaj7
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kManualRootPc);
  CHECK(b.followed().quality == kManualQuality);

  // (c) the sequencer steers over both when it fires on transport start.
  arm_seq_G(b);
  b.cmd(Param::kTransportStart);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kSeqRootPc);
  CHECK(b.followed().quality == kSeqQuality);
}

// --- Case 2: kDetect -------------------------------------------------------
// A detected chord steers; a sequencer firing a DIFFERENT chord does not
// overwrite the followed context, yet the sequencer still sounds and emits.
void test_detect_gates_out_the_sequencer() {
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);

  hold_A_minor(b);  // followed := A minor
  CHECK(b.followed().valid && b.followed().root_pc == kDetectRootPc);

  arm_seq_G(b);
  b.ev.clear();
  b.cmd(Param::kTransportStart);  // sequencer fires G7 at tick 0

  // The sequencer did NOT steer: the followed context is still the detected A.
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kDetectRootPc);
  CHECK(b.followed().quality == kDetectQuality);

  // But the sequencer's side effects DID happen: it emitted its chord event and
  // sounded its voicing.
  CHECK(b.saw_chord_event(60 + kSeqRootPc));  // root note 67
  CHECK(b.chord_note_ons() > 0);
}

// --- Case 3: kSequencer ----------------------------------------------------
// The sequencer steers; a live detect of a different chord does not overwrite
// the followed context, but the detector still tracks the held set.
void test_sequencer_gates_out_detection() {
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kSequencer), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);

  arm_seq_G(b);
  b.cmd(Param::kTransportStart);  // sequencer fires G7 -> followed := G
  CHECK(b.followed().valid && b.followed().root_pc == kSeqRootPc);

  hold_A_minor(b);  // three held keys; detection recognizes but must NOT steer

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kSeqRootPc);
  CHECK(b.followed().quality == kSeqQuality);

  // The held set is still tracked for the display (observable seam).
  CHECK(b.e.chord_held_count() == 3);
}

// --- Case 4: kManual -------------------------------------------------------
// Manual play steers; neither detect nor sequencer overwrite it. Manual play
// still sounds its notes.
void test_manual_gates_out_detect_and_sequencer() {
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kManual), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);

  hold_A_minor(b);  // detection gated out: followed still invalid
  CHECK(!b.followed().valid);

  b.ev.clear();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // manual C -> Cmaj7 steers
  CHECK(b.followed().valid && b.followed().root_pc == kManualRootPc);
  CHECK(b.saw_chord_event(60));  // manual play sounded/emitted its chord
  CHECK(b.chord_note_ons() > 0);

  arm_seq_G(b);
  b.cmd(Param::kTransportStart);  // sequencer fires G7, gated out

  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kManualRootPc);
  CHECK(b.followed().quality == kManualQuality);
}

// --- Case 5: gated producer + lifecycle default ----------------------------
// A gated-out producer firing at transport start must NOT harmonize the band to
// ITS chord. Transport start seeds the home-key tonic via the lifecycle default
// (establish_default), so the followed context is the TONIC, never the gated
// sequencer's G7.
void test_gated_sequencer_does_not_steer_over_seeded_tonic() {
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);

  CHECK(!b.followed().valid);  // nothing has steered yet (no transport start)

  arm_seq_G(b);
  b.cmd(Param::kTransportStart);  // sequencer sounds G7 but is gated out (kDetect)

  // The sequencer sounded (side effect), but the gate blocked it from steering:
  // the followed context is the seeded home-key tonic, NOT the sequencer's G.
  CHECK(b.saw_chord_event(60 + kSeqRootPc));
  CHECK(b.followed().valid);                  // seeded tonic, not unset
  CHECK(b.followed().root_pc != kSeqRootPc);  // never the gated-out sequencer's G
}

}  // namespace

int main() {
  test_auto_is_legacy_last_writer_wins();
  test_detect_gates_out_the_sequencer();
  test_sequencer_gates_out_detection();
  test_manual_gates_out_detect_and_sequencer();
  test_gated_sequencer_does_not_steer_over_seeded_tonic();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_follow: all OK\n");
  }
  return arrangrr::test::failures();
}

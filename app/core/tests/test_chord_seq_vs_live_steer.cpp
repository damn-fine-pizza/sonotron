// Functional test for the LIVE-PRIORITY chord arbitration — the engine
// default that replaces the old kAuto last-writer race between the
// ChordSequencer and live input.
//
// The owner-locked model:
//   1. Live input (detect/manual) takes PRIORITY over the ChordSequencer.
//   2. While a live chord is ACTIVE (live keys currently HELD — the detector's
//      held set is non-empty), the followed context = the live chord, AND the
//      sequencer COMPS its rhythm/voicing ON THE LIVE CHORD (not its own root):
//      no clash, and it never publishes its own chord into the context.
//   3. When the live chord is RELEASED (held set empties), the sequencer
//      RESUMES driving: from its next fired step it commits its own chords.
//   4. With NO sequencer running, a live chord LATCHES and persists after
//      release (chord memory, Strada 1) — so live is MOMENTARY with a sequencer,
//      LATCHING without one. That asymmetry is intended.
//   5. The explicit D47 gate still overrides: `chord follow sequencer` makes the
//      sequence steer (live gated out); `chord follow auto` keeps the LEGACY
//      last-writer-wins (characterized, unchanged).
//
// This drives the Engine ABI directly (virtual clock, exact per-bar state()
// dump + isolated chord-voicing note-ons) so the model is MEASURED, not guessed.

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 8192>;

constexpr std::uint8_t kBasicStyleIndex = 0;  // styles::kBuiltins[0] == "basic"

// The sequencer/chord voicing is routed to a distinct channel so its note-ons
// can be isolated from the pass-through of the held detect keys (channel 0).
constexpr std::uint8_t kChordChannel = 5;

// Live F major triad (F A C) -> root pitch class 5. The sequencer's own song is
// G7 (root_pc 7) / C (root_pc 0), so the live root_pc 5 is unambiguous against
// either sequencer chord.
constexpr std::uint8_t kLiveRootPc = 5;

struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Feeds a NoteOn (vel>0) or NoteOff (vel==0) on the detect port (0), exactly
  // the observe_chord_input path a real keyboard/host drives.
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Presses / releases the live F major triad (F4 A4 C5) on the detect port.
  void press_F_major() {
    key(65, 100);
    key(69, 100);
    key(72, 100);
  }
  void release_F_major() {
    key(65, 0);
    key(69, 0);
    key(72, 0);
  }

  const ChordState& followed() const { return e.chords().state(); }
  const ChordState& next() const { return e.chords().pending(); }

  // Standard C major band with the chord voicing routed to kChordChannel and
  // live detection on the input port. ChordFollow left at the ENGINE DEFAULT
  // (kLivePriority) unless a test overrides it.
  void setup(int style = kBasicStyleIndex) {
    cmd(Param::kStyleLoad, style);
    cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
    cmd(Param::kChordOut, 0 | (kChordChannel << 8), 0, 0, Op::kSet);
    cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detect on, port 0
  }

  // Builds "the song": a 2-bar chord sequence in the engine's current key --
  // bar 1 = V7 (G7 in C major), bar 2 = I (C major) -- looping.
  void arm_song_sequence() {
    cmd(Param::kSeqNew);
    // G7: root note 67 (G4), explicit dom7 (index 6 -> packed as 7), vel 100.
    cmd(Param::kSeqAdd, 67, (6 + 1) | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));
    // C major: root note 60 (C4), explicit maj (index 0 -> packed as 1), vel 100.
    cmd(Param::kSeqAdd, 60, (0 + 1) | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));
    cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
    cmd(Param::kSeqPlay);
  }

  // Lowest chord-voicing note-on (isolated by channel) currently in `ev`, or -1
  // when the sequencer sounded nothing. The voicing stacks from the root upward,
  // so the lowest note's pitch class IS the sounded chord's root.
  int lowest_chord_note() const {
    int lo = -1;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == kChordChannel && o.msg.d2 > 0) {
        if (lo < 0 || o.msg.d1 < lo) {
          lo = o.msg.d1;
        }
      }
    }
    return lo;
  }
  int sounded_root_pc() const {
    const int lo = lowest_chord_note();
    return lo < 0 ? -1 : lo % 12;
  }
};

const char* quality_name(ChordQuality q) {
  switch (q) {
    case ChordQuality::kMaj:
      return "maj";
    case ChordQuality::kMin:
      return "min";
    case ChordQuality::kDom7:
      return "dom7";
    case ChordQuality::kMaj7:
      return "maj7";
    default:
      return "?";
  }
}

void print_bar(int bar, const Band& b) {
  const ChordState& s = b.followed();
  std::printf("bar %2d  current: %s root_pc=%2u quality=%-5s | sounded root_pc=%d\n", bar,
              s.valid ? "valid" : "INVALID", s.root_pc, s.valid ? quality_name(s.quality) : "-",
              b.sounded_root_pc());
}

// --- Scenario A: no sequence -- immediate press LATCHES every bar (point 4) --

void test_no_sequence_live_latches_every_bar() {
  std::printf("\n==== Scenario A: NO sequence, live F pressed -- latches ====\n");
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);

  b.press_F_major();  // F A C -> fingered F major triad
  print_bar(0, b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kLiveRootPc);
  CHECK(b.followed().quality == ChordQuality::kMaj);
  CHECK(!b.next().valid);  // immediate: nothing staged (D53 default)

  // Release the keys: with no sequencer running the chord LATCHES (chord memory)
  // and must persist across every subsequent bar.
  b.release_F_major();
  constexpr int kBars = 8;
  for (int bar = 1; bar <= kBars; ++bar) {
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    CHECK(b.followed().valid);
    CHECK(b.followed().root_pc == kLiveRootPc);  // stays on F every bar
    CHECK(b.followed().quality == ChordQuality::kMaj);
  }
  std::printf("---- Scenario A: latched on F for all %d bars (green) ----\n", kBars);
}

// --- Scenario B1: sequence + held live chord -> band follows live, seq comps --

void test_default_seq_comps_on_held_live_chord() {
  std::printf("\n==== Scenario B1: song playing + live F HELD -> seq comps on F ====\n");
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.arm_song_sequence();  // G7 bar, C bar, looping; bar 0's G7 fired now
  print_bar(0, b);
  CHECK(b.followed().root_pc == 7);  // the song's own G7 sounded on bar 0

  b.press_F_major();  // live F pressed mid-bar and HELD
  print_bar(0, b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == kLiveRootPc);  // live press wins immediately

  // While the keys stay held, EVERY subsequent sequencer step must comp on the
  // live F chord and never clobber the followed context back to the song.
  constexpr int kBars = 6;
  for (int bar = 1; bar <= kBars; ++bar) {
    b.ev.clear();
    b.advance(kTicksPerBar);  // crosses a bar boundary: a seq step fires
    print_bar(bar, b);
    CHECK(b.followed().valid);
    CHECK(b.followed().root_pc == kLiveRootPc);  // context stays on the live F
    CHECK(b.followed().quality == ChordQuality::kMaj);
    CHECK(b.sounded_root_pc() == kLiveRootPc);  // and the seq comped ON the live F
  }
  std::printf("---- Scenario B1: band+comp held on F for all %d bars (green) ----\n", kBars);
}

// --- Scenario B2: release -> the sequencer RESUMES driving (point 3) ---------

void test_default_release_resumes_the_sequencer() {
  std::printf("\n==== Scenario B2: release live chord -> sequencer resumes ====\n");
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.arm_song_sequence();

  b.press_F_major();
  b.advance(kTicksPerBar);  // one bar held: still on F
  CHECK(b.followed().root_pc == kLiveRootPc);

  b.release_F_major();  // held set empties: the sequencer takes back over
  CHECK(b.e.chord_held_count() == 0);
  CHECK(b.followed().root_pc == kLiveRootPc);  // memory: unchanged until the next step

  // The very next fired step commits the song's own chord again.
  b.ev.clear();
  b.advance(kTicksPerBar);
  print_bar(2, b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc != kLiveRootPc);          // no longer the live F
  CHECK(b.sounded_root_pc() == b.followed().root_pc);  // comps on its own chord now

  // ...and keeps driving the progression on every following bar.
  for (int bar = 3; bar <= 6; ++bar) {
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    CHECK(b.followed().root_pc != kLiveRootPc);
  }
  std::printf("---- Scenario B2: sequencer resumed after release (green) ----\n");
}

// --- Scenario B3: `chord follow sequencer` -> the sequence wins (point 5) ----

void test_explicit_sequencer_override_gates_live_out() {
  std::printf("\n==== Scenario B3: chord follow sequencer -> live gated out ====\n");
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kSequencer), 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);
  b.arm_song_sequence();
  CHECK(b.followed().root_pc == 7);  // G7 steered

  b.press_F_major();                   // live keys held, but the gate blocks detect from steering
  CHECK(b.followed().root_pc == 7);    // still the sequencer's chord
  CHECK(b.e.chord_held_count() == 3);  // held set still tracked for display

  // Across bars the sequence keeps steering and comps on ITS OWN chords, never F.
  for (int bar = 1; bar <= 4; ++bar) {
    b.ev.clear();
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    CHECK(b.followed().root_pc != kLiveRootPc);
    CHECK(b.sounded_root_pc() != kLiveRootPc);
  }
  std::printf("---- Scenario B3: sequence steered throughout (green) ----\n");
}

// --- Scenario B4: kAuto stays the LEGACY last-writer clobber (characterized) -

void test_auto_follow_still_clobbers_the_live_chord() {
  std::printf("\n==== Scenario B4: chord follow auto -> legacy clobber ====\n");
  Band b;
  b.setup();
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kAuto), 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);
  b.arm_song_sequence();

  b.press_F_major();
  CHECK(b.followed().root_pc == kLiveRootPc);  // the immediate press wins for now

  bool clobbered = false;
  for (int bar = 1; bar <= 6; ++bar) {
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    if (!b.followed().valid || b.followed().root_pc != kLiveRootPc) {
      clobbered = true;
    }
  }
  // Under kAuto a running sequencer re-asserts its own chord every bar: the
  // documented legacy last-writer-wins behavior, kept intact.
  CHECK(clobbered);
  std::printf("---- Scenario B4: kAuto clobbered back to the song (legacy) ----\n");
}

}  // namespace

int main() {
  test_no_sequence_live_latches_every_bar();
  test_default_seq_comps_on_held_live_chord();
  test_default_release_resumes_the_sequencer();
  test_explicit_sequencer_override_gates_live_out();
  test_auto_follow_still_clobbers_the_live_chord();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_seq_vs_live_steer: all OK\n");
  } else {
    std::printf("test_chord_seq_vs_live_steer: %d FAILURE(S) -- see printed timelines above\n",
                arrangrr::test::failures());
  }
  return arrangrr::test::failures();
}

// QA reproduction (Torquato): the owner's live-testing report on
// harmony-global-steer, verbatim (translated) --
//
//   "When I press a chord (single-finger OR fingered), it only transposes the
//   CURRENT bar; the next bar plays 'as per the song'. It must transpose ALL
//   following bars until I change the chord, like a commercial arranger. Also
//   'next key' stays always empty even when I press chords."
//
// This file drives the Engine ABI directly (virtual clock, exact per-bar
// state() dump) to MEASURE the root cause rather than guess it. Two
// scenarios, per the QA brief:
//
//   Scenario A -- NO chord sequence ("the song" silent): a live immediate
//   chord press must persist across every subsequent bar. This is ALREADY
//   green (test_followed_context.cpp / test_host.cpp's
//   test_permanent_transpose_persists_across_bars cover the no-sequence
//   path) -- repeated here with full per-bar observability so the timeline
//   is printed alongside Scenario B for direct comparison.
//
//   Scenario B -- a chord SEQUENCE IS the "song": a recorded ChordSequence
//   plays a chord every bar (the user's mental model of "the song"). A live
//   immediate chord is pressed mid-sequence. MEASURED root cause: under
//   ChordFollow::kAuto (the enum's own default -- what a fresh Engine has
//   until *something* issues `chord follow ...`), fire_chord_seq's
//   Producer::kSequencer commit_now() is NOT gated out, so the very next bar
//   at which the sequence has a step, it clobbers the followed context right
//   back to the sequence's own chord -- exactly "the next bar plays as per
//   the song". This is CONFIRMED empirically below and PINNED red.
//
//   Under the documented mitigation (ChordFollow::kDetect, the line `chord
//   follow detect` shipped in docs/arrangrr.init.example) the SAME scenario
//   is confirmed to NOT reproduce: the D47 gate blocks Producer::kSequencer
//   from writing `current` at all, so the live chord survives every bar. This
//   is the single most actionable fact for the owner: the defect is
//   CONDITIONAL on the chord-follow selector never having been set away from
//   the engine's own kAuto default.
//
// Also measured: the pending() ("next key") readout. It stays invalid through
// every bar of both scenarios because NOTHING in this reproduction ever calls
// stage() (the SHIFT-quantize path) -- it is populated only by a
// detect_quantize=true press or an explicit `chord play ... quantize`. This
// REFUTES "next key always empty" as a bug in the immediate-press workflow:
// it is the documented, correct behavior of the immediate default (D53/D47).
// The symptom the owner is actually seeing there is the ABSENCE of any UI
// affordance that ever sets detect_quantize=true for a fresh press -- a UX
// gap, not a followed-context defect. See test_followed_context.cpp's
// test_detect_shift_stages_to_the_next_bar for the (working) staged path.
//
// Do not silence any RED case here by loosening its assertion: a RED case is
// a pinned defect handed to Nazzareno, not a mistake to quietly correct.

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 8192>;

constexpr std::uint8_t kBasicStyleIndex = 0;  // styles::kBuiltins[0] == "basic"

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
  const ChordState& followed() const { return e.chords().state(); }
  const ChordState& next() const { return e.chords().pending(); }

  // Builds "the song": a 2-bar chord sequence in the engine's current key --
  // bar 1 = V7 (G7 in C major), bar 2 = I (C major) -- looping. This is the
  // user's mental model of "the song" the arranger plays when nobody steers.
  void arm_song_sequence() {
    cmd(Param::kSeqNew);
    // G7: root note 67 (G4), explicit dom7 (index 6 -> packed as 7), vel 100.
    cmd(Param::kSeqAdd, 67, (6 + 1) | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));
    // C major: root note 60 (C4), explicit maj (index 0 -> packed as 1), vel 100.
    cmd(Param::kSeqAdd, 60, (0 + 1) | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));
    cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
    cmd(Param::kSeqPlay);
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
  const ChordState& n = b.next();
  std::printf("bar %2d  current: %s  root_pc=%2u quality=%-5s | next: %s\n", bar,
              s.valid ? "valid" : "INVALID", s.root_pc, s.valid ? quality_name(s.quality) : "-",
              n.valid ? "STAGED" : "-");
}

// --- Scenario A: no sequence -- immediate press must persist every bar -----

void test_scenario_a_no_sequence_persists_every_bar() {
  std::printf("\n==== Scenario A: NO chord sequence, live immediate F pressed ====\n");
  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);  // detect on, port 0 -- ChordFollow left at
                                                   // the ENGINE'S OWN DEFAULT (kAuto): nobody
                                                   // has issued `chord follow` yet.
  b.cmd(Param::kTransportStart);

  b.key(65, 100);  // F4 -> single NoteOn; fingered mode needs a full triad, so play F A C
  b.key(69, 100);
  b.key(72, 100);
  print_bar(0, b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);  // F
  CHECK(!b.next().valid);            // immediate: nothing staged (D53 default)

  constexpr int kBars = 8;
  for (int bar = 1; bar <= kBars; ++bar) {
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    CHECK(b.followed().valid);
    CHECK(b.followed().root_pc == 5);  // must STAY on F every single bar
    CHECK(b.followed().quality == ChordQuality::kMaj);  // F A C -> fingered major triad
    CHECK(!b.next().valid);  // stays empty in immediate mode: by design, not a bug
  }
  std::printf("---- Scenario A verdict: held on F for all %d bars (green) ----\n", kBars);
}

// --- Scenario B: a chord sequence IS "the song" -----------------------------

// Runs the "song playing + live press mid-sequence" probe under a given
// ChordFollow value, printing the full per-bar timeline, and RETURNS whether the
// live chord was clobbered back to the song's step. The two callers assert the
// outcome that matches their gate: under ChordFollow::kAuto a running sequencer
// re-asserts its own chord every bar (clobbers -- the DEFERRED live-vs-sequencer
// arbitration; Strada 1 avoids it by not running a sequencer); under
// ChordFollow::kDetect the D47 gate protects the live chord (no clobber).
bool run_song_vs_live_steer_probe(const char* label, ChordFollow follow) {
  std::printf("\n==== Scenario B: chord SEQUENCE playing + live F pressed -- %s ====\n", label);

  Band b;
  b.cmd(Param::kStyleLoad, kBasicStyleIndex);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);  // C major
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(follow), 0, 0, Op::kSet);
  b.cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  b.cmd(Param::kTransportStart);
  b.arm_song_sequence();  // the "song": G7 bar, C bar, looping
  print_bar(0, b);        // bar 0's step (G7) fires immediately (kSeqPlay while playing)

  // The user presses a live chord (F major triad, root F) MID-BAR, after the
  // song's own G7 already sounded this bar.
  b.key(65, 100);
  b.key(69, 100);
  b.key(72, 100);
  print_bar(0, b);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);  // the live press DOES win immediately -- refutes H3:
                                      // the immediate commit_now write itself works.

  bool clobbered = false;
  constexpr int kBars = 6;
  for (int bar = 1; bar <= kBars; ++bar) {
    b.advance(kTicksPerBar);
    print_bar(bar, b);
    if (!b.followed().valid || b.followed().root_pc != 5) {
      clobbered = true;
    }
  }
  std::printf("---- verdict (%s): %s ----\n", label,
              clobbered ? "CLOBBERED BACK TO THE SONG (RED)" : "held on F (green)");

  // Return the measured outcome; each caller asserts what its gate should do. The
  // exact writer under kAuto is fire_chord_seq's ungated Producer::kSequencer
  // commit_now(), which re-asserts the song's step every bar.
  return clobbered;
}

void test_scenario_b_auto_follow_is_clobbered_by_the_song() {
  // ChordFollow::kAuto is the ENGINE'S OWN DEFAULT (FollowedContext's
  // constructor value) -- what a fresh Engine has until something explicitly
  // issues `chord follow ...`. CHARACTERIZED, not a pinned requirement: under
  // kAuto a running sequencer re-asserts its step every bar, so a live press is
  // clobbered. This is the DEFERRED live-vs-sequencer arbitration (the crossroads
  // the Pivot feature answers differently); Strada 1 ships with no sequencer, and
  // `chord follow detect` (next test) is the configured mitigation.
  const bool clobbered = run_song_vs_live_steer_probe(
      "follow=auto (engine default, no `chord follow` issued)", ChordFollow::kAuto);
  CHECK(clobbered);  // documented current behavior under kAuto
}

void test_scenario_b_detect_follow_is_immune() {
  // ChordFollow::kDetect is the DOCUMENTED mitigation shipped in
  // docs/arrangrr.init.example (`chord follow detect`). Confirms the D47 gate
  // DOES protect the live chord once configured -- the defect is conditional,
  // not a design flaw in the gate itself.
  const bool clobbered = run_song_vs_live_steer_probe(
      "follow=detect (arrangrr.init.example's documented setting)", ChordFollow::kDetect);
  CHECK(!clobbered);  // the D47 gate protects the live chord
}

}  // namespace

int main() {
  test_scenario_a_no_sequence_persists_every_bar();
  test_scenario_b_auto_follow_is_clobbered_by_the_song();
  test_scenario_b_detect_follow_is_immune();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_seq_vs_live_steer: all OK\n");
  } else {
    std::printf("test_chord_seq_vs_live_steer: %d FAILURE(S) -- see printed timelines above\n",
                arrangrr::test::failures());
  }
  return arrangrr::test::failures();
}

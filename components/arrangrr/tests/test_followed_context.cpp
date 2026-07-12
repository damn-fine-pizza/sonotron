// Unit + regression coverage for the harmony-global-steer refactor:
//
//   (1) FollowedContext — the SINGLE owner of the followed chord (`current`)
//       and the staged next chord (`next`): the explicit latch, the D47 gate,
//       stage/commit_now/commit_bar, and establish_default's no-op-if-explicit.
//   (2) the release-phantom fix (Torquato's measured root cause): releasing a
//       held chord note-by-note must NOT stage/commit a phantom subset chord.
//   (3) the input model — IMMEDIATE by default, SHIFT (detect_quantize) = staged
//       to the next bar.
//   (4) the lifecycle policy — style-load and transport-start KEEP an explicit
//       chord (establish_default is a no-op once explicit).
//
// (1) drives the pure owner; (2)-(4) drive the real Engine ABI (binary commands
// + raw MIDI in), asserting on the observable followed/pending chord.

#include "arrangrr/chord/followed_context.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

constexpr Key kCMajor{.root_pc = 0, .mode = Mode::kMajor};

ChordState chord(std::uint8_t root_pc, ChordQuality quality) {
  return ChordState{.root_pc = root_pc, .quality = quality, .valid = true};
}

// --- (1) FollowedContext owner ----------------------------------------------

void test_commit_now_is_immediate_and_latches_explicit() {
  FollowedContext fc;
  CHECK(!fc.state().valid);
  CHECK(!fc.explicit_set());
  fc.commit_now(Producer::kDetect, chord(9, ChordQuality::kMin));  // A minor now
  CHECK(fc.state().valid);
  CHECK(fc.state().root_pc == 9);
  CHECK(fc.state().quality == ChordQuality::kMin);
  CHECK(!fc.pending().valid);  // immediate: nothing staged
  CHECK(fc.explicit_set());
}

void test_stage_then_commit_bar_is_quantized() {
  FollowedContext fc;
  fc.stage(Producer::kDetect, chord(7, ChordQuality::kDom7));  // G7 staged
  CHECK(!fc.state().valid);                                    // current untouched
  CHECK(fc.pending().valid && fc.pending().root_pc == 7);
  CHECK(fc.explicit_set());  // a stage still latches explicit
  fc.commit_bar();
  CHECK(fc.state().valid && fc.state().root_pc == 7);
  CHECK(!fc.pending().valid);  // pending cleared after the bar
}

void test_commit_bar_with_nothing_staged_is_a_no_op() {
  FollowedContext fc;
  fc.commit_now(Producer::kManual, chord(0, ChordQuality::kMaj));
  fc.commit_bar();  // nothing staged: must not disturb current
  CHECK(fc.state().root_pc == 0);
  CHECK(fc.state().quality == ChordQuality::kMaj);
}

void test_second_stage_before_bar_last_wins() {
  FollowedContext fc;
  fc.stage(Producer::kDetect, chord(9, ChordQuality::kMin));
  fc.stage(Producer::kDetect, chord(2, ChordQuality::kMin7));  // last-wins
  CHECK(fc.pending().root_pc == 2);
  fc.commit_bar();
  CHECK(fc.state().root_pc == 2);
}

void test_d47_gate_blocks_a_non_selected_producer() {
  FollowedContext fc;
  fc.set_follow(ChordFollow::kDetect);
  fc.commit_now(Producer::kSequencer, chord(7, ChordQuality::kDom7));  // gated out
  CHECK(!fc.state().valid);
  CHECK(!fc.explicit_set());                                      // a gated publish does NOT latch
  fc.stage(Producer::kSequencer, chord(7, ChordQuality::kDom7));  // gated out
  CHECK(!fc.pending().valid);
  fc.commit_now(Producer::kDetect, chord(9, ChordQuality::kMin));  // selected: passes
  CHECK(fc.state().valid && fc.state().root_pc == 9);
}

void test_d47_auto_lets_every_producer_through() {
  FollowedContext fc;  // default kAuto
  fc.commit_now(Producer::kDetect, chord(9, ChordQuality::kMin));
  CHECK(fc.state().root_pc == 9);
  fc.commit_now(Producer::kSequencer, chord(7, ChordQuality::kDom7));
  CHECK(fc.state().root_pc == 7);  // last-writer-wins
  fc.commit_now(Producer::kManual, chord(0, ChordQuality::kMaj7));
  CHECK(fc.state().root_pc == 0);
}

void test_establish_default_seeds_the_tonic_when_not_explicit() {
  FollowedContext fc;
  fc.establish_default(kCMajor);
  CHECK(fc.state().valid);
  CHECK(fc.state().root_pc == 0);
  CHECK(fc.state().quality == ChordQuality::kMaj);  // C major tonic triad
  FollowedContext minor;
  minor.establish_default(Key{.root_pc = 9, .mode = Mode::kMinor});  // A minor
  CHECK(minor.state().root_pc == 9);
  CHECK(minor.state().quality == ChordQuality::kMin);
}

void test_establish_default_is_a_no_op_once_explicit() {
  FollowedContext fc;
  fc.commit_now(Producer::kManual, chord(5, ChordQuality::kMaj7));  // FMaj7 explicit
  fc.establish_default(kCMajor);  // must NOT re-home over the explicit chord
  CHECK(fc.state().root_pc == 5);
  CHECK(fc.state().quality == ChordQuality::kMaj7);
}

void test_reset_forgets_the_explicit_chord_and_rehomes() {
  FollowedContext fc;
  fc.commit_now(Producer::kManual, chord(5, ChordQuality::kMaj7));
  fc.stage(Producer::kManual, chord(2, ChordQuality::kMin));
  fc.reset(kCMajor);
  CHECK(fc.state().root_pc == 0);                                 // re-homed
  CHECK(!fc.pending().valid);                                     // staged dropped
  CHECK(!fc.explicit_set());                                      // latch cleared
  fc.establish_default(Key{.root_pc = 7, .mode = Mode::kMajor});  // now takes effect
  CHECK(fc.state().root_pc == 7);
}

// --- Engine harness for (2)-(4) ---------------------------------------------

using Events = StaticVector<OutEvent, 4096>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           std::uint16_t idx = 0) {
    Command command{.op = op, .param = p, .idx = idx, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void key(std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t status = static_cast<std::uint8_t>(vel > 0 ? 0x90 : 0x80);
    const std::uint8_t bytes[3] = {status, note, vel};
    e.push_midi_in(0, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  const ChordState& followed() const { return e.chords().state(); }
  const ChordState& next() const { return e.chords().pending(); }
  void setup() {
    cmd(Param::kStyleLoad, 0);  // "basic"
    cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor), 0, Op::kSet);
    cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kDetect), 0, 0, Op::kSet);
    cmd(Param::kChordDetect, 1, 0, 0, Op::kSet);
  }
};

void hold_F_maj7(Band& b) {
  b.key(65, 100);  // F4
  b.key(69, 100);  // A4
  b.key(72, 100);  // C5
  b.key(76, 100);  // E5
}

// --- (3) immediate default vs shift-quantize --------------------------------

void test_detect_is_immediate_by_default() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  hold_F_maj7(b);  // no shift: commits NOW
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);  // FMaj7 immediately
  CHECK(b.followed().quality == ChordQuality::kMaj7);
  CHECK(!b.next().valid);  // nothing staged
}

void test_detect_shift_stages_to_the_next_bar() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  b.e.set_detect_quantize(true);  // SHIFT held
  hold_F_maj7(b);
  // Staged, not committed: current is still the home key, next is FMaj7.
  CHECK(b.followed().root_pc == 0);  // home key C, unchanged mid-bar
  CHECK(b.next().valid);
  CHECK(b.next().root_pc == 5);
  CHECK(b.next().quality == ChordQuality::kMaj7);
  // Land on the next bar boundary: it commits.
  b.advance(kTicksPerBar - 1);
  CHECK(b.followed().root_pc == 5);
  CHECK(!b.next().valid);
}

// --- (2) release-phantom fix (regression, Torquato's root cause) ------------

void test_releasing_a_held_chord_note_by_note_does_not_phantom() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  hold_F_maj7(b);  // immediate commit -> current == FMaj7
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);

  // Release the four notes ONE AT A TIME. The 3-note plateau (A C E, a valid
  // A-minor triad) must NOT re-harmonize the band: a release never re-recognizes.
  b.key(65, 0);                      // release F -> A C E remain (would name A minor pre-fix)
  CHECK(b.followed().root_pc == 5);  // held: still FMaj7
  CHECK(!b.next().valid);
  b.key(69, 0);  // release A
  b.key(72, 0);  // release C
  b.key(76, 0);  // release E -> nothing held
  CHECK(b.followed().root_pc == 5);
  CHECK(!b.next().valid);

  // Advance a full bar with no new key pressed: no phantom commits.
  b.advance(kTicksPerBar);
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_a_genuinely_new_chord_after_release_still_steers() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  hold_F_maj7(b);
  CHECK(b.followed().root_pc == 5);
  // Fully release, then press a NEW chord (A minor triad): growth re-recognizes.
  b.key(65, 0);
  b.key(69, 0);
  b.key(72, 0);
  b.key(76, 0);
  b.key(69, 100);  // A
  b.key(72, 100);  // C
  b.key(76, 100);  // E -> A minor, a genuinely new chord
  CHECK(b.followed().root_pc == 9);
  CHECK(b.followed().quality == ChordQuality::kMin);
}

// --- (4) lifecycle keeps an explicit chord ----------------------------------

void test_style_load_keeps_the_explicit_chord() {
  Band b;
  b.setup();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  hold_F_maj7(b);  // current := FMaj7 (explicit)
  CHECK(b.followed().root_pc == 5);
  b.cmd(Param::kStyleLoad, 1);  // load a DIFFERENT builtin style
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);  // the harmony persists across the band change
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_transport_start_does_not_clobber_an_explicit_chord() {
  Band b;
  b.setup();
  hold_F_maj7(b);  // steer BEFORE the transport is running
  CHECK(b.followed().root_pc == 5);
  b.cmd(Param::kTransportStart);  // must not re-home to the tonic
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 5);
  CHECK(b.followed().quality == ChordQuality::kMaj7);
}

void test_transport_start_seeds_the_tonic_when_no_explicit_chord() {
  Band b;
  b.setup();  // no chord pressed
  b.cmd(Param::kTransportStart);
  CHECK(b.followed().valid);
  CHECK(b.followed().root_pc == 0);  // home key C established as the default
  CHECK(b.followed().quality == ChordQuality::kMaj);
}

}  // namespace

int main() {
  test_commit_now_is_immediate_and_latches_explicit();
  test_stage_then_commit_bar_is_quantized();
  test_commit_bar_with_nothing_staged_is_a_no_op();
  test_second_stage_before_bar_last_wins();
  test_d47_gate_blocks_a_non_selected_producer();
  test_d47_auto_lets_every_producer_through();
  test_establish_default_seeds_the_tonic_when_not_explicit();
  test_establish_default_is_a_no_op_once_explicit();
  test_reset_forgets_the_explicit_chord_and_rehomes();
  test_detect_is_immediate_by_default();
  test_detect_shift_stages_to_the_next_bar();
  test_releasing_a_held_chord_note_by_note_does_not_phantom();
  test_a_genuinely_new_chord_after_release_still_steers();
  test_style_load_keeps_the_explicit_chord();
  test_transport_start_does_not_clobber_an_explicit_chord();
  test_transport_start_seeds_the_tonic_when_no_explicit_chord();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_followed_context: all OK\n");
  }
  return arrangrr::test::failures();
}

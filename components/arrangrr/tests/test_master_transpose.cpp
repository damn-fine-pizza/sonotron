// Functional tests for global transpose (Phase-6 Theme 3 Item #1,
// docs/reflections/phase6-theme3-master-transpose-scope.md): the
// kMasterTranspose ABI verb's dispatch/validation, its propagation to BOTH
// note-emitting paths (Arranger::resolve() via the band, ChordEngine::
// sound() via a pressed/pad chord), the untouched detected-chord root
// (Decision 3), and the untouched Timeline step-track literal content
// (Decision 5b, owner's "recorded content stays literal" call). resolve()'s
// own unit-level coverage (the kInterval/kScaleDegree/kChordTone branches,
// the kFixed exemption, drop-not-fold) lives in test_arranger.cpp
// (test_master_transpose_resolve) since the subject there is the ONE
// producer (Arranger). This file's subject is the cross-producer ABI wiring
// -> functional, same precedent as test_fx.cpp/test_clip.cpp.

#include "arrangrr/chord/chord_engine.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo) {
    Command command;
    command.op = op;
    command.param = p;
    command.idx = idx;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0, 0,
        Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, 0,
        Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0, 0,
        Op::kSet);
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  int ons(std::uint8_t channel, std::uint8_t note = 255) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind != OutEvent::Kind::kMidi || o.msg.type() != midi::kNoteOn) {
        continue;
      }
      if (o.msg.channel() != channel) {
        continue;
      }
      if (note != 255 && o.msg.d1 != note) {
        continue;
      }
      ++n;
    }
    return n;
  }
  // Every kMidi NoteOn on `channel`, in the order fired, as raw MIDI notes.
  StaticVector<std::uint8_t, 16> note_ons(std::uint8_t channel) const {
    StaticVector<std::uint8_t, 16> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == channel) {
        CHECK(out.push_back(o.msg.d1));
      }
    }
    return out;
  }
};

// ---- kMasterTranspose dispatch/validation ----------------------------------

void test_master_transpose_zero_is_a_no_op() {
  Band b;
  b.cmd(Param::kMasterTranspose, 0, 0, 0, 0, Op::kSet);
  CHECK(b.warns() == 0);
  CHECK(b.e.arranger().master_transpose() == 0);
  CHECK(b.e.chords().master_transpose() == 0);
}

void test_master_transpose_accepts_boundary_values() {
  Band b;
  b.cmd(Param::kMasterTranspose, 12, 0, 0, 0, Op::kSet);
  CHECK(b.warns() == 0);
  CHECK(b.e.arranger().master_transpose() == 12);
  CHECK(b.e.chords().master_transpose() == 12);

  b.cmd(Param::kMasterTranspose, -12, 0, 0, 0, Op::kSet);
  CHECK(b.warns() == 0);
  CHECK(b.e.arranger().master_transpose() == -12);
  CHECK(b.e.chords().master_transpose() == -12);
}

void test_master_transpose_rejects_out_of_range() {
  Band b;
  b.cmd(Param::kMasterTranspose, 13, 0, 0, 0, Op::kSet);
  CHECK(b.warns() == 1);
  CHECK(b.e.arranger().master_transpose() == 0);  // rejected: unchanged
  CHECK(b.e.chords().master_transpose() == 0);

  b.cmd(Param::kMasterTranspose, -13, 0, 0, 0, Op::kSet);
  CHECK(b.warns() == 2);
  CHECK(b.e.arranger().master_transpose() == 0);
  CHECK(b.e.chords().master_transpose() == 0);
}

// ---- Arranger path (the band, through resolve()) ---------------------------

void test_master_transpose_shifts_the_band() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // Cmaj7 in C major (I)
  b.cmd(Param::kMasterTranspose, 5, 0, 0, 0, Op::kSet);
  b.ev.clear();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar / 2 - 1);
  // VarA bass root at step 0 without transpose is 36 (C2); with +5 it is 41.
  CHECK(b.ons(1, 41) == 1);
  CHECK(b.ons(1, 36) == 0);  // the untransposed note must NOT also appear
}

// ---- ChordEngine path (a pressed/pad chord, through sound()) --------------

void test_master_transpose_shifts_the_played_chord() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kMasterTranspose, -2, 0, 0, 0, Op::kSet);
  b.ev.clear();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // Cmaj7 in C major, out_port/channel default 0
  // Untransposed Cmaj7 stack anchored at 60: 60 64 67 71. With -2: 58 62 65 69.
  const StaticVector<std::uint8_t, 16> notes = b.note_ons(0);
  CHECK(notes.size() == 4);
  bool saw_58 = false;
  bool saw_60 = false;
  for (std::uint8_t n : notes) {
    if (n == 58) {
      saw_58 = true;
    }
    if (n == 60) {
      saw_60 = true;
    }
  }
  CHECK(saw_58);
  CHECK(!saw_60);  // the untransposed root must NOT also sound
}

// The followed/detected chord root is untouched by construction (Decision 3):
// steer_detect()'s publish is a pure pitch-class write, never reached by
// master_transpose at all. `chord play`'s own steer publishes root_pc from
// the ORIGINAL root_note (pre-transpose), matching the same invariant.
void test_master_transpose_leaves_the_followed_chord_root_untouched() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kMasterTranspose, 7, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, -1, 100);  // root_pc 0 = C
  CHECK(b.e.chords().state().valid);
  CHECK(b.e.chords().state().root_pc == 0);  // NOT shifted by the transpose
}

// ---- ChordEngine path: drop-not-fold at both [0,127] boundaries -----------
// (Torquato QA hardening: the existing test above only exercises a
// within-range shift; these force the transpose to push individual chord
// tones OUT of range on EACH side, proving sound()'s "if (n < 0 || n > 127)
// continue" (chord_engine.hpp) really DROPS the offending tone instead of
// folding it back in range -- and that the surviving tones are still
// correctly transposed, not silently clamped to the edge.)

void test_master_transpose_chord_engine_drops_above_127_not_fold() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kMasterTranspose, 12, 0, 0, 0, Op::kSet);
  b.ev.clear();
  // Root D (110, diatonic degree in C major), forced plain major triad
  // (quality override 0 = kMaj, offsets {0,4,7}): untransposed tones would be
  // {110, 114, 117}; +12 pushes them to {122, 126, 129} -- the last one past
  // 127.
  b.cmd(Param::kChordPlay, 110, static_cast<std::int32_t>(ChordQuality::kMaj), 100);
  const StaticVector<std::uint8_t, 16> notes = b.note_ons(0);
  CHECK(notes.size() == 2);  // the third tone (129) was DROPPED, not folded
  bool saw_122 = false;
  bool saw_126 = false;
  bool saw_out_of_range = false;
  for (std::uint8_t n : notes) {
    if (n == 122) {
      saw_122 = true;
    }
    if (n == 126) {
      saw_126 = true;
    }
    if (n > 127) {
      saw_out_of_range = true;  // impossible on the wire (uint8_t note field), defensive
    }
  }
  CHECK(saw_122);
  CHECK(saw_126);
  CHECK(!saw_out_of_range);
}

void test_master_transpose_chord_engine_drops_below_0_not_fold() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kMasterTranspose, -12, 0, 0, 0, Op::kSet);
  b.ev.clear();
  // Root A (9, diatonic in C major), forced plain major triad: untransposed
  // tones would be {9, 13, 16}; -12 pushes them to {-3, 1, 4} -- the first
  // one below 0.
  b.cmd(Param::kChordPlay, 9, static_cast<std::int32_t>(ChordQuality::kMaj), 100);
  const StaticVector<std::uint8_t, 16> notes = b.note_ons(0);
  CHECK(notes.size() == 2);  // the first tone (-3) was DROPPED, not folded to 0
  bool saw_1 = false;
  bool saw_4 = false;
  bool saw_0 = false;  // a fold-to-0 bug would surface here
  for (std::uint8_t n : notes) {
    if (n == 1) {
      saw_1 = true;
    }
    if (n == 4) {
      saw_4 = true;
    }
    if (n == 0) {
      saw_0 = true;
    }
  }
  CHECK(saw_1);
  CHECK(saw_4);
  CHECK(!saw_0);
}

// ---- Drum/perc exemption, end to end through the WHOLE Engine pipeline ----
// (Torquato QA hardening: test_arranger.cpp's test_master_transpose_resolve
// already proves resolve()'s own kFixed short-circuit in isolation; this
// proves the same invariant survives the full fire_arranger -> schedule_or_
// warn dispatch a real session actually runs, with a LARGE transpose active
// on BOTH sides of zero.)

StaticVector<std::uint8_t, 64> drum_note_sequence(Band& b, std::int32_t transpose) {
  b.setup_basic();
  if (transpose != 0) {
    b.cmd(Param::kMasterTranspose, transpose, 0, 0, 0, Op::kSet);
  }
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.ev.clear();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar / 2);  // half a bar: enough drum hits, well under the capture cap
  StaticVector<std::uint8_t, 64> out;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn && o.msg.channel() == 9) {
      CHECK(out.push_back(o.msg.d1));
    }
  }
  return out;
}

void test_master_transpose_drum_notes_unchanged_end_to_end() {
  Band baseline;
  const StaticVector<std::uint8_t, 64> baseline_drums = drum_note_sequence(baseline, 0);
  CHECK(baseline_drums.size() > 0);

  Band pos;
  const StaticVector<std::uint8_t, 64> pos_drums = drum_note_sequence(pos, 12);
  CHECK(pos_drums.size() == baseline_drums.size());
  for (std::size_t i = 0; i < baseline_drums.size(); ++i) {
    CHECK(pos_drums[i] == baseline_drums[i]);
  }

  Band neg;
  const StaticVector<std::uint8_t, 64> neg_drums = drum_note_sequence(neg, -12);
  CHECK(neg_drums.size() == baseline_drums.size());
  for (std::size_t i = 0; i < baseline_drums.size(); ++i) {
    CHECK(neg_drums[i] == baseline_drums[i]);
  }
}

// ---- 0 is a true no-op, even after drifting away and back ------------------

void test_master_transpose_zero_after_drift_is_a_true_no_op() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.ev.clear();
  b.cmd(Param::kMasterTranspose, 9, 0, 0, 0, Op::kSet);
  b.cmd(Param::kMasterTranspose, 0, 0, 0, 0, Op::kSet);  // drift away, then back to 0
  CHECK(b.e.arranger().master_transpose() == 0);
  CHECK(b.e.chords().master_transpose() == 0);
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar / 2 - 1);
  // Bass root at step 0 without transpose is 36 (C2): identical to the
  // never-touched-transpose baseline in test_master_transpose_shifts_the_band.
  CHECK(b.ons(1, 36) == 1);
}

// ---- Timeline step-track literal content is NOT transposed (Decision 5b) --

void test_master_transpose_does_not_shift_step_track_notes() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTrackNew, static_cast<std::int32_t>(TrackRole::kLead), 0 | (3 << 8));
  const std::size_t track_idx = b.e.timeline().track_count() - 1;
  // idx = track; a = step 0; b = note 60 | (vel 100 << 8); c = gate.
  b.cmd(Param::kTrackStep, 0, 60 | (100 << 8), 120, static_cast<std::uint16_t>(track_idx), Op::kDo);
  b.cmd(Param::kMasterTranspose, 9, 0, 0, 0, Op::kSet);
  b.ev.clear();
  b.cmd(Param::kTransportStart);
  b.advance(1);
  // The recorded literal note (60) sounds unchanged; the transposed value
  // (69) must never appear on this channel.
  CHECK(b.ons(3, 60) == 1);
  CHECK(b.ons(3, 69) == 0);
}

}  // namespace

int main() {
  test_master_transpose_zero_is_a_no_op();
  test_master_transpose_accepts_boundary_values();
  test_master_transpose_rejects_out_of_range();
  test_master_transpose_shifts_the_band();
  test_master_transpose_shifts_the_played_chord();
  test_master_transpose_chord_engine_drops_above_127_not_fold();
  test_master_transpose_chord_engine_drops_below_0_not_fold();
  test_master_transpose_drum_notes_unchanged_end_to_end();
  test_master_transpose_zero_after_drift_is_a_true_no_op();
  test_master_transpose_leaves_the_followed_chord_root_untouched();
  test_master_transpose_does_not_shift_step_track_notes();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_master_transpose: all OK\n");
  }
  return arrangrr::test::failures();
}

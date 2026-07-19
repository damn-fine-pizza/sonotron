#include "arrangrr/chord/chord_sequence.hpp"
#include "arrangrr/chord/chord_sequencer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

void test_sequence_free_durations_and_edit() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  CHECK(s.append(1, -1, 100, 2 * kTicksPerBar));  // ii for 2 bars
  CHECK(s.append(4, -1, 100, kTicksPerBar));      // V for 1 bar
  CHECK(s.append(0, -1, 100, kTicksPerBar));      // I
  CHECK(s.count() == 3);
  CHECK(s.length() == 4 * kTicksPerBar);
  CHECK(s.step(1).start == 2 * kTicksPerBar);
  CHECK(!s.append(0, -1, 100, 0));  // zero duration refused
  // Removing the first step closes the gap.
  CHECK(s.remove(0));
  CHECK(s.count() == 2 && s.step(0).start == 0 && s.length() == 2 * kTicksPerBar);
  CHECK(!s.remove(9));
  s.clear();
  CHECK(s.count() == 0 && s.length() == 0);
}

void test_sequence_quantize_after() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  // Sloppy live starts near 0, 1, 2 bars.
  CHECK(s.record(ChordStep{130, 0, 1, -1, 100}));
  CHECK(s.record(ChordStep{kTicksPerBar - 90, 0, 4, -1, 100}));
  CHECK(s.record(ChordStep{2 * kTicksPerBar + 200, 3 * kTicksPerBar / 2, 0, -1, 100}));
  s.quantize();
  CHECK(s.step(0).start == 0);
  CHECK(s.step(1).start == kTicksPerBar);
  CHECK(s.step(2).start == 2 * kTicksPerBar);
  CHECK(s.step(0).duration == kTicksPerBar);
  CHECK(s.step(1).duration == kTicksPerBar);
  CHECK(s.step(2).duration == 2 * kTicksPerBar);  // 1.5 bars rounds up to 2
}

void test_sequence_quantize_edge_cases() {
  // grid == 0 is a no-op (guard branch), starts untouched.
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  CHECK(s.record(ChordStep{50, 0, 0, -1, 100}));
  s.quantize(0);
  CHECK(s.step(0).start == 50);  // unchanged
  // Empty sequence quantize is also a no-op (no crash).
  ChordSequence empty;
  empty.quantize();
  CHECK(empty.count() == 0);
  // Two near-simultaneous starts snap to the SAME bar: the second is clamped to
  // keep order (snapped < previous_end), and the zero gap falls back to grid.
  ChordSequence t;
  t.key = Key{0, Mode::kMajor};
  CHECK(t.record(ChordStep{10, 0, 0, -1, 100}));  // -> bar 0
  CHECK(t.record(ChordStep{60, 0, 4, -1, 100}));  // also -> bar 0, clamped after
  t.quantize();
  CHECK(t.step(0).start == 0);
  CHECK(t.step(1).start == kTicksPerBar);     // pushed to keep order
  CHECK(t.step(0).duration == kTicksPerBar);  // gap fell back to a full bar
}

void test_transpose_to_valid_mode() {
  // transpose_to with a VALID mode index changes both root and mode (the
  // mode>=0 && mode<kModeCount branch).
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  s.transpose_to(9, static_cast<std::int8_t>(Mode::kMinor));
  CHECK(s.key.root_pc == 9 && s.key.mode == Mode::kMinor);
  // An out-of-range mode leaves the mode untouched (only the root moves).
  s.transpose_to(2, 99);
  CHECK(s.key.root_pc == 2 && s.key.mode == Mode::kMinor);
}

void test_transpose_re_derives() {
  ChordSequence s;
  s.key = Key{0, Mode::kMajor};
  CHECK(s.append(1, -1, 100, kTicksPerBar));
  s.transpose_to(7, -1);  // to G major, degrees untouched
  CHECK(s.key.root_pc == 7 && s.key.mode == Mode::kMajor);
  CHECK(s.step(0).degree == 1);
  s.transpose_by(2);
  CHECK(s.key.root_pc == 9);
  s.transpose_by(-12);
  CHECK(s.key.root_pc == 9);
}

// P0-2: kBeat now fires once per 24-PPQN pulse while playing (96/bar), so a
// multi-bar advance() accumulates far more raw events than before -- bumped
// from 256 to give headroom (the longest single fixture here spans ~4 bars).
using Events = StaticVector<OutEvent, 1024>;

struct SeqFixture {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           std::uint16_t idx = 0) {
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
  void add(std::uint8_t note, Tick dur, std::int8_t quality = -1, std::uint8_t vel = 100) {
    cmd(Param::kSeqAdd, note, (quality + 1) | (vel << 8), static_cast<std::int32_t>(dur));
  }
  StaticVector<std::uint16_t, 32> chords() const {
    StaticVector<std::uint16_t, 32> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChord) {
        CHECK(out.push_back(o.code));
      }
    }
    return out;
  }
  static std::uint8_t degree(std::uint16_t code) { return static_cast<std::uint8_t>(code & 0xFF); }
  static ChordQuality quality(std::uint16_t code) { return static_cast<ChordQuality>(code >> 8); }
};

void test_progression_playback_and_loop() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);  // D -> ii
  f.add(67, kTicksPerBar);  // G -> V
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(2 * kTicksPerBar);  // full cycle + wrap into step 0 again
  const auto ch = f.chords();
  CHECK(ch.size() == 3);
  CHECK(SeqFixture::degree(ch[0]) == 1 && SeqFixture::quality(ch[0]) == ChordQuality::kMin7);
  CHECK(SeqFixture::degree(ch[1]) == 4 && SeqFixture::quality(ch[1]) == ChordQuality::kDom7);
  CHECK(SeqFixture::degree(ch[2]) == 1);  // loop wrapped
}

void test_no_loop_stops_and_releases() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(60, kTicksPerBar);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(kTicksPerBar + 10);
  CHECK(!f.e.sequences().playing());
  // The voicing was released at the sequence end.
  int ons = 0, offs = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind != OutEvent::Kind::kMidi) {
      continue;
    }
    if (o.msg.type() == midi::kNoteOn) {
      ++ons;
    }
    if (o.msg.type() == midi::kNoteOff) {
      ++offs;
    }
  }
  CHECK(ons == 4 && offs == 4);
}

void test_transpose_to_g_replays_rederived() {
  // The WOW: | ii V | in C plays Dm7 G7; transposed to G it plays Am7 D7.
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);
  f.add(67, kTicksPerBar);
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(2 * kTicksPerBar - 1);
  f.cmd(Param::kSeqTranspose, 7, -1, 0, Op::kSet);  // to G major
  f.advance(2 * kTicksPerBar);
  // Roots: Dm7(62..) G7 | Am7 D7 — check the note-on roots per chord event order.
  StaticVector<std::uint8_t, 16> roots;
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kChord) {
      CHECK(roots.push_back(o.msg.status));
    }
  }
  CHECK(roots.size() == 4);
  CHECK(roots[0] == 62 && roots[1] == 67);  // C major: D, G
  CHECK(roots[2] == 69 && roots[3] == 62);  // G major: ii=A, V=D
}

void test_record_quantize_playback() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqRec);
  // Two live chords, sloppy timing (~1 bar apart), captured while sounding.
  f.cmd(Param::kChordPlay, 62, -1, 100);
  f.advance(kTicksPerBar + 70);
  f.cmd(Param::kChordPlay, 67, -1, 100);
  f.advance(kTicksPerBar - 40);
  f.cmd(Param::kSeqStop);  // quantize-after to whole bars
  const ChordSequence* seq = f.e.sequences().current();
  CHECK(seq->count() == 2);
  CHECK(seq->step(0).start == 0 && seq->step(0).duration == kTicksPerBar);
  CHECK(seq->step(1).start == kTicksPerBar && seq->step(1).duration == kTicksPerBar);
  CHECK(seq->step(0).degree == 1 && seq->step(1).degree == 4);
}

void test_seq_more_engine_paths() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  // Bad seq-add arguments.
  f.cmd(Param::kSeqAdd, 60, (0) | (100 << 8), 0);     // zero duration
  f.cmd(Param::kSeqAdd, -1, (0) | (100 << 8), 100);   // bad note
  f.cmd(Param::kSeqAdd, 60, (0) | (0 << 8), 100);     // zero velocity
  f.cmd(Param::kSeqAdd, 60, (99) | (100 << 8), 100);  // bogus quality
  f.cmd(Param::kSeqDel, 5);                           // no such step
  int warns = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kWarn) {
      ++warns;
    }
  }
  CHECK(warns == 5);
  f.ev.clear();
  // Transpose bad args.
  f.cmd(Param::kSeqTranspose, 15, 0, 0, Op::kSet);
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
  f.ev.clear();
  // Rec refused while playing; stop with explicit grid; loop toggle.
  f.add(62, kTicksPerBar);
  f.cmd(Param::kSeqLoop, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kSeqRec);  // playing -> refused
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqEmpty));
  f.ev.clear();
  f.cmd(Param::kSeqStop);  // stop playback
  CHECK(!f.e.sequences().playing());
  f.cmd(Param::kSeqRec);
  f.ev.clear();
  f.cmd(Param::kSeqPlay);  // playback refused while recording (symmetric)
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqEmpty));
  f.ev.clear();
  f.cmd(Param::kChordPlay, 64, -1, 90);
  f.advance(100);
  f.cmd(Param::kSeqStop, kTicksPerBeat);  // explicit finer grid
  CHECK(f.e.sequences().current()->count() == 1);
  CHECK(f.e.sequences().current()->step(0).duration == kTicksPerBeat);
  // Transport stop releases the sequencer voicing.
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.ev.clear();
  f.cmd(Param::kTransportStop);
  int offs = 0;
  for (const OutEvent& o : f.ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOff) {
      ++offs;
    }
  }
  CHECK(offs == 4);
}

void test_armed_empty_sequence() {
  // The demo -i workflow: arm an empty looping sequence, add chords later,
  // then start the transport — the progression must sound from tick 0.
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.ev.clear();  // Phase 3a (§17.3b): drop kKeySet's own kParamState echo
  f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqPlay);  // empty: legal, plays silence
  CHECK(f.ev.empty());     // no warn
  CHECK(f.e.sequences().playing());
  f.cmd(Param::kTransportStart);
  f.advance(10);  // still silent, and no crash on the empty length
  CHECK(f.chords().size() == 0);
  f.cmd(Param::kTransportStop);
  f.add(60, kTicksPerBar);  // now write the progression
  f.add(65, kTicksPerBar);
  f.ev.clear();
  f.cmd(Param::kTransportStart);
  f.advance(kTicksPerBar);
  const auto ch = f.chords();
  CHECK(ch.size() == 2);  // Cmaj7 at 0, Fmaj7 at bar 2 start
}

// Engineering spike (docs/proposals/gui-live-harmony-musical-design.md
// S3.4 Option B): does the EXISTING kSeqUse dispatch support a clean
// A -> B -> A cadence handoff for a swapped-in cadential sequence?
//
// ChordSequencer::m_base and m_playing are SINGLE fields shared by the
// WHOLE pool, not per-slot state (chord_sequencer.hpp:157-163). use()
// (kSeqUse) only moves m_current (chord_sequencer.hpp:34-40) -- it never
// touches m_base. So a BARE kSeqUse switch (Option B's own literal
// wording, "kSeqUse's back to the main progression's slot") does NOT give
// the newly-selected slot a fresh phase-0, and does NOT preserve a
// previously-selected slot's own phase as dedicated per-slot state either
// -- it just leaves m_base wherever the last play() call put it. This
// probe proves the practical consequence for Option B: a bare switch to a
// short, non-looping cadential slot goes silent forever (on_tick's
// non-loop "pos > len" branch neither fires a step nor calls
// stop_playback -- a second, latent bug this probe surfaces), and that
// pairing kSeqUse with kSeqPlay -- the idiom Engine::apply_performance
// and Engine::apply_clip_content already use for every OTHER live
// chord-sequence switch (engine.cpp's `m_seq.use(idx) && m_seq.play(tick)`
// pattern) -- is what actually produces a clean, bar-aligned handoff.
void test_seq_use_bare_switch_leaves_target_out_of_phase() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);

  // Slot 0 (A): the "main progression", a 2-bar loop.
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);  // ii
  f.add(67, kTicksPerBar);  // V
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);

  // Slot 1 (B): a short, non-looping cadential tag (V -> I), Option B's
  // own shape ("loop=false so it holds I after resolving").
  f.cmd(Param::kSeqNew);
  f.add(67, kTicksPerBar);  // V
  f.add(60, kTicksPerBar);  // I
  // loop defaults to false (ChordSequence::loop) -- exactly what B wants.

  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 0);  // back to A
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(3 * kTicksPerBar);  // 1.5 loops into A: mid the V step, arbitrary phase
  f.ev.clear();

  // Ending entry, done as Option B's own text literally describes it: a
  // BARE kSeqUse, no accompanying kSeqPlay.
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 1);
  f.advance(4 * kTicksPerBar);  // ample ticks for B's 2-bar content to fire, if it could
  CHECK(f.chords().empty());    // B never sounds: pos is already far past its length
  // Hardened on_tick (pos >= len, not pos == len) stops playback the instant
  // the stale phase is discovered, instead of hanging silently forever --
  // this is the pos>len defensive fix (chord_sequencer.hpp), a separate,
  // independent fix from Option B's own plumbing (paired kSeqUse+kSeqPlay,
  // proven below). It does NOT give B a per-slot phase; it only turns "stuck
  // silent forever" into "stopped", which is strictly safer either way.
  CHECK(!f.e.sequences().playing());

  // "Next fresh VarA/style load": swap back to A, again bare per Option B's
  // literal wording -- but the sequencer is stopped now, and a bare use()
  // never calls play(), so nothing re-arms playback either.
  f.ev.clear();
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 0);
  f.advance(2 * kTicksPerBar);
  CHECK(f.chords().empty());  // still silent: use() alone never resumes playback
}

// Focused, minimal regression for the pos>len hardening on its own (the
// A/B narrative above already exercises it, but incidentally; this pins the
// exact defect described: on_tick's non-loop branch used to check
// `pos == len`, so a phase that OVERSHOOTS the end by more than one tick
// -- not just lands on it -- silently stalled instead of stopping).
void test_on_tick_pos_greater_than_length_stops_playback() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(60, 3 * kTicksPerBar);  // a long, non-looping sequence
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(2 * kTicksPerBar);  // well inside its length, still playing
  CHECK(f.e.sequences().playing());

  // A new, much SHORTER, non-looping sequence, selected bare (no kSeqPlay):
  // m_base is untouched, so pos is already ~2 bars past this sequence's own
  // 1-bar length the moment the very next tick is evaluated -- pos > len,
  // not pos == len.
  f.cmd(Param::kSeqNew);
  f.add(60, kTicksPerBar);
  f.advance(1);
  CHECK(!f.e.sequences().playing());  // hardened: stops rather than hangs
}

// Same A/B setup, but each switch is paired with kSeqPlay -- already the
// production idiom in Engine::apply_performance (engine.cpp:1863-1865) and
// Engine::apply_clip_content (engine.cpp:1237-1239). Shows the fix for
// Option B is "always rebase on switch", not a new sequencer mechanism.
void test_seq_use_paired_with_play_gives_clean_bar_aligned_handoff() {
  SeqFixture f;
  f.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(62, kTicksPerBar);  // ii
  f.add(67, kTicksPerBar);  // V
  f.cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
  f.cmd(Param::kSeqNew);
  f.add(67, kTicksPerBar);  // V
  f.add(60, kTicksPerBar);  // I

  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 0);
  f.cmd(Param::kSeqPlay);
  f.cmd(Param::kTransportStart);
  f.advance(3 * kTicksPerBar);  // mid A's V step, same arbitrary phase as above
  f.ev.clear();

  // Ending entry: swap to B AND rebase (kSeqUse + kSeqPlay).
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 1);
  f.cmd(Param::kSeqPlay);
  f.advance(2 * kTicksPerBar - 1);  // through B's own two bars, short of its wrap point
  const auto b_chords = f.chords();
  CHECK(b_chords.size() == 2);
  CHECK(SeqFixture::degree(b_chords[0]) == 4);  // V, fired at pos 0 by kSeqPlay itself
  CHECK(SeqFixture::degree(b_chords[1]) == 0);  // I -- lands cleanly on the tonic
  f.advance(2);  // cross B's own length: non-looping, so it stops itself
  CHECK(!f.e.sequences().playing());
  f.ev.clear();

  // "Next fresh VarA/style load": swap back to A, rebased again.
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 0);
  f.cmd(Param::kSeqPlay);
  f.advance(2 * kTicksPerBar - 1);
  const auto a_chords = f.chords();
  CHECK(a_chords.size() == 2);
  CHECK(SeqFixture::degree(a_chords[0]) == 1);  // A resumes at ITS OWN step 0 (ii)
  CHECK(SeqFixture::degree(a_chords[1]) == 4);  // then V -- bar-aligned to the switch tick
}

void test_seq_warns() {
  SeqFixture f;
  f.cmd(Param::kSeqPlay);  // no sequence exists at all
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqEmpty));
  f.ev.clear();
  f.cmd(Param::kSeqAdd, 60, 1 | (100 << 8), 100);  // no sequence yet
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
  f.ev.clear();
  f.cmd(Param::kSeqNew);
  f.cmd(Param::kSeqAdd, 61, 0 | (100 << 8), kTicksPerBar);  // C# chromatic
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kNotInKey));
  f.ev.clear();
  for (std::size_t i = 1; i < kMaxChordSequences; ++i) {
    f.cmd(Param::kSeqNew);
  }
  f.cmd(Param::kSeqNew);
  CHECK(f.ev.size() == 1 && f.ev[0].code == static_cast<std::uint16_t>(WarnCode::kSeqTableFull));
  f.ev.clear();
  f.cmd(Param::kSeqUse, 0, 0, 0, Op::kDo, 99);
  CHECK(f.ev.size() == 1 && f.ev[0].kind == OutEvent::Kind::kWarn);
}

}  // namespace

int main() {
  test_sequence_free_durations_and_edit();
  test_sequence_quantize_after();
  test_sequence_quantize_edge_cases();
  test_transpose_to_valid_mode();
  test_transpose_re_derives();
  test_progression_playback_and_loop();
  test_no_loop_stops_and_releases();
  test_transpose_to_g_replays_rederived();
  test_record_quantize_playback();
  test_seq_more_engine_paths();
  test_armed_empty_sequence();
  test_seq_use_bare_switch_leaves_target_out_of_phase();
  test_on_tick_pos_greater_than_length_stops_playback();
  test_seq_use_paired_with_play_gives_clean_bar_aligned_handoff();
  test_seq_warns();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_chord_seq: all OK\n");
  }
  return arrangrr::test::failures();
}

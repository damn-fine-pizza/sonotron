// A/B FIX PIN (docs/reflections/cli-vs-gui-ab-2026-07.md): the GUI's
// integrated engine path (InProcessBrainSession) never sent `key`/`seq`
// commands, so every style sat on a static tonic triad forever -- PATH-B
// captures in that reflection showed 0 "chord" events and pitch-classes
// pinned at {0,4,7} (a bare tonic triad) for the whole session, no matter
// which style was loaded. The fix being wired in parallel (in_process_brain_
// session.cpp) makes InProcessBrainSession auto-inject a default harmonic
// progression (via the core's existing kKeySet/kSeqNew/kSeqAdd/kSeqLoop/
// kSeqPlay Command verbs) right after a successful `style load`/`style
// switch`, exactly mirroring the already-shipped kDefaultStyleRoutes
// auto-routing idiom in that same file.
//
// This test drives the REAL production path -- a real InProcessBrainSession
// (the same class main.cpp uses), sending ONLY GUI-reachable command lines
// (`style load <name>`, `transport start`) through the real command_line_to_
// command() translator -- NEVER a hand-written `seq`/`key` line. The whole
// point is proving the GUI's own reachable path now produces harmonic
// movement, not that seq/key work in isolation (core unit tests already
// cover that in isolation). It then polls the real OutEvent stream and
// asserts BOTH that "chord" events now appear at all (were completely
// absent before this fix) and that the harmony actually MOVES over time
// (more than one distinct chord, more than 3 distinct pitch classes) rather
// than being pinned to a single static tonic triad -- the exact symptom
// that reflection measured.
//
// TORQUATO QA PASS (2026-07-17, closing the regression-coverage gap on
// commit 7f07a6f / issue #29): the above only pinned 2 of the 16 built-in
// styles (bossa, blues). This pass extends the SAME harness -- unchanged --
// to all 16 entries of arrangrr::styles::kBuiltins (style.hpp) /
// sonotron::kDefaultProgressions (default_style_progressions.hpp), using
// ROBUST invariants rather than exact event counts (Wave-2.5, landing next,
// changes per-style RHYTHM/groove only, never this harmonic-progression
// table -- a test pinned to exact counts would be needlessly fragile to
// that unrelated change):
//   - chord_event_count > 0           (harmony fires at all)
//   - distinct_chord_labels.size()>1  (harmony actually MOVES, not static)
//   - popcount(pitch_class_union)>3   (escapes the bare {0,4,7} tonic triad)
// ONE deliberate, documented exception: funk (kBuiltins index 4) ships a
// single-step "I7 vamp" progression (default_style_progressions.hpp's own
// comment: "static I7 vamp, one chord for the whole loop") -- an
// intentional, genre-idiomatic design baked into the progression TABLE
// itself, not a symptom of the #29 regression (whose signature was exactly
// 0 chord events and a popcount-3 bare triad). test_funk_default_
// progression_vamp_holds() below asserts the invariants that DO apply to a
// one-chord vamp (repeats, richer-than-a-triad harmony) and explicitly does
// NOT assert distinct_chord_labels.size()>1 for that one style -- see its
// own comment.
//
// Per-style wait windows are sized off each style's own tempo (BpmX100,
// style.hpp) and its default progression's first-step bar length
// (default_style_progressions.hpp): seconds_per_bar = 240 / (tempo_x100 /
// 100) (kBeatsPerBar == 4, common/time.hpp), window = (first_step_bars + 4
// bars of margin) * seconds_per_bar, rounded up -- the same generous margin
// bossa's own pre-existing 9000 ms window already banks on (1-bar first
// step at 130 BPM == 1.846 s/bar, ~4.9 bars of window).

#include "src/in_process_brain_session.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

struct ProgressionObservation {
  int chord_event_count = 0;
  std::set<std::string> distinct_chord_labels;  // BrainEvent::chord_out values seen
  std::uint16_t pitch_class_union = 0;          // OR of every kChordFollowed followed_current_pcs
                                                // seen from the "sequencer" producer
};

// Drives `style load <style_name>` then `transport start` through the REAL
// InProcessBrainSession::send() translator (the same one browser_panel.cpp's
// GUI code calls), then polls for up to `wait` of REAL wall-clock time,
// collecting every "chord" (BrainEvent::Kind::kChord) and "chord-followed"
// (BrainEvent::Kind::kChordFollowed) event emitted -- exactly the two OutEvent
// kinds the core's ChordSequencer fires on every step (arrangrr/engine.hpp's
// fire_chord_seq: it emits BOTH `OutEvent::chord(...)` and
// `emit_chord_followed(Producer::kSequencer, ...)` on every fired step, so
// kChordFollowed's numeric `followed_current_pcs` bitmask -- bit0=C..bit11=B
// -- is a robust, string-parsing-free way to measure pitch-class movement).
ProgressionObservation observe_default_progression(const std::string& style_name,
                                                   std::chrono::milliseconds wait) {
  InProcessBrainSession session;
  CHECK(session.start());

  session.send("style load " + style_name);
  session.send("transport start");

  ProgressionObservation obs;
  const auto deadline = std::chrono::steady_clock::now() + wait;
  std::vector<BrainEvent> batch;
  while (std::chrono::steady_clock::now() < deadline) {
    batch.clear();
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      if (ev.kind == BrainEvent::Kind::kChord) {
        ++obs.chord_event_count;
        obs.distinct_chord_labels.insert(ev.chord_out);
      } else if (ev.kind == BrainEvent::Kind::kChordFollowed && ev.followed_source == "sequencer") {
        obs.pitch_class_union |= static_cast<std::uint16_t>(ev.followed_current_pcs);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  session.stop();
  return obs;
}

int popcount16(std::uint16_t v) {
  int n = 0;
  while (v != 0) {
    n += static_cast<int>(v & 1U);
    v >>= 1U;
  }
  return n;
}

// bossa: a fast enough default-tempo style that a 9 real-second window should
// comfortably cross more than one chord in the default progression.
void test_bossa_default_progression_moves() {
  const ProgressionObservation obs =
      observe_default_progression("bossa", std::chrono::milliseconds(9000));

  CHECK(obs.chord_event_count > 0);  // was exactly 0 before this fix (the measured A/B bug)
  CHECK(obs.distinct_chord_labels.size() > 1);   // more than one chord label seen: harmony moves
  CHECK(popcount16(obs.pitch_class_union) > 3);  // more than the static tonic triad's 3 notes

  std::fprintf(stderr,
               "[test_default_style_progression_functional] style=bossa "
               "chord_event_count=%d distinct_chord_labels=%zu pitch_class_popcount=%d\n",
               obs.chord_event_count, obs.distinct_chord_labels.size(),
               popcount16(obs.pitch_class_union));
}

// blues: a slow 66 BPM 12-bar loop -- this window only reaches its first
// chord change, C7->F7 around bar 5, not the full 12 bars -- that is fine,
// we only need to prove movement, not a full cycle.
void test_blues_default_progression_moves() {
  const ProgressionObservation obs =
      observe_default_progression("blues", std::chrono::milliseconds(20000));

  CHECK(obs.chord_event_count > 0);
  CHECK(obs.distinct_chord_labels.size() > 1);
  CHECK(popcount16(obs.pitch_class_union) > 3);

  std::fprintf(stderr,
               "[test_default_style_progression_functional] style=blues "
               "chord_event_count=%d distinct_chord_labels=%zu pitch_class_popcount=%d\n",
               obs.chord_event_count, obs.distinct_chord_labels.size(),
               popcount16(obs.pitch_class_union));
}

// Shared assertion body for every "moving" style (all 16 built-ins except
// funk, see the file header comment): drives `style_name` through the same
// real path as test_bossa_default_progression_moves() above and asserts the
// three robust, count-agnostic invariants.
void assert_progression_moves(const std::string& style_name, std::chrono::milliseconds wait) {
  const ProgressionObservation obs = observe_default_progression(style_name, wait);

  CHECK(obs.chord_event_count > 0);              // was exactly 0 before #29's fix
  CHECK(obs.distinct_chord_labels.size() > 1);   // harmony moves, not static
  CHECK(popcount16(obs.pitch_class_union) > 3);  // escapes the bare tonic triad

  std::fprintf(stderr,
               "[test_default_style_progression_functional] style=%s "
               "chord_event_count=%d distinct_chord_labels=%zu pitch_class_popcount=%d\n",
               style_name.c_str(), obs.chord_event_count, obs.distinct_chord_labels.size(),
               popcount16(obs.pitch_class_union));
}

// basic (index 0): I-IV-V-I plain triads, 1 bar/step, 120 BPM (2.0 s/bar).
void test_basic_default_progression_moves() {
  assert_progression_moves("basic", std::chrono::milliseconds(10500));
}

// pop (index 1): I-V-vi-IV plain triads, 1 bar/step, 120 BPM.
void test_pop_default_progression_moves() {
  assert_progression_moves("pop", std::chrono::milliseconds(10500));
}

// rock (index 2): I-bVII-IV-I (G mixolydian) plain triads, 1 bar/step, 130 BPM.
void test_rock_default_progression_moves() {
  assert_progression_moves("rock", std::chrono::milliseconds(9500));
}

// ballad (index 3): I-vi-ii-V smart sevenths, 2 bars/step (slowest step
// length in the table bar-for-bar besides funk), 72 BPM (3.333 s/bar) --
// needs the longest window of the "moving" set.
void test_ballad_default_progression_moves() {
  assert_progression_moves("ballad", std::chrono::milliseconds(20500));
}

// disco (index 5): I-vi-ii-V smart sevenths, 1 bar/step, 122 BPM.
void test_disco_default_progression_moves() {
  assert_progression_moves("disco", std::chrono::milliseconds(10000));
}

// house (index 6): i-VII (A minor) smart quality, 2 bars/step, 128 BPM.
void test_house_default_progression_moves() {
  assert_progression_moves("house", std::chrono::milliseconds(11500));
}

// swing (index 7): I-vi-ii-V smart sevenths (same shape as disco), 1
// bar/step, 140 BPM (the fastest built-in tempo -- shortest window needed).
void test_swing_default_progression_moves() {
  assert_progression_moves("swing", std::chrono::milliseconds(9000));
}

// samba (index 9): I-VI7-ii-V7, 1 bar/step, 104 BPM.
void test_samba_default_progression_moves() {
  assert_progression_moves("samba", std::chrono::milliseconds(12000));
}

// reggae (index 10): I-IV plain triads, 1 bar/step, 75 BPM (slow tempo,
// needs a longer window despite only 2 steps total).
void test_reggae_default_progression_moves() {
  assert_progression_moves("reggae", std::chrono::milliseconds(16500));
}

// country (index 11): I-IV-I-V plain triads, 1 bar/step, 120 BPM.
void test_country_default_progression_moves() {
  assert_progression_moves("country", std::chrono::milliseconds(10500));
}

// shuffle (index 13): V7-IV7-I7-I7, all explicit dominant 7ths, 1 bar/step,
// 130 BPM.
void test_shuffle_default_progression_moves() {
  assert_progression_moves("shuffle", std::chrono::milliseconds(9500));
}

// latin (index 14): I-IV-V-IV plain triads, 1 bar/step, kDefaultBpm (120
// BPM -- the style's own tempo field is left unset pending an owner call on
// a cut-time tempo, common/time.hpp's kDefaultBpm applies).
void test_latin_default_progression_moves() {
  assert_progression_moves("latin", std::chrono::milliseconds(10500));
}

// motown (index 15): I-vi-IV-V smart sevenths, 1 bar/step, 124 BPM.
void test_motown_default_progression_moves() {
  assert_progression_moves("motown", std::chrono::milliseconds(10000));
}

// funk (index 4): a single-step "I7 vamp" -- default_style_progressions.hpp's
// own comment states this is intentional ("static I7 vamp, one chord for
// the whole loop"), a genre-idiomatic choice baked into the progression
// TABLE, not a symptom of the #29 regression. The regression's own
// signature was exactly 0 chord events and a bare {0,4,7} popcount-3 tonic
// triad; funk's vamp is neither of those things -- it fires repeatedly
// (the sequencer loop stays alive) and its I7 quality has 4 pitch classes
// (root, third, fifth, flat seventh), strictly richer than a plain triad.
// So this test asserts exactly those two invariants and DELIBERATELY does
// NOT assert distinct_chord_labels.size() > 1 -- that would fail by design,
// every loop iteration replays the same one chord, and asserting it anyway
// would manufacture a false bug report against a documented design choice.
// step.bars=2, tempo 108 BPM (2.222 s/bar) -> one loop period = 4.444 s;
// the 16 s window covers ~3.6 loop periods, comfortably more than one firing.
void test_funk_default_progression_vamp_holds() {
  const ProgressionObservation obs =
      observe_default_progression("funk", std::chrono::milliseconds(16000));

  CHECK(obs.chord_event_count > 1);  // the loop is alive, not a one-shot or a dead sequencer
  CHECK(popcount16(obs.pitch_class_union) > 3);  // I7 (4 pitch classes) > a bare triad (3)

  std::fprintf(stderr,
               "[test_default_style_progression_functional] style=funk "
               "chord_event_count=%d distinct_chord_labels=%zu pitch_class_popcount=%d "
               "(single-chord vamp by design, distinct_chord_labels not asserted >1)\n",
               obs.chord_event_count, obs.distinct_chord_labels.size(),
               popcount16(obs.pitch_class_union));
}

}  // namespace

int main() {
  // Driven in arrangrr::styles::kBuiltins order (style.hpp / this file's
  // header comment table).
  test_basic_default_progression_moves();
  test_pop_default_progression_moves();
  test_rock_default_progression_moves();
  test_ballad_default_progression_moves();
  test_funk_default_progression_vamp_holds();
  test_disco_default_progression_moves();
  test_house_default_progression_moves();
  test_swing_default_progression_moves();
  test_bossa_default_progression_moves();
  test_samba_default_progression_moves();
  test_reggae_default_progression_moves();
  test_country_default_progression_moves();
  test_blues_default_progression_moves();
  test_shuffle_default_progression_moves();
  test_latin_default_progression_moves();
  test_motown_default_progression_moves();
  return sonotron::test::failures();
}

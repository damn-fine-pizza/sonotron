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

}  // namespace

int main() {
  test_bossa_default_progression_moves();
  test_blues_default_progression_moves();
  return sonotron::test::failures();
}

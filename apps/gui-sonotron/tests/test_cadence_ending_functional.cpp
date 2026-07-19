// Option B (docs/proposals/gui-live-harmony-musical-design.md S3.4): a
// dedicated cadence `ChordSequence` swapped in on Ending1/Ending2, so the
// arrangement reliably lands on a real cadential resolution (V-I / IV-I)
// instead of relying on timing luck against the free-running main
// progression loop.
//
// This test drives the REAL production path -- a real InProcessBrainSession
// (the same class main.cpp uses), sending ONLY GUI-reachable command lines
// (`style load <name>`, `transport start`, `style section ending1`, `style
// section varA`) through the real command_line_to_command() translator --
// never a hand-written `seq`/`key` line. It proves two things per style:
//
//   1. Entering Ending1 actually swaps in the cadence tag: at least 2 chord
//      events fire after the "ending1" kSection lands (the cadence's own
//      2-step approach-then-tonic shape), and the tonic pitch class in the
//      per-style cadential content table (cadence_progressions.hpp) is
//      among the pitch classes reported by the sequencer producer.
//   2. A live return to a non-Ending section (`style section varA`, sent
//      WHILE Ending is still playing, well before its own natural 2-bar
//      auto-stop) resumes the main progression cleanly -- at least one
//      fresh chord event fires after "varA" lands, proving the paired
//      kSeqUse(main)+kSeqPlay swap-back never leaves the main slot's phase
//      stale (the exact class of bug the spike behind this task pinned:
//      a bare kSeqUse switch, with no rebasing kSeqPlay, silently stalls
//      the sequencer forever instead of resuming it).
//
// Deliberately does NOT drive the "Ending completes naturally, transport
// auto-stops, THEN a fresh style load resumes" path: `style section varA`
// sent here always lands while genuinely still playing (m_pending_valid is
// checked before Ending's own section_end in Arranger::on_tick, so the live
// variation switch always wins the race, never actually reaching Ending's
// stop_transport branch) -- this sidesteps a separate, pre-existing core
// quirk (Transport::start() resets its own tick counter to 0, while
// Transport::stop() does not touch it) that already affects the ALREADY-
// SHIPPED default-progression re-apply mechanism whenever a style
// load/switch is processed while genuinely stopped with a non-zero tick,
// completely independent of this task's own cadence-swap plumbing. Flagged
// in the implementor's report as a pre-existing, out-of-scope open question,
// not asserted against here.

#include "src/in_process_brain_session.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

struct CadenceObservation {
  bool ending_seen = false;
  bool resumed_request_sent = false;
  bool resumed_section_seen = false;
  int cadence_chord_count = 0;
  std::uint16_t cadence_pc_union = 0;
  int resume_chord_count = 0;
};

// Drives `style load <style_name>` / `transport start` / `style section
// ending1` through the real InProcessBrainSession::send() translator, waits
// (real wall-clock, scaled off `seconds_per_bar`) until roughly 1.3 bars
// into Ending1 -- comfortably after the cadence's own 2nd (tonic) step has
// fired, still comfortably before Ending's own natural 2-bar auto-stop --
// then sends `style section varA` to trigger the live swap-back, and keeps
// polling until `total_wait` elapses.
// Classifies a single polled event into the observation, tracking the
// ending-seen / resumed-section-seen state machine and the chord/pitch-class
// tallies either side of the swap-back. Extracted out of the polling loop
// below purely to keep that loop's own cognitive complexity low -- this is
// the same "extract the branchy per-item body" shape already used in
// in_process_brain_session.cpp's observe_cadence_section_event()/
// step_cadence().
void classify_cadence_event(const BrainEvent& ev,
                            std::chrono::steady_clock::time_point& ending_seen_at,
                            CadenceObservation& obs) {
  if (ev.kind == BrainEvent::Kind::kSection) {
    if (!obs.ending_seen && ev.section_name == "ending1") {
      obs.ending_seen = true;
      ending_seen_at = std::chrono::steady_clock::now();
    } else if (obs.ending_seen && !obs.resumed_section_seen && ev.section_name == "varA") {
      obs.resumed_section_seen = true;
    }
    return;
  }
  if (ev.kind == BrainEvent::Kind::kChord) {
    if (obs.resumed_section_seen) {
      ++obs.resume_chord_count;
    } else if (obs.ending_seen) {
      ++obs.cadence_chord_count;
    }
    return;
  }
  if (ev.kind == BrainEvent::Kind::kChordFollowed && ev.followed_source == "sequencer" &&
      obs.ending_seen && !obs.resumed_section_seen) {
    obs.cadence_pc_union |= static_cast<std::uint16_t>(ev.followed_current_pcs);
  }
}

CadenceObservation observe_cadence_at_ending(const std::string& style_name, double seconds_per_bar,
                                             std::chrono::milliseconds total_wait) {
  InProcessBrainSession session;
  CHECK(session.start());

  session.send("style load " + style_name);
  session.send("transport start");
  session.send("style section ending1");

  CadenceObservation obs;
  auto ending_seen_at = std::chrono::steady_clock::now();
  const auto trigger_delay =
      std::chrono::milliseconds(static_cast<long long>(seconds_per_bar * 1.3 * 1000));
  const auto deadline = std::chrono::steady_clock::now() + total_wait;
  std::vector<BrainEvent> batch;
  while (std::chrono::steady_clock::now() < deadline) {
    batch.clear();
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      classify_cadence_event(ev, ending_seen_at, obs);
    }
    if (obs.ending_seen && !obs.resumed_request_sent &&
        std::chrono::steady_clock::now() - ending_seen_at >= trigger_delay) {
      session.send("style section varA");
      obs.resumed_request_sent = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  session.stop();
  return obs;
}

// Shared assertion body: `tonic_pc` is the cadential tag's own resolution
// pitch class (cadence_progressions.hpp's 2nd step root_pc for this style --
// equal to the style's own key_root_pc, default_style_progressions.hpp).
void assert_cadence_resolves_and_resumes(const std::string& style_name, double seconds_per_bar,
                                         std::chrono::milliseconds total_wait,
                                         std::uint8_t tonic_pc) {
  const CadenceObservation obs = observe_cadence_at_ending(style_name, seconds_per_bar, total_wait);

  CHECK(obs.ending_seen);  // the Ending transition actually landed
  // The cadence's own 2-step approach-then-tonic shape: at least 2 chord
  // events fired between Ending landing and the live varA swap-back.
  CHECK(obs.cadence_chord_count >= 2);
  CHECK((obs.cadence_pc_union & static_cast<std::uint16_t>(1U << tonic_pc)) != 0);

  CHECK(obs.resumed_section_seen);    // the live varA swap-back actually landed
  CHECK(obs.resume_chord_count > 0);  // main progression fired again: no stale phase

  std::fprintf(stderr,
               "[test_cadence_ending_functional] style=%s ending_seen=%d "
               "cadence_chord_count=%d cadence_pc_union=0x%03x resumed_section_seen=%d "
               "resume_chord_count=%d\n",
               style_name.c_str(), obs.ending_seen, obs.cadence_chord_count, obs.cadence_pc_union,
               obs.resumed_section_seen, obs.resume_chord_count);
}

// pop (index 1): key C major (key_root_pc=0), IV-I cadence, 120 BPM
// (seconds_per_bar = 240/120 = 2.0).
void test_pop_cadence_resolves_and_resumes() {
  assert_cadence_resolves_and_resumes("pop", 2.0, std::chrono::milliseconds(12500), 0);
}

// rock (index 2): key G mixolydian (key_root_pc=7), plagal IV-I cadence
// (mixolydian has no functional V), 130 BPM (seconds_per_bar = 240/130 =
// 1.846).
void test_rock_cadence_resolves_and_resumes() {
  assert_cadence_resolves_and_resumes("rock", 1.846, std::chrono::milliseconds(11500), 7);
}

// house (index 6): key A minor (key_root_pc=9), harmonic-minor v7-i cadence
// (natural minor has no leading tone either), 128 BPM (seconds_per_bar =
// 240/128 = 1.875).
void test_house_cadence_resolves_and_resumes() {
  assert_cadence_resolves_and_resumes("house", 1.875, std::chrono::milliseconds(11500), 9);
}

// bossa (index 8, MANDATORY per task): key C major (key_root_pc=0), V-I
// cadence -- bossa's own main-progression loop point is vi->ii, NOT
// cadential (S3.4's own explicit call-out), so this is the one style where
// the cadence tag's resolution is NOT already reachable by luck from the
// free-running main progression. 130 BPM (seconds_per_bar = 240/130 =
// 1.846).
void test_bossa_cadence_resolves_and_resumes() {
  assert_cadence_resolves_and_resumes("bossa", 1.846, std::chrono::milliseconds(11500), 0);
}

// funk (index 4): key C major (key_root_pc=0), V7-I7 cadence (funk's own
// main progression is a static I7 vamp with no natural approach at all --
// S3.4's own judgment call). 108 BPM (seconds_per_bar = 240/108 = 2.222).
void test_funk_cadence_resolves_and_resumes() {
  assert_cadence_resolves_and_resumes("funk", 2.222, std::chrono::milliseconds(13500), 0);
}

}  // namespace

int main() {
  test_pop_cadence_resolves_and_resumes();
  test_rock_cadence_resolves_and_resumes();
  test_house_cadence_resolves_and_resumes();
  test_bossa_cadence_resolves_and_resumes();
  test_funk_cadence_resolves_and_resumes();
  return sonotron::test::failures();
}

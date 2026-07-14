// Reachability test (Nazzareno, closing Torquato's RED-by-design QA pin --
// docs/phase6-design-reviews.md "Audio in the standalone GUI"). Formerly
// test_audio_primary_port_unreachable.cpp / EXPECTED TO FAIL: Torquato's QA
// pass found that a fresh InProcessBrainSession had NO reachable way to make
// a track's kMidi land on the primary integrated output port
// (kPrimaryAudioOutPort == 0, in_process_brain_session.cpp), because `style
// load <name>` alone never routes any TrackRole to an output port
// (Route::enabled defaults to false, arranger.hpp) and no gui-sonotron panel
// ever emitted `style route`.
//
// FIX: InProcessBrainSession::send() now auto-issues the default band
// routing (kDefaultStyleRoutes, in_process_brain_session.cpp -- the exact
// role->port:channel map apps/demo/jam/setup.acmd's own `style route` lines
// produce) as real Param::kStyleRoute Commands, through the same validated
// command_ring path, right after a `style load` succeeds. That flips this
// pin from RED to GREEN: this file now asserts the POSITIVE case (mirrors
// test_audio_port_gate.cpp's established shape for the negative case).

#include "src/in_process_brain_session.hpp"

#include <chrono>
#include <thread>
#include <vector>

#include "src/audio_midi_event.hpp"
#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::AudioMidiEvent;
using sonotron::AudioMidiRing;
using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

constexpr int kPrimaryPort = 0;  // mirrors kPrimaryAudioOutPort

void test_style_playback_reaches_primary_port_ring() {
  InProcessBrainSession session;
  AudioMidiRing ring;
  session.set_audio_ring(&ring);
  CHECK(session.start());

  session.send("style load basic");
  session.send("transport start");

  // Two independent, direct proofs of reachability: the GUI-facing poll()
  // stream (matches test_audio_port_gate.cpp's own check shape) AND a direct
  // pop off the actual Theme-2 audio ring -- the literal mechanism the task
  // asks to prove, not just its poll()-side mirror.
  bool saw_primary_midi_via_poll = false;
  bool saw_primary_midi_in_ring = false;
  for (int i = 0; i < 500 && !(saw_primary_midi_via_poll && saw_primary_midi_in_ring); ++i) {
    std::vector<BrainEvent> batch;
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      if (ev.kind == BrainEvent::Kind::kMidiOut && ev.port == kPrimaryPort) {
        saw_primary_midi_via_poll = true;
      }
    }
    AudioMidiEvent ev;
    while (ring.try_pop(ev)) {
      if (ev.port == kPrimaryPort) {
        saw_primary_midi_in_ring = true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(saw_primary_midi_via_poll);
  CHECK(saw_primary_midi_in_ring);

  session.stop();
}

}  // namespace

int main() {
  test_style_playback_reaches_primary_port_ring();
  return sonotron::test::failures();
}

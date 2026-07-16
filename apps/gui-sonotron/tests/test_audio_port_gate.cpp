// Phase-6 Theme 2 QA pass (docs/phase6-design-reviews.md "Audio in the
// standalone GUI", Decision 3/4): pins InProcessBrainSession's owner-chosen
// port filter -- ONLY OutEvent::Kind::kMidi on the primary integrated output
// port (kPrimaryAudioOutPort == 0) is pushed onto the AudioMidiRing handed to
// set_audio_ring(); every other port's kMidi traffic must reach poll() (the
// GUI/out_event_ring path is unfiltered) but must NEVER land in the audio
// ring.
//
// `midi-source load <path>` (midisrc::MidiSourceStage, constructed at a
// FIXED, distinct port -- components/hostrt/shell.cpp's kMidiSourcePort ==
// 1) is the negative-case traffic source: it is NOT gated on transport state
// (its on_tick fires straight off the shared tick clock) and needs no
// pattern-engine routing, so it is an always-reachable, known-non-primary-
// port kMidi source through the already-wired `send()` surface.
//
// There is deliberately NO positive-case test in this file (see
// test_audio_primary_port_unreachable.cpp for why, and for the RED pin that
// documents it): in a fresh InProcessBrainSession there is currently no
// reachable way to make ANY track's kMidi land on the primary port at all,
// so "primary-port events reach the ring" cannot be exercised end to end
// without a production seam this review does not add. What IS verified here
// -- and is the more safety-critical half of the gate regardless -- is that
// non-primary-port traffic can never leak into the ring.
//
// Runs with no ALSA sequencer device required for the assertions themselves
// (mirrors test_in_process_brain_session.cpp).

#include "src/in_process_brain_session.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "audio/audio_midi_event.hpp"
#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::AudioMidiEvent;
using sonotron::AudioMidiRing;
using sonotron::BrainEvent;
using sonotron::InProcessBrainSession;

namespace {

constexpr int kSecondaryPort = 1;  // mirrors components/hostrt/shell.cpp's kMidiSourcePort

// Checked CONTINUOUSLY across the whole wait window, not just once at the
// end, so a transient wrong-port push cannot hide behind a final
// empty-ring snapshot.
void test_secondary_port_events_never_reach_ring() {
  InProcessBrainSession session;
  AudioMidiRing ring;
  session.set_audio_ring(&ring);
  CHECK(session.start());

  session.send(std::string("midi-source load ") + GUI_SONOTRON_TEST_MIDI_FIXTURE);

  bool saw_secondary_midi = false;
  bool ring_stayed_empty = true;
  for (int i = 0; i < 500 && !saw_secondary_midi; ++i) {
    std::vector<BrainEvent> batch;
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      if (ev.kind == BrainEvent::Kind::kMidiOut && ev.port == kSecondaryPort) {
        saw_secondary_midi = true;
      }
    }
    AudioMidiEvent leaked;
    while (ring.try_pop(leaked)) {
      ring_stayed_empty = false;  // any pop here is the ring gate failing
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(saw_secondary_midi);  // the source really did produce port-1 traffic
  CHECK(ring_stayed_empty);   // ...and none of it ever reached the audio ring

  session.stop();
}

}  // namespace

int main() {
  test_secondary_port_events_never_reach_ring();
  return sonotron::test::failures();
}

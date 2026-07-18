// Anti-no-op contract test (Task #11 Phase 1, Sequence Edit step sequencer --
// roadmap node 11600/11610's own "prove it, don't just parse it" rule -- the
// same class of gap the historical `program` no-op bug was). Proves the
// WHOLE vertical slice end to end in the DEFAULT in-process backend: `track
// new` -> `track mute ... on` -> `track step` -> `clip add ... track ...` ->
// `launch clip ... quantize 0` -> `transport start` actually reaches
// arrangrr::Timeline::on_tick and produces a real MIDI NoteOn on the primary
// output port -- not merely that the GUI-side translator (in_process_brain_
// session.cpp's command_line_to_command) parses the five new `track ...`
// verbs and the `clip add ... track ...` selector into well-formed Commands.
// Mirrors test_audio_primary_port_reachable.cpp's exact two-proof shape (the
// GUI-facing poll() stream AND a direct pop off the real Theme-2
// AudioMidiRing).

#include "src/in_process_brain_session.hpp"

#include <chrono>
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

constexpr int kPrimaryPort = 0;  // mirrors kPrimaryAudioOutPort

// Task #11 Phase 1's own end-to-end wiring proof: creates ONE step track on
// the drums role (port 0, channel 9), pre-mutes it (the CALLER's own
// responsibility -- Timeline fires every registered track unconditionally,
// see in_process_brain_session.cpp's `track new` comment), writes a single
// audible step, registers a kStepTrack clip at a known id, launches it
// IMMEDIATELY (quantize 0 -- the same synchronous unmute the existing
// test_clip_add_with_explicit_id_registers_and_launches test already
// establishes needs no `transport start` first), and only THEN starts the
// transport -- proving the note reaches the SAME primary-port MIDI path
// style-based playback does, through arrangrr::Timeline::on_tick, never a
// translator-only round trip.
void test_step_track_click_to_core_reaches_primary_port() {
  InProcessBrainSession session;
  AudioMidiRing ring;
  session.set_audio_ring(&ring);
  CHECK(session.start());

  session.send("track new drums 0 9");
  session.send("track mute 0 on");
  session.send("track step 0 1 60 100 120");
  session.send("clip add drums 0 track 0 id 0");
  session.send("launch clip 0 quantize 0");

  // Drain a moment of setup traffic BEFORE starting the transport, asserting
  // none of the five wire commands above ever produced a "bad_argument"
  // warn -- the translator's own encoding must be well-formed, not merely
  // non-crashing.
  std::vector<BrainEvent> setup_events;
  for (int i = 0; i < 50; ++i) {
    std::vector<BrainEvent> batch;
    session.poll(batch);
    for (BrainEvent& ev : batch) {
      setup_events.push_back(std::move(ev));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  for (const BrainEvent& ev : setup_events) {
    CHECK(ev.kind != BrainEvent::Kind::kWarn);
  }

  session.send("transport start");

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

// A track left WITHOUT the caller's own `track mute <idx> on` (the exact
// step grid_panel.cpp's real gesture always issues immediately after `track
// new`) is audible the instant the transport runs, even with no clip ever
// launched -- pinning WHY that pre-mute step is mandatory, not optional
// (Timeline::on_tick fires every registered track unconditionally,
// regardless of ClipMatrix/launch state).
void test_new_track_without_premute_is_audible_unmuted() {
  InProcessBrainSession session;
  AudioMidiRing ring;
  session.set_audio_ring(&ring);
  CHECK(session.start());

  session.send("track new drums 0 9");
  session.send("track step 0 1 60 100 120");
  // Deliberately NO `track mute 0 on` and NO clip/launch at all.
  session.send("transport start");

  bool saw_primary_midi_in_ring = false;
  for (int i = 0; i < 500 && !saw_primary_midi_in_ring; ++i) {
    std::vector<BrainEvent> batch;
    session.poll(batch);
    AudioMidiEvent ev;
    while (ring.try_pop(ev)) {
      if (ev.port == kPrimaryPort) {
        saw_primary_midi_in_ring = true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(saw_primary_midi_in_ring);

  session.stop();
}

}  // namespace

int main() {
  test_step_track_click_to_core_reaches_primary_port();
  test_new_track_without_premute_is_audible_unmuted();
  return sonotron::test::failures();
}

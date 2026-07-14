// Torquato QA pass, Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in
// the standalone GUI"): Nazzareno's landing shipped gui_sonotron_audio's
// AudioEngine with NO dedicated test (a real audio device can fail
// headless/CI, per the mandate's own note). This is the device-INDEPENDENT
// slice that CAN be proven without one: AudioEngine must construct and
// destruct cleanly whether or not ma_device_init() actually opened a device
// (device_ready() may legitimately be false in a headless sandbox --
// see AudioEngine's own constructor comment), load_soundfont()/note_ring()
// must behave correctly regardless, and destruction (all_notes_off() under
// the mutex, see AudioEngine::~AudioEngine()) must never crash even when
// nothing was ever loaded. What this test CANNOT reach without a real
// device: the data_callback()/render() path itself (private, only invoked
// by miniaudio) -- see this milestone's QA report for the concurrency
// verdict on that gap and the ThreadSanitizer run performed on the
// device-independent producer/consumer boundary instead
// (test_audio_port_gate.cpp, test_in_process_brain_session.cpp).

#include "src/audio_engine.hpp"

#include <string>

#include "melodd/soundfont_discovery.hpp"
#include "src/audio_midi_event.hpp"
#include "test.hpp"

using sonotron::AudioEngine;
using sonotron::AudioMidiEvent;

int main() {
  // --- construction/destruction never crashes, device or no device --------
  {
    AudioEngine engine;
    (void)engine.device_ready();  // legitimately false in a headless sandbox
  }  // ~AudioEngine(): stops the device (if any) then all_notes_off() under
     // the mutex -- must not crash with nothing ever loaded/playing.

  // --- note_ring() is usable even before/without a device --------------------
  {
    AudioEngine engine;
    AudioMidiEvent pushed{.port = 0, .msg = arrangrr::MidiMessage::note_on(0, 60, 100)};
    CHECK(engine.note_ring().try_push(pushed));
    AudioMidiEvent popped;
    CHECK(engine.note_ring().try_pop(popped));
    CHECK(popped.port == pushed.port);
    CHECK(popped.msg.status == pushed.msg.status);
    CHECK(popped.msg.d1 == pushed.msg.d1);
    CHECK(popped.msg.d2 == pushed.msg.d2);
    CHECK(!engine.note_ring().try_pop(popped));  // drained
  }

  // --- load_soundfont(): failure path keeps `error` set, does not crash ----
  {
    AudioEngine engine;
    std::string error;
    CHECK(!engine.load_soundfont("/no/such/soundfont.sf2", error));
    CHECK(!error.empty());
  }

  // --- load_soundfont(): success path (when a system SoundFont exists) -----
  const std::string soundfont = melodd::find_system_soundfont();
  if (soundfont.empty()) {
    std::printf(
        "SKIP (partial): no system GM SoundFont found under /usr/share/soundfonts -- "
        "the device-independent construction/ring/failure-path checks above still ran.\n");
    return sonotron::test::failures();
  }
  {
    AudioEngine engine;
    std::string error;
    CHECK(engine.load_soundfont(soundfont, error));
    CHECK(error.empty());
    // A second, failing load must not disturb the first (Decision 4 parity,
    // same contract test_soundfont_load_split.cpp pins at the Synth level --
    // exercised here through AudioEngine's own public surface instead).
    std::string second_error;
    CHECK(!engine.load_soundfont("/no/such/soundfont.sf2", second_error));
    CHECK(!second_error.empty());
  }  // destructor: all_notes_off() under the mutex, with a real SoundFont
     // loaded this time -- still must not crash.

  if (sonotron::test::failures() == 0) {
    std::printf("OK: AudioEngine construction/ring/load_soundfont surface, device-independent\n");
  }
  return sonotron::test::failures();
}

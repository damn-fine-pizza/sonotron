// Torquato QA pass, Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in
// the standalone GUI"), promoted alongside the ISoundEngine seam (docs/
// proposals/isoundengine-contract.md, Corelli): this is the device-
// INDEPENDENT slice that CAN be proven without a real audio device --
// AudioBackend must construct and destruct cleanly whether or not
// ma_device_init() actually opened a device (device_ready() may legitimately
// be false in a headless sandbox -- see AudioBackend's own constructor
// comment), SoundfontEngine::load()/AudioBackend::note_ring() must behave
// correctly regardless, and destruction (all_notes_off() under the mutex,
// see AudioBackend::~AudioBackend()) must never crash even when nothing was
// ever loaded. What this test CANNOT reach without a real device: the
// private data_callback()/render() path itself (only invoked by miniaudio)
// -- see this milestone's QA report for the concurrency verdict on that gap
// and the ThreadSanitizer run performed on the device-independent
// producer/consumer boundary instead (test_audio_port_gate.cpp,
// test_in_process_brain_session.cpp).

#include "audio/audio_backend.hpp"
#include "audio/soundfont_engine.hpp"

#include <string>

#include "audio/audio_midi_event.hpp"
#include "melodd/soundfont_discovery.hpp"
#include "test.hpp"

using sonotron::AudioMidiEvent;
using sonotron::audio::AudioBackend;
using sonotron::audio::SoundfontEngine;

int main() {
  // --- construction/destruction never crashes, device or no device --------
  {
    SoundfontEngine engine;
    AudioBackend backend(engine);
    (void)backend.device_ready();  // legitimately false in a headless sandbox
  }  // ~AudioBackend(): stops the device (if any) then all_notes_off() under
     // the mutex -- must not crash with nothing ever loaded/playing.

  // --- note_ring() is usable even before/without a device --------------------
  {
    SoundfontEngine engine;
    AudioBackend backend(engine);
    AudioMidiEvent pushed{.port = 0, .msg = arrangrr::MidiMessage::note_on(0, 60, 100)};
    CHECK(backend.note_ring().try_push(pushed));
    AudioMidiEvent popped;
    CHECK(backend.note_ring().try_pop(popped));
    CHECK(popped.port == pushed.port);
    CHECK(popped.msg.status == pushed.msg.status);
    CHECK(popped.msg.d1 == pushed.msg.d1);
    CHECK(popped.msg.d2 == pushed.msg.d2);
    CHECK(!backend.note_ring().try_pop(popped));  // drained
  }

  // --- SoundfontEngine::load(): failure path keeps `error` set, no crash ---
  {
    SoundfontEngine engine;
    AudioBackend backend(engine);
    std::string error;
    CHECK(!engine.load("/no/such/soundfont.sf2", error, backend.render_mutex()));
    CHECK(!error.empty());
  }

  // --- SoundfontEngine::load(): success path (system SoundFont present) ----
  const std::string soundfont = melodd::find_system_soundfont();
  if (soundfont.empty()) {
    std::printf(
        "SKIP (partial): no system GM SoundFont found under /usr/share/soundfonts -- "
        "the device-independent construction/ring/failure-path checks above still ran.\n");
    return sonotron::test::failures();
  }
  {
    SoundfontEngine engine;
    AudioBackend backend(engine);
    std::string error;
    CHECK(engine.load(soundfont, error, backend.render_mutex()));
    CHECK(error.empty());
    // A second, failing load must not disturb the first (Decision 4 parity,
    // same contract test_soundfont_load_split.cpp pins at the Synth level --
    // exercised here through SoundfontEngine's own public surface instead).
    std::string second_error;
    CHECK(!engine.load("/no/such/soundfont.sf2", second_error, backend.render_mutex()));
    CHECK(!second_error.empty());
  }  // destructor: all_notes_off() under the mutex, with a real SoundFont
     // loaded this time -- still must not crash.

  if (sonotron::test::failures() == 0) {
    std::printf(
        "OK: AudioBackend/SoundfontEngine construction/ring/load surface, device-independent\n");
  }
  return sonotron::test::failures();
}

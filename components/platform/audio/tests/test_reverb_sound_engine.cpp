// Anti-NO-OP proof for ReverbSoundEngine (Phase-1 sound task #7, deliverable
// A): a SpyEngine test double pins that dispatch()/all_notes_off() really
// forward to the inner engine unchanged, and that render() really mutates
// the signal (wet != dry) instead of silently regressing to a passthrough.
// The final block repeats the wet != dry proof against a REAL
// SoundfontEngine, same device-independent, SKIP-partial-if-no-system-
// SoundFont convention as test_audio_engine_smoke.cpp.

#include "audio/reverb_sound_engine.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

#include "audio/soundfont_engine.hpp"
#include "melodd/soundfont_discovery.hpp"
#include "test.hpp"

using sonotron::audio::ReverbSoundEngine;
using sonotron::audio::SoundfontEngine;

namespace {

// Minimal ISoundEngine test double: records every call it receives and
// fills render()'s buffer with a fixed, deterministic, NON-SILENT signal
// (0.2f / -0.2f) so "wet == dry" is trivially detectable downstream.
class SpyEngine : public sonotron::audio_engine::ISoundEngine {
 public:
  void dispatch(const arrangrr::MidiMessage& msg) noexcept override {
    ++dispatch_count;
    last_message = msg;
  }

  void all_notes_off() noexcept override { ++all_notes_off_count; }

  void render(float* out, int frame_count) noexcept override {
    ++render_count;
    last_frame_count = frame_count;
    if (out == nullptr) {
      return;
    }
    for (int frame = 0; frame < frame_count; ++frame) {
      out[(static_cast<std::size_t>(frame) * 2) + 0] = 0.2f;
      out[(static_cast<std::size_t>(frame) * 2) + 1] = -0.2f;
    }
  }

  const char* name() const noexcept override { return "spy-engine"; }

  int dispatch_count = 0;
  int all_notes_off_count = 0;
  int render_count = 0;
  int last_frame_count = 0;
  arrangrr::MidiMessage last_message{};
};

}  // namespace

int main() {
  // --- dispatch()/all_notes_off() forward exactly once, unchanged --------
  {
    SpyEngine spy;
    ReverbSoundEngine engine(spy);

    const arrangrr::MidiMessage msg = arrangrr::MidiMessage::note_on(0, 60, 100);
    engine.dispatch(msg);
    CHECK(spy.dispatch_count == 1);
    CHECK(spy.last_message.status == msg.status);
    CHECK(spy.last_message.d1 == msg.d1);
    CHECK(spy.last_message.d2 == msg.d2);

    engine.all_notes_off();
    CHECK(spy.all_notes_off_count == 1);
  }

  // --- render() calls the inner engine once, mutates the buffer ----------
  {
    SpyEngine spy;
    ReverbSoundEngine engine(spy);

    constexpr int kFrameCount = 256;
    std::array<float, static_cast<std::size_t>(kFrameCount) * 2> buffer{};
    engine.render(buffer.data(), kFrameCount);

    CHECK(spy.render_count == 1);
    CHECK(spy.last_frame_count == kFrameCount);

    bool any_nan = false;
    bool differs_from_dry = false;
    for (int frame = 0; frame < kFrameCount; ++frame) {
      const float l = buffer[(static_cast<std::size_t>(frame) * 2) + 0];
      const float r = buffer[(static_cast<std::size_t>(frame) * 2) + 1];
      if (std::isnan(l) || std::isnan(r)) {
        any_nan = true;
      }
      if (l != 0.2f || r != -0.2f) {
        differs_from_dry = true;
      }
    }
    CHECK(!any_nan);
    CHECK(differs_from_dry);
  }

  // --- render(nullptr, 0) still forwards, does not crash -----------------
  {
    SpyEngine spy;
    ReverbSoundEngine engine(spy);
    engine.render(nullptr, 0);
    CHECK(spy.render_count == 1);
  }

  // --- name() composes the inner engine's name() as a substring ----------
  {
    SpyEngine spy;
    ReverbSoundEngine engine(spy);
    const std::string composed = engine.name();
    CHECK(composed.find(spy.name()) != std::string::npos);
  }

  // --- end-to-end against a REAL SoundfontEngine: wet != dry on real ------
  // synthesized audio, not just the spy's synthetic ramp.
  const std::string soundfont = melodd::find_system_soundfont();
  if (soundfont.empty()) {
    std::printf(
        "SKIP (partial): no system GM SoundFont found under /usr/share/soundfonts -- "
        "the SpyEngine-based forwarding/DSP checks above still ran.\n");
    if (sonotron::test::failures() == 0) {
      std::printf("OK: ReverbSoundEngine forwards dispatch/all_notes_off, wet != dry, no NaNs\n");
    }
    return sonotron::test::failures();
  }

  {
    SoundfontEngine dry_engine;
    SoundfontEngine inner_engine;
    ReverbSoundEngine wet_engine(inner_engine);

    std::mutex dry_mutex;
    std::mutex inner_mutex;
    std::string error;
    CHECK(dry_engine.load(soundfont, error, dry_mutex));
    CHECK(error.empty());
    std::string inner_error;
    CHECK(inner_engine.load(soundfont, inner_error, inner_mutex));
    CHECK(inner_error.empty());

    const arrangrr::MidiMessage note = arrangrr::MidiMessage::note_on(0, 60, 100);
    dry_engine.dispatch(note);
    wet_engine.dispatch(note);

    constexpr int kFrameCount = 512;
    std::array<float, static_cast<std::size_t>(kFrameCount) * 2> dry_buffer{};
    std::array<float, static_cast<std::size_t>(kFrameCount) * 2> wet_buffer{};
    dry_engine.render(dry_buffer.data(), kFrameCount);
    wet_engine.render(wet_buffer.data(), kFrameCount);

    bool any_nan = false;
    bool differs = false;
    for (std::size_t i = 0; i < dry_buffer.size(); ++i) {
      if (std::isnan(dry_buffer[i]) || std::isnan(wet_buffer[i])) {
        any_nan = true;
      }
      if (std::fabs(dry_buffer[i] - wet_buffer[i]) > 1e-6f) {
        differs = true;
      }
    }
    CHECK(!any_nan);
    CHECK(differs);
  }

  if (sonotron::test::failures() == 0) {
    std::printf("OK: ReverbSoundEngine forwards dispatch/all_notes_off, wet != dry, no NaNs\n");
  }
  return sonotron::test::failures();
}

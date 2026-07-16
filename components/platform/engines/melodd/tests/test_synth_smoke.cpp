#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "melodd/soundfont_discovery.hpp"
#include "melodd/synth.hpp"
#include "test.hpp"

// Headless render smoke test (docs/design/phase5-execution-plan.md Item F
// gate): drives melodd::Synth with a note-on, renders audio, asserts the
// output is non-silent, then asserts note-off eventually silences it --
// proves synthesis works without needing a soundcard in CI. If no system GM
// SoundFont is installed, this SKIPS (exit 77), matching the convention
// tests/integration/CMakeLists.txt already uses for environment-dependent
// tests, rather than failing.

namespace {

double rms(const float* buffer, int frame_count) {
  double sum_sq = 0.0;
  for (int i = 0; i < frame_count * 2; ++i) {
    const double sample = static_cast<double>(buffer[i]);
    sum_sq += sample * sample;
  }
  return std::sqrt(sum_sq / static_cast<double>(frame_count * 2));
}

}  // namespace

int main() {
  const std::string soundfont = melodd::find_system_soundfont();
  if (soundfont.empty()) {
    std::printf(
        "SKIP: no system GM SoundFont found under /usr/share/soundfonts "
        "(install fluid-soundfont-gm to run this smoke test)\n");
    return 77;
  }

  constexpr int kSampleRate = 44100;
  melodd::Synth synth(kSampleRate);
  std::string error;
  if (!synth.load_soundfont(soundfont, error)) {
    std::printf("FAIL: could not load SoundFont '%s': %s\n", soundfont.c_str(), error.c_str());
    return 1;
  }
  CHECK(synth.loaded());

  // --- note-on renders non-silent audio -----------------------------------
  synth.note_on(/*channel=*/0, /*key=*/60, /*velocity=*/100);
  constexpr int kFrames = 4096;
  std::vector<float> sounding(static_cast<std::size_t>(kFrames) * 2);
  synth.render(sounding.data(), kFrames);
  const double sounding_rms = rms(sounding.data(), kFrames);
  CHECK(sounding_rms > 1e-4);

  // --- note-off eventually silences it -------------------------------------
  synth.note_off(0, 60);
  // Discard several seconds of release tail so the envelope has time to
  // finish, regardless of which instrument the system SoundFont maps GM
  // program 0 to.
  constexpr int kTailFrames = kSampleRate * 4;
  std::vector<float> tail(static_cast<std::size_t>(kTailFrames) * 2);
  synth.render(tail.data(), kTailFrames);

  std::vector<float> after(static_cast<std::size_t>(kFrames) * 2);
  synth.render(after.data(), kFrames);
  const double silence_rms = rms(after.data(), kFrames);
  CHECK(silence_rms < 1e-3);

  if (melodd::test::failures() == 0) {
    std::printf("OK: sounding RMS=%.6f, post-release RMS=%.6f (soundfont=%s)\n", sounding_rms,
                silence_rms, soundfont.c_str());
  }
  return melodd::test::failures();
}

#include <string>

#include "melodd/soundfont_discovery.hpp"
#include "melodd/synth.hpp"
#include "test.hpp"

// Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in the standalone
// GUI", Decision 4): Synth::load_soundfont() was split into
// read_soundfont_file() (disk I/O, no mutex -- the GUI-thread-only half) and
// adopt_soundfont() (the fast pointer-swap tail gui_sonotron_audio::
// AudioEngine guards with its mutex). Proves the split stays behavior-
// identical to the single-call load_soundfont() it used to be:
//   - success loads and resets every channel to the GM default (program 0);
//   - a failed read leaves any PREVIOUSLY loaded SoundFont's state (both
//     `loaded()` and per-channel state) completely untouched;
//   - `error` is set only on failure.
// Uses the same debug_program() test seam as test_dispatch.cpp. Same
// SKIP-if-no-system-SoundFont convention as test_synth_smoke.cpp.

int main() {
  const std::string soundfont = melodd::find_system_soundfont();
  if (soundfont.empty()) {
    std::printf(
        "SKIP: no system GM SoundFont found under /usr/share/soundfonts "
        "(install fluid-soundfont-gm to run this test)\n");
    return 77;
  }

  // --- read_soundfont_file() + adopt_soundfont() matches load_soundfont() -
  melodd::Synth via_split(melodd::kDefaultSampleRate);
  CHECK(!via_split.loaded());
  std::string split_error;
  tsf* loaded = melodd::Synth::read_soundfont_file(soundfont, split_error);
  CHECK(loaded != nullptr);
  CHECK(split_error.empty());
  CHECK(!via_split.loaded());  // read alone does not mutate the Synth yet
  via_split.adopt_soundfont(loaded);
  CHECK(via_split.loaded());
  // Every channel resets to GM program 0 on adopt (including the percussion
  // channel, index 9 -- reset_gm_channel_defaults() sets preset NUMBER 0
  // there too, just from the drum bank; debug_program() reports the number).
  for (int ch = 0; ch < 16; ++ch) {
    CHECK(via_split.debug_program(ch) == 0);
  }

  melodd::Synth via_single(melodd::kDefaultSampleRate);
  std::string single_error;
  CHECK(via_single.load_soundfont(soundfont, single_error));
  CHECK(via_single.loaded());
  for (int ch = 0; ch < 16; ++ch) {
    CHECK(via_single.debug_program(ch) == via_split.debug_program(ch));
  }

  // --- read_soundfont_file() failure: nullptr + error set, nothing adopted -
  std::string fail_error;
  tsf* failed = melodd::Synth::read_soundfont_file("/no/such/soundfont.sf2", fail_error);
  CHECK(failed == nullptr);
  CHECK(!fail_error.empty());

  // --- a failed read leaves a PREVIOUSLY loaded Synth completely untouched -
  // Mutate observable state first (program_change on channel 0) so a bug
  // that accidentally called adopt_soundfont() on a null/failed read (which
  // would tsf_close() the current SoundFont and reset every channel) has
  // something concrete to disturb.
  via_split.program_change(0, 42);
  CHECK(via_split.debug_program(0) == 42);
  std::string second_fail_error;
  tsf* second_failed =
      melodd::Synth::read_soundfont_file("/no/such/soundfont.sf2", second_fail_error);
  CHECK(second_failed == nullptr);
  // No adopt_soundfont() call follows (matches AudioEngine::load_soundfont()'s
  // own early-return-on-failure shape) -- state must be exactly as before.
  CHECK(via_split.loaded());
  CHECK(via_split.debug_program(0) == 42);

  if (melodd::test::failures() == 0) {
    std::printf("OK: read_soundfont_file/adopt_soundfont split matches load_soundfont "
                "(soundfont=%s)\n",
                soundfont.c_str());
  }
  return melodd::test::failures();
}

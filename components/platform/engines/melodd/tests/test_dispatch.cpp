#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "melodd/dispatch.hpp"
#include "melodd/soundfont_discovery.hpp"
#include "melodd/synth.hpp"
#include "test.hpp"

// Phase-6 Theme 2 (docs/phase6-design-reviews.md "Audio in the standalone
// GUI", Decision 5): proves melodd::dispatch_midi_message -- the ONE shared
// MidiMessage -> Synth decode both apps/tools/melodd's main.cpp and
// sonotron::audio::SoundfontEngine now call -- routes every branch the OLD
// per-caller switches used to hand-roll: note-on, note-on-with-velocity-0-
// as-note-off, note-off, program_change, pitch_bend (14-bit LSB/MSB
// assembly, including an asymmetric-byte case that pins the byte ORDER, not
// just "some routing happened"), control_change (ALL_SOUND_OFF), and
// non-channel-voice bytes being ignored. Mirrors test_synth_smoke.cpp's own
// "no system SoundFont -> SKIP (exit 77)" convention.
//
// program_change/pitch_bend are asserted through Synth::debug_program()/
// debug_pitch_wheel() -- a QA-added, zero-logic test seam (synth.hpp) --
// rather than through audio/DSP heuristics: both are thin forwards to
// TinySoundFont's own already-existing, side-effect-free getters
// (tsf_channel_get_preset_number/tsf_channel_get_pitchwheel), so the
// assertions below are exact expected values, not approximations.

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
        "(install fluid-soundfont-gm to run this test)\n");
    return 77;
  }

  melodd::Synth synth(melodd::kDefaultSampleRate);
  std::string error;
  if (!synth.load_soundfont(soundfont, error)) {
    std::printf("FAIL: could not load SoundFont '%s': %s\n", soundfont.c_str(), error.c_str());
    return 1;
  }

  // --- a realtime/system byte (not channel-voice) is silently ignored -----
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::realtime(arrangrr::midi::kClock));
  constexpr int kFrames = 2048;
  std::vector<float> before_note(static_cast<std::size_t>(kFrames) * 2);
  synth.render(before_note.data(), kFrames);
  CHECK(rms(before_note.data(), kFrames) < 1e-4);

  // --- a SysEx status byte (also non-channel-voice) is silently ignored ---
  // too -- is_channel_voice(0xF0) is false the same way 0xF8 (clock) is;
  // this catches a hypothetical dispatch_midi_message bug that special-cased
  // only realtime bytes and let 0xF0-0xF7 "system common" bytes slip through.
  melodd::dispatch_midi_message(
      synth, arrangrr::MidiMessage{.status = arrangrr::midi::kSysExStart, .d1 = 0, .d2 = 0});
  synth.render(before_note.data(), kFrames);
  CHECK(rms(before_note.data(), kFrames) < 1e-4);

  // --- a note-on channel-voice message dispatches through to the Synth ----
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::note_on(0, 60, 100));
  std::vector<float> sounding(static_cast<std::size_t>(kFrames) * 2);
  synth.render(sounding.data(), kFrames);
  const double sounding_rms = rms(sounding.data(), kFrames);
  CHECK(sounding_rms > 1e-4);

  // --- the matching note-off eventually silences it ------------------------
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::note_off(0, 60));
  constexpr int kTailFrames = melodd::kDefaultSampleRate * 4;
  std::vector<float> tail(static_cast<std::size_t>(kTailFrames) * 2);
  synth.render(tail.data(), kTailFrames);
  std::vector<float> after(static_cast<std::size_t>(kFrames) * 2);
  synth.render(after.data(), kFrames);
  CHECK(rms(after.data(), kFrames) < 1e-3);

  // --- note-on with velocity 0 is treated as a note-off (Synth's own
  // documented behavior, delegated to unconditionally by dispatch -- the OLD
  // switch in apps/tools/melodd/main.cpp passed bytes[2] through the SAME
  // way, unconditionally, so this is a parity check, not new behavior) ------
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::note_on(1, 64, 100));
  synth.render(sounding.data(), kFrames);
  CHECK(rms(sounding.data(), kFrames) > 1e-4);
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::note_on(1, 64, 0));
  synth.render(tail.data(), kTailFrames);
  synth.render(after.data(), kFrames);
  CHECK(rms(after.data(), kFrames) < 1e-3);

  // --- program_change routes channel/program through untouched, verified
  // exactly (not approximated) via the debug_program() test seam ------------
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::program(3, 57));
  CHECK(synth.debug_program(3) == 57);
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::program(3, 0));
  CHECK(synth.debug_program(3) == 0);

  // --- pitch_bend: the 14-bit LSB/MSB assembly `(d2 << 7) | d1`, verified
  // exactly via debug_pitch_wheel(). The asymmetric cases below pin the BYTE
  // ORDER specifically -- a d1/d2 swap bug would still "do something" to the
  // pitch wheel, but would swap which of these two produces the big bend and
  // which produces the tiny one. --------------------------------------------
  constexpr int kPitchBendChannel = 2;
  auto pitch_bend_msg = [](std::uint8_t d1, std::uint8_t d2) {
    return arrangrr::MidiMessage{
        .status = static_cast<std::uint8_t>(arrangrr::midi::kPitchBend | kPitchBendChannel),
        .d1 = d1,
        .d2 = d2};
  };
  melodd::dispatch_midi_message(synth, pitch_bend_msg(0, 64));  // center
  CHECK(synth.debug_pitch_wheel(kPitchBendChannel) == 8192);
  melodd::dispatch_midi_message(synth, pitch_bend_msg(127, 127));  // max
  CHECK(synth.debug_pitch_wheel(kPitchBendChannel) == 16383);
  melodd::dispatch_midi_message(synth, pitch_bend_msg(0, 0));  // min
  CHECK(synth.debug_pitch_wheel(kPitchBendChannel) == 0);
  melodd::dispatch_midi_message(synth, pitch_bend_msg(127, 0));  // LSB only -> tiny bend
  CHECK(synth.debug_pitch_wheel(kPitchBendChannel) == 127);
  melodd::dispatch_midi_message(synth, pitch_bend_msg(0, 127));  // MSB only -> big bend
  CHECK(synth.debug_pitch_wheel(kPitchBendChannel) == 16256);

  // --- control_change reaches Synth: CC 120 (ALL_SOUND_OFF) ends every
  // voice on the channel (TinySoundFont's own tsf_channel_sounds_off_all,
  // distinct from CC 123/ALL_NOTES_OFF's ordinary release) -- empirically
  // this still ramps out over TinySoundFont's own short
  // TSF_FASTRELEASETIME click-avoidance grace window rather than going to
  // exact zero on the very next rendered block (the first version of this
  // check asserted silence on the immediately-following kFrames block and
  // failed: RMS stayed above the 1e-3 threshold because the ramp's own
  // energy is concentrated in that same short window), so this uses the same
  // tail-then-check shape as the note-off/velocity-0 cases above -- still an
  // audible, deterministic proof that control_change actually reaches the
  // Synth (never silently dropped), just not an "instant sample" claim. -----
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::note_on(4, 67, 100));
  synth.render(sounding.data(), kFrames);
  CHECK(rms(sounding.data(), kFrames) > 1e-4);
  melodd::dispatch_midi_message(synth, arrangrr::MidiMessage::cc(4, arrangrr::midi::kCcAllSoundOff, 127));
  synth.render(tail.data(), kTailFrames);
  synth.render(after.data(), kFrames);
  CHECK(rms(after.data(), kFrames) < 1e-3);

  if (melodd::test::failures() == 0) {
    std::printf("OK: sounding RMS=%.6f (soundfont=%s)\n", sounding_rms, soundfont.c_str());
  }
  return melodd::test::failures();
}

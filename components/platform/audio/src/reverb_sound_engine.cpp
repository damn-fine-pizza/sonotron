#include "audio/reverb_sound_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "melodd/synth.hpp"

namespace sonotron::audio {

void ReverbSoundEngine::CombFilter::configure(int length_samples, float feedback,
                                              float damp) noexcept {
  m_length = length_samples;
  m_feedback = feedback;
  m_damp1 = damp;
  m_damp2 = 1.0f - damp;
  m_buffer.fill(0.0f);
  m_index = 0;
  m_filter_store = 0.0f;
}

float ReverbSoundEngine::CombFilter::process(float input) noexcept {
  const float output = m_buffer[m_index];
  m_filter_store = (output * m_damp2) + (m_filter_store * m_damp1);
  m_buffer[m_index] = input + (m_filter_store * m_feedback);
  m_index = (m_index + 1 >= m_length) ? 0 : m_index + 1;
  return output;
}

void ReverbSoundEngine::AllpassFilter::configure(int length_samples) noexcept {
  m_length = length_samples;
  m_buffer.fill(0.0f);
  m_index = 0;
}

float ReverbSoundEngine::AllpassFilter::process(float input) noexcept {
  const float buffered = m_buffer[m_index];
  const float output = -input + buffered;
  m_buffer[m_index] = input + (buffered * kFeedback);
  m_index = (m_index + 1 >= m_length) ? 0 : m_index + 1;
  return output;
}

void ReverbSoundEngine::ChorusVoice::configure(float sample_rate, float rate_hz, float depth_ms,
                                               float phase_offset) noexcept {
  m_phase = phase_offset;
  m_phase_increment = 2.0f * std::numbers::pi_v<float> * rate_hz / sample_rate;
  m_depth_samples = depth_ms * 0.001f * sample_rate;
  m_center_samples = kCenterMs * 0.001f * sample_rate;
  m_write_index = 0;
  m_buffer.fill(0.0f);
}

float ReverbSoundEngine::ChorusVoice::process(float input) noexcept {
  m_buffer[m_write_index] = input;
  const float lfo = std::sin(m_phase);
  const float two_pi = 2.0f * std::numbers::pi_v<float>;
  m_phase += m_phase_increment;
  if (m_phase >= two_pi) {
    m_phase -= two_pi;
  }
  const float delay_samples = m_center_samples + lfo * m_depth_samples;
  float read_pos = static_cast<float>(m_write_index) - delay_samples;
  while (read_pos < 0.0f) {
    read_pos += static_cast<float>(kBufferSize);
  }
  const int index0 = static_cast<int>(read_pos) % kBufferSize;
  const int index1 = (index0 + 1) % kBufferSize;
  const float frac = read_pos - static_cast<float>(static_cast<int>(read_pos));
  const float sample = m_buffer[index0] * (1.0f - frac) + m_buffer[index1] * frac;
  m_write_index = (m_write_index + 1 >= kBufferSize) ? 0 : m_write_index + 1;
  return sample;
}

ReverbSoundEngine::ReverbSoundEngine(audio_engine::ISoundEngine& inner)
    : ReverbSoundEngine(inner, Params{}) {}

ReverbSoundEngine::ReverbSoundEngine(audio_engine::ISoundEngine& inner, const Params& params)
    : m_inner(inner), m_params(params) {
  configure_reverb(m_params);

  const float sample_rate = static_cast<float>(melodd::kDefaultSampleRate);
  // Independent L/R LFO phase (90 degrees apart) is what gives the chorus
  // its stereo width instead of just amplitude-modulating a mono signal.
  m_chorus_left.configure(sample_rate, m_params.chorus_rate_hz, m_params.chorus_depth_ms, 0.0f);
  m_chorus_right.configure(sample_rate, m_params.chorus_rate_hz, m_params.chorus_depth_ms,
                           std::numbers::pi_v<float> / 2.0f);

  std::snprintf(m_name, sizeof(m_name), "reverb(%s)", m_inner.name());
}

void ReverbSoundEngine::configure_reverb(const Params& params) noexcept {
  // Freeverb's own room-size/damping -> feedback/damp mapping (Jezar's
  // reference tuning constants), clamped defensively since Params is a
  // public struct a future caller could hand bad values to.
  const float room_size = std::clamp(params.room_size, 0.0f, 1.0f);
  const float damping = std::clamp(params.damping, 0.0f, 1.0f);
  const float wet_level = std::clamp(params.wet_level, 0.0f, 1.0f);
  const float width = std::clamp(params.width, 0.0f, 1.0f);

  constexpr float kScaleRoom = 0.28f;
  constexpr float kOffsetRoom = 0.7f;
  constexpr float kScaleDamp = 0.4f;
  const float feedback = room_size * kScaleRoom + kOffsetRoom;
  const float damp = damping * kScaleDamp;

  // Classic 8-comb-per-channel Jezar Freeverb tuning (samples @ 44100 Hz),
  // right channel offset by the fixed "stereo spread" of 23 samples so left
  // and right tails decorrelate instead of sounding like a mono reverb
  // panned center.
  constexpr int kStereoSpread = 23;
  constexpr std::array<int, 8> kCombTuningLeft = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
  for (std::size_t i = 0; i < kCombTuningLeft.size(); ++i) {
    m_comb_left[i].configure(kCombTuningLeft[i], feedback, damp);
    m_comb_right[i].configure(kCombTuningLeft[i] + kStereoSpread, feedback, damp);
  }

  constexpr std::array<int, 4> kAllpassTuningLeft = {556, 441, 341, 225};
  for (std::size_t i = 0; i < kAllpassTuningLeft.size(); ++i) {
    m_allpass_left[i].configure(kAllpassTuningLeft[i]);
    m_allpass_right[i].configure(kAllpassTuningLeft[i] + kStereoSpread);
  }

  // Wet/dry crossfade (simplified vs raw Freeverb's additive "wet1/wet2"
  // scaling, which assumes a small raw wet gain, not a 0..1 fraction): here
  // wet_level IS the fraction of the mix, so dry_gain = 1 - wet_level keeps
  // overall output bounded instead of piling wet on top of a fixed dry gain.
  m_wet1 = wet_level * (0.5f + 0.5f * width);
  m_wet2 = wet_level * (0.5f - 0.5f * width);
  m_dry_gain = 1.0f - wet_level;
}

void ReverbSoundEngine::dispatch(const arrangrr::MidiMessage& msg) noexcept {
  m_inner.dispatch(msg);
}

void ReverbSoundEngine::all_notes_off() noexcept { m_inner.all_notes_off(); }

const char* ReverbSoundEngine::name() const noexcept { return m_name; }

void ReverbSoundEngine::render(float* out, int frame_count) noexcept {
  m_inner.render(out, frame_count);
  if (out == nullptr || frame_count <= 0) {
    return;
  }
  // Freeverb input gain: 8 parallel combs summed would otherwise overload --
  // this is Jezar's reference "fixedgain" constant.
  constexpr float kCombInputGain = 0.015f;

  for (int frame = 0; frame < frame_count; ++frame) {
    float* sample = out + (static_cast<std::size_t>(frame) * 2);
    const float dry_l = sample[0];
    const float dry_r = sample[1];

    // Light chorus, blended into the pre-reverb signal so the tail itself
    // inherits a bit of the shimmer (chorus_mix defaults small -- subtle).
    const float chorus_l = m_chorus_left.process(dry_l);
    const float chorus_r = m_chorus_right.process(dry_r);
    const float thick_l = dry_l * (1.0f - m_params.chorus_mix) + chorus_l * m_params.chorus_mix;
    const float thick_r = dry_r * (1.0f - m_params.chorus_mix) + chorus_r * m_params.chorus_mix;

    // Freeverb topology: mono-summed input into 8 parallel combs per
    // channel, summed, then diffused through 4 serial allpass filters.
    const float comb_input = (thick_l + thick_r) * kCombInputGain;
    float wet_l = 0.0f;
    float wet_r = 0.0f;
    for (auto& comb : m_comb_left) {
      wet_l += comb.process(comb_input);
    }
    for (auto& comb : m_comb_right) {
      wet_r += comb.process(comb_input);
    }
    for (auto& allpass : m_allpass_left) {
      wet_l = allpass.process(wet_l);
    }
    for (auto& allpass : m_allpass_right) {
      wet_r = allpass.process(wet_r);
    }

    float out_l = thick_l * m_dry_gain + wet_l * m_wet1 + wet_r * m_wet2;
    float out_r = thick_r * m_dry_gain + wet_r * m_wet1 + wet_l * m_wet2;

    // Defensive clamp: this DSP network is stable by construction (every
    // feedback coefficient is < 1), but clamping the final sample is cheap
    // insurance against an out-of-range Params the interface's own comment
    // never promises to validate.
    sample[0] = std::clamp(out_l, -1.0f, 1.0f);
    sample[1] = std::clamp(out_r, -1.0f, 1.0f);
  }
}

}  // namespace sonotron::audio

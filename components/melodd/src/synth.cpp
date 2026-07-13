#include "melodd/synth.hpp"

#include <cstring>
#include <utility>

#include <tsf.h>

namespace melodd {

namespace {
// GM percussion lives on MIDI channel 10, which is index 9 in a 0-indexed
// channel number.
constexpr int kGmPercussionChannel = 9;
constexpr int kGmChannelCount = 16;
}  // namespace

Synth::Synth(int sample_rate) : m_sample_rate(sample_rate > 0 ? sample_rate : 44100) {}

Synth::~Synth() {
  if (m_tsf != nullptr) {
    tsf_close(m_tsf);
  }
}

Synth::Synth(Synth&& other) noexcept
    : m_tsf(std::exchange(other.m_tsf, nullptr)), m_sample_rate(other.m_sample_rate) {}

Synth& Synth::operator=(Synth&& other) noexcept {
  if (this != &other) {
    if (m_tsf != nullptr) {
      tsf_close(m_tsf);
    }
    m_tsf = std::exchange(other.m_tsf, nullptr);
    m_sample_rate = other.m_sample_rate;
  }
  return *this;
}

bool Synth::load_soundfont(const std::string& path, std::string& error) {
  tsf* loaded = tsf_load_filename(path.c_str());
  if (loaded == nullptr) {
    error = "failed to load SoundFont: " + path;
    return false;
  }
  if (m_tsf != nullptr) {
    tsf_close(m_tsf);
  }
  m_tsf = loaded;
  tsf_set_output(m_tsf, TSF_STEREO_INTERLEAVED, m_sample_rate, 0.0f);
  reset_gm_channel_defaults();
  return true;
}

void Synth::reset_gm_channel_defaults() {
  if (m_tsf == nullptr) {
    return;
  }
  for (int channel = 0; channel < kGmChannelCount; ++channel) {
    const int flag_mididrums = (channel == kGmPercussionChannel) ? 1 : 0;
    tsf_channel_set_presetnumber(m_tsf, channel, 0, flag_mididrums);
  }
}

void Synth::note_on(int channel, int key, int velocity) {
  if (m_tsf == nullptr) {
    return;
  }
  if (velocity <= 0) {
    note_off(channel, key);
    return;
  }
  const float vel = static_cast<float>(velocity) / 127.0f;
  tsf_channel_note_on(m_tsf, channel, key, vel);
}

void Synth::note_off(int channel, int key) {
  if (m_tsf == nullptr) {
    return;
  }
  tsf_channel_note_off(m_tsf, channel, key);
}

void Synth::program_change(int channel, int program) {
  if (m_tsf == nullptr) {
    return;
  }
  const int flag_mididrums = (channel == kGmPercussionChannel) ? 1 : 0;
  tsf_channel_set_presetnumber(m_tsf, channel, program, flag_mididrums);
}

void Synth::pitch_bend(int channel, int value14) {
  if (m_tsf == nullptr) {
    return;
  }
  tsf_channel_set_pitchwheel(m_tsf, channel, value14);
}

void Synth::control_change(int channel, int controller, int value) {
  if (m_tsf == nullptr) {
    return;
  }
  tsf_channel_midi_control(m_tsf, channel, controller, value);
}

void Synth::all_notes_off() {
  if (m_tsf == nullptr) {
    return;
  }
  for (int channel = 0; channel < kGmChannelCount; ++channel) {
    tsf_channel_sounds_off_all(m_tsf, channel);
  }
}

void Synth::render(float* out, int frame_count) {
  if (out == nullptr || frame_count <= 0) {
    return;
  }
  if (m_tsf == nullptr) {
    std::memset(out, 0, static_cast<std::size_t>(frame_count) * 2 * sizeof(float));
    return;
  }
  tsf_render_float(m_tsf, out, frame_count, 0);
}

}  // namespace melodd

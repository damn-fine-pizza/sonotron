#include "audio_engine.hpp"

#include <cstdio>

#include "melodd/dispatch.hpp"

namespace sonotron {

AudioEngine::AudioEngine() : m_synth(melodd::kDefaultSampleRate) {
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = static_cast<ma_uint32>(melodd::kDefaultSampleRate);
  config.dataCallback = &AudioEngine::data_callback;
  config.pUserData = this;

  if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
    std::fprintf(stderr,
                 "sonotron: gui_sonotron_audio: cannot open the audio playback device -- "
                 "running without sound\n");
    return;
  }
  if (ma_device_start(&m_device) != MA_SUCCESS) {
    std::fprintf(stderr,
                 "sonotron: gui_sonotron_audio: cannot start the audio playback device -- "
                 "running without sound\n");
    ma_device_uninit(&m_device);
    return;
  }
  m_device_ready = true;
}

AudioEngine::~AudioEngine() {
  // Stop the device FIRST: once ma_device_uninit() returns, data_callback()
  // can no longer fire, so the panic below needs no lock for correctness --
  // taken anyway, cheap and uncontended, to keep every Synth-state mutation
  // going through the one mutex uniformly (Decision 1/5: all_notes_off() on
  // shutdown fixes the hung-note-on-close gap Corelli's review flagged).
  if (m_device_ready) {
    ma_device_uninit(&m_device);
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_synth.all_notes_off();
}

bool AudioEngine::load_soundfont(const std::string& path, std::string& error) {
  tsf* loaded = melodd::Synth::read_soundfont_file(path, error);
  if (loaded == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_synth.adopt_soundfont(loaded);
  return true;
}

void AudioEngine::data_callback(ma_device* device, void* output, const void* /*input*/,
                                ma_uint32 frame_count) {
  auto* self = static_cast<AudioEngine*>(device->pUserData);
  self->render(static_cast<float*>(output), static_cast<int>(frame_count));
}

void AudioEngine::render(float* out, int frame_count) {
  // Drain-and-dispatch, THEN render, all under ONE short lock (Decision 1):
  // the ring's try_pop() is itself lock-free/wait-free (the engine-thread
  // PRODUCER never touches this mutex, only try_push), so nothing here can
  // stall the engine thread. This mutex is only ever contended by a rare
  // GUI-thread load_soundfont()/all_notes_off() call.
  std::lock_guard<std::mutex> lock(m_mutex);
  AudioMidiEvent ev;
  while (m_note_ring.try_pop(ev)) {
    melodd::dispatch_midi_message(m_synth, ev.msg);
  }
  m_synth.render(out, frame_count);
}

}  // namespace sonotron

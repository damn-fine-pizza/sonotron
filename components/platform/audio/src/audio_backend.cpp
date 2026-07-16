#include "audio/audio_backend.hpp"

#include <cstdio>

namespace sonotron::audio {

AudioBackend::AudioBackend(audio_engine::ISoundEngine& engine, int sample_rate) : m_engine(engine) {
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = static_cast<ma_uint32>(sample_rate);
  config.dataCallback = &AudioBackend::data_callback;
  config.pUserData = this;

  if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
    std::fprintf(stderr,
                 "sonotron: audio: cannot open the audio playback device -- "
                 "running without sound\n");
    return;
  }
  if (ma_device_start(&m_device) != MA_SUCCESS) {
    std::fprintf(stderr,
                 "sonotron: audio: cannot start the audio playback device -- "
                 "running without sound\n");
    ma_device_uninit(&m_device);
    return;
  }
  m_device_ready = true;
}

AudioBackend::~AudioBackend() {
  // Stop the device FIRST: once ma_device_uninit() returns, data_callback()
  // can no longer fire, so the panic below needs no lock for correctness --
  // taken anyway, cheap and uncontended, to keep every engine-state mutation
  // going through the one mutex uniformly (all_notes_off() on shutdown fixes
  // the hung-note-on-close gap the original Phase-6 review flagged).
  if (m_device_ready) {
    ma_device_uninit(&m_device);
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_engine.all_notes_off();
}

void AudioBackend::data_callback(ma_device* device, void* output, const void* /*input*/,
                                 ma_uint32 frame_count) {
  auto* self = static_cast<AudioBackend*>(device->pUserData);
  self->render(static_cast<float*>(output), static_cast<int>(frame_count));
}

void AudioBackend::render(float* out, int frame_count) {
  // Drain-and-dispatch, THEN render, all under ONE short lock: the ring's
  // try_pop() is itself lock-free/wait-free (the engine-thread PRODUCER
  // never touches this mutex, only try_push), so nothing here can stall the
  // engine thread. This mutex is only ever contended by a rare GUI-thread
  // config-swap (render_mutex())/all_notes_off() call.
  std::lock_guard<std::mutex> lock(m_mutex);
  AudioMidiEvent ev;
  while (m_note_ring.try_pop(ev)) {
    m_engine.dispatch(ev.msg);
  }
  m_engine.render(out, frame_count);
}

}  // namespace sonotron::audio

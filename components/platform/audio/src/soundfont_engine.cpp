#include "audio/soundfont_engine.hpp"

#include "melodd/dispatch.hpp"

namespace sonotron::audio {

SoundfontEngine::SoundfontEngine(int sample_rate) : m_synth(sample_rate) {}

void SoundfontEngine::dispatch(const arrangrr::MidiMessage& msg) noexcept {
  melodd::dispatch_midi_message(m_synth, msg);
}

void SoundfontEngine::all_notes_off() noexcept { m_synth.all_notes_off(); }

void SoundfontEngine::render(float* out, int frame_count) noexcept {
  m_synth.render(out, frame_count);
}

const char* SoundfontEngine::name() const noexcept { return "soundfont-melodd"; }

bool SoundfontEngine::load(const std::string& path, std::string& error, std::mutex& render_mutex) {
  tsf* loaded = melodd::Synth::read_soundfont_file(path, error);
  if (loaded == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(render_mutex);
  m_synth.adopt_soundfont(loaded);
  return true;
}

}  // namespace sonotron::audio

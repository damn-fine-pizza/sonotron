#pragma once

#include <mutex>
#include <string>

#include "audio_engine/i_sound_engine.hpp"
#include "melodd/synth.hpp"

// SoundfontEngine (docs/proposals/isoundengine-contract.md, Corelli §1/§8):
// the first concrete ISoundEngine, wrapping melodd::Synth -- a General MIDI
// SoundFont realizer. HOST-ONLY, forever: melodd::Synth::load_soundfont
// takes std::string paths and does real file I/O, a hard, permanent
// dependency on a filesystem (Corelli §4). No promotion path exists or is
// claimed for this concrete engine (the interface it implements is a
// separate, core-capable question -- see components/core/audio_engine).

namespace sonotron::audio {

class SoundfontEngine : public audio_engine::ISoundEngine {
 public:
  explicit SoundfontEngine(int sample_rate = melodd::kDefaultSampleRate);

  void dispatch(const arrangrr::MidiMessage& msg) noexcept override;
  void all_notes_off() noexcept override;
  void render(float* out, int frame_count) noexcept override;
  const char* name() const noexcept override;

  // Reads `path` from disk (slow -- GUI thread only, never called from the
  // audio callback) then swaps it in under `render_mutex` (typically
  // AudioBackend::render_mutex() -- the caller supplies whichever mutex
  // serializes against the render/dispatch triad, per Corelli §2's
  // render_mutex() exposure pattern). Returns false (with `error` set) on a
  // read failure; any previously loaded SoundFont is kept in that case.
  bool load(const std::string& path, std::string& error, std::mutex& render_mutex);

 private:
  melodd::Synth m_synth;
};

}  // namespace sonotron::audio

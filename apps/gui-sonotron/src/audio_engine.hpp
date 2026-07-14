#pragma once

#include <mutex>
#include <string>

#include <miniaudio.h>

#include "audio_midi_event.hpp"
#include "melodd/synth.hpp"

// gui_sonotron_audio (Phase-6 Theme 2, docs/phase6-design-reviews.md "Audio
// in the standalone GUI"): a HOST-ONLY, arrangrr-free library -- links
// melodd, miniaudio, gui_sonotron_ring and Threads, nothing else (Decision
// 2). It never includes arrangrr/abi.hpp and never links hostrt/arrangrr/
// runtime, so it stays outside arrangrr's PUBLIC -fno-exceptions -fno-rtti
// regime -- gui_sonotron_engine (apps/gui-sonotron/CMakeLists.txt) remains
// "this library, and only this library, touches arrangrr".

namespace sonotron {

// AudioEngine: melodd::Synth + a miniaudio playback device + the AudioMidi
// ring's consumer + the load/panic mutex (Decision 1/2). Owned by main() in
// integrated mode only (control_path.empty()), declared AFTER
// brain_session_holder so it is destroyed BEFORE the engine thread is
// joined (Decision 5).
//
// Thread-safety (Decision 1/4): note_ring() is the ONLY thing the engine
// thread (producer) ever touches concurrently with this class -- SpscRing's
// own contract makes that safe with no lock, and the engine thread NEVER
// takes m_mutex. m_mutex guards exactly the triad {the ma_device callback's
// ring-drain + dispatch + melodd::Synth::render(), adopt_soundfont()'s
// pointer-swap tail, all_notes_off()} -- the rare, whole-Synth-state
// mutations -- never the ring hand-off itself. load_soundfont()'s disk read
// (melodd::Synth::read_soundfont_file) runs OUTSIDE the mutex, on the
// calling (GUI) thread -- only the fast state-swap tail
// (melodd::Synth::adopt_soundfont) is guarded.
class AudioEngine {
 public:
  AudioEngine();
  ~AudioEngine();

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;
  AudioEngine(AudioEngine&&) = delete;
  AudioEngine& operator=(AudioEngine&&) = delete;

  // Producer-side handle: InProcessBrainSession::set_audio_ring() stores
  // this pointer and pushes translated channel-voice events from the
  // engine thread. Safe to call/store even if the audio device failed to
  // open below (device_ready() false) -- pushes just accumulate a ring no
  // one drains, harmless under the ring's own SPSC contract.
  AudioMidiRing& note_ring() noexcept { return m_note_ring; }

  // Reads `path` from disk (slow -- GUI thread only, never called from the
  // audio callback) then swaps it in under m_mutex (Decision 4). Returns
  // false (with `error` set) on a read failure; any previously loaded
  // SoundFont is kept in that case.
  bool load_soundfont(const std::string& path, std::string& error);

  // False if the playback device failed to open/start (logged to stderr at
  // construction time) -- not fatal, mirrors AlsaMidi's own "log and run
  // silently" pattern in in_process_brain_session.cpp.
  bool device_ready() const noexcept { return m_device_ready; }

 private:
  static void data_callback(ma_device* device, void* output, const void* input,
                            ma_uint32 frame_count);
  void render(float* out, int frame_count);

  std::mutex m_mutex;
  melodd::Synth m_synth;
  AudioMidiRing m_note_ring;
  ma_device m_device{};
  bool m_device_ready = false;
};

}  // namespace sonotron

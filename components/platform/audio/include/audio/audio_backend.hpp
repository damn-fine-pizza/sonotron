#pragma once

#include <mutex>

#include <miniaudio.h>

#include "audio/audio_midi_event.hpp"
#include "audio_engine/i_sound_engine.hpp"

// audio (Phase-6 Theme 2 origin, docs/phase6-design-reviews.md "Audio in
// the standalone GUI"; generalized per docs/proposals/
// isoundengine-contract.md, Corelli §2): a HOST-ONLY, arrangrr-free
// library -- links audio_ring, miniaudio and Threads, nothing else. It
// never includes arrangrr/abi.hpp and never links hostrt/arrangrr/runtime,
// so it stays outside arrangrr's PUBLIC -fno-exceptions -fno-rtti regime --
// gui_sonotron_engine (apps/gui-sonotron/CMakeLists.txt) remains "this
// library, and only this library, touches arrangrr".

namespace sonotron::audio {

// AudioBackend: the concrete device layer (Corelli §2 -- an IAudioBackend
// abstract base was considered and rejected: miniaudio itself is already
// the cross-platform device abstraction, and there is exactly one
// implementation in sight). Owns a miniaudio playback device + the
// AudioMidi ring's consumer + the render/panic mutex; holds a REFERENCE to
// whichever concrete ISoundEngine the composition root chose, so the
// render/dispatch/panic triad itself becomes engine-blind (this is the
// generalization of what used to be a directly-owned melodd::Synth
// member).
//
// Ownership is single-shot, not hot-swappable: the engine reference is
// chosen once at construction by the composition root (today main.cpp) and
// never swapped at runtime -- live engine-switching is a materially bigger
// feature, out of scope here (Corelli §2, NEEDS-DECISION if ever wanted).
//
// Thread-safety: note_ring() is the ONLY thing the producer thread (the
// engine thread pushing translated MIDI) ever touches concurrently with
// this class -- SpscRing's own contract makes that safe with no lock, and
// the producer thread NEVER takes m_mutex. m_mutex guards exactly the
// triad {the ma_device callback's ring-drain + dispatch + engine.render(),
// a concrete engine's own config-swap tail via render_mutex(),
// all_notes_off()} -- the rare, whole-engine-state mutations -- never the
// ring hand-off itself.
class AudioBackend {
 public:
  explicit AudioBackend(audio_engine::ISoundEngine& engine, int sample_rate = 44100);
  ~AudioBackend();

  AudioBackend(const AudioBackend&) = delete;
  AudioBackend& operator=(const AudioBackend&) = delete;
  AudioBackend(AudioBackend&&) = delete;
  AudioBackend& operator=(AudioBackend&&) = delete;

  // Producer-side handle: InProcessBrainSession::set_audio_ring() stores
  // this pointer and pushes translated channel-voice events from the
  // engine thread. Safe to call/store even if the audio device failed to
  // open below (device_ready() false) -- pushes just accumulate a ring no
  // one drains, harmless under the ring's own SPSC contract.
  AudioMidiRing& note_ring() noexcept { return m_note_ring; }

  // False if the playback device failed to open/start (logged to stderr at
  // construction time) -- not fatal, mirrors AlsaMidi's own "log and run
  // silently" pattern in in_process_brain_session.cpp.
  bool device_ready() const noexcept { return m_device_ready; }

  // The ONE thing AudioBackend exposes beyond render/dispatch/panic: a
  // handle a concrete engine's OWN configuration path (e.g.
  // SoundfontEngine::load(path)) locks to serialize a slow state-swap
  // against render() -- mirrors the single mutex this class already
  // covers {render(), a config-swap tail, all_notes_off()}. AudioBackend
  // does not know WHAT is being swapped; it only serializes against the
  // render callback (Corelli §2).
  std::mutex& render_mutex() noexcept { return m_mutex; }

 private:
  static void data_callback(ma_device* device, void* output, const void* input,
                            ma_uint32 frame_count);
  void render(float* out, int frame_count);

  std::mutex m_mutex;
  audio_engine::ISoundEngine& m_engine;  // NOT owned -- composition root owns the lifetime
  AudioMidiRing m_note_ring;
  ma_device m_device{};
  bool m_device_ready = false;
};

}  // namespace sonotron::audio

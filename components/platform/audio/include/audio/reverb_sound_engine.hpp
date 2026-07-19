#pragma once

#include <array>
#include <cstddef>

#include "audio_engine/i_sound_engine.hpp"

// ReverbSoundEngine (Phase-1 sound task #7, docs/proposals/
// audio-engine-fluidsynth-build-vs-buy.md SS6/SS7 "B1"): a Freeverb-style
// reverb + light chorus DECORATOR over any other ISoundEngine, closing the
// dominant "instruments sound synthetic" gap -- TSF/melodd's render path has
// zero ambience (no reverb/chorus send bus, see that proposal's SS1). Wraps
// an inner ISoundEngine& (today: sonotron::audio::SoundfontEngine) the same
// "wrap, don't absorb" way SoundfontEngine itself wraps melodd::Synth:
// dispatch()/all_notes_off() forward unchanged, render() calls the inner
// engine THEN applies the DSP in place on the same buffer.
//
// Reverb is ALWAYS-ON for Phase-1 (no bypass/toggle -- that is a later UX
// call, out of scope here). Params exists only so a future GUI toggle/tuning
// surface has something to construct against, not because Phase-1 wires
// one up.
//
// HOST-ONLY (same regime as SoundfontEngine/AudioBackend -- this whole
// component never reaches the arm-none-eabi branch). No heap: every delay
// line is a fixed-size std::array sized for melodd::kDefaultSampleRate
// (44100 Hz). Not internally thread-safe, same "caller serializes
// dispatch()/render()/all_notes_off()" contract every other ISoundEngine in
// this codebase already documents.

namespace sonotron::audio {

class ReverbSoundEngine : public audio_engine::ISoundEngine {
 public:
  struct Params {
    float room_size = 0.5f;        // 0..1 -> comb feedback (Freeverb mapping)
    float damping = 0.4f;          // 0..1 -> HF damping in the comb feedback loop
    float wet_level = 0.22f;       // 0..1 -> overall wet/dry crossfade (~15-25% wet)
    float width = 1.0f;            // 0..1 -> stereo spread of the wet signal
    float chorus_mix = 0.12f;      // 0..1 -> how much chorused signal blends into
                                   // the pre-reverb dry signal (subtle, not deep)
    float chorus_rate_hz = 0.6f;   // chorus LFO rate
    float chorus_depth_ms = 4.0f;  // chorus modulation depth, milliseconds
  };

  // Split into two constructors, NOT one with a `= Params{}` default
  // argument: Params is a nested type whose default member initializers are
  // a complete-class context deferred to the end of the ENCLOSING class
  // (ReverbSoundEngine) per the standard's nested-class rules -- both GCC
  // and Clang reject `Params{}` used inline as a default argument value
  // here ("default member initializer ... required before the end of its
  // enclosing class"). The single-argument overload delegates to the
  // two-argument one, giving the exact same call-site behavior
  // (`ReverbSoundEngine(inner)` still means "use Params{}'s defaults").
  explicit ReverbSoundEngine(audio_engine::ISoundEngine& inner);
  explicit ReverbSoundEngine(audio_engine::ISoundEngine& inner, const Params& params);

  void dispatch(const arrangrr::MidiMessage& msg) noexcept override;
  void all_notes_off() noexcept override;
  void render(float* out, int frame_count) noexcept override;
  const char* name() const noexcept override;

 private:
  // Freeverb-style comb filter: parallel bank member, own damped one-pole
  // lowpass in its feedback path (the "damping" control). Fixed max-length
  // ring buffer, actual active length set once in configure().
  class CombFilter {
   public:
    void configure(int length_samples, float feedback, float damp) noexcept;
    float process(float input) noexcept;

   private:
    static constexpr int kMaxLength = 1700;  // >= largest tuning (1640) + margin
    std::array<float, kMaxLength> m_buffer{};
    int m_length = kMaxLength;
    int m_index = 0;
    float m_feedback = 0.5f;
    float m_damp1 = 0.5f;
    float m_damp2 = 0.5f;
    float m_filter_store = 0.0f;
  };

  // Freeverb-style allpass filter: serial chain member, diffuses the comb
  // bank's output into a smoother tail. Fixed feedback (0.5, the classic
  // Freeverb constant), fixed max-length ring buffer.
  class AllpassFilter {
   public:
    void configure(int length_samples) noexcept;
    float process(float input) noexcept;

   private:
    static constexpr int kMaxLength = 600;  // >= largest tuning (579) + margin
    std::array<float, kMaxLength> m_buffer{};
    int m_length = kMaxLength;
    int m_index = 0;
    static constexpr float kFeedback = 0.5f;
  };

  // Light chorus voice: one LFO-modulated fractional delay line per
  // channel, independent phase offset between L/R for stereo width.
  class ChorusVoice {
   public:
    void configure(float sample_rate, float rate_hz, float depth_ms, float phase_offset) noexcept;
    float process(float input) noexcept;

   private:
    static constexpr int kBufferSize = 2048;   // >> depth+center at 44100 Hz
    static constexpr float kCenterMs = 15.0f;  // fixed center delay
    std::array<float, kBufferSize> m_buffer{};
    int m_write_index = 0;
    float m_phase = 0.0f;
    float m_phase_increment = 0.0f;
    float m_depth_samples = 0.0f;
    float m_center_samples = 0.0f;
  };

  void configure_reverb(const Params& params) noexcept;

  audio_engine::ISoundEngine& m_inner;  // NOT owned -- composition root owns the lifetime
  Params m_params{};
  float m_dry_gain = 0.0f;
  float m_wet1 = 0.0f;
  float m_wet2 = 0.0f;

  std::array<CombFilter, 8> m_comb_left{};
  std::array<CombFilter, 8> m_comb_right{};
  std::array<AllpassFilter, 4> m_allpass_left{};
  std::array<AllpassFilter, 4> m_allpass_right{};
  ChorusVoice m_chorus_left{};
  ChorusVoice m_chorus_right{};

  char m_name[64] = {};
};

}  // namespace sonotron::audio

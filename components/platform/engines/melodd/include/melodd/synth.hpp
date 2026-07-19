#pragma once

#include <string>

// TinySoundFont's opaque struct; only synth.cpp includes tsf.h itself, so
// this header stays a clean realizer interface (see README.md).
struct tsf;

namespace melodd {

// The sample rate every host binary that instantiates a Synth defaults to
// (Phase-6 Theme 2 design review, Decision 5): the single point of change
// for "what sample rate does realization run at" -- apps/tools/melodd and
// sonotron::audio::SoundfontEngine both construct their Synth at this rate
// instead of each hardcoding the literal independently.
inline constexpr int kDefaultSampleRate = 44100;

// Synth: the melodd "realizer" — a General MIDI SoundFont player built on
// TinySoundFont (docs/design/workstation-vision.md's Engines tier). It
// accepts MIDI channel-voice messages and renders interleaved stereo float
// audio frames. Nothing else: one voice per channel/preset, no effects, no
// mixing, no editing surface (0910/D43 scope discipline — the first slice
// stays ruthlessly narrow).
//
// This class knows nothing about ALSA, miniaudio, or arrangrr — it is a
// plain library, HOST-only by build placement but not itself tied to any
// particular transport. apps/tools/melodd is the standalone binary that
// drives it from a MIDI input port and a miniaudio playback device; the
// documented follow-up is an in-process peer that feeds it arrangrr's
// OutEvent stream directly (see README.md).
//
// NOT internally thread-safe: a caller that dispatches MIDI events from one
// thread (e.g. an ALSA input poller) and renders from another (e.g. an
// audio-callback thread) must synchronize its own calls into one Synth
// instance -- see apps/tools/melodd/main.cpp for the reference pattern (one
// mutex guarding every call).
class Synth {
 public:
  explicit Synth(int sample_rate = kDefaultSampleRate);
  ~Synth();

  Synth(const Synth&) = delete;
  Synth& operator=(const Synth&) = delete;
  Synth(Synth&& other) noexcept;
  Synth& operator=(Synth&& other) noexcept;

  // Loads a General MIDI SoundFont (.sf2) from disk and resets every
  // channel to the GM default (program 0 on channels 0-15, the GM
  // percussion kit on channel 9). Returns false (with `error` set) on
  // failure; any previously loaded SoundFont is kept in that case. Does
  // both halves below back to back -- the right call for a single-threaded
  // caller (apps/tools/melodd's main.cpp).
  bool load_soundfont(const std::string& path, std::string& error);

  // Split of load_soundfont() for a caller that renders concurrently on
  // another thread (sonotron::audio::AudioBackend, Phase-6 Theme 2 design
  // review Decision 1/4): the disk read (this function) is slow and must
  // run OUTSIDE whatever lock also guards render(); the state swap
  // (adopt_soundfont() below) is fast and is the only half that actually
  // needs to be serialized against render(). Returns nullptr (with `error`
  // set) on failure. The returned pointer is not yet owned by any Synth --
  // the caller must pass it to adopt_soundfont() (on success) or tsf_close
  // it itself (this header stays tsf-opaque on purpose, so in practice the
  // only correct caller is adopt_soundfont()).
  static tsf* read_soundfont_file(const std::string& path, std::string& error);

  // Takes ownership of `loaded` (a non-null tsf* from read_soundfont_file()),
  // closing any previously loaded SoundFont and resetting every channel to
  // the GM default. Must not be called concurrently with render() on this
  // Synth -- the caller supplies whatever synchronization that requires.
  void adopt_soundfont(tsf* loaded);

  bool loaded() const noexcept { return m_tsf != nullptr; }

  int sample_rate() const noexcept { return m_sample_rate; }

  // MIDI channel-voice messages this realizer honors. `channel` is
  // 0-indexed (0-15); channel 9 is the GM percussion channel.
  void note_on(int channel, int key, int velocity);  // velocity 0 == note_off
  void note_off(int channel, int key);
  void program_change(int channel, int program);  // GM program, 0-127
  void pitch_bend(int channel, int value14);      // 0..16383, 8192 = center
  // Not required by the first-slice spec, but free to route: TinySoundFont
  // natively honors a subset of controllers (sustain, bank-select, pan,
  // volume, all-notes-off...) through the same one call.
  void control_change(int channel, int controller, int value);

  // Silences every voice on every channel immediately (panic / shutdown).
  void all_notes_off();

  // Test/introspection seam (Torquato QA pass, Phase-6 Theme 2 design
  // review, docs/phase6-design-reviews.md "Audio in the standalone GUI"):
  // thin, zero-logic const forwards to already-existing, side-effect-free
  // TinySoundFont queries (tsf_channel_get_preset_number /
  // tsf_channel_get_pitchwheel), added solely so melodd::dispatch_midi_message
  // 's program_change routing and pitch_bend's exact 14-bit LSB/MSB assembly
  // can be pinned deterministically by a unit test without rendering/DSP
  // heuristics. No production call site uses these; flagged to Giotto per
  // QA policy as a minimal, behavior-preserving accessor (request, not a
  // silent addition) -- see components/platform/engines/melodd/tests/test_dispatch.cpp.
  // Blessed (Giotto): const, zero-logic, no side effect -- fits the
  // accessor exception cleanly; kept as-is.
  int debug_program(int channel) const;
  int debug_pitch_wheel(int channel) const;

  // Renders `frame_count` interleaved stereo frames (2 floats/frame,
  // -1..1 range) into `out`. Silent (zeroed) if no SoundFont is loaded.
  void render(float* out, int frame_count);

 private:
  void reset_gm_channel_defaults();

  tsf* m_tsf = nullptr;
  int m_sample_rate;
};

}  // namespace melodd

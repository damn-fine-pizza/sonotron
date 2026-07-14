// melodd: the standalone drop-in-synth binary (components/melodd/README.md).
// Opens an ALSA sequencer MIDI input port named "melodd" and a miniaudio
// playback device; the miniaudio callback pulls rendered frames from a
// melodd::Synth, incoming ALSA MIDI drives that same Synth. A DIRECT
// replacement for the demo launcher's FluidSynth wiring
// (apps/demo/lib/launch.sh): `aconnect sonotron:1 melodd:0` and you hear the
// band, no external synth required. Every arrangrr MIDI-emitting frontend
// presents as ALSA client "sonotron" (components/hostrt/alsa_midi.hpp's
// kAlsaClientName); port 1 is the outbound MIDI port each frontend opens
// right after its inbound port 0 at startup, so it is stable across launches.
//
// HOST-ONLY. This binary talks to melodd::Synth through raw MIDI bytes only
// -- it links neither arrangrr nor hostrt (D43: a realizer does not know its
// callers). Its own MIDI decode (dispatch_message() below) and
// apps/gui-sonotron's in-process peer (gui_sonotron_audio::AudioEngine,
// Phase-6 Theme 2, docs/phase6-design-reviews.md "Audio in the standalone
// GUI") both funnel into the ONE shared melodd::dispatch_midi_message()
// entry point (components/melodd/include/melodd/dispatch.hpp) instead of
// each hand-rolling its own MIDI-status switch.

#include <alsa/asoundlib.h>
#include <miniaudio.h>
#include <poll.h>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "melodd/dispatch.hpp"
#include "melodd/soundfont_discovery.hpp"
#include "melodd/synth.hpp"

namespace {

constexpr const char* kClientName = "melodd";
constexpr const char* kPortName = "melodd";

std::atomic<bool> g_running{true};

void handle_signal(int /*signum*/) { g_running.store(false, std::memory_order_relaxed); }

// melodd::Synth is not internally thread-safe (see its header): the ALSA
// input drives it from the main thread while miniaudio's callback renders
// it from the audio thread, so every access here is guarded by one mutex.
struct AppState {
  explicit AppState(int sample_rate) : synth(sample_rate) {}
  melodd::Synth synth;
  std::mutex mutex;
};

void data_callback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frame_count) {
  auto* app = static_cast<AppState*>(device->pUserData);
  std::lock_guard<std::mutex> lock(app->mutex);
  app->synth.render(static_cast<float*>(output), static_cast<int>(frame_count));
}

// Decodes raw ALSA-delivered bytes into an arrangrr::MidiMessage (dropping
// anything shorter than its status byte's own data length --
// arrangrr::midi::data_length, common/midi/message.hpp -- i.e. a malformed/
// truncated packet, exactly as the previous per-type length guards did) and
// hands it to melodd::dispatch_midi_message, the shared decode both this
// binary and gui_sonotron_audio now call (Phase-6 Theme 2 design review,
// Decision 5).
void dispatch_message(AppState& app, const std::uint8_t* bytes, long len) {
  if (len < 1 || !arrangrr::midi::is_channel_voice(bytes[0])) {
    return;
  }
  const long needed = 1 + arrangrr::midi::data_length(bytes[0]);
  if (len < needed) {
    return;
  }
  const arrangrr::MidiMessage msg{
      .status = bytes[0],
      .d1 = bytes[1],
      .d2 = needed >= 3 ? bytes[2] : std::uint8_t{0},
  };

  std::lock_guard<std::mutex> lock(app.mutex);
  melodd::dispatch_midi_message(app.synth, msg);
}

}  // namespace

int main(int argc, char** argv) {
  std::string soundfont_override;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--soundfont" && i + 1 < argc) {
      soundfont_override = argv[++i];
    } else {
      std::fprintf(stderr, "melodd: unknown argument '%s'\n", arg.c_str());
      std::fprintf(stderr, "usage: melodd [--soundfont <path.sf2>]\n");
      return 2;
    }
  }

  // --- soundfont discovery, mirroring apps/demo/lib/launch.sh -------------
  const std::string soundfont = melodd::find_system_soundfont(soundfont_override);
  if (soundfont.empty()) {
    std::printf("No GM soundfont found under /usr/share/soundfonts. Get one with:\n");
    std::printf("  sudo dnf install fluid-soundfont-gm\n");
    return 2;
  }

  AppState app(melodd::kDefaultSampleRate);
  std::string error;
  if (!app.synth.load_soundfont(soundfont, error)) {
    std::fprintf(stderr, "melodd: %s\n", error.c_str());
    return 1;
  }
  std::printf("melodd: loaded SoundFont '%s'\n", soundfont.c_str());

  // --- ALSA sequencer input port -------------------------------------------
  snd_seq_t* seq = nullptr;
  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
    std::fprintf(stderr, "melodd: cannot open the ALSA sequencer\n");
    return 1;
  }
  snd_seq_set_client_name(seq, kClientName);
  const int port = snd_seq_create_simple_port(
      seq, kPortName, SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_SYNTHESIZER);
  if (port < 0) {
    std::fprintf(stderr, "melodd: cannot create ALSA port '%s'\n", kPortName);
    snd_seq_close(seq);
    return 1;
  }
  snd_midi_event_t* decoder = nullptr;
  if (snd_midi_event_new(16, &decoder) < 0) {
    std::fprintf(stderr, "melodd: cannot allocate the ALSA MIDI decoder\n");
    snd_seq_close(seq);
    return 1;
  }
  snd_midi_event_no_status(decoder, 1);

  // --- miniaudio playback device --------------------------------------------
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = melodd::kDefaultSampleRate;
  config.dataCallback = data_callback;
  config.pUserData = &app;

  ma_device device;
  if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
    std::fprintf(stderr, "melodd: cannot open the audio playback device\n");
    snd_midi_event_free(decoder);
    snd_seq_close(seq);
    return 1;
  }
  if (ma_device_start(&device) != MA_SUCCESS) {
    std::fprintf(stderr, "melodd: cannot start the audio playback device\n");
    ma_device_uninit(&device);
    snd_midi_event_free(decoder);
    snd_seq_close(seq);
    return 1;
  }

  std::printf("melodd: ALSA MIDI input port '%s:0' ready, audio device running.\n", kClientName);
  std::printf("melodd: connect a source, e.g.  aconnect sonotron:1 %s:0\n", kClientName);
  std::printf("melodd: Ctrl-C to quit.\n");

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  // --- drain the ALSA input until interrupted -------------------------------
  const int poll_fd_count = snd_seq_poll_descriptors_count(seq, POLLIN);
  std::vector<pollfd> pfds(static_cast<std::size_t>(poll_fd_count));
  while (g_running.load(std::memory_order_relaxed)) {
    snd_seq_poll_descriptors(seq, pfds.data(), static_cast<unsigned>(pfds.size()), POLLIN);
    const int ready = poll(pfds.data(), pfds.size(), 200);
    if (ready <= 0) {
      continue;  // timeout or EINTR: loop back and re-check g_running
    }
    snd_seq_event_t* ev = nullptr;
    while (snd_seq_event_input(seq, &ev) >= 0 && ev != nullptr) {
      std::uint8_t bytes[16];
      const long n = snd_midi_event_decode(decoder, bytes, sizeof(bytes), ev);
      if (n > 0) {
        dispatch_message(app, bytes, n);
      }
      snd_seq_free_event(ev);
      if (snd_seq_event_input_pending(seq, 0) <= 0) {
        break;
      }
    }
  }

  std::printf("melodd: shutting down.\n");
  ma_device_uninit(&device);
  snd_midi_event_free(decoder);
  snd_seq_close(seq);
  return 0;
}

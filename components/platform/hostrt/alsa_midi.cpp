#include "alsa_midi.hpp"

#include <poll.h>

namespace arrangrr::host {

AlsaMidi::~AlsaMidi() {
  if (m_decoder) {
    snd_midi_event_free(m_decoder);
  }
  if (m_encoder) {
    snd_midi_event_free(m_encoder);
  }
  if (m_seq) {
    snd_seq_close(m_seq);
  }
}

bool AlsaMidi::open(const char* client_name, std::string& error) {
  if (snd_seq_open(&m_seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
    error = "cannot open ALSA sequencer";
    return false;
  }
  snd_seq_set_client_name(m_seq, client_name);
  if (snd_midi_event_new(16, &m_decoder) < 0 || snd_midi_event_new(16, &m_encoder) < 0) {
    error = "cannot allocate ALSA MIDI codecs";
    return false;
  }
  // The decoder must not merge running status away from us.
  snd_midi_event_no_status(m_decoder, 1);
  snd_midi_event_no_status(m_encoder, 1);
  return true;
}

bool AlsaMidi::create_port(const PortDef& def, std::string& error) {
  const unsigned caps = def.is_input ? (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)
                                     : (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
  const int port =
      snd_seq_create_simple_port(m_seq, def.name.c_str(), caps,
                                 SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
  if (port < 0) {
    error = "cannot create ALSA port '" + def.name + "'";
    return false;
  }
  m_ports.push_back(PortMap{port, def.index, def.is_input});
  return true;
}

void AlsaMidi::send(std::uint8_t core_port, const MidiMessage& msg) {
  int alsa_port = -1;
  for (const PortMap& p : m_ports) {
    if (!p.is_input && p.core_index == core_port) {
      alsa_port = p.alsa_port;
      break;
    }
  }
  if (alsa_port < 0) {
    return;
  }

  std::uint8_t bytes[3] = {msg.status, msg.d1, msg.d2};
  const int len = msg.wire_length();
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  snd_midi_event_reset_encode(m_encoder);
  if (snd_midi_event_encode(m_encoder, bytes, len, &ev) < len) {
    return;
  }
  snd_seq_ev_set_source(&ev, static_cast<unsigned char>(alsa_port));
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  snd_seq_event_output_direct(m_seq, &ev);
}

int AlsaMidi::core_port_for(int alsa_port) const {
  for (const PortMap& p : m_ports) {
    if (p.is_input && p.alsa_port == alsa_port) {
      return p.core_index;
    }
  }
  return -1;
}

void AlsaMidi::drain_input(const InputSink& on_bytes) {
  if (!m_seq) {
    return;
  }
  snd_seq_event_t* ev = nullptr;
  while (snd_seq_event_input(m_seq, &ev) >= 0 && ev != nullptr) {
    const int core_port = core_port_for(ev->dest.port);
    if (core_port >= 0) {
      std::uint8_t buf[16];
      const long n = snd_midi_event_decode(m_decoder, buf, sizeof(buf), ev);
      if (n > 0) {
        on_bytes(static_cast<std::uint8_t>(core_port), buf, static_cast<std::size_t>(n));
      }
    }
    snd_seq_free_event(ev);
    if (snd_seq_event_input_pending(m_seq, 0) <= 0) {
      break;
    }
  }
}

int AlsaMidi::poll_fd_count() const {
  return m_seq ? snd_seq_poll_descriptors_count(m_seq, POLLIN) : 0;
}

int AlsaMidi::fill_poll_fds(MidiPollFd* fds, int space) const {
  if (!m_seq || space <= 0) {
    return 0;
  }
  // Fill through a native ALSA-facing buffer, then copy field-by-field into
  // the caller's portable MidiPollFd array -- a small, infrequent (once per
  // ~100 ms poll wakeup) copy that keeps this translation entirely free of
  // any reinterpret_cast/strict-aliasing concern between `struct pollfd` and
  // MidiPollFd, even though the two happen to be layout-compatible today.
  std::vector<struct pollfd> native(static_cast<std::size_t>(space));
  const int n =
      snd_seq_poll_descriptors(m_seq, native.data(), static_cast<unsigned>(space), POLLIN);
  for (int i = 0; i < n; ++i) {
    fds[i].fd = native[static_cast<std::size_t>(i)].fd;
    fds[i].events = native[static_cast<std::size_t>(i)].events;
    fds[i].revents = native[static_cast<std::size_t>(i)].revents;
  }
  return n;
}

}  // namespace arrangrr::host

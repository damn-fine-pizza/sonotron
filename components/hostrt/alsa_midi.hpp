#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <alsa/asoundlib.h>

#include "common/midi/message.hpp"
#include "shell.hpp"

// ALSA sequencer backend: creates virtual MIDI ports (visible to aconnect /
// DAWs), decodes incoming events to raw bytes for the core parser and encodes
// core output messages back to the wire. Host-only, one backend for M0
// (review decision: ALSA first, others behind the same HAL later).

namespace arrangrr::host {

class AlsaMidi {
 public:
  ~AlsaMidi();

  bool open(const char* client_name, std::string& error);
  bool create_port(const PortDef& def, std::string& error);

  void send(std::uint8_t core_port, const MidiMessage& msg);

  // Drains pending input events; calls on_bytes(core_port, bytes, len).
  template <typename Fn>
  void drain_input(Fn&& on_bytes) {
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

  int poll_fd_count() const;
  int fill_poll_fds(struct pollfd* fds, int space) const;

 private:
  int core_port_for(int alsa_port) const;

  snd_seq_t* m_seq = nullptr;
  snd_midi_event_t* m_decoder = nullptr;
  snd_midi_event_t* m_encoder = nullptr;

  struct PortMap {
    int alsa_port;
    std::uint8_t core_index;
    bool is_input;
  };
  std::vector<PortMap> m_ports;
};

}  // namespace arrangrr::host

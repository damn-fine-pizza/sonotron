#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <alsa/asoundlib.h>

#include "common/midi/message.hpp"
#include "midi_hal.hpp"
#include "shell.hpp"

// ALSA sequencer backend: creates virtual MIDI ports (visible to aconnect /
// DAWs), decodes incoming events to raw bytes for the core parser and encodes
// core output messages back to the wire. The Linux implementation of
// IMidiHal (docs/proposals/looper-in-gui-contract.md §7 item 12); see
// midi_hal.hpp for the platform-selected factory every frontend should go
// through instead of naming this class directly.

namespace arrangrr::host {

class AlsaMidi : public IMidiHal {
 public:
  ~AlsaMidi() override;

  bool open(const char* client_name, std::string& error) override;
  bool create_port(const PortDef& def, std::string& error) override;

  void send(std::uint8_t core_port, const MidiMessage& msg) override;

  // Drains pending input events; calls on_bytes(core_port, bytes, len).
  void drain_input(const InputSink& on_bytes) override;

  int poll_fd_count() const override;
  int fill_poll_fds(MidiPollFd* fds, int space) const override;

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

#include "winmm_midi.hpp"

namespace arrangrr::host {

WinMmMidi::~WinMmMidi() {
  for (const InPort& p : m_in_ports) {
    midiInStop(p.handle);
    midiInClose(p.handle);
  }
  for (const OutPort& p : m_out_ports) {
    midiOutClose(p.handle);
  }
}

bool WinMmMidi::open(const char* client_name, std::string& error) {
  (void)error;
  m_client_name = client_name;
  // Unlike ALSA's sequencer client, WinMM has no client-registration call to
  // make here -- each port is bound to a concrete enumerated device
  // individually in create_port(). This always succeeds; a device-less
  // machine simply fails later, per port, in create_port().
  return true;
}

bool WinMmMidi::create_port(const PortDef& def, std::string& error) {
  std::lock_guard<std::mutex> lock(m_ports_mutex);
  if (def.is_input) {
    const UINT device_id = static_cast<UINT>(in_device_count_opened_so_far());
    if (device_id >= midiInGetNumDevs()) {
      error = "no WinMM input device available for port '" + def.name + "'";
      return false;
    }
    HMIDIIN handle = nullptr;
    const MMRESULT rc =
        midiInOpen(&handle, device_id, reinterpret_cast<DWORD_PTR>(&WinMmMidi::midi_in_proc),
                   reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (rc != MMSYSERR_NOERROR || handle == nullptr) {
      error = "cannot open WinMM input device for port '" + def.name + "'";
      return false;
    }
    midiInStart(handle);
    m_in_ports.push_back(InPort{.handle = handle, .core_index = def.index});
    return true;
  }

  const UINT device_id = static_cast<UINT>(out_device_count_opened_so_far());
  if (device_id >= midiOutGetNumDevs()) {
    error = "no WinMM output device available for port '" + def.name + "'";
    return false;
  }
  HMIDIOUT handle = nullptr;
  const MMRESULT rc = midiOutOpen(&handle, device_id, 0, 0, CALLBACK_NULL);
  if (rc != MMSYSERR_NOERROR || handle == nullptr) {
    error = "cannot open WinMM output device for port '" + def.name + "'";
    return false;
  }
  m_out_ports.push_back(OutPort{.handle = handle, .core_index = def.index});
  return true;
}

void WinMmMidi::send(std::uint8_t core_port, const MidiMessage& msg) {
  HMIDIOUT handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_ports_mutex);
    for (const OutPort& p : m_out_ports) {
      if (p.core_index == core_port) {
        handle = p.handle;
        break;
      }
    }
  }
  if (handle == nullptr) {
    return;
  }
  const DWORD packed = static_cast<DWORD>(msg.status) | (static_cast<DWORD>(msg.d1) << 8) |
                       (static_cast<DWORD>(msg.d2) << 16);
  midiOutShortMsg(handle, packed);
}

void CALLBACK WinMmMidi::midi_in_proc(HMIDIIN handle, UINT msg, DWORD_PTR instance,
                                      DWORD_PTR param1, DWORD_PTR param2) {
  (void)param2;
  if (msg != MIM_DATA) {
    return;
  }
  WinMmMidi* const self = reinterpret_cast<WinMmMidi*>(instance);
  if (self == nullptr) {
    return;
  }
  self->handle_midi_in_message(handle, param1);
}

void WinMmMidi::handle_midi_in_message(HMIDIIN handle, DWORD_PTR param1) {
  std::uint8_t core_index = 0;
  bool found = false;
  {
    std::lock_guard<std::mutex> lock(m_ports_mutex);
    for (const InPort& p : m_in_ports) {
      if (p.handle == handle) {
        core_index = p.core_index;
        found = true;
        break;
      }
    }
  }
  if (!found) {
    return;
  }

  // MIM_DATA packs a short message's bytes into dwParam1, one byte per
  // octet: status in the lowest byte, d1 next, d2 next (the highest byte is
  // unused for anything this HAL forwards -- SysEx is out of scope, same as
  // every other IMidiHal backend, see message.hpp's own doc comment).
  const auto status = static_cast<std::uint8_t>(param1 & 0xFFu);
  const auto d1 = static_cast<std::uint8_t>((param1 >> 8) & 0xFFu);
  const auto d2 = static_cast<std::uint8_t>((param1 >> 16) & 0xFFu);
  const MidiMessage msg{.status = status, .d1 = d1, .d2 = d2};
  const int len = msg.wire_length();
  if (len <= 0) {
    return;
  }

  QueuedInput queued{};
  queued.core_port = core_index;
  queued.bytes[0] = status;
  queued.bytes[1] = d1;
  queued.bytes[2] = d2;
  queued.len = static_cast<std::uint8_t>(len);

  std::lock_guard<std::mutex> lock(m_input_mutex);
  m_input_queue.push_back(queued);
}

void WinMmMidi::drain_input(const InputSink& on_bytes) {
  std::vector<QueuedInput> drained;
  {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    drained.swap(m_input_queue);
  }
  for (const QueuedInput& ev : drained) {
    on_bytes(ev.core_port, ev.bytes, ev.len);
  }
}

int WinMmMidi::poll_fd_count() const {
  // WinMM delivers MIDI input purely through midi_in_proc's callback, never
  // through a pollable file descriptor -- see this header's own doc comment.
  return 0;
}

int WinMmMidi::fill_poll_fds(MidiPollFd* fds, int space) const {
  (void)fds;
  (void)space;
  return 0;
}

}  // namespace arrangrr::host

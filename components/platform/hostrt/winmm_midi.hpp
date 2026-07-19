#pragma once

#if !defined(_WIN32)
#error "winmm_midi.hpp is the Windows backend; it must only be compiled under WIN32"
#endif

// Standard Win32/WinMM headers only -- no mingw-isms (no __attribute__, no
// GNU extensions, no mingw-only pragmas): this file must compile clean under
// BOTH MSVC (the CI windows-latest path, native Visual Studio generator) and
// mingw-w64 (the local cross-build path, cmake/toolchains/mingw-w64.cmake).
#include <windows.h>
// clang-format off
#include <mmsystem.h>
// clang-format on

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "midi_hal.hpp"
#include "shell.hpp"

// The Windows implementation of IMidiHal (docs/proposals/looper-in-gui-
// contract.md §7 item 12), built on plain WinMM (midiIn*/midiOut* -- no
// WinRT MIDI, no third-party loopback driver). Code-complete; a full run is
// CI-verified only (windows-latest, both MSVC native and, locally, a
// mingw-w64 cross-compile via scripts/gui-sonotron/build.sh windows).
//
// WinMM has no equivalent of ALSA's software patchbay (no arbitrary virtual
// port creation, no name-based aconnect-style routing): create_port() binds
// PortDef::index directly to the Nth enumerated WinMM device of the matching
// direction (input devices for is_input, output devices otherwise) --
// device 0 for the first port opened of that direction, device 1 for the
// second, and so on. This is the honest mapping plain WinMM affords; a
// future Windows MIDI Services (WinRT) backend could offer real virtual
// ports if that turns out to matter.
//
// MIDI input arrives on a WinMM-owned callback thread (midiInOpen's
// CALLBACK_FUNCTION), never on the caller's thread -- decoded bytes are
// queued behind a mutex and only handed to drain_input()'s sink from the
// caller's own thread, exactly once per call, never from the callback
// itself.
//
// poll_fd_count()/fill_poll_fds() always report zero: WinMM has no
// meaningful pollable file descriptor for MIDI input at all (delivery is
// purely callback-driven) -- see midi_hal.hpp's own MidiPollFd doc comment,
// every caller already treats "zero fds" as a valid answer.

namespace arrangrr::host {

class WinMmMidi : public IMidiHal {
 public:
  ~WinMmMidi() override;

  bool open(const char* client_name, std::string& error) override;
  bool create_port(const PortDef& def, std::string& error) override;

  void send(std::uint8_t core_port, const MidiMessage& msg) override;
  void drain_input(const InputSink& on_bytes) override;

  int poll_fd_count() const override;
  int fill_poll_fds(MidiPollFd* fds, int space) const override;

 private:
  struct QueuedInput {
    std::uint8_t core_port;
    std::uint8_t bytes[3];
    std::uint8_t len;
  };

  struct OutPort {
    HMIDIOUT handle;
    std::uint8_t core_index;
  };
  struct InPort {
    HMIDIIN handle;
    std::uint8_t core_index;
  };

  static void CALLBACK midi_in_proc(HMIDIIN handle, UINT msg, DWORD_PTR instance, DWORD_PTR param1,
                                    DWORD_PTR param2);
  // Runs on WinMM's own callback thread -- looks up which core input index
  // `handle` belongs to (under m_ports_mutex, since create_port() may still
  // be growing m_in_ports on the caller's thread) and queues the decoded
  // bytes for drain_input() to hand to its sink later, from the caller's own
  // thread only.
  void handle_midi_in_message(HMIDIIN handle, DWORD_PTR param1);

  int out_device_count_opened_so_far() const { return static_cast<int>(m_out_ports.size()); }
  int in_device_count_opened_so_far() const { return static_cast<int>(m_in_ports.size()); }

  std::string m_client_name;

  // Guards m_out_ports/m_in_ports: create_port() (caller thread) appends to
  // them while midi_in_proc (WinMM's own callback thread) concurrently reads
  // them to resolve a core index, so both sides must serialize on the same
  // mutex.
  std::mutex m_ports_mutex;
  std::vector<OutPort> m_out_ports;
  std::vector<InPort> m_in_ports;

  std::mutex m_input_mutex;
  std::vector<QueuedInput> m_input_queue;
};

}  // namespace arrangrr::host

#pragma once

#if !defined(__APPLE__)
#error "coremidi_midi.hpp is the macOS backend; it must only be compiled under APPLE"
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "midi_hal.hpp"
#include "shell.hpp"

// The macOS implementation of IMidiHal (docs/proposals/looper-in-gui-
// contract.md §7 item 12), built on CoreMIDI. Code-complete; UNVERIFIED on
// this dev host (no macOS/osxcross SDK here) -- correctness is CI
// macos-latest's job, both under Xcode's clang (native CI build) and any
// local osxcross cross-compile (cmake/toolchains/osxcross.cmake).
//
// Unlike WinMM, CoreMIDI DOES support arbitrary virtual ports (much closer
// to ALSA's own sequencer model than WinMM's device-binding one):
//   - an input PortDef becomes a virtual DESTINATION (MIDIDestinationCreate)
//     -- other apps connect TO it and send us data, delivered through
//     midi_read_proc.
//   - an output PortDef becomes a virtual SOURCE (MIDISourceCreate) -- other
//     apps connect FROM it; send() posts into it via MIDIReceived(), the
//     standard way a virtual source publishes data to whoever is listening.
//
// MIDI input arrives on a CoreMIDI-owned thread (the readProc callback,
// never the caller's thread) -- decoded bytes are queued behind a mutex and
// only handed to drain_input()'s sink from the caller's own thread, exactly
// once per call, never from the callback itself (same discipline as
// WinMmMidi's own callback -> queue -> drain_input() shape).
//
// poll_fd_count()/fill_poll_fds() always report zero: CoreMIDI I/O runs on
// its own internal thread/run loop, not a file descriptor this process can
// poll() -- see midi_hal.hpp's own MidiPollFd doc comment, every caller
// already treats "zero fds" as a valid answer.

namespace arrangrr::host {

class CoreMidiMidi : public IMidiHal {
 public:
  ~CoreMidiMidi() override;

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

  // Passed as the readProcRefCon to each virtual destination -- lets
  // midi_read_proc recover both `this` and which core input index the
  // destination it fired on corresponds to, without a lookup keyed on the
  // MIDIEndpointRef (CoreMIDI hands the refCon back verbatim per callback,
  // which is simpler and just as correct).
  struct DestContext {
    CoreMidiMidi* self;
    std::uint8_t core_index;
  };

  struct OutPort {
    MIDIEndpointRef source;
    std::uint8_t core_index;
  };
  struct InPort {
    MIDIEndpointRef destination;
    std::unique_ptr<DestContext> context;
  };

  static void midi_read_proc(const MIDIPacketList* packet_list, void* read_proc_ref_con,
                             void* src_conn_ref_con);
  void handle_packet_list(std::uint8_t core_index, const MIDIPacketList* packet_list);

  MIDIClientRef m_client = 0;
  std::vector<OutPort> m_out_ports;
  std::vector<InPort> m_in_ports;

  std::mutex m_input_mutex;
  std::vector<QueuedInput> m_input_queue;
};

}  // namespace arrangrr::host

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "common/midi/message.hpp"
#include "shell.hpp"

// The MIDI hardware-abstraction layer (HAL): a platform-selected seam every
// arrangrr MIDI-emitting frontend (cli-arrangrr, gui-sonotron,
// sonotron-server) drives through IMidiHal&/make_midi_hal(), never through a
// concrete OS backend directly. One implementation per platform:
//   - AlsaMidi     (Linux, today; alsa_midi.hpp)
//   - CoreMidiMidi (macOS; coremidi_midi.hpp, code-complete/CI-verified only)
//   - WinMmMidi    (Windows; winmm_midi.hpp, code-complete/CI-verified only)
// docs/proposals/looper-in-gui-contract.md §7 item 12 (owner-approved). This
// header, and every symbol in it, is host-only — never included from
// components/core.

namespace arrangrr::host {

// Unified MIDI client/device name presented by every arrangrr MIDI-emitting
// frontend (cli-arrangrr, gui-sonotron, sonotron-server) to whichever OS MIDI
// subsystem the platform backend talks to (ALSA sequencer client name today;
// the same string becomes the CoreMIDI client name / WinMM port name on the
// other two platforms). A single source of truth so `aconnect sonotron:1
// <destination>` (or the macOS/Windows equivalent) works no matter which
// frontend the user launched.
inline constexpr const char* kAlsaClientName = "sonotron";

// A minimal, OS-agnostic poll-descriptor shape (same three fields as POSIX
// `struct pollfd` from <poll.h>: fd, events, revents) so this header never
// hard-assumes a POSIX toolchain is present just to declare the interface.
// Windows has no meaningful pollable file descriptor for MIDI I/O at all
// (WinMM delivers input through an OS callback, not a fd) -- WinMmMidi simply
// reports poll_fd_count() == 0 and leaves fill_poll_fds() a no-op, which
// every caller already treats as a valid answer (see AlsaMidi's own
// not-yet-open case), so no platform branching is needed at the call site.
// A POSIX backend (AlsaMidi, CoreMidiMidi) fills this from its own native
// `struct pollfd` array field-by-field (see alsa_midi.cpp) rather than
// reinterpret_cast-ing across the two types, to stay clear of any strict-
// aliasing concern.
struct MidiPollFd {
  int fd = -1;
  short events = 0;
  short revents = 0;
};

class IMidiHal {
 public:
  virtual ~IMidiHal() = default;

  virtual bool open(const char* client_name, std::string& error) = 0;
  virtual bool create_port(const PortDef& def, std::string& error) = 0;

  virtual void send(std::uint8_t core_port, const MidiMessage& msg) = 0;

  // De-templated form of the original AlsaMidi::drain_input (a virtual
  // method cannot itself be a template): drains every pending input event
  // and invokes on_bytes(core_port, bytes, len) once per decoded message.
  // std::function is used rather than std::function_ref (the more literally
  // "non-owning" choice) because it matches the existing house pattern right
  // next door -- UdsServer::LineHandler/ConnectHandler (uds_server.hpp) --
  // and because std::function_ref's library support is C++26-gated per
  // docs/proposals/modernization-survey-2026-07.md, not safe to depend on
  // unconditionally across every toolchain this repo builds with today.
  using InputSink =
      std::function<void(std::uint8_t core_port, const std::uint8_t* bytes, std::size_t len)>;
  virtual void drain_input(const InputSink& on_bytes) = 0;

  virtual int poll_fd_count() const = 0;
  virtual int fill_poll_fds(MidiPollFd* fds, int space) const = 0;
};

// Platform-selected factory: returns the one IMidiHal backend for whichever
// platform this translation unit was compiled for (Linux -> AlsaMidi, macOS
// -> CoreMidiMidi, Windows -> WinMmMidi). Never returns null; callers still
// must check open()'s own return value the same way they always have.
std::unique_ptr<IMidiHal> make_midi_hal();

}  // namespace arrangrr::host

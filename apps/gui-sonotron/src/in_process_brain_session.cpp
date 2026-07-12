#include "in_process_brain_session.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

#include "alsa_midi.hpp"
#include "arrangrr/abi.hpp"
#include "brain_event_from_outevent.hpp"
#include "common/time.hpp"
#include "shell.hpp"
#include "spsc_ring.hpp"

// The engine-thread half of Phase 2b's "integrated" mode (docs/design/
// sonotron-server-phase2-brief.md). Mirrors apps/sonotron-server/main.cpp's
// own live loop (AlsaMidi + Shell + a tick clock) closely on purpose --
// same backend shape, minus the UDS control socket (replaced by the Command
// ring) and minus the poll()-driven wakeup (replaced by a short sleep, since
// there is no fd to block on here). MIDI-in hardware is explicitly deferred
// (Phase 2b scope note): this backend only ever plays what the Command ring
// drives.

namespace sonotron {

namespace {

using arrangrr::Command;
using arrangrr::Op;
using arrangrr::OutEvent;
using arrangrr::Param;
using arrangrr::TickAccumulator;
using arrangrr::host::AlsaMidi;
using arrangrr::host::PortDef;
using arrangrr::host::Shell;

// Human input rate is orders of magnitude below the tick rate; 1024 slots of
// a <=20 B Command is ~20 KB, trivial (Corelli §15.2's own sizing
// recommendation). The Command ring NEVER silently drops (see send()); the
// OutEvent ring MAY drop under backpressure (asymmetric policy, correction
// #3) -- 1024 slots of a <=16 B OutEvent (~16 KB) is equally generous
// headroom, not a tight bound.
constexpr std::size_t kCommandRingCapacity = 1024;
constexpr std::size_t kOutEventRingCapacity = 1024;

// Translates the small, closed set of L1 command lines gui-sonotron's panels
// currently send (transport_panel.cpp, main.cpp's Transport menu: transport
// start/stop/continue, panic) directly into the ABI Command -- none of these
// need Shell-side name/index resolution, so no dispatch logic is duplicated.
// Returns false for anything else (e.g. "style load <name>", "part <role>
// mute|solo ..."), which resolve against Shell-internal lookup tables
// (style-name -> index, track-role parsing) not reachable as a thin adapter
// without reaching into hostrt's private shell_internal.hpp -- see this
// milestone's report for the scoped gap this leaves in integrated mode.
bool command_line_to_command(std::string_view line, Command& out) {
  out = Command{};
  out.op = Op::kDo;
  if (line == "transport start") {
    out.param = Param::kTransportStart;
    return true;
  }
  if (line == "transport stop") {
    out.param = Param::kTransportStop;
    return true;
  }
  if (line == "transport continue") {
    out.param = Param::kTransportContinue;
    return true;
  }
  if (line == "panic") {
    out.param = Param::kPanic;
    return true;
  }
  return false;
}

std::uint64_t monotonic_us() {
  const auto epoch = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
}

}  // namespace

struct InProcessBrainSession::Impl {
  using Status = BrainSession::Status;

  SpscRing<Command, kCommandRingCapacity> command_ring;
  SpscRing<OutEvent, kOutEventRingCapacity> out_event_ring;
  std::atomic<bool> running{false};
  std::atomic<bool> prefer_flats{false};
  std::atomic<Status> status{Status::kDisconnected};
  std::thread engine_thread;
  BrainSnapshot snapshot;
  // GUI-thread-only (never touched by the engine thread): the Command ring's
  // never-silently-drop policy (Corelli §15.2 correction #3) and the
  // "command not yet translated" note both surface here as a synthetic
  // BrainEvent the next poll() returns. Pushing them onto out_event_ring
  // instead would violate that ring's single-producer contract (the engine
  // thread is its only producer).
  std::vector<BrainEvent> local_warnings;

  void run_engine();
};

// Ownership discipline (Corelli §15.6): `shell` (and the Runtime/Stage it
// owns) is a local variable of THIS function, on the engine thread's own
// stack -- never stored in `Impl`, never returned, never reachable from the
// GUI thread. The GUI thread only ever touches `command_ring`/`out_event_ring`
// (both safe to share, that is the whole point of an SPSC ring) and the
// plain atomics above.
void InProcessBrainSession::Impl::run_engine() {
  AlsaMidi alsa;
  std::string alsa_error;
  const bool alsa_ok = alsa.open("sonotron-gui", alsa_error);
  if (!alsa_ok) {
    std::fprintf(stderr,
                 "sonotron: integrated engine: ALSA unavailable (%s) -- running without sound\n",
                 alsa_error.c_str());
  }

  Shell shell([this, &alsa, alsa_ok](const OutEvent& ev) {
    if (alsa_ok && ev.kind == OutEvent::Kind::kMidi) {
      alsa.send(ev.port, ev.msg);
    }
    // engine -> GUI: best-effort, MAY drop under backpressure (asymmetric
    // policy, matches the existing UDS broadcast precedent for a slow
    // client -- gui-contract-map.md's "best-effort; a slow client drops
    // events rather than stalling MIDI").
    (void)out_event_ring.try_push(ev);
  });

  // Default topology mirrors sonotron-server's own live launch: one in, one
  // out, wired thru, so a bare integrated launch is immediately playable.
  if (alsa_ok) {
    shell.set_port_hook([&alsa](const PortDef& def) {
      std::string port_error;
      if (!alsa.create_port(def, port_error)) {
        std::fprintf(stderr, "sonotron: integrated engine: %s\n", port_error.c_str());
      }
    });
    std::string ignored;
    shell.exec_line("port open in in0", ignored);
    shell.exec_line("port open out out0", ignored);
    shell.exec_line("thru in0 out0", ignored);
  }

  prefer_flats.store(shell.prefer_flats(), std::memory_order_relaxed);
  status.store(Status::kConnected, std::memory_order_release);

  TickAccumulator acc;
  std::uint64_t last_us = monotonic_us();

  while (running.load(std::memory_order_acquire)) {
    // Drain the Command ring: never silently dropped by the PRODUCER side
    // (see InProcessBrainSession::send()) -- the engine just applies
    // whatever is queued, in FIFO order, through the exact same
    // Engine::push_command every exec_line handler already calls.
    Command cmd;
    while (command_ring.try_pop(cmd)) {
      shell.push_command(cmd);
    }

    const std::uint64_t now_us = monotonic_us();
    acc.set_bpm(shell.engine().transport().bpm());
    const std::uint32_t ticks = acc.advance_us(now_us - last_us);
    last_us = now_us;
    if (ticks > 0) {
      std::string tick_error;
      shell.advance_by(ticks, tick_error);
    }
    prefer_flats.store(shell.prefer_flats(), std::memory_order_relaxed);

    // Short sleep, not a busy spin: there is no fd to block on here (unlike
    // sonotron-server's timerfd + poll()), so this is the clock's wakeup
    // cadence -- 0.5 ms, same interval sonotron-server's timerfd uses.
    std::this_thread::sleep_for(std::chrono::microseconds(500));
  }
}

InProcessBrainSession::InProcessBrainSession() : m_impl(std::make_unique<Impl>()) {}

InProcessBrainSession::~InProcessBrainSession() { stop(); }

bool InProcessBrainSession::start() {
  if (m_impl->running.load(std::memory_order_acquire)) {
    return true;  // already running
  }
  m_impl->status.store(Status::kConnecting, std::memory_order_release);
  m_impl->running.store(true, std::memory_order_release);
  m_impl->engine_thread = std::thread([this] { m_impl->run_engine(); });
  return true;
}

void InProcessBrainSession::stop() {
  if (!m_impl->running.exchange(false, std::memory_order_acq_rel)) {
    return;  // was not running
  }
  if (m_impl->engine_thread.joinable()) {
    m_impl->engine_thread.join();
  }
  m_impl->status.store(Status::kDisconnected, std::memory_order_release);
}

void InProcessBrainSession::send(std::string_view command_line) {
  if (command_line == "quit" || command_line == "exit") {
    return;  // never tear down the shared engine thread from a stray Enter
  }
  Command cmd;
  if (!command_line_to_command(command_line, cmd)) {
    BrainEvent note;
    note.kind = BrainEvent::Kind::kError;
    note.valid = true;
    note.error = "integrated mode does not translate this command to a Command POD yet";
    note.cmd = std::string(command_line);
    m_impl->local_warnings.push_back(std::move(note));
    return;
  }
  if (!m_impl->command_ring.try_push(cmd)) {
    // Asymmetric overflow policy (Corelli §15.2 correction #3): the Command
    // ring never silently drops. At 1024 slots this should be unreachable;
    // surface it as a warn-class note the next poll() returns rather than
    // dropping the user's command unnoticed.
    BrainEvent warn;
    warn.kind = BrainEvent::Kind::kWarn;
    warn.valid = true;
    warn.warn_code = "command_ring_full";
    m_impl->local_warnings.push_back(std::move(warn));
  }
}

void InProcessBrainSession::poll(std::vector<BrainEvent>& out) {
  for (BrainEvent& warn : m_impl->local_warnings) {
    out.push_back(std::move(warn));
  }
  m_impl->local_warnings.clear();

  const bool prefer_flats = m_impl->prefer_flats.load(std::memory_order_relaxed);
  OutEvent ev;
  while (m_impl->out_event_ring.try_pop(ev)) {
    out.push_back(brain_event_from_outevent(ev, prefer_flats));
  }
}

const BrainSnapshot& InProcessBrainSession::snapshot() const { return m_impl->snapshot; }

BrainSession::Status InProcessBrainSession::status() const {
  return m_impl->status.load(std::memory_order_acquire);
}

}  // namespace sonotron

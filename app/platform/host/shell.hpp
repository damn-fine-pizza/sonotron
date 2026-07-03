#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"

// Host shell: resolves L2/L1 text lines into binary core commands (D26) and
// owns everything the core must not know about — port names, pending
// @tick-scheduled lines, tempo parsing. Strings stop here.
//
// D29 semantics: `@tick <line>` only *schedules*; `advance` is the only
// driver of time. advance() executes pending lines exactly when stream time
// reaches their tick.

namespace arrangrr::host {

struct PortDef {
  std::string name;   // user alias
  bool is_input = false;
  std::uint8_t index = 0;  // core port index
};

class Shell {
 public:
  using EventSink = std::function<void(const OutEvent&)>;
  // Called when a `port open` line runs, so a live backend can create the
  // OS-level port. May be empty (script/null backend).
  using PortHook = std::function<void(const PortDef&)>;
  // Sends raw bytes into the engine input path (used by the live backend).
  void feed_midi(std::uint8_t port, Span<const std::uint8_t> bytes) {
    engine_.push_midi_in(port, bytes, sink_);
  }

  explicit Shell(EventSink sink) : sink_(std::move(sink)) {}
  void set_port_hook(PortHook hook) { port_hook_ = std::move(hook); }

  Engine& engine() { return engine_; }
  const std::vector<PortDef>& ports() const { return ports_; }

  // Executes one line (L2 sugar). Returns false on parse error; the error
  // message is passed to `error`. `@tick` lines are queued, not executed.
  bool exec_line(const std::string& line, std::string& error);

  // Advances stream time by n ticks, firing pending @tick lines on the way —
  // the live clock thread drives this (same path as the `advance` command).
  bool advance_by(std::uint64_t n, std::string& error) {
    return advance_to(engine_.now() + n, error);
  }

  // True after a `quit` line.
  bool quit_requested() const { return quit_; }

  // Enharmonic spelling for event rendering, derived from the current key
  // (flat-side keys print Bb, sharp-side keys print A#). §29.7 nit.
  bool prefer_flats() const { return prefer_flats_; }

 private:
  bool exec_now(const std::vector<std::string>& tokens, std::string& error);
  bool advance_to(std::uint64_t target_tick, std::string& error);
  int find_port(const std::string& name, bool input) const;

  int find_track(const std::string& name) const;
  int find_seq(const std::string& name) const;
  void print_help(const std::string& topic) const;

  Engine engine_;
  EventSink sink_;
  PortHook port_hook_;
  std::vector<PortDef> ports_;
  std::vector<std::string> tracks_;  // name -> index (D26: names live host-side)
  std::vector<std::string> seqs_;
  std::uint8_t next_in_ = 0, next_out_ = 0;
  bool prefer_flats_ = false;

  struct Pending {
    std::uint64_t tick;
    std::uint64_t order;  // stable FIFO among same-tick lines
    std::vector<std::string> tokens;
  };
  std::vector<Pending> pending_;
  std::uint64_t pending_order_ = 0;
  bool quit_ = false;
};

}  // namespace arrangrr::host

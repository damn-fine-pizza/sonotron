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
  std::string name;  // user alias
  bool is_input = false;
  std::uint8_t index = 0;  // core port index
};

class Shell {
 public:
  using EventSink = std::function<void(const OutEvent&)>;
  // Called when a `port open` line runs, so a live backend can create the
  // OS-level port. May be empty (script/null backend).
  using PortHook = std::function<void(const PortDef&)>;
  // Receives help-panel content (empty vector = close). Returns true when a
  // panel UI consumed it; false falls back to plain printing.
  using PanelHook = std::function<bool(const std::vector<std::string>&)>;
  // Sends raw bytes into the engine input path (used by the live backend).
  void feed_midi(std::uint8_t port, Span<const std::uint8_t> bytes) {
    m_engine.push_midi_in(port, bytes, m_sink);
  }

  explicit Shell(EventSink sink) : m_sink(std::move(sink)) {}
  void set_port_hook(PortHook hook) { m_port_hook = std::move(hook); }
  void set_panel_hook(PanelHook hook) { m_panel_hook = std::move(hook); }

  Engine& engine() { return m_engine; }
  const Engine& engine() const { return m_engine; }
  const std::vector<PortDef>& ports() const { return m_ports; }

  // Executes one line (L2 sugar). Returns false on parse error; the error
  // message is passed to `error`. `@tick` lines are queued, not executed.
  bool exec_line(const std::string& line, std::string& error);

  // Advances stream time by n ticks, firing pending @tick lines on the way —
  // the live clock thread drives this (same path as the `advance` command).
  bool advance_by(std::uint64_t n, std::string& error) {
    return advance_to(m_engine.now() + n, error);
  }

  // True after a `quit` line.
  bool quit_requested() const { return m_quit; }

  // Enharmonic spelling for event rendering, derived from the current key
  // (flat-side keys print Bb, sharp-side keys print A#). §29.7 nit.
  bool prefer_flats() const { return m_prefer_flats; }

 private:
  bool exec_now(const std::vector<std::string>& tokens, std::string& error);
  bool advance_to(std::uint64_t target_tick, std::string& error);
  int find_port(const std::string& name, bool input) const;

  int find_track(const std::string& name) const;
  int find_seq(const std::string& name) const;
  std::vector<std::string> build_help(const std::string& topic) const;

  Engine m_engine;
  EventSink m_sink;
  PortHook m_port_hook;
  PanelHook m_panel_hook;
  std::vector<std::string> m_last_help;
  std::vector<PortDef> m_ports;
  std::vector<std::string> m_tracks;  // name -> index (D26: names live host-side)
  std::vector<std::string> m_seqs;
  std::uint8_t m_next_in = 0, m_next_out = 0;
  bool m_prefer_flats = false;

  struct Pending {
    std::uint64_t tick;
    std::uint64_t order;  // stable FIFO among same-tick lines
    std::vector<std::string> tokens;
  };
  std::vector<Pending> m_pending;
  std::uint64_t m_pending_order = 0;
  bool m_quit = false;
};

}  // namespace arrangrr::host

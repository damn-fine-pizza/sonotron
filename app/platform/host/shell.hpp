#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"
#include "panel_manager.hpp"
#include "piano_view.hpp"

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
  // Receives the composed block of all visible panels (empty vector = none
  // visible). Returns true when a panel UI consumed it; false falls back to
  // plain printing.
  using PanelHook = std::function<bool(const std::vector<std::string>&)>;
  // Prints one informational line (`panel list` output and friends). The live
  // TUI wires this to the log pane; unset falls back to stdout.
  using PrintHook = std::function<void(const std::string&)>;
  // Supplies the current terminal width so panel renderers can adapt.
  using WidthProvider = std::function<int()>;
  // Sends raw bytes into the engine input path (used by the live backend).
  void feed_midi(std::uint8_t port, Span<const std::uint8_t> bytes) {
    m_engine.push_midi_in(port, bytes, m_sink);
  }

  explicit Shell(EventSink sink);
  void set_port_hook(PortHook hook) { m_port_hook = std::move(hook); }
  void set_panel_hook(PanelHook hook) { m_panel_hook = std::move(hook); }
  void set_print_hook(PrintHook hook) { m_print_hook = std::move(hook); }
  void set_width_provider(WidthProvider provider) { m_width_provider = std::move(provider); }

  // Seeds the help panel with startup guidance (e.g. a demo motd) and opens it.
  void show_motd(const std::vector<std::string>& lines);

  // Re-renders width-dependent panels from state and re-pushes the composed
  // block — the terminal resize hook lands here (H1 resize contract).
  void refresh_panels();

  const PanelManager& panels() const { return m_panels; }
  const PianoViewState& piano_state() const { return m_piano; }

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

  bool cmd_panel(const std::vector<std::string>& tokens, std::string& error);
  void open_help_topic(const std::string& topic);
  void refresh_piano_content();
  bool push_panels();
  void print_lines(const std::vector<std::string>& lines);
  int panel_columns() const;

  Engine m_engine;
  EventSink m_sink;
  PortHook m_port_hook;
  PanelHook m_panel_hook;
  PrintHook m_print_hook;
  WidthProvider m_width_provider;
  PanelManager m_panels;
  PianoViewState m_piano;
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

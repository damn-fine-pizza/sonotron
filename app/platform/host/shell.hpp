#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"
#include "midi_monitor.hpp"
#include "panel_manager.hpp"
#include "piano_view.hpp"
#include "style_chooser.hpp"

// Host shell: resolves L2/L1 text lines into binary core commands (D26) and
// owns everything the core must not know about — port names, pending
// @tick-scheduled lines, tempo parsing. Strings stop here.
//
// D29 semantics: `@tick <line>` only *schedules*; `advance` is the only
// driver of time. advance() executes pending lines exactly when stream time
// reaches their tick.

namespace arrangrr::host {

struct RcConfig;  // rc_config.hpp — ~/.arrangrr.rc parse result

struct PortDef {
  std::string name;  // user alias
  bool is_input = false;
  std::uint8_t index = 0;  // core port index
};

// Piano key-input policy (docs/TUI_SPEC.md §2).
//   kMomentary — note sounds while the key is physically held; note-off on
//                release. This needs true key-release events, which only the
//                kitty keyboard protocol delivers (a plain TTY has none), so it
//                is honoured through Shell::piano_key_event() fed by the parsed
//                kitty escapes. On terminals without the protocol only plain
//                bytes arrive and the piano degrades to kToggle (see below).
//   kToggle    — press = note-on, press the same key again = note-off. The only
//                policy a plain raw-mode TTY can implement honestly, and the
//                fallback for the plain-byte path in either mode.
enum class PianoKeyMode { kMomentary, kToggle };

// Which interactive mode the live TUI is in. Drives the contextual help
// panel: its content follows the mode (commands in the REPL, piano keys in
// play mode) unless the user pinned an explicit `help <topic>`.
enum class UiMode { kRepl, kPiano, kStyles };

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
  // Supplies the rows available to the panel grid (Console::panel_rows()).
  using HeightProvider = std::function<int()>;
  // Sends raw bytes into the engine input path (used by the live backend).
  void feed_midi(std::uint8_t port, Span<const std::uint8_t> bytes) {
    m_engine.push_midi_in(port, bytes, m_sink);
  }

  explicit Shell(EventSink sink);
  void set_port_hook(PortHook hook) { m_port_hook = std::move(hook); }
  void set_panel_hook(PanelHook hook) { m_panel_hook = std::move(hook); }
  void set_print_hook(PrintHook hook) { m_print_hook = std::move(hook); }
  void set_width_provider(WidthProvider provider) { m_width_provider = std::move(provider); }
  void set_height_provider(HeightProvider provider) { m_height_provider = std::move(provider); }

  // Live-mode routing of output into the uniform panels. Events append to the
  // scrolling kEvents panel (no immediate repaint — the ~100ms grid refresh
  // flushes them); command echo / print output / errors append to the scrolling
  // kConsole panel and repaint at once.
  void log_event(const std::string& line);
  void console_output(const std::string& line);

  // Seeds the help panel with startup guidance (e.g. a demo motd) and opens it.
  void show_motd(const std::vector<std::string>& lines);

  // Re-renders width-dependent panels from state and re-pushes the composed
  // block — the terminal resize hook lands here (H1 resize contract).
  void refresh_panels();

  const PanelManager& panels() const { return m_panels; }
  const PianoViewState& piano_state() const { return m_piano; }
  const MidiMonitor& monitor() const { return m_monitor; }
  const UiStyle& ui_style() const { return m_style; }

  // Live TUI wires the real terminal capabilities in (a fresh Shell assumes
  // no TTY / no UTF-8, so colors stay off for scripts and tests).
  void configure_terminal(bool is_tty, bool utf8);

  // Live TUI key dispatch (H2): consumes the byte when a panel has focus.
  // Returns false with REPL focus so typing stays exactly as before —
  // panel focus is entered only via `panel focus ...` commands. In piano
  // focus SPACE (0x20) toggles the key mode (momentary <-> toggle) and is
  // never a musical note; plain musical bytes always take the toggle path so
  // the piano is never dead on a terminal without key-release events.
  bool handle_ui_key(std::uint8_t byte);

  // SHIFT+TAB from the live loop: step the focus cycle backwards.
  void focus_prev();

  // True key press/release from the kitty keyboard protocol (H3): drives
  // momentary polyphony when the piano is focused. `pressed` = true is a
  // key-down (or autorepeat — idempotent, never a double note-on), false is a
  // key-up. In kToggle mode a press behaves like handle_ui_key and a release
  // is ignored. Returns true when the key mapped to a musical key and was
  // consumed; false lets the caller fall back to the normal byte path (so
  // non-musical kitty keys — TAB, SPACE, shortcuts — still work).
  bool piano_key_event(char key, bool pressed);

  PianoKeyMode piano_key_mode() const { return m_piano_key_mode; }

  // The style/section chooser dissolved into the always-present styles panel:
  // it is ALWAYS constructed and always rendered. Interactions (digits, arrows,
  // -/=, _/+, ENTER, musical keys) only fire when the styles panel is FOCUSED;
  // styles_focused() is the gate main.cpp and handle_ui_key check.
  bool styles_focused() const;
  const StyleChooser& chooser() const { return m_chooser; }

  // Gives focus to the styles panel (backtick shortcut) and seeds the chooser
  // highlight from the arranger's current style/section.
  void focus_styles();

  // Keyboard stepping of the arranger's variation (section) and style with a
  // debounced auto-apply (host-live). The step keys mark a pending (style,
  // section) selection and bump a generation counter, but do NOT switch the
  // band immediately; main.cpp's live loop calls apply_style_step() ~500 ms
  // after the LAST step so pressing fast skips intermediate variations.
  std::uint32_t style_step_gen() const { return m_style_step_gen; }
  bool style_step_pending() const { return m_style_step_pending; }
  void apply_style_step();

  // Arrow/ESC drivers for the escape state machine in main.cpp (arrows arrive
  // as CSI escapes, not plain bytes). Each is a no-op unless the styles panel is
  // focused; chooser_cancel drops focus back to the REPL.
  void chooser_nav_style(int delta);
  void chooser_nav_section(int delta);
  void chooser_cancel();

  // Applies a ~/.arrangrr.rc configuration to the panels (order/heights/layout)
  // and logs any warnings to the console panel.
  void apply_rc(const RcConfig& rc);

  // Terminal-aware momentary lock (H3). The Shell itself stays agnostic — the
  // default is momentary-capable — and the live backend calls this after it has
  // probed the terminal. Passing false (no key-release support) downgrades any
  // current momentary mode to toggle and makes the SPACE mode-switch refuse to
  // go back to momentary, so the piano can never enter a mode the terminal
  // cannot honour.
  void set_momentary_available(bool available);
  bool momentary_available() const { return m_momentary_available; }

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

  // exec_now dispatch is split into per-domain groups so each stays within the
  // cognitive-complexity budget; the groups preserve the original matching
  // order and guards, returning std::nullopt when the command is not theirs.
  std::optional<bool> dispatch_ui(const std::vector<std::string>& tokens, const std::string& cmd,
                                  std::string& error);
  std::optional<bool> dispatch_midi(const std::vector<std::string>& tokens, const std::string& cmd,
                                    std::string& error);
  std::optional<bool> dispatch_music(const std::vector<std::string>& tokens, const std::string& cmd,
                                     std::string& error);
  std::optional<bool> dispatch_transport(const std::vector<std::string>& tokens,
                                         const std::string& cmd, std::string& error);

  bool cmd_help(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_panel(const std::vector<std::string>& tokens, std::string& error);
  bool panel_layout(const std::vector<std::string>& tokens, std::string& error);
  bool panel_target(const std::string& sub, const std::vector<std::string>& tokens,
                    std::string& error);
  bool cmd_piano(const std::vector<std::string>& tokens, std::string& error);
  bool piano_octave(const std::vector<std::string>& tokens, std::string& error);
  bool piano_view(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_notes(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_filter(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_view(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_theme(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_colors(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_port(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_transport(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_bpm(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_route(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_thru(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_clock(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_midi_send(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_panic(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_key(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_play(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_chord(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_style(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_seq(const std::vector<std::string>& tokens, std::string& error);
  bool seq_add(const std::vector<std::string>& tokens, std::string& error);
  bool seq_transpose(const std::vector<std::string>& tokens, std::string& error);
  bool seq_del(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_track(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_program(const std::vector<std::string>& tokens, std::string& error);
  bool track_new(const std::vector<std::string>& tokens, std::string& error);
  bool track_step(const std::vector<std::string>& tokens, int track, std::string& error);
  bool cmd_advance(const std::vector<std::string>& tokens, std::string& error);
  void open_help_topic(const std::string& topic);

  // Style/section chooser plumbing (host-live). The chooser is always present
  // and dissolves into the styles panel; seed_chooser_selection lands its
  // highlight on the arranger's current style/section; chooser_key routes a
  // consumed byte while the styles panel is focused; chooser_apply resolves the
  // selection into a kStyleSwitch.
  void seed_chooser_selection();
  bool chooser_key(std::uint8_t byte);
  void chooser_apply(ChooserApply mode);

  // Style/section keyboard stepping plumbing (host-live). style_step_key maps a
  // byte to an axis+direction; style_step drives the chooser highlight and
  // mirrors it into a debounced pending selection (clamped, no wrap).
  enum class StyleStepAxis { kSection, kStyle };
  bool style_step_key(std::uint8_t byte);
  void style_step(StyleStepAxis axis, int delta);

  void refresh_piano_content();
  void refresh_styles_content();
  void refresh_chords_content();  // live piano->chord readout (detect state + name)
  UiMode current_ui_mode() const;
  std::vector<std::string> contextual_help_lines() const;
  void sync_contextual_panel();
  bool push_panels();
  void print_lines(const std::vector<std::string>& lines);
  void print_line(const std::string& line);
  int panel_columns() const;
  int panel_rows_available() const;
  void toggle_piano_key(char key, int semitone_from_base);
  void piano_momentary_on(char key, int semitone_from_base);
  void piano_momentary_off(char key, int semitone_from_base);
  void piano_all_notes_off();

  // Shared piano note plumbing (used by both the toggle and momentary paths).
  const PianoKeyBinding* piano_binding_for(std::uint8_t byte) const;
  bool piano_midi_note(int semitone_from_base, std::uint8_t& out) const;
  bool piano_note_held(std::uint8_t midi_note) const;
  void piano_send_note(char key, std::uint8_t midi_note, bool note_on);

  Engine m_engine;
  EventSink m_sink;
  PortHook m_port_hook;
  PanelHook m_panel_hook;
  PrintHook m_print_hook;
  WidthProvider m_width_provider;
  HeightProvider m_height_provider;
  PanelManager m_panels;
  PianoViewState m_piano;
  UiStyle m_style;
  UiMode m_ui_mode = UiMode::kRepl;  // last mode the contextual panel synced to
  bool m_help_pinned = false;        // true while an explicit help <topic> shows
  // Always-present style/section chooser (dissolved into the styles panel).
  StyleChooser m_chooser;
  bool m_styles_was_focused = false;  // edge-detect panel focus to seed the chooser
  bool m_await_panel_digit = false;   // a digit right after TAB jumps to panel #N
  // Debounced style/section keyboard stepping (host-live). The pending
  // selection is (style index, section); m_style_step_gen drives the debounce
  // clock in main.cpp, and m_style_step_pending marks a step not yet applied.
  int m_step_style_index = 0;
  SectionType m_step_section = SectionType::kVarA;
  std::uint32_t m_style_step_gen = 0;
  bool m_style_step_pending = false;
  MidiMonitor m_monitor;
  MidiEventFilter m_filter;
  MidiViewOptions m_view_options;
  ActiveNoteTracker m_piano_held;  // notes the piano is currently sounding
  PianoKeyMode m_piano_key_mode = PianoKeyMode::kMomentary;  // default: momentary
  bool m_momentary_available = true;  // cleared when the terminal has no key-release
  char m_pending_source_key = 0;      // annotates monitor events while feeding
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

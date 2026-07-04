#include "shell.hpp"
#include "shell_internal.hpp"

#include <cstdio>

#include "note_names.hpp"

// Shell spine: construction, line/tick execution, per-domain command dispatch,
// panel rendering glue and the port/track/seq name lookups. The command
// handlers and live-input bodies live in the sibling shell_*.cpp translation
// units (all still members of the same arrangrr::host::Shell); the shared
// parsing helpers live in shell_internal.hpp / shell_parse.cpp.

namespace arrangrr::host {

using namespace shell_detail;

namespace {

// Builds the selectable style list from the built-ins the core matches by
// index; each style advertises exactly the sections it defines. Used to seed
// the always-present chooser.
std::vector<StyleInfo> build_style_infos() {
  std::vector<StyleInfo> infos;
  for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
    const Style* style = styles::kBuiltins[i];
    StyleInfo info{.index = static_cast<int>(i), .name = style->name, .sections = {}};
    for (const StyleSection& section : style->sections) {
      info.sections.push_back(section.type);
    }
    infos.push_back(std::move(info));
  }
  return infos;
}

const char* mode_label(Mode mode) {
  switch (mode) {
    case Mode::kMajor:
      return "major";
    case Mode::kMinor:
      return "minor";
    case Mode::kDorian:
      return "dorian";
    case Mode::kPhrygian:
      return "phrygian";
    case Mode::kLydian:
      return "lydian";
    case Mode::kMixolydian:
      return "mixolydian";
    case Mode::kLocrian:
      return "locrian";
  }
  return "?";
}

std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    // '#' opens a comment only at the start of a token — mid-token it is a
    // sharp (F#3). A comment therefore needs whitespace before it.
    if (c == '#' && cur.empty()) {
      break;
    }
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        out.push_back(std::move(cur)), cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(std::move(cur));
  }
  return out;
}

}  // namespace

Shell::Shell(EventSink sink)
    // Every host-visible OutEvent flows through the monitor before the user
    // sink: the MIDI monitor observes exactly what the host emits (H2).
    : m_sink([this, user = std::move(sink)](const OutEvent& ev) {
        m_monitor.observe(ev, m_pending_source_key);
        user(ev);
      }),
      m_chooser(build_style_infos()) {
  m_panels.set_content(PanelId::kFilter, {"filter: channel|port|event|clear (help filter)"});
  m_panels.set_content(PanelId::kChords,
                       {"detect: off  (chord detect on|off)", "chord: (no chord)"});
}

void Shell::print_lines(const std::vector<std::string>& lines) {
  for (const std::string& line : lines) {
    if (m_print_hook) {
      m_print_hook(line);
    } else {
      std::printf("%s\n", line.c_str());
    }
  }
}

int Shell::panel_columns() const {
  if (m_width_provider) {
    return m_width_provider();
  }
  return kDefaultPanelColumns;
}

int Shell::panel_rows_available() const {
  if (m_height_provider) {
    return m_height_provider();
  }
  return kDefaultPanelRows;
}

void Shell::refresh_piano_content() {
  // The piano regenerates at its CELL width (half the terminal in a two-per-row
  // grid), so its compact/minimal tiers pick the layout that fits.
  const int width = m_panels.cell_width(PanelId::kPiano, panel_columns());
  m_panels.set_content(PanelId::kPiano, render_piano_panel(m_piano, width, m_monitor, m_filter,
                                                           m_view_options, m_style));
}

void Shell::refresh_styles_content() {
  // The styles panel IS the chooser: current style/section (bold+colour on the
  // selection) plus a live `key:` line read from the chord engine. The key line
  // sits just above the (least-important) hint line so it survives a short cell.
  std::vector<std::string> lines = m_chooser.render(m_piano.note_naming, m_style);
  const Key& key = m_engine.chords().key();
  const NoteNameOptions opts{
      .naming = m_piano.note_naming, .prefer_flats = m_prefer_flats, .include_octave = false};
  std::string key_line = "key: " + pitch_class_name(key.root_pc, opts) + " " + mode_label(key.mode);
  if (lines.empty()) {
    lines.push_back(std::move(key_line));
  } else {
    lines.insert(lines.end() - 1, std::move(key_line));  // before the hint line
  }
  m_panels.set_content(PanelId::kStyles, std::move(lines));
}

namespace {
// Jazz/lead-sheet suffix for a chord quality ("" = plain major, so root only).
const char* chord_quality_suffix(ChordQuality q) {
  switch (q) {
    case ChordQuality::kMaj:      return "";
    case ChordQuality::kMin:      return "m";
    case ChordQuality::kDim:      return "dim";
    case ChordQuality::kAug:      return "aug";
    case ChordQuality::kMaj7:     return "maj7";
    case ChordQuality::kMin7:     return "m7";
    case ChordQuality::kDom7:     return "7";
    case ChordQuality::kHalfDim7: return "m7b5";
    case ChordQuality::kDim7:     return "dim7";
    case ChordQuality::kSus2:     return "sus2";
    case ChordQuality::kSus4:     return "sus4";
  }
  return "";
}
}  // namespace

void Shell::refresh_chords_content() {
  // The chords panel reports the live piano->chord state: whether detection is
  // armed and the chord the arranger is currently harmonizing against (set by
  // held keys, `chord play`, or a recorded sequence). Chord memory means the
  // name lingers after the keys are released, until a new chord is played.
  const ChordState& chord = m_engine.chords().state();
  const bool detect = m_engine.chord_detect();
  const NoteNameOptions opts{
      .naming = m_piano.note_naming, .prefer_flats = m_prefer_flats, .include_octave = false};
  std::vector<std::string> lines;
  lines.push_back(std::string("detect: ") + (detect ? "on " : "off") + "  (chord detect on|off)");
  lines.push_back(chord.valid ? "chord: " + pitch_class_name(chord.root_pc, opts) +
                                    chord_quality_suffix(chord.quality)
                              : "chord: (no chord)");
  m_panels.set_content(PanelId::kChords, std::move(lines));
}

UiMode Shell::current_ui_mode() const {
  if (m_panels.focus_kind() == PanelFocus::kPanel) {
    if (m_panels.focused_panel() == PanelId::kPiano) {
      return UiMode::kPiano;
    }
    if (m_panels.focused_panel() == PanelId::kStyles) {
      return UiMode::kStyles;
    }
  }
  return UiMode::kRepl;
}

std::vector<std::string> Shell::contextual_help_lines() const {
  // The help panel IS the contextual menu: it teaches the keys of the mode
  // you're in, and always the fundamental navigation shortcuts.
  const UiMode mode = current_ui_mode();
  std::vector<std::string> lines = mode == UiMode::kPiano    ? build_help("piano")
                                   : mode == UiMode::kStyles ? build_help("styles")
                                                             : build_help("");
  lines.push_back("nav: TAB focus | CTRL+P play/stop | ` style/section | CTRL+C quit");
  return lines;
}

void Shell::sync_contextual_panel() {
  // The menu (help) panel follows the interactive mode unless a `help <topic>`
  // is pinned. A mode change unpins so the contextual content resumes. The panel
  // is not force-opened here (that would reshuffle the visible set / focus cycle).
  const UiMode mode = current_ui_mode();
  if (mode != m_ui_mode) {
    m_ui_mode = mode;
    m_help_pinned = false;
  }
  if (!m_help_pinned) {
    m_panels.set_content(PanelId::kHelp, contextual_help_lines());
  }
}

bool Shell::push_panels() {
  // Seed the chooser from the arranger the moment a panel is focused (fresh from
  // the REPL), so stepping/navigation start where the band actually is.
  const bool panel_focused = m_panels.focus_kind() == PanelFocus::kPanel;
  if (panel_focused && !m_styles_was_focused) {
    seed_chooser_selection();
  }
  m_styles_was_focused = panel_focused;

  // Content that depends on state/width/naming is regenerated on every push, so
  // a resize can never leave a stale layout behind (H1 resize contract).
  sync_contextual_panel();
  refresh_styles_content();  // the styles panel always reflects the chooser + key
  if (m_panels.visible(PanelId::kPiano)) {
    refresh_piano_content();
  }
  if (m_panels.visible(PanelId::kChords)) {
    refresh_chords_content();
  }

  return m_panel_hook &&
         m_panel_hook(m_panels.combined_lines(panel_columns(), panel_rows_available(), m_style));
}

void Shell::log_event(const std::string& line) {
  // Append only: the ~100ms grid refresh in main.cpp flushes the events panel,
  // so a burst of MIDI never triggers a repaint per event.
  m_panels.append_line(PanelId::kEvents, line);
}

void Shell::console_output(const std::string& line) {
  m_panels.append_line(PanelId::kConsole, line);
  (void)push_panels();  // command output repaints at once
}

void Shell::configure_terminal(bool is_tty, bool utf8) {
  m_style.set_terminal_is_tty(is_tty);
  m_style.set_terminal_utf8(utf8);
}

void Shell::refresh_panels() { (void)push_panels(); }

void Shell::show_motd(const std::vector<std::string>& lines) {
  m_panels.set_content(PanelId::kHelp, lines);
  m_panels.open(PanelId::kHelp);
  m_help_pinned = true;  // the motd survives re-renders until the mode changes

  if (!push_panels()) {
    print_lines(lines);
  }
}

void Shell::print_line(const std::string& line) { print_lines({line}); }

int Shell::find_seq(const std::string& name) const {
  for (std::size_t i = 0; i < m_seqs.size(); ++i) {
    if (m_seqs[i] == name) {
      return static_cast<int>(i);
    }
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < m_seqs.size()) {
    return static_cast<int>(idx);
  }
  return -1;
}

int Shell::find_track(const std::string& name) const {
  for (std::size_t i = 0; i < m_tracks.size(); ++i) {
    if (m_tracks[i] == name) {
      return static_cast<int>(i);
    }
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < m_tracks.size()) {
    return static_cast<int>(idx);
  }
  return -1;
}

int Shell::find_port(const std::string& name, bool input) const {
  for (const PortDef& p : m_ports) {
    if (p.name == name && p.is_input == input) {
      return p.index;
    }
  }
  // Bare numeric index is accepted too.
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < kMaxPorts) {
    return static_cast<int>(idx);
  }
  return -1;
}

bool Shell::exec_line(const std::string& line, std::string& error) {
  std::vector<std::string> tokens = tokenize(line);
  if (tokens.empty()) {
    return true;
  }

  // @tick prefix: queue for execution when time reaches the tick (D29).
  if (tokens[0].size() > 1 && tokens[0][0] == '@') {
    std::uint64_t tick = 0;
    if (!parse_u64(tokens[0].substr(1), tick)) {
      error = "bad @tick: " + tokens[0];
      return false;
    }
    tokens.erase(tokens.begin());
    if (tokens.empty()) {
      error = "@tick with no command";
      return false;
    }
    if (tick < m_engine.now()) {
      error = "@tick in the past";
      return false;
    }
    m_pending.push_back(
        Pending{.tick = tick, .order = m_pending_order++, .tokens = std::move(tokens)});
    return true;
  }

  return exec_now(tokens, error);
}

bool Shell::advance_to(std::uint64_t target, std::string& error) {
  while (true) {
    // Earliest pending line at or before target (stable by insertion order).
    auto next = m_pending.end();
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
      if (it->tick > target) {
        continue;
      }
      if (next == m_pending.end() || it->tick < next->tick ||
          (it->tick == next->tick && it->order < next->order)) {
        next = it;
      }
    }
    if (next == m_pending.end()) {
      break;
    }
    const std::uint64_t at = next->tick;
    if (at > m_engine.now()) {
      m_engine.advance_ticks(static_cast<std::uint32_t>(at - m_engine.now()), m_sink);
    }
    std::vector<std::string> tokens = std::move(next->tokens);
    m_pending.erase(next);
    if (!exec_now(tokens, error)) {
      return false;
    }
  }
  if (target > m_engine.now()) {
    m_engine.advance_ticks(static_cast<std::uint32_t>(target - m_engine.now()), m_sink);
  }
  return true;
}

std::optional<bool> Shell::dispatch_ui(const std::vector<std::string>& t, const std::string& cmd,
                                       std::string& error) {
  if (cmd == "quit" || cmd == "exit") {
    m_quit = true;
    return true;
  }
  if (cmd == "help") {
    return cmd_help(t, error);
  }
  if (cmd == "panel") {
    return cmd_panel(t, error);
  }
  if (cmd == "piano") {
    return cmd_piano(t, error);
  }
  if (cmd == "notes") {
    return cmd_notes(t, error);
  }
  if (cmd == "filter") {
    return cmd_filter(t, error);
  }
  if (cmd == "view") {
    return cmd_view(t, error);
  }
  if (cmd == "theme") {
    return cmd_theme(t, error);
  }
  if (cmd == "colors") {
    return cmd_colors(t, error);
  }
  return std::nullopt;
}

std::optional<bool> Shell::dispatch_midi(const std::vector<std::string>& t, const std::string& cmd,
                                         std::string& error) {
  if (cmd == "port" && t.size() >= 3 && t[1] == "open") {
    return cmd_port(t, error);
  }
  if (cmd == "route" && t.size() == 4 && t[2] == "->") {
    return cmd_route(t, error);
  }
  if (cmd == "thru" && t.size() >= 3) {
    return cmd_thru(t, error);
  }
  if (cmd == "clock" && t.size() >= 3 && t[1] == "out") {
    return cmd_clock(t, error);
  }
  if (cmd == "midi" && t.size() >= 4 && t[1] == "send") {
    return cmd_midi_send(t, error);
  }
  if (cmd == "panic") {
    return cmd_panic(t, error);
  }
  return std::nullopt;
}

std::optional<bool> Shell::dispatch_music(const std::vector<std::string>& t, const std::string& cmd,
                                          std::string& error) {
  if (cmd == "key" && t.size() >= 3) {
    return cmd_key(t, error);
  }
  if ((cmd == "play" && t.size() >= 2) || (cmd == "chord" && t.size() >= 3 && t[1] == "play")) {
    return cmd_play(t, error);
  }
  if (cmd == "chord" && t.size() >= 2) {
    return cmd_chord(t, error);
  }
  if (cmd == "style" && t.size() >= 2) {
    return cmd_style(t, error);
  }
  if (cmd == "seq" && t.size() >= 2) {
    return cmd_seq(t, error);
  }
  if (cmd == "track" && t.size() >= 3) {
    return cmd_track(t, error);
  }
  return std::nullopt;
}

std::optional<bool> Shell::dispatch_transport(const std::vector<std::string>& t,
                                              const std::string& cmd, std::string& error) {
  if (cmd == "transport" && t.size() >= 2) {
    return cmd_transport(t, error);
  }
  if (cmd == "bpm") {
    return cmd_bpm(t, error);
  }
  if (cmd == "advance" && t.size() >= 2) {
    return cmd_advance(t, error);
  }
  return std::nullopt;
}

bool Shell::exec_now(const std::vector<std::string>& t, std::string& error) {
  const std::string& cmd = t[0];

  if (const std::optional<bool> r = dispatch_ui(t, cmd, error)) {
    return *r;
  }
  if (const std::optional<bool> r = dispatch_midi(t, cmd, error)) {
    return *r;
  }
  if (const std::optional<bool> r = dispatch_music(t, cmd, error)) {
    return *r;
  }
  if (const std::optional<bool> r = dispatch_transport(t, cmd, error)) {
    return *r;
  }

  error = "unknown command: " + cmd;
  return false;
}

}  // namespace arrangrr::host

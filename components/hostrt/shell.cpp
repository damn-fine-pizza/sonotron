#include "shell.hpp"
#include "shell_internal.hpp"

#include <algorithm>
#include <cstdio>

#include "arp_view.hpp"
#include "groove_view.hpp"
#include "note_names.hpp"
#include "parts_view.hpp"

// Shell spine: construction, line/tick execution, per-domain command dispatch,
// panel rendering glue and the port/track/seq name lookups. The command
// handlers and live-input bodies live in the sibling shell_*.cpp translation
// units (all still members of the same arrangrr::host::Shell); the shared
// parsing helpers live in shell_internal.hpp / shell_parse.cpp.

namespace arrangrr::host {

using namespace shell_detail;

namespace {

// Accompany (roadmap 9310), docs/design/orchestrator-pipeline-extraction.md
// §16.2(b)/§16.5: the melody-thru's own output port, distinct from the
// band's typical port 0 -- `OutScheduler::cancel_note_off`'s dedup key is
// (port,channel,note), blind to which stage scheduled it, so a shared port
// risks one stage's retrigger logic cancelling the other's note-off. Fixed
// at construction (the MIDI-source stage's port never changes after
// `midi-source load`).
constexpr std::uint8_t kMidiSourcePort = 1;

// Restyle (roadmap 9320): the restyled output's own port, distinct from both
// the band (port 0, via `style route`) and the raw melody-thru
// (kMidiSourcePort = 1) -- same reasoning as kMidiSourcePort's own comment
// (OutScheduler::cancel_note_off's dedup key is (port,channel,note), blind to
// which stage scheduled it). Fixed at construction; `restyle <style>` only
// selects WHICH style this stage restyles into, never its port.
constexpr std::uint8_t kRestylePort = 2;

// Builds the selectable style list from the built-ins the core matches by
// index; each style advertises exactly the sections it defines. Used to seed
// the always-present chooser. Seam D (§17.2): the core SectionType is cast to
// the panel's arrangrr-free SectionKind mirror at this boundary (both are
// std::uint8_t-valued 1:1, same numbering).
std::vector<StyleInfo> build_style_infos() {
  std::vector<StyleInfo> infos;
  for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
    const Style* style = styles::kBuiltins[i];
    StyleInfo info{.index = static_cast<int>(i), .name = style->name, .sections = {}};
    for (const StyleSection& section : style->sections) {
      info.sections.push_back(static_cast<SectionKind>(section.type));
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
    // Phase 4d (docs/design/orchestrator-pipeline-extraction.md
    // §16.3/§16.9): each declared stage gets its OWN construction args --
    // the MIDI-source stage is constructed inert (no file yet, its own port
    // fixed at kMidiSourcePort, `midi-source load` fills it in later);
    // chorddet needs only the shared FollowedContext (m_followed, already
    // constructed by declaration order ahead of m_runtime); arrangrr's
    // Engine needs the Runtime-injected scheduler/transport PLUS the same
    // shared FollowedContext AND the already-constructed chorddet peer.
    // Must be listed FIRST (declaration order, m_followed/m_runtime precede
    // m_sink/m_chooser in shell.hpp).
    //
    // Roadmap 9320 (Restyle), docs/design/restyle-placement.md §1: the
    // RestyleStage slots in between chorddet and Engine, constructed inert
    // (no style loaded, `restyle <style>` fills it in later) at its own
    // fixed output port (kRestylePort) -- it needs the scheduler + the
    // already-constructed chorddet peer (for `key()`) + the shared
    // FollowedContext, the same triple-injection idiom every other stage
    // here already uses.
    : m_runtime(
          [](auto& sched, auto&) {
            return midisrc::MidiSourceStage<kSchedulerCapacity>(sched, kMidiSourcePort);
          },
          [this](auto&, auto&, auto&) { return ChorddetStage<kMaxPorts>(m_followed); },
          [this](auto& sched, auto&, auto&, auto& chorddet) {
            return RestyleStage<kMaxPorts>(sched, chorddet, m_followed, kRestylePort);
          },
          [this](auto& sched, auto& transport, auto&, auto& chorddet, auto&) {
            return Engine(sched, transport, m_followed, chorddet);
          }),
      // Every host-visible OutEvent flows through the monitor before the user
      // sink: the MIDI monitor observes exactly what the host emits (H2).
      // Seam D (§17.2): midi_monitor.hpp no longer sees the core OutEvent /
      // abi.hpp -- Shell (already core-linking) is the boundary that selects
      // the kMidi events and converts them into the panel's MidiOutEvent
      // mirror; every other kind is filtered out here exactly as
      // MidiMonitor::observe used to filter it internally.
      //
      // Phase-5 infrastructure: the SAME kMidi selection also appends to
      // m_export_events (export_smf()'s source data) -- one more sink
      // observer, same shape/seam as the monitor above, no new fan-out
      // mechanism.
      m_sink([this, user = std::move(sink)](const OutEvent& ev) {
        if (ev.kind == OutEvent::Kind::kMidi) {
          m_monitor.observe(MidiOutEvent{.port = ev.port, .msg = ev.msg, .tick = ev.tick},
                            m_pending_source_key);
          m_export_events.push_back(arrstyle::SmfEvent{.tick = ev.tick, .msg = ev.msg});
        }
        user(ev);
      }),
      m_chooser(build_style_infos()) {
  m_panels.set_content(PanelId::kFilter, {"filter: channel|port|event|clear (help filter)"});
  m_panels.set_content(PanelId::kChords, {"detect: off  (chord detect on|off)"});
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
  // Harmony visualizer (roadmap 11410): colour the keyboard by the FOLLOWED
  // chord — green for the chord the band follows this bar (the committed
  // FollowedContext), amber for the shift-staged next chord (D53). The chord
  // status the styles panel already reads is the single source of truth here.
  //
  // Green only when the followed chord is genuinely ACTIVE — transport playing
  // or a producer explicitly steered it — never for the passive home-tonic
  // default establish_default() seeds at rest; otherwise the keyboard would
  // show a "sounding" chord nobody is actually following (owner feedback).
  const bool chord_active = m_engine.transport().playing() || m_engine.chords().explicit_set();
  const std::uint16_t committed_pcs =
      chord_active ? chord_pitch_class_set(m_engine.chords().state()) : std::uint16_t{0};
  const PianoChordOverlay overlay{
      .committed_pcs = committed_pcs,
      .pending_pcs = chord_pitch_class_set(m_engine.chords().pending()),
  };
  m_panels.set_content(PanelId::kPiano, render_piano_panel(m_piano, width, m_monitor, m_filter,
                                                           m_view_options, m_style, overlay));
}

namespace {
// Jazz/lead-sheet suffix for a chord quality ("" = plain major, so root only).
const char* chord_quality_suffix(ChordQuality q) {
  switch (q) {
    case ChordQuality::kMaj:
      return "";
    case ChordQuality::kMin:
      return "m";
    case ChordQuality::kDim:
      return "dim";
    case ChordQuality::kAug:
      return "aug";
    case ChordQuality::kMaj7:
      return "maj7";
    case ChordQuality::kMin7:
      return "m7";
    case ChordQuality::kDom7:
      return "7";
    case ChordQuality::kHalfDim7:
      return "m7b5";
    case ChordQuality::kDim7:
      return "dim7";
    case ChordQuality::kSus2:
      return "sus2";
    case ChordQuality::kSus4:
      return "sus4";
  }
  return "";
}
// The live chord the band is following ("—" when none is latched yet). This is
// the thing that RESPONDS to played keys, distinct from the static key/tonality.
std::string live_chord_label(const ChordState& chord, const NoteNameOptions& opts) {
  return chord.valid ? pitch_class_name(chord.root_pc, opts) + chord_quality_suffix(chord.quality)
                     : std::string("—");
}
}  // namespace

void Shell::refresh_styles_content() {
  // The styles panel IS the chooser (current style/section, bold+colour on the
  // selection) plus the THREE-line D53 chord-state readout, grouped right after
  // the chooser: `original key:` (song tonic, FIXED reference), `current key:`
  // (the committed followed chord sounding THIS bar — starts == original,
  // changes only at bar boundaries) and `next key:` (the staged pending chord,
  // `-` when none, lands at the next bar). This trio used to live in the chords
  // panel; it moved here so it sits beside the section/style it drives.
  std::vector<std::string> lines = m_chooser.render(m_piano.note_naming, m_style);
  const Key& key = m_engine.chords().key();
  const ChordState& chord = m_engine.chords().state();
  const ChordState& next = m_engine.chords().pending();
  const NoteNameOptions opts{
      .naming = m_piano.note_naming, .prefer_flats = m_prefer_flats, .include_octave = false};
  const ChordQuality home_quality = theory::single_finger_quality(key, key.root_pc);
  std::string original_line = "original key: " + pitch_class_name(key.root_pc, opts) +
                              chord_quality_suffix(home_quality) +
                              "   (song tonic — fixed reference)";
  std::string current_line = "current key: " + live_chord_label(chord, opts) + "   (this bar)";
  std::string next_line =
      "next key: " + (next.valid ? live_chord_label(next, opts) : std::string("-")) +
      "   (lands next bar)";
  const auto insert_at = lines.empty() ? lines.end() : lines.end() - 1;  // before the hint line
  lines.insert(insert_at, {original_line, current_line, next_line});
  m_panels.set_content(PanelId::kStyles, std::move(lines));
}

void Shell::refresh_parts_content() {
  const int cols = m_panels.cell_width(PanelId::kParts, panel_columns());
  m_parts_selected = std::clamp(m_parts_selected, 0, static_cast<int>(kMixerPartCount) - 1);
  // Seam D (docs/design/orchestrator-pipeline-extraction.md §17.2): populate
  // the render-side mirror rows from the live in-process Arranger& (Phase
  // 3a) -- render_parts_panel itself no longer sees a core type.
  std::vector<PartRowState> rows;
  rows.reserve(kMixerParts.size());
  for (const MixerPartRow& part : kMixerParts) {
    const Arranger::PartInfo info = m_engine.arranger().part_info(part.role);
    rows.push_back(PartRowState{
        .name = part.name,
        .is_percussion = part.is_percussion,
        .routed = info.routed,
        .port = info.port,
        .channel = info.channel,
        .gm_program = info.gm_program,
        .muted = info.muted,
        .soloed = info.soloed,
    });
  }
  m_panels.set_content(PanelId::kParts,
                       render_parts_panel(rows, m_engine.arranger().any_solo(), m_monitor,
                                          m_parts_selected, cols, m_style));
}

void Shell::refresh_groove_content() {
  const int cols = m_panels.cell_width(PanelId::kGroove, panel_columns());
  m_groove_selected = std::clamp(m_groove_selected, 0, static_cast<int>(kGrooveRowCount) - 1);
  // Seam D (§17.2): mirror the live GrooveParams into the core-free view struct.
  const GrooveParams& p = m_engine.arranger().groove_params();
  const GrooveViewParams view{
      .swing = p.swing,
      .humanize_timing = p.humanize_timing,
      .humanize_velocity = p.humanize_velocity,
      .accent = p.accent,
      .swing_grid = p.swing_grid,
      .quantize = p.quantize,
  };
  m_panels.set_content(PanelId::kGroove,
                       render_groove_panel(view, m_groove_selected, cols, m_style));
}

void Shell::refresh_arp_content() {
  const int cols = m_panels.cell_width(PanelId::kArp, panel_columns());
  m_arp_selected = std::clamp(m_arp_selected, 0, static_cast<int>(kArpRowCount) - 1);
  // Seam D (§17.2): mirror the live ArpeggiatorParams into the core-free view
  // struct; rate/direction ride the core enums' own raw numeric values.
  const ArpeggiatorParams& p = m_engine.arp().params();
  const ArpViewParams view{
      .rate = static_cast<std::uint8_t>(p.rate),
      .direction = static_cast<std::uint8_t>(p.direction),
      .octaves = p.octaves,
      .gate = p.gate,
      .latch = p.latch,
  };
  m_panels.set_content(PanelId::kArp,
                       render_arp_panel(view, m_engine.arp_enabled(), m_engine.arp().held_count(),
                                        m_arp_selected, cols, m_style));
}

void Shell::refresh_chords_content() {
  // The chords panel is the harmony STATUS panel: detect/follow/mode/scale. The
  // D53 chord-state trio (`original key:`/`current key:`/`next key:`) moved to
  // the styles panel, grouped with the chooser it drives — this panel no longer
  // carries chord-state lines.
  const bool detect = m_engine.chord_detect();
  const ChordMode mode = m_engine.chords().mode();
  const NoteNameOptions opts{
      .naming = m_piano.note_naming, .prefer_flats = m_prefer_flats, .include_octave = false};
  const Key& scale = m_engine.chords().key();
  std::vector<std::string> lines;
  // detect: say plainly whether played keys steer the band, and how to toggle it.
  lines.push_back(detect ? std::string("detect: on   (chord detect off)")
                         : std::string("detect: off — keys don't steer (chord detect on)"));
  // follow (D47): which producer is allowed to steer the band's harmony.
  const ChordFollow follow = m_engine.chord_follow();
  lines.push_back(std::string("follow: ") + chord_follow_label(follow) + "   (" +
                  chord_follow_hint(follow) + ")");
  // mode: single-finger needs one key; the fingered modes need a full triad.
  const char* mode_name = mode == ChordMode::kSingle  ? "single-finger"
                          : mode == ChordMode::kShell ? "shell"
                                                      : "diatonic";
  lines.push_back(std::string("mode: ") + mode_name + "   (chord mode)");
  // scale: the diatonic frame — it drives single-finger maj/min and scale-degree
  // parts, and does NOT transpose the band (the #1 source of confusion, D47).
  lines.push_back("scale: " + pitch_class_name(scale.root_pc, opts) + " " + mode_label(scale.mode) +
                  "   (single-finger + scale-degree parts)");
  // The chords panel IS the harmony surface: playing its keys steers the band
  // silently. Only worth saying when focused (that is when the keys route here).
  if (chords_focused()) {
    lines.push_back(
        std::string("play here: keys steer the band, no sound (octave ./ transpose [])"));
  }
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
    if (m_panels.focused_panel() == PanelId::kParts) {
      return UiMode::kParts;
    }
    if (m_panels.focused_panel() == PanelId::kGroove) {
      return UiMode::kGroove;
    }
    if (m_panels.focused_panel() == PanelId::kArp) {
      return UiMode::kArp;
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
                                   : mode == UiMode::kParts  ? build_help("parts")
                                   : mode == UiMode::kGroove ? build_help("groove")
                                   : mode == UiMode::kArp    ? build_help("arp")
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
  if (m_panels.visible(PanelId::kParts)) {
    refresh_parts_content();
  }
  if (m_panels.visible(PanelId::kGroove)) {
    refresh_groove_content();
  }
  if (m_panels.visible(PanelId::kArp)) {
    refresh_arp_content();
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
    return true;  // blank line, or a full-line '#' comment (tokenize strips it)
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
      m_runtime.advance_ticks(static_cast<std::uint32_t>(at - m_engine.now()), m_sink);
    }
    std::vector<std::string> tokens = std::move(next->tokens);
    m_pending.erase(next);
    if (!exec_now(tokens, error)) {
      return false;
    }
  }
  if (target > m_engine.now()) {
    m_runtime.advance_ticks(static_cast<std::uint32_t>(target - m_engine.now()), m_sink);
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
  if (cmd == "midi-source" && t.size() >= 3 && t[1] == "load") {
    return cmd_midi_source(t, error);
  }
  if (cmd == "export-smf" && t.size() >= 2) {
    return cmd_export_smf(t, error);
  }
  if (cmd == "panic") {
    return cmd_panic(t, error);
  }
  return std::nullopt;
}

std::optional<bool> Shell::dispatch_music(const std::vector<std::string>& t, const std::string& cmd,
                                          std::string& error) {
  // `scale` is an alias of `key` (D47 label clarification): the panel line reads
  // `scale:` — it is the diatonic frame for scale-degree parts, NOT a transpose.
  if ((cmd == "key" || cmd == "scale") && t.size() >= 3) {
    return cmd_key(t, error);
  }
  if (cmd == "note" && t.size() >= 4) {
    return cmd_note(t, error);
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
  if (cmd == "restyle" && t.size() >= 2) {
    return cmd_restyle(t, error);
  }
  if (cmd == "seq" && t.size() >= 2) {
    return cmd_seq(t, error);
  }
  if (cmd == "track" && t.size() >= 3) {
    return cmd_track(t, error);
  }
  if (cmd == "program" && t.size() >= 3) {
    return cmd_program(t, error);
  }
  if (cmd == "part" && t.size() >= 2) {
    return cmd_part(t, error);
  }
  if (cmd == "groove" && t.size() >= 2) {
    return cmd_groove(t, error);
  }
  if (cmd == "arp" && t.size() >= 2) {
    return cmd_arp(t, error);
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

#include "shell.hpp"
#include "shell_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "arp_view.hpp"
#include "groove_view.hpp"
#include "parts_view.hpp"

// Live TUI key dispatch (handle_ui_key / piano_key_event) and the simulated
// piano's note plumbing, plus the `piano` command. Bodies moved verbatim from
// shell.cpp. Every piano note flows through the same feed_midi input path real
// hardware uses.

namespace arrangrr::host {

using namespace shell_detail;

bool Shell::piano_midi_note(int semitone_from_base, std::uint8_t& out) const {
  const int note =
      (m_piano.base_octave + 1) * kSemitonesPerOctave + semitone_from_base + m_piano.transpose;
  if (note < 0 || note > kMidiNoteMax) {
    return false;
  }
  out = static_cast<std::uint8_t>(note);
  return true;
}

bool Shell::surface_note_held(const ActiveNoteTracker& held, std::uint8_t midi_note) const {
  for (std::size_t i = 0; i < held.size(); ++i) {
    const ActiveNote& n = held.notes()[i];
    if (n.note == midi_note && n.channel == m_piano.channel) {
      return true;
    }
  }
  return false;
}

void Shell::surface_send_note(std::uint8_t port, ActiveNoteTracker& /*held*/, char key,
                              std::uint8_t midi_note, bool note_on) {
  // Every surface note goes through the same feed_midi -> push_midi_in path real
  // hardware uses (docs/TUI_SPEC.md §1.3): the piano is an input device, never
  // a shortcut into the engine. `port` selects the surface — kPianoInputPort
  // (kMelody, sounds) or kHarmonyInputPort (kHarmony, suppressed but observed by
  // the ChordDetector, so the harmony surface steers silently).
  std::uint8_t bytes[3];
  bytes[0] =
      static_cast<std::uint8_t>((note_on ? midi::kNoteOn : midi::kNoteOff) | m_piano.channel);
  bytes[1] = midi_note;
  bytes[2] = note_on ? m_piano.velocity : kPianoReleaseVelocity;

  m_pending_source_key = key;
  feed_midi(port, Span<const std::uint8_t>(bytes, sizeof(bytes)));
  m_pending_source_key = 0;

  (void)push_panels();
}

void Shell::toggle_surface_key(std::uint8_t port, ActiveNoteTracker& held, char key,
                               int semitone_from_base) {
  // Toggle note-off policy (H2): a plain TTY delivers no key-release events,
  // so pressing the same key again releases the note. This is also the
  // fallback the plain-byte path always uses, even in momentary mode.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  // Auto-repeat debounce: a plain TTY delivers a held key as a stream of
  // identical bytes; without this each byte would flip the note on/off/on/off
  // (a note "a manetta", no rhythm). Ignore a toggle that lands within the
  // debounce window of this key's previous toggle attempt, and slide the window
  // forward on EVERY attempt so a sustained auto-repeat stays suppressed for as
  // long as the key is held. The clock is injected (set_input_time_us); at time
  // 0 (never injected) the debounce is inert, preserving the simple test path.
  const std::uint64_t now = m_input_time_us;
  const std::uint64_t last = m_toggle_last_us[midi_note];
  const bool debounce_armed = now != 0 && last != 0 && now >= last;
  m_toggle_last_us[midi_note] = now;
  if (debounce_armed && now - last < kToggleAutoRepeatDebounceUs) {
    return;
  }

  if (surface_note_held(held, midi_note)) {
    held.note_off(port, m_piano.channel, midi_note);
    surface_send_note(port, held, key, midi_note, false);
    return;
  }

  if (!held.note_on({.port = port,
                     .channel = m_piano.channel,
                     .note = midi_note,
                     .velocity = m_piano.velocity,
                     .source_key = key,
                     .start_tick = 0})) {
    print_line("piano: too many held notes");
    return;
  }
  surface_send_note(port, held, key, midi_note, true);
}

void Shell::surface_momentary_on(std::uint8_t port, ActiveNoteTracker& held, char key,
                                 int semitone_from_base) {
  // Momentary note-on (kitty key-down): sound the note unless it is already
  // sounding. Autorepeat re-presses land here too, so the held check keeps a
  // physically-held key from double-firing note-on.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  if (surface_note_held(held, midi_note)) {
    return;
  }

  if (!held.note_on({.port = port,
                     .channel = m_piano.channel,
                     .note = midi_note,
                     .velocity = m_piano.velocity,
                     .source_key = key,
                     .start_tick = 0})) {
    print_line("piano: too many held notes");
    return;
  }
  surface_send_note(port, held, key, midi_note, true);
}

void Shell::surface_momentary_off(std::uint8_t port, ActiveNoteTracker& held, char key,
                                  int semitone_from_base) {
  // Momentary note-off (kitty key-up): release the note only if we were
  // sounding it.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    return;
  }

  if (!surface_note_held(held, midi_note)) {
    return;
  }
  held.note_off(port, m_piano.channel, midi_note);
  surface_send_note(port, held, key, midi_note, false);
}

void Shell::surface_all_notes_off(std::uint8_t port, ActiveNoteTracker& held) {
  // Release everything the surface is holding through the normal input path.
  while (held.size() > 0) {
    const ActiveNote n = held.notes()[0];

    std::uint8_t bytes[3] = {static_cast<std::uint8_t>(midi::kNoteOff | n.channel), n.note,
                             kPianoReleaseVelocity};
    held.note_off(n.port, n.channel, n.note);
    feed_midi(port, Span<const std::uint8_t>(bytes, sizeof(bytes)));
  }

  (void)push_panels();
}

bool Shell::piano_octave(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3) {
    error = "piano octave <N>|up|down";
    return false;
  }

  int octave = m_piano.base_octave;
  if (t[2] == "up") {
    octave += 1;
  } else if (t[2] == "down") {
    octave -= 1;
  } else {
    char* end = nullptr;
    octave = static_cast<int>(std::strtol(t[2].c_str(), &end, 10));
    if (end == nullptr || *end != '\0') {
      error = "piano octave: not a number: " + t[2];
      return false;
    }
  }

  if (octave < kPianoMinOctave || octave > kPianoMaxOctave) {
    error = "piano octave: out of range " + std::to_string(kPianoMinOctave) + ".." +
            std::to_string(kPianoMaxOctave);
    return false;
  }

  m_piano.base_octave = octave;
  (void)push_panels();
  return true;
}

bool Shell::piano_view(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3) {
    error = "piano view keyboard|active-notes|event-log";
    return false;
  }

  if (t[2] == "keyboard") {
    m_piano.view = PianoView::kKeyboard;
  } else if (t[2] == "active-notes") {
    m_piano.view = PianoView::kActiveNotes;
  } else if (t[2] == "event-log") {
    m_piano.view = PianoView::kEventLog;
  } else {
    error = "piano view: unknown view: " + t[2];
    return false;
  }

  (void)push_panels();
  return true;
}

bool Shell::cmd_piano(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "piano octave <N>|up|down | channel <1..16> | velocity <1..127> | "
      "keymap default | view keyboard|active-notes|event-log | panic";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }
  const std::string& sub = t[1];

  if (sub == "octave") {
    return piano_octave(t, error);
  }

  if (sub == "channel") {
    int ch = 0;
    if (t.size() < 3 || !parse_int(t[2], ch) || ch < 1 || ch > kMidiChannels) {
      error = "piano channel <1..16>";
      return false;
    }
    m_piano.channel = static_cast<std::uint8_t>(ch - 1);  // user 1-based, wire 0-based
    (void)push_panels();
    return true;
  }

  if (sub == "velocity") {
    int vel = 0;
    if (t.size() < 3 || !parse_int(t[2], vel) || vel < kMidiVelocityMin || vel > kMidiVelocityMax) {
      error = "piano velocity <1..127>";
      return false;
    }
    m_piano.velocity = static_cast<std::uint8_t>(vel);
    (void)push_panels();
    return true;
  }

  if (sub == "keymap") {
    if (t.size() < 3 || t[2] != "default") {
      error = "piano keymap default (the only keymap in H2)";
      return false;
    }
    return true;
  }

  if (sub == "view") {
    return piano_view(t, error);
  }

  if (sub == "panic") {
    // Flush BOTH playable surfaces so a note stuck in toggle mode on either the
    // melody (piano) or the harmony (chords) held set is released.
    surface_all_notes_off(kPianoInputPort, m_piano_held);
    surface_all_notes_off(kHarmonyInputPort, m_harmony_held);
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::parts_focused() const {
  return m_panels.focus_kind() == PanelFocus::kPanel &&
         m_panels.focused_panel() == PanelId::kParts;
}

void Shell::parts_select(int delta) {
  m_parts_selected =
      std::clamp(m_parts_selected + delta, 0, static_cast<int>(kMixerPartCount) - 1);
  (void)push_panels();
}

bool Shell::parts_key(std::uint8_t byte) {
  const TrackRole role = mixer_role(static_cast<std::size_t>(m_parts_selected));
  const auto toggle = [&](Param param, bool on) {
    Command c;
    c.op = Op::kSet;
    c.param = param;
    c.a = static_cast<std::int32_t>(role);
    c.b = on ? 1 : 0;
    m_engine.push_command(c, m_sink);
    (void)push_panels();
  };
  switch (byte) {
    case 'm':
      toggle(Param::kPartMute, !m_engine.arranger().muted(role));
      return true;
    case 's':
      toggle(Param::kPartSolo, !m_engine.arranger().soloed(role));
      return true;
    default:
      return true;  // the parts panel owns its keystrokes (like the styles panel)
  }
}

bool Shell::groove_focused() const {
  return m_panels.focus_kind() == PanelFocus::kPanel &&
         m_panels.focused_panel() == PanelId::kGroove;
}

void Shell::groove_select(int delta) {
  m_groove_selected =
      std::clamp(m_groove_selected + delta, 0, static_cast<int>(kGrooveRowCount) - 1);
  (void)push_panels();
}

void Shell::groove_adjust(int delta) {
  const GrooveField field = groove_row_field(static_cast<std::size_t>(m_groove_selected));
  const GrooveParams& p = m_engine.arranger().groove_params();
  std::int32_t next = 0;
  switch (field) {
    case GrooveField::kSwing:
      next = std::clamp(static_cast<int>(p.swing) + delta * 10, 0, 100);
      break;
    case GrooveField::kHumanizeTiming:
      next = std::clamp(static_cast<int>(p.humanize_timing) + delta * 10, 0, 100);
      break;
    case GrooveField::kHumanizeVelocity:
      next = std::clamp(static_cast<int>(p.humanize_velocity) + delta * 10, 0, 100);
      break;
    case GrooveField::kAccent:
      next = std::clamp(static_cast<int>(p.accent) + delta * 10, 0, 100);
      break;
    case GrooveField::kSwingGrid:
      next = (p.swing_grid == 16) ? 8 : 16;  // left/right both toggle
      break;
    case GrooveField::kQuantize:
      next = std::clamp(static_cast<int>(p.quantize) + delta * 10, 0, 100);
      break;
    case GrooveField::kSeed:
      return;
  }
  Command c;
  c.op = Op::kSet;
  c.param = Param::kGroove;
  c.a = static_cast<std::int32_t>(field);
  c.b = next;
  m_engine.push_command(c, m_sink);
  (void)push_panels();
}

bool Shell::groove_key(std::uint8_t byte) {
  if (byte == 'r') {  // reseed: a new deterministic humanize pattern
    Command c;
    c.op = Op::kSet;
    c.param = Param::kGroove;
    c.a = static_cast<std::int32_t>(GrooveField::kSeed);
    c.b = static_cast<std::int32_t>(m_engine.arranger().groove_params().seed + 1);
    m_engine.push_command(c, m_sink);
    (void)push_panels();
  }
  return true;  // the groove panel owns its keystrokes
}

bool Shell::arp_panel_focused() const {
  return m_panels.focus_kind() == PanelFocus::kPanel &&
         m_panels.focused_panel() == PanelId::kArp;
}

void Shell::arp_select(int delta) {
  m_arp_selected = std::clamp(m_arp_selected + delta, 0, static_cast<int>(kArpRowCount) - 1);
  (void)push_panels();
}

void Shell::arp_adjust(int delta) {
  const ArpeggiatorParams& p = m_engine.arp().params();
  const bool enabled = m_engine.arp_enabled();
  ArpField field = ArpField::kEnabled;
  std::int32_t value = 0;
  switch (static_cast<ArpRow>(m_arp_selected)) {
    case ArpRow::kEnabled:
      field = ArpField::kEnabled;
      value = enabled ? 0 : 1;  // left/right both toggle
      break;
    case ArpRow::kRate:
      field = ArpField::kRate;
      value = std::clamp(static_cast<int>(p.rate) + delta, 0, kArpRateCount - 1);
      break;
    case ArpRow::kDirection:
      field = ArpField::kDirection;
      value = std::clamp(static_cast<int>(p.direction) + delta, 0, kArpDirectionCount - 1);
      break;
    case ArpRow::kOctaves:
      field = ArpField::kOctaves;
      value = std::clamp(static_cast<int>(p.octaves) + delta, 1, 4);
      break;
    case ArpRow::kGate:
      field = ArpField::kGate;
      value = std::clamp(static_cast<int>(p.gate) + delta * 10, 0, 100);
      break;
    case ArpRow::kLatch:
      field = ArpField::kLatch;
      value = p.latch ? 0 : 1;  // toggle
      break;
  }
  Command c;
  c.op = Op::kSet;
  c.param = Param::kArp;
  c.a = static_cast<std::int32_t>(field);
  c.b = value;
  m_engine.push_command(c, m_sink);
  (void)push_panels();
}

bool Shell::arp_panel_key(std::uint8_t /*byte*/) {
  return true;  // the arp panel owns its keystrokes; adjust is via arrows
}

bool Shell::handle_ui_key(std::uint8_t byte) {
  // CTRL+P (0x10) is a GLOBAL play/stop toggle — it works in every focus
  // (repl, piano, chooser) and on every terminal (a plain control byte, and
  // the LineEditor ignores it). Stop when playing, else resume (continue from
  // the current position; from tick 0 that is a play-from-the-top).
  if (byte == kCtrlPlayStop) {
    std::string ignored;
    exec_line(m_engine.transport().playing() ? "transport stop" : "transport continue", ignored);
    return true;
  }

  // CTRL+C (0x03): the terminal runs with ISIG off, so this is a raw byte, not
  // SIGINT — we own it. It ALWAYS quits the app, unconditionally, in every mode
  // (chooser open or not) and never does anything else. Checked before the
  // chooser swallow below so an open chooser can never intercept it.
  if (byte == kCtrlQuit) {
    std::string ignored;
    exec_line("quit", ignored);
    return true;
  }

  // CTRL+Z (0x1A) toggles the grid between 1 and 2 panels per row (global). A
  // raw control byte the line editor ignores, so it works from any focus.
  if (byte == kCtrlLayout) {
    m_panels.toggle_layout();
    console_output("layout: " + std::to_string(m_panels.per_row()) + " per row");
    return true;
  }

  // TAB+digit: a digit typed immediately after TAB focuses panel #N directly.
  // Resolved before every other route so it wins over the styles filter etc.
  const bool await_digit = m_await_panel_digit;
  m_await_panel_digit = false;
  if (await_digit && byte >= '0' && byte <= '9') {
    m_panels.focus_number(byte - '0');
    (void)push_panels();
    return true;
  }

  // Backtick (`) is now a FOCUS SHORTCUT: it gives focus to the styles panel
  // (equivalent to TABbing to it). A plain printable byte, reliably delivered by
  // every terminal and unused by any command, so it works from any focus.
  if (byte == kCtrlChooser) {
    focus_styles();
    return true;
  }

  // TAB cycles focus repl <-> visible panels, even FROM the repl: opening the
  // piano and pressing TAB drops you straight into play mode. Handled BEFORE the
  // styles routing below so TAB always escapes the styles panel. With no panel
  // to focus it falls through so a lone TAB still reaches the editor. It also
  // arms the TAB+digit jump for the next byte.
  if (byte == '\t') {
    if (m_panels.focus_kind() != PanelFocus::kPanel && !m_panels.any_visible()) {
      return false;
    }
    m_panels.focus_next();
    m_await_panel_digit = true;
    (void)push_panels();
    return true;
  }

  // The styles panel absorbed the chooser: while it is focused it owns every
  // other byte (digits/backspace/apply/steps/musical keys) so nothing leaks to
  // the piano or the line editor.
  if (styles_focused()) {
    return chooser_key(byte);
  }

  // The parts mixer owns its keys while focused (m/s mute/solo; up/down arrows
  // arrive via main.cpp). Swallow the rest so nothing leaks to the editor.
  if (parts_focused()) {
    return parts_key(byte);
  }

  if (groove_focused()) {
    return groove_key(byte);
  }

  if (arp_panel_focused()) {
    return arp_panel_key(byte);
  }

  // The chords panel is the harmony surface: while focused it owns the piano key
  // bindings (routed silently to the detect port) plus the shared octave/mode
  // controls, so nothing leaks to the piano or the line editor.
  if (chords_focused()) {
    return chords_key(byte);
  }

  // Every other shortcut/musical key needs a focused panel; with repl focus the
  // byte falls through to the line editor and typing stays exactly as before.
  if (m_panels.focus_kind() != PanelFocus::kPanel) {
    return false;
  }

  if (m_panels.focused_panel() != PanelId::kPiano) {
    return false;  // help/filter focus: keys fall through to the editor
  }

  // SPACE flips the key mode (momentary <-> toggle). It is a mode switch only
  // in piano focus and is never a musical note. Momentary needs true key-release
  // events, so on a terminal that cannot deliver them the switch stays honest:
  // toggle -> momentary is refused with a one-line explanation.
  if (byte == ' ') {
    if (m_piano_key_mode == PianoKeyMode::kMomentary) {
      m_piano_key_mode = PianoKeyMode::kToggle;
      print_line("piano: toggle key mode (press = on, same key again = off)");
    } else if (!m_momentary_available) {
      print_line("this terminal can't do momentary (no key-release) — toggle only");
    } else {
      m_piano_key_mode = PianoKeyMode::kMomentary;
      print_line("piano: momentary key mode (hold to sound; needs a kitty-protocol terminal)");
    }
    return true;
  }

  // Variation/style stepping (-/= sections, _/+ styles). Works in piano focus;
  // the same keys also drive the chooser (chooser_key) while it is up.
  if (style_step_key(byte)) {
    return true;
  }

  const char upper = static_cast<char>(std::toupper(static_cast<int>(byte)));

  // Piano-focus shortcuts take priority over musical keys (none collide).
  // TAB is the way out of play mode; 'P' is deliberately NOT a shortcut — it
  // sits right next to 'O' (C#5) and a stray press must never close the panel.
  switch (upper) {
    case 'N': {
      std::string ignored;
      (void)cmd_notes({"notes", "names", "toggle"}, ignored);
      return true;
    }
    case 'V': {
      const PianoView next = m_piano.view == PianoView::kKeyboard      ? PianoView::kActiveNotes
                             : m_piano.view == PianoView::kActiveNotes ? PianoView::kEventLog
                                                                       : PianoView::kKeyboard;
      m_piano.view = next;
      (void)push_panels();
      return true;
    }
    case 'C':
      m_monitor.clear();
      (void)push_panels();
      return true;
    case 'Z':
      m_panels.toggle_layout();
      (void)push_panels();
      return true;
    case '.': {
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "down"}, ignored);
      return true;
    }
    case '/': {
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "up"}, ignored);
      return true;
    }
    case '[':    // transpose down / up, clamped to +/- two octaves; the piano
    case ']': {  // header always shows the current amount (even 0).
      constexpr int kTransposeLimit = 24;
      const int delta = upper == '[' ? -1 : 1;
      m_piano.transpose = std::clamp(m_piano.transpose + delta, -kTransposeLimit, kTransposeLimit);
      (void)push_panels();
      return true;
    }
    default:
      break;
  }

  // Musical keys: the plain-byte path is ALWAYS toggle (a plain TTY cannot see
  // key-release), so the piano is playable on every terminal even in momentary
  // mode. True momentary press/release arrives via piano_key_event instead.
  if (const PianoKeyBinding* binding = piano_binding_for(byte); binding != nullptr) {
    toggle_surface_key(kPianoInputPort, m_piano_held, binding->key, binding->semitone_from_base);
    return true;
  }

  // Piano focus swallows everything else so stray keys never leak into a
  // half-typed REPL command.
  return true;
}

bool Shell::chords_focused() const {
  return m_panels.focus_kind() == PanelFocus::kPanel &&
         m_panels.focused_panel() == PanelId::kChords;
}

bool Shell::chords_key(std::uint8_t byte) {
  // The chords panel is the HARMONY surface: the SAME piano key bindings play
  // here, but their notes route to kHarmonyInputPort (zone kHarmony) — silent,
  // observed by the ChordDetector, so playing re-harmonizes the band without a
  // sound. Reuses the piano octave/transpose/channel state so a key means the
  // same note on both surfaces (single-finger here = one key -> the scale-aware
  // triad, Phase 1). The plain-byte musical path is toggle on every terminal;
  // kitty momentary press/release routes through piano_key_event.
  if (byte == ' ') {
    // Same momentary<->toggle switch the piano offers — both surfaces share the
    // one key mode. Momentary needs kitty key-release, refused otherwise.
    if (m_piano_key_mode == PianoKeyMode::kMomentary) {
      m_piano_key_mode = PianoKeyMode::kToggle;
      print_line("harmony: toggle key mode (press = on, same key again = off)");
    } else if (!m_momentary_available) {
      print_line("this terminal can't do momentary (no key-release) — toggle only");
    } else {
      m_piano_key_mode = PianoKeyMode::kMomentary;
      print_line("harmony: momentary key mode (hold to steer; needs a kitty-protocol terminal)");
    }
    return true;
  }

  const char upper = static_cast<char>(std::toupper(static_cast<int>(byte)));
  switch (upper) {
    case '.': {  // borrow the piano's octave state so the mapping stays shared
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "down"}, ignored);
      return true;
    }
    case '/': {
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "up"}, ignored);
      return true;
    }
    case '[':
    case ']': {
      constexpr int kTransposeLimit = 24;
      const int delta = upper == '[' ? -1 : 1;
      m_piano.transpose = std::clamp(m_piano.transpose + delta, -kTransposeLimit, kTransposeLimit);
      (void)push_panels();
      return true;
    }
    default:
      break;
  }

  if (const PianoKeyBinding* binding = piano_binding_for(byte); binding != nullptr) {
    toggle_surface_key(kHarmonyInputPort, m_harmony_held, binding->key,
                       binding->semitone_from_base);
    return true;
  }

  // The chords panel swallows everything else so stray keys never leak into a
  // half-typed REPL command.
  return true;
}

void Shell::configure_default_surfaces() {
  // Default two-surface topology (D34(b) split as DATA): the piano port SOUNDS
  // and steers nobody (kMelody), the harmony port is SILENT and IS the chord
  // source (kHarmony + detect on). So a fresh launch immediately does: play
  // piano = sound; focus the chords panel + play = silent re-harmonize.
  m_engine.set_input_zone(kPianoInputPort, InputZone::kMelody);
  m_engine.set_input_zone(kHarmonyInputPort, InputZone::kHarmony);
  m_engine.set_chord_detect(true, kHarmonyInputPort);
  (void)push_panels();
}

const PianoKeyBinding* Shell::piano_binding_for(std::uint8_t byte) const {
  // Case-insensitive; ';' and '\'' have no upper form so they match by byte.
  const char upper = static_cast<char>(std::toupper(static_cast<int>(byte)));
  for (const PianoKeyBinding& binding : default_keymap_white()) {
    if (binding.key == upper || binding.key == static_cast<char>(byte)) {
      return &binding;
    }
  }
  for (const PianoKeyBinding& binding : default_keymap_black()) {
    if (binding.key == upper) {
      return &binding;
    }
  }
  return nullptr;
}

void Shell::set_momentary_available(bool available) {
  m_momentary_available = available;
  // No key-release support means momentary is a lie; drop to toggle now so the
  // piano reflects what the terminal can actually do.
  if (!available && m_piano_key_mode == PianoKeyMode::kMomentary) {
    m_piano_key_mode = PianoKeyMode::kToggle;
  }
}

bool Shell::piano_key_event(char key, bool pressed) {
  // Only a playable surface turns keys into notes (matches handle_ui_key): the
  // piano panel (melody) or the chords panel (harmony). Any other focus lets the
  // caller fall back to the normal byte path.
  const bool piano = m_panels.focus_kind() == PanelFocus::kPanel &&
                     m_panels.focused_panel() == PanelId::kPiano;
  const bool chords = m_panels.focus_kind() == PanelFocus::kPanel &&
                      m_panels.focused_panel() == PanelId::kChords;
  if (!piano && !chords) {
    return false;
  }

  const PianoKeyBinding* binding = piano_binding_for(static_cast<std::uint8_t>(key));
  if (binding == nullptr) {
    return false;  // TAB / SPACE / shortcuts: caller drives the byte path
  }

  // Route to the focused surface: the piano port (kMelody, sounds) or the harmony
  // port (kHarmony, silent + steers). The held sets are separate so a note-off on
  // one surface never clears the other.
  const std::uint8_t port = chords ? kHarmonyInputPort : kPianoInputPort;
  ActiveNoteTracker& held = chords ? m_harmony_held : m_piano_held;

  if (m_piano_key_mode == PianoKeyMode::kToggle) {
    // In toggle mode a key-down toggles; the key-up carries no meaning.
    if (pressed) {
      toggle_surface_key(port, held, binding->key, binding->semitone_from_base);
    }
    return true;
  }

  if (pressed) {
    surface_momentary_on(port, held, binding->key, binding->semitone_from_base);
  } else {
    surface_momentary_off(port, held, binding->key, binding->semitone_from_base);
  }
  return true;
}

}  // namespace arrangrr::host

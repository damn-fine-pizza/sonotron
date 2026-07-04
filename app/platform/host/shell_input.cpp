#include "shell.hpp"
#include "shell_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

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

bool Shell::piano_note_held(std::uint8_t midi_note) const {
  for (std::size_t i = 0; i < m_piano_held.size(); ++i) {
    const ActiveNote& n = m_piano_held.notes()[i];
    if (n.note == midi_note && n.channel == m_piano.channel) {
      return true;
    }
  }
  return false;
}

void Shell::piano_send_note(char key, std::uint8_t midi_note, bool note_on) {
  // Every piano note goes through the same feed_midi -> push_midi_in path real
  // hardware uses (docs/TUI_SPEC.md §1.3): the piano is an input device, never
  // a shortcut into the engine.
  std::uint8_t bytes[3];
  bytes[0] =
      static_cast<std::uint8_t>((note_on ? midi::kNoteOn : midi::kNoteOff) | m_piano.channel);
  bytes[1] = midi_note;
  bytes[2] = note_on ? m_piano.velocity : kPianoReleaseVelocity;

  m_pending_source_key = key;
  feed_midi(kPianoInputPort, Span<const std::uint8_t>(bytes, sizeof(bytes)));
  m_pending_source_key = 0;

  (void)push_panels();
}

void Shell::toggle_piano_key(char key, int semitone_from_base) {
  // Toggle note-off policy (H2): a plain TTY delivers no key-release events,
  // so pressing the same key again releases the note. This is also the
  // fallback the plain-byte path always uses, even in momentary mode.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  if (piano_note_held(midi_note)) {
    m_piano_held.note_off(kPianoInputPort, m_piano.channel, midi_note);
    piano_send_note(key, midi_note, false);
    return;
  }

  if (!m_piano_held.note_on({.port = kPianoInputPort,
                             .channel = m_piano.channel,
                             .note = midi_note,
                             .velocity = m_piano.velocity,
                             .source_key = key,
                             .start_tick = 0})) {
    print_line("piano: too many held notes");
    return;
  }
  piano_send_note(key, midi_note, true);
}

void Shell::piano_momentary_on(char key, int semitone_from_base) {
  // Momentary note-on (kitty key-down): sound the note unless it is already
  // sounding. Autorepeat re-presses land here too, so the held check keeps a
  // physically-held key from double-firing note-on.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  if (piano_note_held(midi_note)) {
    return;
  }

  if (!m_piano_held.note_on({.port = kPianoInputPort,
                             .channel = m_piano.channel,
                             .note = midi_note,
                             .velocity = m_piano.velocity,
                             .source_key = key,
                             .start_tick = 0})) {
    print_line("piano: too many held notes");
    return;
  }
  piano_send_note(key, midi_note, true);
}

void Shell::piano_momentary_off(char key, int semitone_from_base) {
  // Momentary note-off (kitty key-up): release the note only if we were
  // sounding it.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    return;
  }

  if (!piano_note_held(midi_note)) {
    return;
  }
  m_piano_held.note_off(kPianoInputPort, m_piano.channel, midi_note);
  piano_send_note(key, midi_note, false);
}

void Shell::piano_all_notes_off() {
  // Release everything the piano is holding through the normal input path.
  while (m_piano_held.size() > 0) {
    const ActiveNote n = m_piano_held.notes()[0];

    std::uint8_t bytes[3] = {static_cast<std::uint8_t>(midi::kNoteOff | n.channel), n.note,
                             kPianoReleaseVelocity};
    m_piano_held.note_off(n.port, n.channel, n.note);
    feed_midi(kPianoInputPort, Span<const std::uint8_t>(bytes, sizeof(bytes)));
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
    piano_all_notes_off();
    return true;
  }

  error = kUsage;
  return false;
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
    toggle_piano_key(binding->key, binding->semitone_from_base);
    return true;
  }

  // Piano focus swallows everything else so stray keys never leak into a
  // half-typed REPL command.
  return true;
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
  // Only piano focus turns keys into notes (matches handle_ui_key). Non-piano
  // focus lets the caller fall back to the normal byte path.
  if (m_panels.focus_kind() != PanelFocus::kPanel || m_panels.focused_panel() != PanelId::kPiano) {
    return false;
  }

  const PianoKeyBinding* binding = piano_binding_for(static_cast<std::uint8_t>(key));
  if (binding == nullptr) {
    return false;  // TAB / SPACE / shortcuts: caller drives the byte path
  }

  if (m_piano_key_mode == PianoKeyMode::kToggle) {
    // In toggle mode a key-down toggles; the key-up carries no meaning.
    if (pressed) {
      toggle_piano_key(binding->key, binding->semitone_from_base);
    }
    return true;
  }

  if (pressed) {
    piano_momentary_on(binding->key, binding->semitone_from_base);
  } else {
    piano_momentary_off(binding->key, binding->semitone_from_base);
  }
  return true;
}

}  // namespace arrangrr::host

#include "shell.hpp"
#include "shell_internal.hpp"

// Shell display/monitor commands: note naming, the event-log filter, view
// options, themes and colours. Bodies moved verbatim from shell.cpp.

namespace arrangrr::host {

using namespace shell_detail;

bool Shell::cmd_notes(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3 || t[1] != "names") {
    error = "notes names cde|doremi|toggle";
    return false;
  }

  if (t[2] == "cde") {
    m_piano.note_naming = NoteNaming::kCde;
  } else if (t[2] == "doremi") {
    m_piano.note_naming = NoteNaming::kDoReMi;
  } else if (t[2] == "toggle") {
    m_piano.note_naming =
        m_piano.note_naming == NoteNaming::kCde ? NoteNaming::kDoReMi : NoteNaming::kCde;
  } else {
    error = "notes names cde|doremi|toggle";
    return false;
  }

  (void)push_panels();
  return true;
}

bool Shell::cmd_filter(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage = "filter channel <1..16> | port <N> | event note-on|note-off | clear";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }

  if (t[1] == "clear") {
    m_filter = MidiEventFilter{};
    (void)push_panels();
    return true;
  }

  if (t[1] == "channel") {
    int ch = 0;
    if (t.size() < 3 || !parse_int(t[2], ch) || ch < 1 || ch > kMidiChannels) {
      error = "filter channel <1..16>";
      return false;
    }
    m_filter.channel = static_cast<std::uint8_t>(ch - 1);
    (void)push_panels();
    return true;
  }

  if (t[1] == "port") {
    int port = 0;
    if (t.size() < 3 || !parse_int(t[2], port) || port < 0 || port >= static_cast<int>(kMaxPorts)) {
      error = "filter port <0.." + std::to_string(kMaxPorts - 1) + ">";
      return false;
    }
    m_filter.port = static_cast<std::uint8_t>(port);
    (void)push_panels();
    return true;
  }

  if (t[1] == "event") {
    if (t.size() < 3) {
      error = "filter event note-on|note-off";
      return false;
    }
    if (t[2] == "note-on") {
      m_filter.event_kind = MidiEventKindFilter::kNoteOn;
    } else if (t[2] == "note-off") {
      m_filter.event_kind = MidiEventKindFilter::kNoteOff;
    } else {
      error = "filter event note-on|note-off";
      return false;
    }
    (void)push_panels();
    return true;
  }

  if (t[1] == "drums") {
    m_filter.instrument = InstrumentFilter::kDrums;
    (void)push_panels();
    return true;
  }

  if (t[1] == "melodic") {
    m_filter.instrument = InstrumentFilter::kMelodic;
    (void)push_panels();
    return true;
  }

  if (t[1] == "velocity") {
    // filter velocity >= N  (only note-ons below N are hidden)
    int vel = 0;
    const bool ok = t.size() >= 4 && t[2] == ">=" && parse_int(t[3], vel) &&
                    vel >= kMidiVelocityMin && vel <= kMidiVelocityMax;
    if (!ok) {
      error = "filter velocity >= <1..127>";
      return false;
    }
    m_filter.velocity_min = static_cast<std::uint8_t>(vel);
    (void)push_panels();
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_view(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "view show note-names|note-numbers|velocity|channel|port on|off | "
      "view show-octaves boundary|all|none | view external-keys on|off | view clear";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }

  if (t[1] == "external-keys") {
    if (t.size() < 3 || (t[2] != "on" && t[2] != "off")) {
      error = "view external-keys on|off";
      return false;
    }
    // Lights arranger/sequencer notes on the keyboard too (H3 overlay).
    m_view_options.show_external_keys = t[2] == "on";
    (void)push_panels();
    return true;
  }

  if (t[1] == "clear") {
    // Clears the visible monitor buffers (the C shortcut does the same).
    m_monitor.clear();
    (void)push_panels();
    return true;
  }

  if (t[1] == "show-octaves") {
    if (t.size() < 3) {
      error = "view show-octaves boundary|all|none";
      return false;
    }
    if (t[2] == "boundary") {
      m_piano.octave_display = OctaveDisplayMode::kBoundary;
    } else if (t[2] == "all") {
      m_piano.octave_display = OctaveDisplayMode::kAll;
    } else if (t[2] == "none") {
      m_piano.octave_display = OctaveDisplayMode::kNone;
    } else {
      error = "view show-octaves boundary|all|none";
      return false;
    }
    (void)push_panels();
    return true;
  }

  if (t[1] == "show" && t.size() >= 4) {
    const bool on = t[3] == "on";
    if (!on && t[3] != "off") {
      error = "view show " + t[2] + " on|off";
      return false;
    }

    if (t[2] == "note-names") {
      m_view_options.show_note_names = on;
    } else if (t[2] == "note-numbers") {
      m_view_options.show_note_numbers = on;
    } else if (t[2] == "velocity") {
      m_view_options.show_velocity = on;
    } else if (t[2] == "channel") {
      m_view_options.show_channel = on;
    } else if (t[2] == "port") {
      m_view_options.show_port = on;
    } else if (t[2] == "drum-names") {
      m_view_options.show_drum_names = on;
    } else {
      error = "view show: unknown option: " + t[2];
      return false;
    }
    (void)push_panels();
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_theme(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage = "theme list | theme set <name> | theme current";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }

  if (t[1] == "list") {
    std::string line = "themes:";
    for (const std::string& name : UiStyle::theme_names()) {
      line += ' ';
      line += name;
      if (name == m_style.theme_name()) {
        line += "*";  // marks the active theme
      }
    }
    print_line(line);
    return true;
  }

  if (t[1] == "current") {
    print_line(std::string("theme: ") + std::string(m_style.theme_name()));
    return true;
  }

  if (t[1] == "set") {
    if (t.size() < 3) {
      error = "theme set <name>";
      return false;
    }
    if (!m_style.set_theme(t[2])) {
      error = "unknown theme '" + t[2] + "' (theme list)";
      return false;
    }
    (void)push_panels();  // re-render so the switch is visible at once
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_colors(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 2) {
    error = "colors on|off|toggle";
    return false;
  }

  if (t[1] == "on") {
    m_style.set_color_mode(ColorMode::kOn);
  } else if (t[1] == "off") {
    m_style.set_color_mode(ColorMode::kOff);
  } else if (t[1] == "toggle") {
    m_style.set_color_mode(m_style.colors_enabled() ? ColorMode::kOff : ColorMode::kOn);
  } else {
    error = "colors on|off|toggle";
    return false;
  }

  (void)push_panels();
  return true;
}

}  // namespace arrangrr::host

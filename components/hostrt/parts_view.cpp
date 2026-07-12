#include "parts_view.hpp"

#include <cstdio>

#include "gm_program.hpp"

namespace arrangrr::host {

namespace {

// Active notes sounding on a part's routed destination — a crude but honest
// live meter (a held pad shows a long bar, a silent part none).
int activity_on(const MidiMonitor& monitor, std::uint8_t port, std::uint8_t channel) {
  const auto& tracker = monitor.active_notes();
  const auto& notes = tracker.notes();
  int count = 0;
  for (std::size_t i = 0; i < tracker.size(); ++i) {
    if (notes[i].port == port && notes[i].channel == channel) {
      ++count;
    }
  }
  return count;
}

std::string voice_label(const PartRowState& row) {
  if (row.is_percussion) {
    return "(kit)";
  }
  if (row.gm_program < 0) {
    return "--";
  }
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%d %s", row.gm_program,
                gm_program_name(static_cast<std::uint8_t>(row.gm_program)));
  return buf;
}

}  // namespace

std::vector<std::string> render_parts_panel(const std::vector<PartRowState>& rows, bool any_solo,
                                            const MidiMonitor& monitor, int selected, int cols,
                                            const UiStyle& style) {
  std::vector<std::string> out;
  out.push_back("  #  part    ch  voice                 M S  act");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const PartRowState& row = rows[i];

    const char cursor = (static_cast<int>(i) == selected) ? '>' : ' ';
    // Channel column: 1-based when routed, "--" when the part has no route.
    char ch[4] = "--";
    if (row.routed) {
      std::snprintf(ch, sizeof(ch), "%2u", static_cast<unsigned>(row.channel) + 1);
    }
    const char mute = row.muted ? 'M' : '.';
    const char solo = row.soloed ? 'S' : '.';

    // Activity meter: one bar per sounding note, capped so a held pad chord
    // never overflows the cell.
    std::string meter;
    if (row.routed) {
      const int active = activity_on(monitor, row.port, row.channel);
      const int bars = active > 6 ? 6 : active;
      meter.assign(static_cast<std::size_t>(bars), '|');
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%c %zu  %-7s %2s  %-20s %c %c  %s", cursor, i,
                  row.name.c_str(), ch, voice_label(row).c_str(), mute, solo, meter.c_str());
    std::string line(buf);

    // Selected row bold, muted rows dimmed; a soloed row reads as normal (it is
    // the one that plays). Selection wins over mute for legibility.
    if (static_cast<int>(i) == selected) {
      line = style.apply(UiRole::kSuccess, line);
    } else if (row.muted || (any_solo && !row.soloed)) {
      line = style.apply(UiRole::kMuted, line);
    }
    out.push_back(ansi::visible_truncate(line, static_cast<std::size_t>(cols > 0 ? cols : 0)));
  }
  out.push_back("up/down part | m mute | i solo | (voice: program cmd) | TAB exit");
  return out;
}

}  // namespace arrangrr::host

#include "parts_view.hpp"

#include <array>
#include <cstdio>

#include "gm_program.hpp"

namespace arrangrr::host {

namespace {

struct PartRow {
  TrackRole role;
  const char* name;
};

// The eight arranger style parts, in mixer order (matches the doc's 0..7).
constexpr std::array<PartRow, kMixerPartCount> kParts = {{
    {.role = TrackRole::kDrums, .name = "Drums"},
    {.role = TrackRole::kPerc, .name = "Perc"},
    {.role = TrackRole::kBass, .name = "Bass"},
    {.role = TrackRole::kChord1, .name = "Chord1"},
    {.role = TrackRole::kChord2, .name = "Chord2"},
    {.role = TrackRole::kPad, .name = "Pad"},
    {.role = TrackRole::kArp, .name = "Arp"},
    {.role = TrackRole::kPhrase, .name = "Phrase"},
}};

// GM percussion lives on channel 10 regardless of program, so drums/perc show
// "(kit)" rather than a melodic voice name.
bool is_percussion_role(TrackRole role) {
  return role == TrackRole::kDrums || role == TrackRole::kPerc;
}

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

std::string voice_label(const Arranger::PartInfo& info, TrackRole role) {
  if (is_percussion_role(role)) {
    return "(kit)";
  }
  if (info.gm_program < 0) {
    return "--";
  }
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%d %s", info.gm_program,
                gm_program_name(static_cast<std::uint8_t>(info.gm_program)));
  return buf;
}

}  // namespace

TrackRole mixer_role(std::size_t index) {
  return kParts[index < kParts.size() ? index : 0].role;
}

std::vector<std::string> render_parts_panel(const Arranger& arranger, const MidiMonitor& monitor,
                                            int selected, int cols, const UiStyle& style) {
  std::vector<std::string> out;
  out.push_back("  #  part    ch  voice                 M S  act");
  for (std::size_t i = 0; i < kParts.size(); ++i) {
    const PartRow& part = kParts[i];
    const Arranger::PartInfo info = arranger.part_info(part.role);

    const char cursor = (static_cast<int>(i) == selected) ? '>' : ' ';
    // Channel column: 1-based when routed, "--" when the part has no route.
    char ch[4] = "--";
    if (info.routed) {
      std::snprintf(ch, sizeof(ch), "%2u", static_cast<unsigned>(info.channel) + 1);
    }
    const char mute = info.muted ? 'M' : '.';
    const char solo = info.soloed ? 'S' : '.';

    // Activity meter: one bar per sounding note, capped so a held pad chord
    // never overflows the cell.
    std::string meter;
    if (info.routed) {
      const int active = activity_on(monitor, info.port, info.channel);
      const int bars = active > 6 ? 6 : active;
      meter.assign(static_cast<std::size_t>(bars), '|');
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%c %zu  %-7s %2s  %-20s %c %c  %s", cursor, i, part.name, ch,
                  voice_label(info, part.role).c_str(), mute, solo, meter.c_str());
    std::string line(buf);

    // Selected row bold, muted rows dimmed; a soloed row reads as normal (it is
    // the one that plays). Selection wins over mute for legibility.
    if (static_cast<int>(i) == selected) {
      line = style.apply(UiRole::kSuccess, line);
    } else if (info.muted || (arranger.any_solo() && !info.soloed)) {
      line = style.apply(UiRole::kMuted, line);
    }
    out.push_back(ansi::visible_truncate(line, static_cast<std::size_t>(cols > 0 ? cols : 0)));
  }
  out.push_back("up/down part | m mute | s solo | (voice: program cmd) | TAB exit");
  return out;
}

}  // namespace arrangrr::host

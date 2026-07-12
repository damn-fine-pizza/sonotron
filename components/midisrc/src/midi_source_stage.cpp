#include "midisrc/midi_source_stage.hpp"

#include <algorithm>
#include <cstdint>

namespace midisrc {

std::vector<SourceEvent> build_source_events(const arrstyle::SmfFile& file) {
  std::vector<SourceEvent> events;
  std::size_t note_count = 0;
  for (const arrstyle::SmfTrack& track : file.tracks) {
    note_count += track.notes.size();
  }
  events.reserve(note_count * 2);  // one on + one off per note

  // Convert from the file's own ticks-per-quarter-note division to the
  // engine's fixed internal PPQN (`arrangrr::kPpqn`) — exact integer scaling.
  // `parse_smf` already rejects SMPTE division and a zero division, so
  // `division` here is always a valid positive ticks-per-quarter value.
  const std::uint32_t division = file.division;
  const auto to_internal_tick = [division](std::uint32_t smf_tick) -> arrangrr::Tick {
    const std::uint64_t scaled = static_cast<std::uint64_t>(smf_tick) * arrangrr::kPpqn / division;
    return static_cast<arrangrr::Tick>(scaled);
  };

  for (const arrstyle::SmfTrack& track : file.tracks) {
    for (const arrstyle::SmfNote& note : track.notes) {
      const arrangrr::Tick on_tick = to_internal_tick(note.tick);
      const arrangrr::Tick off_tick = to_internal_tick(note.tick + note.gate);
      events.push_back(SourceEvent{
          .tick = on_tick,
          .msg = arrangrr::MidiMessage::note_on(note.channel, note.note, note.velocity),
      });
      events.push_back(SourceEvent{
          .tick = off_tick,
          .msg = arrangrr::MidiMessage::note_off(note.channel, note.note),
      });
    }
  }

  // Stable sort: preserves each note's own on-before-off relative order when
  // both land on the same internal tick (a very short gate after scaling).
  std::stable_sort(events.begin(), events.end(),
                   [](const SourceEvent& a, const SourceEvent& b) { return a.tick < b.tick; });
  return events;
}

}  // namespace midisrc

#include "midi_import.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "midisrc/smf.hpp"

namespace arrstyle {

namespace {

constexpr std::uint8_t kGmDrumChannel = 9;  // 0-based channel 10
constexpr std::uint8_t kMaxChannels = 16;
constexpr int kQuarterMultiplier = 4;  // ticks_per_bar = division * num * 4 / den

std::string lower_copy(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// Conservative role inference. Drum channel is unambiguous; otherwise a name
// keyword hint, else a positional default. Never claims more than it knows.
Role infer_role(std::uint8_t channel, const std::string& track_name) {
  if (channel == kGmDrumChannel) {
    return Role::kDrums;
  }
  const std::string name = lower_copy(track_name);
  if (name.find("drum") != std::string::npos || name.find("perc") != std::string::npos) {
    return Role::kPercussion;
  }
  if (name.find("bass") != std::string::npos) {
    return Role::kBass;
  }
  if (name.find("chord") != std::string::npos || name.find("comp") != std::string::npos) {
    return Role::kChord1;
  }
  if (name.find("pad") != std::string::npos || name.find("string") != std::string::npos) {
    return Role::kPad;
  }
  if (name.find("lead") != std::string::npos || name.find("melody") != std::string::npos) {
    return Role::kLead;
  }
  return Role::kPhrase;
}

}  // namespace

bool import_midi(const std::vector<std::uint8_t>& bytes, const std::string& source, StyleModel& out,
                 Diagnostics& diag) {
  SmfFile smf;
  if (!parse_smf(bytes, source, smf, diag)) {
    return false;
  }

  out = StyleModel{};
  out.source_format = SourceFormat::kStandardMidiFile;
  out.source_ppqn = smf.division;
  out.tempo_milli_bpm = smf.tempo_milli_bpm;
  out.time_sig_num = smf.time_sig_num;
  out.time_sig_den = smf.time_sig_den;

  // Name: first non-empty track name, else the source label.
  for (const SmfTrack& t : smf.tracks) {
    if (!t.name.empty()) {
      out.name = t.name;
      break;
    }
  }
  if (out.name.empty()) {
    out.name = source;
  }

  if (!smf.has_tempo) {
    diag.warn("no tempo meta event; assuming 120 BPM", source);
  }
  if (!smf.has_time_sig) {
    diag.warn("no time signature meta event; assuming 4/4", source);
  }

  // Collect notes per channel, remembering the first track name that used it.
  std::array<std::vector<SmfNote>, kMaxChannels> by_channel{};
  std::array<std::string, kMaxChannels> channel_name{};
  for (const SmfTrack& t : smf.tracks) {
    for (const SmfNote& n : t.notes) {
      by_channel[n.channel].push_back(n);
      if (channel_name[n.channel].empty() && !t.name.empty()) {
        channel_name[n.channel] = t.name;
      }
    }
  }

  StyleSection section;
  section.kind = SectionKind::kMain;
  section.variation = SectionVariation::kA;

  for (std::uint8_t ch = 0; ch < kMaxChannels; ++ch) {
    if (by_channel[ch].empty()) {
      continue;
    }
    std::vector<SmfNote>& notes = by_channel[ch];
    std::sort(notes.begin(), notes.end(), [](const SmfNote& a, const SmfNote& b) {
      return a.tick != b.tick ? a.tick < b.tick : a.note < b.note;
    });

    PhraseLane lane;
    lane.role = infer_role(ch, channel_name[ch]);
    lane.source_channel = ch;
    lane.transposition =
        lane.role == Role::kDrums ? TranspositionPolicy::kFixed : TranspositionPolicy::kChordTone;
    lane.retrigger = RetriggerPolicy::kSustain;
    for (const SmfNote& n : notes) {
      lane.events.push_back(PhraseEvent{
          .tick = n.tick, .note = n.note, .velocity = n.velocity, .gate_ticks = n.gate});
    }
    diag.info("channel " + std::to_string(ch + 1) + " -> lane '" +
                  std::string(to_string(lane.role)) + "' (" + std::to_string(lane.events.size()) +
                  " notes)",
              source);
    section.lanes.push_back(std::move(lane));
  }

  if (section.lanes.empty()) {
    diag.error("no note events found in file", source);
    return false;
  }

  // Bars = ceil(total_ticks / ticks_per_bar), at least one.
  const std::uint32_t den = out.time_sig_den == 0 ? 4 : out.time_sig_den;
  const std::uint32_t ticks_per_bar =
      static_cast<std::uint32_t>(smf.division) * out.time_sig_num * kQuarterMultiplier / den;
  std::uint16_t bars = 1;
  if (ticks_per_bar > 0 && smf.total_ticks > 0) {
    bars = static_cast<std::uint16_t>((smf.total_ticks + ticks_per_bar - 1) / ticks_per_bar);
    if (bars == 0) {
      bars = 1;
    }
  }
  section.bars = bars;
  out.sections.push_back(std::move(section));

  // Surface every dropped/unmodelled message class.
  auto report_drop = [&](std::uint32_t count, const char* what) {
    if (count > 0) {
      diag.warn(std::to_string(count) + " " + what +
                    " message(s) dropped (not represented in the style model)",
                source);
    }
  };
  report_drop(smf.dropped_cc, "control change");
  report_drop(smf.dropped_program_change, "program change");
  report_drop(smf.dropped_pitch_bend, "pitch bend");
  report_drop(smf.dropped_aftertouch, "aftertouch");
  report_drop(smf.dropped_sysex, "sysex");
  if (smf.unmatched_note_on > 0) {
    diag.warn(std::to_string(smf.unmatched_note_on) +
                  " note(s) had no matching Note Off; gate clamped to end of track",
              source);
  }
  return true;
}

}  // namespace arrstyle

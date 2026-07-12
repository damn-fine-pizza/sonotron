#include "event_labels.hpp"

#include "arrangrr/abi.hpp"
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/chord/theory.hpp"
#include "common/midi/message.hpp"
#include "note_names.hpp"
#include "runtime/transport.hpp"

namespace arrangrr::host {

const char* warn_name(std::uint16_t code) {
  // One entry per WarnCode: the static_assert refuses to compile a new warn
  // until it has a wire name.
  static constexpr const char* kNames[] = {
      "none",         "scheduler_full",   "route_table_full", "unknown_command",
      "bad_argument", "track_table_full", "not_in_key",       "seq_table_full",
      "seq_empty",    "unsupported",
  };
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == kWarnCodeCount,
                "every WarnCode needs a wire name");
  return code < kWarnCodeCount ? kNames[code] : "unknown";
}

const char* transport_name(std::uint16_t state) {
  switch (static_cast<TransportState>(state)) {
    case TransportState::kPlaying:
      return "playing";
    case TransportState::kPaused:
      return "paused";
    default:
      return "stopped";
  }
}

const char* realtime_name(std::uint8_t status) {
  switch (status) {
    case midi::kClock:
      return "clock";
    case midi::kStart:
      return "start";
    case midi::kContinue:
      return "continue";
    case midi::kStop:
      return "stop";
    case midi::kActiveSensing:
      return "active_sensing";
    case midi::kSystemReset:
      return "reset";
    default:
      return "realtime";
  }
}

std::string note_label(std::uint8_t channel, std::uint8_t note, bool prefer_flats) {
  if (channel == kGmDrumChannelZeroBased) {
    if (const char* drum = gm_drum_name(note)) {
      return drum;
    }
  }

  return note_name(note, {NoteNaming::kCde, prefer_flats, true});
}

const char* section_name(std::uint16_t code) {
  static constexpr const char* kNames[] = {
      "intro1", "intro2", "varA",  "varB",  "varC",    "varD",    "fillA",
      "fillB",  "fillC",  "fillD", "break", "ending1", "ending2",
  };
  return code < kSectionTypeCount ? kNames[code] : "?";
}

const char* quality_suffix(ChordQuality q) {
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

std::string roman_degree(std::uint8_t degree, ChordQuality q) {
  static constexpr const char* kUpper[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
  static constexpr const char* kLower[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
  if (degree == kNoDegree) {
    return "-";  // keyless modes (single/shell)
  }
  if (degree > 6) {
    return "?";
  }
  const bool minor_family = q == ChordQuality::kMin || q == ChordQuality::kMin7 ||
                            q == ChordQuality::kDim || q == ChordQuality::kDim7 ||
                            q == ChordQuality::kHalfDim7;
  std::string out = (minor_family ? kLower : kUpper)[degree];
  if (q == ChordQuality::kHalfDim7) {
    out += "m7b5";
  } else if (q == ChordQuality::kDim || q == ChordQuality::kDim7) {
    out += "dim";
  }
  return out;
}

ChordState followed_state(std::uint8_t packed, bool valid) {
  ChordState s;
  s.root_pc = static_cast<std::uint8_t>(packed & 0x0F);
  s.quality = static_cast<ChordQuality>(packed >> 4);
  s.valid = valid;
  return s;
}

std::uint16_t followed_pcs(const ChordState& s) {
  if (!s.valid) {
    return 0;
  }
  const ChordShape shape = theory::shape_of(s.quality);
  std::uint16_t pcs = 0;
  for (std::uint8_t i = 0; i < shape.count; ++i) {
    const unsigned pc = (s.root_pc + shape.offsets[i]) % 12;
    pcs = static_cast<std::uint16_t>(pcs | (1U << pc));
  }
  return pcs;
}

std::string followed_label(const ChordState& s, bool prefer_flats) {
  if (!s.valid) {
    return "-";
  }
  return pitch_class_name(s.root_pc, {NoteNaming::kCde, prefer_flats, false}) +
         quality_suffix(s.quality);
}

const char* producer_name(std::uint8_t src) {
  switch (static_cast<Producer>(src)) {
    case Producer::kDetect:
      return "detect";
    case Producer::kSequencer:
      return "sequencer";
    case Producer::kManual:
    default:
      return "manual";
  }
}

}  // namespace arrangrr::host

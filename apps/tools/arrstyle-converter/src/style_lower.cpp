#include "style_lower.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <vector>

namespace arrstyle {

namespace {

// ---------------------------------------------------------------------------
// Small local mirrors of runtime tables (see style_lower.hpp's doc comment).
// This tool links neither chorddet nor arrangrr, so these are duplicated, not
// shared; each is commented with the file it must stay in lock-step with.
// ---------------------------------------------------------------------------

struct ChordShape {
  int count = 0;
  int offsets[4] = {0, 0, 0, 0};
};

// Mirrors components/chorddet/include/chorddet/theory.hpp: theory::shape_of().
ChordShape shape_for(ChordQuality q) noexcept {
  switch (q) {
    case ChordQuality::kMaj:
      return {.count = 3, .offsets = {0, 4, 7, 0}};
    case ChordQuality::kMin:
      return {.count = 3, .offsets = {0, 3, 7, 0}};
    case ChordQuality::kDim:
      return {.count = 3, .offsets = {0, 3, 6, 0}};
    case ChordQuality::kAug:
      return {.count = 3, .offsets = {0, 4, 8, 0}};
    case ChordQuality::kMaj7:
      return {.count = 4, .offsets = {0, 4, 7, 11}};
    case ChordQuality::kMin7:
      return {.count = 4, .offsets = {0, 3, 7, 10}};
    case ChordQuality::kDom7:
      return {.count = 4, .offsets = {0, 4, 7, 10}};
    case ChordQuality::kHalfDim7:
      return {.count = 4, .offsets = {0, 3, 6, 10}};
    case ChordQuality::kDim7:
      return {.count = 4, .offsets = {0, 3, 6, 9}};
    case ChordQuality::kSus2:
      return {.count = 3, .offsets = {0, 2, 7, 0}};
    case ChordQuality::kSus4:
      return {.count = 3, .offsets = {0, 5, 7, 0}};
    case ChordQuality::kUnknown:
      return {};
  }
  return {};
}

// Mirrors components/arrangrr/include/arrangrr/arranger/arranger.hpp's private
// Arranger::kRoleAnchor (the per-role register anchor the NTT kernel resolves
// against at playback: note = anchor + chord.root_pc + shape.offsets[tone] +
// 12*octave). Only entries for roles that can legitimately carry a kChordTone
// lane matter; kDrums/kPercussion/kUnassigned are unused placeholders (kFixed
// lanes never reduce; kUnassigned lanes never reach this table).
int role_anchor(Role r) noexcept {
  switch (r) {
    case Role::kBass:
      return 36;
    case Role::kPad:
      return 48;
    case Role::kArp:
    case Role::kPhrase:
    case Role::kLead:
      return 72;
    case Role::kDrums:
    case Role::kPercussion:
    case Role::kChord1:
    case Role::kChord2:
    case Role::kUnassigned:
    default:
      return 60;
  }
}

// Role -> the device TrackRole spelling this pass EMITS AS TEXT (never an
// actual arrangrr::TrackRole value — the tool includes no arrangrr header).
// Returns nullptr for Role::kUnassigned: no device destination exists.
const char* role_track_tag(Role r) noexcept {
  switch (r) {
    case Role::kDrums:
      return "kDrums";
    case Role::kPercussion:
      return "kPerc";
    case Role::kBass:
      return "kBass";
    case Role::kChord1:
      return "kChord1";
    case Role::kChord2:
      return "kChord2";
    case Role::kPad:
      return "kPad";
    case Role::kArp:
      return "kArp";
    case Role::kPhrase:
      return "kPhrase";
    case Role::kLead:
      return "kLead";
    case Role::kUnassigned:
    default:
      return nullptr;
  }
}

const char* policy_tag(TranspositionPolicy p) noexcept {
  return p == TranspositionPolicy::kFixed ? "kFixed" : "kChordTone";
}

// (kind, variation) -> the device SectionType TEXT tag, or nullptr when no
// device slot exists for that combination (only 2 intro / 4 var / 4 fill / 1
// break / 2 ending slots exist).
const char* section_slot_tag(SectionKind kind, SectionVariation var) noexcept {
  switch (kind) {
    case SectionKind::kIntro:
      if (var == SectionVariation::kA) {
        return "Intro1";
      }
      if (var == SectionVariation::kB) {
        return "Intro2";
      }
      return nullptr;
    case SectionKind::kMain:
      switch (var) {
        case SectionVariation::kA:
          return "VarA";
        case SectionVariation::kB:
          return "VarB";
        case SectionVariation::kC:
          return "VarC";
        case SectionVariation::kD:
          return "VarD";
        case SectionVariation::kNone:
        default:
          return nullptr;
      }
    case SectionKind::kFill:
      switch (var) {
        case SectionVariation::kA:
          return "FillA";
        case SectionVariation::kB:
          return "FillB";
        case SectionVariation::kC:
          return "FillC";
        case SectionVariation::kD:
          return "FillD";
        case SectionVariation::kNone:
        default:
          return nullptr;
      }
    case SectionKind::kBreak:
      return "Break";  // a single device slot; variation is irrelevant
    case SectionKind::kEnding:
      if (var == SectionVariation::kA) {
        return "Ending1";
      }
      if (var == SectionVariation::kB) {
        return "Ending2";
      }
      return nullptr;
    default:
      return nullptr;
  }
}

constexpr const char* kSlotOrder[] = {
    "Intro1", "Intro2", "VarA",  "VarB",  "VarC",    "VarD",    "FillA",
    "FillB",  "FillC",  "FillD", "Break", "Ending1", "Ending2",
};
constexpr int kSlotCount = 13;

int slot_index(const std::string& tag) noexcept {
  for (int i = 0; i < kSlotCount; ++i) {
    if (tag == kSlotOrder[i]) {
      return i;
    }
  }
  return -1;
}

// The device's fixed transport resolution (mirrors common/time.hpp's kPpqn).
constexpr std::uint32_t kDevicePpqn = 960;
constexpr std::uint32_t kMinBpmX100 = 2000;
constexpr std::uint32_t kMaxBpmX100 = 40000;

std::uint16_t rescale_step(std::uint32_t tick, std::uint16_t source_ppqn, bool& off_grid) noexcept {
  const std::uint64_t num = static_cast<std::uint64_t>(tick) * 4;
  const std::uint64_t denom = source_ppqn == 0 ? 1 : source_ppqn;
  off_grid = (num % denom) != 0;
  const std::uint64_t rounded = (num + denom / 2) / denom;
  return static_cast<std::uint16_t>(std::min<std::uint64_t>(rounded, 0xFFFFU));
}

std::uint16_t rescale_gate(std::uint32_t gate_ticks, std::uint16_t source_ppqn) noexcept {
  const std::uint64_t num = static_cast<std::uint64_t>(gate_ticks) * kDevicePpqn;
  const std::uint64_t denom = source_ppqn == 0 ? 1 : source_ppqn;
  std::uint64_t rounded = (num + denom / 2) / denom;
  if (rounded == 0) {
    rounded = 1;  // a zero-length gate would never sound; keep it audible
  }
  return static_cast<std::uint16_t>(std::min<std::uint64_t>(rounded, 0xFFFFU));
}

// Reduces an absolute MIDI note against a reference chord (root_pc/shape) into
// a (chord-tone index, octave) pair, inverting the SAME arithmetic the device
// NTT kernel uses at playback: note = anchor + root_pc + offsets[i] + 12*oct.
// Returns false (no output written) when the note's pitch class matches none
// of the reference chord's tones exactly — the mechanical reduction stops
// there rather than guessing the nearest tone.
bool reduce_chord_tone(std::uint8_t note, std::uint8_t root_pc, const ChordShape& shape, int anchor,
                       std::int8_t& tone_index, std::int8_t& octave) noexcept {
  if (shape.count == 0) {
    return false;
  }
  const int pc = static_cast<int>(note) % 12;
  for (int i = 0; i < shape.count; ++i) {
    const int candidate_pc = (static_cast<int>(root_pc) + shape.offsets[i]) % 12;
    if (pc != candidate_pc) {
      continue;
    }
    // pc matched mod 12, so this is always an exact multiple of 12.
    const int numerator =
        static_cast<int>(note) - anchor - static_cast<int>(root_pc) - shape.offsets[i];
    const int oct = numerator / 12;
    if (oct < -128 || oct > 127) {
      continue;  // does not fit the device's int8_t octave field
    }
    tone_index = static_cast<std::int8_t>(i);
    octave = static_cast<std::int8_t>(oct);
    return true;
  }
  return false;
}

bool is_valid_identifier(const std::string& s) noexcept {
  if (s.empty()) {
    return false;
  }
  if (!(s[0] == '_' || (s[0] >= 'a' && s[0] <= 'z'))) {
    return false;
  }
  for (char c : s) {
    const bool ok = c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!ok) {
      return false;
    }
  }
  return true;
}

std::string escape_cpp_literal(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(static_cast<char>(c));
    } else if (c >= 0x20 && c < 0x7F) {
      out.push_back(static_cast<char>(c));
    }
    // control and non-ASCII bytes are dropped: never let a hostile style name
    // break out of the generated string literal.
  }
  return out;
}

std::string section_label(const StyleSection& sec) {
  return std::string(to_string(sec.kind)) + "/" + to_string(sec.variation);
}

struct LoweredEvent {
  std::uint16_t step = 0;
  std::int8_t tone = 0;
  std::int8_t octave = 0;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;
};

struct LoweredPattern {
  const char* role_tag = nullptr;
  const char* policy = nullptr;
  std::vector<LoweredEvent> events;
};

struct LoweredSlot {
  bool used = false;
  std::uint16_t bars = 1;
  std::vector<LoweredPattern> patterns;
};

// Running counters threaded through the whole lowering pass: every dropped
// field/event is tallied here and reported once at the end (DESIGN.md's "no
// silent data loss", applied to the compiler pass too).
struct LowerStats {
  std::size_t sections_dropped_no_slot = 0;
  std::size_t sections_dropped_duplicate_slot = 0;
  std::size_t sections_dropped_empty = 0;
  std::size_t lanes_dropped_unassigned_role = 0;
  std::size_t lanes_dropped_no_reference_chord = 0;
  std::size_t events_dropped_reduction = 0;
  std::size_t events_snapped_off_grid = 0;
  bool any_register_clamp_or_retrigger = false;
};

void lower_fixed_events(const PhraseLane& lane, std::uint16_t source_ppqn,
                        std::vector<LoweredEvent>& out, LowerStats& stats) {
  for (const PhraseEvent& e : lane.events) {
    bool off_grid = false;
    const std::uint16_t step = rescale_step(e.tick, source_ppqn, off_grid);
    if (off_grid) {
      ++stats.events_snapped_off_grid;
    }
    out.push_back(LoweredEvent{
        .step = step,
        .tone = static_cast<std::int8_t>(e.note),  // MIDI note 0..127 fits int8_t
        .octave = 0,
        .vel = e.velocity,
        .gate = rescale_gate(e.gate_ticks, source_ppqn),
    });
  }
}

// Reduces every event of a kChordTone lane against its own reference chord.
// Returns the count of events that failed the mechanical reduction (dropped,
// never guessed) so the caller can report it once per lane.
std::size_t lower_chord_tone_events(const PhraseLane& lane, std::uint16_t source_ppqn,
                                    std::vector<LoweredEvent>& out, LowerStats& stats) {
  const ChordShape shape = shape_for(lane.source_quality);
  const int anchor = role_anchor(lane.role);
  const std::uint8_t root_pc = static_cast<std::uint8_t>(lane.source_root_pc);
  std::size_t dropped = 0;
  for (const PhraseEvent& e : lane.events) {
    std::int8_t tone_index = 0;
    std::int8_t octave = 0;
    if (!reduce_chord_tone(e.note, root_pc, shape, anchor, tone_index, octave)) {
      ++dropped;
      continue;
    }
    bool off_grid = false;
    const std::uint16_t step = rescale_step(e.tick, source_ppqn, off_grid);
    if (off_grid) {
      ++stats.events_snapped_off_grid;
    }
    out.push_back(LoweredEvent{
        .step = step,
        .tone = tone_index,
        .octave = octave,
        .vel = e.velocity,
        .gate = rescale_gate(e.gate_ticks, source_ppqn),
    });
  }
  return dropped;
}

// Lowers one PhraseLane into a LoweredPattern. Returns std::nullopt when the
// lane contributes nothing (an unmapped role, an undecoded reference chord,
// or every event failing reduction) — always with a diagnostic explaining why.
std::optional<LoweredPattern> lower_lane(const StyleSection& sec, const PhraseLane& lane,
                                         std::uint16_t source_ppqn, const std::string& source,
                                         Diagnostics& diag, LowerStats& stats) {
  const char* role_tag = role_track_tag(lane.role);
  if (role_tag == nullptr) {
    diag.warn("section '" + section_label(sec) + "', channel " +
                  std::to_string(lane.source_channel + 1) +
                  ": role 'unassigned' has no device TrackRole destination; lane dropped (" +
                  std::to_string(lane.events.size()) + " event(s))",
              source);
    ++stats.lanes_dropped_unassigned_role;
    return std::nullopt;
  }
  if (lane.note_low != 0 || lane.note_high != 127 ||
      lane.retrigger == RetriggerPolicy::kRetrigger) {
    stats.any_register_clamp_or_retrigger = true;
  }

  LoweredPattern pattern;
  pattern.role_tag = role_tag;
  pattern.policy = policy_tag(lane.transposition);

  if (lane.transposition == TranspositionPolicy::kFixed) {
    lower_fixed_events(lane, source_ppqn, pattern.events, stats);
  } else {
    if (lane.source_root_pc < 0 || lane.source_quality == ChordQuality::kUnknown) {
      diag.warn("section '" + section_label(sec) + "', channel " +
                    std::to_string(lane.source_channel + 1) +
                    ": chord-tone lane has no decoded reference chord (source_root_pc/"
                    "source_quality); cannot reduce mechanically, lane dropped (" +
                    std::to_string(lane.events.size()) + " event(s))",
                source);
      ++stats.lanes_dropped_no_reference_chord;
      return std::nullopt;
    }
    const std::size_t reduction_dropped =
        lower_chord_tone_events(lane, source_ppqn, pattern.events, stats);
    if (reduction_dropped > 0) {
      diag.warn("section '" + section_label(sec) + "', channel " +
                    std::to_string(lane.source_channel + 1) + ": " +
                    std::to_string(reduction_dropped) +
                    " event(s) did not land on a root/3rd/5th/7th chord tone of the lane's "
                    "reference chord (a passing tone, 9th/13th, or similar); dropped rather "
                    "than guessed",
                source);
      stats.events_dropped_reduction += reduction_dropped;
    }
  }

  if (pattern.events.empty()) {
    return std::nullopt;
  }
  return pattern;
}

// Lowers one StyleModel section into a LoweredSlot. Returns std::nullopt when
// no lane survives (every lane dropped, or the section had none to begin
// with) — the caller reports that as an emptied-section diagnostic.
std::optional<LoweredSlot> lower_section(const StyleSection& sec, std::uint16_t source_ppqn,
                                         const std::string& source, Diagnostics& diag,
                                         LowerStats& stats) {
  LoweredSlot slot;
  slot.bars = sec.bars;
  for (const PhraseLane& lane : sec.lanes) {
    std::optional<LoweredPattern> pattern = lower_lane(sec, lane, source_ppqn, source, diag, stats);
    if (pattern.has_value()) {
      slot.patterns.push_back(std::move(*pattern));
    }
  }
  if (slot.patterns.empty()) {
    return std::nullopt;
  }
  slot.used = true;
  return slot;
}

void emit_diagnostics_summary(const std::string& source, const LowerStats& stats,
                              Diagnostics& diag) {
  if (stats.events_snapped_off_grid > 0) {
    diag.warn(std::to_string(stats.events_snapped_off_grid) +
                  " event(s) were not on the device's fixed 16th-grid and were snapped to the "
                  "nearest step (this pass cannot preserve source swing/micro-timing)",
              source);
  }
  if (stats.any_register_clamp_or_retrigger) {
    diag.info(
        "one or more lanes carried a CASM register clamp (note_low/note_high) and/or a "
        "retrigger rule; the current device Style format has no destination field for "
        "either (retrigger is a documented FUTURE NTT input, model.hpp) — dropped",
        source);
  }
  diag.info(
      "gm_program/voicing per pattern and the style-level groove feel have no source in "
      "StyleModel (no Program Change / groove data is captured on import); left at their "
      "historical defaults (-1 / kAsWritten / {}) rather than guessed",
      source);
  const std::size_t total_dropped =
      stats.sections_dropped_no_slot + stats.sections_dropped_duplicate_slot +
      stats.sections_dropped_empty + stats.lanes_dropped_unassigned_role +
      stats.lanes_dropped_no_reference_chord + stats.events_dropped_reduction;
  if (total_dropped > 0) {
    diag.info(
        "lowering summary: " + std::to_string(stats.sections_dropped_no_slot) +
            " section(s) with no device slot, " +
            std::to_string(stats.sections_dropped_duplicate_slot) + " duplicate-slot section(s), " +
            std::to_string(stats.sections_dropped_empty) +
            " section(s) emptied by dropped lanes, " +
            std::to_string(stats.lanes_dropped_unassigned_role) + " unassigned-role lane(s), " +
            std::to_string(stats.lanes_dropped_no_reference_chord) +
            " lane(s) with no reference chord, " + std::to_string(stats.events_dropped_reduction) +
            " event(s) failed chord-tone reduction — all dropped, none guessed",
        source);
  }
}

std::string emit_header(const StyleModel& model, const StyleLowerOptions& opts,
                        const std::array<LoweredSlot, kSlotCount>& slots) {
  std::ostringstream os;
  os << "#pragma once\n\n";
  os << "#include \"arrangrr/arranger/style_model.hpp\"\n\n";
  os << "// GENERATED by arrstyle-converter's style-lowering pass (Phase-5 Item #8, first\n";
  os << "// slice). Source: \"" << escape_cpp_literal(model.name) << "\" ("
     << to_string(model.source_format) << "). NOT wired into arrangrr/arranger/style.hpp's\n";
  os << "// kBuiltins list, any default build, or ci.sh: a standalone, privately-built proof\n";
  os << "// artifact. See the importer/lowering diagnostics for every field this pass could\n";
  os << "// not map.\n\n";
  os << "namespace arrangrr {\n";
  os << "namespace styles {\n";
  os << "namespace " << opts.style_name << " {\n\n";

  for (int i = 0; i < kSlotCount; ++i) {
    const LoweredSlot& slot = slots[static_cast<std::size_t>(i)];
    if (!slot.used) {
      continue;
    }
    for (std::size_t p = 0; p < slot.patterns.size(); ++p) {
      const LoweredPattern& pattern = slot.patterns[p];
      os << "inline constexpr StyleEvent k" << kSlotOrder[i] << "_" << p << "Events[] = {";
      for (std::size_t k = 0; k < pattern.events.size(); ++k) {
        const LoweredEvent& ev = pattern.events[k];
        if (k != 0) {
          os << ", ";
        }
        os << "{.step=" << ev.step << ", .tone=" << static_cast<int>(ev.tone)
           << ", .octave=" << static_cast<int>(ev.octave) << ", .vel=" << static_cast<int>(ev.vel)
           << ", .gate=" << ev.gate << "}";
      }
      os << "};\n";
    }
    os << "inline constexpr StylePattern k" << kSlotOrder[i] << "Patterns[] = {";
    for (std::size_t p = 0; p < slot.patterns.size(); ++p) {
      const LoweredPattern& pattern = slot.patterns[p];
      if (p != 0) {
        os << ", ";
      }
      os << "{.role=TrackRole::" << pattern.role_tag << ", .policy=RolePolicy::" << pattern.policy
         << ", .events=Span<const StyleEvent>(k" << kSlotOrder[i] << "_" << p << "Events)}";
    }
    os << "};\n\n";
  }

  os << "inline constexpr StyleSection kSections[] = {";
  bool first_section = true;
  for (int i = 0; i < kSlotCount; ++i) {
    if (!slots[static_cast<std::size_t>(i)].used) {
      continue;
    }
    if (!first_section) {
      os << ", ";
    }
    first_section = false;
    os << "{.type=SectionType::k" << kSlotOrder[i]
       << ", .bars=" << slots[static_cast<std::size_t>(i)].bars
       << ", .patterns=Span<const StylePattern>(k" << kSlotOrder[i] << "Patterns)}";
  }
  os << "};\n\n";

  std::uint32_t bpm_x100 = model.tempo_milli_bpm / 10;
  bpm_x100 = std::max(kMinBpmX100, std::min(kMaxBpmX100, bpm_x100));
  os << "inline constexpr Style kStyle{.name=\"" << escape_cpp_literal(opts.style_name)
     << "\", .sections=Span<const StyleSection>(kSections), .tempo=" << bpm_x100 << "};\n";
  os << "\n}  // namespace " << opts.style_name << "\n";
  os << "\n}  // namespace styles\n";
  os << "}  // namespace arrangrr\n";
  return os.str();
}

}  // namespace

bool lower_style(const StyleModel& model, const StyleLowerOptions& opts, std::string& out_text,
                 Diagnostics& diag) {
  if (!is_valid_identifier(opts.style_name)) {
    diag.error("style_name '" + opts.style_name +
                   "' is not a valid lowercase C++ identifier ([a-z_][a-z0-9_]*); refusing to "
                   "sanitize it — pick a valid name explicitly",
               model.name);
    return false;
  }

  std::array<LoweredSlot, kSlotCount> slots{};
  LowerStats stats;

  for (const StyleSection& sec : model.sections) {
    const char* tag = section_slot_tag(sec.kind, sec.variation);
    if (tag == nullptr) {
      diag.warn("section '" + section_label(sec) +
                    "' has no device SectionType slot (only 2 intro / 4 var / 4 fill / 1 "
                    "break / 2 ending exist); dropped rather than guessed",
                model.name);
      ++stats.sections_dropped_no_slot;
      continue;
    }
    const int idx = slot_index(tag);
    if (slots[static_cast<std::size_t>(idx)].used) {
      diag.warn("SectionType k" + std::string(tag) +
                    " is already populated from an earlier section; a second '" +
                    section_label(sec) +
                    "' section maps to the same slot and is dropped (won't guess which one wins)",
                model.name);
      ++stats.sections_dropped_duplicate_slot;
      continue;
    }

    std::optional<LoweredSlot> slot =
        lower_section(sec, model.source_ppqn, model.name, diag, stats);
    if (!slot.has_value()) {
      diag.warn("section '" + section_label(sec) + "' produced no lowerable lane; dropped",
                model.name);
      ++stats.sections_dropped_empty;
      continue;
    }
    slots[static_cast<std::size_t>(idx)] = std::move(*slot);
  }

  const bool any_used =
      std::any_of(slots.begin(), slots.end(), [](const LoweredSlot& s) { return s.used; });
  if (!any_used) {
    diag.error("no section could be lowered onto a device SectionType slot; nothing to compile",
               model.name);
    return false;
  }

  emit_diagnostics_summary(model.name, stats, diag);
  out_text = emit_header(model, opts, slots);
  return true;
}

}  // namespace arrstyle

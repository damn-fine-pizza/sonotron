#include "sff_import.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include "casm.hpp"
#include "midi_import.hpp"
#include "midisrc/smf.hpp"

namespace arrstyle {

namespace {

bool contains_marker(const std::vector<std::uint8_t>& bytes, const char* marker) {
  const std::string needle = marker;
  if (bytes.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= bytes.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (static_cast<char>(bytes[i + j]) != needle[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

bool starts_with_smf(const std::vector<std::uint8_t>& bytes) {
  return bytes.size() >= 4 && bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' &&
         bytes[3] == 'd';
}

// ----------------------------------------------------------------------------
// Bass register filter (Feature 2).
// ----------------------------------------------------------------------------

constexpr std::uint8_t kBassRegisterLow = 28;     // E1
constexpr std::uint8_t kBassRegisterHigh = 55;    // G3
constexpr std::uint8_t kBassArtefactMargin = 24;  // >2 octaves outside => drop
constexpr int kOctave = 12;

// ----------------------------------------------------------------------------
// CASM -> native mapping.
// ----------------------------------------------------------------------------

constexpr std::uint8_t kYamahaTypeMaj = 0;
constexpr std::uint8_t kYamahaTypeMaj7 = 2;
constexpr std::uint8_t kRetriggerThreshold = 3;  // >=3 = a real retrigger rule
constexpr int kQuarterMultiplier = 4;            // ticks_per_bar = ppqn * num * 4 / den
constexpr std::uint8_t kMinVelocity = 1;
constexpr std::uint8_t kMaxVelocity = 127;

ChordQuality quality_from_source_type(std::uint8_t type) {
  switch (type) {
    case kYamahaTypeMaj:
      return ChordQuality::kMaj;
    case kYamahaTypeMaj7:
      return ChordQuality::kMaj7;
    default:
      return ChordQuality::kUnknown;
  }
}

TranspositionPolicy policy_from_channel(const CasmChannel& c, Role role) {
  // Prefer the decoded NTT where we have it (SFF1): Bypass = literal notes.
  if (c.ntt == NoteTranspositionTable::kBypass) {
    return TranspositionPolicy::kFixed;
  }
  if (c.ntt != NoteTranspositionTable::kUnknown) {
    return TranspositionPolicy::kChordTone;
  }
  // SFF2 (NTT not decoded): derive from the role (drums/perc are literal).
  return (role == Role::kDrums || role == Role::kPercussion) ? TranspositionPolicy::kFixed
                                                             : TranspositionPolicy::kChordTone;
}

RetriggerPolicy retrigger_from_channel(const CasmChannel& c) {
  return c.retrigger >= kRetriggerThreshold ? RetriggerPolicy::kRetrigger
                                            : RetriggerPolicy::kSustain;
}

const CasmSegment* segment_for_section(const CasmData& casm, const std::string& name) {
  for (const CasmSegment& seg : casm.segments) {
    for (const std::string& s : seg.section_names) {
      if (s == name) {
        return &seg;
      }
    }
  }
  return nullptr;
}

const CasmChannel* channel_for_source(const CasmSegment* seg, std::uint8_t source_channel) {
  if (seg == nullptr) {
    return nullptr;
  }
  for (const CasmChannel& c : seg->channels) {
    if (c.source_channel == source_channel) {
      return &c;
    }
  }
  return nullptr;
}

// Yamaha pads embedded track names with NUL/control bytes; keep only the
// leading printable run so the JSON name is clean and byte-stable.
std::string clean_name(const std::string& raw) {
  std::string out;
  for (const char c : raw) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte < 0x20 || byte == 0x7F) {
      break;
    }
    out.push_back(c);
  }
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

std::string style_name(const SmfFile& smf, const std::string& source) {
  for (const SmfTrack& t : smf.tracks) {
    const std::string cleaned = clean_name(t.name);
    if (!cleaned.empty()) {
      return cleaned;
    }
  }
  return source;
}

// A style section resolved to native kind/variation plus its absolute tick span.
struct SectionSpan {
  SectionKind kind = SectionKind::kMain;
  SectionVariation variation = SectionVariation::kA;
  std::string name;
  std::uint32_t start = 0;
  std::uint32_t end = 0;  // exclusive; std::numeric_limits<uint32_t>::max() for the last
};

std::vector<SectionSpan> build_section_spans(const std::vector<StyleMarker>& markers,
                                             std::uint32_t total_ticks) {
  std::vector<SectionSpan> spans;
  for (const StyleMarker& m : markers) {
    SectionSpan s;
    if (parse_style_section(m.name, s.kind, s.variation)) {
      s.name = m.name;
      s.start = m.tick;
      spans.push_back(s);
    }
  }
  std::sort(spans.begin(), spans.end(),
            [](const SectionSpan& a, const SectionSpan& b) { return a.start < b.start; });
  for (std::size_t i = 0; i < spans.size(); ++i) {
    if (i + 1 < spans.size()) {
      spans[i].end = spans[i + 1].start;
    } else {
      spans[i].end = total_ticks >= spans[i].start ? std::numeric_limits<std::uint32_t>::max()
                                                   : spans[i].start;
    }
  }
  return spans;
}

std::uint16_t bars_for_span(std::uint32_t span_ticks, std::uint32_t ppqn, std::uint8_t num,
                            std::uint8_t den) {
  const std::uint32_t denom = den == 0 ? 4 : den;
  const std::uint32_t ticks_per_bar = ppqn * num * kQuarterMultiplier / denom;
  if (ticks_per_bar == 0 || span_ticks == 0) {
    return 1;
  }
  const std::uint32_t bars = (span_ticks + ticks_per_bar - 1) / ticks_per_bar;
  return bars == 0 ? 1 : static_cast<std::uint16_t>(bars);
}

// Applies the bass register filter to a lane in-place; returns the drop count.
std::size_t apply_bass_filter(PhraseLane& lane) {
  std::size_t dropped = 0;
  std::vector<PhraseEvent> kept;
  kept.reserve(lane.events.size());
  for (PhraseEvent e : lane.events) {
    const std::optional<std::uint8_t> folded = normalize_bass_note(e.note);
    if (!folded.has_value()) {
      ++dropped;
      continue;
    }
    e.note = folded.value();
    kept.push_back(e);
  }
  lane.events = std::move(kept);
  return dropped;
}

// Builds one lane from a source channel's notes, using the CASM entry when
// present, else the canonical channel->role convention. Returns the number of
// bass notes dropped by the register filter.
PhraseLane make_lane(std::uint8_t channel, std::vector<PhraseEvent> events, const CasmChannel* cc,
                     const std::string& section_name, const std::string& source, Diagnostics& diag,
                     std::size_t& bass_dropped) {
  std::sort(events.begin(), events.end(), [](const PhraseEvent& a, const PhraseEvent& b) {
    return a.tick != b.tick ? a.tick < b.tick : a.note < b.note;
  });

  PhraseLane lane;
  lane.source_channel = channel;
  if (cc != nullptr) {
    lane.role = role_from_destination_channel(cc->destination_channel);
    lane.transposition = policy_from_channel(*cc, lane.role);
    lane.retrigger = retrigger_from_channel(*cc);
    lane.source_root_pc = static_cast<std::int8_t>(casm_root_pitch_class(cc->source_chord_root));
    lane.source_quality = quality_from_source_type(cc->source_chord_type);
    lane.note_low = cc->note_low;
    lane.note_high = cc->note_high;
  } else {
    lane.role = role_from_destination_channel(channel);
    lane.transposition = (lane.role == Role::kDrums || lane.role == Role::kPercussion)
                             ? TranspositionPolicy::kFixed
                             : TranspositionPolicy::kChordTone;
    diag.info("section '" + section_name + "': no CASM entry for source channel " +
                  std::to_string(channel + 1) + "; using channel-role fallback",
              source);
  }
  lane.events = std::move(events);
  if (lane.role == Role::kBass) {
    bass_dropped += apply_bass_filter(lane);
  }
  return lane;
}

// Builds one StyleSection from a marker span: groups the span's notes by source
// channel (ticks made section-relative, velocity clamped) into role lanes.
StyleSection build_section(const SectionSpan& span, const SmfFile& smf, const CasmData& casm,
                           const std::string& source, Diagnostics& diag,
                           std::size_t& bass_dropped) {
  const CasmSegment* seg = segment_for_section(casm, span.name);
  std::array<std::vector<PhraseEvent>, 16> by_channel{};
  std::uint32_t max_rel_end = 0;
  for (const SmfTrack& t : smf.tracks) {
    for (const SmfNote& n : t.notes) {
      if (n.tick < span.start || n.tick >= span.end) {
        continue;
      }
      const std::uint32_t rel = n.tick - span.start;
      // MIDI velocity is 7-bit; clamp defensively so a parser desync on a
      // malformed file can never emit an out-of-range value.
      const std::uint8_t vel = n.velocity > kMaxVelocity   ? kMaxVelocity
                               : n.velocity < kMinVelocity ? kMinVelocity
                                                           : n.velocity;
      by_channel[n.channel].push_back(
          PhraseEvent{.tick = rel, .note = n.note, .velocity = vel, .gate_ticks = n.gate});
      max_rel_end = std::max(max_rel_end, rel + n.gate);
    }
  }

  StyleSection section;
  section.kind = span.kind;
  section.variation = span.variation;
  for (std::uint8_t ch = 0; ch < 16; ++ch) {
    if (by_channel[ch].empty()) {
      continue;
    }
    section.lanes.push_back(make_lane(ch, std::move(by_channel[ch]), channel_for_source(seg, ch),
                                      span.name, source, diag, bass_dropped));
  }
  if (!section.lanes.empty()) {
    const std::uint32_t span_ticks =
        span.end == std::numeric_limits<std::uint32_t>::max() ? max_rel_end : span.end - span.start;
    section.bars = bars_for_span(span_ticks, smf.division, smf.time_sig_num, smf.time_sig_den);
  }
  return section;
}

// Imports a raw SMF as a single-section style, then tags it as SFF-sourced.
// Used when a .sty carries no decodable CASM / section markers.
bool import_raw_fallback(const std::vector<std::uint8_t>& bytes, const std::string& source,
                         StyleModel& out, Diagnostics& diag) {
  diag.warn("no CASM chord tables / section markers decoded; importing as a raw SMF", source);
  StyleModel raw;
  if (!import_midi(bytes, source, raw, diag)) {
    return false;
  }
  raw.source_format = SourceFormat::kYamahaSff;
  out = std::move(raw);
  return true;
}

}  // namespace

std::optional<std::uint8_t> normalize_bass_note(std::uint8_t note) noexcept {
  int n = note;
  // A note more than two octaves outside the bass register is a percussion
  // artefact, not a bass note: drop it rather than fold it into a plausible pitch.
  if (n > kBassRegisterHigh + kBassArtefactMargin) {
    return std::nullopt;
  }
  if (n < static_cast<int>(kBassRegisterLow) - static_cast<int>(kBassArtefactMargin)) {
    return std::nullopt;
  }
  while (n > kBassRegisterHigh) {
    n -= kOctave;
  }
  while (n < kBassRegisterLow) {
    n += kOctave;
  }
  return static_cast<std::uint8_t>(n);
}

bool looks_like_sff(const std::vector<std::uint8_t>& bytes) {
  return starts_with_smf(bytes) &&
         (contains_marker(bytes, "CASM") || contains_marker(bytes, "Sff1") ||
          contains_marker(bytes, "Sff2") || contains_marker(bytes, "CSEG"));
}

bool sff_is_importable(const std::vector<std::uint8_t>& bytes) {
  if (looks_like_sff(bytes)) {
    return true;
  }
  for (const StyleMarker& m : scan_style_markers(bytes)) {
    SectionKind kind{};
    SectionVariation variation{};
    if (parse_style_section(m.name, kind, variation)) {
      return true;
    }
  }
  return false;
}

void inspect_sff(const std::vector<std::uint8_t>& bytes, const std::string& source,
                 std::ostream& out, Diagnostics& diag) {
  out << "format: sff\n";
  out << "note:   SFF: unsupported subset — inspect-only\n";

  const bool has_casm = contains_marker(bytes, "CASM");
  out << "casm:   " << (has_casm ? "present (not decoded)" : "absent") << '\n';

  if (starts_with_smf(bytes)) {
    // The leading part is a public SMF; extract what is trivial. Our SMF reader
    // reads exactly `ntrks` tracks and ignores the trailing SFF chunks.
    SmfFile smf;
    Diagnostics local;
    if (parse_smf(bytes, source, smf, local)) {
      out << "embedded_smf: yes\n";
      out << "ppqn:   " << smf.division << '\n';
      out << "tempo_milli_bpm: " << smf.tempo_milli_bpm << '\n';
      out << "time_sig: " << static_cast<int>(smf.time_sig_num) << '/'
          << static_cast<int>(smf.time_sig_den) << '\n';
      out << "tracks: " << smf.tracks.size() << '\n';
    } else {
      out << "embedded_smf: unreadable\n";
    }
  } else {
    out << "embedded_smf: no\n";
  }
  diag.info("SFF decoding (CASM/CSEG/OTS) is not implemented; inspect-only", source);
}

bool import_sff(const std::vector<std::uint8_t>& bytes, const std::string& source, StyleModel& out,
                Diagnostics& diag) {
  SmfFile smf;
  if (!parse_smf(bytes, source, smf, diag)) {
    return false;  // parse_smf already recorded an error.
  }

  CasmData casm;
  decode_casm(bytes, source, casm, diag);
  const std::vector<StyleMarker> markers = scan_style_markers(bytes);
  const std::vector<SectionSpan> spans = build_section_spans(markers, smf.total_ticks);

  // Without section markers there is nothing to split on: import as a single
  // raw section. (A CASM-less but marker-carrying style still gets real
  // sections below, with roles inferred from the canonical channel map.)
  if (spans.empty()) {
    return import_raw_fallback(bytes, source, out, diag);
  }
  if (!casm.present) {
    diag.warn("no CASM chord tables; roles inferred from the canonical channel map", source);
  }

  out = StyleModel{};
  out.source_format = SourceFormat::kYamahaSff;
  out.source_ppqn = smf.division;
  out.tempo_milli_bpm = smf.tempo_milli_bpm;
  out.time_sig_num = smf.time_sig_num;
  out.time_sig_den = smf.time_sig_den;
  out.name = style_name(smf, source);

  std::size_t total_bass_dropped = 0;
  for (const SectionSpan& span : spans) {
    StyleSection section = build_section(span, smf, casm, source, diag, total_bass_dropped);
    if (!section.lanes.empty()) {
      out.sections.push_back(std::move(section));
    }
  }

  if (out.sections.empty()) {
    diag.warn("CASM decoded but no section produced notes; importing as a raw SMF", source);
    return import_raw_fallback(bytes, source, out, diag);
  }

  if (total_bass_dropped > 0) {
    diag.warn(std::to_string(total_bass_dropped) +
                  " bass note(s) outside the bass register were dropped as percussion artefacts",
              source);
  }
  diag.info("decoded " + std::to_string(out.sections.size()) + " section(s) from CASM" +
                (casm.sff2 ? " (SFF2)" : " (SFF1)"),
            source);
  return true;
}

}  // namespace arrstyle

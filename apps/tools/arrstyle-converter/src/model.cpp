#include "model.hpp"

namespace arrstyle {

namespace {

// Table-driven name <-> enum: one source of truth per enum keeps the writer
// and the validator in lock-step (a missing name is a compile-time gap).
template <typename Enum>
struct NameEntry {
  Enum value;
  const char* name;
};

template <typename Enum, std::size_t N>
const char* lookup_name(const NameEntry<Enum> (&table)[N], Enum v) noexcept {
  for (const NameEntry<Enum>& e : table) {
    if (e.value == v) {
      return e.name;
    }
  }
  return "unknown";
}

template <typename Enum, std::size_t N>
bool lookup_value(const NameEntry<Enum> (&table)[N], const std::string& s, Enum& out) noexcept {
  for (const NameEntry<Enum>& e : table) {
    if (s == e.name) {
      out = e.value;
      return true;
    }
  }
  return false;
}

constexpr NameEntry<SourceFormat> kSourceFormat[] = {
    {SourceFormat::kUnknown, "unknown"},
    {SourceFormat::kStandardMidiFile, "midi"},
    {SourceFormat::kChordPro, "chordpro"},
    {SourceFormat::kYamahaSff, "sff"},
};

constexpr NameEntry<SectionKind> kSectionKind[] = {
    {SectionKind::kIntro, "intro"}, {SectionKind::kMain, "main"},     {SectionKind::kFill, "fill"},
    {SectionKind::kBreak, "break"}, {SectionKind::kEnding, "ending"},
};

constexpr NameEntry<SectionVariation> kSectionVariation[] = {
    {SectionVariation::kA, "a"}, {SectionVariation::kB, "b"},       {SectionVariation::kC, "c"},
    {SectionVariation::kD, "d"}, {SectionVariation::kNone, "none"},
};

constexpr NameEntry<Role> kRole[] = {
    {Role::kDrums, "drums"},   {Role::kPercussion, "percussion"},
    {Role::kBass, "bass"},     {Role::kChord1, "chord1"},
    {Role::kChord2, "chord2"}, {Role::kPad, "pad"},
    {Role::kArp, "arp"},       {Role::kPhrase, "phrase"},
    {Role::kLead, "lead"},     {Role::kUnassigned, "unassigned"},
};

constexpr NameEntry<TranspositionPolicy> kTransposition[] = {
    {TranspositionPolicy::kFixed, "fixed"},
    {TranspositionPolicy::kChordTone, "chord_tone"},
};

constexpr NameEntry<RetriggerPolicy> kRetrigger[] = {
    {RetriggerPolicy::kSustain, "sustain"},
    {RetriggerPolicy::kRetrigger, "retrigger"},
};

constexpr NameEntry<PhraseSourceHarmony> kHarmony[] = {
    {PhraseSourceHarmony::kChordSequence, "chord_sequence"},
    {PhraseSourceHarmony::kLiveChord, "live_chord"},
};

constexpr NameEntry<ChordQuality> kChordQuality[] = {
    {ChordQuality::kMaj, "maj"},   {ChordQuality::kMin, "min"},
    {ChordQuality::kDim, "dim"},   {ChordQuality::kAug, "aug"},
    {ChordQuality::kMaj7, "maj7"}, {ChordQuality::kMin7, "min7"},
    {ChordQuality::kDom7, "dom7"}, {ChordQuality::kHalfDim7, "half_dim7"},
    {ChordQuality::kDim7, "dim7"}, {ChordQuality::kSus2, "sus2"},
    {ChordQuality::kSus4, "sus4"}, {ChordQuality::kUnknown, "unknown"},
};

}  // namespace

const char* to_string(SourceFormat v) noexcept { return lookup_name(kSourceFormat, v); }
const char* to_string(SectionKind v) noexcept { return lookup_name(kSectionKind, v); }
const char* to_string(SectionVariation v) noexcept { return lookup_name(kSectionVariation, v); }
const char* to_string(Role v) noexcept { return lookup_name(kRole, v); }
const char* to_string(TranspositionPolicy v) noexcept { return lookup_name(kTransposition, v); }
const char* to_string(RetriggerPolicy v) noexcept { return lookup_name(kRetrigger, v); }
const char* to_string(PhraseSourceHarmony v) noexcept { return lookup_name(kHarmony, v); }
const char* to_string(ChordQuality v) noexcept { return lookup_name(kChordQuality, v); }

bool parse_section_kind(const std::string& s, SectionKind& out) noexcept {
  return lookup_value(kSectionKind, s, out);
}
bool parse_section_variation(const std::string& s, SectionVariation& out) noexcept {
  return lookup_value(kSectionVariation, s, out);
}
bool parse_role(const std::string& s, Role& out) noexcept { return lookup_value(kRole, s, out); }
bool parse_transposition_policy(const std::string& s, TranspositionPolicy& out) noexcept {
  return lookup_value(kTransposition, s, out);
}
bool parse_retrigger_policy(const std::string& s, RetriggerPolicy& out) noexcept {
  return lookup_value(kRetrigger, s, out);
}
bool parse_chord_quality(const std::string& s, ChordQuality& out) noexcept {
  return lookup_value(kChordQuality, s, out);
}

}  // namespace arrstyle

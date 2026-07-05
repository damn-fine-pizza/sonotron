#include "chordpro_import.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace arrstyle {

namespace {

std::string trim(const std::string& s) {
  std::size_t begin = 0;
  std::size_t end = s.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
    --end;
  }
  return s.substr(begin, end - begin);
}

std::string lower_copy(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// Letter (A..G) -> pitch class. Returns -1 for a non-letter.
int letter_pc(char c) {
  switch (std::toupper(static_cast<unsigned char>(c))) {
    case 'C':
      return 0;
    case 'D':
      return 2;
    case 'E':
      return 4;
    case 'F':
      return 5;
    case 'G':
      return 7;
    case 'A':
      return 9;
    case 'B':
      return 11;
    default:
      return -1;
  }
}

// Reads a root pitch class (letter + optional #/b) from `s` at `pos`, advancing
// `pos`. Returns -1 if no valid root is present.
int read_root(const std::string& s, std::size_t& pos) {
  if (pos >= s.size()) {
    return -1;
  }
  const int base = letter_pc(s[pos]);
  if (base < 0) {
    return -1;
  }
  ++pos;
  int pc = base;
  while (pos < s.size() && (s[pos] == '#' || s[pos] == 'b')) {
    pc = s[pos] == '#' ? (pc + 1) % 12 : (pc + 11) % 12;
    ++pos;
  }
  return pc;
}

ChordQuality quality_from_suffix(const std::string& raw) {
  const std::string s = raw;
  if (s.empty()) {
    return ChordQuality::kMaj;
  }
  // Order matters: test longer / more specific spellings first.
  if (s == "m7b5" || s == "min7b5" || s == "ø" || s == "m7-5") {
    return ChordQuality::kHalfDim7;
  }
  if (s == "dim7" || s == "o7") {
    return ChordQuality::kDim7;
  }
  if (s == "dim" || s == "o" || s == "°") {
    return ChordQuality::kDim;
  }
  if (s == "maj7" || s == "M7" || s == "ma7" || s == "Δ") {
    return ChordQuality::kMaj7;
  }
  if (s == "m7" || s == "min7" || s == "-7") {
    return ChordQuality::kMin7;
  }
  if (s == "7" || s == "dom7") {
    return ChordQuality::kDom7;
  }
  if (s == "aug" || s == "+") {
    return ChordQuality::kAug;
  }
  if (s == "sus2") {
    return ChordQuality::kSus2;
  }
  if (s == "sus4" || s == "sus") {
    return ChordQuality::kSus4;
  }
  if (s == "m" || s == "min" || s == "-") {
    return ChordQuality::kMin;
  }
  if (s == "maj" || s == "M") {
    return ChordQuality::kMaj;
  }
  // Extended forms (9/11/13/6/add...) fold to their base triad/seventh family.
  if (!s.empty() && (s[0] == 'm' || s[0] == '-')) {
    return ChordQuality::kMin;
  }
  return ChordQuality::kMaj;
}

}  // namespace

bool parse_chord_token(const std::string& token, ChordEvent& out) {
  out.source_text = token;
  std::size_t pos = 0;
  const int root = read_root(token, pos);
  if (root < 0) {
    out.root_pc = -1;
    out.quality = ChordQuality::kUnknown;
    return false;
  }
  out.root_pc = static_cast<std::int8_t>(root);

  // Split off an optional slash bass.
  std::string rest = token.substr(pos);
  std::string bass_part;
  const std::size_t slash = rest.find('/');
  if (slash != std::string::npos) {
    bass_part = rest.substr(slash + 1);
    rest = rest.substr(0, slash);
  }
  out.quality = quality_from_suffix(rest);
  out.bass_pc = -1;
  if (!bass_part.empty()) {
    std::size_t bp = 0;
    const int bass = read_root(bass_part, bp);
    if (bass >= 0) {
      out.bass_pc = static_cast<std::int8_t>(bass);
    }
  }
  return true;
}

bool import_chordpro(const std::string& text, const std::string& source, SongModel& out,
                     Diagnostics& diag) {
  out = SongModel{};
  out.harmony_source = PhraseSourceHarmony::kChordSequence;

  std::uint16_t position = 0;
  std::uint16_t bar = 0;
  std::size_t lyric_lines = 0;
  int line_no = 0;

  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    ++line_no;
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue;
    }

    bool line_has_lyrics = false;

    std::size_t i = 0;
    while (i < line.size()) {
      const char c = line[i];
      if (c == '{') {
        // Directive: {name} or {name: value}.
        const std::size_t close = line.find('}', i);
        if (close == std::string::npos) {
          diag.warn("unterminated directive", source, line_no);
          break;
        }
        const std::string inner = line.substr(i + 1, close - i - 1);
        std::string key = inner;
        std::string value;
        const std::size_t colon = inner.find(':');
        if (colon != std::string::npos) {
          key = inner.substr(0, colon);
          value = trim(inner.substr(colon + 1));
        }
        const std::string lkey = lower_copy(trim(key));
        if (lkey == "title" || lkey == "t") {
          out.name = value;
        } else if (lkey == "tempo" || lkey == "bpm") {
          const int bpm = std::atoi(value.c_str());
          if (bpm > 0) {
            out.tempo_milli_bpm = static_cast<std::uint32_t>(bpm) * 1000;
          }
        } else if (lkey == "key") {
          std::size_t kp = 0;
          const int root = read_root(value, kp);
          if (root >= 0) {
            out.key_root_pc = static_cast<std::int8_t>(root);
            const std::string suffix = lower_copy(value.substr(kp));
            out.key_mode = (suffix.rfind("m", 0) == 0 && suffix.rfind("maj", 0) != 0) ? 1 : 0;
          }
        } else if (lkey == "start_of_verse" || lkey == "sov") {
          out.sections.push_back(SongSection{.label = "Verse", .position = position});
        } else if (lkey == "start_of_chorus" || lkey == "soc") {
          out.sections.push_back(SongSection{.label = "Chorus", .position = position});
        } else if (lkey == "start_of_bridge" || lkey == "sob") {
          out.sections.push_back(SongSection{.label = "Bridge", .position = position});
        } else if (lkey == "comment" || lkey == "c") {
          out.sections.push_back(
              SongSection{.label = value.empty() ? "Comment" : value, .position = position});
        } else if (lkey == "end_of_verse" || lkey == "eov" || lkey == "end_of_chorus" ||
                   lkey == "eoc" || lkey == "end_of_bridge" || lkey == "eob" ||
                   lkey == "subtitle" || lkey == "st" || lkey == "artist" || lkey == "define" ||
                   lkey == "capo") {
          // Recognized but not represented in the SongModel.
        } else {
          diag.info("unsupported directive '" + lkey + "' ignored", source, line_no);
        }
        i = close + 1;
      } else if (c == '[') {
        const std::size_t close = line.find(']', i);
        if (close == std::string::npos) {
          diag.warn("unterminated chord bracket", source, line_no);
          break;
        }
        const std::string token = trim(line.substr(i + 1, close - i - 1));
        ChordEvent chord;
        chord.position = position;
        chord.bar = bar;
        if (!parse_chord_token(token, chord)) {
          diag.warn("unparseable chord '" + token + "'", source, line_no);
        }
        out.chords.push_back(std::move(chord));
        ++position;
        i = close + 1;
      } else if (c == '|') {
        ++bar;
        ++i;
      } else {
        if (std::isspace(static_cast<unsigned char>(c)) == 0) {
          line_has_lyrics = true;
        }
        ++i;
      }
    }

    if (line_has_lyrics) {
      ++lyric_lines;
    }
  }

  if (lyric_lines > 0) {
    diag.info(
        std::to_string(lyric_lines) + " lyric line(s) ignored (ChordPro lyrics carry no timing)",
        source);
  }
  if (out.chords.empty()) {
    diag.error("no chords found", source);
    return false;
  }
  if (out.name.empty()) {
    out.name = source;
  }
  return true;
}

}  // namespace arrstyle

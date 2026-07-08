#include "canon.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "json.hpp"

namespace arrstyle {

namespace {

constexpr int kGridSteps = 16;
constexpr std::size_t kMaxBassSteps = 8;  // 8 nibbles fit a std::uint32_t

// Reads a whole file into `text`. Returns false on open failure.
bool read_file(const std::string& path, std::string& text) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  text = ss.str();
  return true;
}

std::int64_t member_int(const Json& obj, const std::string& key, std::int64_t fallback) {
  const Json* v = obj.find(key);
  if (v != nullptr && v->is_int()) {
    return v->as_int();
  }
  return fallback;
}

std::string member_string(const Json& obj, const std::string& key) {
  const Json* v = obj.find(key);
  if (v != nullptr && v->is_string()) {
    return v->as_string();
  }
  return {};
}

// Splits "0,4,8,12" into onset steps.
std::vector<int> parse_cell_key(const std::string& key) {
  std::vector<int> steps;
  std::size_t i = 0;
  while (i < key.size()) {
    if (key[i] < '0' || key[i] > '9') {
      ++i;
      continue;
    }
    int value = 0;
    while (i < key.size() && key[i] >= '0' && key[i] <= '9') {
      value = value * 10 + (key[i] - '0');
      ++i;
    }
    steps.push_back(value);
  }
  return steps;
}

// Loads a per-genre canon object: { "genres": [ { name, tempo_bpm, ... } ] }.
bool load_per_genre(const Json& root, CanonInput& out) {
  const Json* genres = root.find("genres");
  if (genres == nullptr || !genres->is_array()) {
    return false;
  }
  for (const Json& g : genres->elements()) {
    if (!g.is_object()) {
      continue;
    }
    GenreAggregate ga;
    ga.name = member_string(g, "name");
    ga.tempo_bpm = static_cast<int>(member_int(g, "tempo_bpm", 0));
    ga.time_sig_num = static_cast<int>(member_int(g, "time_sig_num", 4));
    ga.time_sig_den = static_cast<int>(member_int(g, "time_sig_den", 4));
    ga.swing_percent = static_cast<int>(member_int(g, "swing_percent", 0));
    if (const Json* cells = g.find("rhythm_cells"); cells != nullptr && cells->is_array()) {
      for (const Json& c : cells->elements()) {
        CanonCell cell;
        if (const Json* steps = c.find("cell"); steps != nullptr && steps->is_array()) {
          for (const Json& s : steps->elements()) {
            if (s.is_int()) {
              cell.steps.push_back(static_cast<int>(s.as_int()));
            }
          }
        } else if (const Json* key = c.find("key"); key != nullptr && key->is_string()) {
          cell.steps = parse_cell_key(key->as_string());
        }
        cell.count = static_cast<long>(member_int(c, "count", 0));
        ga.rhythm_cells.push_back(std::move(cell));
      }
    }
    if (const Json* bass = g.find("bass_templates"); bass != nullptr && bass->is_array()) {
      for (const Json& b : bass->elements()) {
        CanonBass tpl;
        tpl.degrees = member_string(b, "degrees");
        if (tpl.degrees.empty()) {
          tpl.degrees = member_string(b, "key");
        }
        tpl.count = static_cast<long>(member_int(b, "count", 0));
        if (!tpl.degrees.empty()) {
          ga.bass_templates.push_back(std::move(tpl));
        }
      }
    }
    out.genres.push_back(std::move(ga));
  }
  return true;
}

// Reads a shipped flat histogram file `[{ "key": ..., "count": N }]`.
bool read_histogram(const std::string& path, std::vector<std::pair<std::string, long>>& out) {
  std::string text;
  if (!read_file(path, text)) {
    return false;
  }
  Json root;
  std::string error;
  if (!parse_json(text, root, error) || !root.is_array()) {
    return false;
  }
  for (const Json& e : root.elements()) {
    if (!e.is_object()) {
      continue;
    }
    const Json* key = e.find("key");
    const Json* count = e.find("count");
    if (key != nullptr && key->is_string() && count != nullptr && count->is_int()) {
      out.emplace_back(key->as_string(), static_cast<long>(count->as_int()));
    }
  }
  return true;
}

// Builds a single "corpus" genre from the shipped global histograms.
bool load_flat_histograms(const std::filesystem::path& dir, CanonInput& out) {
  std::vector<std::pair<std::string, long>> cells;
  std::vector<std::pair<std::string, long>> bass;
  const bool has_cells = read_histogram((dir / "rhythm-cells.json").string(), cells);
  const bool has_bass = read_histogram((dir / "bass-templates.json").string(), bass);
  if (!has_cells && !has_bass) {
    return false;
  }
  GenreAggregate ga;
  ga.name = "corpus";
  for (const auto& [key, count] : cells) {
    ga.rhythm_cells.push_back({.steps = parse_cell_key(key), .count = count});
  }
  for (const auto& [key, count] : bass) {
    if (!key.empty()) {
      ga.bass_templates.push_back({.degrees = key, .count = count});
    }
  }
  out.genres.push_back(std::move(ga));
  return true;
}

// Produces a C++ identifier fragment from a genre name ("cha-cha" -> "Chacha").
std::string to_identifier(const std::string& name) {
  std::string id;
  bool first = true;
  for (char c : name) {
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    if (!alnum) {
      continue;
    }
    if (first && c >= 'a' && c <= 'z') {
      id.push_back(static_cast<char>(c - 'a' + 'A'));
    } else {
      id.push_back(c);
    }
    first = false;
  }
  if (id.empty()) {
    id = "Genre";
  }
  return id;
}

// The genre name comes from an untrusted aggregate JSON and is emitted into
// generated C++. Escape it so a hostile name (quotes, backslashes, newlines,
// control bytes) can never close a string literal or a // comment and inject
// source into the header the core later compiles.
std::string escape_cpp_literal(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(static_cast<char>(c));
    } else if (c >= 0x20 && c < 0x7F) {
      out.push_back(static_cast<char>(c));
    }
    // control and non-ASCII bytes are dropped
  }
  return out;
}

std::string comment_safe(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    out.push_back((c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : ' ');
  }
  return out;
}

// Clamp emitted values to their destination field widths so an out-of-range
// aggregate can never produce a header that fails to compile (narrowing in
// aggregate-init is ill-formed).
unsigned clamp_u16(long long v) {
  return static_cast<unsigned>(v < 0 ? 0 : (v > 0xFFFF ? 0xFFFF : v));
}
unsigned clamp_u8(long long v) {
  return static_cast<unsigned>(v < 0 ? 0 : (v > 0xFF ? 0xFF : v));
}

std::string hex16(std::uint16_t v) {
  std::array<char, 8> buf{};
  std::snprintf(buf.data(), buf.size(), "0x%04X", v);
  return std::string(buf.data());
}

std::string hex32(std::uint32_t v) {
  std::array<char, 12> buf{};
  std::snprintf(buf.data(), buf.size(), "0x%08X", v);
  return std::string(buf.data());
}

// Stable sort by count desc, tie-break by a stable string key so the emitted
// table is byte-identical run to run (a golden-test requirement).
template <typename T, typename KeyOf>
void sort_by_count_desc(std::vector<T>& v, KeyOf key_of) {
  std::stable_sort(v.begin(), v.end(), [&](const T& a, const T& b) {
    if (a.count != b.count) {
      return a.count > b.count;
    }
    return key_of(a) < key_of(b);
  });
}

}  // namespace

std::uint8_t degree_to_semitone(const std::string& token) noexcept {
  // Accept an optional leading accidental then a diatonic degree number.
  int accidental = 0;
  std::size_t i = 0;
  while (i < token.size() && (token[i] == 'b' || token[i] == '#')) {
    accidental += (token[i] == 'b') ? -1 : 1;
    ++i;
  }
  if (i >= token.size() || token[i] < '1' || token[i] > '7') {
    return kBassRestNibble;
  }
  const int degree = token[i] - '0';
  // Major-scale semitone of each diatonic degree (1..7).
  static constexpr std::array<int, 8> kDegreeSemitone = {0, 0, 2, 4, 5, 7, 9, 11};
  int semi = kDegreeSemitone[static_cast<std::size_t>(degree)] + accidental;
  semi = ((semi % 12) + 12) % 12;
  return static_cast<std::uint8_t>(semi);
}

std::uint32_t pack_bass_template(const std::string& degrees) noexcept {
  std::uint32_t packed = 0;
  std::size_t step = 0;
  std::size_t i = 0;
  while (i <= degrees.size() && step < kMaxBassSteps) {
    if (i == degrees.size() || degrees[i] == '-') {
      // (empty token handled by the initial fill below)
      ++i;
      continue;
    }
    std::size_t start = i;
    while (i < degrees.size() && degrees[i] != '-') {
      ++i;
    }
    const std::string token = degrees.substr(start, i - start);
    const std::uint8_t nib = degree_to_semitone(token);
    packed |= static_cast<std::uint32_t>(nib & 0x0FU) << (4U * step);
    ++step;
  }
  // Fill remaining steps with the rest sentinel.
  for (; step < kMaxBassSteps; ++step) {
    packed |= static_cast<std::uint32_t>(kBassRestNibble) << (4U * step);
  }
  return packed;
}

std::uint16_t cell_to_mask(const std::vector<int>& steps) noexcept {
  std::uint16_t mask = 0;
  for (int s : steps) {
    if (s >= 0 && s < kGridSteps) {
      mask |= static_cast<std::uint16_t>(1U << static_cast<unsigned>(s));
    }
  }
  return mask;
}

bool load_canon_input(const std::string& kb_path, CanonInput& out, Diagnostics& diag) {
  std::error_code ec;
  const std::filesystem::path path(kb_path);
  if (std::filesystem::is_directory(path, ec)) {
    const std::filesystem::path canon = path / "canon.json";
    if (std::filesystem::exists(canon, ec)) {
      std::string text;
      if (!read_file(canon.string(), text)) {
        diag.error("cannot read " + canon.string(), kb_path);
        return false;
      }
      Json root;
      std::string error;
      if (!parse_json(text, root, error)) {
        diag.error("invalid JSON in " + canon.string() + ": " + error, kb_path);
        return false;
      }
      if (!load_per_genre(root, out)) {
        diag.error("canon.json has no \"genres\" array", kb_path);
        return false;
      }
      return true;
    }
    if (load_flat_histograms(path, out)) {
      diag.info("distilled a single \"corpus\" genre from flat histograms", kb_path);
      return true;
    }
    diag.error("no canon.json or rhythm-cells.json/bass-templates.json in " + kb_path, kb_path);
    return false;
  }

  // Treat kb_path as a per-genre JSON file.
  std::string text;
  if (!read_file(kb_path, text)) {
    diag.error("cannot open KB path: " + kb_path, kb_path);
    return false;
  }
  Json root;
  std::string error;
  if (!parse_json(text, root, error)) {
    diag.error("invalid JSON: " + error, kb_path);
    return false;
  }
  if (!load_per_genre(root, out)) {
    diag.error("expected a top-level object with a \"genres\" array", kb_path);
    return false;
  }
  return true;
}

std::string emit_canon_header(const CanonInput& in, const CanonOptions& opts) {
  std::ostringstream os;
  os << "#pragma once\n"
     << "\n"
     << "// AUTO-GENERATED by arrstyle-converter `build-canon`. Do not edit by hand.\n"
     << "// Source: KB aggregate distillation (top rhythm cells + bass templates).\n"
     << "// Freestanding/constexpr-valid (DESIGN.md D33): only <cstdint>, no heap,\n"
     << "// no std containers. Rhythm cells are 16-step onset masks; bass templates\n"
     << "// pack up to 8 steps as 4-bit semitone offsets from the chord root\n"
     << "// (0..11), 0xF marking a rest.\n"
     << "\n"
     << "#include <cstdint>\n"
     << "\n"
     << "namespace arrangrr {\n"
     << "namespace canon {\n"
     << "\n"
     << "using RhythmCell = std::uint16_t;\n"
     << "using BassTemplate = std::uint32_t;\n"
     << "\n"
     << "struct GenreCanon {\n"
     << "  const char* name;\n"
     << "  std::uint16_t tempo_bpm;\n"
     << "  std::uint8_t time_sig_num;\n"
     << "  std::uint8_t time_sig_den;\n"
     << "  std::uint8_t swing_percent;\n"
     << "  const RhythmCell* rhythm_cells;\n"
     << "  std::uint8_t rhythm_cell_count;\n"
     << "  const BassTemplate* bass_templates;\n"
     << "  std::uint8_t bass_template_count;\n"
     << "};\n"
     << "\n"
     << "inline constexpr std::uint8_t kBassRestNibble = 0x0F;\n"
     << "\n";

  // Work on a mutable copy so we can sort/truncate without touching the input.
  CanonInput work = in;
  std::vector<std::string> ids;
  std::vector<std::size_t> cell_counts;
  std::vector<std::size_t> bass_counts;

  for (GenreAggregate& g : work.genres) {
    const std::string id = to_identifier(g.name);
    ids.push_back(id);

    sort_by_count_desc(g.rhythm_cells, [](const CanonCell& c) { return cell_to_mask(c.steps); });
    sort_by_count_desc(g.bass_templates, [](const CanonBass& b) { return b.degrees; });
    if (g.rhythm_cells.size() > opts.max_cells) {
      g.rhythm_cells.resize(opts.max_cells);
    }
    if (g.bass_templates.size() > opts.max_bass) {
      g.bass_templates.resize(opts.max_bass);
    }
    cell_counts.push_back(g.rhythm_cells.size());
    bass_counts.push_back(g.bass_templates.size());

    os << "// --- " << comment_safe(g.name) << " ---\n";
    os << "inline constexpr RhythmCell k" << id << "RhythmCells[] = {";
    for (std::size_t i = 0; i < g.rhythm_cells.size(); ++i) {
      os << (i == 0 ? "" : ", ") << hex16(cell_to_mask(g.rhythm_cells[i].steps));
    }
    if (g.rhythm_cells.empty()) {
      os << "0x0000";  // keep the array non-empty for a valid definition
    }
    os << "};\n";

    os << "inline constexpr BassTemplate k" << id << "BassTemplates[] = {";
    for (std::size_t i = 0; i < g.bass_templates.size(); ++i) {
      os << (i == 0 ? "" : ", ") << hex32(pack_bass_template(g.bass_templates[i].degrees));
    }
    if (g.bass_templates.empty()) {
      os << "0xFFFFFFFF";  // all-rest sentinel
    }
    os << "};\n\n";
  }

  os << "inline constexpr GenreCanon kGenreCanon[] = {\n";
  for (std::size_t gi = 0; gi < work.genres.size(); ++gi) {
    const GenreAggregate& g = work.genres[gi];
    os << "    {\"" << escape_cpp_literal(g.name) << "\", " << clamp_u16(g.tempo_bpm) << ", "
       << clamp_u8(g.time_sig_num) << ", " << clamp_u8(g.time_sig_den) << ", "
       << clamp_u8(g.swing_percent) << ", k" << ids[gi] << "RhythmCells, "
       << clamp_u8(static_cast<long long>(cell_counts[gi])) << ", k" << ids[gi] << "BassTemplates, "
       << clamp_u8(static_cast<long long>(bass_counts[gi])) << "},\n";
  }
  os << "};\n"
     << "inline constexpr std::uint8_t kGenreCanonCount = " << work.genres.size() << ";\n"
     << "\n"
     << "}  // namespace canon\n"
     << "}  // namespace arrangrr\n";

  return os.str();
}

}  // namespace arrstyle

#include "cli.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "canon.hpp"
#include "chordpro_import.hpp"
#include "diagnostics.hpp"
#include "genre.hpp"
#include "json.hpp"
#include "midi_import.hpp"
#include "serialize.hpp"
#include "sff_import.hpp"
#include "smf.hpp"
#include "validate.hpp"

namespace arrstyle {

namespace {

bool read_binary(const std::string& path, std::vector<std::uint8_t>& out, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string data = ss.str();
  out.assign(data.begin(), data.end());
  return true;
}

bool read_text(const std::string& path, std::string& out, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  out = ss.str();
  return true;
}

bool write_text(const std::string& path, const std::string& data, std::string& error) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "cannot write file: " + path;
    return false;
  }
  file << data;
  return static_cast<bool>(file);
}

bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string to_lower(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

SourceFormat detect_format(const std::string& path, const std::vector<std::uint8_t>& bytes) {
  const std::string lower = to_lower(path);
  if (ends_with(lower, ".sty") || ends_with(lower, ".sst") || ends_with(lower, ".prs") ||
      ends_with(lower, ".bcs") || ends_with(lower, ".pcs")) {
    return SourceFormat::kYamahaSff;
  }
  if (looks_like_sff(bytes)) {
    return SourceFormat::kYamahaSff;
  }
  if (bytes.size() >= 4 && bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' &&
      bytes[3] == 'd') {
    return SourceFormat::kStandardMidiFile;
  }
  if (ends_with(lower, ".mid") || ends_with(lower, ".midi") || ends_with(lower, ".smf")) {
    return SourceFormat::kStandardMidiFile;
  }
  if (ends_with(lower, ".cho") || ends_with(lower, ".chopro") || ends_with(lower, ".chord") ||
      ends_with(lower, ".crd") || ends_with(lower, ".pro")) {
    return SourceFormat::kChordPro;
  }
  // Heuristic: braces or bracketed chords => ChordPro.
  for (std::uint8_t b : bytes) {
    if (b == '{' || b == '[') {
      return SourceFormat::kChordPro;
    }
  }
  return SourceFormat::kUnknown;
}

// Finds "--out <path>" in args (after the subcommand). Returns false if absent.
bool find_out(const std::vector<std::string>& args, std::size_t start, std::string& out) {
  for (std::size_t i = start; i < args.size(); ++i) {
    if (args[i] == "--out" && i + 1 < args.size()) {
      out = args[i + 1];
      return true;
    }
  }
  return false;
}

void print_usage(std::ostream& out) {
  out << "arrstyle-converter — import alien style/song formats into arrangrr native JSON\n\n"
         "usage: arrstyle-converter <command> [args]\n\n"
         "commands:\n"
         "  inspect <file>                         detect format and print a summary\n"
         "  import-midi <in.mid> --out <f.json>    Standard MIDI File -> .arrstyle.json\n"
         "  import-chordpro <in> --out <f.json>    ChordPro -> .arrsong.json\n"
         "  import-sff <in.sty> --out <f.json>     Yamaha SFF (.sty) -> .arrstyle.json\n"
         "  validate <file.json>                   validate an .arrstyle/.arrsong document\n"
         "  infer-genre <file>                     classify a style's genre from content\n"
         "  build-canon [--kb <dir>] --out <f.hpp> distil KB aggregates into a constexpr header\n"
         "              [--max-cells N] [--max-bass N]\n"
         "  help                                   show this message\n"
         "  version                                print the tool version\n";
}

int cmd_inspect(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2) {
    err << "inspect: expected a file argument\n";
    return kExitUsage;
  }
  const std::string& path = args[1];
  std::vector<std::uint8_t> bytes;
  std::string error;
  if (!read_binary(path, bytes, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  Diagnostics diag;
  const SourceFormat fmt = detect_format(path, bytes);
  out << "file:   " << path << '\n';
  out << "bytes:  " << bytes.size() << '\n';

  switch (fmt) {
    case SourceFormat::kStandardMidiFile: {
      SmfFile smf;
      if (!parse_smf(bytes, path, smf, diag)) {
        diag.print(err);
        return kExitFailure;
      }
      out << "format: midi\n";
      out << "smf_format: " << smf.format << '\n';
      out << "ppqn:   " << smf.division << '\n';
      out << "tempo_milli_bpm: " << smf.tempo_milli_bpm << '\n';
      out << "time_sig: " << static_cast<int>(smf.time_sig_num) << '/'
          << static_cast<int>(smf.time_sig_den) << '\n';
      out << "tracks: " << smf.tracks.size() << '\n';
      std::size_t notes = 0;
      for (const SmfTrack& t : smf.tracks) {
        notes += t.notes.size();
      }
      out << "notes:  " << notes << '\n';
      const GenreGuess guess = infer_genre(smf);
      const int conf_pct = static_cast<int>(guess.confidence * 100.0F + 0.5F);
      out << "genre_inferred: " << guess.genre << " (conf " << conf_pct << "%)\n";
      break;
    }
    case SourceFormat::kChordPro: {
      std::string text(bytes.begin(), bytes.end());
      SongModel song;
      if (!import_chordpro(text, path, song, diag)) {
        diag.print(err);
        return kExitFailure;
      }
      out << "format: chordpro\n";
      out << "name:   " << song.name << '\n';
      out << "sections: " << song.sections.size() << '\n';
      out << "chords: " << song.chords.size() << '\n';
      break;
    }
    case SourceFormat::kYamahaSff:
      inspect_sff(bytes, path, out, diag);
      break;
    case SourceFormat::kUnknown:
      out << "format: unknown\n";
      diag.warn("could not detect the file format", path);
      break;
  }
  diag.print(err);
  return kExitOk;
}

int cmd_import_midi(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2) {
    err << "import-midi: expected an input file\n";
    return kExitUsage;
  }
  std::string out_path;
  if (!find_out(args, 2, out_path)) {
    err << "import-midi: missing --out <file>\n";
    return kExitUsage;
  }
  std::vector<std::uint8_t> bytes;
  std::string error;
  if (!read_binary(args[1], bytes, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  Diagnostics diag;
  StyleModel style;
  if (!import_midi(bytes, args[1], style, diag)) {
    diag.print(err);
    return kExitFailure;
  }
  const std::string json = to_json(style).dump() + "\n";
  if (!write_text(out_path, json, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  out << "wrote " << out_path << " (" << style.sections.size() << " section(s))\n";
  diag.print(err);
  return diag.has_errors() ? kExitFailure : kExitOk;
}

int cmd_import_chordpro(const std::vector<std::string>& args, std::ostream& out,
                        std::ostream& err) {
  if (args.size() < 2) {
    err << "import-chordpro: expected an input file\n";
    return kExitUsage;
  }
  std::string out_path;
  if (!find_out(args, 2, out_path)) {
    err << "import-chordpro: missing --out <file>\n";
    return kExitUsage;
  }
  std::string text;
  std::string error;
  if (!read_text(args[1], text, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  Diagnostics diag;
  SongModel song;
  if (!import_chordpro(text, args[1], song, diag)) {
    diag.print(err);
    return kExitFailure;
  }
  const std::string json = to_json(song).dump() + "\n";
  if (!write_text(out_path, json, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  out << "wrote " << out_path << " (" << song.chords.size() << " chord(s))\n";
  diag.print(err);
  return diag.has_errors() ? kExitFailure : kExitOk;
}

int cmd_import_sff(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2) {
    err << "import-sff: expected an input file\n";
    return kExitUsage;
  }
  std::vector<std::uint8_t> bytes;
  std::string error;
  if (!read_binary(args[1], bytes, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  // Only genuine Yamaha styles (SFF markers or section markers) are imported;
  // a plain SMF or other input is refused with a pointer to `inspect`.
  if (!sff_is_importable(bytes)) {
    out << "SFF: not a Yamaha style — inspect-only (use `inspect`, or `import-midi` "
           "for a plain .mid)\n";
    err << "error: " << args[1] << ": not a Yamaha SFF style (no CASM/SFF markers)\n";
    return kExitFailure;
  }
  std::string out_path;
  if (!find_out(args, 2, out_path)) {
    err << "import-sff: missing --out <file>\n";
    return kExitUsage;
  }
  Diagnostics diag;
  StyleModel style;
  if (!import_sff(bytes, args[1], style, diag)) {
    diag.print(err);
    return kExitFailure;
  }
  const std::string json = to_json(style).dump() + "\n";
  if (!write_text(out_path, json, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  out << "wrote " << out_path << " (" << style.sections.size() << " section(s))\n";
  diag.print(err);
  return diag.has_errors() ? kExitFailure : kExitOk;
}

int cmd_validate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2) {
    err << "validate: expected a JSON file argument\n";
    return kExitUsage;
  }
  std::string text;
  std::string error;
  if (!read_text(args[1], text, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  Diagnostics diag;
  const bool ok = validate_text(text, args[1], diag);
  diag.print(err);
  if (ok) {
    out << "valid: " << args[1] << '\n';
    return kExitOk;
  }
  return kExitFailure;
}

int cmd_infer_genre(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2) {
    err << "infer-genre: expected a MIDI/SFF file argument\n";
    return kExitUsage;
  }
  std::vector<std::uint8_t> bytes;
  std::string error;
  if (!read_binary(args[1], bytes, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  Diagnostics diag;
  SmfFile smf;
  if (!parse_smf(bytes, args[1], smf, diag)) {
    diag.print(err);
    return kExitFailure;
  }
  const GenreGuess guess = infer_genre(smf);
  const int conf_pct = static_cast<int>(guess.confidence * 100.0F + 0.5F);
  out << guess.genre << " (conf " << conf_pct << "%)\n";
  return kExitOk;
}

// Finds "--<name> <value>" in args (after the subcommand). Returns false if absent.
bool find_option(const std::vector<std::string>& args, std::size_t start, const std::string& name,
                 std::string& out) {
  for (std::size_t i = start; i < args.size(); ++i) {
    if (args[i] == name && i + 1 < args.size()) {
      out = args[i + 1];
      return true;
    }
  }
  return false;
}

int cmd_build_canon(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  std::string kb_path = "resources/kb/styles/aggregate";
  std::string kb_arg;
  if (find_option(args, 1, "--kb", kb_arg)) {
    kb_path = kb_arg;
  }
  std::string out_path;
  if (!find_out(args, 1, out_path)) {
    err << "build-canon: missing --out <file>\n";
    return kExitUsage;
  }
  CanonOptions opts;
  std::string value;
  if (find_option(args, 1, "--max-cells", value)) {
    opts.max_cells = static_cast<std::size_t>(std::max(0, std::atoi(value.c_str())));
  }
  if (find_option(args, 1, "--max-bass", value)) {
    opts.max_bass = static_cast<std::size_t>(std::max(0, std::atoi(value.c_str())));
  }

  Diagnostics diag;
  CanonInput input;
  if (!load_canon_input(kb_path, input, diag)) {
    diag.print(err);
    return kExitFailure;
  }
  const std::string header = emit_canon_header(input, opts);
  std::string error;
  if (!write_text(out_path, header, error)) {
    err << error << '\n';
    return kExitFailure;
  }
  out << "wrote " << out_path << " (" << input.genres.size() << " genre(s))\n";
  diag.print(err);
  return kExitOk;
}

}  // namespace

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.empty()) {
    print_usage(err);
    return kExitUsage;
  }
  const std::string& cmd = args[0];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    print_usage(out);
    return kExitOk;
  }
  if (cmd == "version" || cmd == "--version") {
    out << "arrstyle-converter " << kSchemaVersion << ".0\n";
    return kExitOk;
  }
  if (cmd == "inspect") {
    return cmd_inspect(args, out, err);
  }
  if (cmd == "import-midi") {
    return cmd_import_midi(args, out, err);
  }
  if (cmd == "import-chordpro") {
    return cmd_import_chordpro(args, out, err);
  }
  if (cmd == "import-sff") {
    return cmd_import_sff(args, out, err);
  }
  if (cmd == "validate") {
    return cmd_validate(args, out, err);
  }
  if (cmd == "infer-genre") {
    return cmd_infer_genre(args, out, err);
  }
  if (cmd == "build-canon") {
    return cmd_build_canon(args, out, err);
  }
  err << "unknown command: " << cmd << "\n\n";
  print_usage(err);
  return kExitUsage;
}

}  // namespace arrstyle

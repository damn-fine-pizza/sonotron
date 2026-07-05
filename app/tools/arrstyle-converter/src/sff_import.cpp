#include "sff_import.hpp"

#include <array>

#include "smf.hpp"

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

}  // namespace

bool looks_like_sff(const std::vector<std::uint8_t>& bytes) {
  return starts_with_smf(bytes) &&
         (contains_marker(bytes, "CASM") || contains_marker(bytes, "Sff1") ||
          contains_marker(bytes, "Sff2") || contains_marker(bytes, "CSEG"));
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

bool import_sff(const std::vector<std::uint8_t>& bytes, const std::string& source,
                Diagnostics& diag) {
  (void)bytes;
  diag.error("SFF import is not implemented (inspect-only in this MVP)", source);
  return false;
}

}  // namespace arrstyle

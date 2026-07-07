#include "gm_program.hpp"

#include <array>
#include <cctype>
#include <cstdlib>

namespace arrangrr::host {

namespace {

// The 128 canonical General MIDI program names, in order (program 0..127).
constexpr std::array<const char*, 128> kGmNames = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavi",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
    "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
    "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
    "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)",
    "Electric Guitar (clean)", "Electric Guitar (muted)", "Overdriven Guitar",
    "Distortion Guitar", "Guitar harmonics",
    "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass",
    "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
    "String Ensemble 1", "String Ensemble 2", "SynthStrings 1", "SynthStrings 2",
    "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
    "French Horn", "Brass Section", "SynthBrass 1", "SynthBrass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
    "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute",
    "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
    "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
    "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
    "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
    "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
    "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
    "Sitar", "Banjo", "Shamisen", "Koto",
    "Kalimba", "Bag pipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
    "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
    "Telephone Ring", "Helicopter", "Applause", "Gunshot",
};

// Lowercases and drops every non-alphanumeric char, so "Electric Bass (finger)",
// "electric-bass-finger" and "ElectricBassFinger" all normalize alike.
std::string normalize(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isalnum(c) != 0) {
      out.push_back(static_cast<char>(std::tolower(c)));
    }
  }
  return out;
}

}  // namespace

const char* gm_program_name(std::uint8_t program) { return kGmNames[program & 0x7F]; }

int parse_gm_program(const std::string& token) {
  // A bare number wins (0..127), so "program synth 40" is unambiguous.
  if (!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])) != 0)) {
    char* end = nullptr;
    const long value = std::strtol(token.c_str(), &end, 10);
    if (end != nullptr && *end == '\0' && value >= 0 && value <= 127) {
      return static_cast<int>(value);
    }
    return -1;  // numeric but out of range / trailing garbage
  }

  const std::string needle = normalize(token);
  if (needle.empty()) {
    return -1;
  }
  // Exact normalized match first.
  for (std::size_t i = 0; i < kGmNames.size(); ++i) {
    if (normalize(kGmNames[i]) == needle) {
      return static_cast<int>(i);
    }
  }
  // Then a UNIQUE substring match (reject ambiguity so a name never surprises).
  int hit = -1;
  for (std::size_t i = 0; i < kGmNames.size(); ++i) {
    if (normalize(kGmNames[i]).find(needle) != std::string::npos) {
      if (hit >= 0) {
        return -1;  // ambiguous: caller should be more specific
      }
      hit = static_cast<int>(i);
    }
  }
  return hit;
}

}  // namespace arrangrr::host

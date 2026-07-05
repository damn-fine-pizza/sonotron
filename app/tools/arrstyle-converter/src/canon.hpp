#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diagnostics.hpp"

// Canon-builder: the bridge from the host KB (rich, std-container JSON produced
// by the extractor) to the DEVICE canon (a compact, freestanding constexpr
// table living in flash, DESIGN.md D33). It reads per-genre aggregate JSON,
// distills a small canonical pattern set (the most frequent rhythm cells and
// bass templates), and EMITS a self-contained C++ header whose only dependency
// is <cstdint> — no heap, no std containers in the generated code.
//
// The emitter itself is an ordinary host program and uses std freely; only its
// OUTPUT is constrained to the freestanding subset.

namespace arrstyle {

// One rhythm cell as a list of 16th-grid onset steps (0..15) plus its frequency.
struct CanonCell {
  std::vector<int> steps;
  long count = 0;
};

// One bass template as a degree string ("1-5-1-5", "1-b3-b3-5") plus frequency.
struct CanonBass {
  std::string degrees;
  long count = 0;
};

// The per-genre aggregate the emitter distils from.
struct GenreAggregate {
  std::string name;
  int tempo_bpm = 0;
  int time_sig_num = 4;
  int time_sig_den = 4;
  int swing_percent = 0;
  std::vector<CanonCell> rhythm_cells;
  std::vector<CanonBass> bass_templates;
};

struct CanonInput {
  std::vector<GenreAggregate> genres;
};

struct CanonOptions {
  std::size_t max_cells = 6;  // top-N rhythm cells kept per genre
  std::size_t max_bass = 4;   // top-N bass templates kept per genre
};

// Sentinel nibble for an unused/rest bass step in a packed BassTemplate.
inline constexpr std::uint8_t kBassRestNibble = 0xF;

// Maps a scale-degree token ("1", "b3", "#4", "b7", ...) to its semitone offset
// from the chord root (0..11). Returns kBassRestNibble for empty/unknown tokens.
std::uint8_t degree_to_semitone(const std::string& token) noexcept;

// Packs a "1-5-1-5"-style degree string into a BassTemplate: 4 bits per step,
// low step in the low nibble, up to 8 steps; unused steps = kBassRestNibble.
std::uint32_t pack_bass_template(const std::string& degrees) noexcept;

// Turns a list of 16th-grid onset steps into a 16-bit onset mask (bit i = step i).
std::uint16_t cell_to_mask(const std::vector<int>& steps) noexcept;

// Loads aggregate data. `kb_path` may be a per-genre canon JSON FILE, or a
// DIRECTORY containing either `canon.json` (per-genre) or the shipped flat
// histograms (`rhythm-cells.json` + `bass-templates.json`, distilled into a
// single "corpus" genre). Returns false and records an error on failure.
bool load_canon_input(const std::string& kb_path, CanonInput& out, Diagnostics& diag);

// Distils `in` (top-N by count, deterministic order) and emits a freestanding
// constexpr C++ header as a string. Pure: no I/O, no diagnostics.
std::string emit_canon_header(const CanonInput& in, const CanonOptions& opts);

}  // namespace arrstyle

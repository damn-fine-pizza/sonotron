#pragma once

#include <string>

#include "smf.hpp"

// Content-based genre inference for the ~546 corpus files whose filename carries
// no genre token. This is a HOST-side heuristic classifier: it looks at what the
// music actually does (tempo band, time signature, the drum kick/snare/hi-hat
// cell, a swing estimate, and note densities), never at the file name. It is a
// guess with a confidence, not a verdict — mislabels are expected and reported
// as a percentage so the caller can decide how much to trust it.

namespace arrstyle {

struct GenreGuess {
  std::string genre = "unknown";  // native lowercase token (pop/rock/disco/...)
  float confidence = 0.0F;        // 0..1; how strongly the signals agreed
};

// Classifies `smf` from its content. Never throws; on empty/degenerate input it
// returns {"unknown", 0}. Deterministic for a given SmfFile.
GenreGuess infer_genre(const SmfFile& smf);

}  // namespace arrstyle

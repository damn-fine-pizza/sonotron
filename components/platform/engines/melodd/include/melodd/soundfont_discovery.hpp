#pragma once

#include <string>

namespace melodd {

// Finds a General MIDI SoundFont (.sf2) on this host, mirroring
// apps/demo/lib/launch.sh's discovery order so `melodd` and the demo
// launcher agree on where to look: `/usr/share/soundfonts/FluidR3_GM.sf2`,
// then `/usr/share/soundfonts/default.sf2`, then any `*.sf2` found directly
// under `/usr/share/soundfonts` (lexicographic order, for determinism).
//
// If `override_path` is non-empty, it is returned as-is (the caller decides
// how to report a missing/invalid override); otherwise the search above
// runs. Returns an empty string if nothing is found and no override was
// given -- never throws, never touches anything outside a plain existence
// check.
std::string find_system_soundfont(const std::string& override_path = "");

}  // namespace melodd

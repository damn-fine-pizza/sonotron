#pragma once

#include <string>

namespace melodd {

// Finds a General MIDI SoundFont (.sf2/.sf3) on this host, mirroring
// apps/demo/lib/launch.sh's discovery order so `melodd` and the demo
// launcher agree on where to look: `<search_dir>/FluidR3_GM.sf2`, then
// `<search_dir>/default.sf2`, then any `*.sf2`/`*.sf3` found directly under
// `<search_dir>` (lexicographic order across both extensions together, for
// determinism -- `.sf3` is a packaging/size choice (Ogg-Vorbis-compressed
// samples), not a lower-quality format, so no extension-based priority).
//
// If `override_path` is non-empty, it is returned as-is (the caller decides
// how to report a missing/invalid override); otherwise the search above
// runs under `search_dir` (defaults to the real system SoundFont directory
// -- overridable so this function stays unit-testable against a scratch
// directory, see test_soundfont_discovery.cpp). Returns an empty string if
// nothing is found and no override was given -- never throws, never touches
// anything outside a plain existence check.
std::string find_system_soundfont(const std::string& override_path = "",
                                  const std::string& search_dir = "/usr/share/soundfonts");

}  // namespace melodd

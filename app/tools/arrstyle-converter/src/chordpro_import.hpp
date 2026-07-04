#pragma once

#include <string>

#include "diagnostics.hpp"
#include "model.hpp"

// ChordPro -> SongModel (an import SUBSET). Recognizes a small set of
// directives (title/key/tempo/section markers), the inline [Chord] tokens, and
// '|' bar separators. Lyrics are intentionally ignored (ChordPro carries no
// reliable timing) and reported as a single info diagnostic — never a silent
// drop of musical data, because no musical data is being dropped.

namespace arrstyle {

bool import_chordpro(const std::string& text, const std::string& source, SongModel& out,
                     Diagnostics& diag);

// Exposed for unit testing: parse a single chord token (without the brackets)
// into root/quality/bass. Returns false on an unparseable root.
bool parse_chord_token(const std::string& token, ChordEvent& out);

}  // namespace arrstyle

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "model.hpp"

// Standard MIDI File -> StyleModel (an import SUBSET). One Main/VarA section is
// produced; notes are grouped into PhraseLanes by MIDI channel, roles inferred
// conservatively (GM drum channel 10 => Drums/fixed; otherwise a name/channel
// heuristic, defaulting to Phrase/chord-tone). Anything not modelled by the
// SMF reader is reported as a warning, never dropped silently.

namespace arrstyle {

bool import_midi(const std::vector<std::uint8_t>& bytes, const std::string& source, StyleModel& out,
                 Diagnostics& diag);

}  // namespace arrstyle

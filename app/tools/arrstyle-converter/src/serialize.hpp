#pragma once

#include "json.hpp"
#include "model.hpp"

// Model -> JSON. Field order is fixed and documented (DESIGN.md) so the output
// is byte-stable: "format"/"version" lead every document, arrays follow model
// order, and every enum is emitted through model.hpp's stable names.

namespace arrstyle {

inline constexpr int kSchemaVersion = 1;

Json to_json(const StyleModel& style);
Json to_json(const SongModel& song);

}  // namespace arrstyle

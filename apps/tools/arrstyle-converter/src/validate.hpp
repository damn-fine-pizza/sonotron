#pragma once

#include <string>

#include "diagnostics.hpp"
#include "json.hpp"

// Structural + semantic validation of an .arrstyle.json / .arrsong.json
// document, working off the same JSON model the writer produces. Errors are
// recorded in `diag`; the CLI turns diag.has_errors() into a non-zero exit.

namespace arrstyle {

// Validates already-parsed JSON. Dispatches on the "format" field.
bool validate_document(const Json& doc, const std::string& source, Diagnostics& diag);

// Convenience: parse `text` then validate. A JSON parse error is itself an
// error diagnostic.
bool validate_text(const std::string& text, const std::string& source, Diagnostics& diag);

}  // namespace arrstyle

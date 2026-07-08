#pragma once

#include <string>

#include "layout_model.hpp"

// A small JSON reader/writer of our own, scoped to the layout file's schema
// only. No third-party dependency, and this is a fresh, gui-sonotron-local
// implementation — it does not reuse hostrt/jsonl (D38: the GUI is a pure
// client) nor apps/tools/arrstyle-converter's json.cpp (a different app's
// internal tool; cross-app linking would be its own layering mistake).
//
// The layout file is ours and trusted, not untrusted input like an
// SFF/CASM import, so a minimal recursive-descent parser bounded to the
// exact subset we emit (a top-level object with "window" and a flat
// "zones" array of scalar-valued objects) is deliberate and sufficient —
// there is no general JSON value type here, by design.

namespace sonotron {

// Parses `text` (the on-disk layout JSON) into `out`. Returns true on
// success; on failure returns false and fills `error` with a 1-based
// line/column message.
bool parse_layout(const std::string& text, Layout& out, std::string& error);

// Serializes `layout` to canonical JSON text (deterministic key order,
// fixed 2-space indentation, no trailing newline — callers add one), the
// exact on-disk format read back by `parse_layout`.
std::string write_layout(const Layout& layout);

// Loads the layout from `path`. If the file does not exist, writes
// `default_layout()` to `path` first and returns that. Returns false
// (leaving `out` untouched) on an I/O or parse failure, with a message in
// `error`.
bool load_or_create_default(const std::string& path, Layout& out, std::string& error);

// Writes `layout` to `path` (creating/overwriting it). Returns false on
// I/O failure, with a message in `error`.
bool save_layout(const std::string& path, const Layout& layout, std::string& error);

}  // namespace sonotron

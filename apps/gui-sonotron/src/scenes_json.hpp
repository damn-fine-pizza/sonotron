#pragma once

#include <string>

#include "grid_model.hpp"

// A small JSON reader/writer of our own, scoped to the scene-name file's
// schema only -- the same hand-rolled idiom as layout_json.hpp (see that
// header's own comment for why: no third-party dependency, a fresh
// gui-sonotron-local implementation, not shared with hostrt/jsonl or
// arrstyle-converter's json.cpp).
//
// repeat-zone-real-contract.md §4/§8b decision 3 (OWNER LOCKED): scene names
// are host-only (GridModel::scene_name/set_scene_name), persisted in a
// `scenes.json` file SIBLING to layout.json -- not folded into layout.json
// itself (that file has its own schema-upgrade/reset discipline, tested by
// test_layout_schema_upgrade.cpp; a renamed scene surviving a hand-edit
// mistake in an unrelated file is a lower-risk shape than growing that
// schema). The file is ours and trusted, not untrusted input, so a minimal
// parser bounded to the exact subset emitted below (a top-level object with
// one flat "scenes" array of strings) is deliberate and sufficient -- there
// is no general JSON value type here, by design, same as layout_json.hpp.

namespace sonotron {

// Parses `text` (the on-disk scenes JSON) and writes every name found into
// `model` via GridModel::set_scene_name, index-by-position in the "scenes"
// array, PLUS (SLICE 4a, additive format extension) every section found into
// `model` via GridModel::set_scene_section, index-by-position in a sibling
// top-level "sections" array of small integers (the raw SectionType byte,
// GridModel::scene_section's own storage shape), PLUS (auto-song fix,
// additive format extension) every per-scene length found into `model` via
// GridModel::set_scene_bars, index-by-position in a sibling top-level
// "bars" array of small positive integers (GridModel::scene_bars' own
// storage shape), PLUS (task #5 additive format extension) every per-scene
// repeat count found into `model` via GridModel::set_scene_repeat, index-by-
// position in a sibling top-level "repeats" array of small positive integers
// or the GridModel::kSceneRepeatInfinite sentinel (GridModel::scene_repeat's
// own storage shape). Fewer than kMaxSceneCount entries in ANY array leaves
// the remaining indices at whatever `model` already had (its own constructor
// default: the bare column number for names, GridModel::kDefaultSectionType
// for sections, GridModel::kDefaultSceneBars for bars, GridModel::
// kDefaultSceneRepeat for repeats); more than kMaxSceneCount entries are
// ignored past that bound, forward-compatible with a file written by a
// future binary with a larger grid. The "sections"/"bars"/"repeats" keys are
// OPTIONAL: a scenes.json written by an older binary (missing any key)
// parses cleanly and every scene keeps the constructor default for the
// missing field(s) -- old files are not a parse failure. Returns true on
// success; on failure returns false and fills `error` with a 1-based
// line/column message, leaving `model` partially updated (same "parse
// failure is a whole-file failure" discipline as layout_json.hpp's
// parse_layout -- the caller is expected to discard `model`'s scene state on
// a `false` return, not trust a partial parse).
bool parse_scenes(const std::string& text, GridModel& model, std::string& error);

// Serializes every one of `model`'s kMaxSceneCount scene names, sections,
// per-scene bar lengths, AND per-scene repeat counts to canonical JSON text
// (deterministic key order -- "scenes" then "sections" then "bars" then
// "repeats" -- fixed 2-space indentation, no trailing newline -- callers add
// one), the exact on-disk format read back by `parse_scenes`.
std::string write_scenes(const GridModel& model);

// Loads scene names from `path` into `model`. If the file does not exist,
// writes `model`'s current names (its own defaults, "1".."N") to `path`
// first and returns true -- mirroring load_or_create_default()'s "missing
// file -> current defaults" shape in layout_json.hpp exactly. Returns false
// (leaving `model` untouched) on an I/O or parse failure, with a message in
// `error`.
bool load_scenes_or_create_default(const std::string& path, GridModel& model, std::string& error);

// Writes `model`'s scene names to `path` (creating/overwriting it). Returns
// false on I/O failure, with a message in `error`.
bool save_scenes(const std::string& path, const GridModel& model, std::string& error);

}  // namespace sonotron

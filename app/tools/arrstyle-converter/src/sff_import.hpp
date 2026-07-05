#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "model.hpp"

// Yamaha SFF (Style File Format) importer. SFF is a REVERSE-ENGINEERED format: a
// .sty file is a Standard MIDI File (the pattern data) with appended CASM / OTS
// chunks. We ship NO proprietary format internals and NO copyrighted style
// content; we only parse user-provided files. `inspect` reports the public SMF
// facts; `import` decodes the CASM chord tables (casm.hpp) into a native, chord-
// relative StyleModel, falling back to a raw SMF import when no CASM is present.

namespace arrstyle {

// Inspect-only: prints an honest capability note plus any trivially-extractable
// SMF facts. Always returns 0-worthy success (records info/warnings only).
void inspect_sff(const std::vector<std::uint8_t>& bytes, const std::string& source,
                 std::ostream& out, Diagnostics& diag);

// Imports a Yamaha .sty into a native StyleModel. Decodes CASM (sections, roles,
// per-channel NTR/NTT transposition rules) so non-drum lanes are chord-relative
// and transposable. Falls back to a raw SMF import (with a warning) when CASM or
// section markers are missing. Returns false (with an error) only on unusable
// input; never throws.
bool import_sff(const std::vector<std::uint8_t>& bytes, const std::string& source, StyleModel& out,
                Diagnostics& diag);

// True if the bytes look like a Yamaha style (embedded SMF + a CASM/Sff marker).
bool looks_like_sff(const std::vector<std::uint8_t>& bytes);

// True if `import_sff` can make a style out of these bytes: either it looks like
// an SFF file, or it carries Yamaha section markers (CASM-less styles still have
// them). A plain SMF with no style markers is NOT importable as a style.
bool sff_is_importable(const std::vector<std::uint8_t>& bytes);

// Bass register filter: octave-folds a bass note into a sane bass register, or
// returns nullopt when the note is a percussion artefact far outside any bass
// octave (the study found spurious high bass notes such as B5). Reused by any
// import path that assigns a bass role.
std::optional<std::uint8_t> normalize_bass_note(std::uint8_t note) noexcept;

}  // namespace arrstyle

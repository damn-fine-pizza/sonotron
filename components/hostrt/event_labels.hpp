#pragma once

#include <cstdint>
#include <string>

#include "arrangrr/chord/chord_engine.hpp"

// Shared OutEvent label-computation helpers (Phase 2 brief, Corelli §15.3/
// §15.4 review "correction #4"): the single source of truth for every
// enum/code -> human-readable label an OutEvent can carry. `to_jsonl`/
// `to_human` (jsonl.cpp) call these; a future in-process OutEvent ->
// BrainEvent decode (Phase 2b) calls the SAME functions, so the JSONL wire
// and the in-process decode cannot drift into two independently-maintained
// label tables the way a copy-per-consumer split would.

namespace arrangrr::host {

const char* warn_name(std::uint16_t code);
const char* transport_name(std::uint16_t state);
const char* realtime_name(std::uint8_t status);
const char* section_name(std::uint16_t code);
// Phase-5 Item #2: the numeric LaunchState a kClip OutEvent carries in
// msg.status (arrangrr::LaunchState, a core-internal enum -- this takes the
// plain wire code, like warn_name/section_name above, so event_labels.hpp
// stays decoupled from clip_matrix.hpp).
const char* clip_state_name(std::uint8_t state);
const char* quality_suffix(ChordQuality quality);
std::string roman_degree(std::uint8_t degree, ChordQuality quality);

// Ch10 (0-based 9) prefers the GM drum name; every other channel, and any
// ch10 note without a conventional GM name, falls back to the scientific
// pitch name (e.g. "E3").
std::string note_label(std::uint8_t channel, std::uint8_t note, bool prefer_flats);

// --- kChordFollowed decode (HOST-side; the core carries only numeric fields) --
// Unpacks one ChordState byte (root_pc low nibble, quality high nibble) into
// the pure-theory ChordState, tagged with its valid latch.
ChordState followed_state(std::uint8_t packed, bool valid);

// The 12-bit pitch-class set (bit0=C .. bit11=B) for a followed ChordState;
// 0 when the chord is not valid.
std::uint16_t followed_pcs(const ChordState& state);

// The same chord label the kChord `out` field renders (pitch class + quality
// suffix, e.g. "Cmaj7", "Am7", "G"); "-" when the chord is not valid.
std::string followed_label(const ChordState& state, bool prefer_flats);

const char* producer_name(std::uint8_t src);

}  // namespace arrangrr::host

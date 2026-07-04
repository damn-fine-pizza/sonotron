#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diagnostics.hpp"

// A minimal, bounds-checked Standard MIDI File reader (hand-rolled, no
// dependency). It parses the header + track chunks, pairs Note On/Off into
// timed notes, and extracts tempo / time signature / track names. Every
// channel message it does NOT model (CC, program change, pitch bend,
// aftertouch, SysEx) is COUNTED, never silently ignored — the importer turns
// those counts into warnings.

namespace arrstyle {

struct SmfNote {
  std::uint32_t tick = 0;    // absolute tick of the Note On
  std::uint8_t channel = 0;  // 0-based
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
  std::uint32_t gate = 0;  // ticks between Note On and its Note Off
};

struct SmfTrack {
  std::string name;
  std::vector<SmfNote> notes;
};

struct SmfFile {
  std::uint16_t format = 0;
  std::uint16_t ntrks = 0;
  std::uint16_t division = 0;  // ticks per quarter note (SMPTE is rejected)
  std::uint32_t tempo_milli_bpm = 120000;
  std::uint8_t time_sig_num = 4;
  std::uint8_t time_sig_den = 4;
  bool has_tempo = false;
  bool has_time_sig = false;
  std::uint32_t total_ticks = 0;
  std::vector<SmfTrack> tracks;

  // Dropped (unmodelled) message counts, surfaced as warnings by the importer.
  std::uint32_t dropped_cc = 0;
  std::uint32_t dropped_program_change = 0;
  std::uint32_t dropped_pitch_bend = 0;
  std::uint32_t dropped_aftertouch = 0;
  std::uint32_t dropped_sysex = 0;
  std::uint32_t unmatched_note_on = 0;
};

// Parses `bytes`. Returns false (and records an error diagnostic) on a
// malformed/unsupported file. `source` labels diagnostics.
bool parse_smf(const std::vector<std::uint8_t>& bytes, const std::string& source, SmfFile& out,
               Diagnostics& diag);

}  // namespace arrstyle

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A minimal, host-only binary file reader. Exported so `MidiSourceStage`
// (`midisrc/midi_source_stage.hpp`) can load an SMF file at construction
// time without duplicating the `ifstream` read that `apps/tools/
// arrstyle-converter/src/cli.cpp`'s own (still-private, still-generic-across-
// formats) `read_binary` already implements for its own callers — this is the
// ONE copy the new host-only consumer (a Stage constructor, not a CLI
// subcommand) needs. Phase 4c, docs/design/orchestrator-pipeline-extraction.md
// §16.5.

namespace midisrc {

// Reads the whole file at `path` into `out`. Returns false (and fills
// `error`) if the file cannot be opened.
bool read_binary_file(const std::string& path, std::vector<std::uint8_t>& out, std::string& error);

// Writes `bytes` to `path`, truncating any existing file. Returns false (and
// fills `error`) if the file cannot be opened for writing. The `export-smf`
// L1 verb (phase-5 infrastructure) is this function's first caller.
bool write_binary_file(const std::string& path, const std::vector<std::uint8_t>& bytes,
                       std::string& error);

}  // namespace midisrc

#pragma once

#include <ostream>
#include <string>
#include <vector>

// The CLI is a thin, testable layer: run() takes argv-style tokens plus output
// streams and returns the process exit code, so tests can drive whole
// subcommands (and assert exit codes) without spawning a process.

namespace arrstyle {

// Exit codes: 0 ok, 1 runtime/import/validation failure, 2 usage error.
inline constexpr int kExitOk = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace arrstyle

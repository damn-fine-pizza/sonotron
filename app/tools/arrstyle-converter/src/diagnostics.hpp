#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

// Diagnostics: a severity + message + source location, collected as a batch.
// Every importer/validator threads one Diagnostics through; the CLI decides the
// process exit code from has_errors(). Unsupported musical features are never
// dropped silently — they surface here as warnings.

namespace arrstyle {

enum class Severity : std::uint8_t {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
};

const char* to_string(Severity s) noexcept;

// A negative line/column means "not applicable" (whole-file diagnostic).
inline constexpr int kNoLocation = -1;

struct Diagnostic {
  Severity severity = Severity::kInfo;
  std::string message;
  std::string source;  // file name or logical context ("" if none)
  int line = kNoLocation;
  int column = kNoLocation;
};

class Diagnostics {
 public:
  void add(Severity severity, std::string message, std::string source = "", int line = kNoLocation,
           int column = kNoLocation);

  void info(std::string message, std::string source = "", int line = kNoLocation,
            int column = kNoLocation);
  void warn(std::string message, std::string source = "", int line = kNoLocation,
            int column = kNoLocation);
  void error(std::string message, std::string source = "", int line = kNoLocation,
             int column = kNoLocation);

  bool has_errors() const noexcept;
  std::size_t count(Severity severity) const noexcept;
  const std::vector<Diagnostic>& items() const noexcept { return m_items; }

  // One line per diagnostic, stable format: "<severity>: <source>:<line>: <msg>".
  void print(std::ostream& out) const;

 private:
  std::vector<Diagnostic> m_items;
};

}  // namespace arrstyle

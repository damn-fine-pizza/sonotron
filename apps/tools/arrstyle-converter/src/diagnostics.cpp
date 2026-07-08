#include "diagnostics.hpp"

namespace arrstyle {

const char* to_string(Severity s) noexcept {
  switch (s) {
    case Severity::kInfo:
      return "info";
    case Severity::kWarning:
      return "warning";
    case Severity::kError:
      return "error";
  }
  return "info";
}

void Diagnostics::add(Severity severity, std::string message, std::string source, int line,
                      int column) {
  m_items.push_back(Diagnostic{.severity = severity,
                               .message = std::move(message),
                               .source = std::move(source),
                               .line = line,
                               .column = column});
}

void Diagnostics::info(std::string message, std::string source, int line, int column) {
  add(Severity::kInfo, std::move(message), std::move(source), line, column);
}

void Diagnostics::warn(std::string message, std::string source, int line, int column) {
  add(Severity::kWarning, std::move(message), std::move(source), line, column);
}

void Diagnostics::error(std::string message, std::string source, int line, int column) {
  add(Severity::kError, std::move(message), std::move(source), line, column);
}

bool Diagnostics::has_errors() const noexcept { return count(Severity::kError) > 0; }

std::size_t Diagnostics::count(Severity severity) const noexcept {
  std::size_t n = 0;
  for (const Diagnostic& d : m_items) {
    if (d.severity == severity) {
      ++n;
    }
  }
  return n;
}

void Diagnostics::print(std::ostream& out) const {
  for (const Diagnostic& d : m_items) {
    out << to_string(d.severity) << ": ";
    if (!d.source.empty()) {
      out << d.source;
      if (d.line >= 0) {
        out << ':' << d.line;
        if (d.column >= 0) {
          out << ':' << d.column;
        }
      }
      out << ": ";
    }
    out << d.message << '\n';
  }
}

}  // namespace arrstyle

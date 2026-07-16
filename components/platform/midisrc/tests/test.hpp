#pragma once

// Minimal host-only test harness — no external dependency, mirroring
// apps/tools/arrstyle-converter/tests/test.hpp (and the core's own
// app/core/tests/test.hpp). Usage: CHECK(expr); return
// midisrc::test::failures() from main().

#include <cstdio>

namespace midisrc::test {

inline int& failures() noexcept {
  static int count = 0;
  return count;
}

inline void report(const char* file, int line, const char* expr) noexcept {
  std::printf("FAIL %s:%d: %s\n", file, line, expr);
  ++failures();
}

}  // namespace midisrc::test

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) midisrc::test::report(__FILE__, __LINE__, #cond); \
  } while (false)

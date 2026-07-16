#pragma once

// Minimal host-only test harness -- no external dependency, mirroring
// components/platform/midisrc/tests/test.hpp. Usage: CHECK(expr); return
// melodd::test::failures() from main().

#include <cstdio>

namespace melodd::test {

inline int& failures() noexcept {
  static int count = 0;
  return count;
}

inline void report(const char* file, int line, const char* expr) noexcept {
  std::printf("FAIL %s:%d: %s\n", file, line, expr);
  ++failures();
}

}  // namespace melodd::test

#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) melodd::test::report(__FILE__, __LINE__, #cond); \
  } while (false)

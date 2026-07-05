#pragma once

// Minimal host-only test harness — no external dependency, mirroring the core
// test harness (app/core/tests/test.hpp). Usage: CHECK(expr); return
// arrstyle::test::failures() from main().

#include <cstdio>

namespace arrstyle::test {

inline int& failures() noexcept {
  static int count = 0;
  return count;
}

inline void report(const char* file, int line, const char* expr) noexcept {
  std::printf("FAIL %s:%d: %s\n", file, line, expr);
  ++failures();
}

}  // namespace arrstyle::test

#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) arrstyle::test::report(__FILE__, __LINE__, #cond); \
  } while (false)

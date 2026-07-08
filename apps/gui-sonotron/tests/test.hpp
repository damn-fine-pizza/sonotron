#pragma once

// Minimal host-only test harness — no external dependency, mirroring the
// project's other local harnesses (e.g. apps/tools/arrstyle-converter's).
// Usage: CHECK(expr); return sonotron::test::failures() from main().

#include <cstdio>

namespace sonotron::test {

inline int& failures() noexcept {
  static int count = 0;
  return count;
}

inline void report(const char* file, int line, const char* expr) noexcept {
  std::printf("FAIL %s:%d: %s\n", file, line, expr);
  ++failures();
}

}  // namespace sonotron::test

#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) sonotron::test::report(__FILE__, __LINE__, #cond); \
  } while (false)

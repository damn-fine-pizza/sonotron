#pragma once

// Minimal host-only test harness — no external dependency (D4).
// Usage: CHECK(expr); return arrangrr::test::failures() from main().

#include <cstdio>

namespace arrangrr::test {

inline int& failures() noexcept {
  static int count = 0;
  return count;
}

inline void report(const char* file, int line, const char* expr) noexcept {
  std::printf("FAIL %s:%d: %s\n", file, line, expr);
  ++failures();
}

}  // namespace arrangrr::test

#define CHECK(cond)                                    \
  do {                                                 \
    if (!(cond)) arrangrr::test::report(__FILE__, __LINE__, #cond); \
  } while (false)

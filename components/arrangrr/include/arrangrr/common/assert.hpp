#pragma once

// Freestanding-safe assertion. Traps on failure; no iostream, no abort(),
// no dependency on hosted <cassert>. Disabled in release firmware builds
// where ARRANGRR_NO_ASSERT is defined.

#if defined(ARRANGRR_NO_ASSERT)
#define ARR_ASSERT(cond) ((void)0)
#else
#define ARR_ASSERT(cond)      \
  do {                        \
    if (!(cond)) [[unlikely]] \
      __builtin_trap();       \
  } while (false)
#endif

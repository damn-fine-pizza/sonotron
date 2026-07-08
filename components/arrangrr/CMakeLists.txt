add_library(arrangrr STATIC
  src/version.cpp
  src/common_checks.cpp
  src/engine.cpp
  src/engine_checks.cpp
)

target_include_directories(arrangrr PUBLIC include)

# The whole core (and everything linking it) runs without exceptions/RTTI —
# same subset on host and firmware (D3, D32).
target_compile_options(arrangrr PUBLIC
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)

# PUBLIC, not PRIVATE: the core is ~100% headers — its real code compiles
# inside dependent TUs, and warnings that stop at the two ceremony files
# never see it (review finding).
target_compile_options(arrangrr PUBLIC
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
  -Werror
)

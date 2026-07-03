add_library(arrangrr_core STATIC
  src/version.cpp
  src/common_checks.cpp
)

target_include_directories(arrangrr_core PUBLIC include)

# The whole core (and everything linking it) runs without exceptions/RTTI —
# same subset on host and firmware (D3, D32).
target_compile_options(arrangrr_core PUBLIC
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)

target_compile_options(arrangrr_core PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
  -Werror
)

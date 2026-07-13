# Vendored single-header library (see ARRGRR_VENDOR.md): compiled into a
# small static library so consumers just link `miniaudio` and
# `#include <miniaudio.h>`, instead of every consumer defining
# MINIAUDIO_IMPLEMENTATION itself (which would violate the ODR the moment
# two translation units did it). Host-only; never entered from the
# firmware branch.
add_library(miniaudio STATIC miniaudio_impl.c)
target_include_directories(miniaudio PUBLIC .)
find_package(Threads REQUIRED)
target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)

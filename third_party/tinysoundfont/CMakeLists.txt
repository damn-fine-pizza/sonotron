# Vendored single-header library (see ARRGRR_VENDOR.md): compiled into a
# small static library so consumers just link `tinysoundfont` and
# `#include <tsf.h>`, instead of every consumer defining TSF_IMPLEMENTATION
# itself (which would violate the ODR the moment two translation units did
# it). Host-only; never entered from the firmware branch.
add_library(tinysoundfont STATIC tsf_impl.c)
target_include_directories(tinysoundfont PUBLIC .)

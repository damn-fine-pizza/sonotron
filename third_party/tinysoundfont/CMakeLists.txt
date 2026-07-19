# Vendored single-header library (see ARRGRR_VENDOR.md): compiled into a
# small static library so consumers just link `tinysoundfont` and
# `#include <tsf.h>`, instead of every consumer defining TSF_IMPLEMENTATION
# itself (which would violate the ODR the moment two translation units did
# it). Host-only; never entered from the firmware branch.
add_library(tinysoundfont STATIC tsf_impl.c)
target_include_directories(tinysoundfont PUBLIC .)

# tsf_impl.c pulls in stb_vorbis's declarations (header-only) before
# including tsf.h, to unlock TSF's .sf3 decode path -- the actual compiled
# symbols live in the separately-built `stb_vorbis` static library (see
# third_party/stb_vorbis/ARRGRR_VENDOR.md). PRIVATE: only this target's own
# tsf_impl.c calls into stb_vorbis directly, tsf.h's public surface (what
# melodd/synth.cpp sees) never names an stb_vorbis type.
target_link_libraries(tinysoundfont PRIVATE stb_vorbis)

# Vendored single-file library (see ARRGRR_VENDOR.md): compiled into a small
# static library so consumers link `stb_vorbis` and either `#include
# <stb_vorbis.c>` (declarations only, define STB_VORBIS_HEADER_ONLY first)
# or rely on the symbols this library's own TU (stb_vorbis_impl.c) already
# compiled -- same two-file split idea as third_party/tinysoundfont's
# tsf.h/tsf_impl.c, adapted to stb_vorbis.c's own single-file header/impl
# macro convention (see ARRGRR_VENDOR.md).
add_library(stb_vorbis STATIC stb_vorbis_impl.c)
target_include_directories(stb_vorbis PUBLIC .)

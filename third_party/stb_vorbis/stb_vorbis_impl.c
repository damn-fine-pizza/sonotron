// stb_vorbis is a single-file library that plays both header and
// implementation roles via macros (see ARRGRR_VENDOR.md). This translation
// unit is the ONE place that pulls in the FULL implementation (no
// STB_VORBIS_HEADER_ONLY defined) -- any other TU that needs only the
// declarations must define STB_VORBIS_HEADER_ONLY before its own
// #include <stb_vorbis.c>, then #undef it again, to avoid a duplicate-
// symbol link error against this TU.
#include "stb_vorbis.c"

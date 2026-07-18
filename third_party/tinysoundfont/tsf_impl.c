// TinySoundFont is a single-header library: this translation unit is the
// ONE place that defines TSF_IMPLEMENTATION and pulls in the implementation
// body, per tsf.h's own documented usage. See ARRGRR_VENDOR.md.

// .sf3 (Ogg-Vorbis-compressed SoundFont) support: TSF's own decode path
// (tsf_decode_ogg/tsf_decode_sf3_samples, see tsf.h ~lines 867-935) is
// gated behind STB_VORBIS_INCLUDE_STB_VORBIS_H -- pull in stb_vorbis's
// declarations (header-only: the actual implementation is compiled
// separately into the `stb_vorbis` static library this target now links,
// see third_party/stb_vorbis/ARRGRR_VENDOR.md) BEFORE including tsf.h, so
// that gate activates.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

#define TSF_IMPLEMENTATION
#include "tsf.h"

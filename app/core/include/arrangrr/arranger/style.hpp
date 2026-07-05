#pragma once

#include "arrangrr/arranger/style_model.hpp"

#include "arrangrr/arranger/styles/basic.hpp"
#include "arrangrr/arranger/styles/pop.hpp"
#include "arrangrr/arranger/styles/rock.hpp"
#include "arrangrr/arranger/styles/ballad.hpp"
#include "arrangrr/arranger/styles/funk.hpp"
#include "arrangrr/arranger/styles/disco.hpp"
#include "arrangrr/arranger/styles/house.hpp"
#include "arrangrr/arranger/styles/swing.hpp"
#include "arrangrr/arranger/styles/bossa.hpp"
#include "arrangrr/arranger/styles/samba.hpp"
#include "arrangrr/arranger/styles/reggae.hpp"
#include "arrangrr/arranger/styles/country.hpp"
#include "arrangrr/arranger/styles/blues.hpp"
#include "arrangrr/arranger/styles/shuffle.hpp"
#include "arrangrr/arranger/styles/latin.hpp"
#include "arrangrr/arranger/styles/motown.hpp"

namespace arrangrr {
namespace styles {

inline constexpr const Style* kBuiltins[] = {
    &basic::kStyle,  &pop::kStyle,    &rock::kStyle,    &ballad::kStyle,
    &funk::kStyle,   &disco::kStyle,  &house::kStyle,   &swing::kStyle,
    &bossa::kStyle,  &samba::kStyle,  &reggae::kStyle,  &country::kStyle,
    &blues::kStyle,  &shuffle::kStyle, &latin::kStyle,  &motown::kStyle,
};
inline constexpr std::uint8_t kBuiltinCount = 16;

}  // namespace styles
}  // namespace arrangrr

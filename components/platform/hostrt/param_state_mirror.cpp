#include "param_state_mirror.hpp"

#include "arrangrr/arp/arpeggiator.hpp"
#include "arrangrr/arranger/groove.hpp"

namespace arrangrr::host {

namespace {

// Mirrors abi.hpp's OutEvent::param_state() packing: v0|v1<<8 forms the
// 16-bit value. Every field the view structs display is a byte-range value
// (0..100 percentages, small indices) so this is a plain truncating read,
// the SAME display-only truncation abi.hpp's own kGroove/kArp table comment
// documents for GrooveField::kSeed (there v1 legitimately carries the high
// byte the view never shows at all).
std::uint8_t low_byte(std::uint8_t v0, std::uint8_t /*v1*/) { return v0; }

}  // namespace

void ParamStateMirror::apply(const ParamStateWire& w) {
  switch (w.param) {
    case Param::kGroove:
      switch (static_cast<GrooveField>(w.sub)) {
        case GrooveField::kSwing:
          groove.swing = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kHumanizeTiming:
          groove.humanize_timing = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kHumanizeVelocity:
          groove.humanize_velocity = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kAccent:
          groove.accent = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kSwingGrid:
          groove.swing_grid = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kQuantize:
          groove.quantize = low_byte(w.v0, w.v1);
          break;
        case GrooveField::kSeed:
          break;  // the view never shows the seed
        default:
          break;
      }
      break;
    case Param::kArp:
      switch (static_cast<ArpField>(w.sub)) {
        case ArpField::kEnabled:
          arp_enabled = w.v0 != 0;
          break;
        case ArpField::kRate:
          arp.rate = low_byte(w.v0, w.v1);
          break;
        case ArpField::kDirection:
          arp.direction = low_byte(w.v0, w.v1);
          break;
        case ArpField::kOctaves:
          arp.octaves = low_byte(w.v0, w.v1);
          break;
        case ArpField::kGate:
          arp.gate = low_byte(w.v0, w.v1);
          break;
        case ArpField::kLatch:
          arp.latch = w.v0 != 0;
          break;
        case ArpField::kSeed:
          break;  // the view never shows the seed
        default:
          break;
      }
      break;
    case Param::kPartMute:
      if (w.sub < kPartRoleCount) {
        part_muted[w.sub] = w.v0 != 0;
      }
      break;
    case Param::kPartSolo:
      if (w.sub < kPartRoleCount) {
        part_soloed[w.sub] = w.v0 != 0;
      }
      break;
    case Param::kStyleLoad:
      style_index = static_cast<int>(w.v0) | (static_cast<int>(w.v1) << 8);
      break;
    case Param::kChordDetect:
      chord_detect = w.v0 != 0;
      chord_detect_port = w.sub;
      break;
    case Param::kChordFollow:
      chord_follow = w.v0;
      break;
    case Param::kChordMode:
      chord_mode = w.v0;
      break;
    case Param::kKeySet:
      key_root_pc = w.v0;
      key_mode = w.v1;
      break;
    default:
      break;  // additive-only ABI growth: an unknown Param is ignored, not fatal
  }
}

}  // namespace arrangrr::host

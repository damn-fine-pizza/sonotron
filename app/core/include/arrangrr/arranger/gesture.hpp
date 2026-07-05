#pragma once

#include "arrangrr/arranger/style_model.hpp"  // StyleEvent, StylePattern, ChordGesture
#include "arrangrr/chord/theory.hpp"          // ChordState
#include "arrangrr/common/time.hpp"           // TickOffset

// Gesture-expansion stage of the arranger's resolution pipeline (D40): turns a
// single StyleEvent into ONE or MORE timed note-specs according to its
// ChordGesture. This is the generative layer above the NTT note vocabulary —
// strum / roll / arpeggiate the resolved chord — mirroring how the standalone
// ArpeggiatorEngine expands held notes, but driven from the style event and the
// live ChordState instead of the keyboard.
//
// The stage produces StyleEvent *specs* (not resolved pitches): the caller
// resolves each spec through the unchanged, wrong-note-proof kernel
// (Arranger::resolve), so this file never computes a MIDI note itself and stays
// a pure, deterministic, freestanding transform. Any output must be bounded
// (no heap) and reproducible (seed by position, like the arp's D16 hash).

namespace arrangrr::gesture {

// Maximum notes one gesture may expand a single StyleEvent into. Bounds the
// caller's stack buffer; keep gesture output within this.
inline constexpr int kMaxGestureFan = 8;

// Expands `ev` into 1..kMaxGestureFan note-specs for the given pattern/chord.
// out_specs[i] is a StyleEvent the caller resolves; out_delays[i] is an extra
// timing offset (ticks) added to that note's scheduled time (e.g. a strum
// stagger or a sub-grid roll). kNone (and, until the concrete gesture variants
// land in Phase 1, every value) yields the event unchanged with zero delay.
// Returns the produced count (>= 1).
inline int expand(const StylePattern& /*pattern*/, const StyleEvent& ev,
                  const ChordState& /*chord*/, StyleEvent (&out_specs)[kMaxGestureFan],
                  TickOffset (&out_delays)[kMaxGestureFan]) noexcept {
  // Scaffold stub: no expansion yet. The one-event -> one-note passthrough keeps
  // the arranger byte-for-byte identical until real gestures are implemented.
  out_specs[0] = ev;
  out_delays[0] = 0;
  return 1;
}

}  // namespace arrangrr::gesture

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

// Fixed micro-stagger (ticks) between successive tones of a strum. Small enough
// to read as one chord "brushed" rather than an arpeggio; independent of tempo
// and of the note's gate (that is what the rolls use instead).
inline constexpr TickOffset kStrumStaggerTicks = 20;

// The one-event -> one-note passthrough. kNone and every degenerate case (no
// live chord, empty shape, unknown gesture) route here so the arranger stays
// byte-for-byte identical to the pre-gesture baseline.
inline int passthrough(const StyleEvent& ev, StyleEvent (&out_specs)[kMaxGestureFan],
                       TickOffset (&out_delays)[kMaxGestureFan]) noexcept {
  out_specs[0] = ev;
  out_delays[0] = 0;
  return 1;
}

// Expands `ev` into 1..kMaxGestureFan note-specs for the given pattern/chord.
// out_specs[i] is a StyleEvent the caller resolves; out_delays[i] is an extra
// timing offset (ticks) added to that note's scheduled time (e.g. a strum
// stagger or a sub-grid roll). kNone yields the event unchanged with zero delay;
// the strum/roll gestures fan the live chord's tones out in time. Returns the
// produced count (>= 1).
inline int expand(const StylePattern& pattern, const StyleEvent& ev, const ChordState& chord,
                  StyleEvent (&out_specs)[kMaxGestureFan],
                  TickOffset (&out_delays)[kMaxGestureFan]) noexcept {
  // Golden-locked path: no gesture -> the event passes through untouched.
  if (ev.gesture == ChordGesture::kNone) {
    return passthrough(ev, out_specs, out_delays);
  }
  // A gesture fans out the live CHORD, so it only applies to a chord-tone event
  // of a chord-tone part. On a fixed drum event it would emit literal notes
  // 0..count-1 (resolve() ignores src for kFixed) — subsonic garbage on the
  // drum channel; on a scale-degree/interval event it would silently rewrite a
  // melodic line into a chord. Both pass straight through instead.
  if (pattern.policy != RolePolicy::kChordTone || ev.src != NoteSource::kChordTone) {
    return passthrough(ev, out_specs, out_delays);
  }

  // How many chord tones exist right now (triad = 3, seventh = 4). Without a
  // live chord there is nothing to spread: fall back to the single note.
  const int shape_count =
      chord.valid ? static_cast<int>(theory::shape_of(chord.quality).count) : 0;
  if (shape_count == 0) {
    return passthrough(ev, out_specs, out_delays);
  }
  // Never write past the caller's buffer (chords cap at 4 today, but bound it).
  const int count = shape_count < kMaxGestureFan ? shape_count : kMaxGestureFan;

  // Direction + timing spread per gesture. Ascending = low chord tone first.
  // A strum uses a fixed stagger; a roll spreads the tones across the gate.
  bool ascending = true;
  bool is_roll = false;
  switch (ev.gesture) {
    case ChordGesture::kStrumUp:
      ascending = true;
      is_roll = false;
      break;
    case ChordGesture::kStrumDown:
      ascending = false;
      is_roll = false;
      break;
    case ChordGesture::kRollUp:
      ascending = true;
      is_roll = true;
      break;
    case ChordGesture::kRollDown:
      ascending = false;
      is_roll = true;
      break;
    case ChordGesture::kNone:
    default:
      return passthrough(ev, out_specs, out_delays);
  }

  // Roll unfurls the chord across its own duration; strum uses the fixed micro
  // stagger. Integer division keeps the whole gesture inside [0, gate).
  const TickOffset per_tone_delay =
      is_roll ? static_cast<TickOffset>(ev.gate / static_cast<std::uint16_t>(count))
              : kStrumStaggerTicks;

  for (int i = 0; i < count; ++i) {
    StyleEvent spec = ev;
    // Emit chord-tone specs: resolve() maps each tone index to the right pitch
    // against the live chord, so this stage never computes a MIDI note itself.
    spec.src = NoteSource::kChordTone;
    spec.gesture = ChordGesture::kNone;  // produced specs are terminal notes
    spec.tone = static_cast<std::int8_t>(ascending ? i : (count - 1 - i));
    out_specs[i] = spec;
    out_delays[i] = static_cast<TickOffset>(i) * per_tone_delay;
  }
  return count;
}

}  // namespace arrangrr::gesture

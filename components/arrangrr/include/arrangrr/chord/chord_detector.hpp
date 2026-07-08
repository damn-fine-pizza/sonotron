#pragma once

#include <cstdint>

#include "arrangrr/chord/theory.hpp"

// Live chord detection (piano -> chord): the held-note half of the live
// harmonizer. As notes are pressed and released on an input keyboard, this
// maintains the set of currently-sounding notes and, when at least a triad is
// held, recognizes the chord exactly like shell-mode entry (D12 mode C): the
// lowest held note is the root, the pitch classes above it complete the
// quality via theory::complete_shell_full. The recognized ChordState is what
// STEERS the arranger's NTT resolution (D24) — the played notes themselves
// already sound through routing, so this only re-harmonizes the band.
//
// Chord memory (pro-arranger behaviour): recognition succeeds only while a
// full chord is held; dropping below the minimum leaves the LAST chord in
// place, so the band keeps playing on it until a new chord is pressed. That
// hold-last policy is the caller's — this class merely reports "no chord yet"
// (returns false) when too few notes are down.
//
// Core-portable and freestanding: no heap, bounded state (a 128-bit held-note
// set, 16 bytes), no host/UI dependency.

namespace arrangrr {

// A triad is the smallest gesture that unambiguously names a chord; two notes
// are an interval, not a harmony. This is the FINGERED default: below it the
// detector reports no chord (hold-last memory keeps the band on the old chord).
// Single-finger mode (Yamaha-style) lowers the threshold to 1 via
// set_min_notes(1): then a lone key is a major chord on that root, a second key
// colours it (minor/7th/etc. via the same shell interpretation), a third and up
// resolve the full chord — the interpretation is unchanged, only the minimum
// number of held notes differs.
inline constexpr std::uint8_t kMinChordNotes = 3;

class ChordDetector {
 public:
  // Presses/releases a MIDI note (0..127). Out-of-range notes are ignored.
  // A note pressed twice stays pressed once (idempotent set semantics), so a
  // stray duplicate NoteOn never corrupts the count.
  constexpr void note_on(std::uint8_t note) noexcept {
    if (note > 127 || is_held(note)) {
      return;
    }
    m_held[note >> 5] |= bit(note);
    ++m_count;
  }
  constexpr void note_off(std::uint8_t note) noexcept {
    if (note > 127 || !is_held(note)) {
      return;
    }
    m_held[note >> 5] &= static_cast<std::uint32_t>(~bit(note));
    --m_count;
  }
  // Releases every held note (panic / detection toggle): the held set resets,
  // but any chord the caller already latched is untouched — memory persists.
  constexpr void clear() noexcept {
    m_held[0] = m_held[1] = m_held[2] = m_held[3] = 0;
    m_count = 0;
  }

  constexpr std::uint8_t held_count() const noexcept { return m_count; }

  // Minimum simultaneously-held notes before recognize() names a chord.
  // kMinChordNotes (3) = fingered; 1 = single-finger. A zero is clamped to 1 so
  // recognition can never trigger on an empty keyboard.
  constexpr void set_min_notes(std::uint8_t n) noexcept { m_min_notes = n == 0 ? 1 : n; }
  constexpr std::uint8_t min_notes() const noexcept { return m_min_notes; }

  // Scale-aware single-finger (Dxx, refines D45): the current key and the
  // single-finger flag together let the ONE-key case resolve to the diatonic
  // maj/min triad of the pressed root (theory::single_finger_quality) instead
  // of the key-agnostic "always major" shell result. They only affect the
  // n==0 (bare-root) resolution; two or more pitch classes still shell-complete
  // exactly as before, so nothing else about recognition changes.
  constexpr void set_key(const Key& key) noexcept { m_key = key; }
  constexpr void set_single_finger(bool on) noexcept { m_single_finger = on; }

  // Recognizes the chord from the currently-held notes. Returns true and fills
  // `out` (valid = true) when at least kMinChordNotes are down; returns false
  // otherwise so the caller keeps the previous chord (chord memory). The root
  // is the lowest held note's pitch class; up to three distinct pitch classes
  // above it feed theory::complete_shell_full, mirroring shell-mode entry.
  constexpr bool recognize(ChordState& out) const noexcept {
    if (m_count < m_min_notes) {
      return false;
    }
    const int root = lowest_held();
    if (root < 0) {
      return false;
    }
    const auto root_pc = static_cast<std::uint8_t>(root % 12);
    std::uint8_t iv[3] = {0, 0, 0};
    std::uint8_t n = 0;
    for (int note = root + 1; note <= 127 && n < 3; ++note) {
      if (!is_held(static_cast<std::uint8_t>(note))) {
        continue;
      }
      const auto rel = static_cast<std::uint8_t>((note - root) % 12);
      if (rel != 0 && !has_interval(iv, n, rel)) {
        iv[n++] = rel;
      }
    }
    // Single-finger, one pitch class held (n == 0 = only the root and its
    // octaves — octaves alone leave n == 0): resolve the diatonic maj/min triad
    // of that root in the current key. Any OTHER held interval is a colour note
    // (n >= 1 — a held fifth included) and falls through to shell completion;
    // fingered mode never reaches here with n == 0 below its triad minimum.
    const ChordQuality quality = (m_single_finger && n == 0)
                                     ? theory::single_finger_quality(m_key, root_pc)
                                     : theory::complete_shell_full(iv, n);
    out = ChordState{.root_pc = root_pc, .quality = quality, .valid = true};
    return true;
  }

 private:
  static constexpr std::uint32_t bit(std::uint8_t note) noexcept {
    return std::uint32_t{1} << (note & 31);
  }
  constexpr bool is_held(std::uint8_t note) const noexcept {
    return (m_held[note >> 5] & bit(note)) != 0;
  }
  constexpr int lowest_held() const noexcept {
    for (int note = 0; note <= 127; ++note) {
      if (is_held(static_cast<std::uint8_t>(note))) {
        return note;
      }
    }
    return -1;
  }
  static constexpr bool has_interval(const std::uint8_t (&iv)[3], std::uint8_t n,
                                     std::uint8_t rel) noexcept {
    for (std::uint8_t i = 0; i < n; ++i) {
      if (iv[i] == rel) {
        return true;
      }
    }
    return false;
  }

  std::uint32_t m_held[4] = {0, 0, 0, 0};  // 128-bit held-note set
  std::uint8_t m_count = 0;
  std::uint8_t m_min_notes = kMinChordNotes;  // fingered (3) by default; 1 = single-finger
  Key m_key{};                    // current key, for scale-aware single-finger
  bool m_single_finger = false;   // scale-aware one-key maj/min resolution (Dxx)
};

}  // namespace arrangrr

#pragma once

#include <cstdint>

#include "arrangrr/arranger/gesture.hpp"  // gesture::expand (per-event note fan-out)
#include "arrangrr/arranger/groove.hpp"
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/arranger/voicing.hpp"  // NoteReq, VoicingState (voice-leading)
#include "arrangrr/chord/theory.hpp"      // ChordState, ChordShape, theory::shape_of
#include "arrangrr/common/assert.hpp"     // ARR_ASSERT (voice-group cap net)
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar

// The arranger (second WOW): plays the loaded style's current section on
// transport ticks, resolving chord-tone patterns against the live chord
// state — the NTT core (D24): every resolved note is a chord tone by
// construction, so wrong notes are impossible. Section switching is
// quantized to the bar (§11): variations loop, fills are one-shot and
// return, intros lead into a variation, endings stop the transport.

namespace arrangrr {

class Arranger {
 public:
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay, const MidiMessage& msg)>;

  struct TickResult {
    bool section_changed = false;
    bool style_changed = false;
    SectionType section = SectionType::kVarA;
    bool stop_transport = false;
  };

  bool load(std::uint8_t builtin_index) noexcept {
    if (builtin_index >= styles::kBuiltinCount) {
      return false;
    }
    return load_style(styles::kBuiltins[builtin_index]);
  }
  constexpr bool loaded() const noexcept { return m_style != nullptr; }
  constexpr SectionType current() const noexcept { return m_current; }
  constexpr const Style* current_style() const noexcept { return m_style; }

  // Loads a style by pointer (compiled user styles, tests). The pointee must
  // outlive the arranger — builtin styles are constexpr, compiled ones live
  // in flash-mapped storage (D33).
  bool load_style(const Style* style) noexcept {
    if (style == nullptr) {
      return false;
    }
    m_style = style;
    m_current = SectionType::kVarA;
    m_return_to = SectionType::kVarA;
    m_pending_valid = false;
    m_section_start = 0;
    m_voicing.reset();  // a new style must not voice-lead from the old one
    return true;
  }

  bool set_route(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount || port >= kMaxPorts || channel > 15) {
      return false;
    }
    m_routes[idx] = Route{.port = port, .channel = channel, .enabled = true};
    return true;
  }

  // Per-part (per-role) live mute/solo, mirroring the step-track mixer: a muted
  // role is silent; when ANY role is soloed, only soloed roles play. This is the
  // arranger-band mixer behind the host `parts` panel.
  void set_mute(TrackRole role, bool on) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx < kRoleCount) {
      set_bit(m_muted, idx, on);
    }
  }
  void set_solo(TrackRole role, bool on) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx < kRoleCount) {
      set_bit(m_solo, idx, on);
    }
  }
  constexpr bool muted(TrackRole role) const noexcept {
    return bit(m_muted, static_cast<std::uint8_t>(role));
  }
  constexpr bool soloed(TrackRole role) const noexcept {
    return bit(m_solo, static_cast<std::uint8_t>(role));
  }
  constexpr bool any_solo() const noexcept { return m_solo != 0; }

  // Global groove feel (the `groove` panel). set_groove_field clamps per field.
  void set_groove_field(GrooveField field, std::int32_t value) noexcept {
    groove::set_field(m_groove, field, value);
  }
  constexpr const GrooveParams& groove_params() const noexcept { return m_groove; }

  // A snapshot of one part for the host mixer: its route, its voice in the
  // current section, and its mute/solo state. `present` is false when the
  // current section has no pattern for this role (e.g. arp only in varC/varD).
  struct PartInfo {
    bool routed = false;
    std::uint8_t port = 0;
    std::uint8_t channel = 0;  // 0-based
    std::int16_t gm_program = -1;
    bool muted = false;
    bool soloed = false;
    bool present = false;
  };
  PartInfo part_info(TrackRole role) const noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    PartInfo info;
    if (idx >= kRoleCount) {
      return info;
    }
    const Route& r = m_routes[idx];
    info.routed = r.enabled;
    info.port = r.port;
    info.channel = r.channel;
    info.muted = bit(m_muted, idx);
    info.soloed = bit(m_solo, idx);
    if (m_style != nullptr) {
      const StyleSection* section = m_style->find(m_current);
      if (section != nullptr) {
        for (const StylePattern& pattern : section->patterns) {
          if (pattern.role == role) {
            info.present = true;
            info.gm_program = pattern.gm_program;
            break;
          }
        }
      }
    }
    return info;
  }

  // Requests a section switch, applied at the next bar boundary (or at once
  // when the transport is not running — `immediate`).
  bool request(SectionType t, bool immediate) noexcept {
    if (m_style == nullptr || m_style->find(t) == nullptr) {
      return false;
    }
    if (immediate) {
      m_current = t;
      if (section_is_variation(t)) {
        m_return_to = t;
      }
      m_pending_valid = false;
    } else {
      m_pending = t;
      m_pending_valid = true;
    }
    return true;
  }

  // Requests a combined style + section switch. `immediate` applies it at once
  // (a hard mid-bar cut); otherwise both land TOGETHER at the next bar boundary
  // so live style changes stay seamless. The target section must exist in the
  // new style, else it falls back to varA (every style is expected to define
  // one). Returns false if the style is null or lacks even the fallback.
  bool request_style(const Style* style, SectionType section, bool immediate) noexcept {
    if (style == nullptr) {
      return false;
    }
    const SectionType target = style->find(section) != nullptr ? section : SectionType::kVarA;
    if (style->find(target) == nullptr) {
      return false;
    }

    if (immediate) {
      m_style = style;
      m_current = target;
      if (section_is_variation(target)) {
        m_return_to = target;
      }
      m_pending_valid = false;
      m_pending_style = nullptr;
      return true;
    }

    m_pending_style = style;
    m_pending = target;
    m_pending_valid = true;
    return true;
  }

  void on_transport_start() noexcept {
    m_section_start = 0;
    m_voicing.reset();  // start each run with a clean voice-leading history
    if (section_is_variation(m_current)) {
      m_return_to = m_current;
    }
  }

  // Emits a Program Change for each routed role of the current section that
  // declares a default GM voice (StylePattern.gm_program >= 0), so loading a
  // style also picks its instruments. Unrouted roles are skipped (no known
  // destination). Call once after a style loads.
  void emit_voices(ScheduleFn schedule) const {
    if (m_style == nullptr) {
      return;
    }
    const StyleSection* section = m_style->find(m_current);
    if (section == nullptr) {
      return;
    }
    for (const StylePattern& pattern : section->patterns) {
      const Route& route = m_routes[static_cast<std::uint8_t>(pattern.role)];
      if (route.enabled && pattern.gm_program >= 0 && pattern.gm_program <= 127) {
        schedule(route.port, 0,
                 MidiMessage::program(route.channel,
                                      static_cast<std::uint8_t>(pattern.gm_program)));
      }
    }
  }

  // One transport tick. Resolution order matters upstream: feed the chord
  // AFTER the chord sequencer has fired this tick, so bar downbeats resolve
  // against the fresh chord.
  //
  // Deliberately over the cognitive-complexity threshold: this is the realtime
  // core heartbeat (bar-boundary detection, pending style/section switches,
  // one-shot transitions, grid firing). Splitting it would scatter the tight
  // timing logic across functions for no readability gain and real risk.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  TickResult on_tick(Tick transport_tick, const Key& key, const ChordState& chord,
                     ScheduleFn schedule) {
    TickResult result;
    if (m_style == nullptr) {
      return result;
    }
    const StyleSection* section = m_style->find(m_current);
    if (section == nullptr) {
      return result;
    }

    // Bar boundary: apply pending switches / one-shot transitions.
    if (transport_tick > 0 || m_section_start == transport_tick) {
      const Tick pos = transport_tick - m_section_start;
      const Tick len = static_cast<Tick>(section->bars) * kTicksPerBar;
      const bool bar_boundary = pos != 0 && pos % kTicksPerBar == 0;
      const bool section_end = pos == len;
      if (bar_boundary || section_end) {
        SectionType next = m_current;
        bool style_switched = false;
        if (m_pending_valid) {
          // A pending style change lands together with its section, so a live
          // style switch is seamless (both on the same downbeat).
          if (m_pending_style != nullptr && m_pending_style != m_style) {
            m_style = m_pending_style;
            style_switched = true;
          }
          m_pending_style = nullptr;
          next = m_pending;
          m_pending_valid = false;
        } else if (section_end) {
          if (section_is_fill(m_current) || section_is_intro(m_current)) {
            next = m_return_to;  // one-shots resolve to the active variation
          } else if (section_is_ending(m_current)) {
            result.stop_transport = true;
            return result;
          }
        }
        // The section clock restarts when the section wraps, the section
        // changes, OR the style changes (even to the same section type) — a
        // mid-section bar boundary must not reset `pos`, or bars 2..N of a
        // multi-bar section would never play.
        if (section_end || next != m_current || style_switched) {
          m_section_start = transport_tick;
        }
        if (next != m_current || style_switched) {
          m_current = next;
          if (section_is_variation(next)) {
            m_return_to = next;
          }
          result.section_changed = true;
          result.style_changed = style_switched;
          result.section = next;
          // Re-resolve against the (possibly new) style — required even when
          // the section TYPE is unchanged but the style switched.
          section = m_style->find(m_current);
          if (section == nullptr) {
            return result;
          }
        }
      }
    }

    // Fire the grid slots of this tick.
    const Tick rel = transport_tick - m_section_start;
    if (rel % kTicksPerStep != 0) {
      return result;
    }
    const std::uint16_t step = static_cast<std::uint16_t>(rel / kTicksPerStep);
    const bool solo_active = any_solo();
    for (const StylePattern& pattern : section->patterns) {
      const Route& route = m_routes[static_cast<std::uint8_t>(pattern.role)];
      if (!route.enabled || part_silenced(pattern.role, solo_active)) {
        continue;
      }
      // Per-role, per-step resolution pipeline (D40):
      //   gather this step's events -> gesture::expand (1 event -> N specs)
      //   -> resolve() each (unchanged NTT kernel) into a bounded voice group
      //   -> m_voicing.voice() (voice-leading) -> groove + schedule per note.
      // With the default kNone gestures and kAsWritten voicing this emits, in
      // the same order and with the same timing, exactly what the former
      // one-note-per-event loop did — byte-for-byte up to kMaxVoiceNotes notes
      // per role per step; a step denser than that truncates (bounded, D32; no
      // real style reaches it — the ARR_ASSERT below turns the drop into a
      // debug signal rather than silence).
      NoteReq group[kMaxVoiceNotes];
      int count = 0;
      for (const StyleEvent& ev : pattern.events) {
        if (ev.step != step) {
          continue;
        }
        StyleEvent specs[gesture::kMaxGestureFan];
        TickOffset delays[gesture::kMaxGestureFan];
        const int produced = gesture::expand(pattern, ev, chord, specs, delays);
        for (int i = 0; i < produced; ++i) {
          const int note = resolve(pattern, specs[i], key, chord);
          if (note < 0) {
            continue;
          }
          if (count >= kMaxVoiceNotes) {
            ARR_ASSERT(count < kMaxVoiceNotes);  // a step exceeded the cap
            break;
          }
          const bool is_chord_tone =
              pattern.policy == RolePolicy::kChordTone && specs[i].src == NoteSource::kChordTone;
          group[count++] = NoteReq{.note = note,
                                   .vel = specs[i].vel,
                                   .gate = specs[i].gate,
                                   .gesture_delay = delays[i],
                                   .chord_tone = is_chord_tone};
        }
      }
      if (count == 0) {
        continue;
      }
      // Voice-leading over the role's chord-tone notes (identity for kAsWritten).
      m_voicing.voice(pattern.role, pattern.voicing, group, count);
      for (int i = 0; i < count; ++i) {
        const NoteReq& nr = group[i];
        // Groove: swing/accent/humanize reshape timing and velocity
        // (deterministic; drums swing too, downbeats stay put). The note-off
        // shifts with the note-on so the gate length is preserved; a gesture
        // adds its own extra delay on top of the groove offset.
        const GrooveOut g = groove::apply(m_groove, static_cast<std::uint8_t>(pattern.role), step,
                                          transport_tick, nr.vel);
        const TickOffset on = g.timing_offset + nr.gesture_delay;
        schedule(route.port, on,
                 MidiMessage::note_on(route.channel, static_cast<std::uint8_t>(nr.note), g.velocity));
        schedule(route.port, static_cast<TickOffset>(nr.gate) + on,
                 MidiMessage::note_off(route.channel, static_cast<std::uint8_t>(nr.note)));
      }
    }
    return result;
  }

 private:
  static constexpr std::uint8_t kRoleCount = 10;
  // Upper bound on the notes one role emits on a single step: authored
  // chord-tone events plus any gesture fan-out. Generous vs real styles (a
  // dense chord part is ~4 tones), so the group buffer never overflows; extra
  // notes past this are dropped (bounded, no heap — D32).
  static constexpr int kMaxVoiceNotes = 16;

  struct Route {
    std::uint8_t port = 0;
    std::uint8_t channel = 0;
    bool enabled = false;
  };

  // A part plays unless it is muted, or a solo is active and it is not soloed.
  bool part_silenced(TrackRole role, bool solo_active) const noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    return bit(m_muted, idx) || (solo_active && !bit(m_solo, idx));
  }
  static constexpr bool bit(std::uint16_t mask, std::uint8_t i) noexcept {
    return (mask & static_cast<std::uint16_t>(1u << i)) != 0;
  }
  static void set_bit(std::uint16_t& mask, std::uint8_t i, bool on) noexcept {
    if (on) {
      mask = static_cast<std::uint16_t>(mask | (1u << i));
    } else {
      mask = static_cast<std::uint16_t>(mask & ~(1u << i));
    }
  }

  // NTT core (D24): chord-tone index -> concrete note. Bass anchors low
  // (octave 2), everything else around octave 4; indices past the shape wrap
  // an octave up, so tone 3 over a triad is the root one octave higher.
  //
  // The per-event NoteSource selects how `tone` is read (kChordTone keeps this
  // exact historical computation). kFixed roles short-circuit to the literal
  // note REGARDLESS of src — drums never transpose.
  static int resolve(const StylePattern& pattern, const StyleEvent& ev, const Key& key,
                     const ChordState& chord) noexcept {
    if (pattern.policy == RolePolicy::kFixed) {
      return ev.tone;
    }
    const int anchor = kRoleAnchor[static_cast<std::uint8_t>(pattern.role)];
    switch (ev.src) {
      case NoteSource::kInterval: {
        // Signed semitone offset from the chord root; no shape/quality lookup.
        if (!chord.valid) {
          return -1;  // silent until a chord exists
        }
        const int note = anchor + chord.root_pc + ev.tone + 12 * ev.octave;
        return (note < 0 || note > 127) ? -1 : note;
      }
      case NoteSource::kScaleDegree: {
        // Key-diatonic: independent of the chord (the key always exists).
        const int note =
            anchor + key.root_pc + theory::degree_to_semitones(key.mode, ev.tone) + 12 * ev.octave;
        return (note < 0 || note > 127) ? -1 : note;
      }
      case NoteSource::kChordTone:
      default: {
        if (!chord.valid || ev.tone < 0) {
          return -1;  // silent until a chord exists
        }
        const ChordShape shape = theory::shape_of(chord.quality);
        if (shape.count == 0) {
          return -1;
        }
        const std::uint8_t wrap = static_cast<std::uint8_t>(ev.tone / shape.count);
        const std::uint8_t offset = shape.offsets[ev.tone % shape.count];
        const int note = anchor + chord.root_pc + offset + 12 * (ev.octave + wrap);
        return (note < 0 || note > 127) ? -1 : note;
      }
    }
  }

  // Default register anchor per role (MIDI note of chord-tone 0 at octave 0),
  // so stacked tonal roles don't all pile into one octave = timbral mush. Bass
  // sits low; pad fills the gap under the mid comp; arp/lead/phrase sit above.
  // StyleEvent.octave still fine-tunes per pattern; kFixed roles ignore this.
  // kBass(36) and kChord1(60) keep their historical registers.
  static constexpr int kRoleAnchor[kRoleCount] = {
      60,  // kDrums  (fixed; unused)
      60,  // kPerc   (fixed; unused)
      36,  // kBass
      60,  // kChord1
      60,  // kChord2
      48,  // kPad
      72,  // kArp
      72,  // kPhrase
      72,  // kLead
      60,  // kCc     (unused)
  };

  const Style* m_style = nullptr;
  const Style* m_pending_style = nullptr;  // queued with m_pending for a seamless switch
  SectionType m_current = SectionType::kVarA;
  SectionType m_return_to = SectionType::kVarA;
  SectionType m_pending = SectionType::kVarA;
  bool m_pending_valid = false;
  Tick m_section_start = 0;
  Route m_routes[kRoleCount]{};
  std::uint16_t m_muted = 0;  // per-role mute bitmask (kRoleCount bits)
  std::uint16_t m_solo = 0;   // per-role solo bitmask
  GrooveParams m_groove;      // global groove feel applied to every part
  VoicingState m_voicing;     // per-role voice-leading memory (D40)
};

}  // namespace arrangrr

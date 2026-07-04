#pragma once

#include <cstdint>

#include "arrangrr/arranger/style.hpp"
#include "arrangrr/chord/chord_engine.hpp"
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
    if (section_is_variation(m_current)) {
      m_return_to = m_current;
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
  TickResult on_tick(Tick transport_tick, const ChordState& chord, ScheduleFn schedule) {
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
    for (const StylePattern& pattern : section->patterns) {
      const Route& route = m_routes[static_cast<std::uint8_t>(pattern.role)];
      if (!route.enabled) {
        continue;
      }
      for (const StyleEvent& ev : pattern.events) {
        if (ev.step != step) {
          continue;
        }
        const int note = resolve(pattern, ev, chord);
        if (note < 0) {
          continue;
        }
        schedule(route.port, 0,
                 MidiMessage::note_on(route.channel, static_cast<std::uint8_t>(note), ev.vel));
        schedule(route.port, static_cast<TickOffset>(ev.gate),
                 MidiMessage::note_off(route.channel, static_cast<std::uint8_t>(note)));
      }
    }
    return result;
  }

 private:
  static constexpr std::uint8_t kRoleCount = 10;

  struct Route {
    std::uint8_t port = 0;
    std::uint8_t channel = 0;
    bool enabled = false;
  };

  // NTT core (D24): chord-tone index -> concrete note. Bass anchors low
  // (octave 2), everything else around octave 4; indices past the shape wrap
  // an octave up, so tone 3 over a triad is the root one octave higher.
  static int resolve(const StylePattern& pattern, const StyleEvent& ev,
                     const ChordState& chord) noexcept {
    if (pattern.policy == RolePolicy::kFixed) {
      return ev.tone;
    }
    if (!chord.valid || ev.tone < 0) {
      return -1;  // silent until a chord exists
    }
    const ChordShape shape = theory::shape_of(chord.quality);
    if (shape.count == 0) {
      return -1;
    }
    const std::uint8_t wrap = static_cast<std::uint8_t>(ev.tone / shape.count);
    const std::uint8_t offset = shape.offsets[ev.tone % shape.count];
    const int anchor = (pattern.role == TrackRole::kBass ? 36 : 60) + chord.root_pc;
    const int note = anchor + offset + 12 * (ev.octave + wrap);
    return (note < 0 || note > 127) ? -1 : note;
  }

  const Style* m_style = nullptr;
  const Style* m_pending_style = nullptr;  // queued with m_pending for a seamless switch
  SectionType m_current = SectionType::kVarA;
  SectionType m_return_to = SectionType::kVarA;
  SectionType m_pending = SectionType::kVarA;
  bool m_pending_valid = false;
  Tick m_section_start = 0;
  Route m_routes[kRoleCount]{};
};

}  // namespace arrangrr

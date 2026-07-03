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
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay,
                                      const MidiMessage& msg)>;

  struct TickResult {
    bool section_changed = false;
    SectionType section = SectionType::kVarA;
    bool stop_transport = false;
  };

  bool load(std::uint8_t builtin_index) noexcept {
    if (builtin_index >= styles::kBuiltinCount) return false;
    style_ = styles::kBuiltins[builtin_index];
    current_ = SectionType::kVarA;
    return_to_ = SectionType::kVarA;
    pending_valid_ = false;
    section_start_ = 0;
    return true;
  }
  constexpr bool loaded() const noexcept { return style_ != nullptr; }
  constexpr SectionType current() const noexcept { return current_; }

  bool set_route(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount || port >= kMaxPorts || channel > 15) return false;
    routes_[idx] = Route{port, channel, true};
    return true;
  }

  // Requests a section switch, applied at the next bar boundary (or at once
  // when the transport is not running — `immediate`).
  bool request(SectionType t, bool immediate) noexcept {
    if (style_ == nullptr || style_->find(t) == nullptr) return false;
    if (immediate) {
      current_ = t;
      if (section_is_variation(t)) return_to_ = t;
      pending_valid_ = false;
    } else {
      pending_ = t;
      pending_valid_ = true;
    }
    return true;
  }

  void on_transport_start() noexcept {
    section_start_ = 0;
    if (section_is_variation(current_)) return_to_ = current_;
  }

  // One transport tick. Resolution order matters upstream: feed the chord
  // AFTER the chord sequencer has fired this tick, so bar downbeats resolve
  // against the fresh chord.
  TickResult on_tick(Tick transport_tick, const ChordState& chord, ScheduleFn schedule) {
    TickResult result;
    if (style_ == nullptr) return result;
    const StyleSection* section = style_->find(current_);
    if (section == nullptr) return result;

    // Bar boundary: apply pending switches / one-shot transitions.
    if (transport_tick > 0 || section_start_ == transport_tick) {
      const Tick pos = transport_tick - section_start_;
      const Tick len = static_cast<Tick>(section->bars) * kTicksPerBar;
      const bool bar_boundary = pos != 0 && pos % kTicksPerBar == 0;
      const bool section_end = pos == len;
      if (bar_boundary || section_end) {
        SectionType next = current_;
        if (pending_valid_) {
          next = pending_;
          pending_valid_ = false;
        } else if (section_end) {
          if (section_is_fill(current_) || section_is_intro(current_)) {
            next = return_to_;  // one-shots resolve to the active variation
          } else if (section_is_ending(current_)) {
            result.stop_transport = true;
            return result;
          }
        }
        if (bar_boundary || section_end) {
          section_start_ = transport_tick;
          if (next != current_) {
            current_ = next;
            if (section_is_variation(next)) return_to_ = next;
            result.section_changed = true;
            result.section = next;
            section = style_->find(current_);
            if (section == nullptr) return result;
          }
        }
      }
    }

    // Fire the grid slots of this tick.
    const Tick rel = transport_tick - section_start_;
    if (rel % kTicksPerStep != 0) return result;
    const std::uint16_t step = static_cast<std::uint16_t>(rel / kTicksPerStep);
    for (const StylePattern& pattern : section->patterns) {
      const Route& route = routes_[static_cast<std::uint8_t>(pattern.role)];
      if (!route.enabled) continue;
      for (const StyleEvent& ev : pattern.events) {
        if (ev.step != step) continue;
        const int note = resolve(pattern, ev, chord);
        if (note < 0) continue;
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
    if (pattern.policy == RolePolicy::kFixed) return ev.tone;
    if (!chord.valid || ev.tone < 0) return -1;  // silent until a chord exists
    const ChordShape shape = theory::shape_of(chord.quality);
    if (shape.count == 0) return -1;
    const std::uint8_t wrap = static_cast<std::uint8_t>(ev.tone / shape.count);
    const std::uint8_t offset = shape.offsets[ev.tone % shape.count];
    const int anchor = (pattern.role == TrackRole::kBass ? 36 : 60) + chord.root_pc;
    const int note = anchor + offset + 12 * (ev.octave + wrap);
    return (note < 0 || note > 127) ? -1 : note;
  }

  const Style* style_ = nullptr;
  SectionType current_ = SectionType::kVarA;
  SectionType return_to_ = SectionType::kVarA;
  SectionType pending_ = SectionType::kVarA;
  bool pending_valid_ = false;
  Tick section_start_ = 0;
  Route routes_[kRoleCount]{};
};

}  // namespace arrangrr

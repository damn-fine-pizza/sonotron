#include "preview_oracle.hpp"

#include <chrono>
#include <thread>
#include <vector>

#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/arranger/motif.hpp"
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/arranger/style_model.hpp"
#include "arrangrr/common/span.hpp"
#include "chorddet/theory.hpp"
#include "src/brain_event.hpp"
#include "src/default_style_progressions.hpp"
#include "src/in_process_brain_session.hpp"

// See preview_oracle.hpp's own header comment for the full design. This file
// deliberately names arrangrr:: types directly (test code, not gui_sonotron_
// layout/gui_sonotron_models -- the D38 core-free discipline those two
// libraries hold to does not apply here, same as preview.cpp's own only-TU-
// that-names-arrangrr carve-out).

namespace sonotron::preview_oracle {

namespace {

constexpr std::size_t kCoreRoleCount = 10;  // mirrors preview.cpp's own literal

// Same canonical placeholder harmony preview_for() builds (preview.cpp): a
// fixed C-major key + tonic triad. Reused verbatim here so a divergence
// real_bar_pitch() finds is never attributable to a DIFFERENT harmony
// assumption -- only to reaching bar 2+ at all.
const arrangrr::Key kPlaceholderKey{.root_pc = 0, .mode = arrangrr::Mode::kMajor};
const arrangrr::ChordState kPlaceholderChord{
    .root_pc = 0, .quality = arrangrr::ChordQuality::kMaj, .valid = true};

// Shared lookup mirroring preview_for()'s own (style_index, section,
// role_index) -> StylePattern resolution, additionally handing back the
// owning StyleSection (bars count, and the section's full pattern span --
// needed by a generated, not authored, motif's idiom onset mask).
const arrangrr::StylePattern* find_pattern(int style_index, arrangrr::SectionType section,
                                            std::size_t role_index,
                                            const arrangrr::StyleSection** out_section) {
  if (out_section != nullptr) {
    *out_section = nullptr;
  }
  if (style_index < 0 || style_index >= static_cast<int>(arrangrr::styles::kBuiltinCount) ||
      role_index >= kCoreRoleCount) {
    return nullptr;
  }
  const arrangrr::Style* style = arrangrr::styles::kBuiltins[static_cast<std::size_t>(style_index)];
  const arrangrr::StyleSection* sec = style->find(section);
  if (sec == nullptr) {
    return nullptr;
  }
  if (out_section != nullptr) {
    *out_section = sec;
  }
  const auto role = static_cast<arrangrr::TrackRole>(static_cast<std::uint8_t>(role_index));
  for (const arrangrr::StylePattern& p : sec->patterns) {
    if (p.role == role) {
      return &p;
    }
  }
  return nullptr;
}

}  // namespace

std::set<int> real_bar_onset_steps(int style_index, int section_type_value, std::size_t role_index,
                                    int bar_index) {
  std::set<int> out;
  const auto section = static_cast<arrangrr::SectionType>(section_type_value);
  const arrangrr::StyleSection* sec = nullptr;
  const arrangrr::StylePattern* pattern = find_pattern(style_index, section, role_index, &sec);
  if (pattern == nullptr || bar_index < 0 || bar_index >= static_cast<int>(sec->bars)) {
    return out;
  }
  const int lo = bar_index * 16;
  const int hi = lo + 16;
  for (const arrangrr::StyleEvent& ev : pattern->events) {
    if (static_cast<int>(ev.step) >= lo && static_cast<int>(ev.step) < hi) {
      out.insert(static_cast<int>(ev.step) - lo);
    }
  }
  return out;
}

int real_bar_pitch(int style_index, int section_type_value, std::size_t role_index, int bar_index,
                    int step_in_bar) {
  const auto section = static_cast<arrangrr::SectionType>(section_type_value);
  const arrangrr::StyleSection* sec = nullptr;
  const arrangrr::StylePattern* pattern = find_pattern(style_index, section, role_index, &sec);
  if (pattern == nullptr || bar_index < 0 || bar_index >= static_cast<int>(sec->bars)) {
    return -1;
  }
  const int target_step = bar_index * 16 + step_in_bar;
  for (const arrangrr::StyleEvent& ev : pattern->events) {
    if (static_cast<int>(ev.step) != target_step) {
      continue;
    }
    const int note = pattern->policy == arrangrr::RolePolicy::kFixed
                          ? ev.tone
                          : arrangrr::Arranger::resolve(*pattern, ev, kPlaceholderKey,
                                                        kPlaceholderChord, /*transpose=*/0);
    if (note >= 0 && note <= 127) {
      return note;
    }
  }
  return -1;
}

std::set<int> real_motif_repeat_onset_steps(int style_index, int section_type_value,
                                             std::size_t role_index, std::uint32_t repeat) {
  std::set<int> out;
  const auto section = static_cast<arrangrr::SectionType>(section_type_value);
  const arrangrr::StyleSection* sec = nullptr;
  const arrangrr::StylePattern* pattern = find_pattern(style_index, section, role_index, &sec);
  if (pattern == nullptr || pattern->motif == nullptr) {
    return out;
  }
  // Mirrors preview.cpp's own seed construction verbatim (from_span for an
  // authored seed, generate() only for a from-scratch generated one).
  const arrangrr::Motif seed =
      pattern->events.empty()
          ? arrangrr::motif::generate(
                pattern->motif->seed, pattern->motif->length,
                arrangrr::motif::idiom_onset_mask(sec->patterns, pattern->motif->idiom_role),
                pattern->motif->center_degree, pattern->motif->vel, pattern->motif->gate)
          : arrangrr::motif::from_span(pattern->events);
  const arrangrr::Motif generated = arrangrr::motif::apply_repeat(seed, *pattern->motif, repeat);
  for (std::uint8_t i = 0; i < generated.count; ++i) {
    out.insert(static_cast<int>(generated.events[i].step));
  }
  return out;
}

int real_first_progression_chord_pitch(int style_index, int section_type_value,
                                        std::size_t role_index, int step) {
  const auto section = static_cast<arrangrr::SectionType>(section_type_value);
  const arrangrr::StyleSection* sec = nullptr;
  const arrangrr::StylePattern* pattern = find_pattern(style_index, section, role_index, &sec);
  if (pattern == nullptr) {
    return -1;
  }
  if (style_index < 0 || static_cast<std::size_t>(style_index) >= kDefaultProgressions.size()) {
    return -1;
  }
  const DefaultProgression& prog = kDefaultProgressions[static_cast<std::size_t>(style_index)];
  if (prog.step_count == 0 || prog.steps[0].quality_ovr < 0) {
    return -1;  // smart-quality first step: not handled by this simple helper
  }
  const arrangrr::Key real_key{.root_pc = prog.key_root_pc, .mode = prog.key_mode};
  const arrangrr::ChordState real_chord{
      .root_pc = prog.steps[0].root_pc,
      .quality = static_cast<arrangrr::ChordQuality>(prog.steps[0].quality_ovr),
      .valid = true};
  for (const arrangrr::StyleEvent& ev : pattern->events) {
    if (static_cast<int>(ev.step) != step) {
      continue;
    }
    if (pattern->policy == arrangrr::RolePolicy::kFixed) {
      return ev.tone;  // literal, harmony-independent -- not useful for this bug, but honest
    }
    const int note = arrangrr::Arranger::resolve(*pattern, ev, real_key, real_chord, /*transpose=*/0);
    if (note >= 0 && note <= 127) {
      return note;
    }
  }
  return -1;
}

HarmonyObservation observe_default_harmony(const std::string& style_name, long wait_milliseconds) {
  InProcessBrainSession session;
  session.start();
  session.send("style load " + style_name);
  session.send("transport start");

  HarmonyObservation obs;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_milliseconds);
  std::vector<BrainEvent> batch;
  while (std::chrono::steady_clock::now() < deadline) {
    batch.clear();
    session.poll(batch);
    for (const BrainEvent& ev : batch) {
      if (ev.kind == BrainEvent::Kind::kChord) {
        ++obs.chord_event_count;
      } else if (ev.kind == BrainEvent::Kind::kChordFollowed && ev.followed_source == "sequencer") {
        if (obs.pitch_class_union == 0 && obs.first_pcs == 0) {
          obs.first_pcs = static_cast<std::uint16_t>(ev.followed_current_pcs);
        }
        obs.pitch_class_union |= static_cast<std::uint16_t>(ev.followed_current_pcs);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  session.stop();
  return obs;
}

}  // namespace sonotron::preview_oracle

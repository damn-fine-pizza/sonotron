#include "grid_model.hpp"

#include <algorithm>
#include <utility>

namespace sonotron {

GridModel::GridModel(std::size_t scene_count)
    : m_scene_count(scene_count == 0 ? 1 : scene_count), m_cells(kPartCount * m_scene_count) {
  for (std::size_t i = 0; i < kMaxSceneCount; ++i) {
    m_scene_names[i] = std::to_string(i + 1);
  }
  m_scene_sections.fill(kDefaultSectionType);
  m_scene_bars.fill(kDefaultSceneBars);
}

std::string_view GridModel::part_label(std::size_t part_index) const {
  return kTrackRoleLabels[part_index];
}

std::size_t GridModel::index_of(std::size_t part_index, std::size_t scene_index) const {
  return part_index * m_scene_count + scene_index;
}

const GridCell& GridModel::cell(std::size_t part_index, std::size_t scene_index) const {
  return m_cells[index_of(part_index, scene_index)];
}

void GridModel::set_cell(std::size_t part_index, std::size_t scene_index, GridCellKind kind,
                         std::string label, int loop_slot_id) {
  GridCell& target = m_cells[index_of(part_index, scene_index)];
  target.kind = kind;
  target.label = std::move(label);
  target.loop_slot_id = loop_slot_id;
}

void GridModel::clear_cell(std::size_t part_index, std::size_t scene_index) {
  set_cell(part_index, scene_index, GridCellKind::kEmpty, std::string());
}

void GridModel::add_scene() {
  if (m_scene_count >= kMaxSceneCount) {
    return;
  }
  const std::size_t new_scene_count = m_scene_count + 1;
  std::vector<GridCell> new_cells(kPartCount * new_scene_count);
  for (std::size_t part = 0; part < kPartCount; ++part) {
    for (std::size_t scene = 0; scene < m_scene_count; ++scene) {
      new_cells[part * new_scene_count + scene] = m_cells[part * m_scene_count + scene];
    }
    // The freshly added scene column starts empty (default GridCell{}).
  }
  m_cells = std::move(new_cells);
  m_scene_count = new_scene_count;
}

std::string_view GridModel::scene_name(std::size_t scene_index) const {
  if (scene_index >= kMaxSceneCount) {
    return std::string_view();
  }
  return m_scene_names[scene_index];
}

void GridModel::set_scene_name(std::size_t scene_index, std::string name) {
  if (scene_index >= kMaxSceneCount) {
    return;
  }
  m_scene_names[scene_index] = std::move(name);
}

std::uint8_t GridModel::scene_section(std::size_t scene_index) const {
  if (scene_index >= kMaxSceneCount) {
    return kDefaultSectionType;
  }
  return m_scene_sections[scene_index];
}

void GridModel::set_scene_section(std::size_t scene_index, std::uint8_t section) {
  if (scene_index >= kMaxSceneCount) {
    return;
  }
  m_scene_sections[scene_index] = section;
}

int GridModel::scene_bars(std::size_t scene_index) const {
  if (scene_index >= kMaxSceneCount) {
    return kDefaultSceneBars;
  }
  return m_scene_bars[scene_index];
}

void GridModel::set_scene_bars(std::size_t scene_index, int bars) {
  if (scene_index >= kMaxSceneCount) {
    return;
  }
  m_scene_bars[scene_index] = std::max(bars, 1);
}

std::string_view section_wire_name(std::uint8_t section) {
  static constexpr std::array<std::string_view, 13> kNames = {
      "intro1", "intro2", "varA",  "varB",  "varC",    "varD",    "fillA",
      "fillB",  "fillC",  "fillD", "break", "ending1", "ending2",
  };
  if (section >= kNames.size()) {
    return std::string_view();
  }
  return kNames[section];
}

std::optional<int> next_scene_to_launch(bool auto_song, bool playing, int active_scene,
                                        int scene_count, int bars_elapsed_in_scene,
                                        int active_scene_section_bars) {
  if (!auto_song || !playing || scene_count <= 0) {
    return std::nullopt;
  }
  if (bars_elapsed_in_scene < active_scene_section_bars) {
    return std::nullopt;
  }
  // Normalize defensively before wrapping: a caller-tracked active_scene
  // should already be in [0, scene_count), but a negative or stale value
  // must still land in range rather than index out of bounds downstream.
  const int normalized = ((active_scene % scene_count) + scene_count) % scene_count;
  return (normalized + 1) % scene_count;
}

bool bar_just_advanced(int current_bar, int& last_checked_bar) {
  if (current_bar == last_checked_bar) {
    return false;
  }
  last_checked_bar = current_bar;
  return true;
}

float section_playhead_phase(int current_bar, int active_scene_start_bar, int beat_num, int pulse,
                             int beats_per_bar, int section_bars) {
  constexpr float kNoPlayhead = -1.0F;
  constexpr float kPulsesPerBeat = 24.0F;
  if (current_bar <= 0 || section_bars <= 0 || beats_per_bar <= 0) {
    return kNoPlayhead;
  }
  const int bars_elapsed = current_bar - active_scene_start_bar;
  if (bars_elapsed < 0) {
    // A bar REWIND: active_scene_start_bar is a stale pre-stop/restart
    // anchor. Honestly report "no playhead" rather than a nonsensical
    // negative-going phase (see the header comment's rewind-guard note).
    return kNoPlayhead;
  }
  const int beat_in_bar = std::max(beat_num - 1, 0);
  const float within_bar =
      (static_cast<float>(beat_in_bar) + static_cast<float>(pulse) / kPulsesPerBeat) /
      static_cast<float>(beats_per_bar);
  const float phase =
      (static_cast<float>(bars_elapsed) + within_bar) / static_cast<float>(section_bars);
  return std::clamp(phase, 0.0F, 1.0F);
}

}  // namespace sonotron

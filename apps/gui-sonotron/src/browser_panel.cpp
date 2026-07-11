#include "browser_panel.hpp"

#include <cfloat>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

namespace sonotron {

namespace {

constexpr int kFilterBufferSize = 64;

void render_search_field(BrowserModel& model) {
  char buffer[kFilterBufferSize];
  std::strncpy(buffer, model.search_filter().c_str(), sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputTextWithHint("##browser_search", "search...", buffer, sizeof(buffer))) {
    model.set_search_filter(std::string(buffer));
  }
}

void render_styles_branch(BrowserModel& model, BrainSession& brain_session) {
  if (!ImGui::TreeNodeEx("Styles", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    if (!model.style_matches_filter(i)) {
      continue;
    }
    const std::string name(model.style_name(i));
    ImGui::Selectable(name.c_str());
    // A real load, not a placeholder: `style load <name>` is a shipped L1
    // verb (§7 A1) — components/hostrt/shell_music_commands.cpp::cmd_style
    // resolves builtins by NAME, case-insensitively.
    if (ImGui::IsItemClicked()) {
      brain_session.send("style load " + name);
    }
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload(kStyleDragPayloadId, &i, sizeof(i));
      ImGui::TextUnformatted(name.c_str());
      ImGui::EndDragDropSource();
    }
  }
  ImGui::TreePop();
}

void render_placeholder_branch(const char* title, const std::vector<std::string>& entries) {
  if (!ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (entries.empty()) {
    ImGui::TextDisabled("(none authored yet)");
  } else {
    for (const std::string& entry : entries) {
      ImGui::Selectable(entry.c_str());
    }
  }
  ImGui::TreePop();
}

}  // namespace

void render_browser_panel(BrowserModel& model, BrainSession& brain_session) {
  render_search_field(model);
  render_styles_branch(model, brain_session);
  render_placeholder_branch("Clips", model.clip_names());
  render_placeholder_branch("MIDI seqs", model.midi_seq_names());
}

}  // namespace sonotron

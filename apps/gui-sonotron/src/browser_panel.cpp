#include "browser_panel.hpp"

#include <array>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <string>
#include <string_view>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kFilterBufferSize = 64;

// The v02 non-style sections (spec §2a). These are design-intent lists with no
// load verb wired from the browser -> local-only (see docs/v02-feature-list).
constexpr std::array<std::string_view, 8> kVariations = {
    "intro", "verse A", "verse B", "chorus", "bridge", "break", "fill", "outro",
};
constexpr std::array<std::string_view, 10> kKits = {
    "acoustic kit", "808",     "909",      "jazz kit", "fingered bass",
    "picked bass",  "rhodes",  "dx piano", "warm pad", "saw lead",
};

bool matches(std::string_view item, const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  std::string hay(item);
  std::string needle = filter;
  for (char& ch : hay) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  for (char& ch : needle) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return hay.find(needle) != std::string::npos;
}

// A tree section header: "▾ TITLE" in a section color.
bool section_header(const char* title, const ImVec4& color) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const bool open = ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen |
                                                 ImGuiTreeNodeFlags_SpanAvailWidth);
  ImGui::PopStyleColor();
  return open;
}

// One leaf row "· item"; `active` gives it the cyan text + left cyan border +
// faint wash of the selected style. Returns true on click.
bool leaf_row(std::string_view item, bool active) {
  const std::string label = "\xC2\xB7 " + std::string(item);
  ImGui::PushStyleColor(ImGuiCol_Text, active ? theme::kCyan : theme::kTextSecondary);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.12F));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                        ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.22F));
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::Selectable(label.c_str(), active);
  ImGui::PopStyleColor(3);
  if (active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetItemRectSize().y;
    dl->AddRectFilled(ImVec2(p0.x - 2.0F, p0.y), ImVec2(p0.x, p0.y + h), neon::u32(theme::kCyan));
  }
  return clicked;
}

void render_styles(BrowserModel& model, BrainSession& brain_session, V02State& fx,
                   const std::string& filter, int& shown) {
  if (!section_header("styles", theme::kCyan)) {
    return;
  }
  int local_shown = 0;
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    const std::string name(model.style_name(i));
    if (!matches(name, filter)) {
      continue;
    }
    ++local_shown;
    ++shown;
    ImGui::PushID(static_cast<int>(i));
    if (leaf_row(name, fx.active_style == static_cast<int>(i))) {
      brain_session.send("style load " + name);
      fx.active_style = static_cast<int>(i);
    }
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload(kStyleDragPayloadId, &i, sizeof(i));
      ImGui::TextUnformatted(name.c_str());
      ImGui::EndDragDropSource();
    }
    ImGui::PopID();
  }
  if (local_shown == 0) {
    ImGui::TextDisabled("  (no match)");
  }
  ImGui::TreePop();
}

template <std::size_t N>
void render_list(const char* title, const ImVec4& color,
                 const std::array<std::string_view, N>& items, const std::string& filter,
                 int& shown) {
  if (!section_header(title, color)) {
    return;
  }
  int local_shown = 0;
  for (const std::string_view item : items) {
    if (!matches(item, filter)) {
      continue;
    }
    ++local_shown;
    ++shown;
    leaf_row(item, /*active=*/false);
  }
  if (local_shown == 0) {
    ImGui::TextDisabled("  (no match)");
  }
  ImGui::TreePop();
}

}  // namespace

void render_browser_panel(BrowserModel& model, BrainSession& brain_session, V02State& fx) {
  ImGui::TextColored(theme::kPink, "BROWSER");
  ImGui::Spacing();

  const float search_h = ImGui::GetFrameHeightWithSpacing() + 4.0F;
  ImGui::BeginChild("browser_tree", ImVec2(0.0F, ImGui::GetContentRegionAvail().y - search_h),
                    ImGuiChildFlags_None, ImGuiWindowFlags_None);
  const std::string filter = model.search_filter();
  int shown = 0;
  render_styles(model, brain_session, fx, filter, shown);
  render_list("variations", theme::kAmber, kVariations, filter, shown);
  render_list("kits \xC2\xB7 GM", theme::kGreen, kKits, filter, shown);
  if (section_header("clips", theme::kBlue)) {
    ImGui::TextDisabled("  (none authored yet)");
    ImGui::TreePop();
  }
  ImGui::EndChild();

  // Search field pinned at the bottom (filters all sections live).
  char buffer[kFilterBufferSize];
  std::strncpy(buffer, filter.c_str(), sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputTextWithHint("##browser_search", "search\xE2\x80\xA6", buffer, sizeof(buffer))) {
    model.set_search_filter(std::string(buffer));
  }
}

}  // namespace sonotron

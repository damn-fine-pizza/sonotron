#include "browser_panel.hpp"

#include <array>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kFilterBufferSize = 64;

// The v02 non-style sections (spec §2a). "variations" is now a REAL drag
// source (repeat-zone-real-contract.md SLICE 4a): each row carries a
// SectionType byte a scene header (grid_panel.cpp) accepts as a drop target
// to set that column's section. "kits" stays a design-intent, local-only list
// (see docs/v02-feature-list) -- no kit-load verb is wired from here.
constexpr std::array<std::string_view, 8> kVariations = {
    "intro", "verse A", "verse B", "chorus", "bridge", "break", "fill", "outro",
};
// SectionType byte each kVariations row drags onto a scene header --
// numerically mirrors arrangrr::SectionType (components/core/arrangrr/
// include/arrangrr/arranger/style_model.hpp), same hand-copied-literal
// discipline grid_model.hpp's own kDefaultSectionType/section_wire_name
// already use (D38: this file never includes arrangrr/). Index-parallel
// with kVariations above -- entry i's payload is kVariationSections[i].
constexpr std::array<std::uint8_t, 8> kVariationSections = {
    0,   // intro   -> kIntro1
    2,   // verse A -> kVarA
    3,   // verse B -> kVarB
    4,   // chorus  -> kVarC
    5,   // bridge  -> kVarD
    10,  // break   -> kBreak
    6,   // fill    -> kFillA
    11,  // outro   -> kEnding1
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

// A tree section header "▾ title" — uniform near-white across all sections, as
// in the design (no per-section tint; only the active style leaf goes cyan).
bool section_header(const char* title) {
  ImGui::PushStyleColor(ImGuiCol_Text, theme::kText);
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
  if (!section_header("styles")) {
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
      // While playing, morph live (quantized to the next bar) instead of
      // hard-resetting the arranger -- `style load` still stops-and-reloads
      // for the not-yet-playing case (in_process_brain_session.cpp's
      // command_line_to_command).
      brain_session.send(fx.playing ? "style switch " + name : "style load " + name);
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

// "variations" (kVariations/kVariationSections above): a real drag SOURCE
// (repeat-zone-real-contract.md SLICE 4a), each row carrying its own
// SectionType byte under kVariationDragPayloadId -- distinct from
// kStyleDragPayloadId so grid_panel.cpp's scene-header drop target never
// confuses the two payload shapes. Clicking a row (leaf_row's own return
// value) does nothing yet -- there is still no `style section` verb wired
// from a plain click here, only from the drag; the scene HEADER's own ▶
// button is what sends `style section <name>` (grid_panel.cpp).
void render_variations(const std::string& filter, int& shown) {
  if (!section_header("variations")) {
    return;
  }
  int local_shown = 0;
  for (std::size_t i = 0; i < kVariations.size(); ++i) {
    const std::string_view item = kVariations[i];
    if (!matches(item, filter)) {
      continue;
    }
    ++local_shown;
    ++shown;
    ImGui::PushID(static_cast<int>(i));
    leaf_row(item, /*active=*/false);
    if (ImGui::BeginDragDropSource()) {
      const std::uint8_t section = kVariationSections[i];
      ImGui::SetDragDropPayload(kVariationDragPayloadId, &section, sizeof(section));
      ImGui::TextUnformatted(std::string(item).c_str());
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
void render_list(const char* title, const std::array<std::string_view, N>& items,
                 const std::string& filter, int& shown) {
  if (!section_header(title)) {
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
  render_variations(filter, shown);
  render_list("kits \xC2\xB7 GM", kKits, filter, shown);
  if (section_header("clips")) {
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

#include "browser_panel.hpp"

#include <array>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kFilterBufferSize = 64;

// Below this many total styles, render one flat, ungrouped list (the
// pre-task-#30 behavior) instead of family headers + the family-filter
// combo -- owner correction 2026-07-17: 16 built-in styles buried under 8
// genre headers (two of them ALWAYS empty, kBallroomTraditional/
// kWorldRegional -- see browser_model.hpp's kBuiltinStyleFamilies, neither
// family has any of today's 16 members) was strictly worse than the old
// flat list for a corpus this small. 24 sits comfortably above today's 16
// (room to grow the built-in set a bit without flipping modes) and well
// below the point a flat list stops fitting a 210px rail. The family/
// genre taxonomy (task #30, docs/proposals/style-browser-corpus-scale.md)
// stays wired for the day the ~1010-style import lands (memory:
// browser-scale-many-styles) -- it is simply gated off until the corpus
// actually needs it.
constexpr std::size_t kFlatListThreshold = 24;

// Family section render order (task #30, docs/proposals/style-browser-
// corpus-scale.md §3.2): declaration order of StyleFamily, kOther last (true
// by construction -- kOther IS declared last in browser_model.hpp).
constexpr std::array<StyleFamily, 8> kFamilyRenderOrder = {
    StyleFamily::kPopRockBallad, StyleFamily::kDanceFourOnFloor,
    StyleFamily::kFunkGroove,    StyleFamily::kSwingShuffleJazz,
    StyleFamily::kLatinClave,    StyleFamily::kBallroomTraditional,
    StyleFamily::kWorldRegional, StyleFamily::kOther,
};

// The non-style sections (spec §2a). "variations" is now a REAL drag
// source (repeat-zone-real-contract.md SLICE 4a): each row carries a
// SectionType byte a scene header (grid_panel.cpp) accepts as a drop target
// to set that column's section. "kits" now sends the real GM percussion-kit
// program-change verb (render_kits below), the same way "voices" already does.
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
// Wire name each kVariations row sends via `style section <name>` on click --
// index-parallel with kVariations/kVariationSections above, mirroring
// grid_model.cpp's own section_wire_name() table at these same numeric
// SectionType indices (0=intro1, 2=varA, 3=varB, 4=varC, 5=varD, 10=break,
// 6=fillA, 11=ending1).
constexpr std::array<std::string_view, 8> kVariationWireNames = {
    "intro1", "varA", "varB", "varC", "varD", "break", "fillA", "ending1",
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
  const bool open =
      ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
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

// One style leaf row: the SAME click-to-load/switch and drag-drop-source
// behavior render_styles always had, factored out so both the family-
// grouped loop below and its ImGuiListClipper wrapper can call it per row.
void render_style_leaf(BrowserModel& model, BrainSession& brain_session, const AppState& app_state,
                       UiState& fx, std::size_t i) {
  const std::string name(model.style_name(i));
  ImGui::PushID(static_cast<int>(i));
  if (leaf_row(name, fx.active_style == static_cast<int>(i))) {
    // While playing, morph live (quantized to the next bar) instead of
    // hard-resetting the arranger -- `style load` still stops-and-reloads
    // for the not-yet-playing case (in_process_brain_session.cpp's
    // command_line_to_command).
    //
    // Owner task #2: the switch used to silently default to varA (the
    // translator's own fallback, in_process_brain_session.cpp). Pass the
    // engine's OWN current section (app_state.section(), the authoritative
    // kSection echo -- never a guess) as an explicit suffix so the switch
    // preserves it. An empty or "-" reading (no section committed yet)
    // falls back to the translator's own 3-token/varA default rather than
    // sending a malformed suffix.
    const std::string_view current_section = app_state.section();
    std::string verb = fx.playing ? "style switch " + name : "style load " + name;
    if (fx.playing && !current_section.empty() && current_section != "-") {
      verb += " section " + std::string(current_section);
    }
    brain_session.send(verb);
    fx.active_style = static_cast<int>(i);
  }
  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(kStyleDragPayloadId, &i, sizeof(i));
    ImGui::TextUnformatted(name.c_str());
    ImGui::EndDragDropSource();
  }
  ImGui::PopID();
}

// Below kFlatListThreshold, skip family grouping entirely and render one
// flat "styles" header + list -- the same shape browser_panel.cpp had
// before task #30 (see git commit f722000^'s own render_styles), factored
// here to share render_style_leaf with the grouped path above/below. No
// ImGuiListClipper -- a corpus this small never needs virtualizing.
void render_styles_flat(BrowserModel& model, BrainSession& brain_session, const AppState& app_state,
                        UiState& fx, int& shown) {
  if (!section_header("styles")) {
    return;
  }
  int local_shown = 0;
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    if (!model.style_matches_filter(i)) {
      continue;
    }
    ++local_shown;
    ++shown;
    render_style_leaf(model, brain_session, app_state, fx, i);
  }
  if (local_shown == 0) {
    ImGui::TextDisabled("  (no match)");
  }
  ImGui::TreePop();
}

// One family bucket: a flat, ImGuiListClipper-virtualized list of leaves
// (task #30, docs/proposals/style-browser-corpus-scale.md §3.2) -- NOT a
// nested tree per family, since the vendored ImGui's own demo notes
// clipping composes awkwardly with tree nodes (imgui_demo.cpp:4200). Each
// family gets one always-visible section_header (reusing the existing
// helper unchanged) followed by its own flat, clipped leaf list; an empty
// bucket (no members, or nothing matching the current filter) now renders
// nothing at all -- see kFlatListThreshold above for why.
void render_style_family_section(BrowserModel& model, BrainSession& brain_session,
                                 const AppState& app_state, UiState& fx, StyleFamily family,
                                 int& shown) {
  std::vector<std::size_t> indices;
  for (std::size_t i = 0; i < model.style_count(); ++i) {
    if (model.style_family(i) == family && model.style_matches_filter(i)) {
      indices.push_back(i);
    }
  }
  if (indices.empty()) {
    // A family bucket with nothing in it (or nothing matching the current
    // filter) renders NOTHING -- not a header plus "(no match)". A player
    // should never see a bucket that can never hold anything for the
    // current corpus (owner correction 2026-07-17).
    return;
  }
  const std::string title(style_family_label(family));
  if (!section_header(title.c_str())) {
    return;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(indices.size()));
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const std::size_t i = indices[static_cast<std::size_t>(row)];
      ++shown;
      render_style_leaf(model, brain_session, app_state, fx, i);
    }
  }
  ImGui::TreePop();
}

void render_styles(BrowserModel& model, BrainSession& brain_session, const AppState& app_state,
                   UiState& fx, int& shown) {
  if (model.style_count() <= kFlatListThreshold) {
    render_styles_flat(model, brain_session, app_state, fx, shown);
    return;
  }
  for (const StyleFamily family : kFamilyRenderOrder) {
    render_style_family_section(model, brain_session, app_state, fx, family, shown);
  }
}

// "variations" (kVariations/kVariationSections/kVariationWireNames above): a
// real drag SOURCE (repeat-zone-real-contract.md SLICE 4a), each row carrying
// its own SectionType byte under kVariationDragPayloadId -- distinct from
// kStyleDragPayloadId so grid_panel.cpp's scene-header drop target never
// confuses the two payload shapes. Clicking a row (leaf_row's own return
// value) now sends `style section <wire_name>` directly (Shell::cmd_style,
// "quantized to next bar while playing, immediate otherwise") -- a simpler,
// standalone "switch section now" action, independent of grid_panel.cpp's own
// scene-header mechanism. Click and drag are not mutually exclusive: click
// applies the section now, drag still targets a grid scene-header column.
void render_variations(BrainSession& brain_session, const std::string& filter, int& shown) {
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
    if (leaf_row(item, /*active=*/false)) {
      brain_session.send("style section " + std::string(kVariationWireNames[i]));
    }
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

// Destination picker for the Voices tab (browser-redesign-taxonomy.md Phase
// 1's own note: "genuinely new surface needed"). Port name + 1-based channel,
// persisted on the model (not UiState -- out of scope for this slice), so it
// survives switching categories and scrolling the voice list.
void render_voice_destination_picker(BrowserModel& model) {
  constexpr int kPortBufSize = 32;
  char port_buf[kPortBufSize];
  const std::string current_port(model.voice_port());
  std::strncpy(port_buf, current_port.c_str(), sizeof(port_buf) - 1);
  port_buf[sizeof(port_buf) - 1] = '\0';

  ImGui::TextColored(theme::kTextMuted, "destination (port:ch)");
  const float half = (ImGui::GetContentRegionAvail().x - 6.0F) * 0.5F;
  ImGui::SetNextItemWidth(half);
  if (ImGui::InputTextWithHint("##voice_port", "out0", port_buf, sizeof(port_buf))) {
    model.set_voice_port(std::string(port_buf));
  }
  ImGui::SameLine(0.0F, 6.0F);
  int channel = model.voice_channel();
  ImGui::SetNextItemWidth(half);
  if (ImGui::InputInt("##voice_channel", &channel)) {
    model.set_voice_channel(channel);
  }
}

// Destination picker for the Kits tab -- mirrors render_voice_destination_picker
// exactly, but reads/writes the model's INDEPENDENT kit_port/kit_channel state
// (defaults to channel 10, the GM percussion channel, not channel 1).
void render_kit_destination_picker(BrowserModel& model) {
  constexpr int kPortBufSize = 32;
  char port_buf[kPortBufSize];
  const std::string current_port(model.kit_port());
  std::strncpy(port_buf, current_port.c_str(), sizeof(port_buf) - 1);
  port_buf[sizeof(port_buf) - 1] = '\0';

  ImGui::TextColored(theme::kTextMuted, "destination (port:ch)");
  const float half = (ImGui::GetContentRegionAvail().x - 6.0F) * 0.5F;
  ImGui::SetNextItemWidth(half);
  if (ImGui::InputTextWithHint("##kit_port", "out0", port_buf, sizeof(port_buf))) {
    model.set_kit_port(std::string(port_buf));
  }
  ImGui::SameLine(0.0F, 6.0F);
  int channel = model.kit_channel();
  ImGui::SetNextItemWidth(half);
  if (ImGui::InputInt("##kit_channel", &channel)) {
    model.set_kit_channel(channel);
  }
}

// "voices · sounds": the 128 GM program names (kGmVoiceNames, browser_model.
// hpp), ImGuiListClipper-virtualized like the style-family buckets (§3.2 --
// 128 rows is well past kFlatListThreshold). Click sends `program <port>[:ch]
// <voice>` to the CURRENT destination picker state and marks the row as the
// local "last sent" echo (NOT wire-confirmed -- no per-part program readback
// exists, parts_model.hpp).
void render_voices(BrowserModel& model, BrainSession& brain_session, const std::string& filter,
                   int& shown) {
  if (!section_header("voices \xC2\xB7 sounds")) {
    return;
  }
  std::vector<std::size_t> indices;
  for (std::size_t i = 0; i < model.voice_count(); ++i) {
    if (matches(model.voice_name(i), filter)) {
      indices.push_back(i);
    }
  }
  if (indices.empty()) {
    ImGui::TextDisabled("  (no match)");
    ImGui::TreePop();
    return;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(indices.size()));
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const std::size_t i = indices[static_cast<std::size_t>(row)];
      ++shown;
      const std::string name(model.voice_name(i));
      ImGui::PushID(static_cast<int>(i));
      if (leaf_row(name, model.last_voice_sent() == static_cast<int>(i))) {
        brain_session.send(model.build_program_verb(name));
        model.set_last_voice_sent(static_cast<int>(i));
      }
      ImGui::PopID();
    }
  }
  ImGui::TreePop();
}

// "kits · GM": the 9 canonical GM Level 2 percussion-kit names
// (kGmDrumKitNames, browser_model.hpp). Click sends `program <port>[:ch]
// <program-number>` to the CURRENT kit destination picker state (defaults to
// channel 10, the GM percussion channel) and marks the row as the local
// "last sent" echo (NOT wire-confirmed -- no per-part program readback
// exists, parts_model.hpp -- same caveat render_voices's own comment
// documents).
void render_kits(BrowserModel& model, BrainSession& brain_session, const std::string& filter,
                 int& shown) {
  if (!section_header("kits \xC2\xB7 GM")) {
    return;
  }
  std::vector<std::size_t> indices;
  for (std::size_t i = 0; i < model.kit_count(); ++i) {
    if (matches(model.kit_name(i), filter)) {
      indices.push_back(i);
    }
  }
  if (indices.empty()) {
    ImGui::TextDisabled("  (no match)");
    ImGui::TreePop();
    return;
  }
  for (const std::size_t i : indices) {
    ++shown;
    const std::string name(model.kit_name(i));
    ImGui::PushID(static_cast<int>(i));
    if (leaf_row(name, model.last_kit_sent() == static_cast<int>(i))) {
      brain_session.send(model.build_kit_verb(i));
      model.set_last_kit_sent(static_cast<int>(i));
    }
    ImGui::PopID();
  }
  ImGui::TreePop();
}

// Family filter combo (task #30): "All families" (nullopt) plus one entry
// per StyleFamily, in the SAME kFamilyRenderOrder the sections below render
// in, so the combo's own listed order matches what a player sees scrolling
// the tree. ANDed with the text search below via BrowserModel::
// style_matches_filter -- selecting one never clears/replaces the text
// field's own filter.
void render_family_filter_combo(BrowserModel& model) {
  const std::optional<StyleFamily> current = model.family_filter();
  const std::string preview =
      current.has_value() ? std::string(style_family_label(*current)) : std::string("All families");
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##browser_family_filter", preview.c_str())) {
    if (ImGui::Selectable("All families", !current.has_value())) {
      model.set_family_filter(std::nullopt);
    }
    for (const StyleFamily family : kFamilyRenderOrder) {
      const bool selected = current.has_value() && *current == family;
      const std::string label(style_family_label(family));
      ImGui::PushID(static_cast<int>(family));
      if (ImGui::Selectable(label.c_str(), selected)) {
        model.set_family_filter(family);
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
}

constexpr std::array<BrowserCategory, kBrowserCategoryCount> kCategoryOrder = {
    BrowserCategory::kStyles, BrowserCategory::kVariations, BrowserCategory::kVoices,
    BrowserCategory::kKits,   BrowserCategory::kClips,
};

// Outer category selector (decision fork 1): the SAME combo idiom as
// render_family_filter_combo above, reused rather than a literal ImGui tab
// bar -- see this file's own family-filter combo for the precedent and
// docs/proposals/browser-redesign-taxonomy.md §3 for why a tab bar does not
// fit 210px.
void render_category_combo(BrowserModel& model) {
  const BrowserCategory current = model.category();
  const std::string preview(browser_category_label(current));
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##browser_category", preview.c_str())) {
    for (const BrowserCategory category : kCategoryOrder) {
      const bool selected = category == current;
      const std::string label(browser_category_label(category));
      ImGui::PushID(static_cast<int>(category));
      if (ImGui::Selectable(label.c_str(), selected)) {
        model.set_category(category);
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
}

}  // namespace

void render_browser_panel(BrowserModel& model, BrainSession& brain_session,
                          const AppState& app_state, UiState& fx) {
  ImGui::TextColored(theme::kPink, "BROWSER");
  ImGui::Spacing();
  render_category_combo(model);
  ImGui::Spacing();

  const BrowserCategory category = model.category();
  const bool flat_style_mode = model.style_count() <= kFlatListThreshold;
  // The family-filter combo only earns its vertical space once the corpus
  // is big enough to need family grouping in the first place (see
  // kFlatListThreshold above) -- for today's 16 built-ins it would just be
  // dead space over a flat list.
  if (category == BrowserCategory::kStyles && !flat_style_mode) {
    render_family_filter_combo(model);
    ImGui::Spacing();
  }
  if (category == BrowserCategory::kVoices) {
    render_voice_destination_picker(model);
    ImGui::Spacing();
  }
  if (category == BrowserCategory::kKits) {
    render_kit_destination_picker(model);
    ImGui::Spacing();
  }

  const float search_h = ImGui::GetFrameHeightWithSpacing() + 4.0F;
  ImGui::BeginChild("browser_tree", ImVec2(0.0F, ImGui::GetContentRegionAvail().y - search_h),
                    ImGuiChildFlags_None, ImGuiWindowFlags_None);
  const std::string filter = model.search_filter();
  int shown = 0;
  switch (category) {
    case BrowserCategory::kStyles:
      render_styles(model, brain_session, app_state, fx, shown);
      break;
    case BrowserCategory::kVariations:
      render_variations(brain_session, filter, shown);
      break;
    case BrowserCategory::kVoices:
      render_voices(model, brain_session, filter, shown);
      break;
    case BrowserCategory::kKits:
      render_kits(model, brain_session, filter, shown);
      break;
    case BrowserCategory::kClips:
      if (section_header("clips")) {
        ImGui::TextDisabled("  (none authored yet)");
        ImGui::TreePop();
      }
      break;
  }
  ImGui::EndChild();

  // Search field pinned at the bottom -- scoped to the ACTIVE category only
  // (browser-redesign-taxonomy.md §3: per-tab search, never a global
  // cross-family search).
  char buffer[kFilterBufferSize];
  std::strncpy(buffer, filter.c_str(), sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputTextWithHint("##browser_search", "search\xE2\x80\xA6", buffer, sizeof(buffer))) {
    model.set_search_filter(std::string(buffer));
  }
}

}  // namespace sonotron

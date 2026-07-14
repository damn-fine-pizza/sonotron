#include "theme.hpp"

#include <cstdarg>
#include <cstdio>

namespace sonotron::theme {

namespace {

// A translucent wash of `color` at the given alpha -- ImGui alpha-blends
// every draw against whatever surface sits underneath it, so this is the
// direct ImGui equivalent of the design ref's
// `color-mix(in srgb, X n%, transparent)` hover/selection washes.
ImVec4 wash(const ImVec4& color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }

}  // namespace

void apply() {
  ImGuiStyle& style = ImGui::GetStyle();

  // ---- Geometry (tokens/spacing.css): square corners, ImGui's 4/8 rhythm.
  // Radius is the exception, not the rule -- only tabs round.
  style.WindowRounding = 0.0F;
  style.ChildRounding = 0.0F;
  style.FrameRounding = 0.0F;
  style.PopupRounding = 0.0F;
  style.ScrollbarRounding = 0.0F;
  style.GrabRounding = 0.0F;
  style.TabRounding = 4.0F;  // --sn-radius-tab, the one exception

  style.WindowBorderSize = 1.0F;
  style.ChildBorderSize = 1.0F;
  style.PopupBorderSize = 1.0F;
  style.FrameBorderSize = 1.0F;

  style.WindowPadding = ImVec2(8.0F, 8.0F);  // --sn-pad-panel
  style.FramePadding = ImVec2(4.0F, 3.0F);   // --sn-pad-frame-x/y
  style.ItemSpacing = ImVec2(8.0F, 4.0F);    // --sn-gap-item / --sn-gap-row
  style.IndentSpacing = 21.0F;               // --sn-indent

  // ---- Colors (tokens/colors.css) ----
  ImVec4* colors = style.Colors;

  colors[ImGuiCol_Text] = kText;
  colors[ImGuiCol_TextDisabled] = kTextMuted;
  colors[ImGuiCol_WindowBg] = kWindowBg;
  colors[ImGuiCol_ChildBg] = kPanelBg;
  colors[ImGuiCol_PopupBg] = kPopupBg;
  colors[ImGuiCol_Border] = kBorder;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);

  colors[ImGuiCol_FrameBg] = kFrameBg;
  colors[ImGuiCol_FrameBgHovered] = kFrameBgHover;
  colors[ImGuiCol_FrameBgActive] = kFrameBgActive;

  colors[ImGuiCol_TitleBg] = kTitleBg;
  colors[ImGuiCol_TitleBgActive] = kTitleBg;
  colors[ImGuiCol_TitleBgCollapsed] = wash(kTitleBg, 0.75F);

  colors[ImGuiCol_MenuBarBg] = kMenuBarBg;

  colors[ImGuiCol_ScrollbarBg] = kPanelBg2;
  colors[ImGuiCol_ScrollbarGrab] = kBorderStrong;
  colors[ImGuiCol_ScrollbarGrabHovered] = kAccent;
  colors[ImGuiCol_ScrollbarGrabActive] = kAccentActive;

  colors[ImGuiCol_CheckMark] = kAccent;
  colors[ImGuiCol_CheckboxSelectedBg] = kAccentRest;
  colors[ImGuiCol_SliderGrab] = kAccentGrab;
  colors[ImGuiCol_SliderGrabActive] = kAccentActive;

  colors[ImGuiCol_Button] = kAccentRest;
  colors[ImGuiCol_ButtonHovered] = kAccent;
  colors[ImGuiCol_ButtonActive] = kAccentActive;

  colors[ImGuiCol_Header] = kHeader;
  colors[ImGuiCol_HeaderHovered] = wash(kAccent, 0.22F);
  colors[ImGuiCol_HeaderActive] = wash(kAccent, 0.35F);

  colors[ImGuiCol_Separator] = kBorder;
  colors[ImGuiCol_SeparatorHovered] = kAccent;
  colors[ImGuiCol_SeparatorActive] = kAccentActive;

  colors[ImGuiCol_ResizeGrip] = wash(kBorderStrong, 0.30F);
  colors[ImGuiCol_ResizeGripHovered] = kAccent;
  colors[ImGuiCol_ResizeGripActive] = kAccentActive;

  colors[ImGuiCol_InputTextCursor] = kText;

  colors[ImGuiCol_TabHovered] = kAccent;
  colors[ImGuiCol_Tab] = kFrameBg;
  colors[ImGuiCol_TabSelected] = kHeader;
  colors[ImGuiCol_TabSelectedOverline] = kAccent;
  colors[ImGuiCol_TabDimmed] = kFrameBg;
  colors[ImGuiCol_TabDimmedSelected] = kHeader;
  colors[ImGuiCol_TabDimmedSelectedOverline] = wash(kAccent, 0.5F);

  colors[ImGuiCol_TableHeaderBg] = kPanelBg2;
  colors[ImGuiCol_TableBorderStrong] = kBorderStrong;
  colors[ImGuiCol_TableBorderLight] = kBorder;
  colors[ImGuiCol_TableRowBg] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
  colors[ImGuiCol_TableRowBgAlt] = wash(kPanelBg2, 0.5F);

  colors[ImGuiCol_TextLink] = kAccent;
  colors[ImGuiCol_TextSelectedBg] = wash(kAccent, 0.35F);
  colors[ImGuiCol_TreeLines] = kBorder;

  colors[ImGuiCol_DragDropTarget] = kAmber;
  colors[ImGuiCol_DragDropTargetBg] = wash(kAmber, 0.20F);
  colors[ImGuiCol_UnsavedMarker] = kAmber;

  colors[ImGuiCol_NavCursor] = kAccent;
  colors[ImGuiCol_NavWindowingHighlight] = wash(kText, 0.70F);
  colors[ImGuiCol_NavWindowingDimBg] = wash(kWindowBg, 0.20F);
  colors[ImGuiCol_ModalWindowDimBg] = wash(kWindowBg, 0.60F);
}

void text_bold_colored(const ImVec4& color, const char* fmt, ...) {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImGui::TextColored(color, "%s", buffer);  // real draw, real cursor advance
  ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 1.0F, pos.y),
                                      ImGui::ColorConvertFloat4ToU32(color), buffer);
}

void render_meter_cells(int cells, int filled, const ImVec4& fill_color, const ImVec2& cell_size,
                        float gap) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImU32 fill = ImGui::ColorConvertFloat4ToU32(fill_color);
  const ImU32 track = ImGui::ColorConvertFloat4ToU32(kMeterTrack);
  const ImU32 border = ImGui::ColorConvertFloat4ToU32(kBorder);

  for (int i = 0; i < cells; ++i) {
    const ImVec2 top_left(pos.x + static_cast<float>(i) * (cell_size.x + gap), pos.y);
    const ImVec2 bottom_right(top_left.x + cell_size.x, top_left.y + cell_size.y);
    if (i < filled) {
      draw_list->AddRectFilled(top_left, bottom_right, fill);
    } else {
      draw_list->AddRectFilled(top_left, bottom_right, track);
      draw_list->AddRect(top_left, bottom_right, border);
    }
  }

  const float total_width =
      static_cast<float>(cells) * cell_size.x + static_cast<float>(cells - 1) * gap;
  ImGui::Dummy(ImVec2(total_width, cell_size.y));
}

}  // namespace sonotron::theme

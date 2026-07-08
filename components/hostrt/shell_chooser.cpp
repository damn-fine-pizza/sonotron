#include "shell.hpp"
#include "shell_internal.hpp"

#include "rc_config.hpp"

// Style/section chooser plumbing (dissolved into the always-present styles
// panel): focus, keyboard routing, debounced stepping, arrow/ESC drivers, and
// the ~/.arrangrr.rc panel layout apply. Bodies moved verbatim from shell.cpp.

namespace arrangrr::host {

using namespace shell_detail;

bool Shell::styles_focused() const {
  return m_panels.focus_kind() == PanelFocus::kPanel &&
         m_panels.focused_panel() == PanelId::kStyles;
}

bool Shell::repl_focused() const { return m_panels.focus_kind() == PanelFocus::kRepl; }

void Shell::focus_styles() {
  // Backtick shortcut: focus the styles panel (opens it) and seed the chooser
  // from the arranger. push_panels seeds on the repl->panel focus edge.
  m_panels.focus(PanelId::kStyles);
  (void)push_panels();
}

void Shell::focus_prev() {
  m_panels.focus_prev();
  (void)push_panels();
}

void Shell::seed_chooser_selection() {
  // Land the chooser where the band already is: the loaded built-in style and
  // its current section.
  int idx = 0;
  if (const Style* loaded = m_engine.arranger().current_style(); loaded != nullptr) {
    for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
      if (styles::kBuiltins[i] == loaded) {
        idx = static_cast<int>(i);
        break;
      }
    }
  }
  const SectionType current = m_engine.arranger().current();
  m_chooser.select(idx, current);
  // Mirror into the debounced pending so a following step continues from here.
  m_step_style_index = idx;
  m_step_section = current;
}

bool Shell::chooser_key(std::uint8_t byte) {
  if (byte >= kAsciiDigitLow && byte <= kAsciiDigitHigh) {
    m_chooser.feed_digit(static_cast<char>(byte));
    (void)push_panels();
    return true;
  }
  if (byte == kBackspaceDel || byte == kBackspaceBs) {
    m_chooser.backspace();
    (void)push_panels();
    return true;
  }
  if (byte == kEnterCr || byte == kEnterLf) {
    chooser_apply(ChooserApply::kNextBar);
    return true;
  }
  if (byte == kCtrlApplyNow) {
    chooser_apply(ChooserApply::kImmediate);
    return true;
  }
  // Variation/style stepping drives the chooser highlight AND the debounced
  // pending selection: -/= step the variation, _/+ the style.
  if (style_step_key(byte)) {
    return true;
  }
  // Note-letters no longer set the scale here: they are routed to the band by the
  // global steer choke in handle_ui_key BEFORE chooser_key runs. The scale/key is
  // set only via the `scale`/`key` command now (it does NOT transpose the band).
  // Every other byte is swallowed while the styles panel is focused.
  return true;
}

void Shell::chooser_apply(ChooserApply mode) {
  if (const StyleInfo* style = m_chooser.selected_style(); style != nullptr) {
    // Same engine entry point cmd_style uses (m_sink); the core forces
    // immediate when the transport is stopped regardless of the flag.
    Command c;
    c.op = Op::kDo;
    c.param = Param::kStyleSwitch;
    c.a = style->index;
    c.b = static_cast<std::int32_t>(m_chooser.selected_section());
    c.c = mode == ChooserApply::kImmediate ? 1 : 0;
    m_engine.push_command(c, m_sink);
  }
  // Applying keeps the styles panel focused, so you can keep switching styles
  // and sections in a row.
  (void)push_panels();
}

bool Shell::style_step_key(std::uint8_t byte) {
  switch (byte) {
    case kStepSectionPrev:
      style_step(StyleStepAxis::kSection, -1);
      return true;
    case kStepSectionNext:
      style_step(StyleStepAxis::kSection, +1);
      return true;
    case kStepStylePrev:
      style_step(StyleStepAxis::kStyle, -1);
      return true;
    case kStepStyleNext:
      style_step(StyleStepAxis::kStyle, +1);
      return true;
    default:
      return false;
  }
}

void Shell::style_step(StyleStepAxis axis, int delta) {
  // The always-present chooser IS the styles+variations screen: drive its
  // highlight, then mirror the selection into the debounced pending so the
  // ~500 ms apply lands on exactly what is shown. nav_style preserves the
  // variation by type across a style change (no jump back to the first).
  if (axis == StyleStepAxis::kStyle) {
    m_chooser.nav_style(delta);
  } else {
    m_chooser.nav_section(delta);
  }
  if (const StyleInfo* style = m_chooser.selected_style(); style != nullptr) {
    m_step_style_index = style->index;
  }
  m_step_section = m_chooser.selected_section();
  ++m_style_step_gen;
  m_style_step_pending = true;
  (void)push_panels();
}

void Shell::apply_style_step() {
  if (!m_style_step_pending) {
    return;
  }
  // Same engine entry point chooser_apply uses; immediate=false so a running
  // transport lands the switch on the next bar (the core forces immediate when
  // stopped).
  Command c;
  c.op = Op::kDo;
  c.param = Param::kStyleSwitch;
  c.a = m_step_style_index;
  c.b = static_cast<std::int32_t>(m_step_section);
  c.c = 0;
  m_engine.push_command(c, m_sink);
  m_style_step_pending = false;
  (void)push_panels();
}

void Shell::chooser_nav_style(int delta) {
  // Arrow up/down while the styles panel is focused: move the style highlight
  // (variation preserved by type). ENTER applies; arrows do not auto-switch.
  if (styles_focused()) {
    m_chooser.nav_style(delta);
    (void)push_panels();
  }
}

void Shell::chooser_nav_section(int delta) {
  if (styles_focused()) {
    m_chooser.nav_section(delta);
    (void)push_panels();
  }
}

void Shell::chooser_cancel() {
  // ESC while the styles panel is focused drops focus back to the REPL.
  if (styles_focused()) {
    m_panels.focus_repl();
    (void)push_panels();
  }
}

void Shell::apply_rc(const RcConfig& rc) {
  if (rc.has_layout) {
    m_panels.set_per_row(rc.per_row);
  }
  if (!rc.order.empty()) {
    std::vector<PanelId> ids;
    ids.reserve(rc.order.size());
    for (const RcPanel& entry : rc.order) {
      ids.push_back(entry.id);
    }
    m_panels.set_order(ids);
    m_panels.close_all();
    for (const RcPanel& entry : rc.order) {
      m_panels.open(entry.id);
      m_panels.set_full_row(entry.id, entry.full_row);
      if (entry.height > 0) {
        m_panels.set_height(entry.id, entry.height);
      }
    }
    m_panels.open(PanelId::kStyles);  // the styles panel is always present
  }
  for (const std::string& warning : rc.warnings) {
    console_output("rc: " + warning);
  }
  (void)push_panels();
}

}  // namespace arrangrr::host

// Host-only FUNCTIONAL tests for TUI panel navigation: focus order, panel
// numbering, Tab / Shift+Tab cycling and the editbox focus flag. The subject is
// the PURE, terminal-free logic in PanelManager (the render is immediate-mode
// and untestable at the pixel level, but every navigation decision lives here),
// plus the thin Shell seam that the live loop drives (repl_focused / focus_prev
// / TAB+digit). These lock the just-fixed VISUAL top-to-bottom convention so the
// tab-order/numbering class of bug can never regress, and probe the residual
// edge cases (numbering after set_order, focus_next when a neighbour is hidden,
// closing the focused panel). Deterministic: no RNG, no terminal, no timing.

#include <algorithm>
#include <string>
#include <vector>

#include "arrangrr/abi.hpp"
#include "panel_manager.hpp"
#include "shell.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

// The 11 canonical panels, so a test can iterate the whole universe and assert
// hidden panels number 0 while the visible set numbers exactly 1..N.
const std::vector<PanelId> kAllPanels = {
    PanelId::kEvents, PanelId::kConsole, PanelId::kStyles, PanelId::kChords,
    PanelId::kPiano,  PanelId::kHelp,    PanelId::kFilter, PanelId::kEmpty,
    PanelId::kParts,  PanelId::kGroove,  PanelId::kArp,
};

bool contains(const std::vector<PanelId>& v, PanelId id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}

// The contract's single source of truth, computed INDEPENDENTLY of the code
// under test: given the bottom-to-top order and the visible set, the on-screen
// top-to-bottom order (the one panel numbers and Tab focus must follow) is the
// reverse of the order filtered to the visible panels.
std::vector<PanelId> expected_top_to_bottom(const std::vector<PanelId>& bottom_to_top,
                                            const std::vector<PanelId>& visible) {
  std::vector<PanelId> vis;
  for (const PanelId id : bottom_to_top) {
    if (contains(visible, id)) {
      vis.push_back(id);
    }
  }
  std::reverse(vis.begin(), vis.end());
  return vis;
}

PanelManager make_manager(const std::vector<PanelId>& bottom_to_top,
                          const std::vector<PanelId>& visible) {
  PanelManager pm;
  pm.set_order(bottom_to_top);
  for (const PanelId id : visible) {
    pm.open(id);
  }
  return pm;
}

// --- Contract 1 + 2: numbering is a top-to-bottom permutation of the visible
// set; focus_number(n) is 1-based top-to-bottom and rejects out-of-range. -----
void check_numbering_and_focus_number(const std::vector<PanelId>& order,
                                      const std::vector<PanelId>& visible) {
  PanelManager pm = make_manager(order, visible);
  const std::vector<PanelId> top = expected_top_to_bottom(order, visible);
  const int n = static_cast<int>(top.size());

  // Numbering: exactly the visible panels carry 1..N (top=1), hidden ones 0.
  std::vector<int> seen_numbers;
  for (const PanelId id : kAllPanels) {
    if (contains(visible, id)) {
      const int num = pm.panel_number(id);
      CHECK(num >= 1 && num <= n);
      seen_numbers.push_back(num);
    } else {
      CHECK(pm.panel_number(id) == 0);
    }
  }
  // The numbers of the visible set are a permutation of {1..N} (no gaps/dupes).
  std::sort(seen_numbers.begin(), seen_numbers.end());
  for (int i = 0; i < n; ++i) {
    CHECK(seen_numbers[static_cast<std::size_t>(i)] == i + 1);
  }
  // ...and they descend the screen: expected top-to-bottom[i] is number i+1.
  for (int i = 0; i < n; ++i) {
    CHECK(pm.panel_number(top[static_cast<std::size_t>(i)]) == i + 1);
  }

  // focus_number(n): 1-based top-to-bottom, lands on the nth-from-top panel.
  pm.focus_repl();
  for (int i = 1; i <= n; ++i) {
    CHECK(pm.focus_number(i));
    CHECK(pm.focus_kind() == PanelFocus::kPanel);
    CHECK(pm.focused_panel() == top[static_cast<std::size_t>(i - 1)]);
    CHECK(pm.panel_number(pm.focused_panel()) == i);
  }

  // Out-of-range leaves focus untouched. Park on a known state first.
  if (n > 0) {
    CHECK(pm.focus_number(1));
  } else {
    pm.focus_repl();
  }
  const PanelFocus kind_before = pm.focus_kind();
  const PanelId panel_before = pm.focused_panel();
  for (const int bad : {0, -1, n + 1, n + 5}) {
    CHECK(!pm.focus_number(bad));
    CHECK(pm.focus_kind() == kind_before);
    CHECK(pm.focused_panel() == panel_before);
  }
}

// --- Contract 3 + 4 + 6: focus_next (Tab) walks repl -> #1 -> ... -> #N -> repl
// forever; focus_prev (Shift+Tab) is the exact reverse; neither ever lands on a
// hidden panel. Asserted over two full periods to pin periodicity. ------------
void check_cycles(const std::vector<PanelId>& order, const std::vector<PanelId>& visible) {
  const std::vector<PanelId> top = expected_top_to_bottom(order, visible);
  const int n = static_cast<int>(top.size());
  const int period = n + 1;  // N panels + the repl

  // focus_next: repl, then top..bottom, then back to repl — periodic.
  {
    PanelManager pm = make_manager(order, visible);
    pm.focus_repl();
    for (int k = 0; k < 2 * period; ++k) {
      pm.focus_next();
      const int pos = k % period;  // 0..N-1 = panels top->bottom, N = repl
      if (pos < n) {
        CHECK(pm.focus_kind() == PanelFocus::kPanel);
        CHECK(pm.focused_panel() == top[static_cast<std::size_t>(pos)]);
        CHECK(pm.visible(pm.focused_panel()));  // never a hidden panel
      } else {
        CHECK(pm.focus_kind() == PanelFocus::kRepl);
      }
    }
  }

  // focus_prev: repl, then bottom..top, then back to repl — the mirror image.
  {
    PanelManager pm = make_manager(order, visible);
    pm.focus_repl();
    for (int k = 0; k < 2 * period; ++k) {
      pm.focus_prev();
      const int pos = k % period;  // 0..N-1 = panels bottom->top, N = repl
      if (pos < n) {
        CHECK(pm.focus_kind() == PanelFocus::kPanel);
        CHECK(pm.focused_panel() == top[static_cast<std::size_t>(n - 1 - pos)]);
        CHECK(pm.visible(pm.focused_panel()));
      } else {
        CHECK(pm.focus_kind() == PanelFocus::kRepl);
      }
    }
  }
}

// --- Contract 5: from ANY state, next-then-prev and prev-then-next are the
// identity (the cycle is a clean ring, so Tab and Shift+Tab are true inverses).
void check_inverse_round_trip(const std::vector<PanelId>& order,
                              const std::vector<PanelId>& visible) {
  const std::vector<PanelId> top = expected_top_to_bottom(order, visible);
  const int n = static_cast<int>(top.size());

  // Every reachable state is a starting point: the repl, plus each visible panel.
  for (int start = -1; start < n; ++start) {
    // next then prev == identity.
    {
      PanelManager pm = make_manager(order, visible);
      if (start < 0) {
        pm.focus_repl();
      } else {
        pm.focus(top[static_cast<std::size_t>(start)]);
      }
      const PanelFocus kind0 = pm.focus_kind();
      const PanelId panel0 = pm.focused_panel();
      pm.focus_next();
      pm.focus_prev();
      CHECK(pm.focus_kind() == kind0);
      if (kind0 == PanelFocus::kPanel) {
        CHECK(pm.focused_panel() == panel0);
      }
    }
    // prev then next == identity.
    {
      PanelManager pm = make_manager(order, visible);
      if (start < 0) {
        pm.focus_repl();
      } else {
        pm.focus(top[static_cast<std::size_t>(start)]);
      }
      const PanelFocus kind0 = pm.focus_kind();
      const PanelId panel0 = pm.focused_panel();
      pm.focus_prev();
      pm.focus_next();
      CHECK(pm.focus_kind() == kind0);
      if (kind0 == PanelFocus::kPanel) {
        CHECK(pm.focused_panel() == panel0);
      }
    }
  }
}

void run_contract(const std::vector<PanelId>& order, const std::vector<PanelId>& visible) {
  check_numbering_and_focus_number(order, visible);
  check_cycles(order, visible);
  check_inverse_round_trip(order, visible);
}

// A handful of configurations chosen to stress the order mapping: the empty
// set, a single panel, several panels in the DEFAULT order, several in a
// deliberately scrambled full permutation, and the whole grid. Full
// permutations keep the expected order independent of PanelManager's private
// default so the test stays a real oracle, not a mirror of the implementation.
void test_contract_across_configurations() {
  // A full 11-panel permutation, bottom-to-top, distinct from the default.
  const std::vector<PanelId> scrambled = {
      PanelId::kArp,    PanelId::kEvents, PanelId::kPiano,  PanelId::kFilter,
      PanelId::kStyles, PanelId::kGroove, PanelId::kHelp,   PanelId::kConsole,
      PanelId::kEmpty,  PanelId::kParts,  PanelId::kChords,
  };

  // 0 visible.
  run_contract(kAllPanels, {});
  // 1 visible.
  run_contract(kAllPanels, {PanelId::kPiano});
  run_contract(scrambled, {PanelId::kStyles});
  // Several visible, default order.
  run_contract(kAllPanels,
               {PanelId::kPiano, PanelId::kConsole, PanelId::kStyles, PanelId::kEvents});
  // Several visible, scrambled order (the bug lived exactly in this mapping).
  run_contract(scrambled, {PanelId::kPiano, PanelId::kConsole, PanelId::kStyles, PanelId::kEvents});
  run_contract(scrambled, {PanelId::kArp, PanelId::kFilter, PanelId::kHelp});
  // All visible.
  run_contract(kAllPanels, kAllPanels);
  run_contract(scrambled, kAllPanels);
}

// Locks the DEFAULT bottom-to-top order's numbering to the documented
// convention (events near the top, piano at the bottom) — a guard on the actual
// shipped default, complementing the order-independent checks above.
void test_default_order_numbering() {
  PanelManager pm;            // default order, nothing scrambled
  pm.open(PanelId::kPiano);   // default order puts piano at the bottom -> #2
  pm.open(PanelId::kStyles);  // styles paints above piano -> #1
  CHECK(pm.panel_number(PanelId::kStyles) == 1);
  CHECK(pm.panel_number(PanelId::kPiano) == 2);

  // Events sits near the top of the default open set: with events + piano open,
  // events is #1.
  PanelManager pm2;
  pm2.open(PanelId::kPiano);
  pm2.open(PanelId::kEvents);
  CHECK(pm2.panel_number(PanelId::kEvents) == 1);
  CHECK(pm2.panel_number(PanelId::kPiano) == 2);
}

// Contract 6: focusing then closing the focused panel returns focus to the repl;
// and focus_next after a NON-focused neighbour is hidden keeps a consistent
// cycle (the just-fixed order mapping must survive a mid-cycle visibility change).
void test_close_and_hidden_neighbour() {
  // Closing the focused panel -> repl.
  {
    PanelManager pm = make_manager(kAllPanels, {PanelId::kPiano, PanelId::kStyles});
    pm.focus(PanelId::kPiano);
    CHECK(pm.focus_kind() == PanelFocus::kPanel);
    pm.close(PanelId::kPiano);
    CHECK(pm.focus_kind() == PanelFocus::kRepl);
  }

  // Closing a DIFFERENT panel leaves focus put, and focus_next resumes from the
  // focused panel's NEW position in the reduced cycle. Order bottom->top:
  // [A(bottom)=kPiano, B=kConsole, C(top)=kStyles]; top-to-bottom = C,B,A.
  {
    const std::vector<PanelId> order = {PanelId::kPiano, PanelId::kConsole, PanelId::kStyles};
    PanelManager pm = make_manager(order, order);
    // Focus the TOP panel (#1 = kStyles).
    CHECK(pm.focus_number(1));
    CHECK(pm.focused_panel() == PanelId::kStyles);
    // Hide the MIDDLE panel (not focused): focus stays on kStyles.
    pm.close(PanelId::kConsole);
    CHECK(pm.focus_kind() == PanelFocus::kPanel);
    CHECK(pm.focused_panel() == PanelId::kStyles);
    // New top-to-bottom = [kStyles(#1), kPiano(#2)]; Tab from kStyles -> kPiano.
    CHECK(pm.panel_number(PanelId::kStyles) == 1);
    CHECK(pm.panel_number(PanelId::kPiano) == 2);
    pm.focus_next();
    CHECK(pm.focused_panel() == PanelId::kPiano);
    CHECK(pm.visible(pm.focused_panel()));
    pm.focus_next();  // bottom -> repl
    CHECK(pm.focus_kind() == PanelFocus::kRepl);
  }

  // close_all drops focus to the repl regardless of what was focused.
  {
    PanelManager pm = make_manager(kAllPanels, {PanelId::kArp, PanelId::kGroove});
    pm.focus(PanelId::kArp);
    pm.close_all();
    CHECK(pm.focus_kind() == PanelFocus::kRepl);
    CHECK(!pm.any_visible());
    // With nothing visible Tab/Shift+Tab stay on the repl, no dead end / crash.
    pm.focus_next();
    CHECK(pm.focus_kind() == PanelFocus::kRepl);
    pm.focus_prev();
    CHECK(pm.focus_kind() == PanelFocus::kRepl);
  }
}

// Contract 7: the Shell seam the live loop actually drives. repl_focused()
// mirrors focus_kind()==kRepl and flips as focus moves via the shell's public
// entry points (focus_next through handle_ui_key TAB, focus_prev = Shift+Tab,
// TAB+digit, and `panel focus repl`). This is the same code path main.cpp's
// finish_csi calls for Shift+Tab (shell.focus_prev()).
void test_shell_repl_focus_flag_and_backtab() {
  std::vector<OutEvent> events;
  Shell shell{[&](const OutEvent& ev) { events.push_back(ev); }};
  std::string err;

  CHECK(shell.exec_line("panel open piano", err));   // default order: bottom -> #2
  CHECK(shell.exec_line("panel open styles", err));  // above piano -> #1
  CHECK(shell.repl_focused());
  CHECK(shell.panels().focus_kind() == PanelFocus::kRepl);

  // TAB (via handle_ui_key) enters the TOP panel: repl flag clears.
  CHECK(shell.handle_ui_key('\t'));
  CHECK(!shell.repl_focused());
  CHECK(shell.panels().focus_kind() == PanelFocus::kPanel);
  CHECK(shell.panels().focused_panel() == PanelId::kStyles);  // #1 = top

  // Shift+Tab seam (main.cpp calls exactly this): from the top panel it steps
  // BACK to the repl, so the flag comes back true.
  shell.focus_prev();
  CHECK(shell.repl_focused());

  // Shift+Tab again from the repl wraps to the BOTTOM panel (#2 = piano).
  shell.focus_prev();
  CHECK(!shell.repl_focused());
  CHECK(shell.panels().focused_panel() == PanelId::kPiano);

  // TAB+digit jump: '1' after TAB focuses the top panel directly, clearing repl.
  shell.exec_line("panel focus repl", err);
  CHECK(shell.repl_focused());
  CHECK(shell.handle_ui_key('\t'));  // arms the digit jump (and enters top)
  CHECK(shell.handle_ui_key('1'));   // explicit #1
  CHECK(!shell.repl_focused());
  CHECK(shell.panels().focused_panel() == PanelId::kStyles);

  // Full forward cycle through the shell TAB: styles(#1) -> piano(#2) -> repl.
  CHECK(shell.handle_ui_key('\t'));
  CHECK(shell.panels().focused_panel() == PanelId::kPiano);
  CHECK(!shell.repl_focused());
  CHECK(shell.handle_ui_key('\t'));
  CHECK(shell.repl_focused());  // past the bottom -> back to the editbox

  // `panel focus repl` is idempotent and keeps the flag honest.
  CHECK(shell.exec_line("panel focus repl", err));
  CHECK(shell.repl_focused());
}

}  // namespace

int main() {
  test_contract_across_configurations();
  test_default_order_numbering();
  test_close_and_hidden_neighbour();
  test_shell_repl_focus_flag_and_backtab();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_panel_nav: all OK\n");
  }
  return arrangrr::test::failures();
}

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "arrangrr/engine.hpp"
#include "midi_monitor.hpp"
#include "midisrc/smf_write.hpp"
#include "orchestrator/accompany.hpp"
#include "panel_manager.hpp"
#include "piano_view.hpp"
#include "runtime/pipeline.hpp"
#include "runtime/runtime.hpp"
#include "style_chooser.hpp"

// Host shell: resolves L2/L1 text lines into binary core commands (D26) and
// owns everything the core must not know about — port names, pending
// @tick-scheduled lines, tempo parsing. Strings stop here.
//
// D29 semantics: `@tick <line>` only *schedules*; `advance` is the only
// driver of time. advance() executes pending lines exactly when stream time
// reaches their tick.

namespace arrangrr::host {

struct RcConfig;  // rc_config.hpp — ~/.arrangrr.rc parse result

struct PortDef {
  std::string name;  // user alias
  bool is_input = false;
  std::uint8_t index = 0;  // core port index
};

// Piano key-input policy (docs/TUI_SPEC.md §2).
//   kMomentary — note sounds while the key is physically held; note-off on
//                release. This needs true key-release events, which only the
//                kitty keyboard protocol delivers (a plain TTY has none), so it
//                is honoured through Shell::piano_key_event() fed by the parsed
//                kitty escapes. On terminals without the protocol only plain
//                bytes arrive and the piano degrades to kToggle (see below).
//   kToggle    — press = note-on, press the same key again = note-off. The only
//                policy a plain raw-mode TTY can implement honestly, and the
//                fallback for the plain-byte path in either mode.
enum class PianoKeyMode { kMomentary, kToggle };

// Which interactive mode the live TUI is in. Drives the contextual help
// panel: its content follows the mode (commands in the REPL, piano keys in
// play mode) unless the user pinned an explicit `help <topic>`.
enum class UiMode { kRepl, kPiano, kStyles, kParts, kGroove, kArp };

class Shell {
 public:
  using EventSink = std::function<void(const OutEvent&)>;
  // Called when a `port open` line runs, so a live backend can create the
  // OS-level port. May be empty (script/null backend).
  using PortHook = std::function<void(const PortDef&)>;
  // Receives the composed block of all visible panels (empty vector = none
  // visible). Returns true when a panel UI consumed it; false falls back to
  // plain printing.
  using PanelHook = std::function<bool(const std::vector<std::string>&)>;
  // Prints one informational line (`panel list` output and friends). The live
  // TUI wires this to the log pane; unset falls back to stdout.
  using PrintHook = std::function<void(const std::string&)>;
  // Supplies the current terminal width so panel renderers can adapt.
  using WidthProvider = std::function<int()>;
  // Supplies the rows available to the panel grid (Console::panel_rows()).
  using HeightProvider = std::function<int()>;
  // Sends raw bytes into the engine input path (used by the live backend).
  // Phase-4d (§16.3/§16.4): goes through the Pipeline's own fan-out now
  // (Seam C), not `m_engine` directly -- the SAME raw bytes reach both the
  // chorddet peer (its own MidiParser) and arrangrr's own routing.
  void feed_midi(std::uint8_t port, Span<const std::uint8_t> bytes) {
    m_runtime.stage().push_midi_in(port, bytes, m_sink);
  }

  // Accompany (roadmap 9310), docs/design/orchestrator-pipeline-
  // extraction.md §16.5/§16.7, Phase 4d: loads a plain Standard MIDI File
  // into the pipeline's (always-declared, inert-until-loaded) MIDI-source
  // stage (its output port is fixed at Shell construction, distinct from
  // the band's typical port, §16.2(b)). Returns false with `error` set on a
  // bad path or an unparsable file (mirrors every other cmd_* handler's
  // error reporting) -- the `midi-source load <path>` L1 verb this backs.
  bool load_midi_source(const std::string& path, std::string& error);

  // Phase-5 infrastructure (by-ear-validation gap, no committed SMF WRITER
  // before this): writes every kMidi OutEvent observed since Shell
  // construction to `path` as a Standard MIDI File (single track, division =
  // arrangrr::kPpqn -- tick values ride straight through, no rescaling), so
  // a real session's musical output (Restyle/Motif/...) can be audited in any
  // DAW/synth. The capture is a plain unbounded vector (not the MidiMonitor's
  // bounded ring, which drops old events) -- fine at the scale a live session
  // produces; documented, not a hot/realtime path. Returns false with `error`
  // set when `path` cannot be opened for writing -- the `export-smf <path>`
  // L1 verb this backs.
  bool export_smf(const std::string& path, std::string& error);

  // Direct ABI-Command entry point (Phase 2b in-process ring, docs/design/
  // sonotron-server-phase2-brief.md "Thread-boundary mechanism"): a thin
  // adapter over Engine::push_command for a caller that already holds a POD
  // Command (e.g. drained from the GUI->engine ring) instead of an L1 text
  // line -- the exact same core entry point every exec_line handler already
  // calls (see apply_style_step()/configure_default_surfaces() for existing
  // internal call sites), so no dispatch logic is duplicated or reopened.
  void push_command(const Command& cmd) { m_engine.push_command(cmd, m_sink); }

  explicit Shell(EventSink sink);
  void set_port_hook(PortHook hook) { m_port_hook = std::move(hook); }
  void set_panel_hook(PanelHook hook) { m_panel_hook = std::move(hook); }
  void set_print_hook(PrintHook hook) { m_print_hook = std::move(hook); }
  void set_width_provider(WidthProvider provider) { m_width_provider = std::move(provider); }
  void set_height_provider(HeightProvider provider) { m_height_provider = std::move(provider); }

  // Live-mode routing of output into the uniform panels. Events append to the
  // scrolling kEvents panel (no immediate repaint — the ~100ms grid refresh
  // flushes them); command echo / print output / errors append to the scrolling
  // kConsole panel and repaint at once.
  void log_event(const std::string& line);
  void console_output(const std::string& line);

  // Seeds the help panel with startup guidance (e.g. a demo motd) and opens it.
  void show_motd(const std::vector<std::string>& lines);

  // Re-renders width-dependent panels from state and re-pushes the composed
  // block — the terminal resize hook lands here (H1 resize contract).
  void refresh_panels();

  const PanelManager& panels() const { return m_panels; }
  const PianoViewState& piano_state() const { return m_piano; }
  const MidiMonitor& monitor() const { return m_monitor; }
  const UiStyle& ui_style() const { return m_style; }

  // Test/inspection seam: how many notes the harmony (chords) surface currently
  // holds. The harmony surface is output-SUPPRESSED (kHarmony), so it emits no
  // OutEvent the MidiMonitor could observe; this const accessor is the only way
  // a test can watch the held-set the way monitor().active_notes() watches the
  // sounding surfaces. Behaviour-preserving: returns existing state, no logic.
  std::size_t harmony_held_count() const { return m_harmony_held.size(); }

  // Live TUI wires the real terminal capabilities in (a fresh Shell assumes
  // no TTY / no UTF-8, so colors stay off for scripts and tests).
  void configure_terminal(bool is_tty, bool utf8);

  // Live TUI key dispatch (H2): consumes the byte when a panel has focus.
  // Returns false with REPL focus so typing stays exactly as before —
  // panel focus is entered only via `panel focus ...` commands. In piano
  // focus SPACE (0x20) toggles the key mode (momentary <-> toggle) and is
  // never a musical note; plain musical bytes always take the toggle path so
  // the piano is never dead on a terminal without key-release events.
  bool handle_ui_key(std::uint8_t byte);

  // SHIFT+TAB from the live loop: step the focus cycle backwards.
  void focus_prev();

  // True key press/release from the kitty keyboard protocol (H3): drives
  // momentary polyphony when the piano is focused. `pressed` = true is a
  // key-down (or autorepeat — idempotent, never a double note-on), false is a
  // key-up. In kToggle mode a press behaves like handle_ui_key and a release
  // is ignored. Returns true when the key mapped to a musical key and was
  // consumed; false lets the caller fall back to the normal byte path (so
  // non-musical kitty keys — TAB, SPACE, shortcuts — still work).
  bool piano_key_event(char key, bool pressed);

  PianoKeyMode piano_key_mode() const { return m_piano_key_mode; }

  // The live loop injects the current wall-clock (monotonic microseconds) before
  // it processes a batch of REPL input, so the toggle-mode auto-repeat debounce
  // (toggle_surface_key) can measure key cadence WITHOUT the Shell ever touching a
  // real clock — that keeps the debounce unit-testable with an injected time.
  // Left at 0 (the default) the debounce is inert, so tests that never inject a
  // time keep their exact toggle-on/toggle-off behaviour.
  void set_input_time_us(std::uint64_t us) { m_input_time_us = us; }

  // True if the byte maps to a musical piano key. The live loop uses this to
  // drop kitty keyboard-protocol autorepeat (kRepeat) events for musical keys,
  // so a physically-held key can never re-fire a note-on.
  bool piano_is_musical_key(std::uint8_t byte) const { return piano_binding_for(byte) != nullptr; }

  // The style/section chooser dissolved into the always-present styles panel:
  // it is ALWAYS constructed and always rendered. Interactions (digits, arrows,
  // -/=, _/+, ENTER, musical keys) only fire when the styles panel is FOCUSED;
  // styles_focused() is the gate main.cpp and handle_ui_key check.
  bool styles_focused() const;
  const StyleChooser& chooser() const { return m_chooser; }

  // The `parts` mixer: focused-panel gate (mirrors styles_focused) and its key
  // handling. Arrow up/down (from main.cpp) move the selected part; m/s toggle
  // its mute/solo. Voice is changed via the `program` command.
  bool parts_focused() const;
  void parts_select(int delta);
  bool parts_key(std::uint8_t byte);

  // The `groove` panel: focus gate + key handling. Arrow up/down move the
  // selected parameter; left/right adjust it (via kGroove commands).
  bool groove_focused() const;
  void groove_select(int delta);
  void groove_adjust(int delta);
  bool groove_key(std::uint8_t byte);

  // The `chords` panel is the HARMONY surface: focus gate + key handling. When
  // focused the SAME piano key bindings play here, but their notes route to the
  // harmony input port (zone kHarmony) — silent, observed by the ChordDetector,
  // so playing re-harmonizes the band. Reuses the piano octave/transpose/channel
  // state so a musical key means the same note on both surfaces (the piano's
  // view-only shortcuts N/V/C/Z are NOT shared — they stay in the piano branch).
  bool chords_focused() const;
  bool chords_key(std::uint8_t byte);

  // The ONE global chord-steering choke point (spec: steering works from EVERY
  // panel but the REPL). With a panel focused, a musical note-letter key steers
  // the band silently through the harmony surface (priority over the panel's own
  // letter shortcuts), and SPACE flips the harmony key mode. Returns true when it
  // consumed the byte; false lets handle_ui_key continue to the per-panel
  // handlers (non-note keys) or, with the REPL focused, to the line editor.
  bool try_global_steer(std::uint8_t byte);

  // Piano-panel-only shortcuts (variation stepping, N/V/C/Z view keys, and the
  // octave/transpose keys). Note-letters and SPACE were already consumed by the
  // global steer choke, so this only sees the non-note piano shortcuts.
  bool piano_panel_key(std::uint8_t byte);

  // Shared surface key handling (piano + chords route through the same code):
  // the SPACE key-mode toggle (labelled per surface) and the musical/octave/
  // transpose keys that both surfaces bind identically, differing only in which
  // port + held-set they drive.
  bool surface_key_mode_toggle(const char* surface, const char* momentary_action);
  bool surface_musical_key(std::uint8_t port, ActiveNoteTracker& held, std::uint8_t byte);

  // Default two-surface topology (live launch): pins the piano port to kMelody
  // (sounds, no steer), the harmony port to kHarmony (silent) and points the
  // chord detector at the harmony port with detection ON. Called once at startup
  // so a fresh launch already re-harmonizes from the chords panel.
  void configure_default_surfaces();

  // True when focus is on the command line (repl) rather than a panel — the host
  // highlights the input box in that state so "you can type here" is visible.
  bool repl_focused() const;

  // The `arp` panel: focus gate + key handling (arrow up/down select, left/right
  // adjust a parameter via kArp commands).
  bool arp_panel_focused() const;
  void arp_select(int delta);
  void arp_adjust(int delta);
  bool arp_panel_key(std::uint8_t byte);

  // Gives focus to the styles panel (backtick shortcut) and seeds the chooser
  // highlight from the arranger's current style/section.
  void focus_styles();

  // Keyboard stepping of the arranger's variation (section) and style with a
  // debounced auto-apply (host-live). The step keys mark a pending (style,
  // section) selection and bump a generation counter, but do NOT switch the
  // band immediately; main.cpp's live loop calls apply_style_step() ~500 ms
  // after the LAST step so pressing fast skips intermediate variations.
  std::uint32_t style_step_gen() const { return m_style_step_gen; }
  bool style_step_pending() const { return m_style_step_pending; }
  void apply_style_step();

  // Arrow/ESC drivers for the escape state machine in main.cpp (arrows arrive
  // as CSI escapes, not plain bytes). Each is a no-op unless the styles panel is
  // focused; chooser_cancel drops focus back to the REPL.
  void chooser_nav_style(int delta);
  void chooser_nav_section(int delta);
  void chooser_cancel();

  // Applies a ~/.arrangrr.rc configuration to the panels (order/heights/layout)
  // and logs any warnings to the console panel.
  void apply_rc(const RcConfig& rc);

  // Terminal-aware momentary lock (H3). The Shell itself stays agnostic — the
  // default is momentary-capable — and the live backend calls this after it has
  // probed the terminal. Passing false (no key-release support) downgrades any
  // current momentary mode to toggle and makes the SPACE mode-switch refuse to
  // go back to momentary, so the piano can never enter a mode the terminal
  // cannot honour.
  void set_momentary_available(bool available);
  bool momentary_available() const { return m_momentary_available; }

  Engine& engine() { return m_engine; }
  const Engine& engine() const { return m_engine; }
  const std::vector<PortDef>& ports() const { return m_ports; }

  // Public, pure name-resolution seam (Phase 2b integrated-mode gap, docs/
  // design/sonotron-server-phase2-brief.md): resolves a builtin style name /
  // a track-role name to the same index/enum `style load <name>` and
  // `part <role> ...` resolve to inside cmd_style()/cmd_part() via
  // exec_line(). Both are pure lookups over static builtin tables (no Shell
  // state involved), so they are exposed as static methods rather than
  // widening the Shell instance surface -- a caller that builds its own
  // Command POD without owning a Shell (in_process_brain_session's L1-text
  // translator) can reach them without hostrt's private shell_internal.hpp
  // becoming part of a public header. Style lookup is case-insensitive
  // (matches cmd_style's D26 resolution); role lookup is exact, lower-case
  // (matches parse_role). Returns -1 / false on an unknown name, exactly as
  // the exec_line path does.
  static int resolve_style_index(const std::string& name);
  static bool resolve_track_role(const std::string& name, TrackRole& out);

  // Executes one line (L2 sugar). Returns false on parse error; the error
  // message is passed to `error`. `@tick` lines are queued, not executed.
  bool exec_line(const std::string& line, std::string& error);

  // Advances stream time by n ticks, firing pending @tick lines on the way —
  // the live clock thread drives this (same path as the `advance` command).
  bool advance_by(std::uint64_t n, std::string& error) {
    return advance_to(m_engine.now() + n, error);
  }

  // True after a `quit` line.
  bool quit_requested() const { return m_quit; }

  // Enharmonic spelling for event rendering, derived from the current key
  // (flat-side keys print Bb, sharp-side keys print A#). §24.7 nit.
  bool prefer_flats() const { return m_prefer_flats; }

 private:
  bool exec_now(const std::vector<std::string>& tokens, std::string& error);
  bool advance_to(std::uint64_t target_tick, std::string& error);
  int find_port(const std::string& name, bool input) const;

  int find_track(const std::string& name) const;
  int find_seq(const std::string& name) const;
  std::vector<std::string> build_help(const std::string& topic) const;

  // exec_now dispatch is split into per-domain groups so each stays within the
  // cognitive-complexity budget; the groups preserve the original matching
  // order and guards, returning std::nullopt when the command is not theirs.
  std::optional<bool> dispatch_ui(const std::vector<std::string>& tokens, const std::string& cmd,
                                  std::string& error);
  std::optional<bool> dispatch_midi(const std::vector<std::string>& tokens, const std::string& cmd,
                                    std::string& error);
  std::optional<bool> dispatch_music(const std::vector<std::string>& tokens, const std::string& cmd,
                                     std::string& error);
  std::optional<bool> dispatch_transport(const std::vector<std::string>& tokens,
                                         const std::string& cmd, std::string& error);

  bool cmd_help(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_panel(const std::vector<std::string>& tokens, std::string& error);
  bool panel_layout(const std::vector<std::string>& tokens, std::string& error);
  bool panel_target(const std::string& sub, const std::vector<std::string>& tokens,
                    std::string& error);
  bool cmd_piano(const std::vector<std::string>& tokens, std::string& error);
  bool piano_octave(const std::vector<std::string>& tokens, std::string& error);
  bool piano_view(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_notes(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_filter(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_view(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_theme(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_colors(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_port(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_transport(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_bpm(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_route(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_thru(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_clock(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_midi_send(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_midi_source(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_export_smf(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_panic(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_key(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_note(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_play(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_chord(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_style(const std::vector<std::string>& tokens, std::string& error);
  // `restyle <style>` (roadmap 9320): load-time L1 verb selecting the target
  // style the RestyleStage transforms the imported melody's own notes into
  // (docs/design/restyle-placement.md §3). Mirrors `midi-source load`: no
  // ABI Command, reaches the Pipeline stage directly.
  bool cmd_restyle(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_seq(const std::vector<std::string>& tokens, std::string& error);
  bool seq_add(const std::vector<std::string>& tokens, std::string& error);
  bool seq_transpose(const std::vector<std::string>& tokens, std::string& error);
  bool seq_del(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_track(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_program(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_part(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_groove(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_arp(const std::vector<std::string>& tokens, std::string& error);
  // Phase-5 Item #2 (docs/design/clip-primitive-design.md): the clip launch
  // primitive's L1 grammar (shell_clip_commands.cpp). `clip add` is
  // host/script-only registration; `launch`/`stop` are the documented verbs
  // (ux-workstation.md §7/§8).
  bool cmd_clip(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_launch(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_stop_clip(const std::vector<std::string>& tokens, std::string& error);
  // Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
  // Performance"): the pad-bank + Performance recall L1 grammar
  // (shell_pad_commands.cpp). `pad assign` is host/script-only registration
  // (mirrors `clip add`'s own convention, see kPadAssign's abi.hpp comment);
  // `pad trigger`/`pad release` and `perf store`/`perf recall` ride the ABI;
  // `perf save`/`perf load` are HOST-ONLY file I/O (Architectural Principle
  // #2: the core never touches a filesystem) over performance.hpp's
  // serialize()/deserialize().
  bool cmd_pad(const std::vector<std::string>& tokens, std::string& error);
  bool cmd_perf(const std::vector<std::string>& tokens, std::string& error);
  bool perf_save(const std::string& path, std::string& error);
  bool perf_load(const std::string& path, std::string& error);
  // Phase-5 Item #10: `fx set|param|enable|clear` — the per-role MIDI-FX
  // insert chain (shell_fx_commands.cpp).
  bool cmd_fx(const std::vector<std::string>& tokens, std::string& error);
  bool track_new(const std::vector<std::string>& tokens, std::string& error);
  bool track_step(const std::vector<std::string>& tokens, int track, std::string& error);
  bool cmd_advance(const std::vector<std::string>& tokens, std::string& error);
  void open_help_topic(const std::string& topic);

  // Style/section chooser plumbing (host-live). The chooser is always present
  // and dissolves into the styles panel; seed_chooser_selection lands its
  // highlight on the arranger's current style/section; chooser_key routes a
  // consumed byte while the styles panel is focused; chooser_apply resolves the
  // selection into a kStyleSwitch.
  void seed_chooser_selection();
  bool chooser_key(std::uint8_t byte);
  void chooser_apply(ChooserApply mode);

  // Style/section keyboard stepping plumbing (host-live). style_step_key maps a
  // byte to an axis+direction; style_step drives the chooser highlight and
  // mirrors it into a debounced pending selection (clamped, no wrap).
  enum class StyleStepAxis { kSection, kStyle };
  bool style_step_key(std::uint8_t byte);
  void style_step(StyleStepAxis axis, int delta);

  void refresh_piano_content();
  void refresh_styles_content();
  void refresh_chords_content();  // live piano->chord readout (detect state + name)
  void refresh_parts_content();   // arranger style-parts mixer
  void refresh_groove_content();  // groove feel parameters
  void refresh_arp_content();     // live arpeggiator parameters

  int m_parts_selected = 0;   // highlighted row in the parts mixer
  int m_groove_selected = 0;  // highlighted row in the groove panel
  int m_arp_selected = 0;     // highlighted row in the arp panel
  UiMode current_ui_mode() const;
  std::vector<std::string> contextual_help_lines() const;
  void sync_contextual_panel();
  bool push_panels();
  void print_lines(const std::vector<std::string>& lines);
  void print_line(const std::string& line);
  int panel_columns() const;
  int panel_rows_available() const;
  // Note plumbing shared by the two playable surfaces (piano = melody, chords =
  // harmony). `port`/`held` select the surface; piano_midi_note (the shared
  // m_piano octave/transpose/channel state) makes a key mean the same note on
  // both. The toggle path is the plain-byte fallback on every terminal; the
  // momentary path fires from kitty key press/release.
  void toggle_surface_key(std::uint8_t port, ActiveNoteTracker& held, char key,
                          int semitone_from_base);
  void surface_momentary_on(std::uint8_t port, ActiveNoteTracker& held, char key,
                            int semitone_from_base);
  void surface_momentary_off(std::uint8_t port, ActiveNoteTracker& held, char key,
                             int semitone_from_base);
  void surface_all_notes_off(std::uint8_t port, ActiveNoteTracker& held);

  // Shared piano note plumbing (used by both surfaces' toggle and momentary paths).
  const PianoKeyBinding* piano_binding_for(std::uint8_t byte) const;
  bool piano_midi_note(int semitone_from_base, std::uint8_t& out) const;
  bool surface_note_held(const ActiveNoteTracker& held, std::uint8_t midi_note) const;
  void surface_send_note(std::uint8_t port, ActiveNoteTracker& held, char key,
                         std::uint8_t midi_note, bool note_on);

  // Phase-1 runtime extraction: Transport/OutScheduler moved out of Engine
  // into runtime::Runtime, which owns the ONE instance of each and injects
  // them by reference into the Engine (Stage-adapter) it constructs. m_engine
  // stays a reference bound to m_runtime.stage() so every OTHER existing
  // `m_engine.foo()` call site in this class (chords()/arranger()/arp()/
  // set_input_zone()/... — the Stage's own domain surface) keeps compiling
  // unchanged; only advance_ticks() moved to m_runtime (Engine no longer
  // exposes it — see runtime/stage.hpp's on_tick()/flush() split).
  //
  // Phase 4a (docs/design/phase4-execution-plan.md, orchestrator-pipeline-
  // extraction.md §16.3): Runtime drove a 1-stage `Pipeline<Engine>` instead
  // of a bare `Engine` -- one indirection level added on top of `.stage()`,
  // zero behaviour change (Pipeline degenerates to a transparent forwarding
  // wrapper).
  //
  // Phase 4d (§16.3/§16.4/§16.7/§16.9): the pipeline `hostrt::Shell` drives
  // grows to its real 3-stage Accompany shape,
  // `orchestrator::AccompanyPipeline<kSchedulerCapacity>` (`[MIDI-source,
  // chorddet, arrangrr]`) -- chorddet ordered BEFORE arrangrr so its
  // harmonic context is visible the SAME tick (D53); the MIDI-source stage
  // is constructed inert (nothing loaded) and stays byte-identical to not
  // being there at all until `midi-source load` runs (§16.7,
  // midisrc::MidiSourceStage's own guarantee) -- this is what lets the SAME
  // Shell drive both the plain interactive/18-golden default AND the new
  // Accompany golden category through one L1 grammar, one CLI binary.
  // `m_followed` (the shared FollowedContext, formerly an Engine-owned
  // value) is now owned HERE, at the orchestrator level (`hostrt::Shell`
  // plays that role for the pipeline it drives), injected by reference into
  // both peer stages. `m_engine` keeps its exact old spelling/binding
  // contract (now `.stage<kArrangrrStageIndex>()`) so every OTHER existing
  // `m_engine.foo()` call site in this class stays unchanged.
  //
  // Roadmap 9320 (Restyle), docs/design/restyle-placement.md §1: the pipeline
  // grew again to its real 4-stage shape, `[MIDI-source, chorddet, restyle,
  // arrangrr]` -- RestyleStage inserted between chorddet and arrangrr (the
  // forward-flow seam already reaches exactly this slot); it is constructed
  // inert (no style loaded) and stays byte-identical to not being there at
  // all until `restyle <style>` runs, the same convention MidiSourceStage
  // already established. `kArrangrrStageIndex` tracks the shift
  // automatically (named constant, orchestrator/accompany.hpp), so this
  // binding needed no change at all.
  FollowedContext m_followed{};
  runtime::Runtime<orchestrator::AccompanyPipeline<kSchedulerCapacity>, kSchedulerCapacity>
      m_runtime;
  Engine& m_engine = m_runtime.stage().template stage<orchestrator::kArrangrrStageIndex>();
  EventSink m_sink;
  PortHook m_port_hook;
  PanelHook m_panel_hook;
  PrintHook m_print_hook;
  WidthProvider m_width_provider;
  HeightProvider m_height_provider;
  PanelManager m_panels;
  PianoViewState m_piano;
  UiStyle m_style;
  UiMode m_ui_mode = UiMode::kRepl;  // last mode the contextual panel synced to
  bool m_help_pinned = false;        // true while an explicit help <topic> shows
  // Always-present style/section chooser (dissolved into the styles panel).
  StyleChooser m_chooser;
  bool m_styles_was_focused = false;  // edge-detect panel focus to seed the chooser
  bool m_await_panel_digit = false;   // a digit right after TAB jumps to panel #N
  // Debounced style/section keyboard stepping (host-live). The pending
  // selection is (style index, section); m_style_step_gen drives the debounce
  // clock in main.cpp, and m_style_step_pending marks a step not yet applied.
  int m_step_style_index = 0;
  SectionType m_step_section = SectionType::kVarA;
  std::uint32_t m_style_step_gen = 0;
  bool m_style_step_pending = false;
  MidiMonitor m_monitor;
  // Phase-5 infrastructure: unbounded capture of every kMidi OutEvent since
  // construction, fed by the same sink wrapper that feeds m_monitor (see the
  // Shell constructor) -- export_smf()'s source data.
  std::vector<arrstyle::SmfEvent> m_export_events;
  MidiEventFilter m_filter;
  MidiViewOptions m_view_options;
  ActiveNoteTracker m_piano_held;    // notes the melody (piano) surface holds
  ActiveNoteTracker m_harmony_held;  // notes the harmony (chords) surface holds
  PianoKeyMode m_piano_key_mode = PianoKeyMode::kMomentary;  // default: momentary
  bool m_momentary_available = true;  // cleared when the terminal has no key-release
  char m_pending_source_key = 0;      // annotates monitor events while feeding
  // Toggle-mode auto-repeat debounce (H3): a plain TTY delivers OS key
  // auto-repeat as a stream of identical bytes; without this each repeat would
  // flip the note on/off/on/off ("a manetta"). m_input_time_us is the injected
  // clock; m_toggle_last_us[note] is the last toggle attempt time for that MIDI
  // note (0 = never / clock un-injected -> debounce inert).
  std::uint64_t m_input_time_us = 0;
  std::array<std::uint64_t, 128> m_toggle_last_us{};
  std::vector<PortDef> m_ports;
  std::vector<std::string> m_tracks;  // name -> index (D26: names live host-side)
  std::vector<std::string> m_seqs;
  std::uint8_t m_next_in = 0, m_next_out = 0;
  bool m_prefer_flats = false;

  struct Pending {
    std::uint64_t tick;
    std::uint64_t order;  // stable FIFO among same-tick lines
    std::vector<std::string> tokens;
  };
  std::vector<Pending> m_pending;
  std::uint64_t m_pending_order = 0;
  bool m_quit = false;
};

}  // namespace arrangrr::host

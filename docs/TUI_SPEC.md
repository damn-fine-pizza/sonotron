# arrangrr — Host TUI: Piano / MIDI Monitor Specification

**Status:** living spec — updated as phases land; not a changelog, the source of truth for this feature.
**Feature classification:** host-live / host-tool (see §1 and the classification rule in DESIGN.md §0).
**Core impact:** none allowed. Any change that touches `core/` under this feature's name is a bug in the plan, not a fact to accommodate.

This document consolidates the roadmap for the "piano keyboard + MIDI monitor" panel of the host TUI (`app/platform/host/console.*`, `app/platform/host/shell.*`, and new files under `app/platform/host/`). It follows DESIGN.md's conventions: D-numbered decisions are locked facts from the parent design; this document adds its own numbered sections but does not mint new D-numbers — where it depends on a DESIGN.md decision, it says so explicitly (e.g. "per D26").

---

## 1. Architecture constraints (non-negotiable)

These constraints are inherited directly from DESIGN.md §2 (Architectural principles) and §0/D26 (binary core ABI, host-only strings). They are restated here because this feature is the one most tempted to violate them — a piano widget and a MIDI log feel like they belong "close to the engine," and they do not.

1. **The embedded core stays UI-free.** No terminal rendering, no ASCII art, no ANSI escape codes, no REPL strings, no panel/window management, no piano-key drawing, no host log buffers, no filter predicates, no monitor views, no color themes, no unicode/ASCII fallback logic. If a symbol needed by this feature would have to live in `core/` to work, the feature is designed wrong — not the core.
2. **All TUI code lives in `app/platform/host/`.** This includes the panel manager, the piano renderer, the MIDI monitor formatter/filter pipeline, key-dispatch, resize handling, and the color/theme/unicode layers deferred to Phase 3. Nothing under this feature is `core/`-portable; nothing under this feature is `firmware/`-facing.
3. **The simulated piano is an input device, not a shortcut.** Pressing a computer key for the simulated piano produces MIDI bytes exactly as an external MIDI keyboard would, and those bytes are injected via `Shell::feed_midi(port, bytes)` on the same input port used by real hardware. The piano key handler never calls into the router, engine, or note tracker directly, and it never writes to ALSA on its own — it goes through the same `feed_midi` → `push_midi_in` → router → engine path as any other input. This guarantees the piano cannot become a second, divergent code path for "note came in."
4. **Output stability is sacred.** The default JSONL/golden output format is unaffected by any of this: enabling the piano panel, the monitor, filters, or colors must never change a single byte of `--format jsonl` output used by goldens, nor of script-mode (`--script`) output. `--clock virtual` and non-TTY behavior (piping stdout, redirected input) are unchanged by definition — this whole feature only exists when attached to a real terminal with a live human at the keys.
5. **Every host buffer is bounded.** No `std::vector` growing without a cap anywhere in this feature: the active-note tracker, the visual event ring, the log ring, the panel manager's per-panel line store — all have a fixed capacity, declared as a named constant, with an explicit "what happens when full" policy (reject+warn, ring overwrite, or truncate — never silently grow, never crash).
6. **Feature classification rule.** Every unit of work in arrangrr is classified as exactly one of: **core-portable** (compiles for STM32, lives in `core/`), **host-live** (Linux-only, live/realtime UI or IO, `app/platform/host/`), **host-tool** (Linux-only, offline/dev tooling — benchmarks, editors, style compiler), **firmware-hal** (STM32-only adapters). This entire piano/MIDI-monitor feature is **host-live + host-tool**: the interactive panels and key dispatch are host-live; the STM32 impact reports and benchmarks (Phase 3) are host-tool. It never contributes a core-portable or firmware-hal line.

---

## 2. Phasing

The feature ships in three phases. Phase 1 is in progress; Phase 2 is the MVP that makes the panel musically useful; Phase 3 is explicitly deferred polish and tooling. Nothing in a later phase may be pulled forward without updating this document first — the phase boundaries exist precisely so Phase 1 doesn't quietly grow into Phase 2's scope.

### Phase 1 — Foundation (in progress)

Scope:

- **Note-name utilities** (CDE and DoReMi spellings, `C4 = 60`, `prefer_flats` toggle) extracted from `jsonl.cpp` into a shared, reusable location. `jsonl.cpp` currently owns `pc_name()`/`note_name()` (sharp/flat pitch-class tables, MIDI-number → name) purely for JSONL/human event formatting; the piano and monitor need the same mapping, so it moves to shared host code rather than being duplicated or reached into from two directions.
- **Small multi-panel manager**: a fixed set of panel identifiers (`help`, `piano`, `filter` for now — more arrive in later phases), each with visibility state and focus state, laid out vertically (stacked, not side-by-side — side-by-side is Phase 3), each with a per-panel line cap, and defined oversized-content behavior (truncate, never silently drop the newest or the header). Non-TTY / script mode is untouched: the panel manager only exists when attached to a live terminal.
- **The canonical `panel ...` command family** (see §3) — this is the *only* place panel lifecycle (open/close/toggle/focus) lives. No panel gets its own bespoke on/off verb.
- **A real help panel**: `help <topic>` now does two things atomically — it updates the help panel's content to the requested topic, and it opens the panel. `help open` / `help close` are explicitly **removed** — they never existed as a shipped command, but early drafts of this roadmap assumed them; this spec forbids them going forward. Panel lifecycle (open/close/toggle) is exclusively `panel open help` / `panel close help` / `panel toggle help`. `help <topic>` is content-plus-open; nothing else.
- **A static, pure piano renderer**: draws the keyboard layout described in §4 with no input handling, no MIDI awareness, and no active-note highlighting. It is a pure function of (terminal width, note-name mode) → lines. This exists so §4's layout logic can be tested and reviewed in isolation before Phase 2 wires live state into it.
- **Deterministic tests** for the above: note-name utility unit tests, panel-manager visibility/focus/layout tests, static-renderer golden-line tests for each width tier (§4).
- **An STM32 impact report** (§7 format) accompanying the Phase 1 changeset, even though Phase 1 touches no core code — the report exists to make "zero core impact" a checked fact, not an assumption.

Explicitly **OUT of Phase 1** (do not implement, do not partially implement, do not leave stubs that look load-bearing):

- Live key dispatch (pressing a computer key to sound a note).
- MIDI generation from the piano.
- Monitor buffers of any kind (log ring, event ring).
- Active-note tracking or highlighting.
- The visual event buffer.
- Filters beyond a placeholder panel `id` existing in the panel manager (no filter logic, no filter data model).
- Colors, themes, unicode rendering or fallback logic.
- Side-by-side panel layout.
- Benchmarks or stress tools.
- Any change to `ScheduledEvent` or other core layout/size decisions.
- Any refactor of `core/` for this feature's convenience.

### Phase 2 — Piano/monitor MVP

This is the phase where the piano becomes playable and the monitor becomes real.

**Piano commands** (see §3 for full grammar): `piano octave <N>|up|down` clamped to **-1..9**; `piano channel <1..16>` (user-facing, 1-based; stored internally 0-based per the core's channel convention); `piano velocity <1..127>`; `piano keymap` (report/reset the fixed keymap of §4); `piano panic` (all-notes-off for piano-originated notes only, routed through the normal panic path); `piano view keyboard|active-notes|event-log` (which sub-view the piano panel currently shows).

**Note-name mode**: `notes names cde|doremi|toggle` — switches the shared note-name utility (from Phase 1) between CDE and DoReMi spelling, or toggles between them. This affects every rendered note name across the piano and the monitor simultaneously — one setting, one source of truth, never two independently-drifting spellings.

**Live TUI key dispatch layer**, inserted **before** the LineEditor in the input pipeline, active **only** when the piano panel has focus. This ordering matters: when the piano is focused, raw key presses are piano-first — a keystroke that maps to a musical key or a piano shortcut never reaches the line editor's text-entry logic. When focus is elsewhere (REPL/help/filter), keys behave exactly as today. Piano shortcuts, all active only with piano focus:

| Key | Action |
|---|---|
| `TAB` | Focus next panel |
| `P` | Close piano panel / return focus to REPL |
| `N` | Toggle note-name mode (CDE ↔ DoReMi) |
| `V` | Cycle piano view (keyboard → active-notes → event-log → keyboard) |
| `C` | Clear buffers (active-notes + event-log, not the panel itself) |
| `[` | Octave down |
| `]` | Octave up |

`Z`, `D`, `F` are optional-if-trivial extension shortcuts (reserved for Phase 3's side-by-side toggle, drum-kit shortcut, and filter-focus shortcut respectively) — implement only if they fall out naturally from the dispatch table; do not force them into Phase 2.

**MIDI generation**: pressing a mapped musical key (§4's keymap) calls `Shell::feed_midi(port, bytes)` on the shell's normal, currently-selected input port — the same port real hardware would use. Note-off policy is **toggle**, not press/release, because terminals do not deliver key-release events: the first press of a key sends Note On; pressing the *same* key again sends Note Off for that note. This is documented in the `help piano` topic so it's never a surprise mid-performance. Two consequences worth stating explicitly: (a) a stuck note is recovered with `piano panic`, not by "releasing" a key that was never tracked as held; (b) re-pressing a different key for the same pitch at a different octave is a distinct note, tracked independently.

**Momentary key mode (H3, kitty keyboard protocol)**: Phase 2's toggle is the *fallback*, not the ceiling. A raw/canonical-off TTY delivers a byte on key **press only** — there is no key-release event in a standard terminal, so honest press/release polyphony is impossible with plain reads and must never be faked. The one legitimate mechanism is the **kitty keyboard protocol** (progressive enhancement, CSI u): terminals that support it (kitty, foot, ghostty, wezterm, recent xterm) emit distinct escape sequences for key press, autorepeat and release once the right flags are pushed. Two modes exist, held in a shell-side `PianoKeyMode { kMomentary, kToggle }`:

- **`kMomentary` (default)** — a note sounds while its key is physically held and stops on release. Driven by `Shell::piano_key_event(char key, bool pressed)`, fed from parsed kitty key events (`\x1b[<code>;<mods>:<event>u`, event 1=press 2=repeat 3=release). Press → note-on if not already sounding; autorepeat → ignored (already sounding, no double note-on); release → note-off. Polyphonic and correct because the terminal reports each key's own release.
- **`kToggle`** — the Phase 2 behaviour, unchanged: press = note-on, same key again = note-off.

**SPACE** (`0x20`) in piano focus toggles between the two modes and logs the new mode; it is a mode switch only, never a musical key. The flags pushed are `0x1` (disambiguate) `| 0x2` (report event types) `| 0x8` (report all keys as escape codes) = **11**, enabled with `CSI > 11 u` and popped with `CSI < 1 u`; the flag constants and the parser live in `app/platform/host/kitty_keys.{hpp,cpp}`. Enabling is **scoped to piano focus** and gated behind `isatty`: pushing the flags globally would reroute every REPL keystroke (arrows, history, editing) through CSI-u escapes, so the flags are pushed only while the piano is focused and popped the instant focus leaves (and again on shutdown). **Graceful degradation is mandatory**: on a terminal without the protocol no kitty events ever arrive, so the plain-byte path still runs through the toggle logic and the piano is never dead — kitty events (when they arrive) drive true press/release, plain bytes keep the toggle fallback, and both coexist across terminal types.

**Range enforcement**: a keymap+octave combination that would produce a MIDI note number outside 0..127 is **rejected** with a clear message (e.g. "note out of MIDI range, adjust octave") — it is never silently clamped or wrapped into range. Wrapping would sound a wrong note while looking like it worked; rejection is the honest failure mode.

**`ActiveNoteTracker`** (host-only, distinct from the core's Note/Voice Tracker in DESIGN.md §3): bounded capacity of **128** concurrently-tracked notes (matching MIDI's own note-number space, one slot per possible pitch is the simplest bound that can never be legitimately exceeded by a single channel, and is generous even across channels for a monitor view). When full, a new note is **rejected with a warning** rather than evicting an old entry — an eviction would make the active-notes view lie about what's actually sounding.

**`PianoVisualEventBuffer`** (host-only): a bounded ring of **32** entries, of which at most **5** rows are ever rendered in the event-log sub-view at once. No animation, no frame timer, no FPS concept — the buffer is repainted on state change only, consistent with §5's "regenerate, don't patch" contract.

**Duration formatting**: at the project's canonical PPQN of 960 (DESIGN.md D27), tick durations map to musical note values exactly where the arithmetic is exact, and approximately (marked with an ASCII `~` prefix) otherwise:

| Ticks | Value | Exact? |
|---|---|---|
| 120 | 1/32 | exact |
| 240 | 1/16 | exact |
| 480 | 1/8 | exact |
| 960 | 1/4 | exact |
| anything else | nearest value, prefixed `~` | approximate |

The formatter never fabricates false precision (e.g. printing "1/16" for a duration that is close-but-not-960/4) — approximate durations are visibly approximate.

**`MidiLogEvent`** (minimal monitor model): `{ tick, same_tick_index, port, msg }` — `same_tick_index` disambiguates multiple events landing on the identical tick (consistent with DESIGN.md D29's total-order tie-breaking by sequence number; the monitor surfaces that same ordering rather than inventing its own). The monitor pipeline is strictly staged: **`MidiLogEvent` → filter → formatter → renderer**. Each stage is replaceable and testable independently; the renderer never re-derives filtering or formatting decisions — it draws exactly what the formatter handed it. The guiding rule for this whole model: **honestly report what is observed.** No inferred state, no "probably a chord" guesses, no smoothing over dropped or out-of-order bytes — if the input was ambiguous or partial, the log says so rather than presenting a clean fiction.

**Minimal filters** (data, not code — see the style rule in §6): by channel, by port, by event kind (`note-on` | `note-off`), and `clear`. **View options** (also data): `show note-names on|off`, `show note-numbers on|off`, `show velocity on|off`, `show channel on|off`, `show port on|off`, `show-octaves boundary|all|none`, `clear`. Filters and view options are always represented as plain data structures consumed by the formatter/renderer — **never** hardcoded `if` branches inside a renderer. This is what makes Phase 3's richer filters (note lists, ranges, drum/melodic split, velocity thresholds) additive rather than a rewrite.

### Phase 3 — Deferred

Everything below is explicitly **out of scope** until Phase 2 has shipped and been used. Listing it now prevents scope creep from re-litigating "should this be in Phase 2" mid-implementation.

- **Colors**: `--colors` / `--no-colors` at startup, `colors on|off|toggle` at runtime.
- **Themes**: `--theme <name>` at startup, `theme list|set|current` at runtime; styles expressed as **semantic `UiRole`** values (e.g. `UiRole::ActiveNote`, `UiRole::PanelBorder`) resolved to concrete styling by the active theme — renderers never touch raw ANSI codes directly (this generalizes the style rule in §6 to color). Minimum shipped theme set: `default`, `mono`, `high-contrast`.
- **Unicode**: `--unicode` / `--no-unicode` at startup plus a runtime toggle; ASCII fallback is **always** available and is the default when terminal capability is unknown — unicode is opt-in polish, never a requirement.
- **Side-by-side layout**: the `Z` shortcut toggles piano and monitor panels between stacked (current, Phase 1/2 default) and side-by-side; side-by-side automatically falls back to vertical stacking when the terminal is too narrow to hold both (see §5's resize contract).
- **GM drum-name toggle**: recognizes channel 10 (General MIDI convention) and swaps note numbers for GM percussion names when enabled.
- **Richer views**: a `channels` view summarizing per-channel activity.
- **Richer filters**: explicit note lists, note ranges, drums-vs-melodic split, velocity thresholds (`velocity >= N`).
- **Bar-position display** in the human-readable log, formatted as `@tick#sameTickIndex` — deliberately never rendered as a fake decimal bar:beat position, because a decimal would imply precision the tick/index pair doesn't actually carry.
- **Benchmarks/stress tools** (host-tool, not host-live): fixed scenarios — transport only; 16 tracks × 64 steps; arranger active; a 128-step chord sequence; dense same-tick load; heavy retrigger; clock fanned out across 4 ports. Measured metrics: ticks/second, events/second, scheduler high-water mark, count of `SchedulerFull` warnings, per-port MIDI bytes/second measured against the DIN-5 physical ceiling of 3125 B/s.
- **Human MIDI log enrichment**: note names and GM drum names (channel 10, when GM naming is enabled) folded into the human-format log lines.

---

## 3. Command grammar (canonical, no aliases)

All commands below are **L2 surface** syntax (DESIGN.md D23/§28) — verb-first, space-separated, no dotted paths. There are no aliases for any command in this section: one name, one meaning, so `--echo-expand` and scripts stay unambiguous. Where a namespace has both content commands and lifecycle commands (`help`), the split is explicit — see §3.5.

### 3.1 `panel` — lifecycle for every panel, uniformly

```
panel list                      # panel ids + open/closed + focused
panel open <panel>               # open (or no-op if already open)
panel close <panel>              # close (or no-op if already closed)
panel toggle <panel>             # open<->close
panel close all                  # close every open panel
panel focus <panel>              # give a specific open panel input focus
panel focus repl                 # return focus to the REPL/LineEditor
panel focus next                 # cycle focus forward through open panels
panel status                     # one-line summary: which are open, which is focused
panel help                       # panel-command help text (not a panel-content command)
```

`<panel>` is one of the registered panel ids: `help`, `piano`, `filter` (Phase 1 set; Phase 2 adds no new panel ids, only content to `piano`). Every current and future panel obeys exactly this grammar — a panel never invents its own open/close verb (see the forbidden-duplicates list in §3.6).

### 3.2 `piano` — the simulated piano (Phase 2)

```
piano octave <N>                 # absolute, clamped to -1..9
piano octave up                  # +1, clamped
piano octave down                # -1, clamped
piano channel <1..16>            # user-facing 1-based; stored 0-based internally
piano velocity <1..127>
piano keymap                     # print the fixed keymap (see §4); no runtime remapping in Phase 2
piano panic                      # all-notes-off for piano-originated notes
piano view keyboard               # show the static/live keyboard sub-view
piano view active-notes           # show the active-notes sub-view
piano view event-log              # show the recent-events sub-view
```

`piano` commands never implicitly open the piano panel — that is `panel open piano`'s job (§3.6 forbids `piano on`/`piano show` as an alternate spelling of that).

### 3.3 `notes` — shared note-name mode

```
notes names cde                  # C, C#, D, ... spelling everywhere
notes names doremi                # Do, Do#, Re, ... spelling everywhere
notes names toggle                 # flip between the two
```

### 3.4 `view` — rendering options for the monitor (data, not renderer branches; §2 Phase 2, §6)

```
view show note-names on|off
view show note-numbers on|off
view show velocity on|off
view show channel on|off
view show port on|off
view show-octaves boundary|all|none
view clear
```

### 3.5 `filter` — monitor filtering (data; Phase 2 minimal set, Phase 3 extends)

```
filter channel <1..16>|all
filter port <name>|all
filter event note-on|note-off|all
filter clear
```

### 3.6 `help` — the one namespace that mixes content and lifecycle, on purpose

```
help                              # opens help panel with the default/overview topic
help <topic>                     # updates help panel content to <topic> AND opens it
```

`help` is a deliberate, narrow exception to "commands don't duplicate `panel`'s lifecycle verbs": because a bare `help <topic>` is such a common, low-friction thing to type, it is allowed to both set content and open the panel in one shot. It does **not** get its own close verb — closing help is `panel close help` like any other panel, and there is no `help close`. There is likewise no `help open <topic>` — `help <topic>` already opens it.

### 3.7 Forbidden duplicates

The following spellings are explicitly rejected — if a command handler for any of these exists in the tree, it is a bug against this spec and should be removed in favor of the canonical form on its right:

| Forbidden | Canonical form |
|---|---|
| `piano on` | `panel open piano` |
| `piano off` | `panel close piano` |
| `piano show` | `panel open piano` |
| `piano hide` | `panel close piano` |
| `help open` | `help <topic>` (opens as a side effect) |
| `help close` | `panel close help` |
| `help open <topic>` | `help <topic>` |
| `<any-panel> toggle` (bespoke, panel-specific) | `panel toggle <panel>` |

### 3.8 `chord detect` — live piano→chord harmonizer surface (DESIGN.md D34)

This is the one command in this document whose *engine* is **not** host-only: the recognition logic is a **core-portable** `ChordDetector` + `ChordEngine::set_context` (DESIGN.md D34 / §11), so it is exempt from §1's "core impact: none allowed" rule — that rule governs the piano/monitor TUI, not this feature. What lives here is only the **host-live command and panel surface** over that core detector; the detection itself is specified in DESIGN.md, not here.

**Command grammar** (L2 surface, no aliases, same rules as §3):

```
chord detect on                  # enable: notes held on the input port re-harmonize the running band
chord detect off                 # disable live detection
```

**What it does, from the player's point of view.** With `chord detect on` and the arranger playing, holding a chord on the simulated piano (or any external keyboard on the input port) makes the whole band follow that chord — bass and comping re-harmonize in real time, like a pro arranger keyboard. The keys you hold still sound through normal routing; detection only *steers* the arranger, so there is no doubled voicing. A chord is recognized once **≥3 notes** are held (lowest note = root, notes above complete the quality).

**Chord-memory (hold-last).** Releasing the keys does **not** stop the band or clear the chord: dropping below 3 held notes leaves the **last** recognized chord in place, and the arranger keeps playing on it until you hold a new chord. Lifting your hands is not a reset — a new chord replaces the old one, silence does not.

**`kChords` panel readout.** The existing `kChords` panel (previously a placeholder) now renders the **live-recognized chord name** (e.g. `C maj7`, `A min`) together with the **detect on/off** state; before the first chord is recognized it shows `(no chord)`. The panel is a pure readout of the engine's current harmonic context — it draws what was recognized, it does not itself recognize (consistent with §5's "state is the source of truth, renderers regenerate" rule).

**Scope (MVP) and future refinement.** For the MVP the **whole** simulated keyboard acts as chord input while `chord detect` is ON — a single toggle, no split. **Keyboard split / zones** (low range = chords, high range = melody) is a documented **future refinement**, deferred out of this pass (it is core-portable too; see DESIGN.md D34 / §11's "Split & layer").

---

## 4. Piano rendering spec (two-row keys)

The piano is rendered as **two rows of key cells**: a computer-keyboard binding centered directly above the musical note label it triggers. This two-row shape is the one constant across all width tiers (§4.4) — only cell width, spacing, and black-key placement scale down.

### 4.1 Note-label spacing rule

- **White-key labels are SPACED**: `C 4` (CDE mode) or `Do 4` (DoReMi mode) — a literal space between the letter/syllable and the octave digit. This spacing exists purely for visual column alignment against black-key labels in the two-row layout; it is a *piano-rendering-only* convention.
- **Black-key labels are NOT spaced**: `C#4` (CDE) or `Do#4` (DoReMi) — sharp marker glued to the octave digit.
- **Event/log note names are always unspaced**, regardless of piano-panel spacing: `C4`, `Do#4`. The monitor and JSONL/human formatters never adopt the piano's spacing convention — one note-name utility (Phase 1), two independent presentation rules layered on top of it (piano-key labels vs. everything else).

### 4.2 Octave display

Every visible key shows its octave, in both the wide and compact renderers, by default (`show-octaves = all`). For the **piano renderer specifically**, the `boundary` view-option value is treated as an alias for `all` — piano keys are small and dense enough that "only show the octave at C" would be more confusing than helpful, so the distinction that matters for the *monitor* (§2 Phase 2 view options) collapses to one behavior on the keyboard. `show-octaves = none` hides the per-key octave digit in the key rows, but the panel header still states the current base octave — a player can always recover "what octave am I on" from the header even with per-key octaves off.

### 4.3 Keymap (fixed, Phase 2 — no runtime remapping)

Two rows of computer keys, one white, one black, laid out as a single continuous 18-semitone span from the current base octave:

| Row | Keys | Semitone offsets from base-octave `C` |
|---|---|---|
| White | `A S D F G H J K L ; '` | 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 |
| Black | `W E T Y U O` | 1, 3, 6, 8, 10, 13 |

`P` is **never** a musical key in any keymap — it is permanently reserved to close the piano panel / return focus to the REPL (§2's shortcut table). This is a hard exclusion, not a default that a future remap could override, because losing the panic-adjacent "get me out of here" key to a remap would be a live-performance hazard.

### 4.4 Width tiers

Three renderer tiers, selected purely by terminal column count (re-selected on every resize per §5), plus a hard floor below which no keyboard art is attempted.

**Wide** (`kWideMinColumns = 76` and above): black keys are drawn horizontally *between* their white neighbors, matching physical keyboard geometry:

```
        W         E               T         Y         U               O
       C#4       D#4             F#4       G#4       A#4             C#5

  A         S         D         F         G         H         J         K
 C 4       D 4       E 4       F 4       G 4       A 4       B 4       C 5
```

**Compact** (`kCompactMinColumns = 56` up to `kWideMinColumns - 1`): the same two-row idea, tighter cell widths and gaps — black keys still positioned between their white neighbors, just with less horizontal padding per cell.

**Minimal** (below `kCompactMinColumns`, down to a usable floor): keyboard geometry is abandoned in favor of grouped plain text, one line per row:

```
black: W/C#4 E/D#4 T/F#4 Y/G#4 U/A#4 O/C#5
white: A/C 4 S/D 4 D/E 4 F/F 4 G/G 4 H/A 4 J/B 4 K/C 5
```

**Below minimal** (terminal too narrow even for the grouped-text form): a single honest fallback message line (e.g. "piano: widen terminal to view keyboard") — **never** a broken, wrapped, or clipped rendering of the wide/compact art. A half-drawn keyboard is worse than no keyboard.

### 4.5 Implementation shape

The renderer is a small pipeline, not a monolithic draw function: **binding → label → row composer → lines**. Concretely: (1) the fixed keymap (§4.3) maps a computer key to a semitone offset; (2) the shared note-name utility (Phase 1) plus `format_keyboard_note_label()` turn a MIDI note number into a spaced or unspaced label depending on white/black; (3) a row composer lays out a row of cells at the current tier's width/gap constants; (4) the result is plain lines handed to the panel manager.

All tier thresholds and geometry constants live in a `piano_layout::` namespace as named `constexpr`s — at minimum `kWideMinColumns = 76`, `kCompactMinColumns = 56`, plus cell-width and gap constants for each tier. No bare numeric literal for a column threshold, cell width, or gap is acceptable in the row-composer code; if a new constant is needed, it is named and added to `piano_layout::`, not inlined (see §6).

---

## 5. Resize robustness contract

**State is the source of truth; rendered lines are regenerated on every geometry change, never patched.** This is the single governing rule of this section — every other statement below is a consequence of it. Patching previously-rendered lines in place (shifting characters, splicing in new columns) is exactly the kind of incremental-update bug class this rule exists to rule out: it is far easier to prove "the renderer is a pure function of state + geometry" than to prove "every incremental patch path stays consistent."

**Resize flow:**

1. Resize is detected (SIGWINCH or equivalent polling, host-only).
2. `TerminalGeometry` (rows × columns) is refreshed from the terminal.
3. `ConsoleLayout` is recomputed from the new geometry — named rows: log-top, log-bottom, panel-top, panel-bottom, status, input. These are named fields, not offsets computed ad hoc at each call site.
4. Every currently-visible panel is **re-rendered from its retained state** (not from its previous rendered lines) using the new `ConsoleLayout`.
5. Output is truncated to the newly available area per panel (per-panel line cap from Phase 1's panel manager, §2).
6. The terminal scroll region is updated to match the new layout.
7. A full repaint is issued.

**Invariants that must hold across any resize, maximize, or restore:**

- The wide/compact/minimal renderer tier (§4.4) is **re-selected** every time — a maximize event that crosses a tier threshold must flip the renderer, not keep the old tier stretched or squeezed.
- Resize **never resets** piano state (current octave, channel, velocity, view mode), panel state (which panels are open, which is focused), or filter state.
- Resize **never clears** the active-note tracker or the visual event buffer (Phase 2) — a window resize is not a `piano panic` or a `view clear`.
- Resize has **no effect whatsoever** in script/non-TTY mode — there is no terminal geometry to react to, and the code path must not be reachable there at all.
- The status bar and the input line are **always visible**, at every terminal size this feature supports — they are the last thing to be sacrificed, never the first.
- Terminals too small for any panel content degrade gracefully to the "below minimal" fallback line (§4.4) rather than emitting garbled partial art.
- All of the above is host-only logic; none of it has any bearing on, or dependency from, `core/`.

---

## 6. Style rules (enforced)

These rules apply to all **new** code written for this feature (Phase 1 onward). They are enforced the same way the rest of the host layer is: `clang-tidy` for the member-naming and brace rules, code review for the rest.

- **No magic numbers.** Every bare numeric literal that means something (a column threshold, a buffer capacity, a semitone offset, a tick duration) is a named `constexpr` inside a small, purpose-named namespace — `piano_layout::`, `ansi::` (Phase 3), `midi::`, `layout::`. A literal `76` appearing inline in row-composer code is a defect, not a style nit.
- **No raw ANSI literals in implementation code.** Escape sequences are confined to dedicated, named helpers (and, from Phase 3 onward, resolved through the `UiRole` → theme indirection in §2's Phase 3 section) — never spelled out as string literals at the call site.
- **Protocol fragments live only in dedicated helpers.** Anything that looks like it's constructing part of the L0/L1 wire format, or part of a MIDI byte sequence, belongs in one clearly-named function others call — not inlined at each use.
- **Non-trivial bodies belong in `.cpp` files, not headers.** Constructors/destructors with real work, RAII types, anything with observable side effects — declared in the header, defined in the `.cpp`. Headers stay declarations plus genuinely trivial inline accessors.
- **Blank lines separate logical phases** within a function — the same visual-grouping discipline used elsewhere in the host layer.
- **Comments explain policy and intent, never decode a literal.** A comment next to `kWideMinColumns = 76` should say *why* 76 (the layout it enables), never "76 columns" (that's what the name already says).
- **Members use the `m_` prefix, no trailing underscore; braces are always present**, even for one-line bodies — enforced by the existing `clang-tidy`/`clang-format` configuration for the whole project (DESIGN.md's C++ style policy applies unchanged here).

**Note on `console.cpp`:** the existing `app/platform/host/console.cpp` predates these rules and contains raw ANSI literals inline. That file is **not** retrofitted as part of this feature — grandfathering it in wholesale would either block this spec on an unrelated cleanup or quietly relax the rule for new code too. Retrofitting `console.cpp` is a separately filed task. These rules bind all **new** code written under this specification, full stop.

---

## 7. STM32 impact discipline + measured baseline (2026-07-03, HEAD 4f46bbe)

Every change made under this specification ships with an impact table covering: files touched in `core/` (should read "none" for this entire feature), new core RAM, new core flash, new host-only RAM, ARM build status (must stay green per DESIGN.md D3), JSONL output (must be byte-for-byte unchanged for existing goldens), scheduler behavior (must be unaffected), and MIDI throughput (must be unaffected — the piano rides the same input path as external hardware, per §1.3, so it cannot introduce new throughput behavior by construction).

### 7.1 Measured baseline

Captured at HEAD `4f46bbe`, before this feature's Phase 2 work begins, so later impact tables have a fixed point to diff against:

| Quantity | Value |
|---|---|
| `firmware_stub.elf` `.text` | 45,731 B |
| `firmware_stub.elf` `.data` | 97,352 B |
| `firmware_stub.elf` `.bss` | 440 B |
| `sizeof(Engine)` | 97,496 B |
| `OutScheduler<4096>` | 65,552 B |
| `sizeof(ScheduledEvent)` | 16 B |
| `ChordSequencer` | 24,864 B |
| `Timeline` | 4,200 B |
| `Track` | 262 B |
| `NoteTracker` | 2,560 B |
| `Router` | 168 B |
| `Command` | 20 B |
| `OutEvent` | 16 B |
| `kMaxTracks` | 16 (current software limit) |
| `kSchedulerCapacity` | 4096 |

### 7.2 Two open core findings (report-only here — fixes are separate core tasks, not part of this feature)

1. **The engine-gate static `Engine` lands in `.data`** (≈97 KB copied from flash at boot), because its members have non-zero defaults requiring initialization at load time rather than zero-fill. The eventual fix is to give the engine zero defaults plus explicit runtime initialization, so its large pools live in `.bss` (which the loader zero-fills cheaply) instead of `.data` (which it must copy). This is a core change and is out of scope for this document beyond flagging it.
2. **`sizeof(ScheduledEvent) == 16 B`, versus the 12 B budget in DESIGN.md's D33 pool table** — at `kSchedulerCapacity = 4096`, that's 64 KB actually consumed against a 48 KB line item. Two honest resolutions exist: update DESIGN.md's budget to reflect 16 B, or shrink the sequence-number field to `u16` to get back under 12 B. **Packing the struct (`[[gnu::packed]]` or equivalent) is explicitly not an acceptable shortcut here without a dedicated Cortex-M7 alignment/access-cost discussion** — unaligned access on M7 has real performance and, in some configurations, correctness implications, and this spec will not paper over that with an attribute.

### 7.3 Saturation ranking (keep this honest when adding features)

When reasoning about "is this fast enough," reach for these limits in this order — each one saturates before the next becomes the bottleneck:

1. **Per-port DIN-5 bandwidth**, 3125 B/s — a 4-note chord (note-on ×4 at 3 bytes each, running status aside) is already about 3.8 ms on a single DIN port. This is the first ceiling in any live scenario with real hardware attached.
2. **Scheduler capacity** — every gated note occupies two scheduler slots (the note-on and its matching note-off), so a "16 tracks × dense chords" scenario burns capacity twice as fast as a naive note count suggests.
3. **`cancel_note_off` cost** — currently O(N) worst case against the scheduler's live entries. This should be benchmarked (Phase 3's benchmark scenarios, §2) before anyone proposes optimizing it — premature optimization here would spend effort against a cost that may never actually bind in practice.

---

## 8. Linux roles

Per DESIGN.md D2/D7, Linux is devenv-first, never the primary target — but Linux plays three genuinely distinct roles in this project, and this feature only exists in the third and, live, the second:

1. **Deterministic simulator** — goldens, `--script`, `--clock virtual`. No wall clock, no ALSA required. This feature has **zero footprint** here: none of the panel manager, piano renderer, or monitor exists in this role, and none of it may leak into it (§1.4).
2. **Host live runtime** — real product usage: ALSA MIDI, the router/arranger/performance tool actually being played. **This is where the TUI, and this entire feature, belongs.** The piano is a live input device standing in for external hardware; the monitor is a live diagnostic view onto real traffic.
3. **Authoring/tooling environment** — the style compiler, pattern editors, benchmarks, and other host-tool utilities that are never ported to STM32. Phase 3's benchmark suite (§2) lives here, alongside this document's own impact-report tooling (§7).

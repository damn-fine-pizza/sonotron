# Looper (node 6000) into gui-sonotron — as-built contract + gap analysis

Status: SCOPE DOC (read-only architecture analysis, Corelli). No product code
touched. This is a contract + gap map for Nazzareno to implement against, not
an implementation.

Scope trigger: owner-locked goal — "press-and-hold a Repeat-Zone cell to
record a loop into it." Input source: BOTH an on-screen keyboard/pad surface
in the GUI AND real hardware MIDI-in — Linux ALSA only for this pass; the
platform-MIDI abstraction (a future CoreMIDI backend) is explicitly deferred
to a later macOS workstream and is flagged where it slots in (§3, §7 item 6).

---

## 0. Where this sits on the roadmap

`docs/DESIGN.md:866-898` records node `6000` (the Looper) as **◑ partial,
Phase 7 SLICE 1 shipped** — `6100` (record/overdub/replace/erase/undo),
`6200` (quantize-after), `6300` (retroactive capture), `6400` (loop length)
are all **✅ done**; only `6500`'s SYNC half (bar-aligned overdub-while-
playing) stays unbuilt, explicitly noted as deferred in `loop_buffer.hpp:
146-152`. This is core-engine-complete work, sitting squarely inside the
"behind the freeze line" era (`11700`/`11730`, `docs/DESIGN.md:1203-1238`) —
the GUI (`11600`) already exists and is the living instrument every remaining
musical feature (including `6000`) is meant to be grown onto, exactly as this
task requires.

**Dual-target note stated once, up front**: `11600`/`11700` are HOST-ONLY by
construction (`docs/DESIGN.md:1235-1238`) — nothing in this document proposes
device (`12000`/STM32) UI work; the STM32 target keeps its own separate
physical UI (`12400`), never this GUI.

---

## 1. As-built inventory — the core Looper

**`LoopBuffer`** (`components/core/arrangrr/include/arrangrr/loop/
loop_buffer.hpp`) is a bounded pool of `kMaxLoopSlots` slots (host: 16 ×
512 events; device: 8 × 3072 events — target-conditional, `config.hpp:
138-164`, budget pinned by a `static_assert` against the STM32H743 512 KB
envelope). Recording is a **single active target** (one performer, one live
stream, mirrors `ChordSequencer`); playback is **per-slot** (several loops
sound simultaneously, driven by `Engine::fire_loop` iterating every
`kPlaying` `kLoopBuffer` clip, `engine.cpp:1301-1327`). ABI:
`kLoopNew/kLoopRecordStart/kLoopRecordStop/kLoopErase/kLoopUndo/kLoopLength`
= Param `59-64` (`abi.hpp:330-354`), `kRetroCaptureArm/Disarm/Grab` = Param
`69-71` (`abi.hpp:391-409`). Every verb is frozen-ABI, additive-tested
(`test_abi_frozen.cpp`).

**`RetroCaptureRing`** (`components/core/arrangrr/include/arrangrr/loop/
retro_capture.hpp`) is a SEPARATE always-on-while-armed ring tapping the
**same live-note tee** `LoopBuffer::note_on/note_off` taps in
`Engine::push_midi_in` (its own header states this explicitly, `:11-14`:
*"STREAM = LIVE INPUT... NOT the arranger's generated output"*). `grab()`
materializes a window of the ring into an EXISTING `LoopBuffer` slot
(registered via the existing `kLoopNew` — retroactive capture never
registers its own slots, `retro_capture.hpp` header comment).

**`ClipMatrix` already treats a loop clip as a first-class citizen, at the
type level, with zero new engine surface needed for coexistence.**
`ContentKind::kLoopBuffer = 3` (`clip_matrix.hpp:31-41`) sits in the SAME
4-value enum as `kStyleSection`/`kChordSequence`/`kStepTrack`, dispatched by
`Engine::apply_clip_content`'s `switch` (`engine.cpp:1206-1247`) exactly like
every other kind — `case ContentKind::kLoopBuffer: apply_clip_content_loop_
buffer(...)` (`:1237-1238`), which calls `LoopBuffer::start_playback`/
`stop_playback` (`:1259-1284`). A `Clip` is one uniform 12-byte POD
(`{part_role, scene_index, kind, content_index, state, n_bars,
due_bar_index}`, `clip_matrix.hpp:56-82`) regardless of which `ContentKind`
it holds — a grid cell that today holds a `kStyleSection` reference and one
that would hold a `kLoopBuffer` reference (a `LoopBuffer` slot id) are
structurally identical registrations in the same pool. **This part of the
task's question — "how do the two ContentKinds coexist in the same grid" —
is already answered, and answered cleanly: they always have, by design,
since Phase 7 SLICE 1.** The gap is entirely on the GUI/host side (§4).

---

## 2. As-built inventory — the note-ingestion crux

**`Engine::push_midi_in`** (`engine.hpp:182-232`) takes `(std::uint8_t port,
Span<const std::uint8_t> bytes, EventSink sink)` — raw MIDI bytes, parsed
per-port by `m_parsers[port]`. Inside the parsed-message callback it runs a
tee, in this exact order: arp capture → harmony suppress → **Loop tee**
(`if (is_note_message(msg) && m_loop.recording() && port == m_loop.
recording_port()) { observe_loop_input(port, msg); }`, `:206-212`) → **Retro
tee** (`m_retro.armed() && port == m_retro.port()`, `:214-219`) → normal
routing. Both loop paths are pure passive taps on whatever already reaches
`push_midi_in` — **there is no other way into either capture path**. The
question that decides this whole contract is therefore: what reaches
`push_midi_in` today, and from where?

**Answer for hardware (ALSA), traced precisely: NOTHING reaches it in
gui-sonotron.** `apps/gui-sonotron/src/in_process_brain_session.cpp`'s
`Impl::run_engine()` (`:572-675`) opens an `AlsaMidi` (`alsa.open(...)`,
`:575`), opens `in0`/`out0` ports and issues `shell.exec_line("thru in0
out0", ignored)` (`:611-621`) — but `Shell::cmd_thru` (`shell_io_commands.
cpp:430-445`) is **not** an ALSA-level MIDI thru: it is sugar for a core-side
`Param::kRouteAdd` route-table entry (`c.param = Param::kRouteAdd`) that only
has any effect once bytes for port `in0` actually reach `push_midi_in`. The
loop's own body (`:630-674`) drains exactly two things — `command_ring`
(→ `shell.push_command`) and `path_command_ring` — plus a tick clock; **it
never calls `AlsaMidi::drain_input`**, confirmed by an exhaustive search:
`drain_input` appears in exactly four places in the whole tree —
`alsa_midi.hpp`'s own declaration, `apps/sonotron-server/main.cpp:324-326`
(`alsa.drain_input([&](...){ shell.feed_midi(port, ...); })`, poll()-driven),
`apps/tools/cli-arrangrr/main.cpp`, and `components/platform/hostrt/tests/
test_host.cpp` — **never** in `apps/gui-sonotron/`. Physical MIDI hardware
plugged into gui-sonotron's `in0` ALSA port today produces literally zero
effect: the port is opened, a route is staged, and no byte is ever read off
the wire. This is the mirror image of the in-process backend's own header
comment (`in_process_brain_session.cpp:26-28`: *"MIDI-in hardware is
explicitly deferred (Phase 2b scope note): this backend only ever plays what
the Command ring drives"*) — confirmed, not assumed.

**Answer for an on-screen surface: the wire shape ALREADY EXISTS in the
frozen ABI, and is already implemented and tested — but only at the L1
text-verb layer, never on the Command-ring path gui-sonotron's engine thread
drains.** `Param::kNoteRaw = 43` (`abi.hpp:177-189`) is documented, in the
ABI header itself, as *"a pure client's equivalent of what
surface_send_note() already builds in-process. The host translates this
DIRECTLY into the same 3-byte MIDI note-on/off message and feeds it through
feed_midi(); it never reaches Engine::push_command()."* Its wire shape —
`idx = port | (channel_0based << 8)`, `a = note`, `b = velocity`, `c = 1
on/0 off` — fits the existing `Command` POD (`idx`/`a`/`b`/`c` fields,
`sizeof(Command) <= 20`) with **zero new fields and zero ABI change
required**. It is consumed exactly once in the whole tree:
`Shell::cmd_note` (`components/platform/hostrt/shell_music_commands.cpp:
190-236`), the `note <port>[:ch] on|off <midinote> [velocity]` L1 verb,
which builds a `Command{Param::kNoteRaw,...}` purely as an intermediate
value, then calls a local helper `note_raw_to_bytes()` (`:181-186`) and
`feed_midi()` directly (`:232-234`) — **it never pushes this `Command`
anywhere**; `Param::kNoteRaw` has NO case in `Engine::push_command`'s own
dispatch switch (confirmed: it appears nowhere in `engine.cpp`), so pushing
one through the Command ring today would land on the unhandled-param default
and emit `WarnCode::kUnknownCommand` (`engine.cpp:120/731`). `cmd_note` is
reachable only via `Shell::exec_line("note ...")`, run on whichever thread
owns the `Shell` — for `cli-arrangrr`/`sonotron-server`'s own REPL/socket
loop this is the same thread that already calls `exec_line`; for
gui-sonotron's integrated backend, `exec_line` only ever runs **inside**
`run_engine()` on the engine thread (`shell.exec_line("port open in in0",
...)` etc., `:619-621`) — the GUI thread never calls it, and
`InProcessBrainSession::send()`'s own translator, `command_line_to_command()`
(`:296-508`), has no branch for the `note` verb at all (its whitelist is
`transport`/`panic`/`style load|switch|section`/`transpose`/`bpm`/`pad
bank`/`part ... mute|solo`/`clip add ... style ... id`/`launch clip|scene`/
`stop clip` — thirteen shapes, none of them `note`).

**Net finding, stated precisely (this corrects the task brief's framing):
the Command ring does NOT need a new variant to carry a MIDI note — it
already can, via `Param::kNoteRaw`, unused since it was designed** (its own
comment cites `docs/design/orchestrator-pipeline-extraction.md §17.3a`).
What is missing is exactly two small, additive, host-only wiring steps,
neither of which touches the ABI:
1. `command_line_to_command()` gains a `note <port>[:ch] on|off <midinote>
   [velocity]` branch (mirrors `cmd_note`'s own grammar/packing exactly) —
   or, better for the on-screen keyboard's actual call shape (no text
   parsing needed at the call site), a small typed builder function next to
   `command_line_to_command` that gui-sonotron's own note-input code calls
   directly with `(port, channel, note, velocity, on)` and gets back a ready
   `Command{Param::kNoteRaw,...}`.
2. `run_engine()`'s Command-ring drain loop (`:636-638`, currently
   `while (command_ring.try_pop(cmd)) { shell.push_command(cmd); }`) needs a
   special case for `cmd.param == Param::kNoteRaw`: translate to 3 raw bytes
   (`note_raw_to_bytes`'s own logic, today `private` to `shell_music_
   commands.cpp`'s anonymous namespace — needs hoisting to a shared, testable
   spot, e.g. a small free function `arrangrr::host::note_raw_to_bytes` or a
   new `Shell::push_note_raw(const Command&)` public method built the same
   way `Shell::feed_midi`/`Shell::push_command` already are) and call
   `shell.feed_midi(port, bytes)` **instead of** `shell.push_command(cmd)`
   for that one Param value.

Both are SHIPPABLE, host-only, zero new dependency, zero ABI change — the
verb, the packing, and the byte-translation logic are already written and
already have a passing test (`test_note_verb`, `components/platform/hostrt/
tests/test_host.cpp:382-409`); the gap is a missing dispatch branch in
exactly one file gui-sonotron owns.

**Asymmetry worth flagging plainly**: `--control <path>` mode
(`UdsBrainSession`, the pure external-client path, `apps/gui-sonotron/
main.cpp:10-15`) is **NOT** affected by this gap. `apps/sonotron-server/
main.cpp:79/237` runs `shell.exec_line(line, error)` for every line received
over the control socket — so a GUI (or any client) already connected via
`--control` to a real `sonotron-server` could send the literal text `note
in0 on 60 100` today and it would work end to end, unmodified, because
`sonotron-server`'s own Shell already implements `cmd_note`. The gap
identified above is specific to gui-sonotron's DEFAULT (`InProcessBrainSession`,
no `--control`) backend, which is a text-translator + Command-ring pair, not
a full `exec_line` pass-through.

**The future platform-MIDI abstraction slot, named precisely.** `AlsaMidi`
(`components/platform/hostrt/alsa_midi.hpp`) is a concrete, ALSA-specific
class — its own header states *"Host-only, one backend for M0 (review
decision: ALSA first, others behind the same HAL later)"* (`:14-15`) — but
**no HAL interface exists yet**: `cli-arrangrr`, `sonotron-server`, and
`gui-sonotron` all name `arrangrr::host::AlsaMidi` directly, not an abstract
type. A future CoreMIDI backend is not "swap a file behind an existing
seam" — it requires FIRST extracting an interface (e.g. `IMidiHal` with
`open/create_port/send/drain_input/poll_fd_count`) that `AlsaMidi` then
implements, a genuine new abstraction, not merely a new implementation of
one that already exists. **This is out of scope for the present, Linux-only
pass** (per the task's own framing) but is the exact seam a macOS workstream
will need to open; flagged here so it is not silently assumed free later.

---

## 3. As-built inventory — the hostrt L1 verb surface (the second crux)

**The Looper has ZERO host-verb surface anywhere in the tree.** An
exhaustive search across `components/platform/hostrt/` (excluding tests)
for every Looper/retro-capture ABI symbol —
`kLoopNew`/`kLoopRecordStart`/`kLoopRecordStop`/`kLoopErase`/`kLoopUndo`/
`kLoopLength` and `kRetroCaptureArm`/`Disarm`/`Grab` — returns **zero
matches** outside the test harnesses. Contrast this with the Repeat-Zone's
own clip primitive, which DOES have a full L1 verb
(`shell_clip_commands.cpp`'s `clip add <role> <scene> style|seq|track
<selector> [id <n>]`, cited at length in `docs/proposals/repeat-zone-real-
contract.md §1`) that both the CLI and (via the GUI's own translator) the
integrated backend can reach. **The Looper has no equivalent at all**: not
in the TUI's `Shell`, not in `cli-arrangrr`, not in `sonotron-server`'s
control-socket grammar, not in gui-sonotron's `command_line_to_command()`.
It is reachable ONLY by constructing a raw `Command` directly, which today
only the test harnesses do (`test_loop.cpp`, `test_loop_ops.cpp`,
`test_retro_capture.cpp`, etc., via `TestEngine::push_command`).

This means bringing the Looper to gui-sonotron is not, like the Repeat-Zone
was, "wire an existing CLI verb into a GUI panel." It requires **first**
building a host verb family for `kLoopNew`/`kLoopRecordStart`/
`kLoopRecordStop`/`kLoopErase`/`kLoopUndo`/`kLoopLength` (and, as a bonus,
`kRetroCaptureArm`/`Disarm`/`Grab`) — mirroring `shell_clip_commands.cpp`'s
own shape exactly (a new `shell_loop_commands.cpp`, parallel structure) —
**before** gui-sonotron's own `command_line_to_command()` translator has
anything real to translate `into`. This is small, additive, no-ABI-change
work (every verb is already a frozen, tested Param), but it is a genuine
missing layer, not a detail — it belongs early in the build plan (§9).

**The on-screen-keyboard precedent already exists, but is TUI-only.**
`components/platform/hostrt/shell_input.cpp`'s `Shell::surface_send_note`
(`:41-59`) is the exact gesture this task needs, already built and battle-
tested for the live TUI's own simulated piano: *"Every surface note goes
through the same feed_midi -> push_midi_in path real hardware uses
(docs/TUI_SPEC.md §1.3): the piano is an input device, never a shortcut into
the engine"* (`:43-45`). `toggle_surface_key`/`surface_momentary_on/off`/
`piano_key_event` (`:61-161, 763-796`) are the toggle/momentary key-state
machines around it. **This code lives in `arrangrr::host::Shell`, the SAME
class `in_process_brain_session.cpp` instantiates and drives** (`Shell shell
(...)`, `in_process_brain_session.cpp:587`) — but it is invoked only from
the TUI's own raw-terminal-byte dispatch (`handle_ui_key`, called from
`cli-arrangrr`'s/`sonotron-server`'s own input loop), never from
gui-sonotron's Command-ring path. The plumbing to turn "a key/pad press" into
a `feed_midi` call already exists in the class gui-sonotron already links;
what is missing is a way for the GUI THREAD (which cannot call `Shell`
methods directly — only the engine thread owns `shell`, per
`in_process_brain_session.cpp:566-571`'s own ownership-discipline comment)
to ask the engine thread to do so. §2's `kNoteRaw` wiring is exactly that
ask-mechanism; `surface_send_note`'s own TUI-side logic (velocity, channel,
port selection) is not reusable as-is (it is tied to `ActiveNoteTracker`/
`PianoKeyMode`, TUI-specific state) but is the right reference shape for a
GUI-side note-input helper to imitate.

---

## 4. ClipMatrix / Repeat-Zone coexistence — the GUI-side half

Per §1, the core already treats a loop clip as a first-class `ContentKind`.
The GUI side has NOT caught up, in three separate, precisely located spots:

**`GridCellKind` has no loop value.** `apps/gui-sonotron/src/grid_model.hpp:
29`: `enum class GridCellKind : std::uint8_t { kEmpty, kStyleSection,
kChordSequence, kStepTrack };` — four values, mirroring the core's
`ContentKind` MINUS `kLoopBuffer = 3` (`clip_matrix.hpp:31-41`, which HAS
four values including the loop kind). A recorded loop cell has nowhere to
live in `GridModel`'s own type today; this enum needs a fifth value before
any cell can display "this is a loop, not a style section."

**`preview_for` only ever resolves `Style`/`Section` content — it has zero
concept of a `LoopBuffer` slot.** `apps/gui-sonotron/src/preview.cpp:30-105`
(`preview::preview_for(int style_index, Section section, std::size_t
role_index)`) walks a built-in `Style`'s `StyleSection::patterns` and
resolves a `StylePattern` against a placeholder harmony (`preview.cpp:
54-56`) — it never touches `LoopBuffer`/`LoopClip`/`LoopEvent` at all. A
loop cell's preview (the recorded note pattern, per-slot) would need an
ENTIRELY SEPARATE data path: reading `LoopBuffer::get(slot_id)`'s
`LoopClip` events (note/tone/octave/velocity/start/duration, `loop_event.hpp`)
and resolving them the same way `Engine::fire_loop_clip` does at playback
time (chord/key-relative resolution, `loop_buffer.hpp:322-391`'s own
`resolve_note`) — not a small tweak to `preview_for`, a second preview
function with a different core-reading shape, living in the SAME narrowly-
scoped `gui_sonotron_preview` library (`apps/gui-sonotron/CMakeLists.txt`'s
own comment: *"the ONLY two libraries that [include arrangrr/ headers
directly]"*) — the layering discipline that library already exists FOR is
exactly this kind of addition; it does not need a new core-touching library,
just a new function in the existing one.

**The Looper's own readback event is not decoded — worse, it is actively
MISDECODED as a fabricated warning.** `OutEvent::Kind::kLoop`
(`LoopEventKind::kRecordStarted/kRecordStopped/kErased/kUndone/kGrabbed`,
`abi.hpp:429-442`) has NO case in `brain_event_from_outevent.cpp`'s switch
(`:20-126`) — it falls to the `default:` branch (`:121-125`), shared with
`OutEvent::Kind::kWarn`, which does `out.kind = BrainEvent::Kind::kWarn;
out.warn_code = host::warn_name(ev.code);`. But for a `kLoop` event,
`ev.code` is the target **slot id** (per `abi.hpp:437-441`'s own comment:
*"kGrabbed... reuses the EXISTING kLoop OutEvent (code = target slot id)"*),
not a `WarnCode` enumerant at all — so today, the moment ANY loop record
starts/stops/erases/undoes/is-grabbed (e.g. by a script, `cli-arrangrr`, or a
future GUI verb), gui-sonotron's `AppState` would decode it as a spurious,
meaningless "warning" line with a garbage warn-code label. This is a small,
concrete defect, cited here because it is structurally load-bearing: it
shows the `kClip`-readback precedent (`repeat-zone-real-contract.md §1`,
already wired) was NOT extended to `kLoop` when node `6000` shipped — the
same "push-`OutEvent`, decode into `BrainEvent`" shape needs its own,
dedicated `case OutEvent::Kind::kLoop:` branch before any GUI panel can show
real record/overdub/erase/undo state.

**Launch/auto-song semantics carry over unchanged, with one asterisk.**
`apply_clip_content`'s `kLoopBuffer` branch (`engine.cpp:1259-1284`) starts/
stops per-slot playback exactly like every other `ContentKind`
`ClipMatrix::arm/force/on_bar` already promote through the SAME bar-boundary
gate (`clip_matrix.hpp:44-46`) — so `launch clip <id>`/`stop clip <id>` and
column-launch (`kSceneQuantize`) work on a loop clip with **zero new engine
logic**, identical to a style-section cell. The one musical asterisk (not a
wiring gap, a genuine feature limitation already documented in the core):
`loop_buffer.hpp:146-152`'s own comment — overdub-while-playing is NOT
bar-synchronized yet (6500's SYNC half, deferred) — so launching a loop cell
mid-song is correct, but *recording into an already-playing loop* is a
"SLICE-1 simplification," not full node 6000. Not this contract's job to
fix; flagged so the build plan does not silently promise more than the core
delivers.

---

## 5. The on-screen keyboard/pad surface — a new visual, not in the v02 design

**The v02 DesignSync design has no keyboard/pad component.** The design kit
(`LaunchCell`/`LaunchGrid`/the controls kit, per the v02 memory ground-truth)
defines the Repeat-Zone grid, transport, browser, parts mixer, and Sequence
Edit — it has no on-screen piano, pad bank, or note-input surface of any
kind. Building one for this workstream is, like auto-song before it, a
**deliberate addition outside the locked visual design**, not a gap in
reading the design correctly. This needs owner/design sign-off on the
visual itself (layout, key range, velocity/octave controls, pad-vs-keyboard
choice) before implementation — this contract only establishes the WIRE
surface it would drive (§2's `kNoteRaw` path), not its pixels.

Two structurally distinct shapes, both reachable through the exact same
`kNoteRaw` wire path once §2 lands, genuinely a product decision:
- **A small on-screen PIANO strip** (mirrors the TUI's own
  `PianoView::kKeyboard`, `shell_input.cpp:214-227`, conceptually — not a
  code reuse, ImGui vs. terminal rendering are unrelated) — octave range,
  velocity fixed or a slider, one "row" of keys.
- **A pad bank** (mirrors `PadEngine`'s own `PadType::kDrum`/one-shot
  triggers, `pad/pad_bank.hpp`, conceptually) — fixed note-per-pad, simpler
  gesture (press = on, release = off), no pitch range to lay out.

**NEEDS-OWNER-DECISION**, not resolvable from the tree: piano strip vs. pad
bank vs. both; this is a visual/UX call, not an architectural one, but it
gates what `render_*_panel` function actually gets built.

---

## 6. The press-and-hold-cell record gesture

**No existing ImGui precedent for a hold-to-record gesture exists in the
grid today — the closest analogue is the double-click-to-rename state
machine, useful as a REFERENCE SHAPE, not reusable code.**
`grid_panel.cpp`'s `render_scene_header_cell` (`:283-...`) already carries a
small explicit state machine for a multi-frame gesture: `fx.renaming_scene`
(which cell, if any, is mid-rename), `fx.rename_focus_pending` (one-frame
edge for `SetKeyboardFocusHere`), commit-on-Enter/focus-loss, cancel-on-
Escape. A press-and-hold-to-record gesture needs the analogous shape:
`ImGui::IsItemActive()` + `ImGui::IsMouseDown(ImGuiMouseButton_Left)` sampled
across frames to detect "held", a debounce/minimum-hold-duration so a plain
click does not accidentally arm a recording, `ImGui::IsItemDeactivated()` (or
a raw mouse-up check) to detect release, and V02-state fields
(`fx.recording_cell`, mirroring `fx.renaming_scene`'s own `int, -1 = none`
shape) to track which cell is currently being held.

**The verb sequence to fire, per §1/§3's ABI (once the host-verb layer of §3
exists)**:
- Hold-start (crosses the hold-duration threshold): if the cell is empty,
  `kLoopNew` first (register a slot), THEN `kLoopRecordStart` (mode=record,
  port = whichever input surface is live — the on-screen keyboard's own
  port, or an ALSA hardware port once §2's ALSA wiring lands) at the cell's
  own stable id (mirroring the Repeat-Zone's `cell_id(role,scene)`
  convention, `repeat-zone-real-contract.md §3 Shape A` — the SAME
  explicit-id argument `kClipAdd` already gained works identically for
  `kLoopNew`... **except it doesn't yet**: `kLoopNew`'s own comment
  (`abi.hpp:330-333`) documents the SAME implicit-sequential-id convention
  `kClipAdd` had BEFORE the Repeat-Zone binding contract added an explicit-id
  argument — `kLoopNew` has not received the equivalent treatment. This is a
  genuine small NEEDS-NEW-ENGINE-VERB item, structurally identical to the one
  the Repeat-Zone contract already solved and shipped for `kClipAdd`).
- Hold-release: `kLoopRecordStop` (quantize grid = 0/default one bar, or a
  UI-exposed grid choice).
- The cell then needs registering into `ClipMatrix` as a `kLoopBuffer` clip
  referencing the new slot id (the SAME `clip add ... id <n>` shape the
  Repeat-Zone contract built for `kStyleSection`, extended with a `loop`
  selector — another small, additive grammar addition, not a new mechanism).

**Which loop verbs to expose, sized against the task's own ask**:
record/overdub/erase/undo/length are the TOP-10 core set (all shipped,
§1); retro-capture arm/grab is the explicitly-named bonus. A minimal v1
GUI surface needs: record (the hold gesture itself), stop (release), erase
(a per-cell context action), undo (one global "undo last loop edit" button —
`LoopBuffer::undo`'s own single-shared-shadow-generation discipline,
`loop_buffer.hpp:264-281`, means only ONE undo exists at a time, tree-wide,
regardless of how many cells the GUI shows — a UI affordance for "undo"
must reflect this bounded, non-per-cell truth honestly, not imply a full
undo stack). Overdub/replace-mode selection and retro-capture arm/grab are
reasonable v2 additions, not required for the owner's stated MVP goal
("press-and-hold... to record a loop").

---

## 7. New wire surface required — enumerated

| # | Surface | Kind | Why | Label |
|---|---|---|---|---|
| 1 | `command_line_to_command()`/a typed builder translates a note gesture into the EXISTING `Param::kNoteRaw` Command | Host-only, ZERO ABI change (verb already frozen) | §2 crux | SHIPPABLE now |
| 2 | `run_engine()`'s Command-ring drain special-cases `Param::kNoteRaw` → `feed_midi()` instead of `push_command()` (hoist `note_raw_to_bytes` to a shared, public spot) | Host-only, zero ABI | §2 crux — without this, item 1's Command lands on `kUnknownCommand` | SHIPPABLE now |
| 3 | `run_engine()` calls `alsa.drain_input(...)` → `shell.feed_midi(...)` every loop iteration, mirroring `sonotron-server`'s own pattern exactly | Host-only, zero ABI, zero new class (reuses `AlsaMidi::drain_input`, already written) | §2 — hardware MIDI-in is currently dead in gui-sonotron | SHIPPABLE now |
| 4 | A new `shell_loop_commands.cpp` (`loop new`, `loop record <slot> <mode> <port>`, `loop stop <slot> [grid]`, `loop erase <slot>`, `loop undo <slot>`, `loop length <slot> ...`) mirroring `shell_clip_commands.cpp`'s shape | Host-only, ZERO ABI (every Param already frozen) | §3 — the Looper has NO host verb anywhere today | SHIPPABLE now (small, mechanical) |
| 5 | `kLoopNew` gains an explicit-`id` argument (mirrors `kClipAdd`'s own Repeat-Zone fix exactly) | Small ABI-additive change (idx field repurposed, same pattern already shipped for `kClipAdd`) | §6 — the GUI's cell-stable-id convention needs it | NEEDS-NEW-ENGINE-VERB (small, precedented) |
| 6 | `clip add <role> <scene> loop <slot-id> id <n>` grammar addition (extends the EXISTING `clip add` verb with a `loop` selector, alongside `style`/`seq`/`track`) | Host-only, zero core ABI (the `ContentKind::kLoopBuffer` value already exists) | §1/§6 — registers a recorded loop into the SAME grid pool as a style-section cell | SHIPPABLE now, contingent on item 5 landing first (id-argument parity) |
| 7 | `GridCellKind` gains a `kLoopBuffer` value; `GridCell` gains whatever a loop cell needs to remember (its `LoopBuffer` slot id, at minimum) | Host-only, zero ABI | §4 | SHIPPABLE now |
| 8 | A SECOND preview function (loop-content preview, reading `LoopBuffer::get(slot)`'s `LoopClip`) living in `gui_sonotron_preview` alongside `preview_for` | Host-only, links `arrangrr` (already an allowed 2nd library per the CMake comment) | §4 | SHIPPABLE now (moderate size — needs the SAME chord/key-relative resolve `fire_loop_clip` does) |
| 9 | `brain_event_from_outevent.cpp` gains a dedicated `case OutEvent::Kind::kLoop:` (decode slot id + `LoopEventKind`) instead of falling to the misdecoding `default:` | Host-only, zero ABI | §4 | SHIPPABLE now, small, arguably a correctness FIX independent of everything else here |
| 10 | The on-screen keyboard/pad ImGui surface itself (a new panel) | Host-only, zero ABI (drives item 1/2's wire) | §5 | NEEDS-OWNER-DECISION (visual) then SHIPPABLE |
| 11 | The press-and-hold gesture state machine in `grid_panel.cpp` (mirrors the rename gesture's shape) | Host-only, zero ABI | §6 | SHIPPABLE now, contingent on items 4/5/6 |
| 12 | ALSA input as an abstract `IMidiHal` interface (`AlsaMidi` becomes ONE implementation), the seam a future CoreMIDI backend needs | Host-only, zero ABI, but a genuine new abstraction, not a swap | §2's flagged future slot | NEEDS-OWNER-DECISION / OUT OF SCOPE for this pass (explicitly deferred per the task brief) |

No item above requires a new host or core **dependency**. Item 5 is the only
genuinely new ABI surface, and it is small and directly precedented (the
Repeat-Zone contract already shipped the identical fix for `kClipAdd`).

---

## 8. Dual-target check

Every surface in §7 is either:
- **HOST-ONLY** (every GUI-side item: the note-gesture translator, the ALSA
  drain wiring, the new `shell_loop_commands.cpp` L1 verb file, `GridCellKind`/
  `GridCell` additions, the second preview function, the `kLoop` decode case,
  the keyboard/pad panel, the hold gesture) — none of this crosses into
  `components/core/`, no freestanding/no-heap concern.
- **Core-side, bounded POD, directly precedented** (item 5, `kLoopNew`'s
  explicit-id argument) — the exact same shape as `kClipAdd`'s own fix
  (`idx` field repurposed from "unused" to "explicit id, sentinel = legacy
  append"), already proven to cross-build `arm-none-eabi` in that form.

Nothing proposed here introduces a container, an allocation, a float, or an
`-fno-exceptions`/`-fno-rtti` violation on the realtime path. Item 12 (the
`IMidiHal` abstraction) is explicitly OUT OF SCOPE for this Linux-only pass,
per the task's own framing — flagged, not sized, not built here.

---

## 9. SHIPPABLE-now / NEEDS-NEW-ENGINE-VERB / NEEDS-OWNER-DECISION — the split

**SHIPPABLE now (no new ABI beyond one small precedented item, host-only
work):**
- Fix the ALSA input dead-end: wire `alsa.drain_input` into `run_engine()`'s
  loop (§7 item 3).
- Wire `Param::kNoteRaw` end to end for the integrated backend: the
  translator branch (item 1) + the drain-loop special case (item 2).
- Build `shell_loop_commands.cpp`, the Looper's first-ever host verb family
  (item 4) — a prerequisite for everything downstream, mechanical, mirrors
  `shell_clip_commands.cpp` closely.
- Extend `clip add` with a `loop` selector (item 6), contingent on item 5.
- `GridCellKind::kLoopBuffer` + whatever `GridCell` needs (item 7).
- The loop-content preview function (item 8).
- The `kLoop` OutEvent decode case (item 9) — arguably ships independently of
  everything else, as a correctness fix.
- The on-screen surface's ImGui implementation and the hold gesture (items
  10/11), once their prerequisite decisions/wiring land.

**NEEDS-NEW-ENGINE-VERB (small, additive, directly precedented):**
- `kLoopNew` explicit-`id` argument (item 5) — identical fix to the one
  already shipped for `kClipAdd`.

**NEEDS-OWNER-DECISION:**
1. **On-screen surface shape**: piano strip vs. pad bank vs. both (§5) —
   pure visual/UX call, blocks item 10's implementation, not its wiring.
2. **Loop verb scope for v1**: record/stop/erase/undo/length only, or also
   overdub/replace-mode selection and retro-capture arm/grab (§6) — sizes
   the host-verb file (item 4) and the GUI panel (item 11) together.
3. **The future platform-MIDI abstraction** (item 12): confirmed explicitly
   OUT OF SCOPE here per the task brief, but flagged so nobody assumes
   `AlsaMidi` is already behind a swappable seam — it is not.

---

## 10. Ordered build plan (for Nazzareno, once owner decisions land)

1. **Fix the ALSA dead-end + wire `kNoteRaw`** (§7 items 1/2/3) — zero ABI
   risk, ships independent of every other decision, and is the single
   biggest correctness gap in the tree today (hardware MIDI-in silently does
   nothing). Do this first: every other loop-input path (on-screen or
   hardware) depends on `push_midi_in` actually being reached.
2. **Build `shell_loop_commands.cpp`** (§7 item 4) — the Looper's first host
   verb family, mirroring `shell_clip_commands.cpp`. Ships independent of
   the GUI panel work; testable via the CLI alone (`cli-arrangrr`) before any
   ImGui code exists.
3. **`kLoopNew` explicit-id argument** (§7 item 5) — small, precedented ABI
   change; needed before item 4's `loop new` verb can address a
   GUI-chosen stable cell id.
4. **`clip add ... loop <slot> id <n>`** (§7 item 6) + **`GridCellKind::
   kLoopBuffer`** (§7 item 7) — the registration/coexistence half; makes a
   recorded loop a real grid cell for the first time.
5. **Loop-content preview + `kLoop` OutEvent decode fix** (§7 items 8/9) —
   readback and visual honesty; can ship in parallel with step 4.
6. **Owner resolves §9's decisions 1/2** — sizes the panel and the exposed
   verb set before real ImGui work starts.
7. **The on-screen keyboard/pad panel + the press-and-hold gesture** (§7
   items 10/11) — last, because every wire surface it needs (steps 1-5) must
   already be real, or the panel has nothing genuine to drive; building the
   visual first over a half-wired backend would repeat exactly the Repeat-
   Zone's own original "GridModel is empty, launch addresses nothing" defect
   this document's sibling contract diagnosed.

---

## What I flagged / dependency stance

No new host or core dependency anywhere in this workstream. The single
richest finding is that the hardest-sounding part of the task — "how does a
GUI note reach `push_midi_in`" — is **already solved at the ABI level**
(`Param::kNoteRaw`, frozen, tested) and simply never wired into gui-sonotron's
integrated backend; the remaining work is small, mechanical, host-only
plumbing, not new engine design. The genuinely new engine surface is limited
to one item (`kLoopNew`'s explicit-id argument), and it is a direct copy of a
fix already shipped for `kClipAdd`. The platform-MIDI abstraction
(`IMidiHal`) is named and flagged, not built or assumed — that decision, and
the visual shape of the on-screen surface, are the owner's calls, not mine.

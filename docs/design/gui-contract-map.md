# GUI ↔ Core Contract Map (node 11600, Phase 0 orientation)

Status: **orientation reference** for the host GUI client. Read alongside
`components/arrangrr/include/arrangrr/abi.hpp` (frozen v1 vocabulary, node 11720) and the shipped
UDS-JSONL adapter (node 11500, `components/hostrt/uds_server.*`, `jsonl.cpp`, `shell.cpp`).

The GUI is a **separate process, pure client**. It never links or `#include`s the core. It
speaks to the headless core **only** over the Unix-domain-socket control adapter. Everything
below is grounded in the current tree; `file:line` citations live in the Phase 0 agent notes.

---

## 0. The single most important layering fact

The v1 ABI that is *frozen and pinned by tests* is the **binary** `Op`/`Param`/`Command`/
`OutEvent` vocabulary — but that binary struct **never crosses the socket**. What crosses:

- **GUI → core (requests):** plain **text L1 command lines**, the exact REPL grammar
  (`chord play C`, `style load 3`, `groove swing 40`, `transport start`, `bpm 120`), one per
  `\n`-terminated line. `Shell::exec_line` tokenizes and dispatches. There is **no** wire-level
  `Op`/`Param`, no request id, no JSON envelope on the inbound side.
- **core → GUI (events):** canonical **JSONL** lines, one `OutEvent` each, rendered by
  `to_jsonl()` and **broadcast to every connected client** (best-effort; a slow client drops
  events rather than stalling MIDI).

> ⚠️ `docs/DESIGN.md §24` describes a JSON-envelope protocol (`{"op":"do","path":...,"id":5}`
> with `id`/`re` correlation and a `hello` handshake). **That was never shipped.** Build against
> the real code, not §24.

### Transport facts
- `AF_UNIX` / `SOCK_STREAM`, newline-delimited framing, `kMaxLineLength = 4096` per line.
- Path is **explicit**: launch the core with `arrangrr --control /path/to.sock` (no default; no
  `--control` ⇒ no socket). Headless/non-tty launch skips TUI/panels — the mode the GUI wants.
- Multi-client, no handshake, no per-client subscription. Connect = raw `connect()`.
- **Trap:** sending `quit`/`exit` over the socket **terminates the whole host process for every
  client**. Never wire window-close to a bare `quit`.
- Best client template to imitate: `components/hostrt/tests/test_host.cpp::test_uds_server_end_to_end()`.

### Integration shape — how the client drains this framing
*(extracted from gui-toolkit-decision.md, retired 2026-07-11 — the ImGui+GLFW choice was approved and vendored)*

- **Single binary, single thread, poll-in-frame.** The ImGui render loop (~60 fps, ~16 ms budget)
  drains the non-blocking UDS socket fd (`recv`/`poll`, `MSG_DONTBLOCK`, to `EAGAIN`) once per
  frame. **No background reader thread** — it would add synchronization, a second failure point, and
  a drop-policy to reinvent, to protect a guarantee the server does not offer anyway (its broadcast
  is already best-effort/lossy). Max added latency = one frame; zero risk of the GUI entering the
  timing path. This mirrors the server's own poll pattern in `main.cpp`.

---

## Pure-client boundary rules

*(extracted from gui-toolkit-decision.md, retired 2026-07-11 — the ImGui+GLFW choice was approved and vendored)*

Confirmed sound **on the wire** (only text L1 out / JSONL in cross the socket). The real trap is
**host-side code reuse**, and the graph confirms it:
- `jsonl.cpp` (which renders the JSONL) **is coupled to the core** — it includes `chord_engine.hpp`
  / `theory.hpp` / `transport.hpp` and lives in `arrangrr_host`, which links `arrangrr_core` PUBLIC.
- **Rule 1:** the GUI target links **neither `arrangrr_core` nor `arrangrr_host`**, and `#include`s
  **zero** core headers. Model its `CMakeLists.txt` on `apps/tools/arrstyle-converter` ("does not link
  arrangrr_core"), NOT on `arrangrr_host`.
- **Rule 2:** the GUI carries its **own** wire layer — a `LineBuffer`-style newline reassembly reader
  + a minimal JSON-line parser + its **own** name tables (section↔string, chord-quality↔suffix,
  warn↔string) written as plain strings. **Never** `static_cast<ChordQuality>` / `static_cast<
  SectionType>` a core enum. (`LineBuffer` is already dependency-free "by design" and *could* be
  extracted to a shared host-only header to avoid duplication — a file-placement call for Palladio;
  the extraction itself is safe. `arrstyle-converter/src/json.cpp` is an existing dependency-free
  JSON helper worth checking for reuse.)
- `note_names.{hpp,cpp}` is already a clean seam (takes only integers, no core includes) — reusable.

### Toolkit: decided & vendored
The desktop GUI toolkit is settled and already in the tree: **Dear ImGui vendored** (upstream
`ocornut/imgui` core `.cpp/.h`, never a distro package) driven by the **`imgui_impl_glfw` +
`imgui_impl_opengl3`** upstream backend pair, on **GLFW3** (window + GL context + input) rendering
through **system OpenGL**. GLFW was chosen over SDL2/SDL3 and hello_imgui for the narrowest footprint
(GLFW does only window/context/input, versus SDL's full multimedia layer this MIDI-only, socket-driven
app never touches) and native Wayland support on the Fedora dev platform. None of these are core-linking
concerns — they are pure window/render libs, isolated from the boundary rules above.

---

## 1. Commands the GUI SENDS (text line → resulting internal Op/Param)

Grouped by surface. Left column is the *text* the GUI writes; right is the frozen `Param` it
resolves to (for traceability against the freeze). Exact verb spellings live in
`components/hostrt/shell_music_commands.cpp` / `shell_io_commands.cpp`.

### Transport / clock
| Text line | Param | Notes |
|---|---|---|
| `transport start` | `kTransportStart` | rewind to 0, play |
| `transport stop` | `kTransportStop` | |
| `transport continue` | `kTransportContinue` | resume at tick (CTRL+P uses stop/continue) |
| `bpm <20..400>` | `kTransportTempo` | clamped 20–400 BPM |
| `panic` | `kPanic` | all-notes-off |
| `clock out <mask>` | `kClockOutMask` | which ports get F8/FA/FB/FC |

### Style / arranger / section
| Text line | Param | Notes |
|---|---|---|
| `style load <0..15>` | `kStyleLoad` | builtins: basic,pop,rock,ballad,funk,disco,house,swing,bossa,samba,reggae,country,blues,shuffle,latin,motown |
| `style section <type>` | `kStyleSection` | intro1/2, varA..D, fillA..D, break, ending1/2 (quantized to next bar while playing) |
| `style switch <style> <section> <immediate>` | `kStyleSwitch` | combined; the chooser's ENTER=next-bar / CTRL+\ = immediate |

### Chord / harmony (the heart)
| Text line | Param | Notes |
|---|---|---|
| `chord play <note[s]>` | `kChordPlay` | steer the band; single note in single-finger mode; SHIFT variant stages to next bar |
| `chord stop` | `kChordStop` | release voicing |
| `chord detect on\|off [port]` | `kChordDetect` | live piano→chord detection on an input port |
| `chord follow <auto\|detect\|sequencer\|manual\|live>` | `kChordFollow` | engine default = `kLivePriority` |
| `chord mode <diatonic\|single\|shell>` | `kChordMode` | input interpretation |
| `chord hold on\|off` | `kChordHold` | |
| `chord out <port:ch>` | `kChordOut` | |
| `key <root> <mode>` / `scale ...` | `kKeySet` | major/minor/dorian/…/locrian |
| (input zone) | `kInputZone` | port → melody(sounds) / harmony(silent, feeds detector) |

### Recorded chord sequences
`seq new/use/rec/add/loop/play/stop/transpose/del/clear` → `kSeqNew..kSeqClear`. Secondary for
the GUI MVP (song-mode territory).

### Parts mixer
| Text line | Param | Notes |
|---|---|---|
| `part mute <role> <0\|1>` | `kPartMute` | roles: drums,perc,bass,chord1,chord2,pad,arp,phrase |
| `part solo <role> <0\|1>` | `kPartSolo` | any-solo ⇒ only soloed parts play |
| `program <gm> <port:ch>` | `kProgram` | GM voice select |
| `style route <role> <port:ch>` | `kStyleRoute` | |

### Groove / feel
`groove <field> <value>` → `kGroove`. Fields: `swing, humanize_timing, humanize_velocity,
accent` (0..100%), `swing_grid` (8|16), `quantize` (0..100%), `seed`.

### Arpeggiator
`arp <field> <value>` → `kArp`; `arp out <port:ch>` → `kArpOut`. Fields: `enabled, rate
(1/4..1/32), direction (up/down/updown/downup/asplayed/random), octaves (1..4), gate (0..100),
latch, seed`.

### Timeline step tracks (step sequencer)
`track new/step/length/mute/solo` → `kTrackNew..kTrackSolo`. Advanced; not MVP.

### Reserved — DO NOT SEND
MIDI-FX / insert chain (`kFxSet/kFxParam/kFxEnable/kFxClear`) is **shape-reserved but has no
live ids** in v1; sending it yields `unsupported`. Reserve UI space, wire nothing.

---

## 2. Events the GUI RECEIVES (JSONL)

Exactly five `OutEvent::Kind`s cross the wire; nothing else outbound.

| JSONL shape | Kind | Fires when |
|---|---|---|
| `{"ev":"midi-out","port":N,"msg":"noteon\|noteoff\|cc\|program\|pitchbend\|raw\|<realtime>",…,"@":tick}` | `kMidi` | **every** sounded byte — arranger, tracks, chord, arp all sound through raw MIDI; there is no separate "note" event |
| `{"ev":"chord","in":"<note>","out":"<PCquality>","deg":"<roman\|->","@":tick}` | `kChord` | **only** from the recorded-sequencer path — **NOT** from `chord play` nor from live detection (see Gaps §3) |
| `{"ev":"section","name":"<varA\|fillB\|…>","@":tick}` | `kSection` | arranger section **change** only |
| `{"ev":"transport","state":"playing\|paused\|stopped","@":tick}` | `kTransport` | emitted from the arranger-ending stop path; **start/explicit-stop may not emit it** (see Gaps) |
| `{"ev":"warn","code":"<name>","@":tick}` | `kWarn` | failure/nack only: scheduler_full, route_table_full, unknown_command, bad_argument, track_table_full, not_in_key, seq_table_full, seq_empty, unsupported |

Error to the offending client only (not an event): `{"error":"<msg>","cmd":"<line>"}`.
**There is no positive/success ack.** Success is inferred from resulting events or silence.

---

## 3. GAPS — what a live GUI needs that v1 lacks (ADDITIVE-ABI proposals, never breaks)

These are flagged, not invented. Each is an **additive** OutEvent/field — the freeze is
additive-only, so none touches an existing id. **P0 = blocks the central live surface.**

1. **[P0] No "currently-followed chord" stream.** `chord play` and live piano→chord detection
   change the followed context **silently** — the only `kChord` event is the recorded-sequencer
   path. The GUI's central green/amber harmony visualizer has **no event to subscribe to** for
   the two most important producers. → Propose an additive `kChordFollowed` event carrying
   `{current, pending, valid, source}`, fired on every commit (manual / detect / sequencer /
   bar-promote).
2. **[P0] No transport position / beat heartbeat.** `Transport::position()` (bar/beat/tick) is
   in-process only. A playhead / bar-progress UI would have to decode raw MIDI clock (F8) and
   only if `clock out` happens to be enabled. → Propose additive `kBeat`/`kPosition`, or extend
   `kTransport` to fire on start/stop/continue.
3. **No state-on-connect sync.** A GUI attaching mid-session cannot learn current style/section,
   key, mute/solo, groove/arp values, follow mode — all Params are write-only, `Op::kGet`
   appears unwired in v1. → Propose an additive "snapshot on connect" or per-domain readback.
4. **No positive ack / no request correlation.** Can't confirm "kStyleLoad accepted" except by
   side effect. → Propose an ack event; note `Command` has no correlation field to match it.
5. **No held-notes / detector state event.** "3 of 3 fingers held" is in-process introspection
   only. → additive detector-state event.

Priority for the WOW loop: **1 and 2 first** — they make the live harmony visualizer and the
playhead real over the wire. 3 makes reconnection honest. 4–5 are polish. The GUI can ship a
first slice by *inferring* the current chord from the `kMidi` note stream + `kSection`, but that
is a workaround; the proper fix is proposals 1–2.

---

## 4. What the GUI should MIRROR vs. what it should NOT duplicate

Per `docs/reflections/hybrid-arranger-gap-analysis.md`: the GUI is "a mirror that reflects and a
keyboard that commands, never a truth and never a note in the timing path." The TUI already
does keyboard-first live control well (CTRL+P, backtick chooser, parts mixer, MIDI monitor).
Split: **GUI for 2-D structure, inspection, and a big readable harmony surface; live steering
stays a first-class but not re-invented keyboard/MIDI path.**

Note the doc/brief say "CTRL+SPACE chooser" but the *shipped* chooser key is **backtick** `` ` ``;
CTRL+SPACE is the piano surface's momentary↔toggle switch. Carry the real bindings forward.

Colour semantics to preserve exactly: **bold GREEN = chord followed THIS bar** (gated OFF at
rest — lit only when transport plays or a chord is explicitly steered), **AMBER = pending chord
staged for next bar**, off otherwise; green wins over amber on a shared pitch class.

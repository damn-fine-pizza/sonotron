# Phase-6 design reviews

Consolidated architecture/scope reviews for the Phase-6 items of
`docs/phase6-plan.md`. Each top-level section is one design item. Authors:
Corelli (as-built architecture/placement/thread-seam shape), Prospero
(pre-code direction), Ottorino (musical scope), as noted per section.

Shared ground carried from `docs/phase5-design-reviews.md` (still true):
dual-target doctrine (`components/arrangrr`'s `CMakeLists.txt` applies
`-fno-exceptions -fno-rtti -fno-threadsafe-statics` PUBLIC to every dependent
TU; host-only packages may use heap/`std::vector`/`std::string`/file I/O),
seeded determinism (D16), the Phase-5 ABI position (`components/arrangrr/
include/arrangrr/abi.hpp`'s `Command`/`OutEvent` unfrozen for Phase 5,
`sizeof(Command) <= 20`, `sizeof(OutEvent) <= 16`).

---

## Audio in the standalone GUI — thread/latency seam (Theme 2, node `0910`/D43)

Corelli, DESIGN (2026-07-14). Pre-code as-built shape review, no code
written or edited by this review.

### What Theme 2 is

Make `apps/gui-sonotron` audible on its own by realizing the in-process
engine's `OutEvent` MIDI stream through `melodd::Synth`
(`components/melodd/include/melodd/synth.hpp`) and a `miniaudio` playback
device, mirroring `apps/tools/melodd/main.cpp`'s reference wiring but
in-process instead of over an ALSA round-trip
(`components/melodd/README.md`'s documented follow-up).

### Traced ground

- `components/melodd/include/melodd/synth.hpp` — `Synth` is explicitly **not
  internally thread-safe**: "a caller that dispatches MIDI events from one
  thread ... and renders from another ... must synchronize its own calls."
- `apps/tools/melodd/main.cpp` — the reference pattern: one
  `std::mutex` guards every call into `Synth` (`note_on`/`note_off`/
  `program_change`/`pitch_bend`/`control_change`/`render`), including from
  the `ma_device` `data_callback`. The file's own header comment states this
  is acceptable there because melodd "is a downstream audio sink... there is
  no realtime scheduling on this path to protect from resampling jitter."
- `apps/gui-sonotron/src/spsc_ring.hpp` — the header-only lock-free SPSC ring
  already used to cross the engine-thread → GUI-thread boundary. Its own
  header comment is a standing ruling in this codebase: "Corelli §15.2
  correction #2: lock-free from the first landing, NOT mutex-first — a
  mutex-guarded queue and the brief's own 'wait-free' claim are mutually
  exclusive, since a GUI-thread producer preempted inside a locked critical
  section would stall the engine-thread consumer for an OS-scheduler-defined,
  unbounded duration."
- `apps/gui-sonotron/src/in_process_brain_session.cpp` — the engine thread's
  `Shell` callback already does exactly the filtering a Synth feed needs:
  `if (alsa_ok && ev.kind == OutEvent::Kind::kMidi) { alsa.send(ev.port,
  ev.msg); } (void)out_event_ring.try_push(ev);`. `InProcessBrainSession`'s
  header deliberately exposes no `engine()`/`stage()` accessor ("Ownership
  discipline (Corelli §15.6): `shell`... is a local variable of THIS
  function... never reachable from the GUI thread").
- `apps/gui-sonotron/CMakeLists.txt` — `gui_sonotron_engine` is documented as
  "this library, and only this library, includes arrangrr/runtime/hostrt
  headers and links hostrt/ALSA directly." Every other library in the
  directory (`gui_sonotron_ring`, `gui_sonotron_models`, `gui_sonotron_brain`,
  `gui_sonotron_layout`, `gui_sonotron_screenshot`) is core-free by
  construction.
- `components/hostrt/CMakeLists.txt:43` —
  `target_link_libraries(hostrt PUBLIC arrangrr runtime orchestrator
  ALSA::ALSA)`. Combined with `components/arrangrr/CMakeLists.txt`'s PUBLIC
  `-fno-exceptions -fno-rtti -fno-threadsafe-statics`, this means
  `gui_sonotron_engine`'s own `.cpp` files (which link `hostrt` PUBLIC) are
  **already** compiled without exceptions/RTTI today, transitively, before
  this theme adds anything.
- `components/melodd/CMakeLists.txt` and `components/melodd/README.md` — "This
  component links only `third_party/tinysoundfont` — never `arrangrr`,
  `runtime`, or `hostrt`. It is realization-ignorant of its caller." No
  `-fno-exceptions` on the `melodd` target: its TUs compile with default
  (exceptions-on) host flags. `grep` for `throw`/`.at(`/`std::sto*` in
  `components/melodd/src/synth.cpp` and `soundfont_discovery.cpp` returns
  nothing — the public surface is exception-clean by convention (`bool` +
  `std::string& error`), even though the library itself is not built
  exception-free.
- `apps/gui-sonotron/main.cpp` — the `brain_session_holder` (whichever
  `BrainSession` backend) is a `unique_ptr` local in `main()`, destroyed at
  function-scope exit, after `glfwTerminate()` and the layout-save. Only
  `InProcessBrainSession::start()` spawns a real engine thread; `--control`
  (`UdsBrainSession`) is a pure JSONL-text client with no raw `OutEvent`
  access at all.
- `docs/DESIGN.md` node `0910`/D43: "arrangrr (the MIDI brain) and a future
  host-only audio engine are peer modules wired by an orchestrator,
  name-blind, talking only through the POD interface; audio never crosses
  the interface; the core stays audio-ignorant." `0700`: "the core contract is
  typed BINARY commands/events... string-path resolution and JSONL live
  host-side."

### Decision 1 — thread topology & MIDI handoff

**Recommendation: lock-free ring for note-shaped MIDI (option B), NOT a
mutex, for the interactive dispatch path — mirroring `spsc_ring.hpp`'s own
already-established ruling for this exact codebase's engine↔GUI boundary.**
A second, dedicated `SpscRing<AudioMidiEvent, N>` (new, small POD — see
Decision 3) carries channel-voice events from the engine thread to the
`ma_device` audio callback, drained at the top of the callback exactly the
way the existing OutEvent ring is drained at the top of `poll()`.

Justification: this is an *interactive instrument* — pad/clip triggering is
felt as output latency, and a mutex taken in an audio callback can be blocked
by *any* other holder of that mutex for an OS-scheduler-defined, unbounded
duration (the same argument `spsc_ring.hpp` already made for the GUI↔engine
boundary, transposed one hop further). A stall in `apps/tools/melodd`'s own
mutex is invisible because nothing else contends for it under normal load
(one ALSA poller, one audio callback, no GUI thread in that process); in
`gui-sonotron` the same mutex would also be a candidate lock for the
SoundFont-load path (Decision 4) and potentially other GUI-thread work,
raising real contention risk that does not exist in the reference binary.

Tradeoff accepted: a second bounded ring (more state, one more POD contract)
instead of reusing `apps/tools/melodd`'s single-mutex shape wholesale. This is
the same tradeoff the codebase already made once for the GUI↔engine boundary
and documented as worth it.

**Narrow exception, not a contradiction of the above:** `load_soundfont()`
and `all_notes_off()` (Decision 4) do NOT fit the ring model (they mutate the
whole `tsf*` state, not a discrete timed event) and are rare, GUI-initiated,
latency-tolerant actions. These two, plus `render()`, should share one small
mutex — exactly `apps/tools/melodd/main.cpp`'s own pattern, but scoped
*only* to this triad, never to note dispatch. The two mechanisms are not in
tension: the ring solves the felt-latency problem (many events/sec, must
never stall); the mutex solves the rare-mutation problem (at most one call
per file-picker click, contention-free in practice).

### Decision 2 — target boundary & ownership

**Recommendation: a new, arrangrr-free `gui_sonotron_audio` library**
(`melodd` + `miniaudio` + `gui_sonotron_ring` + `Threads`, nothing else),
owning a new `AudioEngine` class (`melodd::Synth` + `ma_device` + the note
ring's consumer + the load/panic mutex). It does **not** include
`arrangrr/abi.hpp` and does **not** link `hostrt`/`arrangrr`/`runtime` —
preserving `gui_sonotron_engine`'s documented CMake invariant ("this
library, and only this library...") unchanged rather than adding a second
arrangrr-toucher.

The `OutEvent → AudioMidiEvent` translation happens **inside**
`gui_sonotron_engine`'s `run_engine()`, at the exact point that already
filters `Kind::kMidi` for the ALSA send (`in_process_brain_session.cpp`'s
Shell callback). `InProcessBrainSession` gains one narrow, deliberate
addition mirroring its own existing pimpl discipline (no `engine()`/
`stage()` accessor today, by design): a reference/handle to the new
consumer-side ring, exposed through a small header that names only
`AudioMidiEvent` (built from `common::MidiMessage`, already a
dependency-free type used by `apps/tools/melodd`), never `arrangrr::OutEvent`
or any `arrangrr::Param`/`Kind` enum. `gui_sonotron_audio` therefore never
needs to include an arrangrr header, and inherits none of `arrangrr`'s PUBLIC
`-fno-exceptions -fno-rtti` compile flags — keeping `melodd`'s
default-flags TUs fully isolated from the core's flag regime rather than
silently crossing that boundary (see the traced-ground note on
`components/hostrt/CMakeLists.txt:43`).

`AudioEngine` is owned by `main()`, as a sibling local to
`brain_session_holder`, constructed only in the integrated-mode branch
(`if (control_path.empty())`) and declared *after* `brain_session_holder`
so C++'s reverse-destruction-order rule tears it down *before* the
`InProcessBrainSession` (and its engine thread / ring producer) is
destroyed — see the lifetime note under Decision 5.

**Scope cut for this slice:** audio attaches **only** in integrated
in-process mode. `--control <path>` (`UdsBrainSession`) stays silent (as
today — an external synth over ALSA is the answer there); its transport is
JSONL text, not the binary ring, so an audio tap there would need a new
wire-level channel-voice echo in the socket protocol — a real ABI/wire
change, out of this theme's scope, and NEEDS-DECISION if ever wanted.

Feasibility: **HOST-ONLY**, no new dependency (miniaudio + TinySoundFont
already vendored per `apps/gui-sonotron/CMakeLists.txt`'s existing
`third_party` wiring and `components/melodd/CMakeLists.txt`).

### Decision 3 — `OutEvent` → `Synth` MIDI mapping

`OutEvent::Kind::kMidi` (`components/arrangrr/include/arrangrr/abi.hpp`)
already carries everything `Synth` needs: `port` (u8), `msg`
(`common::MidiMessage` — `status`/`d1`/`d2`, `type()`/`channel()` decode
helpers), `tick`. This is exactly the shape `apps/tools/melodd/main.cpp`'s
`dispatch_message()` already decodes from raw ALSA bytes into
`note_on`/`note_off`/`program_change`/`pitch_bend`/`control_change` calls.

**Recommendation: do not hand `melodd::Synth` (nor the new
`gui_sonotron_audio` library) the full `arrangrr::OutEvent` `Kind` space.**
Translate at the one point inside `gui_sonotron_engine` that already touches
`OutEvent` (Decision 2) into the small `AudioMidiEvent{port, MidiMessage}`
POD, and extract `dispatch_message`'s switch (currently a file-local,
anonymous-namespace function in `apps/tools/melodd/main.cpp`) into a shared,
reusable `MidiMessage → Synth` dispatch entry point owned by `melodd` itself
(e.g. alongside `synth.hpp`), so both the standalone binary and the new GUI
glue call ONE authoritative decode instead of maintaining two independently
hand-written switches — the same "shared label helpers, not two drifting
tables" discipline `brain_event_from_outevent.cpp`/`components/hostrt/
jsonl.cpp` already apply via `event_labels.hpp`. This is a structural
seam-to-cut, not a line edit; Giotto decides the exact function shape.

**Flagged, not resolved here (product scope, not architecture):**
`OutEvent.port` is the arrangrr output-port index used for ALSA routing
(`in_process_brain_session.cpp`'s `alsa.send(ev.port, ev.msg)` sends *every*
`kMidi` event regardless of port). `melodd::Synth` has no concept of a
"port" — it is one 16-channel destination. Today's engine, in the integrated
default, wires exactly one output port (`port open out out0`). Whether the
GUI's Synth realization should (a) realize every `kMidi` event unconditionally
(mirrors "melodd is a peer, not a replacement" — the same MIDI a hardware
synth on `out0` would receive, it also hears), or (b) filter/gate by port
once multiple output ports exist, is a product decision, not a structural
one — flagged for the owner, not decided by this review.

### Decision 4 — SoundFont load lifecycle

`load_soundfont()` performs the file read (`tsf_load_filename` inside
`components/melodd/src/synth.cpp`) synchronously; that must happen on the
GUI thread (the file-picker's own call site), never inside the `ma_device`
callback (blocking file I/O in an audio callback is a worse glitch risk than
a short mutex). **Recommendation:** guard the *pointer-swap* tail of
`load_soundfont()` — not the disk read — with the same small mutex Decision
1 already reserves for `{render(), load_soundfont(), all_notes_off()}`. This
reconciles cleanly with the ring-based note dispatch (Decision 1): the
critical section covers only `render()`'s brief execution and the equally
brief `m_tsf` pointer replacement, never note-by-note traffic.

`find_system_soundfont()` (`components/melodd/include/melodd/
soundfont_discovery.hpp`) should be reused verbatim for a default-path
prefill in the "Load SoundFont…" picker — it already mirrors `apps/demo/
lib/launch.sh`'s discovery order and takes an `override_path`, exactly the
shape a file-picker's chosen path needs. HOST-ONLY, zero new dependency.

### Decision 5 — other structural notes

- **Device lifecycle vs GUI teardown.** `main()`'s `brain_session_holder`
  destroys at function-scope exit (`apps/gui-sonotron/main.cpp`); the new
  `AudioEngine` local must be declared *after* it so C++ destroys `AudioEngine`
  first (device stopped, ring abandoned) *before* `InProcessBrainSession`
  joins its engine thread. Either teardown order is memory-safe given the
  ring's SPSC contract (a producer pushing into an ignored/full ring after
  its sole consumer stops draining is harmless), but the declaration-order
  rule is the cheapest way to make the *intended* order the *guaranteed* one
  rather than an accident of which destructor happens to run first.
- **Panic/all-notes-off on stop is a real, currently-missing wire.**
  `Synth::all_notes_off()`'s own doc comment says "Silences every voice on
  every channel immediately (panic / **shutdown**)" — the hook already exists
  for exactly this. Today, engine-side `Param::kPanic` emits real note-off
  `OutEvent`s through the *live* ring (works fine while playing), but nothing
  calls `all_notes_off()` when the engine thread itself stops (window close,
  `InProcessBrainSession::stop()`): any note sounding at that instant has no
  more note-offs coming and will hang. `AudioEngine`'s own shutdown path must
  call `all_notes_off()` explicitly — a one-line fix, but it belongs in the
  new class's destructor, not assumed.
- **Sample-rate ownership.** `apps/tools/melodd/main.cpp` hardcodes
  `kSampleRate = 44100` locally; the new GUI binary would need the identical
  value duplicated a second time. Minor coupling/cohesion point: promote it
  to a `melodd::kDefaultSampleRate` constant living with `Synth` itself
  (the thing that actually owns the "what sample rate does realization run
  at" decision), so it changes in one place, not two independently-edited
  binaries. HOST-ONLY, no dependency.
- **Doc-hygiene note (not mine to fix).** `apps/gui-sonotron/src/
  in_process_brain_session.cpp` and `apps/gui-sonotron/CMakeLists.txt`
  cite `docs/design/sonotron-server-phase2-brief.md` and
  `docs/design/gui-contract-map.md` by path; neither exists post the
  42-files-to-9-canonical-files consolidation (`13cf904`). The design intent
  they describe is traceable in the code comments themselves (quoted above),
  so this review is not blocked by it, but the dangling paths are worth a
  Palladio/Vasari pass separately.

### Feasibility summary

| Item | Label | Dependency |
|---|---|---|
| Note-shaped MIDI ring (Decision 1) | SHIPPABLE | none (mirrors `spsc_ring.hpp`) |
| `render`/`load_soundfont`/`all_notes_off` mutex (Decision 1/4) | SHIPPABLE | none |
| `gui_sonotron_audio` library, arrangrr-free (Decision 2) | SHIPPABLE | none (miniaudio/TinySoundFont already vendored) |
| `AudioEngine` owned by `main()`, integrated-mode-only (Decision 2) | SHIPPABLE | none |
| Audio tap for `--control`/external-server mode | NEEDS-DECISION | would need a JSONL wire addition — owner scope call, deferred |
| `OutEvent.port` filtering semantics for realization (Decision 3) | NEEDS-DECISION | product scope, not structural |
| Shared `MidiMessage → Synth` dispatch helper (Decision 3) | SHIPPABLE | none |
| `find_system_soundfont` reuse for prefill (Decision 4) | SHIPPABLE | none |
| `melodd::kDefaultSampleRate` promotion (Decision 5) | SHIPPABLE | none |
| `all_notes_off()` on `AudioEngine` shutdown (Decision 5) | SHIPPABLE | none |

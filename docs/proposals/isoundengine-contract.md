# ISoundEngine / IAudioBackend — the pluggable sound-engine seam

Status: PROPOSAL — logical/API design only, no code written. Complements
`docs/proposals/audio-engines-family-layout.md` (Palladio, on-disk placement,
2026-07-14/15), which this document assumes as the physical-move plan of
record where it does not conflict with the newer two-tier `components/core`
vs `components/platform` REGIME axis. Where the two disagree on a directory
name, this document flags it explicitly (§7) rather than re-litigating
placement, which is not mine to fix.

Author: Corelli. Structural review + contract design, no product-code edits.

---

## 0. Ground truth traced

- `docs/DESIGN.md` — hard rules #1-#10 (pure/sink-and-source core, thin HAL,
  bounded everything, "virtual only at the HAL boundaries" line 580, fixed-
  point-by-default line 64, D4/`0800` dependency-free core line 706-708),
  node `0910`/D43 (line 709-716: "arrangrr … and a future host-only audio
  engine are peer modules wired by an orchestrator, name-blind, talking only
  through the POD interface; audio never crosses the interface; the core
  stays audio-ignorant (identity `0110` intact)"), the CI-verified-not-
  aspirational operating rule (line 583: "a feature enters the core only if
  CI compiles it on both targets … the list is positive/CI-verified, not
  aspirational").
- `CMakeLists.txt:41-101` — the actual fork line: `components/common`,
  `components/runtime`, `components/chorddet`, `components/arrangrr` are
  unconditional (lines 45-52); everything else (`midisrc`, `orchestrator`,
  `hostrt`, `melodd`, `gui-sonotron`, tools) sits in the host-only `else()`
  branch (lines 56-101).
- `components/melodd/CMakeLists.txt` — today's only "engine": HOST-ONLY by
  explicit banner comment, links `tinysoundfont`, does not link `common`
  (only reaches its include dir, to avoid inheriting `-fno-exceptions`).
- `components/arrangrr/CMakeLists.txt:26-30` — `-fno-exceptions -fno-rtti
  -fno-threadsafe-statics` applied PUBLIC, "same subset on host and firmware
  (D3, D32)".
- `components/common/include/common/midi/message.hpp` — `MidiMessage` is a
  3-byte POD (`static_assert(sizeof(MidiMessage) == 3)`), unconditional tier,
  already the shared vocabulary both `arrangrr` and `melodd` use.
- `apps/gui-sonotron/src/audio_engine.{hpp,cpp}` — today's `AudioEngine`:
  owns `melodd::Synth`, an `ma_device`, `AudioMidiRing`, one `std::mutex`
  guarding exactly the triad `{ring-drain+dispatch+render(), adopt_soundfont()
  tail, all_notes_off()}`. `render(float* out, int frame_count)` already
  takes a caller-provided buffer, no owned return.
- `apps/gui-sonotron/src/audio_midi_event.hpp` — `AudioMidiEvent{port,
  MidiMessage}`, deliberately NOT `arrangrr::OutEvent`, so the audio side
  never names `arrangrr::Param`/`Kind`.
- `apps/gui-sonotron/src/spsc_ring.hpp` — `SpscRing<T,N>`: template, only
  `<array>`/`<atomic>`/`<type_traits>`, trivially-copyable-only static_assert,
  no heap, no exceptions. Its own header comment claims "Host-side glue
  only … never in the freestanding core" — a claim about *location*, not
  about *content* (see §5).
- `apps/gui-sonotron/src/in_process_brain_session.cpp:466-478` — the exact
  translation point: `if (audio_out_ring != nullptr && ev.kind ==
  OutEvent::Kind::kMidi && …) audio_out_ring->try_push(AudioMidiEvent{.port =
  ev.port, .msg = ev.msg});` — confirmed still current on this branch.
- `apps/gui-sonotron/main.cpp:503-536` — `AudioEngine` is a `unique_ptr`
  local declared after `brain_session_holder`; `in_process_session-
  >set_audio_ring(&audio_engine->note_ring())` wires producer to consumer;
  `render_soundfont_dialog` calls `AudioEngine::load_soundfont` directly on
  the concrete type from the GUI thread.
- `docs/phase6-design-reviews.md` §"Audio in the standalone GUI" — the prior
  Corelli review that shaped today's `AudioEngine`; its Decisions 1-5 are the
  precedent this proposal generalizes, not replaces.
- `docs/proposals/audio-engines-family-layout.md` — Palladio's placement plan
  (flat `components/audio` + `components/audio-engines/{core,soundfont,
  physical-models,analog}`), already argued the WRAP-not-absorb call on
  `melodd` and the host-only-`else()`-branch placement for the interface.
- `components/samplrr/README.md` — "SLOT — no code yet. Host-only sampler
  engine" — a fourth future family member already reserved, host-only by its
  own stub text (not requested in this task, noted for completeness only).

---

## 1. The `ISoundEngine` contract

Placement: a new leaf, `components/core/audio_engine/include/audio_engine/
i_sound_engine.hpp` (INTERFACE target `audio_engine_core`) — see §4 for why
this belongs on the `core` side of the REGIME axis, not `platform`.

```cpp
namespace sonotron::audio_engine {

// The render/synthesis seam. Every method signature is regime-neutral on
// purpose: no std::string, no std::function, no exceptions, no owned
// allocation — POD in, POD out, caller-provided buffers only. This is what
// makes the interface itself core-capable NOW even though every concrete
// implementation today is host-only (§4).
class ISoundEngine {
 public:
  virtual ~ISoundEngine() = default;

  // One MIDI channel-voice message. Same "not internally thread-safe"
  // contract melodd::Synth already documents: the caller (IAudioBackend)
  // must serialize dispatch()/render()/all_notes_off() itself.
  virtual void dispatch(const arrangrr::MidiMessage& msg) noexcept = 0;

  // Panic / shutdown: silence every voice on every channel immediately.
  // Kept as its own virtual (not synthesized from 16x CC-123 dispatch
  // calls) because the engine, not the caller, knows the cheapest way to
  // zero its own voice state.
  virtual void all_notes_off() noexcept = 0;

  // Renders `frame_count` interleaved stereo frames (2 floats/frame, -1..1)
  // into a CALLER-OWNED buffer. Implementations with nothing loaded/no
  // voices active must zero-fill, never leave `out` uninitialized.
  virtual void render(float* out, int frame_count) noexcept = 0;

  // Static-literal identification for diagnostics/UI only — never on a hot
  // path, no ownership transfer, free on a core-capable implementation too.
  virtual const char* name() const noexcept = 0;
};

}  // namespace sonotron::audio_engine
```

**Deliberately excluded from the interface — configuration/load.**
`load_soundfont(path, error)`-shaped operations do NOT belong on
`ISoundEngine`. This is an Interface-Segregation call, not an oversight: a
soundfont engine loads a `.sf2` file; a physical-models engine would load a
DSP parameter table; an analog engine likely loads nothing (patch state is
in-memory parameters, possibly just `set_param(id, value)`). Forcing one
shared "load a thing from a path" signature onto all three would either
under-specify (a `void* + size` blob, ceremony) or leak host-only
`std::string`/file-I/O concerns onto the one member (`analog`) this contract
exists to keep promotable. **Concrete engines expose their own configuration
API on their own concrete type**; the composition root (today `main.cpp`,
tomorrow whatever picks the engine) talks to the concrete type for
configuration and to the `ISoundEngine&` only for the render/dispatch/panic
triad — exactly how `main.cpp:287` already calls `AudioEngine::
load_soundfont` (a concrete-type method), never through an abstract seam.

---

## 2. `IAudioBackend` — a naming critique, then the concrete shape

The task framing calls for `IAudioBackend`. Structural critique: **an
abstract base class here is ceremony not yet earned.** There is exactly one
implementation in sight (a `miniaudio`-backed host device), and `miniaudio`
itself is already the cross-platform (Linux/macOS/Windows) device
abstraction — a second `IAudioBackend` implementation would only appear if
the project dropped `miniaudio` for a second device library, which is not on
the table. Introducing a virtual interface with one implementation and no
second candidate is exactly the "abstraction invented where none was needed"
defect the role exists to catch (compare: `ISoundEngine` has THREE named,
distinct implementations — soundfont/physical/analog — that is what earns
its `virtual`). **Recommendation: keep it a concrete class**, `AudioBackend`
— today's `AudioEngine` renamed and relocated, generalized to hold a
reference to an `ISoundEngine` instead of an owned `melodd::Synth`:

```cpp
namespace sonotron::audio {

class AudioBackend {
 public:
  explicit AudioBackend(audio_engine::ISoundEngine& engine,
                         int sample_rate = 44100);
  ~AudioBackend();
  AudioBackend(const AudioBackend&) = delete;
  AudioBackend& operator=(const AudioBackend&) = delete;

  AudioMidiRing& note_ring() noexcept { return m_note_ring; }
  bool device_ready() const noexcept { return m_device_ready; }

  // The ONE thing AudioBackend exposes beyond render/dispatch/panic: a
  // handle any concrete engine's OWN configuration path (e.g.
  // SoundfontEngine::load(path)) can lock to serialize a slow state-swap
  // against render() -- mirrors today's single mutex covering exactly
  // {render(), adopt_soundfont() tail, all_notes_off()}
  // (audio_engine.hpp's own header comment). AudioBackend does not know
  // WHAT is being swapped; it only serializes against the render callback.
  std::mutex& render_mutex() noexcept { return m_mutex; }

 private:
  static void data_callback(ma_device*, void*, const void*, ma_uint32);
  void render(float* out, int frame_count);

  std::mutex m_mutex;
  audio_engine::ISoundEngine& m_engine;   // NOT owned -- composition root owns lifetime
  AudioMidiRing m_note_ring;
  ma_device m_device{};
  bool m_device_ready = false;
};

}  // namespace sonotron::audio
```

`render_mutex()` is intentionally on `AudioBackend` (a class that is
platform-tier, host-only, forever — §4) and NOT anywhere near
`ISoundEngine`: `std::mutex` is exactly the kind of host type the interface
must never reference if it is to stay promotable. A future `SoundfontEngine::
load()` (host-only concrete type) is free to take `AudioBackend&` and its
`render_mutex()` in its own signature — that coupling is fine because both
sides of it are host-only forever.

**Ownership is single-shot, not hot-swappable.** `AudioBackend` holds
`ISoundEngine&`, chosen once at construction by the composition root — it
does not own or swap the engine at runtime. Live engine-switching
(soundfont → analog without restart) is a materially bigger feature (needs
`unique_ptr<ISoundEngine>` + a swap serialized under `m_mutex`, plus a
"silence outgoing engine before swap" protocol) and is **NEEDS-DECISION**,
out of this proposal's scope — flagged, not built.

---

## 3. The MIDI-in → audio-out data path

Unchanged shape from today, generalized at exactly one point:

```
gui_sonotron_engine (in_process_brain_session.cpp:466-478)
  OutEvent::Kind::kMidi  --translate-->  AudioMidiEvent{port, MidiMessage}
                                              |
                                    AudioMidiRing (SpscRing<AudioMidiEvent,1024>)
                                    producer: engine thread
                                    consumer: ma_device callback thread
                                              |
                                       AudioBackend::render()
                                       (holds the ONE render_mutex)
                                              |
                          while (ring.try_pop(ev)) engine.dispatch(ev.msg);
                          engine.render(out, frame_count);
```

- **Who owns the ring:** `AudioBackend` (platform tier), exactly as today.
  `ISoundEngine` implementations never see the ring — they only ever
  receive already-dequeued `MidiMessage` values through `dispatch()`. This
  is correct and should not change: the ring is a device-layer/thread-
  topology concern, orthogonal to which synthesis backend is plugged in.
- **Event type crossing the seam:** `AudioMidiEvent{port, MidiMessage}`
  only — `arrangrr::OutEvent`/`Param`/`Kind` never reach `components/
  core/audio_engine` or any concrete engine. This is the same discipline
  `docs/phase6-design-reviews.md` Decision 2/3 already established for
  `gui_sonotron_audio`, and it is exactly what `0910`/D43 demands ("name-
  blind, talking only through the POD interface … the core stays audio-
  ignorant"). Carry it forward unchanged.
- **The render callback contract:** `AudioBackend::data_callback` (static,
  `ma_device*` trampoline) → `render(float* out, int frame_count)` → drains
  ring → `m_engine.dispatch(...)` per event → `m_engine.render(out,
  frame_count)`, all under `m_mutex`. This is a near-literal find/replace of
  today's `audio_engine.cpp:69-77` (`melodd::dispatch_midi_message(m_synth,
  ev.msg)` → `m_engine.dispatch(ev.msg)`; `m_synth.render(out, frame_count)`
  → `m_engine.render(out, frame_count)`) — strong evidence the seam is
  already correctly shaped in the existing code, not a new invention.
- **Open, flagged, product-not-structural (carried from the prior review):**
  `OutEvent.port` filtering — today ALL `kMidi` events reach the ring
  regardless of port (`in_process_brain_session.cpp:476`). If the family
  ever wants per-port engine routing (soundfont on port 0, analog on port 1,
  simultaneously), `ISoundEngine::dispatch` or `AudioBackend` would need a
  port-keyed fan-out — not requested, not built, **NEEDS-DECISION**.

---

## 4. Regime assignment — validation, not re-litigation

Confirmed against the REGIME axis (needs a hosted OS: yes/no):

| Component | Regime | Why |
|---|---|---|
| `ISoundEngine` (interface only) | **core-capable, now** | Pure virtual, POD-only signatures (`arrangrr::MidiMessage`, `float*`, `int`, `const char*`), no heap/exceptions/RTTI/std-string in the contract itself. Nothing in it needs an OS. |
| `soundfont` engine (wraps `melodd::Synth`) | **host-only, forever** | `melodd::Synth::load_soundfont` takes `std::string` paths and does real file I/O (`tsf_load_filename`) — a hard, permanent dependency on a filesystem. No promotion path exists or is claimed. |
| `physical-models` engine (slot, no code) | **host-only, forever** (per the task's own framing) | Any physical-modeling engine realistic enough to be useful will load impulse-response/DSP tables from disk exactly like `soundfont` does — same file-I/O dependency, same verdict. |
| `analog` engine (slot, no code) | **the one plausible future core-capable member** | A from-scratch, formula/table-driven subtractive synth has no inherent file-I/O requirement — oscillators/filters/envelopes are `constexpr` tables + arithmetic, the same regime `components/arrangrr`'s chord/theory tables already live in. Promotable **conditionally**: only if its actual implementation stays bounded/no-heap/no-exception when written (this is a promise about future code, not a property of an empty slot — flag this to whoever implements it, per Palladio's own note). |
| `AudioBackend` (device layer) | **platform, forever** | Owns a real `ma_device`, a `std::mutex`, an OS audio thread. No promotion path — an eventual firmware audio path (I2S/DMA ISR) would be an entirely different driver, not this class recompiled. |

This matches and confirms `docs/proposals/audio-engines-family-layout.md`
§4's own conclusion. One addition: **the interface's placement decision is
currently under-verified.** Palladio's plan (rightly, for placement reasons)
puts `audio_engine_core` in the host-only `else()` branch for now. But
DESIGN.md's own operating rule (line 583) is explicit: "a feature enters the
core only if CI compiles it on both targets … the list is positive/CI-
verified, not aspirational." As drafted, nothing in either proposal compiles
`i_sound_engine.hpp` on `arm-none-eabi` until the day someone actually
promotes it — which means "dual-target-ready" is, today, an unverified claim
about a header nobody has tried to compile freestanding. **Concrete fix,
cheap and small:** add a compile-only check under `tests/arm-smoke/` — a
trivial `NullSoundEngine : public ISoundEngine` (all four methods, no-ops)
compiled (not necessarily linked into a running firmware image) on the
`arm-none-eabi` toolchain, alongside the existing arm-smoke gate
(`CMakeLists.txt:54-55`). This costs nothing at runtime, adds one TU, and
turns "the seam is shaped so it can be promoted later" from a design intent
into a CI-enforced fact from day one — consistent with how the rest of the
core tree already operates. **SHIPPABLE, no dependency, small enough to be
Nazzareno's, not a redesign.**

---

## 5. A tier mis-assignment worth flagging (not mine to relocate)

`SpscRing<T, Capacity>` (`apps/gui-sonotron/src/spsc_ring.hpp`) and
`AudioMidiEvent` (`apps/gui-sonotron/src/audio_midi_event.hpp`) are, by
content, already regime-neutral: `<atomic>`/`<array>`/`<cstdint>` only, no
heap, `is_trivially_copyable_v` enforced, no exceptions. The ring's own
header comment says "Host-side glue only … never in the freestanding core"
— that is a claim about where it currently LIVES, not about what it
CONTAINS; nothing in the template requires a hosted OS. Palladio's layout
proposal (§1 of `audio-engines-family-layout.md`) moves both files under
`components/audio/include/audio/` — the platform-tier device-layer
directory, alongside the `ma_device` glue. Under the newer two-tier REGIME
axis this task is grounded in, that pairing conflates "generic bounded SPSC
ring" (core-capable content) with "owns a real playback device" (inherently
platform) inside the same directory. **Structural point, handed to
Palladio, not resolved here:** `SpscRing<T,N>` (and the `AudioMidiEvent` POD)
plausibly belong beside `components/runtime`'s own ring/queue primitives on
the `core` side, with only the *instantiation* (`AudioMidiRing`) and its
consumer (`AudioBackend`) staying platform-tier. This does not block the
`ISoundEngine` contract (the ring never appears in it, per §3), but it is
the same category of drift this proposal is built to catch, so it is named
rather than silently absorbed into Palladio's existing plan.

---

## 6. Promotion to freestanding/no-heap — concrete, load-bearing

What would ACTUALLY have to change, itemized:

1. **Render-callback signature.** Already shaped correctly and must stay
   that way verbatim: `void render(float* out, int frame_count) noexcept`
   — caller-provided buffer, no `std::vector`, no owned return, `noexcept`.
   `dispatch(const arrangrr::MidiMessage&)` likewise — 3-byte POD, no
   `std::variant`/`std::any`. This is already true in §1's draft; the
   discipline is "don't regress it," not "achieve it later."
2. **Virtual dispatch is NOT itself a blocker.** `-fno-rtti` (D3/D32,
   `components/arrangrr/CMakeLists.txt:27`) disables `dynamic_cast`/`typeid`,
   not virtual functions or vtables — a vtable costs flash bytes, not heap.
   DESIGN.md line 580 explicitly reserves `virtual` for "HAL boundaries,"
   and `ISoundEngine` is precisely that: an engine-selection HAL. Using
   `virtual` here is DOCTRINALLY CORRECT under the existing rule, not a new
   exception being requested.
3. **Which library relocates:** `components/core/audio_engine/` (the
   interface + the arm-smoke `NullSoundEngine` check from §4) moves from
   wherever it starts (host-only `else()` branch, per Palladio's placement
   caution) to the UNCONDITIONAL block at `CMakeLists.txt:41-52` — a
   physical `add_subdirectory` reorder, not a redesign, PROVIDED §4's
   arm-smoke check has been green the whole time (that's the entire point
   of adding it now instead of at promotion time).
4. **Which engines split, and how:** `soundfont`/`physical-models` never
   promote (§4 — permanent file-I/O dependency). `analog`, if/when
   implemented, moves from `components/platform/engines/analog` to
   `components/core/engines/analog` ONLY once its concrete implementation
   is independently audited bounded/no-heap/no-exception — the same
   "positive/CI-verified, not aspirational" bar DESIGN.md already applies
   to every other core feature. This is a second, later gate, not automatic
   just because it sits in the `core` directory.
5. **`AudioBackend` never promotes** (§4) — a hypothetical firmware audio
   path (STM32H743 I2S/DMA ISR) is a NEW, undesigned driver that would call
   the SAME `ISoundEngine::dispatch()/render()` virtual methods on a
   core-capable `analog` instance. The value of getting §1's signatures
   right now is exactly that this future driver needs no interface
   redesign, only a new caller.
6. **The float-on-STM32-realtime-path tension — flagged, owner-only.**
   DESIGN.md's hard rule #6 (line 64) states plainly: "Float allowed only
   where truly needed … **never in the STM32 realtime path**." That rule
   predates `0910`/D43 and was written when `arrangrr` was declared
   audio-ignorant by identity (`0110`: "MIDI-only, never audio"). An
   eventual on-device `analog` engine rendering `float* out` IS a new
   realtime path on the STM32H743, and DSP synthesis genuinely benefits
   from the part's hardware single-precision FPU rather than fighting it
   with fixed-point — but the rule as literally written forbids exactly
   that. **This is a real drift between a locked D-decision and the "big
   prize" vision the owner has now articulated, and it is not mine to
   resolve.** Two honest readings, either legitimate: (a) narrow rule #6's
   *scope* explicitly to the tick/scheduling path it was written for
   (leaving a DSP audio-render path as a distinct, newly-declared exception
   the H743 FPU justifies), or (b) hold the rule literally and require
   `analog`'s render path to be fixed/Q-format DSP even on-device. I flag
   this rather than silently picking a side — it is a doctrine call, not a
   structural one I can adjudicate from the tree alone.

---

## 7. ABI, coupling, and naming seams called out

- **Not an ABI change.** `ISoundEngine` never touches `arrangrr/abi.hpp`'s
  `Command`/`OutEvent` wire surface (unfrozen for Phase 5 per that file's own
  banner, but irrelevant here regardless) — it is a new, purely in-process
  C++ virtual interface, no serialization, no cross-process concern. The
  ABI-stability rules (append-only Param IDs, `sizeof` budgets) do not apply
  to it directly. Source-level stability across its three-plus
  implementations is still worth informal discipline (additive methods
  cheap, removing/reshaping one breaks every concrete engine at compile
  time) — noted, not enforced with the same machinery as the wire ABI.
- **Naming reconciliation owed to Palladio, not fixed here.** This task's
  framing (`components/platform/engines/soundfont-melodd`,
  `engines/physical`, `engines/analog`) does not match the already-written
  `docs/proposals/audio-engines-family-layout.md` (`components/audio-
  engines/{soundfont,physical-models,analog}`, flat `components/audio`).
  Both are internally coherent; picking one is a placement call, explicitly
  Palladio's, not mine — flagged so the two documents don't quietly diverge
  and both get treated as "the plan."
- **No new dependency anywhere in this proposal.** `ISoundEngine`/
  `AudioBackend` as specified add zero third-party dependencies — same
  `miniaudio`/`tinysoundfont` already vendored (`third_party/miniaudio`,
  `third_party/tinysoundfont`), same `Threads::Threads`. If a real `analog`
  implementation later wants an oscillator/filter DSP library instead of
  hand-rolled tables, THAT is a fresh `0800` dependency decision — flagged
  now as a likely future ask, not decided, not assumed.
- **Coupling this proposal removes:** `apps/gui-sonotron` currently is the
  only place `melodd::Synth` and `AudioEngine` are wired together
  (`main.cpp:519`, `audio_engine.cpp` constructor). After this seam,
  `apps/gui-sonotron`'s only remaining audio-specific knowledge is "which
  concrete `ISoundEngine` to construct and hand to `AudioBackend`" — the
  render/dispatch/panic triad itself becomes engine-blind. This is the
  actual point of the exercise and is a genuine coupling reduction, not
  ceremony, evidenced by §3's near-literal find/replace.

---

## 8. Keep / rework / owner sign-off

**Keep, unchanged:**
- The ring-based (not mutex-first) MIDI handoff shape, `AudioMidiEvent`
  as the boundary POD, the render-mutex triad discipline
  (`render`/config-swap/`all_notes_off`) — all already correct per
  `docs/phase6-design-reviews.md` and confirmed still current in the tree.
- `melodd`/`components/melodd` staying exactly where it is, WRAPPED not
  absorbed (Palladio's call, concurred with).
- `MidiMessage` (`components/common`) as the one shared vocabulary crossing
  the seam — no new event type needed beyond `AudioMidiEvent`.

**Rework (this proposal's actual content):**
- Extract `ISoundEngine` (§1) as a new interface, core-tier, host-agnostic
  signatures from day one.
- Rename/generalize `AudioEngine` → `AudioBackend` holding `ISoundEngine&`
  instead of an owned `melodd::Synth`; add `render_mutex()` accessor (§2).
- Add one new concrete adapter, `SoundfontEngine : ISoundEngine`, wrapping
  `melodd::Synth` (host-only, forever) — the melodd/TSF family's first
  member behind the seam.
- Add the arm-smoke compile-only check for the interface header (§4) —
  small, cheap, makes "dual-target-ready" a CI fact instead of a promise.

**Owner sign-off needed before Nazzareno executes:**
1. Reconcile this document's directory names against Palladio's
   `audio-engines-family-layout.md` (§7) — one naming scheme, not two.
2. Resolve §6 item 6 (float-on-STM32-realtime-path vs. rule #6) — narrow the
   rule's scope explicitly, or hold it and commit `analog` to fixed-point DSP.
   This gates what "core-capable" even means for `analog` once it exists.
3. Confirm hot-swap-at-runtime (soundfont ↔ analog without restart) is
   explicitly OUT of scope for the first slice (§2) — if wanted, the
   ownership model changes (`unique_ptr<ISoundEngine>` + swap protocol)
   and should be scoped before Nazzareno starts, not discovered mid-build.
4. Confirm the `render_mutex()` exposure pattern (§2) as the sanctioned way
   a concrete engine's own config API serializes against `render()` —
   this is the one place `AudioBackend`'s surface grows beyond "device
   layer," worth an explicit nod since it is the seam most likely to be
   copy-pasted incorrectly by a future third engine.

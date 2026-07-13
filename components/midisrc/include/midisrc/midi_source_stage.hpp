#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "common/time.hpp"
#include "midisrc/diagnostics.hpp"
#include "midisrc/file_io.hpp"
#include "midisrc/smf.hpp"
#include "runtime/out_scheduler.hpp"
#include "runtime/stage.hpp"

// The MIDI-source Stage adapter
// (docs/design/orchestrator-pipeline-extraction.md §16.5, Phase 4c): replays
// a Standard MIDI File's note events into the shared `OutScheduler`,
// participating in the SAME (tick, class, seq_no) D29 total order as every
// other stage (§16.2a) — zero bespoke merge logic. HOST-ONLY (file I/O,
// std::vector/std::string) — never cross-builds arm-none-eabi. Does not know
// about chorddet or arrangrr (D43): it only ever touches the `OutScheduler&`
// it is given, exactly the reference-injection idiom every other stage
// already uses.
//
// Design (fixed per §16.5): the file is opened and parsed ONCE, at
// construction, off the tick loop (heap freely used here — this is the
// "laptop tool" regime `DESIGN.md:6`'s float caveat already carves out for
// host-only code, never the realtime path). `on_tick` then only advances a
// plain index cursor comparing against `ctx.now` and calls `schedule(...)`
// for every event due at or before the current tick. `flush()` is a no-op:
// nothing of its own to drain — the shared scheduler is drained exactly once,
// by the arrangrr stage's own `flush()`, per Pipeline's flush design (§16.3).
//
// Forward-flow (Phase 4e, §3.6's "emits raw MIDI thru events AND feeds notes
// into" chorddet, runtime/pipeline.hpp's "forward-flow" header comment): the
// 3-arg `on_tick(ctx, sink, forward)` overload below additionally calls
// `forward(port, bytes, count)` for every due event, encoding the SAME
// `MidiMessage` already handed to the scheduler as its raw wire bytes
// (`status, d1, d2`) — `Pipeline` is the ONLY caller of this overload (this
// stage stays name-blind to chorddet, D43); the plain 2-arg overload
// (unchanged, still used directly by this component's own unit tests) simply
// supplies a no-op forward.
//
// cancel_note_off risk (§16.2(b), documented not fixed): this stage never
// calls `cancel_note_off` — a straight SMF replay has no ratchet/retrigger
// concept of its own, so the (port,channel,note)-blind tombstone risk
// documented there does not apply here. Give the melody thru its own
// port/channel, distinct from the band's, exactly per that same note.

namespace midisrc {

// One flat, tick-sorted wire event (note-on or note-off), ready to schedule.
struct SourceEvent {
  arrangrr::Tick tick = 0;
  arrangrr::MidiMessage msg{};
};

// Builds a flat, tick-sorted `SourceEvent` list from an already-parsed
// `SmfFile`, merging every track's notes and converting from the file's own
// PPQN division to the engine's fixed internal `arrangrr::kPpqn` resolution.
// Exposed standalone (not just as a Stage-constructor implementation detail)
// so it is independently unit-testable without a scheduler.
std::vector<SourceEvent> build_source_events(const arrstyle::SmfFile& file);

template <std::size_t N>
class MidiSourceStage {
 public:
  // Constructed inert (§16.5's "parse ONCE, off the tick loop" now split
  // into "construct once, load lazily", Phase 4d): `port` is this source's
  // own output port (keep it distinct from the band's port/channel,
  // §16.2(b)); no file is opened yet, so `ok()` is false and `on_tick` is a
  // no-op until `load()` succeeds. This lets a pipeline that ALWAYS declares
  // a MIDI-source stage (e.g. `hostrt::Shell`'s unified pipeline, driven
  // interactively or via the `midi-source load <path>` L1 verb, docs/design/
  // orchestrator-pipeline-extraction.md §16.7) stay byte-identical to one
  // without a MIDI-source stage at all when no file is ever loaded.
  explicit MidiSourceStage(arrangrr::OutScheduler<N>& scheduler, std::uint8_t port = 0) noexcept
      : m_scheduler(scheduler), m_port(port) {}

  // Opens and parses `path` right here (host-only, heap freely used, off any
  // tick loop — see the header comment above). `diag` collects parse
  // errors/warnings the same way every arrstyle importer already does.
  // Returns `ok()`'s new value; a failed load leaves any PREVIOUSLY loaded
  // events untouched-but-superseded (the cursor/event list are reset first,
  // so a failed reload cannot half-apply a new file over the old one).
  bool load(const std::string& path, arrstyle::Diagnostics& diag) {
    m_events.clear();
    m_cursor = 0;
    m_ok = false;
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!read_binary_file(path, bytes, error)) {
      diag.error(error, path);
      return false;
    }
    arrstyle::SmfFile file;
    if (!arrstyle::parse_smf(bytes, path, file, diag)) {
      return false;
    }
    m_events = build_source_events(file);
    m_ok = true;
    return true;
  }

  bool ok() const noexcept { return m_ok; }
  std::size_t event_count() const noexcept { return m_events.size(); }

  // Double-note guard (roadmap 9320, Restyle, docs/design/restyle-
  // placement.md §2): when a downstream RestyleStage owns the transformed
  // output, the raw "thru" replay scheduled below must be suppressed so the
  // SAME input note does not sound twice (once raw, once restyled). Additive
  // and defaults to true (byte-identical to every existing caller that never
  // calls this -- Accompany's own goldens are unaffected). Forward-flow
  // itself is untouched by this flag: chorddet/Restyle still need the raw
  // bytes even with the thru silenced.
  void set_thru_enabled(bool enabled) noexcept { m_thru_enabled = enabled; }
  bool thru_enabled() const noexcept { return m_thru_enabled; }

  // Plain 2-arg overload: forwards to the 3-arg one below with a no-op
  // `forward` — kept so this component's OWN unit tests (and anyone driving
  // this stage outside a Pipeline) never need to know forward-flow exists.
  template <typename SinkT>
  void on_tick(const runtime::StageContext& ctx, SinkT sink) {
    on_tick(ctx, sink, [](std::uint8_t, const std::uint8_t*, std::size_t) {});
  }

  // Forward-flow overload (Phase 4e, see the header comment): `Pipeline`
  // calls THIS one (SFINAE-detected, runtime/pipeline.hpp) so every due
  // event's raw wire bytes also reach a later chorddet-shaped stage, same
  // tick (D53) the thru note lands in the shared `OutScheduler`.
  template <typename SinkT, typename ForwardT>
  void on_tick(const runtime::StageContext& ctx, SinkT sink, ForwardT&& forward) {
    (void)sink;  // every note goes through the shared scheduler, not the sink
                 // directly (§16.2a) — mirrors every other stage's own
                 // reference-injection idiom.
    while (m_cursor < m_events.size() && m_events[m_cursor].tick <= ctx.now) {
      const arrangrr::MidiMessage& msg = m_events[m_cursor].msg;
      if (m_thru_enabled) {
        (void)m_scheduler.schedule(m_port, m_events[m_cursor].tick, msg);
      }
      const std::uint8_t wire[3] = {msg.status, msg.d1, msg.d2};
      forward(m_port, wire, static_cast<std::size_t>(msg.wire_length()));
      ++m_cursor;
    }
  }

  template <typename SinkT>
  void flush(SinkT sink) {
    (void)sink;  // nothing of its own to drain — see the header comment above.
  }

 private:
  arrangrr::OutScheduler<N>& m_scheduler;
  std::uint8_t m_port;
  std::vector<SourceEvent> m_events;  // tick-sorted, built once at construction
  std::size_t m_cursor = 0;
  bool m_ok = false;
  bool m_thru_enabled = true;  // roadmap 9320 double-note guard; on by default
};

}  // namespace midisrc

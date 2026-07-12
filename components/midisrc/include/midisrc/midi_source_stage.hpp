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
  // Opens and parses `path` right here (host-only, heap freely used, off any
  // tick loop — see the header comment above). `port` is this source's own
  // output port; keep it distinct from the band's port/channel (§16.2(b)).
  // `diag` collects parse errors/warnings the same way every arrstyle
  // importer already does; `ok()` reports whether the file loaded.
  MidiSourceStage(arrangrr::OutScheduler<N>& scheduler, std::uint8_t port, const std::string& path,
                  arrstyle::Diagnostics& diag)
      : m_scheduler(scheduler), m_port(port) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!read_binary_file(path, bytes, error)) {
      diag.error(error, path);
      return;
    }
    arrstyle::SmfFile file;
    if (!arrstyle::parse_smf(bytes, path, file, diag)) {
      return;
    }
    m_events = build_source_events(file);
    m_ok = true;
  }

  bool ok() const noexcept { return m_ok; }
  std::size_t event_count() const noexcept { return m_events.size(); }

  template <typename SinkT>
  void on_tick(const runtime::StageContext& ctx, SinkT sink) {
    (void)sink;  // every note goes through the shared scheduler, not the sink
                 // directly (§16.2a) — mirrors every other stage's own
                 // reference-injection idiom.
    while (m_cursor < m_events.size() && m_events[m_cursor].tick <= ctx.now) {
      (void)m_scheduler.schedule(m_port, m_events[m_cursor].tick, m_events[m_cursor].msg);
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
};

}  // namespace midisrc

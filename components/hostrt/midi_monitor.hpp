#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "arrangrr/abi.hpp"
#include "common/midi/message.hpp"

// Host-only MIDI monitor model (H2, docs/TUI_SPEC.md): normalized visible
// events, active-note tracking, the piano's visual event ring and duration
// formatting. Observes the host-visible OUTPUT event stream (every OutEvent
// the shell sink sees) — it does not see raw inputs that produce no output.
// All storage is bounded; no core involvement.

namespace arrangrr::host {

namespace monitor_limits {
inline constexpr std::size_t kMaxActiveNotes = 128;
inline constexpr std::size_t kLogCapacity = 256;
inline constexpr std::size_t kVisualEventCapacity = 32;
inline constexpr std::size_t kVisualEventRows = 5;
}  // namespace monitor_limits

namespace duration_ticks {
// PPQN = 960 (D27): exact musical duration labels.
inline constexpr std::uint32_t kQuarter = 960;
inline constexpr std::uint32_t kEighth = kQuarter / 2;
inline constexpr std::uint32_t kSixteenth = kQuarter / 4;
inline constexpr std::uint32_t kThirtySecond = kQuarter / 8;
}  // namespace duration_ticks

// "dur=240t 1/16" when exact, "dur=230t~1/16" when approximate (ASCII only).
std::string format_duration_ticks(std::uint32_t ticks);

// One normalized, filterable visible event.
struct MidiLogEvent {
  std::uint32_t tick = 0;
  std::uint32_t same_tick_index = 0;  // 0,1,2... among events sharing `tick`
  std::uint8_t port = 0;
  MidiMessage msg{};
};

enum class MidiEventKindFilter {
  kAny,
  kNoteOn,
  kNoteOff,
};

// Drums = GM percussion channel (ch10 1-based); melodic = everything else.
enum class InstrumentFilter {
  kAny,
  kDrums,
  kMelodic,
};

// Filters are data, never renderer logic. Empty optional = pass-through.
struct MidiEventFilter {
  std::optional<std::uint8_t> channel;  // 0-based internal
  std::optional<std::uint8_t> port;
  std::optional<std::uint8_t> velocity_min;  // note-ons below this are hidden
  MidiEventKindFilter event_kind = MidiEventKindFilter::kAny;
  InstrumentFilter instrument = InstrumentFilter::kAny;
};

bool filter_passes(const MidiEventFilter& filter, const MidiLogEvent& event);

struct MidiViewOptions {
  bool show_note_names = true;
  bool show_note_numbers = true;
  bool show_velocity = true;
  bool show_channel = true;
  bool show_port = true;
  bool show_drum_names = true;     // GM names on the percussion channel (H3)
  bool show_external_keys = true;  // light arranger/seq notes on the keyboard too
};

struct ActiveNote {
  std::uint8_t port = 0;
  std::uint8_t channel = 0;
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
  char source_key = 0;  // piano key that generated it, 0 = external
  std::uint32_t start_tick = 0;
};

// Bounded active-note set; a full tracker rejects (returns false) rather
// than growing — the warning is the caller's to surface.
class ActiveNoteTracker {
 public:
  [[nodiscard]] bool note_on(const ActiveNote& note);
  void note_off(std::uint8_t port, std::uint8_t channel, std::uint8_t note);
  void clear();
  std::size_t size() const;
  const std::array<ActiveNote, monitor_limits::kMaxActiveNotes>& notes() const;

 private:
  std::array<ActiveNote, monitor_limits::kMaxActiveNotes> m_notes{};
  std::size_t m_size = 0;
};

struct PianoVisualEvent {
  std::uint32_t start_tick = 0;
  std::uint32_t end_tick = 0;  // valid when !active
  std::uint8_t port = 0;
  std::uint8_t channel = 0;
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
  bool active = false;
  char source_key = 0;
};

// Bounded ring of recent piano-visible events; oldest rotates out.
class PianoVisualEventBuffer {
 public:
  void note_on(const PianoVisualEvent& event);
  void note_off(std::uint8_t port, std::uint8_t channel, std::uint8_t note, std::uint32_t end_tick);
  void clear();
  // Newest last; at most kVisualEventCapacity entries.
  std::vector<PianoVisualEvent> recent_events() const;

 private:
  std::array<PianoVisualEvent, monitor_limits::kVisualEventCapacity> m_events{};
  std::size_t m_next = 0;
  std::size_t m_size = 0;
};

// The monitor: observes host-visible OutEvents, keeps the bounded log,
// active notes and the visual ring in sync, tracks same-tick indices.
class MidiMonitor {
 public:
  // `source_key` annotates the piano key that caused the next matching
  // note-on (0 for everything else).
  void observe(const OutEvent& event, char source_key = 0);
  void clear();

  const ActiveNoteTracker& active_notes() const { return m_active; }
  const PianoVisualEventBuffer& visual_events() const { return m_visual; }
  // Filtered view over the bounded log, newest last.
  std::vector<MidiLogEvent> log_events(const MidiEventFilter& filter) const;
  bool tracker_overflowed() const { return m_overflowed; }

 private:
  std::array<MidiLogEvent, monitor_limits::kLogCapacity> m_log{};
  std::size_t m_log_next = 0;
  std::size_t m_log_size = 0;
  std::uint32_t m_last_tick = 0;
  std::uint32_t m_same_tick_count = 0;
  ActiveNoteTracker m_active;
  PianoVisualEventBuffer m_visual;
  bool m_overflowed = false;
};

}  // namespace arrangrr::host

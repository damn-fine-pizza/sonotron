#include "midi_monitor.hpp"

#include <array>
#include <cstdio>

#include "note_names.hpp"

namespace arrangrr::host {

namespace {

// A running note-on carries a non-zero velocity; 0x9x with velocity 0 is a
// note-off per the MIDI spec.
bool msg_is_note_on(const MidiMessage& msg) { return msg.type() == midi::kNoteOn && msg.d2 > 0; }

bool msg_is_note_off(const MidiMessage& msg) {
  return msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0);
}

std::uint32_t abs_diff(std::uint32_t a, std::uint32_t b) { return a > b ? a - b : b - a; }

// Exact musical duration labels, finest first so ties resolve to the finer
// value deterministically.
struct DurationLabel {
  std::uint32_t ticks;
  const char* text;
};

constexpr std::array<DurationLabel, 4> kDurationLabels = {{
    {duration_ticks::kThirtySecond, "1/32"},
    {duration_ticks::kSixteenth, "1/16"},
    {duration_ticks::kEighth, "1/8"},
    {duration_ticks::kQuarter, "1/4"},
}};

}  // namespace

std::string format_duration_ticks(std::uint32_t ticks) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "dur=%ut", static_cast<unsigned>(ticks));
  std::string out(buf);

  // Exact grid value: append the plain label.
  for (const DurationLabel& label : kDurationLabels) {
    if (ticks == label.ticks) {
      out += ' ';
      out += label.text;
      return out;
    }
  }

  // Above a quarter note we keep bare ticks: no coarser label to approximate.
  if (ticks > duration_ticks::kQuarter) {
    return out;
  }

  // Otherwise approximate to the nearest of the four labels.
  const DurationLabel* nearest = &kDurationLabels[0];
  std::uint32_t best = abs_diff(ticks, kDurationLabels[0].ticks);
  for (const DurationLabel& label : kDurationLabels) {
    const std::uint32_t d = abs_diff(ticks, label.ticks);
    if (d < best) {
      best = d;
      nearest = &label;
    }
  }

  out += '~';
  out += nearest->text;
  return out;
}

bool filter_passes(const MidiEventFilter& filter, const MidiLogEvent& event) {
  // Port applies to every event.
  if (filter.port.has_value() && *filter.port != event.port) {
    return false;
  }

  // Channel applies only to channel-voice messages; system/realtime traffic
  // has no channel and passes the channel test untouched.
  if (filter.channel.has_value() && midi::is_channel_voice(event.msg.status) &&
      event.msg.channel() != *filter.channel) {
    return false;
  }

  // Drums/melodic split follows the GM convention: percussion lives on ch10.
  if (filter.instrument != InstrumentFilter::kAny && midi::is_channel_voice(event.msg.status)) {
    const bool is_drums = event.msg.channel() == kGmDrumChannelZeroBased;
    if (filter.instrument == InstrumentFilter::kDrums && !is_drums) {
      return false;
    }
    if (filter.instrument == InstrumentFilter::kMelodic && is_drums) {
      return false;
    }
  }

  // The velocity floor hides soft note-ons; everything else is unaffected.
  if (filter.velocity_min.has_value() && msg_is_note_on(event.msg) &&
      event.msg.d2 < *filter.velocity_min) {
    return false;
  }

  switch (filter.event_kind) {
    case MidiEventKindFilter::kAny:
      return true;
    case MidiEventKindFilter::kNoteOn:
      return msg_is_note_on(event.msg);
    case MidiEventKindFilter::kNoteOff:
      return msg_is_note_off(event.msg);
  }

  return true;
}

bool ActiveNoteTracker::note_on(const ActiveNote& note) {
  // Retrigger of a held (port, channel, note) replaces the velocity: this is
  // not growth, so it always succeeds.
  for (std::size_t i = 0; i < m_size; ++i) {
    if (m_notes[i].port == note.port && m_notes[i].channel == note.channel &&
        m_notes[i].note == note.note) {
      m_notes[i].velocity = note.velocity;
      return true;
    }
  }

  if (m_size >= monitor_limits::kMaxActiveNotes) {
    return false;
  }

  m_notes[m_size] = note;
  ++m_size;
  return true;
}

void ActiveNoteTracker::note_off(std::uint8_t port, std::uint8_t channel, std::uint8_t note) {
  for (std::size_t i = 0; i < m_size; ++i) {
    if (m_notes[i].port == port && m_notes[i].channel == channel && m_notes[i].note == note) {
      // Shift the tail left to keep insertion order stable.
      for (std::size_t j = i; j + 1 < m_size; ++j) {
        m_notes[j] = m_notes[j + 1];
      }
      --m_size;
      return;
    }
  }
}

void ActiveNoteTracker::clear() {
  m_notes = {};
  m_size = 0;
}

std::size_t ActiveNoteTracker::size() const { return m_size; }

const std::array<ActiveNote, monitor_limits::kMaxActiveNotes>& ActiveNoteTracker::notes() const {
  return m_notes;
}

void PianoVisualEventBuffer::note_on(const PianoVisualEvent& event) {
  m_events[m_next] = event;
  m_next = (m_next + 1) % monitor_limits::kVisualEventCapacity;

  if (m_size < monitor_limits::kVisualEventCapacity) {
    ++m_size;
  }
}

void PianoVisualEventBuffer::note_off(std::uint8_t port, std::uint8_t channel, std::uint8_t note,
                                      std::uint32_t end_tick) {
  // Walk newest to oldest and close the most recent still-active match.
  for (std::size_t k = 1; k <= m_size; ++k) {
    const std::size_t idx =
        (m_next + monitor_limits::kVisualEventCapacity - k) % monitor_limits::kVisualEventCapacity;
    PianoVisualEvent& event = m_events[idx];
    if (event.active && event.port == port && event.channel == channel && event.note == note) {
      event.active = false;
      event.end_tick = end_tick;
      return;
    }
  }
}

void PianoVisualEventBuffer::clear() {
  m_events = {};
  m_next = 0;
  m_size = 0;
}

std::vector<PianoVisualEvent> PianoVisualEventBuffer::recent_events() const {
  std::vector<PianoVisualEvent> out;
  out.reserve(m_size);

  const std::size_t start = (m_next + monitor_limits::kVisualEventCapacity - m_size) %
                            monitor_limits::kVisualEventCapacity;
  for (std::size_t k = 0; k < m_size; ++k) {
    out.push_back(m_events[(start + k) % monitor_limits::kVisualEventCapacity]);
  }

  return out;
}

void MidiMonitor::observe(const OutEvent& event, char source_key) {
  // Only real MIDI output ever enters the log or the note models.
  if (event.kind != OutEvent::Kind::kMidi) {
    return;
  }

  // Consecutive events on the same tick get 0,1,2...; a new tick resets. The
  // very first event of all lands on index 0.
  if (m_log_size > 0 && event.tick == m_last_tick) {
    ++m_same_tick_count;
  } else {
    m_same_tick_count = 0;
  }
  m_last_tick = event.tick;

  MidiLogEvent log_event;
  log_event.tick = event.tick;
  log_event.same_tick_index = m_same_tick_count;
  log_event.port = event.port;
  log_event.msg = event.msg;

  m_log[m_log_next] = log_event;
  m_log_next = (m_log_next + 1) % monitor_limits::kLogCapacity;
  if (m_log_size < monitor_limits::kLogCapacity) {
    ++m_log_size;
  }

  if (msg_is_note_on(event.msg)) {
    ActiveNote note;
    note.port = event.port;
    note.channel = event.msg.channel();
    note.note = event.msg.d1;
    note.velocity = event.msg.d2;
    note.source_key = source_key;
    note.start_tick = event.tick;
    if (!m_active.note_on(note)) {
      m_overflowed = true;
    }

    PianoVisualEvent visual;
    visual.start_tick = event.tick;
    visual.port = event.port;
    visual.channel = event.msg.channel();
    visual.note = event.msg.d1;
    visual.velocity = event.msg.d2;
    visual.active = true;
    visual.source_key = source_key;
    m_visual.note_on(visual);
  } else if (msg_is_note_off(event.msg)) {
    m_active.note_off(event.port, event.msg.channel(), event.msg.d1);
    m_visual.note_off(event.port, event.msg.channel(), event.msg.d1, event.tick);
  }
}

void MidiMonitor::clear() {
  m_log = {};
  m_log_next = 0;
  m_log_size = 0;
  m_last_tick = 0;
  m_same_tick_count = 0;
  m_active.clear();
  m_visual.clear();
  m_overflowed = false;
}

std::vector<MidiLogEvent> MidiMonitor::log_events(const MidiEventFilter& filter) const {
  std::vector<MidiLogEvent> out;

  const std::size_t start =
      (m_log_next + monitor_limits::kLogCapacity - m_log_size) % monitor_limits::kLogCapacity;
  for (std::size_t k = 0; k < m_log_size; ++k) {
    const MidiLogEvent& event = m_log[(start + k) % monitor_limits::kLogCapacity];
    if (filter_passes(filter, event)) {
      out.push_back(event);
    }
  }

  return out;
}

}  // namespace arrangrr::host

#include "brain_event_from_outevent.hpp"

#include "common/midi/message.hpp"
#include "event_labels.hpp"
#include "note_names.hpp"

// See the header comment: this mirrors components/hostrt/jsonl.cpp's
// to_jsonl()/to_human() switch, field-for-field, but populates a
// sonotron::BrainEvent instead of formatting text -- both call the exact same
// arrangrr::host label helpers (event_labels.hpp), so a chord-quality suffix,
// a warn name, a section name etc. can never drift between the wire path and
// this in-process path.

namespace sonotron {

BrainEvent brain_event_from_outevent(const arrangrr::OutEvent& ev, bool prefer_flats) {
  using arrangrr::ChordQuality;
  using arrangrr::MidiMessage;
  using arrangrr::OutEvent;
  namespace host = arrangrr::host;

  BrainEvent out;
  out.valid = true;
  out.tick = static_cast<long>(ev.tick);

  switch (ev.kind) {
    case OutEvent::Kind::kSection:
      out.kind = BrainEvent::Kind::kSection;
      out.section_name = host::section_name(ev.code);
      break;

    case OutEvent::Kind::kChord: {
      out.kind = BrainEvent::Kind::kChord;
      const auto degree = static_cast<std::uint8_t>(ev.code & 0xFF);
      const auto quality = static_cast<ChordQuality>(ev.code >> 8);
      out.chord_in = host::note_name(
          ev.msg.status,
          {.naming = host::NoteNaming::kCde, .prefer_flats = prefer_flats, .include_octave = true});
      out.chord_out = host::pitch_class_name(ev.msg.status, {.naming = host::NoteNaming::kCde,
                                                             .prefer_flats = prefer_flats,
                                                             .include_octave = false}) +
                      host::quality_suffix(quality);
      out.chord_deg = host::roman_degree(degree, quality);
      break;
    }

    case OutEvent::Kind::kMidi: {
      out.kind = BrainEvent::Kind::kMidiOut;
      out.port = ev.port;
      const MidiMessage& m = ev.msg;
      if (arrangrr::midi::is_realtime(m.status)) {
        out.msg = host::realtime_name(m.status);
        break;
      }
      switch (m.type()) {
        case arrangrr::midi::kNoteOn:
          out.msg = "noteon";
          break;
        case arrangrr::midi::kNoteOff:
          out.msg = "noteoff";
          break;
        case arrangrr::midi::kControlChange:
          out.msg = "cc";
          break;
        case arrangrr::midi::kProgramChange:
          out.msg = "program";
          break;
        case arrangrr::midi::kPitchBend:
          out.msg = "pitchbend";
          break;
        default:
          out.msg = "raw";
          break;
      }
      break;
    }

    case OutEvent::Kind::kChordFollowed: {
      out.kind = BrainEvent::Kind::kChordFollowed;
      const auto cur = host::followed_state(ev.msg.status, (ev.msg.d2 & 0x1) != 0);
      const auto next = host::followed_state(ev.msg.d1, (ev.msg.d2 & 0x2) != 0);
      const auto src = static_cast<std::uint8_t>((ev.msg.d2 >> 2) & 0x3);
      out.followed_current = host::followed_label(cur, prefer_flats);
      out.followed_current_pcs = host::followed_pcs(cur);
      out.followed_next = host::followed_label(next, prefer_flats);
      out.followed_next_pcs = host::followed_pcs(next);
      out.followed_source = host::producer_name(src);
      break;
    }

    case OutEvent::Kind::kBeat:
      out.kind = BrainEvent::Kind::kBeat;
      out.beat_bar = ev.code;
      out.beat_index = ev.msg.status;
      out.beat_pulse = ev.msg.d1;
      break;

    case OutEvent::Kind::kTransport:
      out.kind = BrainEvent::Kind::kTransport;
      out.transport_state = host::transport_name(ev.code);
      break;

    case OutEvent::Kind::kWarn:
    default:
      out.kind = BrainEvent::Kind::kWarn;
      out.warn_code = host::warn_name(ev.code);
      break;
  }

  return out;
}

}  // namespace sonotron

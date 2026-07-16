#include "brain_event_from_outevent.hpp"

#include "common/midi/message.hpp"
#include "event_labels.hpp"
#include "note_names.hpp"

// See the header comment: this mirrors components/platform/hostrt/jsonl.cpp's
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

    case OutEvent::Kind::kClip:
      out.kind = BrainEvent::Kind::kClip;
      out.clip_id = ev.code;
      out.clip_state = host::clip_state_name(ev.msg.status);
      break;

    // Phase 7 (node 6000, the Looper -- docs/proposals/looper-in-gui-
    // contract.md §7 item 9): `ev.code` is the target LoopBuffer slot id
    // (abi.hpp:437-441), NOT a WarnCode -- this dedicated case, mirroring
    // the kClip precedent above, is what keeps it from falling through to
    // the kWarn/default branch below and being misdecoded as a fabricated
    // warning.
    case OutEvent::Kind::kLoop:
      out.kind = BrainEvent::Kind::kLoop;
      out.loop_slot_id = ev.code;
      out.loop_event_kind = host::loop_event_kind_name(ev.msg.status);
      break;

    case OutEvent::Kind::kTransport:
      out.kind = BrainEvent::Kind::kTransport;
      out.transport_state = host::transport_name(ev.code);
      break;

    // Phase 7 (node T0, the variable time-signature engine): `ev.code` is the
    // CURRENT beats_per_bar (abi.hpp:592), NOT a WarnCode -- this dedicated
    // case, mirroring the kClip/kLoop precedent above, is what keeps it from
    // falling through to the kWarn/default branch below and being misdecoded
    // as a fabricated warning.
    case OutEvent::Kind::kTimeSig:
      out.kind = BrainEvent::Kind::kTimeSig;
      out.time_sig_beats_per_bar = ev.code;
      break;

    case OutEvent::Kind::kParamState:
      // Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b): the
      // GUI does not decode this echo yet (its own panel-mirror-struct wiring
      // is future work, mirroring hostrt's Seam D) -- treat it exactly like a
      // line the GUI does not model, matching parse_brain_event's own
      // "unmodeled" convention (kind=kUnknown, invalid), NOT kWarn (ev.code
      // here is a Param id, not a WarnCode -- falling through to the kWarn
      // branch below would render a bogus "unknown" warning).
      out.kind = BrainEvent::Kind::kUnknown;
      out.valid = false;
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

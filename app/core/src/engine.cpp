#include "arrangrr/engine.hpp"

// Binary-ABI command handling (D26), split per domain: the dispatch is a
// ten-line switch, each family owns its validation and warns.

namespace arrangrr {

void Engine::push_command(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kTransportTempo:
    case Param::kTransportStart:
    case Param::kTransportStop:
    case Param::kTransportContinue:
      cmd_transport(cmd, sink);
      break;
    case Param::kPanic:
    case Param::kRouteAdd:
    case Param::kRouteClear:
    case Param::kClockOutMask:
      cmd_routing(cmd, sink);
      break;
    case Param::kKeySet:
    case Param::kChordPlay:
    case Param::kChordStop:
    case Param::kChordHold:
    case Param::kChordOut:
    case Param::kChordMode:
      cmd_chord(cmd, sink);
      break;
    case Param::kSeqNew:
    case Param::kSeqUse:
    case Param::kSeqRec:
    case Param::kSeqAdd:
    case Param::kSeqLoop:
    case Param::kSeqPlay:
    case Param::kSeqStop:
    case Param::kSeqTranspose:
    case Param::kSeqDel:
    case Param::kSeqClear:
      cmd_seq(cmd, sink);
      break;
    case Param::kTrackNew:
    case Param::kTrackStep:
    case Param::kTrackLength:
    case Param::kTrackMute:
    case Param::kTrackSolo:
      cmd_track(cmd, sink);
      break;
    case Param::kStyleLoad:
    case Param::kStyleSection:
    case Param::kStyleRoute:
      cmd_style(cmd, sink);
      break;
    default:
      sink(OutEvent::warn(WarnCode::kUnknownCommand, m_now));
      break;
  }
}

void Engine::cmd_transport(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kTransportTempo:
      if (!m_transport.set_bpm(static_cast<BpmX100>(cmd.a))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kTransportStart:
      m_transport.start();
      emit_realtime(midi::kStart, sink);
      // MIDI convention: the first F8 follows FA immediately — the slave's
      // beat zero is this clock, not one period later.
      emit_realtime(midi::kClock, sink);
      if (m_seq.playing()) {
        (void)m_seq.play(0);  // rebase to the new tick 0
      }
      m_arranger.on_transport_start();
      fire_timeline(0, sink);  // grid step 0 plays on start, like the F8
      fire_chord_seq(0, sink);
      fire_arranger(0, sink);
      flush(sink);
      sink(OutEvent::transport(static_cast<std::uint16_t>(m_transport.state()), m_now));
      break;
    case Param::kTransportStop:
      m_transport.stop();
      if (m_seq.playing()) {
        m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, m_now, msg, sink);
        });
        flush(sink);
      }
      emit_realtime(midi::kStop, sink);
      sink(OutEvent::transport(static_cast<std::uint16_t>(m_transport.state()), m_now));
      break;
    case Param::kTransportContinue:
    default:
      m_transport.resume();
      emit_realtime(midi::kContinue, sink);
      sink(OutEvent::transport(static_cast<std::uint16_t>(m_transport.state()), m_now));
      break;
  }
}

void Engine::cmd_routing(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kPanic:
      m_tracker.panic([&](std::uint8_t port, const MidiMessage& msg) {
        schedule_or_warn(port, m_now, msg, sink);
      });
      flush(sink);
      break;
    case Param::kRouteAdd: {
      const Route route{
          .in_port = static_cast<std::uint8_t>(cmd.a & 0xFF),
          .in_channel = static_cast<std::int8_t>((cmd.a >> 8) & 0xFF),
          .out_port = static_cast<std::uint8_t>(cmd.b & 0xFF),
          .out_channel = static_cast<std::int8_t>((cmd.b >> 8) & 0xFF),
          .pass = static_cast<std::uint8_t>(cmd.c & 0xFF),
      };
      if (route.in_port >= kMaxPorts || route.out_port >= kMaxPorts) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else if (!m_router.add(route)) {
        sink(OutEvent::warn(WarnCode::kRouteTableFull, m_now));
      }
      break;
    }
    case Param::kRouteClear:
      m_router.clear();
      break;
    case Param::kClockOutMask:
    default:
      m_clock_out_mask = static_cast<std::uint8_t>(cmd.a & 0xFF);
      break;
  }
}

void Engine::cmd_chord(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kKeySet:
      if (cmd.a < 0 || cmd.a > 11 || cmd.b < 0 || cmd.b >= kModeCount) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else {
        m_chords.set_key(Key{static_cast<std::uint8_t>(cmd.a), static_cast<Mode>(cmd.b)});
      }
      break;
    case Param::kChordOut: {
      const auto port = static_cast<std::uint8_t>(cmd.a & 0xFF);
      const auto channel = static_cast<std::uint8_t>((cmd.a >> 8) & 0xFF);
      if (port >= kMaxPorts || channel > 15) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        break;
      }
      // Release the sounding voicing on the OLD destination first, or its
      // NoteOffs would chase the chord onto the new port and strand it.
      if (m_chords.sounding()) {
        m_chords.release([&](std::uint8_t p, const MidiMessage& msg) {
          schedule_or_warn(p, m_now, msg, sink);
        });
        flush(sink);
      }
      m_chords.set_output(port, channel);
      break;
    }
    case Param::kChordHold:
      // Reserved by the ABI, not implemented yet (live-keyboard gestures):
      // refusing honestly beats nodding and doing nothing.
      sink(OutEvent::warn(WarnCode::kUnsupported, m_now));
      break;
    case Param::kChordMode:
      if (cmd.a < 0 || cmd.a >= kChordModeCount) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else {
        m_chords.set_mode(static_cast<ChordMode>(cmd.a));
      }
      break;
    case Param::kChordPlay: {
      const auto vel = static_cast<std::uint8_t>(cmd.c);
      // Up to 4 packed notes, zero-terminated (one per byte).
      std::uint8_t notes[4];
      std::uint8_t note_count = 0;
      for (int i = 0; i < 4; ++i) {
        const auto n = static_cast<std::uint8_t>((cmd.a >> (8 * i)) & 0xFF);
        if (n == 0) {
          break;
        }
        if (n > 127) {
          note_count = 0;
          break;
        }
        notes[note_count++] = n;
      }
      if (note_count == 0 || vel == 0 || vel > 127 || cmd.b >= kQualityCount) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        break;
      }
      const auto schedule = [&](std::uint8_t port, const MidiMessage& msg) {
        schedule_or_warn(port, m_now, msg, sink);
      };
      ChordResult r;
      switch (m_chords.mode()) {
        case ChordMode::kSingle:
          r = m_chords.play_single(notes[0], static_cast<std::int8_t>(cmd.b), vel, schedule);
          break;
        case ChordMode::kShell:
          r = m_chords.play_shell(notes, note_count, static_cast<std::int8_t>(cmd.b), vel,
                                  schedule);
          break;
        case ChordMode::kDiatonic:
        default:
          r = m_chords.play(notes[0], static_cast<std::int8_t>(cmd.b), vel, schedule);
          break;
      }
      if (r.degree < 0) {
        sink(OutEvent::warn(WarnCode::kNotInKey, m_now));
        break;
      }
      // Recording captures only diatonic degrees (D28 functional storage);
      // keyless modes record when the root happens to fit the seq key.
      if (m_seq.recording()) {
        const int deg = r.degree == static_cast<std::int8_t>(kNoDegree)
                            ? theory::degree_of(m_seq.current()->key,
                                                static_cast<std::uint8_t>(r.root_note % 12))
                            : r.degree;
        if (deg >= 0) {
          const std::int8_t ovr =
              r.degree == static_cast<std::int8_t>(kNoDegree)
                  ? static_cast<std::int8_t>(r.quality)  // pin the resolved quality
                  : static_cast<std::int8_t>(cmd.b);
          m_seq.capture(m_now, static_cast<std::int8_t>(deg), ovr, vel);
        } else {
          sink(OutEvent::warn(WarnCode::kNotInKey, m_now));
        }
      }
      sink(OutEvent::chord(m_chords.out_port(), static_cast<std::uint8_t>(r.degree),
                           static_cast<std::uint8_t>(r.quality), r.root_note, r.shape.count, vel,
                           m_now));
      flush(sink);
      break;
    }
    case Param::kChordStop:
    default:
      m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
        schedule_or_warn(port, m_now, msg, sink);
      });
      flush(sink);
      break;
  }
}

void Engine::cmd_seq(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kSeqNew:
      if (m_seq.add_sequence(m_chords.key()) < 0) {
        sink(OutEvent::warn(WarnCode::kSeqTableFull, m_now));
      }
      break;
    case Param::kSeqUse:
      if (!m_seq.use(cmd.idx)) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kSeqRec:
      if (!m_seq.start_record(m_now)) {
        sink(OutEvent::warn(WarnCode::kSeqEmpty, m_now));
      }
      break;
    case Param::kSeqStop:
      if (m_seq.recording()) {
        (void)m_seq.stop_record(m_now, cmd.a > 0 ? static_cast<Tick>(cmd.a) : kTicksPerBar);
      } else {
        m_seq.stop_playback([&] {
          m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
        });
        flush(sink);
      }
      break;
    case Param::kSeqAdd: {
      ChordSequence* seq = m_seq.current();
      const auto note = static_cast<std::uint8_t>(cmd.a & 0x7F);
      const std::int8_t quality_ovr = static_cast<std::int8_t>((cmd.b & 0xFF) - 1);
      const auto vel = static_cast<std::uint8_t>((cmd.b >> 8) & 0x7F);
      if (seq == nullptr || cmd.a < 0 || cmd.a > 127 || cmd.c <= 0 || vel == 0 ||
          quality_ovr >= static_cast<std::int8_t>(kQualityCount)) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        break;
      }
      const int degree = theory::degree_of(seq->key, static_cast<std::uint8_t>(note % 12));
      if (degree < 0) {
        sink(OutEvent::warn(WarnCode::kNotInKey, m_now));
      } else if (!seq->append(static_cast<std::int8_t>(degree), quality_ovr, vel,
                              static_cast<Tick>(cmd.c))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    }
    case Param::kSeqLoop:
      if (ChordSequence* seq = m_seq.current(); seq != nullptr) {
        seq->loop = cmd.a != 0;
      } else {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kSeqPlay:
      if (!m_seq.play(m_transport.tick())) {
        sink(OutEvent::warn(WarnCode::kSeqEmpty, m_now));
      } else if (m_transport.playing()) {
        fire_chord_seq(m_transport.tick(), sink);
        flush(sink);
      }
      break;
    case Param::kSeqTranspose:
      if (ChordSequence* seq = m_seq.current(); seq == nullptr) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else if (cmd.a >= 0) {
        if (cmd.a > 11 || cmd.b >= kModeCount) {
          sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        } else {
          seq->transpose_to(static_cast<std::uint8_t>(cmd.a), static_cast<std::int8_t>(cmd.b));
        }
      } else {
        seq->transpose_by(static_cast<std::int8_t>(cmd.c));
      }
      break;
    case Param::kSeqDel:
      if (ChordSequence* seq = m_seq.current();
          seq == nullptr || !seq->remove(static_cast<std::size_t>(cmd.a))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kSeqClear:
    default:
      if (ChordSequence* seq = m_seq.current(); seq != nullptr) {
        seq->clear();
      } else {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
  }
}

void Engine::cmd_track(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kTrackNew: {
      const auto port = static_cast<std::uint8_t>(cmd.b & 0xFF);
      const auto channel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
      if (port >= kMaxPorts || channel > 15 || cmd.a > static_cast<std::int32_t>(TrackRole::kCc)) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else if (m_timeline.add_track(static_cast<TrackRole>(cmd.a), port, channel) < 0) {
        sink(OutEvent::warn(WarnCode::kTrackTableFull, m_now));
      }
      break;
    }
    case Param::kTrackStep: {
      const auto note = static_cast<std::uint8_t>(cmd.b & 0xFF);
      const auto vel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
      if (!m_timeline.set_step(cmd.idx, static_cast<std::size_t>(cmd.a), note, vel,
                               static_cast<std::uint16_t>(cmd.c))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    }
    case Param::kTrackLength:
      if (!m_timeline.set_length(cmd.idx, static_cast<std::size_t>(cmd.a))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kTrackMute:
    case Param::kTrackSolo:
    default: {
      Track* t = m_timeline.track(cmd.idx);
      if (t == nullptr) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else if (cmd.param == Param::kTrackMute) {
        t->mute = cmd.a != 0;
      } else {
        t->solo = cmd.a != 0;
      }
      break;
    }
  }
}

void Engine::cmd_style(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kStyleLoad:
      if (cmd.a < 0 || !m_arranger.load(static_cast<std::uint8_t>(cmd.a))) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    case Param::kStyleSection:
      if (cmd.a < 0 || cmd.a >= kSectionTypeCount ||
          !m_arranger.request(static_cast<SectionType>(cmd.a), !m_transport.playing())) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else if (!m_transport.playing()) {
        sink(OutEvent::section(static_cast<std::uint16_t>(m_arranger.current()), m_now));
      }
      break;
    case Param::kStyleRoute:
    default: {
      const auto port = static_cast<std::uint8_t>(cmd.b & 0xFF);
      const auto channel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
      if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(TrackRole::kCc) ||
          !m_arranger.set_route(static_cast<TrackRole>(cmd.a), port, channel)) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      }
      break;
    }
  }
}

}  // namespace arrangrr

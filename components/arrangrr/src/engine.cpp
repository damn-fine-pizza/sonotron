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
    case Param::kChordDetect:
    case Param::kChordFollow:
    case Param::kInputZone:
      cmd_chord(cmd, sink);
      break;
    case Param::kArp:
    case Param::kArpOut:
      cmd_arp(cmd, sink);
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
    case Param::kStyleSwitch:
    case Param::kPartMute:
    case Param::kPartSolo:
    case Param::kGroove:
      cmd_style(cmd, sink);
      break;
    case Param::kProgram:
      cmd_voice(cmd, sink);
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
      // The band starts in the home key WITHOUT clobbering an explicit chord:
      // establish_default is a no-op once any producer has steered a real chord,
      // so a manual/detected chord survives transport-start. Drop any staged
      // shift chord so bar 0 plays with `next` empty. A time-aligned producer
      // that fires on tick 0 (the ChordSequencer below) still overrides this.
      m_chords.establish_default();
      m_chords.reset_pending();
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
      m_detector.clear();  // every key is up now; the latched chord stays (memory)
      m_arp.panic();       // drop any held/latched arp notes
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
      chord_key_set(cmd, sink);
      break;
    case Param::kChordOut:
      chord_out(cmd, sink);
      break;
    case Param::kChordHold:
      // Reserved by the ABI, not implemented yet (live-keyboard gestures):
      // refusing honestly beats nodding and doing nothing.
      sink(OutEvent::warn(WarnCode::kUnsupported, m_now));
      break;
    case Param::kChordMode:
      chord_mode(cmd, sink);
      break;
    case Param::kChordDetect:
      chord_detect_cmd(cmd, sink);
      break;
    case Param::kChordFollow:
      chord_follow_cmd(cmd, sink);
      break;
    case Param::kInputZone:
      chord_input_zone(cmd, sink);
      break;
    case Param::kChordPlay:
      chord_play(cmd, sink);
      break;
    case Param::kChordStop:
    default:
      m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
        schedule_or_warn(port, m_now, msg, sink);
      });
      flush(sink);
      break;
  }
}

void Engine::chord_key_set(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || cmd.a > 11 || cmd.b < 0 || cmd.b >= kModeCount) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const Key key{.root_pc = static_cast<std::uint8_t>(cmd.a), .mode = static_cast<Mode>(cmd.b)};
  m_chords.set_key(key);
  m_detector.set_key(key);  // scale-aware single-finger reads the same key
}

void Engine::chord_out(const Command& cmd, EventSink sink) {
  const auto port = static_cast<std::uint8_t>(cmd.a & 0xFF);
  const auto channel = static_cast<std::uint8_t>((cmd.a >> 8) & 0xFF);
  if (port >= kMaxPorts || channel > 15) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  // Release the sounding voicing on the OLD destination first, or its
  // NoteOffs would chase the chord onto the new port and strand it.
  if (m_chords.sounding()) {
    m_chords.release(
        [&](std::uint8_t p, const MidiMessage& msg) { schedule_or_warn(p, m_now, msg, sink); });
    flush(sink);
  }
  m_chords.set_output(port, channel);
}

void Engine::chord_mode(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || cmd.a >= kChordModeCount) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto mode = static_cast<ChordMode>(cmd.a);
  m_chords.set_mode(mode);
  // "single" is single-finger everywhere: the typed chord path already
  // reads only the root (play_single), and the live piano detector drops
  // its minimum to one held note so a lone key steers the band; the other
  // modes keep the fingered triad minimum. The detector also flips into
  // scale-aware single-finger so a lone key resolves the diatonic maj/min
  // triad of its root (Dxx), matching the typed play_single path.
  const bool single = mode == ChordMode::kSingle;
  m_detector.set_min_notes(single ? 1 : kMinChordNotes);
  m_detector.set_single_finger(single);
}

void Engine::chord_detect_cmd(const Command& cmd, EventSink sink) {
  // Live piano->chord: a = 0/1 enable, b = input port (default 0). The
  // held notes on that port re-harmonize the arranger in real time.
  const std::int32_t port = cmd.b;
  if (port < 0 || static_cast<std::size_t>(port) >= kMaxPorts) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  set_chord_detect(cmd.a != 0, static_cast<std::uint8_t>(port));
}

void Engine::chord_follow_cmd(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(ChordFollow::kLivePriority)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  set_chord_follow(static_cast<ChordFollow>(cmd.a));
}

void Engine::chord_input_zone(const Command& cmd, EventSink sink) {
  // Dxx: a = input port, b = InputZone. kHarmony silences that port's notes
  // (silent chord recognition); kMelody routes/sounds. Whole-port decision,
  // no pitch split yet.
  if (cmd.a < 0 || static_cast<std::size_t>(cmd.a) >= kMaxPorts || cmd.b < 0 ||
      cmd.b > static_cast<std::int32_t>(InputZone::kHarmony)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  set_input_zone(static_cast<std::uint8_t>(cmd.a), static_cast<InputZone>(cmd.b));
}

void Engine::chord_play(const Command& cmd, EventSink sink) {
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
    return;
  }
  const auto schedule = [&](std::uint8_t port, const MidiMessage& msg) {
    schedule_or_warn(port, m_now, msg, sink);
  };
  // Manual `chord play` always SOUNDS its notes and always attempts to steer;
  // the owner's D47 gate decides whether Producer::kManual actually publishes.
  // Immediate by default (`current` changes now); a shift/quantized variant
  // (idx != 0) STAGES it for the next bar like a SHIFT-note, additive and POD.
  const bool steer = true;
  const bool quantize = cmd.idx != 0;
  ChordResult r;
  switch (m_chords.mode()) {
    case ChordMode::kSingle:
      r = m_chords.play_single(notes[0], static_cast<std::int8_t>(cmd.b), vel, schedule, steer,
                               quantize);
      break;
    case ChordMode::kShell:
      r = m_chords.play_shell(notes, note_count, static_cast<std::int8_t>(cmd.b), vel, schedule,
                              steer, quantize);
      break;
    case ChordMode::kDiatonic:
    default:
      r = m_chords.play(notes[0], static_cast<std::int8_t>(cmd.b), vel, schedule, steer, quantize);
      break;
  }
  if (r.degree < 0) {
    sink(OutEvent::warn(WarnCode::kNotInKey, m_now));
    return;
  }
  // Recording captures only diatonic degrees (D28 functional storage);
  // keyless modes record when the root happens to fit the seq key.
  if (m_seq.recording()) {
    const int deg =
        r.degree == static_cast<std::int8_t>(kNoDegree)
            ? theory::degree_of(m_seq.current()->key, static_cast<std::uint8_t>(r.root_note % 12))
            : r.degree;
    if (deg >= 0) {
      const std::int8_t ovr = r.degree == static_cast<std::int8_t>(kNoDegree)
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
  // Announce the followed-context change (immediate commit or a shift-staged
  // next). The D47 gate may have made the steer a no-op; the delta check inside
  // keeps this silent then.
  emit_chord_followed(Producer::kManual, sink);
  flush(sink);
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
      seq_stop(cmd, sink);
      break;
    case Param::kSeqAdd:
      seq_add(cmd, sink);
      break;
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
      seq_transpose(cmd, sink);
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

void Engine::seq_stop(const Command& cmd, EventSink sink) {
  if (m_seq.recording()) {
    (void)m_seq.stop_record(m_now, cmd.a > 0 ? static_cast<Tick>(cmd.a) : kTicksPerBar);
    return;
  }
  m_seq.stop_playback([&] {
    m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
      schedule_or_warn(port, m_now, msg, sink);
    });
  });
  flush(sink);
}

void Engine::seq_add(const Command& cmd, EventSink sink) {
  ChordSequence* seq = m_seq.current();
  const auto note = static_cast<std::uint8_t>(cmd.a & 0x7F);
  const std::int8_t quality_ovr = static_cast<std::int8_t>((cmd.b & 0xFF) - 1);
  const auto vel = static_cast<std::uint8_t>((cmd.b >> 8) & 0x7F);
  if (seq == nullptr || cmd.a < 0 || cmd.a > 127 || cmd.c <= 0 || vel == 0 ||
      quality_ovr >= static_cast<std::int8_t>(kQualityCount)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const int degree = theory::degree_of(seq->key, static_cast<std::uint8_t>(note % 12));
  if (degree < 0) {
    sink(OutEvent::warn(WarnCode::kNotInKey, m_now));
  } else if (!seq->append(static_cast<std::int8_t>(degree), quality_ovr, vel,
                          static_cast<Tick>(cmd.c))) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

void Engine::seq_transpose(const Command& cmd, EventSink sink) {
  ChordSequence* seq = m_seq.current();
  if (seq == nullptr) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  if (cmd.a < 0) {
    seq->transpose_by(static_cast<std::int8_t>(cmd.c));
    return;
  }
  if (cmd.a > 11 || cmd.b >= kModeCount) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  seq->transpose_to(static_cast<std::uint8_t>(cmd.a), static_cast<std::int8_t>(cmd.b));
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
      const auto b = static_cast<std::uint32_t>(cmd.b);
      const auto c = static_cast<std::uint32_t>(cmd.c);
      const auto note = static_cast<std::uint8_t>(b & 0xFF);
      const auto vel = static_cast<std::uint8_t>((b >> 8) & 0xFF);
      const auto gate = static_cast<std::uint16_t>(c & 0xFFFF);
      // Param-locks ride the free high bits, opt-in via bit 31 of c. When the
      // flag is clear (the original short form) the neutral defaults apply, so
      // existing kTrackStep commands are byte-identical.
      std::uint8_t probability = 100;
      std::uint8_t ratchet = 1;
      std::uint8_t micro = 0;  // forward-only lay-back (0..127)
      bool tie = false;
      if ((c & 0x80000000u) != 0) {
        probability = static_cast<std::uint8_t>((b >> 16) & 0xFF);
        ratchet = static_cast<std::uint8_t>((b >> 24) & 0x0F);
        tie = ((b >> 28) & 0x1) != 0;
        micro = static_cast<std::uint8_t>((c >> 16) & 0xFF);
      }
      if (!m_timeline.set_step(cmd.idx, static_cast<std::size_t>(cmd.a), note, vel, gate,
                               probability, ratchet, micro, tie)) {
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
      } else {
        apply_arranger_voices(sink);  // pick the style's default voices
        apply_style_tempo();          // 9120: adopt the style's default tempo
        // Owner decision: loading a style changes the BAND, keeps the HARMONY.
        // establish_default seeds the home key only when nothing explicit is in
        // force, so a chord the user steered persists across a style load; the
        // staged shift chord is dropped.
        m_chords.establish_default();
        m_chords.reset_pending();
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
    case Param::kStyleSwitch:
      style_switch(cmd, sink);
      break;
    case Param::kPartMute:
    case Param::kPartSolo: {
      if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(TrackRole::kCc)) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        break;
      }
      const auto role = static_cast<TrackRole>(cmd.a);
      if (cmd.param == Param::kPartMute) {
        m_arranger.set_mute(role, cmd.b != 0);
      } else {
        m_arranger.set_solo(role, cmd.b != 0);
      }
      break;
    }
    case Param::kGroove:
      if (cmd.a < 0 || cmd.a >= kGrooveFieldCount) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else {
        m_arranger.set_groove_field(static_cast<GrooveField>(cmd.a), cmd.b);
      }
      break;
    case Param::kStyleRoute:
    default:
      style_route(cmd, sink);
      break;
  }
}

void Engine::style_switch(const Command& cmd, EventSink sink) {
  // A combined style + section switch (D24). Immediate on explicit request
  // (CTRL+\ "now") or whenever the transport is stopped — a queued switch
  // could never land without ticks; otherwise it rides the next bar
  // boundary (ENTER "next-bar"), matching kStyleSection's quantization.
  const bool immediate = cmd.c != 0 || !m_transport.playing();
  const bool ok = cmd.a >= 0 && cmd.a < static_cast<std::int32_t>(styles::kBuiltinCount) &&
                  cmd.b >= 0 && cmd.b < kSectionTypeCount &&
                  m_arranger.request_style(styles::kBuiltins[static_cast<std::uint8_t>(cmd.a)],
                                           static_cast<SectionType>(cmd.b), immediate);
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  if (immediate) {
    sink(OutEvent::section(static_cast<std::uint16_t>(m_arranger.current()), m_now));
    apply_arranger_voices(sink);  // the new style's voices land with the cut
    apply_style_tempo();          // 9120: the new style's tempo lands with the cut
  }
}

void Engine::style_route(const Command& cmd, EventSink sink) {
  const auto port = static_cast<std::uint8_t>(cmd.b & 0xFF);
  const auto channel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
  if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(TrackRole::kCc) ||
      !m_arranger.set_route(static_cast<TrackRole>(cmd.a), port, channel)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  apply_arranger_voices(sink);  // a role just gained a route: voice it now
}

// Voice selection: a Program Change on a port+channel so the arranger (or the
// user) picks the GM instrument, instead of leaving the timbre to the synth.
void Engine::cmd_voice(const Command& cmd, EventSink sink) {
  if (cmd.param != Param::kProgram) {
    sink(OutEvent::warn(WarnCode::kUnknownCommand, m_now));
    return;
  }
  const auto port = static_cast<std::uint8_t>(cmd.b & 0xFF);
  const auto channel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
  if (cmd.a < 0 || cmd.a > 127 || port >= kMaxPorts || channel > 15) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  schedule_or_warn(port, m_now, MidiMessage::program(channel, static_cast<std::uint8_t>(cmd.a)),
                   sink);
  flush(sink);
}

// Live arpeggiator: kArp sets one field (kEnabled toggles capture on the input
// port; the rest are engine params); kArpOut sets the output route.
void Engine::cmd_arp(const Command& cmd, EventSink sink) {
  if (cmd.param == Param::kArpOut) {
    const auto port = static_cast<std::uint8_t>(cmd.a & 0xFF);
    const auto channel = static_cast<std::uint8_t>((cmd.a >> 8) & 0xFF);
    if (port >= kMaxPorts || channel > 15) {
      sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    } else {
      set_arp_out(port, channel);
    }
    return;
  }
  if (cmd.a < 0 || cmd.a >= kArpFieldCount) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  } else if (static_cast<ArpField>(cmd.a) == ArpField::kEnabled) {
    set_arp_enabled(cmd.b != 0, m_arp_in_port);
  } else {
    m_arp.set_field(static_cast<ArpField>(cmd.a), cmd.b);
  }
}

}  // namespace arrangrr

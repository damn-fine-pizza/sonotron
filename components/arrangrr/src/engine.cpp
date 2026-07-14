#include "arrangrr/engine.hpp"

#include <cstring>  // std::memcpy: PerfInsert::params <-> Insert::Params raw-byte copy (Phase-6
                    // Theme 3 Item #3) -- Insert::Params is a padding-free 4-byte union
                    // (insert_chain.hpp's own static_assert(sizeof(Insert) == 6) proves it), so
                    // this is NOT the "never memcpy the record" case GrooveParams motivates.

// Binary-ABI command handling (D26), split per domain: the dispatch is a
// ten-line switch, each family owns its validation and warns.

namespace arrangrr {

// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention.
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
    case Param::kClipAdd:
    case Param::kClipLaunch:
    case Param::kClipStop:
    case Param::kSceneQuantize:
      cmd_clip(cmd, sink);
      break;
    case Param::kPadAssign:
    case Param::kPadTrigger:
    case Param::kPadRelease:
    case Param::kPadBankSelect:
      cmd_pad(cmd, sink);
      break;
    case Param::kPerformanceStore:
    case Param::kPerformanceRecall:
      cmd_perf(cmd, sink);
      break;
    case Param::kFxSet:
    case Param::kFxParam:
    case Param::kFxEnable:
    case Param::kFxClear:
      cmd_fx(cmd, sink);
      break;
    case Param::kMasterTranspose:
      cmd_master_transpose(cmd, sink);
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
      m_chorddet.clear();  // every key is up now; the latched chord stays (memory)
      m_arp.panic();       // drop any held/latched arp notes
      // Torquato finding 3: Panic silences the WIRE via NoteTracker above but
      // used to never touch m_pads -- a kToggle (or kHold) pad left
      // PadRuntime::on == true after the wire went silent needed a second
      // trigger to sound again (kToggle's flip would read the stale `on`
      // and turn it back OFF instead of sounding). Reset every pad's runtime
      // on-state here so the bookkeeping matches what Panic just did to the
      // wire, for every PadType uniformly (not just kDrum/kCC -- any
      // kToggle/kHold pad's `on` flag is equally stale after a panic).
      for (std::size_t i = 0; i < kMaxPads; ++i) {
        if (PadRuntime* rt = m_pads.runtime(i); rt != nullptr) {
          rt->on = false;
        }
      }
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
  m_chorddet.set_key(key);  // scale-aware single-finger reads the same key
  // Phase 3a (§17.3b): echo the current key so a client can reconstruct the
  // chords panel's "scale:" line.
  sink(OutEvent::param_state(Param::kKeySet, 0, static_cast<std::uint8_t>(cmd.a),
                             static_cast<std::uint8_t>(cmd.b), m_now));
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
  m_chorddet.set_min_notes(single ? 1 : kMinChordNotes);
  m_chorddet.set_single_finger(single);
  // Phase 3a (§17.3b): echo the current chord mode for the chords panel.
  sink(OutEvent::param_state(Param::kChordMode, 0, static_cast<std::uint8_t>(cmd.a), 0, m_now));
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
  // Phase 3a (§17.3b): echo detect on/off + which port, for the chords panel.
  sink(OutEvent::param_state(Param::kChordDetect, static_cast<std::uint8_t>(port),
                             cmd.a != 0 ? 1 : 0, 0, m_now));
}

void Engine::chord_follow_cmd(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(ChordFollow::kLivePriority)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  set_chord_follow(static_cast<ChordFollow>(cmd.a));
  // Phase 3a (§17.3b): echo which producer may steer, for the chords panel.
  sink(OutEvent::param_state(Param::kChordFollow, 0, static_cast<std::uint8_t>(cmd.a), 0, m_now));
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
// GCOVR_EXCL_STOP

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
  // Immediate by default (`current` changes now); a boundary != kImmediate
  // STAGES it for the next bar like a SHIFT-note, additive and POD (Phase-5
  // Item #2: retired the old `idx != 0` overload -- idx is unused here now).
  const bool steer = true;
  const bool quantize = cmd.boundary != Boundary::kImmediate;
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

// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention.
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
        // Phase 3a (§17.3b): echo the newly active style index -- no other
        // OutEvent reports it (kSection only ever carries the SECTION).
        sink(OutEvent::param_state(Param::kStyleLoad, 0, static_cast<std::uint8_t>(cmd.a & 0xFF),
                                   static_cast<std::uint8_t>((cmd.a >> 8) & 0xFF), m_now));
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
      // Phase 3a (§17.3b): echo the part's new mute/solo state.
      sink(OutEvent::param_state(cmd.param, static_cast<std::uint8_t>(role), cmd.b != 0 ? 1 : 0, 0,
                                 m_now));
      break;
    }
    case Param::kGroove:
      if (cmd.a < 0 || cmd.a >= kGrooveFieldCount) {
        sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
      } else {
        m_arranger.set_groove_field(static_cast<GrooveField>(cmd.a), cmd.b);
        // Phase 3a (§17.3b): echo the field id + new value (16-bit LE; wide
        // fields like GrooveField::kSeed are truncated to their low 16 bits).
        sink(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(cmd.a),
                                   static_cast<std::uint8_t>(cmd.b & 0xFF),
                                   static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF), m_now));
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
  // (CTRL+\ "now", boundary == kImmediate) or whenever the transport is
  // stopped — a queued switch could never land without ticks; otherwise it
  // rides the next bar boundary (ENTER "next-bar"), matching kStyleSection's
  // quantization. (Phase-5 Item #2: retired the old `c != 0`-is-immediate
  // overload -- boundary is the single shared spelling now.)
  const bool immediate = cmd.boundary == Boundary::kImmediate || !m_transport.playing();
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

// Phase-6 Theme 3 Item #1 (global transpose, docs/reflections/phase6-theme3-
// master-transpose-scope.md): kMasterTranspose sets a signed semitone offset,
// validated/rejected outside the owner's locked [-12, +12] UI range (the same
// reject-not-clamp discipline cmd_voice's port/channel bounds check uses
// above). Propagated to BOTH note-emitting paths the design traces -- the
// Arranger's own resolve() (the band) and ChordEngine::sound() (the pressed/
// pad chord) -- so they move together; the followed/detected chord and
// Timeline step-track literal notes are untouched by construction (neither
// reads master_transpose at all).
void Engine::cmd_master_transpose(const Command& cmd, EventSink sink) {
  if (cmd.a < -12 || cmd.a > 12) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto semitones = static_cast<std::int8_t>(cmd.a);
  m_arranger.set_master_transpose(semitones);
  m_chords.set_master_transpose(semitones);
  sink(OutEvent::param_state(Param::kMasterTranspose, 0, static_cast<std::uint8_t>(semitones), 0,
                             m_now));
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
    return;
  }
  if (static_cast<ArpField>(cmd.a) == ArpField::kEnabled) {
    set_arp_enabled(cmd.b != 0, m_arp_in_port);
  } else {
    m_arp.set_field(static_cast<ArpField>(cmd.a), cmd.b);
  }
  // Phase 3a (§17.3b): echo the field id + new value (16-bit LE; ArpField::
  // kSeed is truncated to its low 16 bits, same convention as kGroove).
  sink(OutEvent::param_state(Param::kArp, static_cast<std::uint8_t>(cmd.a),
                             static_cast<std::uint8_t>(cmd.b & 0xFF),
                             static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF), m_now));
}

// Phase-5 Item #2 (docs/design/clip-primitive-design.md): the clip launch
// primitive. cmd_clip dispatches the 4 verbs; clip_request is the shared
// immediate-vs-quantized path clip_launch/clip_stop/clip_scene_launch all
// route through; apply_clip_content is the ONE place that translates a fired
// clip's {kind, content_index} into a real effect on the subsystem Engine
// already owns (Arranger/ChordSequencer/Timeline) -- ClipMatrix itself never
// touches them (scope tripwire, decision 5).
void Engine::cmd_clip(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kClipAdd:
      clip_add(cmd, sink);
      break;
    case Param::kClipLaunch:
      clip_launch(cmd, sink);
      break;
    case Param::kClipStop:
      clip_stop(cmd, sink);
      break;
    case Param::kSceneQuantize:
    default:
      clip_scene_launch(cmd, sink);
      break;
  }
}

// Host/script-only registration (no L1 grammar of its own beyond `clip add`,
// shell_clip_commands.cpp): a = TrackRole, b = scene_index, c = ContentKind
// (low byte) | (content_index << 8).
void Engine::clip_add(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || cmd.a > static_cast<std::int32_t>(TrackRole::kCc) || cmd.b < 0 || cmd.b > 255) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto kind_value = cmd.c & 0xFF;
  if (kind_value > static_cast<std::int32_t>(ContentKind::kStepTrack)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto role = static_cast<TrackRole>(cmd.a);
  const auto scene = static_cast<std::uint8_t>(cmd.b);
  const auto kind = static_cast<ContentKind>(kind_value);
  const auto content_index = static_cast<std::uint16_t>((cmd.c >> 8) & 0xFFFF);
  if (m_clips.add(role, scene, kind, content_index) < 0) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

void Engine::clip_launch(const Command& cmd, EventSink sink) {
  clip_request(cmd.idx, LaunchState::kPlaying, cmd, sink);
}

void Engine::clip_stop(const Command& cmd, EventSink sink) {
  clip_request(cmd.idx, LaunchState::kStopped, cmd, sink);
}

// `launch scene <n> quantize <q>`: fans out to every registered clip whose
// scene_index matches idx, each launched through the SAME clip_request path
// (so an individual clip's own quantize window rules apply identically).
void Engine::clip_scene_launch(const Command& cmd, EventSink sink) {
  bool any = false;
  for (std::size_t id = 0; id < m_clips.size(); ++id) {
    const Clip* c = m_clips.get(id);
    if (c == nullptr || c->scene_index != cmd.idx) {
      continue;
    }
    any = true;
    clip_request(id, LaunchState::kPlaying, cmd, sink);
  }
  if (!any) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

void Engine::clip_request(std::size_t id, LaunchState target, const Command& cmd, EventSink sink) {
  const Clip* c = m_clips.get(id);
  if (c == nullptr) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const Clip content = *c;  // copy: apply_clip_content only reads kind/content_index
  const auto wire_id = static_cast<std::uint16_t>(id);
  if (cmd.boundary == Boundary::kImmediate) {
    apply_clip_content(content, target, sink);
    (void)m_clips.force(id, target);
    sink(OutEvent::clip(wire_id, static_cast<std::uint8_t>(target), m_now));
    return;
  }
  const std::uint8_t n_bars = cmd.boundary == Boundary::kNextNBars ? cmd.n_bars : 1;
  (void)m_clips.arm(id, target, n_bars);
  const LaunchState pending =
      target == LaunchState::kPlaying ? LaunchState::kArmed : LaunchState::kQueuedStop;
  sink(OutEvent::clip(wire_id, static_cast<std::uint8_t>(pending), m_now));
}
// GCOVR_EXCL_STOP

void Engine::apply_clip_content(const Clip& clip, LaunchState target, EventSink sink) {
  switch (clip.kind) {
    case ContentKind::kStyleSection:
      // A style section has no natural "stopped" target (the arranger
      // always plays SOME section) -- launching is the only musically
      // meaningful direction; stopping is bookkeeping-only (the clip's own
      // state still moves to kStopped in the caller).
      if (target == LaunchState::kPlaying) {
        const auto section = static_cast<SectionType>(clip.content_index);
        if (m_arranger.request(section, /*immediate=*/true)) {
          sink(OutEvent::section(static_cast<std::uint16_t>(m_arranger.current()), m_now));
        }
      }
      break;
    case ContentKind::kChordSequence:
      if (target == LaunchState::kPlaying) {
        if (m_seq.use(clip.content_index) && m_seq.play(m_transport.tick())) {
          fire_chord_seq(m_transport.tick(), sink);
        }
      } else if (m_seq.playing() && m_seq.current_index() == clip.content_index) {
        // Only stop the sequencer when ITS active sequence is actually the
        // one this clip references -- ChordSequencer is a single-active-
        // sequence machine, not per-clip parallel playback.
        m_seq.stop_playback([&]() {
          m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
        });
        flush(sink);
      }
      break;
    case ContentKind::kStepTrack:
    default:
      if (Track* t = m_timeline.track(clip.content_index); t != nullptr) {
        t->mute = target != LaunchState::kPlaying;
      }
      break;
  }
}

void Engine::fire_clips(Tick transport_tick, EventSink sink) {
  m_clips.on_bar(transport_tick, [&](std::size_t id, const Clip& clip) {
    apply_clip_content(clip, clip.state, sink);
    sink(OutEvent::clip(static_cast<std::uint16_t>(id), static_cast<std::uint8_t>(clip.state),
                        m_now));
  });
}

// Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): pad banks are WRAPPER-ONLY -- cmd_pad dispatches the 3
// verbs; fire_pad is the ONE place that translates a fired pad into a call on
// the verb it wraps (clip_request/clip_scene_launch/Arranger::request/
// perf_recall), mirroring apply_clip_content's own placement for the clip
// primitive. PadEngine itself never touches Arranger/ClipMatrix/
// PerformanceStore (scope tripwire, same discipline as ClipMatrix).
// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention.
void Engine::cmd_pad(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kPadAssign:
      pad_assign(cmd, sink);
      break;
    case Param::kPadTrigger:
      pad_trigger(cmd, sink);
      break;
    case Param::kPadBankSelect:
      pad_bank_select(cmd, sink);
      break;
    case Param::kPadRelease:
    default:
      pad_release(cmd, sink);
      break;
  }
}

// Phase-6 Theme 3 Item #4: mirrors cmd_master_transpose's own validate-or-
// reject shape (bad_argument, state unchanged on reject).
void Engine::pad_bank_select(const Command& cmd, EventSink sink) {
  if (cmd.a < 0 || static_cast<std::size_t>(cmd.a) >= kMaxPadBanks) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  m_pad_bank = static_cast<std::uint16_t>(cmd.a);
  sink(OutEvent::param_state(Param::kPadBankSelect, 0, static_cast<std::uint8_t>(m_pad_bank), 0,
                             m_now));
}

// Host/script-only registration (no L1 grammar of its own beyond `pad
// assign`, shell_pad_commands.cpp): a = type | (mode << 8) | (sync << 16) |
// (pitch << 24); b = dest_port | (dest_channel << 8) | (n_bars << 16);
// c = source_idx | (source_aux << 24).
void Engine::pad_assign(const Command& cmd, EventSink sink) {
  const auto packed_a = static_cast<std::uint32_t>(cmd.a);
  const auto packed_b = static_cast<std::uint32_t>(cmd.b);
  const auto packed_c = static_cast<std::uint32_t>(cmd.c);
  const auto type_v = packed_a & 0xFFu;
  const auto mode_v = (packed_a >> 8) & 0xFFu;
  const auto sync_v = (packed_a >> 16) & 0xFFu;
  const auto pitch_v = (packed_a >> 24) & 0xFFu;
  const auto dest_port = packed_b & 0xFFu;
  const auto dest_channel = (packed_b >> 8) & 0xFFu;
  const auto n_bars = (packed_b >> 16) & 0xFFu;
  const auto source_idx = packed_c & 0xFFFFu;
  const auto source_aux = (packed_c >> 24) & 0xFFu;
  const bool ok = type_v < static_cast<std::uint32_t>(kPadTypeCount) &&
                  mode_v <= static_cast<std::uint32_t>(PadMode::kToggle) &&
                  sync_v <= static_cast<std::uint32_t>(Boundary::kNextNBars) &&
                  pitch_v <= static_cast<std::uint32_t>(PadPitch::kTransposeWithChord) &&
                  dest_port < kMaxPorts && dest_channel <= 15;
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  Pad pad;
  pad.type = static_cast<PadType>(type_v);
  pad.mode = static_cast<PadMode>(mode_v);
  pad.sync = static_cast<Boundary>(sync_v);
  pad.pitch = static_cast<PadPitch>(pitch_v);
  pad.n_bars = static_cast<std::uint8_t>(n_bars < 1 ? 1 : n_bars);
  pad.dest_port = static_cast<std::uint8_t>(dest_port);
  pad.dest_channel = static_cast<std::uint8_t>(dest_channel);
  pad.source_idx = static_cast<std::uint16_t>(source_idx);
  pad.source_aux = static_cast<std::uint8_t>(source_aux);
  if (!m_pads.assign(cmd.idx, pad)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

// Mode dispatch: kToggle flips PadRuntime::on and launches/stops accordingly;
// every other mode (kOneShot/kLoop/kHold) launches -- kHold's stop half lives
// in pad_release below.
void Engine::pad_trigger(const Command& cmd, EventSink sink) {
  const Pad* pad = m_pads.get(cmd.idx);
  PadRuntime* rt = m_pads.runtime(cmd.idx);
  if (pad == nullptr || rt == nullptr) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  if (pad->mode == PadMode::kToggle) {
    rt->on = !rt->on;
    fire_pad(*pad, rt->on ? LaunchState::kPlaying : LaunchState::kStopped, cmd.idx, sink);
    return;
  }
  rt->on = true;
  fire_pad(*pad, LaunchState::kPlaying, cmd.idx, sink);
}

void Engine::pad_release(const Command& cmd, EventSink sink) {
  const Pad* pad = m_pads.get(cmd.idx);
  PadRuntime* rt = m_pads.runtime(cmd.idx);
  if (pad == nullptr || rt == nullptr) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  if (pad->mode != PadMode::kHold) {
    return;  // kOneShot/kLoop/kToggle: release is a no-op (toggle already acted at trigger)
  }
  rt->on = false;
  fire_pad(*pad, LaunchState::kStopped, cmd.idx, sink);
}
// GCOVR_EXCL_STOP

void Engine::fire_pad(const Pad& pad, LaunchState target, std::uint16_t pad_id, EventSink sink) {
  // pad.dest_port/dest_channel are RESERVED for every PadType below EXCEPT
  // kDrum/kCC (see pad_bank.hpp's own header comment): those fan out to an
  // EXISTING verb that already owns its own destination (a clip's role
  // route, the arranger's style routes, or a Performance's captured routes).
  // kDrum/kCC consume dest_port/dest_channel directly -- the pad IS its own
  // destination (Decision 2, pad-owned, locked by the owner). pad.pitch is
  // ignored by every PadType, kDrum/kCC included (a raw note/CC has no
  // "transpose with chord" reading).
  Command boundary_cmd;
  boundary_cmd.boundary = pad.sync;
  boundary_cmd.n_bars = pad.n_bars;
  switch (pad.type) {
    case PadType::kPhrase:
    case PadType::kChord:
      // Pads that wrap clips reuse ClipMatrix quantization entirely: the SAME
      // clip_request path clip/launch/stop verbs use, addressed by a physical
      // pad instead of a GUI cell click.
      clip_request(pad.source_idx, target, boundary_cmd, sink);
      break;
    case PadType::kSceneColumn:
      if (target == LaunchState::kPlaying) {
        boundary_cmd.idx = pad.source_idx;
        clip_scene_launch(boundary_cmd, sink);
      }
      // No defined "stop a scene column" effect (clip_scene_launch's own
      // scope); a kHold/kToggle release is a bookkeeping-only no-op here.
      break;
    case PadType::kVariation:
    case PadType::kFill:
      if (target == LaunchState::kPlaying) {
        if (pad.source_idx >= kSectionTypeCount) {
          sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
          break;
        }
        // Arranger::request has no N-bar quantize primitive (only
        // immediate-vs-next-bar): kNextNBars degrades to next-bar here,
        // unlike Phrase/Chord/SceneColumn pads above, which route through
        // ClipMatrix's own true N-bar boundary window.
        const bool immediate = pad.sync == Boundary::kImmediate || !m_transport.playing();
        if (!m_arranger.request(static_cast<SectionType>(pad.source_idx), immediate)) {
          sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
        } else if (immediate) {
          sink(OutEvent::section(static_cast<std::uint16_t>(m_arranger.current()), m_now));
        }
      }
      break;
    case PadType::kPerformance:
      if (target == LaunchState::kPlaying) {
        boundary_cmd.idx = pad.source_idx;
        perf_recall(boundary_cmd, sink);
      }
      break;
    // Phase-6 Theme 3 Item #2: direct emission, split into their own
    // functions below to keep fire_pad's own cognitive complexity under the
    // clang-tidy gate (same discipline as every other case-handler split in
    // this class).
    // Torquato finding 1: pad.sync/pad.n_bars used to be read only by
    // pad_assign's own structural validation and then silently dropped --
    // fire_pad_drum/fire_pad_cc consulted `target` alone. Honor it exactly
    // like kVariation/kFill just above: kImmediate (or transport stopped)
    // fires now; anything else arms. kNextBar and kNextNBars both arm for
    // ONE bar -- pad.n_bars is intentionally not read here, matching
    // kVariation/kFill's own documented kNextNBars-degrades-to-next-bar
    // behavior (neither Arranger::request nor this direct-emission path has
    // a true N-bar primitive; "match their exact behavior, don't invent a
    // third").
    case PadType::kDrum:
    case PadType::kCC: {
      const bool immediate = pad.sync == Boundary::kImmediate || !m_transport.playing();
      if (immediate) {
        if (pad.type == PadType::kDrum) {
          fire_pad_drum(pad, target, sink);
        } else {
          fire_pad_cc(pad, target, sink);
        }
      } else if (pad_id < kMaxPads) {
        m_pad_latch[pad_id].arm(1);
        m_pad_pending_target[pad_id] = target;
      }
      break;
    }
    case PadType::kNone:
    default:
      break;
  }
}

// Semantic range checks (note/velocity > 127) happen HERE, at fire time --
// pad_assign only validates structural bounds, mirroring the kVariation/kFill
// precedent in fire_pad above. Emits via the SAME schedule_or_warn choke
// point every other note-emitting path already shares (flush()'s existing
// NoteTracker::observe + host echo cover panic-safety and echo for free, no
// bespoke tracking).
void Engine::fire_pad_drum(const Pad& pad, LaunchState target, EventSink sink) {
  if (pad.source_idx > 127 || pad.source_aux > 127) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto note = static_cast<std::uint8_t>(pad.source_idx);
  if (target == LaunchState::kPlaying) {
    const std::uint8_t vel = pad.source_aux != 0 ? pad.source_aux : kPadDrumDefaultVelocity;
    // Torquato finding 2: a kOneShot/kLoop retrigger before its own fixed
    // gate elapses used to be cut short by the FIRST trigger's now-stale
    // scheduled note-off. cancel_note_off is the scheduler's own dedicated
    // retrigger primitive (§9.B, out_scheduler.hpp) -- fire_timeline's
    // schedule_pattern already uses it for exactly this same-note-overlap
    // case. Tombstone any pending off for this (port, channel, note) due at
    // or after now and re-anchor it to fire right now, immediately before
    // the new note-on (D29 sorts NoteOff before NoteOn on the same tick) --
    // the stale off no longer lands 50-120 ticks late and truncates the
    // retriggered hit. A first trigger (nothing pending yet) leaves
    // cancel_note_off a no-op, so this is byte-identical to the pre-fix path
    // for every non-retrigger case.
    if ((pad.mode == PadMode::kOneShot || pad.mode == PadMode::kLoop) &&
        m_scheduler.cancel_note_off(pad.dest_port, pad.dest_channel, note, m_now)) {
      schedule_or_warn(pad.dest_port, m_now, MidiMessage::note_off(pad.dest_channel, note), sink);
    }
    schedule_or_warn(pad.dest_port, m_now, MidiMessage::note_on(pad.dest_channel, note, vel), sink);
    // kOneShot/kLoop (folded together, Decision 3): pad_release is already a
    // no-op for both, so nothing else would ever send the matching note-off
    // -- schedule it here, at the fixed gate. kHold/kToggle's note-off comes
    // from pad_release/the toggle-off call below instead (target ==
    // kStopped), no gate needed there.
    if (pad.mode == PadMode::kOneShot || pad.mode == PadMode::kLoop) {
      schedule_or_warn(pad.dest_port, m_now + kPadDrumOneShotGateTicks,
                       MidiMessage::note_off(pad.dest_channel, note), sink);
    }
  } else {
    schedule_or_warn(pad.dest_port, m_now, MidiMessage::note_off(pad.dest_channel, note), sink);
  }
  // Immediate echo + panic-safety NOW, mirroring chord_play's own
  // schedule-then-flush precedent for a "do" verb that sounds on the spot --
  // the deferred kOneShot note-off (if any) stays queued, it is not yet due.
  // Localized to kDrum/kCC only: no other PadType's fire_pad case schedules
  // anything itself, so their behavior/goldens are untouched.
  flush(sink);
}

void Engine::fire_pad_cc(const Pad& pad, LaunchState target, EventSink sink) {
  if (pad.source_idx > 127 || pad.source_aux > 127) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto controller = static_cast<std::uint8_t>(pad.source_idx);
  // Decision 4: kHold/kToggle send the on-value on kPlaying and a hardcoded 0
  // on kStopped (no spare Pad byte for a custom off-value); kOneShot/kLoop
  // only ever reach kPlaying (pad_release is a no-op for both), so they fire
  // the on-value once and stay there, by design.
  const std::uint8_t value = target == LaunchState::kPlaying ? pad.source_aux : 0;
  schedule_or_warn(pad.dest_port, m_now, MidiMessage::cc(pad.dest_channel, controller, value),
                   sink);
  flush(sink);  // immediate echo + panic-safety, same reasoning as fire_pad_drum above
}

// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention. Phase-5 Item #9: the Performance store/recall primitive.
// cmd_perf dispatches the 2 verbs; perf_store captures the live rig into a slot; perf_recall
// validates + applies immediately or arms the shared BoundaryLatch (Corelli fix #2) for a quantized
// recall.
void Engine::cmd_perf(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kPerformanceStore:
      perf_store(cmd, sink);
      break;
    case Param::kPerformanceRecall:
    default:
      perf_recall(cmd, sink);
      break;
  }
}

void Engine::perf_store(const Command& cmd, EventSink sink) {
  const Performance perf = capture_performance();
  if (!m_perfs.store(cmd.idx, perf)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

void Engine::perf_recall(const Command& cmd, EventSink sink) {
  if (m_perfs.get(cmd.idx) == nullptr) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const bool immediate = cmd.boundary == Boundary::kImmediate || !m_transport.playing();
  if (immediate) {
    apply_performance(*m_perfs.get(cmd.idx), sink);
    return;
  }
  const std::uint8_t n_bars = cmd.boundary == Boundary::kNextNBars ? cmd.n_bars : 1;
  m_perf_recall.arm(n_bars);
  m_perf_recall_slot = cmd.idx;
}
// GCOVR_EXCL_STOP

// Snapshots the live rig into a Performance. Per-role backing reads
// Arranger::part_info (already-shipped, unchanged) for routing/mute/solo;
// the rest reads the equivalent live accessor each single-field ABI command
// already mutates (groove/style_id/tempo/key/chord-mode/chord-follow/the
// currently-playing chord sequence).
Performance Engine::capture_performance() const {
  Performance perf;
  constexpr std::uint8_t kRoles = static_cast<std::uint8_t>(TrackRole::kCc) + 1;
  static_assert(kRoles == 10);
  for (std::uint8_t r = 0; r < kRoles; ++r) {
    const auto role = static_cast<TrackRole>(r);
    const Arranger::PartInfo info = m_arranger.part_info(role);
    if (info.muted) {
      perf.track_mute_mask |= (1u << r);
    }
    if (info.soloed) {
      perf.track_solo_mask |= (1u << r);
    }
    perf.routes[r] = PerfRoute{.port = info.port,
                               .channel = info.channel,
                               .enabled = info.routed ? std::uint8_t{1} : std::uint8_t{0}};
    // Phase-6 Theme 3 Item #3 (P1): snapshot the role's live FX chain, one
    // PerfInsert per slot. Insert::Params' 4 raw bytes copy verbatim -- see
    // this file's own top-of-file comment on why memcpy is safe here.
    const InsertChain& chain = m_arranger.chain(role);
    for (std::size_t slot = 0; slot < kMaxInserts; ++slot) {
      const Insert* ins = chain.get(slot);
      PerfInsert& out = perf.insert_chains[r][slot];
      out.type = static_cast<std::uint8_t>(ins->type);
      out.enabled = ins->enabled ? std::uint8_t{1} : std::uint8_t{0};
      std::memcpy(out.params, &ins->params, sizeof(out.params));
    }
  }
  perf.groove = m_arranger.groove_params();
  perf.style_id = m_arranger.style_id();
  perf.tempo_x100 = static_cast<std::uint16_t>(m_transport.bpm());
  // Phase-6 Theme 3 Item #3 (P3): master_transpose is now a real
  // std::int16_t field -- a plain widening copy, no reinterpret hack.
  perf.master_transpose = m_arranger.master_transpose();
  perf.pad_bank_id = m_pad_bank;
  perf.chord_sequence_id =
      m_seq.playing() ? static_cast<std::uint16_t>(m_seq.current_index()) : std::uint16_t{0xFFFF};
  perf.controller_map_id = 0xFFFF;   // unbuilt today; reserved
  perf.routing_profile_id = 0xFFFF;  // Phase-6 Theme 3 Item #3 (P2): unbuilt today; reserved
  perf.variation = static_cast<std::uint8_t>(m_arranger.current());
  perf.chord_mode = static_cast<std::uint8_t>(m_chords.mode());
  perf.chord_follow = static_cast<std::uint8_t>(m_chords.follow());
  perf.key_root = m_chords.key().root_pc;
  perf.key_mode = static_cast<std::uint8_t>(m_chords.key().mode);
  // name is left zero-filled: the core ABI never carries a string (D26); a
  // host tool names a slot through PerformanceStore::get()'s mutable overload.
  return perf;
}

// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention. SEMANTIC ATOMICITY (Corelli recommendation): every
// referenced id is validated FIRST; apply_performance() below applies NOTHING when this returns
// false.
//
// QA gate restoration (Phase-5 Item #9 follow-up): the actual bounds-check
// logic moved to the free, pure, Engine-free arrangrr::perf::validate()
// (arrangrr/perf/performance.hpp, defined in src/performance.cpp) so it is
// unit-testable in isolation -- this member is now a thin forwarder that
// supplies the ONE live value that check needs (ChordSequencer::count()).
// Behavior is byte-identical to the pre-extraction body.
bool Engine::validate_performance(const Performance& perf) const noexcept {
  return perf::validate(perf, m_seq.count());
}
// GCOVR_EXCL_STOP

// Applies a validated Performance ATOMICALLY: style/variation land together,
// then routes/mute/solo/groove/tempo/key/chord-mode/chord-follow/
// chord-sequence, in the order the design spec fixes. Called EITHER
// immediately (perf_recall's own immediate path) OR from
// apply_pending_performance_recall exactly at the bar boundary the
// BoundaryLatch armed for -- either way `true` (immediate, Arranger-side) is
// always correct here: by the time this runs, the caller is already AT the
// boundary that matters.
bool Engine::apply_performance(const Performance& perf, EventSink sink) {
  if (!validate_performance(perf)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return false;
  }
  constexpr std::uint8_t kRoles = static_cast<std::uint8_t>(TrackRole::kCc) + 1;
  if (perf.style_id != 0xFFFF) {
    // QA regression fix (test_performance_style_id_regression.cpp): NOT
    // request_style() -- that entry point only ever receives a raw Style*,
    // never a table index, so its immediate branch unconditionally resets
    // Arranger::style_id() to 0xFFFF ("pointer-based, no known index",
    // Corelli fix #1). That is correct for the live `style switch` verb,
    // which never had an index to preserve, but WRONG here:
    // apply_performance already holds the concrete, validate_performance()-
    // checked index (perf.style_id). Arranger::load(idx) is the ONE entry
    // point that sets a concrete style_id. Recall is always applied
    // immediately (see this method's own header comment above), so load()'s
    // own section/groove reset is overwritten right below by the explicit
    // request()/set_groove() calls -- byte-identical musical effect to the
    // old request_style() path, with the style_id metadata now preserved.
    m_arranger.load(static_cast<std::uint8_t>(perf.style_id));
    m_arranger.request(static_cast<SectionType>(perf.variation), /*immediate=*/true);
  } else {
    m_arranger.request(static_cast<SectionType>(perf.variation), /*immediate=*/true);
  }
  for (std::uint8_t r = 0; r < kRoles; ++r) {
    const auto role = static_cast<TrackRole>(r);
    const PerfRoute& route = perf.routes[r];
    m_arranger.set_route(role, route.port, route.channel);
    m_arranger.set_route_enabled(role, route.enabled != 0);
    m_arranger.set_mute(role, (perf.track_mute_mask & (1u << r)) != 0);
    m_arranger.set_solo(role, (perf.track_solo_mask & (1u << r)) != 0);
    // Phase-6 Theme 3 Item #3 (P1): restore the role's FX chain slot-for-slot,
    // byte-exact -- validate_performance() above already bounded every
    // insert_chains[r][slot].type to a real InsertType, so this is a plain
    // restore, no further clamping (mirrors pad_bank_id's own "validated
    // above, plain restore here" precedent).
    for (std::size_t slot = 0; slot < kMaxInserts; ++slot) {
      const PerfInsert& in = perf.insert_chains[r][slot];
      Insert ins;
      ins.type = static_cast<InsertType>(in.type);
      ins.enabled = in.enabled != 0;
      std::memcpy(&ins.params, in.params, sizeof(ins.params));
      m_arranger.restore_fx(role, slot, ins);
    }
  }
  m_arranger.set_groove(perf.groove);
  apply_arranger_voices(sink);           // the (possibly new) style's voices land with the recall
  m_transport.set_bpm(perf.tempo_x100);  // out-of-range silently ignored (Transport::set_bpm)
  // Phase-6 Theme 3 Item #3 (P3): master_transpose is a real std::int16_t,
  // already bounded to [-12, +12] by validate_performance() above, so the
  // narrowing cast back to Arranger/ChordEngine's own std::int8_t is safe;
  // propagated to both note-emitting paths, exactly like cmd_master_transpose.
  const auto transpose = static_cast<std::int8_t>(perf.master_transpose);
  m_arranger.set_master_transpose(transpose);
  m_chords.set_master_transpose(transpose);
  // Phase-6 Theme 3 Item #4: restore the active pad-bank view cursor.
  // validate_performance() above already rejected an out-of-range
  // pad_bank_id, so this is a plain restore, no clamp. No echo event here
  // (mirrors every other silently-restored field in this function; only
  // emit_performance_confirmation's own explicit set below produces
  // events) -- keeps existing recall goldens byte-identical.
  m_pad_bank = perf.pad_bank_id;
  const Key key{.root_pc = perf.key_root, .mode = static_cast<Mode>(perf.key_mode)};
  m_chords.set_key(key);
  m_chorddet.set_key(key);  // scale-aware single-finger reads the same key
  m_chords.set_mode(static_cast<ChordMode>(perf.chord_mode));
  const bool single = static_cast<ChordMode>(perf.chord_mode) == ChordMode::kSingle;
  m_chorddet.set_min_notes(single ? 1 : kMinChordNotes);
  m_chorddet.set_single_finger(single);
  m_chords.set_follow(static_cast<ChordFollow>(perf.chord_follow));
  if (perf.chord_sequence_id != 0xFFFF) {
    if (m_seq.use(perf.chord_sequence_id) && m_seq.play(m_transport.tick())) {
      fire_chord_seq(m_transport.tick(), sink);
    }
  }
  emit_performance_confirmation(perf, sink);
  flush(sink);
  return true;
}

// Compact confirmation (Corelli recommendation): kSection + the SAME
// kParamState echoes the on-connect state dump replays (apps/sonotron-server/
// main.cpp's send_state_dump), so a client re-renders every domain a recall
// just changed without needing a bespoke "performance changed" event.
void Engine::emit_performance_confirmation(const Performance& perf, EventSink sink) {
  constexpr std::uint8_t kRoles = static_cast<std::uint8_t>(TrackRole::kCc) + 1;
  sink(OutEvent::section(static_cast<std::uint16_t>(m_arranger.current()), m_now));
  if (perf.style_id != 0xFFFF) {
    sink(OutEvent::param_state(Param::kStyleLoad, 0,
                               static_cast<std::uint8_t>(perf.style_id & 0xFF),
                               static_cast<std::uint8_t>((perf.style_id >> 8) & 0xFF), m_now));
  }
  for (std::uint8_t r = 0; r < kRoles; ++r) {
    const auto role = static_cast<TrackRole>(r);
    sink(OutEvent::param_state(Param::kPartMute, r, m_arranger.muted(role) ? 1 : 0, 0, m_now));
    sink(OutEvent::param_state(Param::kPartSolo, r, m_arranger.soloed(role) ? 1 : 0, 0, m_now));
  }
  const GrooveParams& g = m_arranger.groove_params();
  sink(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwing),
                             g.swing, 0, m_now));
  sink(OutEvent::param_state(Param::kGroove,
                             static_cast<std::uint8_t>(GrooveField::kHumanizeTiming),
                             g.humanize_timing, 0, m_now));
  sink(OutEvent::param_state(Param::kGroove,
                             static_cast<std::uint8_t>(GrooveField::kHumanizeVelocity),
                             g.humanize_velocity, 0, m_now));
  sink(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kAccent),
                             g.accent, 0, m_now));
  sink(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kSwingGrid),
                             g.swing_grid, 0, m_now));
  sink(OutEvent::param_state(Param::kGroove, static_cast<std::uint8_t>(GrooveField::kQuantize),
                             g.quantize, 0, m_now));
  sink(OutEvent::param_state(Param::kChordMode, 0, perf.chord_mode, 0, m_now));
  sink(OutEvent::param_state(Param::kChordFollow, 0, perf.chord_follow, 0, m_now));
  sink(OutEvent::param_state(Param::kKeySet, 0, perf.key_root, perf.key_mode, m_now));
  // The event payload is a single byte; validate_performance() already
  // bounded master_transpose to [-12, +12], so the low byte alone is always
  // the full value -- same low-byte extraction as capture/apply above, just
  // via an explicit uint16_t reinterpret first (never a signed '&', which
  // -Wsign-conversion correctly flags).
  const auto transpose_u16 = static_cast<std::uint16_t>(perf.master_transpose);
  sink(OutEvent::param_state(Param::kMasterTranspose, 0,
                             static_cast<std::uint8_t>(transpose_u16 & 0xFFu), 0, m_now));
}

void Engine::apply_pending_performance_recall(EventSink sink) {
  if (!m_perf_recall.due(m_transport.tick())) {
    return;
  }
  m_perf_recall.clear();
  if (const Performance* perf = m_perfs.get(m_perf_recall_slot); perf != nullptr) {
    apply_performance(*perf, sink);
  }
}

// Torquato finding 1: mirrors apply_pending_performance_recall exactly, one
// BoundaryLatch per flat pad slot instead of the single m_perf_recall
// instance (several kDrum/kCC pads can be armed concurrently for
// independent boundaries). A pad reassigned to a different type/id between
// arming and this bar (or removed) is silently skipped -- pad_assign already
// drops any stale runtime state on a re-assign (PadEngine::assign), and a
// dangling arm here would otherwise fire against config that no longer
// matches what the user actually armed.
void Engine::apply_pending_pad_fires(EventSink sink) {
  for (std::uint16_t id = 0; id < kMaxPads; ++id) {
    if (!m_pad_latch[id].due(m_transport.tick())) {
      continue;
    }
    m_pad_latch[id].clear();
    const Pad* pad = m_pads.get(id);
    if (pad == nullptr) {
      continue;
    }
    if (pad->type == PadType::kDrum) {
      fire_pad_drum(*pad, m_pad_pending_target[id], sink);
    } else if (pad->type == PadType::kCC) {
      fire_pad_cc(*pad, m_pad_pending_target[id], sink);
    }
  }
}

// GCOVR_EXCL_START -- thin ABI dispatch glue (Phase 6 Theme 1b): validate args, route to ONE
// already-tested subsystem call, optional echo; no independent per-tick musical decision logic
// (callees fire_*/apply_*/emit_* remain individually gated). Functional-tested per the project's
// unit/functional/regression convention.
void Engine::cmd_fx(const Command& cmd, EventSink sink) {
  switch (cmd.param) {
    case Param::kFxSet:
      fx_set(cmd, sink);
      break;
    case Param::kFxParam:
      fx_param(cmd, sink);
      break;
    case Param::kFxEnable:
      fx_enable(cmd, sink);
      break;
    case Param::kFxClear:
    default:
      fx_clear(cmd, sink);
      break;
  }
}

// idx = TrackRole. a = slot (0..kMaxInserts-1), b = InsertType. Re-activates
// the slot with that type's fresh default params (Arranger::set_fx ->
// InsertChain::set_type).
void Engine::fx_set(const Command& cmd, EventSink sink) {
  const bool ok =
      cmd.idx <= static_cast<std::uint16_t>(TrackRole::kCc) && cmd.a >= 0 && cmd.b >= 0 &&
      cmd.b <= static_cast<std::int32_t>(InsertType::kNoteRepeat) &&
      m_arranger.set_fx(static_cast<TrackRole>(cmd.idx), static_cast<std::size_t>(cmd.a),
                        static_cast<InsertType>(cmd.b));
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

// idx = TrackRole. a = slot, b = param id (meaning depends on the slot's
// CURRENT type -- see InsertChain::set_param), c = value (clamped to the
// field's own width there).
void Engine::fx_param(const Command& cmd, EventSink sink) {
  const bool ok =
      cmd.idx <= static_cast<std::uint16_t>(TrackRole::kCc) && cmd.a >= 0 && cmd.b >= 0 &&
      cmd.b <= 255 &&
      m_arranger.set_fx_param(static_cast<TrackRole>(cmd.idx), static_cast<std::size_t>(cmd.a),
                              static_cast<std::uint8_t>(cmd.b), cmd.c);
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

// idx = TrackRole. a = slot, b = 0/1.
void Engine::fx_enable(const Command& cmd, EventSink sink) {
  const bool ok = cmd.idx <= static_cast<std::uint16_t>(TrackRole::kCc) && cmd.a >= 0 &&
                  m_arranger.set_fx_enable(static_cast<TrackRole>(cmd.idx),
                                           static_cast<std::size_t>(cmd.a), cmd.b != 0);
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}

// idx = TrackRole. a = slot, or -1 = the whole chain.
void Engine::fx_clear(const Command& cmd, EventSink sink) {
  if (cmd.idx > static_cast<std::uint16_t>(TrackRole::kCc)) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
    return;
  }
  const auto role = static_cast<TrackRole>(cmd.idx);
  const bool ok = cmd.a < 0 ? m_arranger.clear_fx(role)
                            : m_arranger.clear_fx(role, static_cast<std::size_t>(cmd.a));
  if (!ok) {
    sink(OutEvent::warn(WarnCode::kBadArgument, m_now));
  }
}
// GCOVR_EXCL_STOP

}  // namespace arrangrr

#pragma once

#include <cstdint>

#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/loop/loop_event.hpp"

// LoopBuffer: bounded pool of LoopClip slots (node 6000, the note-level peer
// of ChordSequencer, chord/chord_sequencer.hpp). Engine-owned, mirrors
// ChordSequencer's FORM -- record/quantize-after/loop -- but with three
// genuinely NEW capabilities ChordSequence never needed: overdub (merge into
// existing content instead of clear-then-record), erase, and a single-
// generation undo (Fork E, docs/reflections/phase7-scope-6000-8100-clip-
// timeline-seam.md).
//
// Two structural departures from ChordSequencer, both deliberate:
//  - RECORDING is a single active target (like ChordSequencer: one performer,
//    one live input stream at a time) -- m_recording/m_rec_slot/m_rec_port
//    below.
//  - PLAYBACK is PER-SLOT, not a single global cursor: a real multi-track
//    looper expects several captured loops (a bass loop, a drum loop, a
//    chord loop) to sound SIMULTANEOUSLY. ClipMatrix already tracks which
//    slots are launched (Clip::state); LoopBuffer keeps one small per-slot
//    PlayState (the tick playback started + the currently-sounding note
//    cache for correct note-off resolution) and Engine's fire_loop (the
//    node's own realtime driver, engine.cpp) iterates every kPlaying
//    kLoopBuffer clip and drives that slot's on_tick.
//
// Timebase discipline (D29, matches ChordSequencer exactly): RECORDING runs
// against stream time (`now`, always advancing -- a capture gesture can run
// with the transport stopped, same as ChordSequencer::start_record/capture);
// PLAYBACK runs against the transport's musical tick (on_tick is called from
// Engine::on_tick's transport-gated block, alongside fire_chord_seq).

namespace arrangrr {

// 6100: which capture semantic a record-start selects.
//   kRecord  -- clear the slot first, then capture (ChordSequencer's own
//               start_record behavior).
//   kReplace -- the SAME clear-then-capture mechanism as kRecord. A distinct
//               wire value purely for host/UI intent ("re-record over
//               existing content" vs. "first recording of an empty slot") --
//               punch-in/region-scoped replace (erase only the overlapping
//               window) is a finer distinction the owner never locked for
//               this slice and is deliberately NOT built here.
//   kOverdub -- do NOT clear; new events are appended to the EXISTING
//               content. Genuinely new (ChordSequence::record only ever
//               appends to an already-cleared buffer, chord_sequence.hpp).
enum class LoopRecordMode : std::uint8_t {
  kRecord = 0,
  kOverdub = 1,
  kReplace = 2,
};

// Bounded per-slot polyphony bookkeeping: which events are currently open
// (during capture) or sounding (during playback). 16 concurrent notes is
// generous headroom for one performer's two hands; a full table degrades
// gracefully (the note is dropped, mirroring every other bounded pool here)
// rather than growing unboundedly. Free (non-nested) so StaticVector<
// LoopBufferPlayState, N> can be a LoopBuffer member without the nested-
// class-incomplete-until-enclosing-class-is-complete pitfall (a class
// defined INSIDE LoopBuffer cannot back a StaticVector member of LoopBuffer
// ITSELF -- its default member initializers are only usable after
// LoopBuffer's own closing brace).
inline constexpr std::size_t kMaxLoopHeldNotes = 16;
struct LoopBufferHeldNote {
  std::uint8_t note = 0;
  std::uint16_t event_index = 0;
  bool used = false;
};
struct LoopBufferSoundingNote {
  std::uint16_t event_index = 0;
  std::uint8_t note = 0;
  bool used = false;
};
struct LoopBufferPlayState {
  Tick base = 0;
  // Idempotency guard (see LoopBuffer::on_tick's own header comment): the
  // last transport_tick this slot actually fired for, so a SECOND call for
  // the SAME tick (e.g. an immediate-launch's synchronous fire followed by
  // the ordinary per-tick fire_loop pass landing on that exact same tick) is
  // a safe no-op instead of a double-fire, regardless of which call site
  // reaches on_tick first.
  Tick last_tick = 0;
  bool has_ticked = false;
  LoopBufferSoundingNote sounding[kMaxLoopHeldNotes]{};
};

class LoopBuffer {
 public:
  // Fired note during playback: `on` selects note-on (true) vs. note-off
  // (false); the resolved absolute MIDI note (0..127) and the event's
  // captured velocity ride the callback directly (the caller -- Engine's
  // fire_loop -- builds the MidiMessage and routes it via the launching
  // clip's own part_role, LoopBuffer itself never touches ports/channels,
  // same scope tripwire ChordSequencer/Timeline already observe).
  using FireFn = FunctionRef<void(std::uint8_t note, std::uint8_t velocity, bool on)>;

  struct TickResult {
    bool ended = false;  // a non-looping clip reached its natural end this tick
  };

  // ---- pool ------------------------------------------------------------
  int add_slot() noexcept {
    LoopClip clip;
    if (!m_pool.push_back(clip)) {
      return -1;
    }
    // m_play shares m_pool's own capacity (kMaxLoopSlots): a successful
    // m_pool.push_back above guarantees this one succeeds too.
    (void)m_play.push_back(LoopBufferPlayState{});
    return static_cast<int>(m_pool.size() - 1);
  }
  std::size_t count() const noexcept { return m_pool.size(); }
  const LoopClip* get(std::size_t idx) const noexcept {
    return idx < m_pool.size() ? &m_pool[idx] : nullptr;
  }
  LoopClip* get(std::size_t idx) noexcept { return idx < m_pool.size() ? &m_pool[idx] : nullptr; }

  // ---- recording (6100) --------------------------------------------------
  // Starts capturing live notes arriving on `port` into slot `idx`. A shadow
  // copy of the slot's PRIOR content is saved first (Fork E: ONE shared,
  // single-generation shadow across the whole pool -- a live performer can
  // only be actively editing one slot at a time, so this is not "double the
  // whole pool", just double ONE slot's own footprint) so undo() can revert
  // this call's effect. Recording is a single active target (like
  // ChordSequencer): starting a new recording implicitly abandons any
  // in-progress one without closing its open notes -- callers are expected
  // to stop_record() first; this mirrors ChordSequencer's own single-cursor
  // discipline (no cross-slot recording queue).
  bool start_record(std::size_t idx, Tick now, std::uint8_t port, LoopRecordMode mode) noexcept {
    LoopClip* clip = get(idx);
    if (clip == nullptr) {
      return false;
    }
    save_shadow(idx, *clip);
    if (mode != LoopRecordMode::kOverdub) {
      clip->clear();
    }
    m_recording = true;
    m_rec_slot = idx;
    m_rec_port = port;
    m_rec_mode = mode;
    // Overdub aligns new material to the EXISTING content's own length (so a
    // note captured partway through a pass lands inside the same cycle the
    // old material already occupies) rather than the transport's live
    // playback position -- a SLICE-1 simplification: true bar-aligned
    // overdub-while-playing synchronization is 6300/6500 territory
    // (deferred). Recording fresh (kRecord/kReplace) always starts a new
    // zero reference, matching ChordSequencer::start_record exactly.
    m_overdub_wrap = mode == LoopRecordMode::kOverdub ? clip->content_length() : 0;
    m_record_base = now;
    clear_held();
    return true;
  }
  bool recording() const noexcept { return m_recording; }
  std::size_t recording_slot() const noexcept { return m_rec_slot; }
  std::uint8_t recording_port() const noexcept { return m_rec_port; }

  // Called for every live note-on while recording (Engine taps push_midi_in
  // on the matching port). Opens a new event; multiple concurrently-held
  // notes are tracked (unlike ChordSequencer's single "previous open step",
  // a note-level capture is genuinely polyphonic).
  void note_on(Tick now, std::uint8_t note, std::uint8_t velocity, const ChordState& chord,
               const Key& key) noexcept {
    if (!m_recording) {
      return;
    }
    LoopClip* clip = get(m_rec_slot);
    if (clip == nullptr) {
      return;
    }
    Tick rel = now - m_record_base;
    if (m_overdub_wrap > 0) {
      rel %= m_overdub_wrap;
    }
    std::int8_t tone = 0;
    std::int8_t octave = 0;
    LoopNoteSource source = LoopNoteSource::kInterval;
    decompose_note(note, chord, key, tone, octave, source);
    const std::size_t event_index = clip->count();
    if (!clip->record(LoopEvent{.start = rel,
                                .duration = 0,
                                .tone = tone,
                                .octave = octave,
                                .velocity = velocity,
                                .source = source})) {
      return;  // pool full: the note is silently dropped (same graceful
               // degradation discipline as every other bounded pool here)
    }
    hold(note, static_cast<std::uint16_t>(event_index));
  }

  // Called for every live note-off while recording. Closes the matching
  // open event's duration; a note-off with no matching held note (e.g. the
  // note-on was dropped by a full pool) is a silent no-op.
  void note_off(Tick now, std::uint8_t note) noexcept {
    if (!m_recording) {
      return;
    }
    LoopClip* clip = get(m_rec_slot);
    if (clip == nullptr) {
      return;
    }
    std::uint16_t event_index = 0;
    if (!release_held(note, event_index)) {
      return;
    }
    LoopEvent* ev = clip->mutable_event(event_index);
    if (ev == nullptr) {
      return;
    }
    Tick rel = now - m_record_base;
    if (m_overdub_wrap > 0) {
      rel %= m_overdub_wrap;
    }
    ev->duration = rel >= ev->start ? rel - ev->start : 0;
  }

  // Stops recording: closes every still-held note at `now` (falling back to
  // one grid unit for a zero-length close, mirroring ChordSequencer::
  // stop_record), then quantizes-after (6200, non-destructive to the event
  // COUNT/positions, only reshapes start/duration onto the grid).
  bool stop_record(Tick now, Tick grid = kTicksPerBar) noexcept {
    if (!m_recording) {
      return false;
    }
    LoopClip* clip = get(m_rec_slot);
    m_recording = false;
    if (clip == nullptr) {
      return false;
    }
    Tick rel = now - m_record_base;
    if (m_overdub_wrap > 0) {
      rel %= m_overdub_wrap;
    }
    for (std::size_t i = 0; i < kMaxLoopHeldNotes; ++i) {
      if (!m_held[i].used) {
        continue;
      }
      LoopEvent* ev = clip->mutable_event(m_held[i].event_index);
      if (ev != nullptr && ev->duration == 0) {
        ev->duration = rel > ev->start ? rel - ev->start : grid;
      }
    }
    clear_held();
    clip->quantize(grid == 0 ? kTicksPerBar : grid);
    return true;
  }

  // ---- erase / undo (6100) ------------------------------------------------
  bool erase(std::size_t idx) noexcept {
    LoopClip* clip = get(idx);
    if (clip == nullptr) {
      return false;
    }
    save_shadow(idx, *clip);
    clip->clear();
    return true;
  }

  // Restores the single retained prior generation for slot `idx` (Fork E):
  // a no-op (false) when the shadow does not back THIS slot -- either
  // nothing has been recorded/erased yet, a DIFFERENT slot was edited more
  // recently (the shadow is shared, one generation total), or undo() was
  // already called once for this edit (single generation: consumed on use,
  // no redo/toggle).
  bool undo(std::size_t idx) noexcept {
    if (m_shadow_slot < 0 || static_cast<std::size_t>(m_shadow_slot) != idx) {
      return false;
    }
    LoopClip* clip = get(idx);
    if (clip == nullptr) {
      return false;
    }
    *clip = m_shadow;
    m_shadow_slot = -1;
    return true;
  }

  // ---- playback (per-slot; mirrors ChordSequencer::play/on_tick) ---------
  // Arming an EMPTY slot is legal (ChordSequencer's own documented
  // convention): it plays silence until content exists.
  bool start_playback(std::size_t idx, Tick transport_tick) noexcept {
    if (idx >= m_play.size()) {
      return false;
    }
    m_play[idx] = LoopBufferPlayState{.base = transport_tick};
    return true;
  }

  // Releases any currently-sounding notes for `idx` (explicit stop; also
  // called internally by on_tick when a non-looping clip reaches its
  // natural end).
  void stop_playback(std::size_t idx, FireFn fire) noexcept {
    if (idx >= m_play.size()) {
      return;
    }
    release_all_sounding(m_play[idx], fire);
  }

  // Call once per transport tick, per PLAYING kLoopBuffer clip, while the
  // transport runs (Engine::fire_loop iterates ClipMatrix for the currently
  // kPlaying slots -- LoopBuffer itself has no notion of "is this slot
  // playing", ClipMatrix already owns that truth, same scope discipline
  // ChordSequencer/Timeline observe with respect to ClipMatrix).
  //
  // IDEMPOTENT per (slot, transport_tick): an immediate clip launch fires
  // this slot's own tick-0 content SYNCHRONOUSLY (Engine::
  // apply_clip_content_loop_buffer), because Runtime::advance_ticks only
  // ever calls Engine::on_tick for FUTURE ticks (current+1 onward) -- the
  // tick a loop starts playing on would otherwise never be evaluated by the
  // ordinary per-tick fire_loop pass. That synchronous call and a LATER
  // ordinary fire_loop pass can both legitimately reach this method for the
  // SAME transport_tick (e.g. a quantized bar-boundary launch promoted by
  // fire_clips, still inside the SAME on_tick invocation fire_loop's own
  // later call belongs to); tracking the last tick actually fired makes a
  // repeat call for that same tick a safe no-op instead of a double-fire,
  // regardless of which call site reaches here first.
  TickResult on_tick(std::size_t idx, Tick transport_tick, const ChordState& chord, const Key& key,
                     FireFn fire) {
    TickResult result;
    LoopClip* clip = get(idx);
    if (clip == nullptr || idx >= m_play.size()) {
      return result;
    }
    LoopBufferPlayState& ps = m_play[idx];
    if (transport_tick < ps.base) {
      return result;
    }
    if (ps.has_ticked && ps.last_tick == transport_tick) {
      return result;
    }
    ps.has_ticked = true;
    ps.last_tick = transport_tick;
    const Tick len = clip->length();
    if (len == 0) {
      return result;  // armed but empty (or fixed_length==0): silence
    }
    const Tick raw_pos = transport_tick - ps.base;
    Tick pos = raw_pos;
    bool wrapped = false;
    if (raw_pos >= len) {
      if (!clip->loop) {
        if (raw_pos == len) {
          release_all_sounding(ps, fire);
          result.ended = true;
        }
        return result;
      }
      pos = raw_pos % len;
      wrapped = true;
    }
    // An event whose end lands EXACTLY on the loop boundary (duration ==
    // len - start) must still release before the next cycle's own note-ons
    // fire THIS SAME tick -- checking against the wrapped `pos` (0) would
    // never match `start + duration == len`, silently swallowing that
    // note-off forever (a genuine stuck-note-on-loop-restart bug found while
    // writing this class's own functional test). The off check uses the
    // PRE-wrap boundary (`len`); the on check uses the POST-wrap `pos` (0) --
    // both land on the SAME transport tick, and the scheduler's own D29
    // total order (off before on) keeps this correct, mirroring
    // ChordEngine::sound()'s own release-before-sound-the-new-chord
    // discipline at a chord change.
    const Tick off_check_pos = wrapped ? len : pos;
    for (std::size_t i = 0; i < clip->count(); ++i) {
      const LoopEvent& ev = clip->event(i);
      if (ev.start == pos) {
        const int note = resolve_note(ev, chord, key);
        if (note >= 0) {
          store_sounding(ps, static_cast<std::uint16_t>(i), static_cast<std::uint8_t>(note));
          fire(static_cast<std::uint8_t>(note), ev.velocity, /*on=*/true);
        }
      }
      if (ev.duration > 0 && ev.start + ev.duration == off_check_pos) {
        release_sounding(ps, static_cast<std::uint16_t>(i), fire);
      }
    }
    return result;
  }

 private:
  void hold(std::uint8_t note, std::uint16_t event_index) noexcept {
    for (LoopBufferHeldNote& h : m_held) {
      if (!h.used) {
        h = LoopBufferHeldNote{.note = note, .event_index = event_index, .used = true};
        return;
      }
    }
    // Table full: the note-off for this note will find nothing to close,
    // and stop_record's own fallback (grid-length close) never applies to
    // it either -- an accepted, bounded degradation (same discipline as
    // record() returning false on a full event pool).
  }
  bool release_held(std::uint8_t note, std::uint16_t& event_index) noexcept {
    for (LoopBufferHeldNote& h : m_held) {
      if (h.used && h.note == note) {
        h.used = false;
        event_index = h.event_index;
        return true;
      }
    }
    return false;
  }
  void clear_held() noexcept {
    for (LoopBufferHeldNote& h : m_held) {
      h.used = false;
    }
  }

  static void store_sounding(LoopBufferPlayState& ps, std::uint16_t event_index,
                             std::uint8_t note) noexcept {
    for (LoopBufferSoundingNote& s : ps.sounding) {
      if (!s.used) {
        s = LoopBufferSoundingNote{.event_index = event_index, .note = note, .used = true};
        return;
      }
    }
  }
  static void release_sounding(LoopBufferPlayState& ps, std::uint16_t event_index,
                               FireFn fire) noexcept {
    for (LoopBufferSoundingNote& s : ps.sounding) {
      if (s.used && s.event_index == event_index) {
        s.used = false;
        fire(s.note, 0, /*on=*/false);
        return;
      }
    }
  }
  static void release_all_sounding(LoopBufferPlayState& ps, FireFn fire) noexcept {
    for (LoopBufferSoundingNote& s : ps.sounding) {
      if (s.used) {
        s.used = false;
        fire(s.note, 0, /*on=*/false);
      }
    }
  }

  void save_shadow(std::size_t idx, const LoopClip& clip) noexcept {
    m_shadow = clip;
    m_shadow_slot = static_cast<int>(idx);
  }

  StaticVector<LoopClip, kMaxLoopSlots> m_pool;
  StaticVector<LoopBufferPlayState, kMaxLoopSlots>
      m_play;  // parallel to m_pool, one entry per slot

  // Recording: a single active target (mirrors ChordSequencer).
  bool m_recording = false;
  std::size_t m_rec_slot = 0;
  std::uint8_t m_rec_port = 0;
  LoopRecordMode m_rec_mode = LoopRecordMode::kRecord;
  Tick m_record_base = 0;
  Tick m_overdub_wrap =
      0;  // > 0 while overdubbing: wrap new starts onto the existing content length
  LoopBufferHeldNote m_held[kMaxLoopHeldNotes]{};

  // Undo (Fork E): ONE shared shadow generation across the whole pool.
  LoopClip m_shadow;
  int m_shadow_slot = -1;  // -1 = no shadow held
};

}  // namespace arrangrr

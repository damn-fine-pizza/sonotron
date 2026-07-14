#pragma once

#include <cstdint>

#include "arrangrr/arp/arpeggiator.hpp"   // Phase-6 Theme 4 (5220): per-role arp-insert engine
#include "arrangrr/arranger/gesture.hpp"  // gesture::expand (per-event note fan-out)
#include "arrangrr/arranger/groove.hpp"
#include "arrangrr/arranger/motif.hpp"  // motif engine (9210): gather-phase seed/transform producer
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/arranger/voicing.hpp"  // NoteReq, VoicingState (voice-leading)
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/fx/insert_chain.hpp"  // Phase-5 Item #10: per-role MIDI-FX insert chain
#include "chorddet/theory.hpp"           // ChordState, ChordShape, theory::shape_of
#include "common/assert.hpp"             // ARR_ASSERT (voice-group cap net)
#include "common/time.hpp"

// The arranger (second WOW): plays the loaded style's current section on
// transport ticks, resolving chord-tone patterns against the live chord
// state — the NTT core (D24): every resolved note is a chord tone by
// construction, so wrong notes are impossible. Section switching is
// quantized to the bar (§11): variations loop, fills are one-shot and
// return, intros lead into a variation, endings stop the transport.

namespace arrangrr {

class Arranger {
 public:
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay, const MidiMessage& msg)>;
  // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): on_tick's OWN
  // schedule callback additionally carries the arrangrr::kScheduleSource*
  // producer tag (config.hpp) for every note it emits -- ordinary/non-arp
  // notes are tagged kScheduleSourceCore, a role's own arp-insert emissions
  // (the P5 pass) are tagged kScheduleSourceRoleArpBase + the role index, so
  // Engine::schedule_pattern's retrigger-care never cancels one producer's
  // note-off with another's note-on. emit_voices() below has no note/
  // retrigger concern (Program Change only), so it keeps the narrower,
  // UNCHANGED 3-arg ScheduleFn above.
  using NoteScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay,
                                          const MidiMessage& msg, std::uint8_t source)>;

  struct TickResult {
    bool section_changed = false;
    bool style_changed = false;
    SectionType section = SectionType::kVarA;
    bool stop_transport = false;
  };

  bool load(std::uint8_t builtin_index) noexcept {
    if (builtin_index >= styles::kBuiltinCount) {
      return false;
    }
    if (!load_style(styles::kBuiltins[builtin_index])) {
      return false;
    }
    // Phase-5 Item #9 (Corelli fix #1): the ONE path that knows the builtin
    // TABLE INDEX, so it is the only one that can set a concrete, known
    // style_id -- load_style() itself (called by pointer, from here AND from
    // request_style()'s live-switch path) has no index to give it and marks
    // "unknown/compiled" instead.
    m_style_id = builtin_index;
    return true;
  }
  constexpr bool loaded() const noexcept { return m_style != nullptr; }
  constexpr SectionType current() const noexcept { return m_current; }
  constexpr const Style* current_style() const noexcept { return m_style; }
  // Phase-5 Item #9 (Corelli fix #1): the live-backing field
  // Performance::style_id captures/recalls -- 0xFFFF ("unknown/compiled")
  // whenever the current style was NOT reached through load(builtin_index)
  // (a raw load_style(ptr) call, or a request_style() live switch, which
  // receives a pointer, not an index).
  constexpr std::uint16_t style_id() const noexcept { return m_style_id; }

  // Loads a style by pointer (compiled user styles, tests). The pointee must
  // outlive the arranger — builtin styles are constexpr, compiled ones live
  // in flash-mapped storage (D33).
  bool load_style(const Style* style) noexcept {
    if (style == nullptr) {
      return false;
    }
    m_style = style;
    m_style_id = 0xFFFF;  // unknown/compiled (Corelli fix #1); load() overwrites this after
    m_current = SectionType::kVarA;
    m_return_to = SectionType::kVarA;
    m_pending_valid = false;
    m_section_start = 0;
    m_voicing.reset();   // a new style must not voice-lead from the old one
    m_motif_repeat = 0;  // 9210: a new style's motif call-and-response restarts at the statement
    m_groove = style->groove;  // 9110: adopt the style's default feel (user edits re-apply after)
    reset_role_arps();  // Phase-6 Theme 4 (5220): a new style must not arpeggiate a stale chord
    return true;
  }

  bool set_route(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount || port >= kMaxPorts || channel > 15) {
      return false;
    }
    m_routes[idx] = Route{.port = port, .channel = channel, .enabled = true};
    return true;
  }

  // Phase-5 Item #9 (Corelli fix #3): set_route() above always ENABLES the
  // route it writes -- there was no way to restore a route to DISABLED
  // without this. A Performance recall needs exactly that (a role that was
  // routed-but-off when captured). Port/channel are left untouched; only the
  // enabled flag moves.
  bool set_route_enabled(TrackRole role, bool enabled) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    m_routes[idx].enabled = enabled;
    return true;
  }

  // Per-part (per-role) live mute/solo, mirroring the step-track mixer: a muted
  // role is silent; when ANY role is soloed, only soloed roles play. This is the
  // arranger-band mixer behind the host `parts` panel.
  void set_mute(TrackRole role, bool on) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx < kRoleCount) {
      set_bit(m_muted, idx, on);
    }
  }
  void set_solo(TrackRole role, bool on) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx < kRoleCount) {
      set_bit(m_solo, idx, on);
    }
  }
  constexpr bool muted(TrackRole role) const noexcept {
    return bit(m_muted, static_cast<std::uint8_t>(role));
  }
  constexpr bool soloed(TrackRole role) const noexcept {
    return bit(m_solo, static_cast<std::uint8_t>(role));
  }
  constexpr bool any_solo() const noexcept { return m_solo != 0; }

  // Global groove feel (the `groove` panel). set_groove_field clamps per field.
  void set_groove_field(GrooveField field, std::int32_t value) noexcept {
    groove::set_field(m_groove, field, value);
  }
  constexpr const GrooveParams& groove_params() const noexcept { return m_groove; }
  // Phase-5 Item #9 (Corelli recommendation): bulk recall setter -- a
  // Performance carries a whole captured GrooveParams, not one field at a
  // time (unlike the live `groove` panel's set_groove_field above).
  constexpr void set_groove(const GrooveParams& groove) noexcept { m_groove = groove; }

  // Phase-6 Theme 3 Item #1 (global transpose, docs/reflections/phase6-
  // theme3-master-transpose-scope.md Decision 2 "late"): a signed semitone
  // offset added to the ABSOLUTE note number resolve() computes, applied
  // right before its own [0,127] drop-not-fold clamp. kFixed (drums/perc)
  // patterns short-circuit before this ever applies -- they are exempt for
  // free, the same way they are exempt from the NTT chord-follow.
  //
  // Regression fix (Torquato QA pin, test_master_transpose_voicing_
  // regression.cpp): resolve() runs BEFORE m_voicing.voice() in on_tick's
  // D40 pipeline (see the comment there), so a kLead-voiced role's per-role
  // "previous register" memory (VoicingState::m_last) is recorded under
  // whatever transpose was in effect the last time that role sounded. Left
  // untouched, a live transpose delta beyond nearest_octave()'s tritone
  // threshold gets silently folded by a spurious octave one bar later. Shift
  // that memory by the SAME delta being applied here so it stays centered on
  // the new transpose -- voice-leading continuity is preserved (no register
  // jump from the player's own nudge) and nearest_octave() never sees a
  // stale reference. root_pc and the late-offset placement itself are
  // untouched.
  constexpr void set_master_transpose(std::int8_t semitones) noexcept {
    const int delta = static_cast<int>(semitones) - static_cast<int>(m_master_transpose);
    m_voicing.shift(delta);
    m_master_transpose = semitones;
  }
  constexpr std::int8_t master_transpose() const noexcept { return m_master_transpose; }

  // Phase-5 Item #10 (MIDI-FX insert chain, node 5100/5200): one InsertChain
  // per TrackRole (NOT per Timeline Track -- the same ordinal space as
  // m_routes/m_muted/m_solo). All four setters are thin, bounds-checked
  // forwarders onto the role's chain; a slot index out of range (checked
  // inside InsertChain itself) or a role out of range (checked here) both
  // fail closed (false), never trap -- the same "graceful degradation"
  // discipline as set_route/set_route_enabled above.
  bool set_fx(TrackRole role, std::size_t slot, InsertType type) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    return m_chain[idx].set_type(slot, type);
  }
  bool set_fx_param(TrackRole role, std::size_t slot, std::uint8_t param_id,
                    std::int32_t value) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    return m_chain[idx].set_param(slot, param_id, value);
  }
  bool set_fx_enable(TrackRole role, std::size_t slot, bool enabled) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    return m_chain[idx].set_enabled(slot, enabled);
  }
  // Clears ONE slot of the role's chain.
  bool clear_fx(TrackRole role, std::size_t slot) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    return m_chain[idx].clear(slot);
  }
  // Clears the WHOLE chain of the role (every slot back to the inert default).
  bool clear_fx(TrackRole role) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    m_chain[idx].clear_all();
    return true;
  }

  // Phase-6 Theme 3 Item #3 (Performance format_version 2, P1): the one READ
  // accessor onto m_chain -- every FX surface above is write-only by design
  // (v1 never needed to read a chain back). `role` is always a real TrackRole
  // value in practice (a scoped enum with exactly kRoleCount members), so an
  // out-of-range idx traps rather than degrading gracefully -- same
  // discipline as InsertChain's own constructor (ARR_ASSERT), and unlike the
  // write forwarders above (which fail closed) because there is no safe
  // "empty" InsertChain to return by reference.
  const InsertChain& chain(TrackRole role) const noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    ARR_ASSERT(idx < kRoleCount);
    return m_chain[idx];
  }
  // Companion write path to chain(): restores ONE slot of a role's chain to
  // an exact Insert value -- Performance recall's (Engine::apply_performance)
  // own write path, mirroring set_fx's bounds-checked-forwarder shape (fails
  // closed on an out-of-range role/slot, unlike chain() above).
  bool restore_fx(TrackRole role, std::size_t slot, const Insert& ins) noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    if (idx >= kRoleCount) {
      return false;
    }
    return m_chain[idx].restore(slot, ins);
  }

  // Phase-6 Theme 4 (5220): drops every role's held arp-insert chord --
  // mirrors the live-keyboard arp's own reset points (Engine::set_arp_
  // enabled/panic calling m_arp.panic()/clear()), keeping the two
  // independent arp instances (Fork 3's own flagged item) equally free of
  // stale state at the same boundaries: a fresh style load (load_style()
  // above), a transport (re)start (on_transport_start() below), and a
  // Panic (Engine::cmd_routing's kPanic case). Session-only bookkeeping --
  // never captured by Performance, so there is nothing to restore here.
  void reset_role_arps() noexcept {
    for (ArpeggiatorEngine& a : m_role_arp) {
      a.panic();
    }
  }

  // A snapshot of one part for the host mixer: its route, its voice in the
  // current section, and its mute/solo state. `present` is false when the
  // current section has no pattern for this role (e.g. arp only in varC/varD).
  struct PartInfo {
    bool routed = false;
    std::uint8_t port = 0;
    std::uint8_t channel = 0;  // 0-based
    std::int16_t gm_program = -1;
    bool muted = false;
    bool soloed = false;
    bool present = false;
  };
  PartInfo part_info(TrackRole role) const noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    PartInfo info;
    if (idx >= kRoleCount) {
      return info;
    }
    const Route& r = m_routes[idx];
    info.routed = r.enabled;
    info.port = r.port;
    info.channel = r.channel;
    info.muted = bit(m_muted, idx);
    info.soloed = bit(m_solo, idx);
    if (m_style != nullptr) {
      const StyleSection* section = m_style->find(m_current);
      if (section != nullptr) {
        for (const StylePattern& pattern : section->patterns) {
          if (pattern.role == role) {
            info.present = true;
            info.gm_program = pattern.gm_program;
            break;
          }
        }
      }
    }
    return info;
  }

  // Requests a section switch, applied at the next bar boundary (or at once
  // when the transport is not running — `immediate`).
  bool request(SectionType t, bool immediate) noexcept {
    if (m_style == nullptr || m_style->find(t) == nullptr) {
      return false;
    }
    if (immediate) {
      m_current = t;
      if (section_is_variation(t)) {
        m_return_to = t;
      }
      m_pending_valid = false;
    } else {
      m_pending = t;
      m_pending_valid = true;
    }
    return true;
  }

  // Requests a combined style + section switch. `immediate` applies it at once
  // (a hard mid-bar cut); otherwise both land TOGETHER at the next bar boundary
  // so live style changes stay seamless. The target section must exist in the
  // new style, else it falls back to varA (every style is expected to define
  // one). Returns false if the style is null or lacks even the fallback.
  bool request_style(const Style* style, SectionType section, bool immediate) noexcept {
    if (style == nullptr) {
      return false;
    }
    const SectionType target = style->find(section) != nullptr ? section : SectionType::kVarA;
    if (style->find(target) == nullptr) {
      return false;
    }

    if (immediate) {
      m_style = style;
      m_style_id = 0xFFFF;  // Corelli fix #1: a pointer-based switch has no known builtin index
      m_current = target;
      if (section_is_variation(target)) {
        m_return_to = target;
      }
      m_pending_valid = false;
      m_pending_style = nullptr;
      m_groove = style->groove;  // 9110: a live style switch adopts the new style's feel
      // Torquato QA (Phase-6 Theme 4 target 6): request_style() is a SEPARATE
      // entry point from load_style() and must reset every role's arp-insert
      // held chord exactly like it does -- otherwise a role's kArp slot keeps
      // arpeggiating a chord resolved under the OLD style even after a live
      // switch to a style that never resolves that role again (the P5
      // per-role-per-tick pass has no other signal that the style changed).
      reset_role_arps();
      return true;
    }

    m_pending_style = style;
    m_pending = target;
    m_pending_valid = true;
    return true;
  }

  void on_transport_start() noexcept {
    m_section_start = 0;
    m_voicing.reset();   // start each run with a clean voice-leading history
    m_motif_repeat = 0;  // 9210: every fresh run restarts the call-and-response at the statement
    reset_role_arps();   // Phase-6 Theme 4 (5220): a fresh run starts with no held arp chord
    if (section_is_variation(m_current)) {
      m_return_to = m_current;
    }
  }

  // Emits a Program Change for each routed role of the current section that
  // declares a default GM voice (StylePattern.gm_program >= 0), so loading a
  // style also picks its instruments. Unrouted roles are skipped (no known
  // destination). Call once after a style loads.
  void emit_voices(ScheduleFn schedule) const {
    if (m_style == nullptr) {
      return;
    }
    const StyleSection* section = m_style->find(m_current);
    if (section == nullptr) {
      return;
    }
    for (const StylePattern& pattern : section->patterns) {
      const Route& route = m_routes[static_cast<std::uint8_t>(pattern.role)];
      if (route.enabled && pattern.gm_program >= 0 && pattern.gm_program <= 127) {
        schedule(
            route.port, 0,
            MidiMessage::program(route.channel, static_cast<std::uint8_t>(pattern.gm_program)));
      }
    }
  }

  // One transport tick. Resolution order matters upstream: feed the chord
  // AFTER the chord sequencer has fired this tick, so bar downbeats resolve
  // against the fresh chord.
  //
  // Deliberately over the cognitive-complexity threshold: this is the realtime
  // core heartbeat (bar-boundary detection, pending style/section switches,
  // one-shot transitions, grid firing). Splitting it would scatter the tight
  // timing logic across functions for no readability gain and real risk.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  TickResult on_tick(Tick transport_tick, const Key& key, const ChordState& chord,
                     NoteScheduleFn schedule) {
    TickResult result;
    if (m_style == nullptr) {
      return result;
    }
    const StyleSection* section = m_style->find(m_current);
    if (section == nullptr) {
      return result;
    }

    // Bar boundary: apply pending switches / one-shot transitions.
    if (transport_tick > 0 || m_section_start == transport_tick) {
      const Tick pos = transport_tick - m_section_start;
      const Tick len = static_cast<Tick>(section->bars) * kTicksPerBar;
      const bool bar_boundary = pos != 0 && pos % kTicksPerBar == 0;
      const bool section_end = pos == len;
      if (bar_boundary || section_end) {
        SectionType next = m_current;
        bool style_switched = false;
        if (m_pending_valid) {
          // A pending style change lands together with its section, so a live
          // style switch is seamless (both on the same downbeat).
          if (m_pending_style != nullptr && m_pending_style != m_style) {
            m_style = m_pending_style;
            m_style_id = 0xFFFF;  // Corelli fix #1: same "unknown" convention as the immediate path
            style_switched = true;
            m_groove = m_style->groove;  // 9110: deferred switch adopts the new style's feel
            // Torquato QA (Phase-6 Theme 4 target 6): the DEFERRED half of
            // request_style()'s own fix above -- the switch only really takes
            // effect HERE, at the bar boundary it was queued for, so this is
            // where a stale held arp-insert chord must actually be dropped
            // (resetting it at request_style()'s own call time would be too
            // early: the OLD style is still playing until this commits).
            reset_role_arps();
          }
          m_pending_style = nullptr;
          next = m_pending;
          m_pending_valid = false;
        } else if (section_end) {
          if (section_is_fill(m_current) || section_is_intro(m_current)) {
            next = m_return_to;  // one-shots resolve to the active variation
          } else if (section_is_ending(m_current)) {
            result.stop_transport = true;
            return result;
          } else {
            // A plain variation looped back to itself: advance the motif
            // engine's repeat counter (9210) so a repeat-keyed call-and-
            // response transform can progress. Bounded, wraps silently (only
            // ever read mod small ranges downstream, motif.hpp).
            ++m_motif_repeat;
          }
        }
        // The section clock restarts when the section wraps, the section
        // changes, OR the style changes (even to the same section type) — a
        // mid-section bar boundary must not reset `pos`, or bars 2..N of a
        // multi-bar section would never play.
        if (section_end || next != m_current || style_switched) {
          m_section_start = transport_tick;
        }
        if (next != m_current || style_switched) {
          m_current = next;
          if (section_is_variation(next)) {
            m_return_to = next;
          }
          result.section_changed = true;
          result.style_changed = style_switched;
          result.section = next;
          // Re-resolve against the (possibly new) style — required even when
          // the section TYPE is unchanged but the style switched.
          section = m_style->find(m_current);
          if (section == nullptr) {
            return result;
          }
        }
      }
    }

    // Fire the grid slots of this tick. `step`/`rel` are computed
    // UNCONDITIONALLY now (Phase-6 Theme 4, P5): the arp-insert pass below
    // must run on every tick regardless of whether this tick lands on the
    // style's own step grid (an arp-insert's own rate can be finer than
    // kTicksPerStep, e.g. a 32nd-note rate at 120 ticks vs the grid's 240) --
    // the OLD early return here would have skipped it entirely on most
    // ticks. The grid-gated per-role step loop below is now an `if` block
    // instead of an early return, so both passes always run to completion.
    const Tick rel = transport_tick - m_section_start;
    const std::uint16_t step = static_cast<std::uint16_t>(rel / kTicksPerStep);
    const bool solo_active = any_solo();
    if (rel % kTicksPerStep == 0) {
      for (const StylePattern& pattern : section->patterns) {
        const Route& route = m_routes[static_cast<std::uint8_t>(pattern.role)];
        if (!route.enabled || part_silenced(pattern.role, solo_active)) {
          continue;
        }
        // Per-role, per-step resolution pipeline (D40):
        //   gather this step's events -> gesture::expand (1 event -> N specs)
        //   -> resolve() each (unchanged NTT kernel) into a bounded voice group
        //   -> m_voicing.voice() (voice-leading) -> groove + schedule per note.
        // With the default kNone gestures and kAsWritten voicing this emits, in
        // the same order and with the same timing, exactly what the former
        // one-note-per-event loop did — byte-for-byte up to kMaxVoiceNotes notes
        // per role per step; a step denser than that truncates (bounded, D32; no
        // real style reaches it — the ARR_ASSERT below turns the drop into a
        // debug signal rather than silence).
        NoteReq group[kMaxVoiceNotes];
        int count = 0;

        // Motif engine (9210): a gather-phase producer occupying the same slot
        // gesture::expand does, one level upstream. When this pattern names a
        // MotifSpec, its per-step events are GENERATED here (from an authored
        // seed in `pattern.events`, or motif::generate() when that is empty)
        // and the repeat-keyed call-and-response transform is applied, rather
        // than reading `pattern.events` literally below. `generated_motif`'s
        // backing array is a stack local (no heap); `source` aliases either it
        // or the pattern's own authored span unchanged, so a pattern with no
        // motif (motif == nullptr, the default) is byte-for-byte identical to
        // the historical behavior.
        Motif generated_motif;
        Span<const StyleEvent> source = pattern.events;
        if (pattern.motif != nullptr) {
          const Motif seed =
              pattern.events.empty()
                  ? motif::generate(
                        pattern.motif->seed, pattern.motif->length,
                        motif::idiom_onset_mask(section->patterns, pattern.motif->idiom_role),
                        pattern.motif->center_degree, pattern.motif->vel, pattern.motif->gate)
                  : motif::from_span(pattern.events);
          generated_motif = motif::apply_repeat(seed, *pattern.motif, m_motif_repeat);
          source = Span<const StyleEvent>(generated_motif.events, generated_motif.count);
        }

        for (const StyleEvent& ev : source) {
          if (ev.step != step) {
            continue;
          }
          StyleEvent specs[gesture::kMaxGestureFan];
          TickOffset delays[gesture::kMaxGestureFan];
          const int produced = gesture::expand(pattern, ev, chord, specs, delays);
          for (int i = 0; i < produced; ++i) {
            const int note = resolve(pattern, specs[i], key, chord, m_master_transpose);
            if (note < 0) {
              continue;
            }
            if (count >= kMaxVoiceNotes) {
              ARR_ASSERT(count < kMaxVoiceNotes);  // a step exceeded the cap
              break;
            }
            const bool is_chord_tone =
                pattern.policy == RolePolicy::kChordTone && specs[i].src == NoteSource::kChordTone;
            group[count++] = NoteReq{.note = note,
                                     .vel = specs[i].vel,
                                     .gate = specs[i].gate,
                                     .gesture_delay = delays[i],
                                     .chord_tone = is_chord_tone};
          }
        }
        if (count == 0) {
          continue;
        }
        // Voice-leading over the role's chord-tone notes (identity for kAsWritten).
        m_voicing.voice(pattern.role, pattern.voicing, group, count);
        const std::uint8_t role_idx = static_cast<std::uint8_t>(pattern.role);
        const FxContext fx_ctx{.key = key,
                               .chord = chord,
                               .step = step,
                               .tick = transport_tick,
                               .role = role_idx,
                               .groove = m_groove};
        // Phase-6 Theme 4 (5220, decision 5): a role's arp-insert (if any) has
        // its held chord REPLACED by this step's freshly-resolved group, not a
        // literal note_on/note_off mirror of a live-keyboard session -- clear
        // once per non-empty group, then ingest() every note of it below
        // (has_arp() gates this so a chain with no arp slot never touches the
        // role's ArpeggiatorEngine at all).
        if (m_chain[role_idx].has_arp()) {
          m_role_arp[role_idx].clear();
        }
        for (int i = 0; i < count; ++i) {
          const NoteReq& nr = group[i];
          // Phase-5 Item #10 (MIDI-FX insert chain): run the resolved note
          // through the role's chain — `seed.offset` starts at 0 (the chain's
          // OWN, self-relative clock); gesture_delay is a pre-existing,
          // chain-independent scheduling offset that rides outside the chain
          // entirely, exactly as it did before this item. An unconfigured /
          // fully-disabled chain (the default state of every role, aside from
          // the auto-present kGroove slot -- Phase-6 Theme 4, 5210) is a
          // provable 1-in/1-out identity (insert_chain.hpp), so fan_count == 1
          // and fanned[0] == seed for every existing style/track — the loop
          // below then reproduces the pre-Item-#10 schedule byte-for-byte.
          const FxNote seed{.note = nr.note, .vel = nr.vel, .gate = nr.gate, .offset = 0};
          // Phase-6 Theme 4 (5220): feed the SAME seed into the role's
          // arp-insert (a genuine no-op unless an enabled kArp slot exists) —
          // per Fork 1's decision, a role with an active kArp slot never
          // schedules its resolved notes directly (apply() below returns 0 for
          // them, since Insert::process's kArp case always swallows), so the
          // block chord and the arp's own rhythm never sound together.
          m_chain[role_idx].ingest(seed, m_role_arp[role_idx]);
          FxNote fanned[kMaxChainFan];
          const int fan_count = m_chain[role_idx].apply(seed, fanned, kMaxChainFan, fx_ctx);
          for (int j = 0; j < fan_count; ++j) {
            // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): ordinary
            // per-role/per-step emissions are the historical, undifferentiated
            // shared pool -- tag them kScheduleSourceCore so their retrigger-
            // care behavior stays byte-for-byte identical to before this fix.
            schedule_fanned_note(route, nr.gesture_delay, fanned[j], schedule, kScheduleSourceCore);
          }
        }
      }
    }
    // Phase-6 Theme 4 (5220) P5: a SECOND, UNGATED pass over every role, once
    // per tick -- unlike the grid-gated loop above (bar/step-locked), an
    // arp-insert must fire on its OWN rate grid, which is independent of
    // whether THIS tick is one of the style's own step boundaries. Placed
    // AFTER the grid-gated loop so a role's arp-insert reflects the
    // freshest resolved chord ingested THIS SAME tick, not last tick's, when
    // both grids happen to coincide.
    for (std::uint8_t r = 0; r < kRoleCount; ++r) {
      const Route& route = m_routes[r];
      if (!route.enabled || part_silenced(static_cast<TrackRole>(r), solo_active)) {
        continue;
      }
      const FxContext fx_ctx{.key = key,
                             .chord = chord,
                             .step = step,
                             .tick = transport_tick,
                             .role = r,
                             .groove = m_groove};
      FxNote emitted[kMaxChainFan];
      const int n =
          m_chain[r].on_tick(transport_tick, m_role_arp[r], fx_ctx, emitted, kMaxChainFan);
      for (int i = 0; i < n; ++i) {
        // No gesture_delay for an arp-emitted note -- it has no upstream
        // NoteReq of its own (simplest faithful model, Fork 1's own open
        // question; flagged in the implementation report).
        //
        // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): tag this
        // role's own arp-insert emissions with a per-role source id (BASE +
        // role index) so they never cross-cancel a DIFFERENT producer's
        // (the live-keyboard arp's, or a DIFFERENT role's own arp-insert's)
        // pending note-off sharing the same (port, channel, note).
        schedule_fanned_note(route, /*extra_delay=*/0, emitted[i], schedule,
                             static_cast<std::uint8_t>(kScheduleSourceRoleArpBase + r));
      }
    }
    return result;
  }

 private:
  static constexpr std::uint8_t kRoleCount = 10;
  // Upper bound on the notes one role emits on a single step: authored
  // chord-tone events plus any gesture fan-out. Generous vs real styles (a
  // dense chord part is ~4 tones), so the group buffer never overflows; extra
  // notes past this are dropped (bounded, no heap — D32).
  static constexpr int kMaxVoiceNotes = 16;

  struct Route {
    std::uint8_t port = 0;
    std::uint8_t channel = 0;
    bool enabled = false;
  };

  // A part plays unless it is muted, or a solo is active and it is not soloed.
  bool part_silenced(TrackRole role, bool solo_active) const noexcept {
    const auto idx = static_cast<std::uint8_t>(role);
    return bit(m_muted, idx) || (solo_active && !bit(m_solo, idx));
  }

  // Phase-6 Theme 4 (5210): the FINAL schedule step, shared by the grid-gated
  // per-step loop and the P5 arp-tick pass -- reads `fn.groove_offset`
  // (written by a kGroove-typed slot's own process(), 0 if the chain has no
  // such slot) instead of calling groove::apply() itself, now that groove is
  // a chain slot rather than a hardcoded call here. `extra_delay` is
  // `nr.gesture_delay` for a resolved-group note, or 0 for an arp-emitted one
  // (an arp-emitted note has no upstream NoteReq of its own).
  //
  // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): `source` is the
  // arrangrr::kScheduleSource* producer tag (config.hpp) this note's emission
  // belongs to -- forwarded verbatim into the 4-arg NoteScheduleFn callback
  // so Engine::schedule_pattern's retrigger-care (out_scheduler.hpp's
  // cancel_note_off) never lets one producer's note-on cancel a DIFFERENT
  // producer's pending note-off on the same (port, channel, note).
  static void schedule_fanned_note(const Route& route, TickOffset extra_delay, const FxNote& fn,
                                   NoteScheduleFn schedule, std::uint8_t source) {
    if (fn.note < 0 || fn.note > 127) {
      return;
    }
    const TickOffset on = fn.groove_offset + extra_delay + fn.offset;
    schedule(route.port, on,
             MidiMessage::note_on(route.channel, static_cast<std::uint8_t>(fn.note), fn.vel),
             source);
    schedule(route.port, static_cast<TickOffset>(fn.gate) + on,
             MidiMessage::note_off(route.channel, static_cast<std::uint8_t>(fn.note)), source);
  }
  static constexpr bool bit(std::uint16_t mask, std::uint8_t i) noexcept {
    return (mask & static_cast<std::uint16_t>(1u << i)) != 0;
  }
  static void set_bit(std::uint16_t& mask, std::uint8_t i, bool on) noexcept {
    if (on) {
      mask = static_cast<std::uint16_t>(mask | (1u << i));
    } else {
      mask = static_cast<std::uint16_t>(mask & ~(1u << i));
    }
  }

  // NTT core (D24): chord-tone index -> concrete note. Bass anchors low
  // (octave 2), everything else around octave 4; indices past the shape wrap
  // an octave up, so tone 3 over a triad is the root one octave higher.
  //
  // The per-event NoteSource selects how `tone` is read (kChordTone keeps this
  // exact historical computation). kFixed roles short-circuit to the literal
  // note REGARDLESS of src — drums never transpose.
  //
  // Phase-6 Theme 3 Item #1: `transpose` (Arranger::m_master_transpose, a
  // signed semitone offset, default 0) is added to the ABSOLUTE note in the
  // kInterval/kScaleDegree/kChordTone branches ONLY, right before each
  // branch's own [0,127] drop-not-fold clamp -- the kFixed short-circuit
  // above returns before `transpose` is ever consulted, so drums/perc stay
  // exempt for free (docs/reflections/phase6-theme3-master-transpose-
  // scope.md Decisions 1/2).
  static int resolve(const StylePattern& pattern, const StyleEvent& ev, const Key& key,
                     const ChordState& chord, std::int8_t transpose) noexcept {
    if (pattern.policy == RolePolicy::kFixed) {
      return ev.tone;
    }
    const int anchor = kRoleAnchor[static_cast<std::uint8_t>(pattern.role)];
    switch (ev.src) {
      case NoteSource::kInterval: {
        // Signed semitone offset from the chord root; no shape/quality lookup.
        if (!chord.valid) {
          return -1;  // silent until a chord exists
        }
        const int note = anchor + chord.root_pc + ev.tone + 12 * ev.octave + transpose;
        return (note < 0 || note > 127) ? -1 : note;
      }
      case NoteSource::kScaleDegree: {
        // Key-diatonic: independent of the chord (the key always exists).
        const int note = anchor + key.root_pc + theory::degree_to_semitones(key.mode, ev.tone) +
                         12 * ev.octave + transpose;
        return (note < 0 || note > 127) ? -1 : note;
      }
      case NoteSource::kChordTone:
      default: {
        if (!chord.valid || ev.tone < 0) {
          return -1;  // silent until a chord exists
        }
        const ChordShape shape = theory::shape_of(chord.quality);
        if (shape.count == 0) {
          return -1;
        }
        const std::uint8_t wrap = static_cast<std::uint8_t>(ev.tone / shape.count);
        const std::uint8_t offset = shape.offsets[ev.tone % shape.count];
        const int note = anchor + chord.root_pc + offset + 12 * (ev.octave + wrap) + transpose;
        return (note < 0 || note > 127) ? -1 : note;
      }
    }
  }

  // Default register anchor per role (MIDI note of chord-tone 0 at octave 0),
  // so stacked tonal roles don't all pile into one octave = timbral mush. Bass
  // sits low; pad fills the gap under the mid comp; arp/lead/phrase sit above.
  // StyleEvent.octave still fine-tunes per pattern; kFixed roles ignore this.
  // kBass(36) and kChord1(60) keep their historical registers.
  static constexpr int kRoleAnchor[kRoleCount] = {
      60,  // kDrums  (fixed; unused)
      60,  // kPerc   (fixed; unused)
      36,  // kBass
      60,  // kChord1
      60,  // kChord2
      48,  // kPad
      72,  // kArp
      72,  // kPhrase
      72,  // kLead
      60,  // kCc     (unused)
  };

  const Style* m_style = nullptr;
  // Phase-5 Item #9 (Corelli fix #1): the live backing for
  // Performance::style_id -- 0xFFFF ("unknown/compiled") until load(idx) sets
  // a concrete builtin index; any pointer-based style assignment
  // (load_style/request_style) resets it to 0xFFFF (see those methods' own
  // comments).
  std::uint16_t m_style_id = 0xFFFF;
  const Style* m_pending_style = nullptr;  // queued with m_pending for a seamless switch
  SectionType m_current = SectionType::kVarA;
  SectionType m_return_to = SectionType::kVarA;
  SectionType m_pending = SectionType::kVarA;
  bool m_pending_valid = false;
  Tick m_section_start = 0;
  Route m_routes[kRoleCount]{};
  std::uint16_t m_muted = 0;  // per-role mute bitmask (kRoleCount bits)
  std::uint16_t m_solo = 0;   // per-role solo bitmask
  GrooveParams m_groove;      // global groove feel applied to every part
  // Phase-6 Theme 3 Item #1: global transpose, semitones, default 0 (no-op).
  // set_master_transpose()'s doc comment above traces how resolve() applies it.
  std::int8_t m_master_transpose = 0;
  VoicingState m_voicing;  // per-role voice-leading memory (D40)
  // Phase-5 Item #10: one MIDI-FX insert chain per role. Default-constructed
  // (every slot inert/passthrough), so a fresh Arranger's on_tick output is
  // byte-identical to the pre-Item-#10 schedule until a chain is configured.
  InsertChain m_chain[kRoleCount];
  // Phase-6 Theme 4 (5220, Fork 3/4): one arp-insert engine per ROLE, beside
  // m_chain -- NOT one per slot (a role plays ONE arpeggiated pattern, not
  // several independently-clocked ones). SESSION-ONLY: never captured by
  // Performance (mirrors the live-keyboard arp's own m_arp, engine.hpp, which
  // is equally never captured) -- a kArp-typed slot's wire-persisted CONFIG
  // (rate/direction/octaves/gate) rides the existing PerfInsert bytes
  // automatically; this array's held notes/step counter do not.
  ArpeggiatorEngine m_role_arp[kRoleCount];
  // Motif engine (9210): how many times the CURRENT section has looped back
  // to itself (statement=even, answer=odd -- motif.hpp's call-and-response
  // policy). One scalar suffices because every StylePattern in a section
  // shares that section's own `bars` length, so they all loop in lockstep;
  // reset on style load and transport start like m_voicing.
  std::uint32_t m_motif_repeat = 0;
};

}  // namespace arrangrr

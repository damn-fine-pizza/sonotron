#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"
#include "arrangrr/arp/arpeggiator.hpp"
#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/chord/chord_engine.hpp"
#include "arrangrr/chord/chord_sequencer.hpp"
#include "arrangrr/clip/clip_matrix.hpp"
#include "arrangrr/common/boundary_latch.hpp"
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/pad/pad_bank.hpp"
#include "arrangrr/perf/performance.hpp"
#include "arrangrr/routing/note_tracker.hpp"
#include "arrangrr/routing/router.hpp"
#include "arrangrr/timeline/timeline.hpp"
#include "chorddet/followed_context.hpp"
#include "chorddet/stage.hpp"
#include "common/time.hpp"
#include "runtime/midi_parser.hpp"
#include "runtime/out_scheduler.hpp"
#include "runtime/stage.hpp"
#include "runtime/transport.hpp"

// The engine: wires parser → router → scheduler → tracker behind the binary
// ABI. Time is injected (advance_ticks); the same code runs under the host
// real-clock thread, the virtual deterministic clock, and the STM32 timer.
//
// Two timelines (D29): `now_` is stream time and always advances with
// advance_ticks — scheduled events fire against it even when the transport is
// stopped (golden G1 plays chords with the transport idle). The transport's
// musical tick advances only while playing.

namespace arrangrr {

// ChordFollow (the D47 harmony-source selector) and Producer now live with
// their single owner in chorddet/followed_context.hpp (included via
// chord_engine.hpp, and directly above for m_followed itself): the gate they
// drive is enforced inside FollowedContext, not threaded as bools through
// the engine.

// Dxx (un-defers D34(b) / §11 split): per-input-port harmony zone. This is the
// "observe-without-route" split as DATA — the zone of a whole port decides
// route-vs-suppress, riding the SAME parsed input path (no parallel
// push_harmony_in). A future pitch split_point can later subdivide one physical
// port into two logical zones without reshaping this.
//   - kMelody: note messages ROUTE/sound normally through m_router (and steer
//     only if the port is also the detect port -> the D34 FullKeyboard branch).
//   - kHarmony: note messages are SUPPRESSED from output (never reach m_router)
//     while still OBSERVED by the detector when it is the detect port -> silent
//     re-harmonization (the canonical Split arranger zone).
// Default kMelody keeps every port byte-identical to the legacy path.
enum class InputZone : std::uint8_t {
  kMelody = 0,   // routes/sounds; observed only as the D34 FullKeyboard branch
  kHarmony = 1,  // observed + suppressed from output (silent chord zone)
};

// Phase-6 Theme 3 Item #2 (docs/reflections/phase6-theme3-pad-drum-cc-scope.md,
// Decision 3): a kOneShot/kLoop Drum pad has no gate/duration field of its own
// (Pad is pinned at 12 bytes, D33) -- this fixed engine-side constant stands
// in, grounded in the corpus's OWN authored kick/snare gate length
// (arrangrr/arranger/styles/basic.hpp's kVarBDrums: gate=120 at kPpqn=960,
// common/time.hpp), not an invented number. Public (not file-local to
// engine.cpp) so functional tests can assert against it by name instead of a
// bare literal.
inline constexpr Tick kPadDrumOneShotGateTicks = 120;
// Decision 1: source_aux 0 means "use the default" for a Drum pad's velocity
// (0 is not itself a usable MIDI velocity -- it is indistinguishable from "no
// note"), matching a mid-range authored velocity in the same corpus table.
inline constexpr std::uint8_t kPadDrumDefaultVelocity = 100;

// The arrangrr Stage-adapter: the arranger stage that `runtime::Runtime<Engine,
// N>` drives (docs/design/orchestrator-pipeline-extraction.md §3.5/§14). Phase
// 1 runtime extraction: `Transport`/`OutScheduler` no longer live here as VALUE
// members — Runtime owns the one instance of each and injects them BY
// REFERENCE at construction (§14.3's precedent, extended to Transport per
// runtime/stage.hpp's header comment). Every method that used to read/mutate
// `m_transport`/`m_scheduler` keeps its BODY unchanged; only the declaration
// site of the two members changed from value to reference.
//
// Phase-4b promotion (§16.1/§16.2c/§16.4, Seam C): live piano->chord
// recognition (`ChordDetector`) and its owner (`FollowedContext`) moved to
// the peer `components/chorddet` component.
//
// Phase-4d (§16.3/§16.4/§16.9): `ChorddetStage` stops being an Engine-owned
// VALUE member (`m_chorddet`) and becomes a Pipeline-owned SIBLING stage,
// injected here BY REFERENCE -- the SAME shared-reference idiom already used
// for Transport/OutScheduler above. `FollowedContext` is ALSO Pipeline-owned
// now (was an Engine-owned value before 4d) and injected by reference,
// shared with the chorddet peer -- same-tick visibility (D53) falls out of
// it being the SAME object, not a message a tick late. `m_chorddet` is kept
// here ONLY for its narrow CONFIG surface (set_key/set_min_notes/
// set_single_finger/clear/enabled/held_count via chord_key_set/chord_mode/
// cmd_routing's kPanic/the set_chord_detect family/fire_chord_seq's
// live-priority check) -- these are single-target ABI command forwards
// arrangrr's OWN `push_command` dispatch must still route (chorddet cannot
// understand `Command`/`Param`, D43), NOT a data-flow concern. The DATA-FLOW
// specific coupling (the raw inbound MIDI byte stream reaching chorddet's
// OWN parser) is what Seam C moves OUT of `push_midi_in` and UP to
// `runtime::Pipeline`'s fan-out instead (see pipeline.hpp): `push_midi_in`
// below is now routing-only and no longer calls into `m_chorddet` at all.
class Engine {
 public:
  // Monomorphic sink at the ABI boundary (D26): one instantiation, no
  // template bloat in flash; the callable is owned by the caller.
  using EventSink = FunctionRef<void(const OutEvent&)>;

  Engine(OutScheduler<kSchedulerCapacity>& scheduler, Transport& transport,
         FollowedContext& followed, ChorddetStage<kMaxPorts>& chorddet) noexcept
      : m_transport(transport),
        m_followed(followed),
        m_chords(m_followed),
        m_chorddet(chorddet),
        m_scheduler(scheduler) {}

  constexpr Tick now() const noexcept { return m_now; }
  constexpr const Transport& transport() const noexcept { return m_transport; }
  const Timeline& timeline() const noexcept { return m_timeline; }
  const ChordEngine& chords() const noexcept { return m_chords; }
  const ChordSequencer& sequences() const noexcept { return m_seq; }
  const Arranger& arranger() const noexcept { return m_arranger; }
  // Phase-5 Item #2: the Repeat-Zone launch primitive (mirrors arp()'s own
  // mutable+const accessor pair). Host/test code registers clips directly
  // through the mutable accessor (kClipAdd's own in-engine equivalent, see
  // cmd_clip in engine.cpp for the ABI path).
  const ClipMatrix& clips() const noexcept { return m_clips; }
  ClipMatrix& clips() noexcept { return m_clips; }
  // Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
  // Performance"): pad-bank wrapper bookkeeping and the Performance recall
  // store, mirroring clips()' own const+mutable accessor pair. Host code
  // configures pads and (de)serializes the store through the mutable
  // accessor; the ABI paths are cmd_pad/cmd_perf below.
  const PadEngine& pads() const noexcept { return m_pads; }
  PadEngine& pads() noexcept { return m_pads; }
  // Phase-6 Theme 3 Item #4: the active pad bank (a VIEW CURSOR only, see
  // kPadBankSelect in abi.hpp) -- 0..kMaxPadBanks-1.
  std::uint16_t pad_bank() const noexcept { return m_pad_bank; }
  const PerformanceStore& performances() const noexcept { return m_perfs; }
  PerformanceStore& performances() noexcept { return m_perfs; }

  // Feeds raw MIDI bytes from an input port. Parsed messages are routed and
  // scheduled at the current tick; due events are flushed to the sink at the
  // end of the batch.
  //
  // Seam C (docs/design/orchestrator-pipeline-extraction.md §16.4): routing
  // (below) and chord recognition used to happen in ONE method sharing ONE
  // parsed message (pre-4b), then, in 4b, in one method that called INTO the
  // chorddet peer as a sub-step. As of 4d this method is ROUTING-ONLY --
  // arrangrr's own concern (routing/arp-capture/harmony-suppress) through
  // arrangrr's OWN `m_parsers`, nothing else. Reaching chorddet's OWN
  // `push_midi_in` (its own MidiParser, never shared with the one below) is
  // now `runtime::Pipeline`'s job (its fan-out calls this method for the
  // terminal stage and chorddet's `push_midi_in` for the peer, bridging
  // chorddet's steer callback into THIS class's own `emit_chord_followed` --
  // see pipeline.hpp). This keeps the single dedup-latched emit point here,
  // unique, never duplicated (Corelli's §16.9 points 2/3).
  void push_midi_in(std::uint8_t port, Span<const std::uint8_t> bytes, EventSink sink) {
    if (port >= kMaxPorts) {
      return;
    }
    for (std::uint8_t byte : bytes) {
      m_parsers[port].feed(byte, [&](const MidiMessage& msg) {
        // The arpeggiator CAPTURES notes on its input port (they feed the arp
        // instead of routing straight through); non-note messages and other
        // ports route normally.
        // Capture ONLY while the transport is playing: the arp only sounds from
        // fire_arp (which runs when playing), so if it swallowed the keyboard
        // with the transport stopped the held keys would light up but never
        // sound. Stopped => notes pass through and play normally.
        const bool arp_captures =
            m_arp_enabled && m_transport.playing() && port == m_arp_in_port && is_note_message(msg);
        // A note on a kHarmony port is a silent chord-recognition gesture: it is
        // OBSERVED (by chorddet, below) but SUPPRESSED from output — it never
        // reaches the router. Non-note traffic (CC, sustain, program) still
        // passes; only the sounding notes are silenced. kMelody ports route
        // exactly as before.
        const bool harmony_suppress =
            is_note_message(msg) && m_input_zone[port] == InputZone::kHarmony;
        if (arp_captures) {
          observe_arp_input(msg);
        } else if (!harmony_suppress) {
          m_router.route(port, msg, [&](std::uint8_t out_port, const MidiMessage& routed) {
            schedule_or_warn(out_port, m_now, routed, sink);
          });
        }
      });
    }
    flush(sink);
  }

  // Live arpeggiator control: enable + input port (the keyboard it listens to).
  void set_arp_enabled(bool enabled, std::uint8_t in_port) noexcept {
    if (in_port < kMaxPorts) {
      m_arp_in_port = in_port;
    }
    if (!enabled && m_arp_enabled) {
      m_arp.panic();  // dropping the effect clears the held set
    }
    m_arp_enabled = enabled;
  }
  constexpr bool arp_enabled() const noexcept { return m_arp_enabled; }
  void set_arp_out(std::uint8_t port, std::uint8_t channel) noexcept {
    if (port < kMaxPorts && channel <= 15) {
      m_arp_out_port = port;
      m_arp_out_channel = channel;
    }
  }
  const ArpeggiatorEngine& arp() const noexcept { return m_arp; }
  ArpeggiatorEngine& arp() noexcept { return m_arp; }

  // Live piano->chord (kChordDetect): whether held notes on the detect port
  // re-harmonize the arranger. `port` selects which input keyboard is the
  // chord source. Toggling on resets the held-note set but never the latched
  // chord (chord memory persists). Phase-4b: thin forward to the chorddet
  // peer, which now owns this config (§16.4).
  void set_chord_detect(bool enabled, std::uint8_t port) noexcept {
    m_chorddet.set_enabled(enabled, port);
  }
  constexpr bool chord_detect() const noexcept { return m_chorddet.enabled(); }

  // Dxx: the harmony zone of a whole input port. kHarmony suppresses the port's
  // note output (silent chord recognition); kMelody routes/sounds. Independent
  // of set_chord_detect (which decides which port OBSERVES): the Split default
  // is the host's chords-panel port set to kHarmony AND to the detect port; a
  // detect port left kMelody is the D34 FullKeyboard branch (sounds + steers).
  // Host wiring (which port is the piano vs the chords panel) is next-phase.
  constexpr void set_input_zone(std::uint8_t port, InputZone zone) noexcept {
    if (port < kMaxPorts) {
      m_input_zone[port] = zone;
    }
  }
  constexpr InputZone input_zone(std::uint8_t port) const noexcept {
    return port < kMaxPorts ? m_input_zone[port] : InputZone::kMelody;
  }

  // D47 chord-follow selector: which producer may steer the followed chord.
  // Owned by the FollowedContext inside m_chords (the single gate).
  constexpr void set_chord_follow(ChordFollow follow) noexcept { m_chords.set_follow(follow); }
  constexpr ChordFollow chord_follow() const noexcept { return m_chords.follow(); }

  // Input model: a live-detect chord commits IMMEDIATELY by default (`current`
  // changes now); a SHIFT-staged one is quantized to the next bar (`next`).
  // The host sets this per keypress (shift held -> quantize). Immediate is the
  // default, so raw MIDI in without a host steers `current` at once.
  constexpr void set_detect_quantize(bool quantize) noexcept { m_chorddet.set_quantize(quantize); }
  constexpr bool detect_quantize() const noexcept { return m_chorddet.quantize(); }
  // Live piano->chord detector state, for host display (how many keys are held
  // and how many are needed before a chord is named — fingered 3 vs single 1).
  constexpr std::uint8_t chord_held_count() const noexcept { return m_chorddet.held_count(); }
  constexpr std::uint8_t chord_min_notes() const noexcept { return m_chorddet.min_notes(); }

  // Applies one binary command (D26). Sink receives any resulting events.
  // Implemented in engine.cpp as per-domain handlers: the dispatch stays a
  // switch, the 300-line god-switch does not.
  void push_command(const Command& cmd, EventSink sink);

  // Schedules a message on the stream timeline at an absolute tick (used by
  // the host for @tick-scheduled script lines).
  void schedule_at(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    schedule_or_warn(port, tick, msg, sink);
    flush(sink);  // fire immediately if already due
  }

  // The STAGE port (runtime/stage.hpp's StageLike concept): reacts to ONE
  // stream tick. `runtime::Runtime<Engine, N>` calls this once per tick
  // (after advancing its own m_now / the shared Transport's tick if playing),
  // then calls `flush()` unconditionally — mirroring EXACTLY the body that
  // used to live inline in `Engine::advance_ticks` (engine.hpp pre-Phase-1):
  // the transport-gated fire loop is unchanged, only split at the boundary
  // Runtime now owns (the raw tick-counting loop itself).
  void on_tick(const runtime::StageContext& ctx, EventSink sink) {
    m_now = ctx.now;
    if (!m_transport.playing()) {
      return;
    }
    if (Transport::is_midi_clock_tick(m_transport.tick())) {
      fire_clock_pulse(sink);
    }
    fire_timeline(m_transport.tick(), sink);
    fire_chord_seq(m_transport.tick(), sink);
    // D53: at the bar downbeat, promote any SHIFT-staged chord into the
    // followed context BEFORE the arranger fires the bar — the same
    // bar-boundary point at which the arranger applies pending style/section
    // switches, ordered so the arranger reads the freshly-committed chord.
    // Phase 7 (node T0): the bar-boundary gate reads the LIVE Transport time
    // signature instead of the compile-time kTicksPerBar constant -- a
    // default-constructed Transport computes the identical value (common/
    // time.hpp's own byte-identity gate), so this is byte-identical until
    // something genuinely calls set_time_sig with a non-4/4 value.
    if (m_transport.tick() % m_transport.ticks_per_bar() == 0) {
      const bool had_pending = m_chords.pending().valid;
      const Producer promoted_by = m_chords.pending_source();
      m_chords.commit_bar();
      // Announce ONLY a genuine promotion (a staged chord landing in
      // `current`), attributed to the producer that staged it. With nothing
      // staged there is no change to report — and pending_source would be
      // stale — so the emit is gated on had_pending, not just the delta.
      if (had_pending) {
        emit_chord_followed(promoted_by, sink);
      }
      // Phase-5 Item #2 (decision 2): promote any armed/queued-stop clip
      // whose quantize window closes THIS bar, still BEFORE fire_arranger --
      // same reason the chord commit above precedes it.
      fire_clips(m_transport.tick(), sink);
      // Phase-5 Item #9 (Corelli fix #3): a pending Performance recall lands
      // HERE -- after fire_clips, still BEFORE fire_arranger. A mid-bar
      // recall that changed the style before fire_clips ran would resolve a
      // clip's content_index against the OLD style; landing after fire_clips
      // but before fire_arranger keeps both correct for the bar that is
      // about to play.
      apply_pending_performance_recall(sink);
      // Torquato finding 1: any kDrum/kCC pad armed for this bar (see
      // fire_pad's kDrum/kCC case) fires now -- same bar-boundary point as
      // the Performance recall promotion right above, order-independent
      // since a Drum/CC hit has no cross-subsystem read/write the way a
      // recall's style/section change does.
      apply_pending_pad_fires(sink);
    }
    fire_arranger(m_transport.tick(), sink);
    fire_arp(m_transport.tick(), sink);
  }

  // Drains every event due at `m_now` in D29 total order. Public: it is the
  // other half of the StageLike concept `runtime::Runtime` calls every tick
  // (was private, called only from the old inline advance_ticks). Body
  // UNCHANGED.
  void flush(EventSink sink) {
    m_scheduler.pop_due(m_now, [&](const ScheduledEvent& ev) {
      m_tracker.observe(ev.port, ev.msg);
      sink(OutEvent::midi(ev.port, ev.msg, ev.tick));
    });
  }

  // The SINGLE emit point for the followed-context change event (kChordFollowed):
  // all four producers (manual play, sequencer, live detect, bar-promote) funnel
  // through here so their wire shape and delta policy cannot diverge. Emits ONLY
  // when the current OR pending followed chord actually changed since the last
  // emit — never one event per tick. `src` names the producer responsible.
  //
  // Public (4d): `runtime::Pipeline`'s inbound-MIDI fan-out (pipeline.hpp)
  // bridges the chorddet peer's own steer callback straight into this method
  // -- the dedup latch below stays the ONE, unique emit point, never
  // duplicated on the chorddet side (Corelli's §16.9 points 2/3).
  void emit_chord_followed(Producer src, EventSink sink) {
    const ChordState& cur = m_chords.state();
    const ChordState& next = m_chords.pending();
    if (m_followed_emitted && same_chord_state(cur, m_last_followed_cur) &&
        same_chord_state(next, m_last_followed_next)) {
      return;  // no delta: do not spam an event every tick
    }
    m_last_followed_cur = cur;
    m_last_followed_next = next;
    m_followed_emitted = true;
    sink(OutEvent::chord_followed(cur, next, src, m_now));
  }

 private:
  // Per-domain command handlers (engine.cpp).
  void cmd_transport(const Command& cmd, EventSink sink);
  void cmd_routing(const Command& cmd, EventSink sink);
  void cmd_chord(const Command& cmd, EventSink sink);
  void cmd_seq(const Command& cmd, EventSink sink);
  void cmd_track(const Command& cmd, EventSink sink);
  void cmd_style(const Command& cmd, EventSink sink);
  void cmd_voice(const Command& cmd, EventSink sink);  // program change (voice select)
  void cmd_arp(const Command& cmd, EventSink sink);    // live arpeggiator
  void cmd_clip(const Command& cmd, EventSink sink);   // Phase-5 Item #2: clip launch primitive
  void cmd_pad(const Command& cmd, EventSink sink);    // Phase-5 Item #9: pad-bank wrapper dispatch
  void cmd_perf(const Command& cmd, EventSink sink);   // Phase-5 Item #9: Performance store/recall
  void cmd_fx(const Command& cmd, EventSink sink);     // Phase-5 Item #10: MIDI-FX insert chain
  void cmd_master_transpose(const Command& cmd, EventSink sink);  // Phase-6 Theme 3 Item #1

  // cmd_chord case handlers, split out to keep cmd_chord's own cognitive
  // complexity under the clang-tidy gate (each case validates + dispatches on
  // its own, no behavior change from being inlined in the switch).
  void chord_key_set(const Command& cmd, EventSink sink);
  void chord_out(const Command& cmd, EventSink sink);
  void chord_mode(const Command& cmd, EventSink sink);
  void chord_detect_cmd(const Command& cmd, EventSink sink);
  void chord_follow_cmd(const Command& cmd, EventSink sink);
  void chord_input_zone(const Command& cmd, EventSink sink);
  void chord_play(const Command& cmd, EventSink sink);

  // cmd_seq case handlers split out for the same reason.
  void seq_add(const Command& cmd, EventSink sink);
  void seq_stop(const Command& cmd, EventSink sink);
  void seq_transpose(const Command& cmd, EventSink sink);

  // cmd_style case handlers split out for the same reason.
  void style_switch(const Command& cmd, EventSink sink);
  void style_route(const Command& cmd, EventSink sink);

  // cmd_clip case handlers (Phase-5 Item #2), split out for the same reason.
  void clip_add(const Command& cmd, EventSink sink);
  void clip_launch(const Command& cmd, EventSink sink);
  void clip_stop(const Command& cmd, EventSink sink);
  void clip_scene_launch(const Command& cmd, EventSink sink);
  // Shared launch/stop request path: applies the effect immediately
  // (Boundary::kImmediate) or arms ClipMatrix for the quantize boundary
  // (kNextBar/kNextNBars), emitting the kClip echo either way.
  void clip_request(std::size_t id, LaunchState target, const Command& cmd, EventSink sink);
  // Drives the underlying content (Arranger/ChordSequencer/Timeline) a fired
  // clip references -- the ONE place that translates {kind, content_index}
  // into a real musical effect, per ClipMatrix's own scope tripwire.
  void apply_clip_content(const Clip& clip, LaunchState target, EventSink sink);
  // Promotes any clip whose quantize window closes THIS bar (on_tick's
  // existing tick % kTicksPerBar == 0 gate, decision #2).
  void fire_clips(Tick transport_tick, EventSink sink);

  // cmd_pad case handlers (Phase-5 Item #9), split out for the same reason.
  void pad_assign(const Command& cmd, EventSink sink);
  void pad_trigger(const Command& cmd, EventSink sink);
  void pad_release(const Command& cmd, EventSink sink);
  // Phase-6 Theme 3 Item #4: kPadBankSelect's own handler, mirroring
  // cmd_master_transpose's validate-or-reject style.
  void pad_bank_select(const Command& cmd, EventSink sink);
  // Fires (or stops) one pad's wrapped content by type -- the ONE place that
  // translates a Pad into a call on the verb it wraps (clip_request/
  // clip_scene_launch/Arranger::request/perf_recall), mirroring
  // apply_clip_content's own placement for the clip primitive. `pad_id` is
  // the flat pad slot (cmd.idx) -- needed ONLY by the kDrum/kCC arm path
  // below (Torquato finding 1) to key m_pad_latch/m_pad_pending_target; every
  // other case ignores it, exactly like ClipMatrix/Arranger's own id-indexed
  // wrapper verbs.
  void fire_pad(const Pad& pad, LaunchState target, std::uint16_t pad_id, EventSink sink);
  // fire_pad's kDrum/kCC cases (Phase-6 Theme 3 Item #2), split out to keep
  // fire_pad's own cognitive complexity under the clang-tidy gate -- same
  // discipline as every other case-handler split in this class. Each is the
  // ONE place that builds a MidiMessage directly (Decision 5, docs/
  // reflections/phase6-theme3-pad-drum-cc-scope.md), emitting via the SAME
  // schedule_or_warn choke point every other emission path shares. Called
  // EITHER immediately from fire_pad's own kImmediate branch, or later from
  // apply_pending_pad_fires when a kNextBar/kNextNBars arm comes due -- the
  // pad.sync check that decides which happens lives in fire_pad, not here.
  void fire_pad_drum(const Pad& pad, LaunchState target, EventSink sink);
  void fire_pad_cc(const Pad& pad, LaunchState target, EventSink sink);
  // Promotes any armed kDrum/kCC pad fire (Torquato finding 1: pad.sync was
  // previously ignored by these two types) whose BoundaryLatch closes THIS
  // bar -- called from on_tick's existing bar gate, alongside
  // apply_pending_performance_recall, which it mirrors exactly (one shared
  // BoundaryLatch instance per pad slot instead of the single m_perf_recall
  // one, since several pads can be armed concurrently for different
  // boundaries).
  void apply_pending_pad_fires(EventSink sink);

  // cmd_perf case handlers (Phase-5 Item #9), split out for the same reason.
  void perf_store(const Command& cmd, EventSink sink);
  void perf_recall(const Command& cmd, EventSink sink);
  // Captures/validates/applies a full rig snapshot -- see performance.hpp's
  // own header comment for the persisted-format discipline these back.
  Performance capture_performance() const;
  bool validate_performance(const Performance& perf) const noexcept;
  bool apply_performance(const Performance& perf, EventSink sink);
  void emit_performance_confirmation(const Performance& perf, EventSink sink);
  // Promotes a pending (armed) Performance recall whose BoundaryLatch closes
  // THIS bar -- called from on_tick's existing bar gate, AFTER fire_clips and
  // BEFORE fire_arranger (Corelli fix #3, see on_tick's own comment above).
  void apply_pending_performance_recall(EventSink sink);

  // cmd_fx case handlers (Phase-5 Item #10), split out for the same reason.
  // All four are thin ABI-unpack + bounds-check + Arranger-setter forwarders
  // -- idx = TrackRole in every case (see abi.hpp's kFxSet/kFxParam/
  // kFxEnable/kFxClear comments for the exact a/b/c packing).
  void fx_set(const Command& cmd, EventSink sink);
  void fx_param(const Command& cmd, EventSink sink);
  void fx_enable(const Command& cmd, EventSink sink);
  void fx_clear(const Command& cmd, EventSink sink);

  // Emits the loaded style's default per-role GM voices on their routes. Cheap
  // and idempotent (re-sending a Program Change is a no-op on the synth), so it
  // is safe to call after a style load, a style switch, or a route change —
  // covering both "route before load" and "route after load" orders.
  void apply_arranger_voices(EventSink sink) {
    m_arranger.emit_voices([&](std::uint8_t port, TickOffset, const MidiMessage& msg) {
      schedule_or_warn(port, m_now, msg, sink);
    });
    flush(sink);
  }

  // Seeds the transport tempo from the loaded style's default (9120). Called on
  // every path that (re)loads the arranger's style — an explicit load, an
  // immediate live switch, or a deferred switch landing at the bar boundary.
  // Scheduling is tick-based, so this only sets the playback rate, never note
  // tick positions; set_bpm rejects out-of-range values (all builtins are in
  // range). This is pure internal wiring: no new ABI command is introduced.
  void apply_style_tempo() noexcept {
    if (const Style* style = m_arranger.current_style(); style != nullptr) {
      m_transport.set_bpm(style->tempo);
    }
  }

  // Sibling to apply_style_tempo (Phase 7, node T0): seeds the transport time
  // signature from the loaded style's default, called at the EXACT SAME three
  // call sites (an explicit load, an immediate live switch, or a deferred
  // switch landing at the bar boundary). Emits OutEvent::kTimeSig ONLY when
  // the value actually changes -- every existing builtin style keeps the
  // default kBeatsPerBar, so this never emits on any existing golden path.
  void apply_style_time_sig(EventSink sink) {
    if (const Style* style = m_arranger.current_style(); style != nullptr) {
      const std::uint8_t before = m_transport.time_sig().beats_per_bar;
      if (style->beats_per_bar != before && m_transport.set_time_sig(style->beats_per_bar)) {
        sink(OutEvent::time_sig(m_transport.time_sig().beats_per_bar, m_now));
      }
    }
  }

  // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): `source` is the
  // arrangrr::kScheduleSource* producer tag (config.hpp), forwarded into the
  // scheduler's own entry (OutScheduler::schedule) so cancel_note_off can
  // later scope its retrigger-care match to the SAME producer. Defaults to
  // kScheduleSourceCore, so every pre-existing caller that never passes one
  // (fire_timeline, fire_chord_seq, emit_beat/realtime/clock) keeps landing in
  // the same undifferentiated shared pool as before this fix, byte-for-byte.
  void schedule_or_warn(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink,
                        std::uint8_t source = kScheduleSourceCore) {
    if (!m_scheduler.schedule(port, tick, msg, source)) {
      sink(OutEvent::warn(WarnCode::kSchedulerFull, m_now));
    }
  }

  static constexpr bool same_chord_state(const ChordState& a, const ChordState& b) noexcept {
    return a.valid == b.valid && a.root_pc == b.root_pc && a.quality == b.quality;
  }

  // Everything gated on the 24-PPQN clock pulse (called from advance_ticks,
  // split out to keep its cognitive complexity under the clang-tidy gate):
  // the F8 clock byte, sent ONLY on ports enabled by the clock-out mask, and
  // the kBeat heartbeat (P0-2), which fires UNCONDITIONALLY when playing
  // regardless of clock-out routing -- it is a host/GUI event, not a
  // scheduled MIDI byte.
  void fire_clock_pulse(EventSink sink) {
    if (m_clock_out_mask != 0) {
      for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
        if (m_clock_out_mask & (1u << p)) {
          schedule_or_warn(p, m_now, MidiMessage::realtime(midi::kClock), sink);
        }
      }
    }
    emit_beat(sink);
  }

  // Emits the transport heartbeat (P0-2, called once per 24-PPQN pulse from
  // fire_clock_pulse while playing): derives bar/beat/pulse from the
  // transport's OWN musical position, carrying m_now as the stream `@` tick
  // like every other event. pulse is the tick-within-beat divided by the
  // MIDI clock divider (0..23), the same cadence is_midi_clock_tick gates on.
  void emit_beat(EventSink sink) {
    const Position pos = m_transport.position();
    const auto pulse = static_cast<std::uint8_t>(pos.tick / kMidiClockDivider);
    sink(OutEvent::beat(pos.bar, pos.beat, pulse, m_now));
  }

  // Transport realtime bytes (FA/FB/FC) go out immediately on clock ports.
  void emit_realtime(std::uint8_t status, EventSink sink) {
    for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
      if (m_clock_out_mask & (1u << p)) {
        schedule_or_warn(p, m_now, MidiMessage::realtime(status), sink);
      }
    }
    flush(sink);
  }

  // Pattern-driven scheduling with retrigger care (§9.B): re-firing a note
  // whose previous NoteOff is still pending would either duplicate the on or
  // get truncated by the stale off. The compensating off is anchored to the
  // INCOMING on's scheduled tick (m_now + delay), NOT to m_now — otherwise a
  // ratchet, which schedules several hits of the same note in one on_tick with
  // increasing delay, would re-emit every sub-hit's off at m_now and leave a
  // string of bare re-attacks on an already-sounding note. cancel_note_off is
  // tick-aware: it only cancels an off that lands at or after the new on, so a
  // sub-hit whose gate ends before the next hit keeps its silent gap. For the
  // normal cross-step case delay == 0, so on_tick == m_now and the wire is
  // byte-identical to the pre-fix path (D29 still sorts the off before the on).
  // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): `source` (default
  // kScheduleSourceCore, config.hpp) scopes cancel_note_off's retrigger-care
  // match to the SAME producer as the incoming note-on -- a pending note-off
  // from a DIFFERENT producer sharing this exact (port, channel, note) is left
  // untouched (see out_scheduler.hpp's own cancel_note_off comment). For every
  // pre-existing caller (all default to kScheduleSourceCore), this is the
  // SAME shared single-pool match as before this fix, so the on_tick == m_now
  // byte-identical wire this function's own header comment documents still
  // holds exactly as before.
  //
  // Torquato QA (Phase-6 Theme 4 dual-arp collision fix), second half: a
  // role's own kArp insert and the live-keyboard arp can each independently
  // hold the SAME (port, channel, note) and both retrigger on the SAME tick.
  // Each producer's OWN same-source dance above already keeps ITS OWN
  // previous note clean; cancel_off_from_other_producer additionally drops
  // any OTHER producer's note-off landing at this EXACT same tick -- that
  // off would otherwise be spurious (the pitch is not really going silent,
  // a DIFFERENT producer is attacking it again in the very same instant).
  // Scoped to an exact-tick match against a DIFFERENT source only, so a true
  // single-producer scenario (every existing golden, and the arp's own
  // retrigger tests) never has another source present here -- a provable
  // no-op for that path, unchanged from before this fix.
  void schedule_pattern(std::uint8_t port, TickOffset delay, const MidiMessage& msg, EventSink sink,
                        std::uint8_t source = kScheduleSourceCore) {
    const Tick on_tick = m_now + static_cast<Tick>(delay);
    if (msg.type() == midi::kNoteOn) {
      if (m_scheduler.cancel_note_off(port, msg.channel(), msg.d1, on_tick, source)) {
        schedule_or_warn(port, on_tick, MidiMessage::note_off(msg.channel(), msg.d1), sink, source);
      }
      m_scheduler.cancel_off_from_other_producer(port, msg.channel(), msg.d1, on_tick, source);
    }
    schedule_or_warn(port, on_tick, msg, sink, source);
  }

  void fire_timeline(Tick transport_tick, EventSink sink) {
    m_timeline.on_tick(transport_tick,
                       [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
                         schedule_pattern(port, delay, msg, sink);
                       });
  }

  void fire_chord_seq(Tick transport_tick, EventSink sink) {
    // LIVE-PRIORITY arbitration (the engine default): while a live chord is
    // actively HELD on the detect port, the band follows that live chord and the
    // sequencer must COMP on it (sound its rhythm/voicing on the live root+quality)
    // WITHOUT publishing its own chord — no clash. On release the held set empties
    // and the sequencer's next fired step resumes committing normally. This dynamic
    // held-vs-released state cannot live in FollowedContext's static gate, so the
    // decision is made here. Under any other ChordFollow value (kAuto legacy,
    // kDetect/kSequencer/kManual explicit) the sequencer behaves exactly as before:
    // it sounds its own chord and its publish is arbitrated by the D47 gate.
    const bool live_priority = m_chords.follow() == ChordFollow::kLivePriority;
    const bool live_held = m_chorddet.enabled() && m_chorddet.held_count() > 0;
    const bool comp_on_live = live_priority && live_held && m_chords.state().valid;
    m_seq.on_tick(
        transport_tick,
        [&](std::uint8_t root_note, ChordQuality quality, std::uint8_t degree, std::uint8_t vel) {
          // While comping on a held live chord, sound the LIVE root+quality and do
          // NOT steer (the live chord already owns the followed context). Otherwise
          // sound the sequencer's own chord and let the D47 gate decide the publish.
          std::uint8_t sound_root = root_note;
          ChordQuality sound_quality = quality;
          bool steer = true;
          if (comp_on_live) {
            const ChordState& live = m_chords.state();
            sound_root = static_cast<std::uint8_t>(60 + live.root_pc);
            sound_quality = live.quality;
            steer = false;
          }
          m_chords.sound(
              sound_root, sound_quality, vel,
              [&](std::uint8_t port, const MidiMessage& msg) {
                schedule_or_warn(port, m_now, msg, sink);
              },
              Producer::kSequencer, steer, /*quantize=*/false);
          sink(OutEvent::chord(m_chords.out_port(), degree,
                               static_cast<std::uint8_t>(sound_quality), sound_root,
                               theory::shape_of(sound_quality).count, vel, m_now));
          // The sequencer just steered (unless comping on a held live chord, where
          // steer==false left the context untouched — the delta check keeps this
          // silent). Announce on the delta.
          emit_chord_followed(Producer::kSequencer, sink);
        },
        [&] {
          m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
        });
  }

  void fire_arranger(Tick transport_tick, EventSink sink) {
    const Arranger::TickResult r = m_arranger.on_tick(
        transport_tick, m_chords.key(), m_chords.state(),
        // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): Arranger's
        // on_tick now carries its own kScheduleSource* producer tag per note
        // (kScheduleSourceCore for ordinary emissions, kScheduleSourceRoleArpBase
        // + role index for a role's own arp-insert) -- forward it verbatim.
        [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg, std::uint8_t source) {
          schedule_pattern(port, delay, msg, sink, source);
        },
        // Phase 7 (node T0): thread the LIVE bar length down explicitly --
        // Arranger holds no Transport&. Byte-identical to the pre-T0 default
        // (kTicksPerBar) until set_time_sig ever moves it.
        m_transport.ticks_per_bar());
    if (r.section_changed) {
      sink(OutEvent::section(static_cast<std::uint16_t>(r.section), m_now));
    }
    if (r.style_changed) {
      apply_style_tempo();  // 9120: a deferred live style switch adopts the new tempo at the bar
      apply_style_time_sig(sink);  // T0: ... and the new style's time signature, at the same bar
    }
    if (r.stop_transport) {
      m_transport.stop();
      if (m_seq.playing()) {
        m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, m_now, msg, sink);
        });
      }
      emit_realtime(midi::kStop, sink);
      sink(OutEvent::transport(static_cast<std::uint16_t>(m_transport.state()), m_now));
    }
  }

  static bool is_note_message(const MidiMessage& msg) {
    return msg.type() == midi::kNoteOn || msg.type() == midi::kNoteOff;
  }

  // Feeds a captured keyboard note into the live arpeggiator. A NoteOn vel 0 is
  // a release. The arp then plays the held set rhythmically from fire_arp.
  void observe_arp_input(const MidiMessage& msg) {
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      m_arp.note_on(msg.d1, msg.d2);
    } else {
      m_arp.note_off(msg.d1);
    }
  }

  // Clock-driven arpeggiator: on each transport tick it may emit the next arp
  // note on its output route (note-off follows after the gate).
  void fire_arp(Tick transport_tick, EventSink sink) {
    if (!m_arp_enabled) {
      return;
    }
    m_arp.on_tick(transport_tick, [&](std::uint8_t note, std::uint8_t velocity, TickOffset gate) {
      // Torquato QA (Phase-6 Theme 4 dual-arp collision fix): the Engine-
      // global live-keyboard arp is its own distinct producer -- tag it
      // kScheduleSourceLiveArp so it never cross-cancels a role's own
      // arp-insert (or ordinary Arranger/Timeline scheduling) sharing the
      // same (port, channel, note).
      schedule_pattern(m_arp_out_port, 0, MidiMessage::note_on(m_arp_out_channel, note, velocity),
                       sink, kScheduleSourceLiveArp);
      schedule_pattern(m_arp_out_port, gate, MidiMessage::note_off(m_arp_out_channel, note), sink,
                       kScheduleSourceLiveArp);
    });
  }

  Tick m_now = 0;
  Transport& m_transport;  // Runtime-owned, injected by reference (§14.3/B3)
  MidiParser m_parsers[kMaxPorts];
  Router m_router;
  Timeline m_timeline;
  // Phase-4d promotion (§16.2c/§16.9): the followed-context owner is now
  // Pipeline-owned (was an Engine-owned value in 4b), injected here BY
  // REFERENCE and shared BY REFERENCE with m_chords (writer for
  // kSequencer/kManual) -- the SAME shared-reference idiom already used for
  // Transport/OutScheduler (§14.3). Declared BEFORE m_chords so member
  // initialization (always DECLARATION order, never the ctor init-list
  // order) binds it first.
  FollowedContext& m_followed;
  ChordEngine m_chords;
  ChordSequencer m_seq;
  Arranger m_arranger;
  // Phase-5 Item #2 (docs/design/clip-primitive-design.md decision 1): the
  // Repeat-Zone launch primitive, an Engine-owned VALUE member mirroring
  // m_arp below (NOT a Pipeline peer -- Clip orchestrates subsystems Engine
  // already owns and shares no raw cross-stage data). Declared after
  // m_arranger/m_seq/m_timeline so a future audit of construction order
  // finds it beside the subsystems it references by index.
  ClipMatrix m_clips;
  // Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
  // Performance"): pad-bank wrapper bookkeeping (mirrors m_clips' own
  // placement/rationale -- PadEngine orchestrates subsystems Engine already
  // owns and shares no raw cross-stage data).
  PadEngine m_pads;
  // Performance recall store + its pending-boundary latch (Corelli fix #2:
  // the ONE shared BoundaryLatch primitive, not a 4th hand-rolled
  // arm-at-boundary mechanism). ClipMatrix/Arranger/ChordEngine each still
  // hand-roll their OWN boundary state (LaunchState::kArmed, m_pending_valid,
  // FollowedContext staging) -- they could adopt BoundaryLatch later; not
  // retrofitted here so their existing goldens stay byte-identical.
  PerformanceStore m_perfs;
  BoundaryLatch m_perf_recall;
  std::uint16_t m_perf_recall_slot = 0;
  // Torquato finding 1 (Phase-6 Theme 3 Item #2 hand-off): kDrum/kCC's own
  // arm-at-boundary state, ONE BoundaryLatch per flat pad slot (unlike
  // m_perf_recall's single instance -- several pads can be armed
  // concurrently for independent boundaries). Reuses the SAME shared
  // BoundaryLatch primitive m_perf_recall already uses (Corelli fix #2),
  // not a new hand-rolled arm mechanism. m_pad_pending_target holds the
  // LaunchState (kPlaying/kStopped) the armed pad will fire with once its
  // latch comes due -- see fire_pad's kDrum/kCC case and
  // apply_pending_pad_fires below.
  BoundaryLatch m_pad_latch[kMaxPads]{};
  LaunchState m_pad_pending_target[kMaxPads]{};
  // Phase-6 Theme 3 Item #4: the active pad bank, a persisted VIEW CURSOR
  // (0..kMaxPadBanks-1) selected by the kPadBankSelect ABI verb
  // (pad_bank_select) and captured/restored via Performance::pad_bank_id
  // (capture_performance/apply_performance) -- flat pad addressing
  // (kPadAssign/kPadTrigger/kPadRelease, 0..kMaxPads-1) is unaffected by
  // this value; the host maps its physical pad surface to flat ids.
  std::uint16_t m_pad_bank = 0;
  // Phase-4d promotion (§16.1/§16.4/§16.9): the chorddet peer is now a
  // Pipeline-owned SIBLING stage (was an Engine-owned value in 4b), injected
  // here BY REFERENCE for its narrow CONFIG surface only (see the class
  // header comment) -- its own ChordDetector and per-port MidiParser array
  // live in the Pipeline-owned instance now, not here. Config
  // (enabled/port/quantize) is still reached from here; set_chord_detect/
  // chord_detect/set_detect_quantize/detect_quantize/chord_held_count/
  // chord_min_notes above are thin forwards, unchanged.
  ChorddetStage<kMaxPorts>& m_chorddet;
  InputZone m_input_zone[kMaxPorts] = {};  // Dxx: per-port harmony zone (kMelody default)
  ArpeggiatorEngine m_arp;                 // live keyboard arpeggiator
  bool m_arp_enabled = false;              // kArp: capture + play the input port
  std::uint8_t m_arp_in_port = 0;          // keyboard the arp listens to
  std::uint8_t m_arp_out_port = 0;         // where the arp plays
  std::uint8_t m_arp_out_channel = 0;      // 0-based
  NoteTracker m_tracker;
  OutScheduler<kSchedulerCapacity>& m_scheduler;  // Runtime-owned, injected by reference (§14.3)
  std::uint8_t m_clock_out_mask = 0;              // off by default; enabled via kClockOutMask
  // Last-emitted followed context, so kChordFollowed fires only on a real delta.
  ChordState m_last_followed_cur{};
  ChordState m_last_followed_next{};
  bool m_followed_emitted = false;
};

}  // namespace arrangrr

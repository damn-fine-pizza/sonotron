#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"              // kMaxInserts (the committed RESERVED symbol, node 5100)
#include "arrangrr/arp/arpeggiator.hpp"  // ArpeggiatorEngine, ArpRate/ArpDirection (Phase-6 Theme 4)
#include "arrangrr/arranger/groove.hpp"  // GrooveParams, groove::apply (Phase-6 Theme 4, 5210)
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"  // kMaxChainFan
#include "chorddet/theory.hpp"  // Key, ChordState, theory::scale_of
#include "common/assert.hpp"    // ARR_ASSERT
#include "common/time.hpp"      // Tick, TickOffset

// MIDI-FX insert chain (Phase-5 Item #10, node 5100/5200): per-note inserts
// (ScaleLock/VelocityProc/Echo/NoteRepeat) grafted onto the Arranger's D40
// resolution pipeline, per TrackRole (the SAME ordinal space as Arranger::
// m_routes -- Corelli's concrete-shape review, locked scope: per-ROLE
// addressing, NOT per-Timeline-Track). Phase-6 Theme 4 (docs/reflections/
// phase6-theme4-insert-interface-fork.md, node 5210/5220) adds TWO more
// InsertType values that are NOT pure stream-transforms:
//   - kGroove (5210): a pure, single-call transform exactly like the
//     original four (groove::apply has no session state) -- it fits
//     process() unchanged, it is only listed here because it is NEW, not
//     because it needed the capability widening below.
//   - kArp (5220): genuinely time-driven -- it must fire on ticks where no
//     resolved input note exists at all, which process()'s "one call, in ->
//     N out" contract cannot express. Insert therefore gains TWO optional,
//     switch-dispatched capabilities (Fork 1's "option 2", NOT SFINAE --
//     `type` is a runtime tag, not a distinct C++ type, so dispatch is a
//     runtime switch, not compile-time capability probing):
//       ingest()  -- feeds one resolved note into the slot's own held state
//                    (kArp's note_on equivalent); a genuine no-op for every
//                    other type.
//       on_tick() -- fires once per transport tick, INDEPENDENT of whether
//                    the grid produced any event this tick; a genuine no-op
//                    for every other type.
// process()/ingest()/on_tick() all copy `in` verbatim for every field they do
// not touch (`out[0] = in` / `FxNote n = in`), so FxNote's own additive
// fields (groove_offset below) free-ride through every existing type
// unchanged -- the same discipline the original four already established.
//
// FX persistence: a wire-mirrored PerfInsert[kRoleCount][kMaxInserts] array
// (arrangrr/perf/performance.hpp, Performance format_version 2, Phase-6 Theme
// 3 Item #3) -- captured via Arranger::chain()'s read accessor, restored via
// Arranger::restore_fx()'s companion write accessor (both below), NOT through
// set_type()/set_param()'s semantic per-field API. kArp's own SESSION state
// (held notes, step counter) never rides this wire -- it lives in a separate,
// per-role ArpeggiatorEngine array (Arranger::m_role_arp), same discipline as
// the live-keyboard arp's own m_arp never being captured either.

namespace arrangrr {

enum class InsertType : std::uint8_t {
  kScaleLock = 0,
  kVelocityProc = 1,
  kEcho = 2,
  kNoteRepeat = 3,
  kGroove = 4,  // Phase-6 Theme 4 (5210): groove-as-insert, pure/single-call
  kArp = 5,     // Phase-6 Theme 4 (5220): arp-as-insert, ingest()/on_tick()
};
inline constexpr std::uint8_t kInsertTypeCount = 6;

// v1: a snap-to-key GATE, not a graded blend -- `strength == 0` is an exact
// passthrough (required for the empty/default-chain byte-identity guarantee,
// see the header's own note below); any nonzero strength snaps the note
// fully to the nearest in-key pitch of FxContext::key. A graded/probabilistic
// strength is future work (root/mode override also reserved -- v1 always
// snaps to the LIVE key).
struct ScaleLockParams {
  std::uint8_t strength = 0;
  std::uint8_t reserved[3] = {0, 0, 0};
};

enum class VelocityProcMode : std::uint8_t {
  kScale = 0,           // amount = percent of the input velocity (0..~255%)
  kFixed = 1,           // amount = the literal output velocity
  kCompressStatic = 2,  // amount = percent pull toward the static center (64)
};

struct VelocityProcParams {
  std::uint8_t mode = static_cast<std::uint8_t>(VelocityProcMode::kScale);
  std::uint8_t amount = 100;
  std::uint8_t reserved[2] = {0, 0};
};

// Seed + `repeats` decaying echo copies, each `delay_ticks` further out;
// velocity decays geometrically by vel_decay/255 per repeat (255 = no decay).
struct EchoParams {
  std::uint8_t repeats = 0;
  std::uint8_t vel_decay = 255;
  std::uint16_t delay_ticks = 0;
};

// `count` hits of the SAME note at 0, rate_ticks, 2*rate_ticks, ...; gate is
// clamped to rate_ticks so consecutive hits never overlap.
struct NoteRepeatParams {
  std::uint8_t count = 1;
  std::uint8_t reserved = 0;
  std::uint16_t rate_ticks = 1;
};

// kGroove (5210) has NO per-slot params of its own -- the feel lives in
// Arranger::m_groove (the global `groove` panel), reached by a kGroove-typed
// slot through FxContext::groove below, never through this union. This
// member exists purely so set_type()'s "always clear stale union bits"
// discipline has an explicit slot to write for kGroove, matching every other
// InsertType's own params struct (Fork 2: "confirm and document; do not
// invent per-slot groove params").
struct GrooveInsertParams {
  std::uint8_t reserved[4] = {0, 0, 0, 0};
};

// kArp (5220)'s SLIM, WIRE-PERSISTED config subset -- exactly the 4 B the
// Insert::Params union budget allows (D33). `latch`/`seed` are deliberately
// NOT here: they stay session-only defaults on the per-role
// ArpeggiatorEngine instance (Arranger::m_role_arp), never captured by
// Performance (Fork 3/4: "session state cannot live in the 6 B wire slot").
// Field values are the raw ArpRate/ArpDirection enum bytes, not re-clamped
// here -- InsertChain::set_param clamps on write, Insert::on_tick below
// consumes them as-is.
struct ArpInsertParams {
  std::uint8_t rate = static_cast<std::uint8_t>(ArpRate::kSixteenth);
  std::uint8_t direction = static_cast<std::uint8_t>(ArpDirection::kUp);
  std::uint8_t octaves = 1;
  std::uint8_t gate = 75;
};

// One chain slot: a type tag + enabled flag + its own params, index-
// referenced and value-typed (never a pointer/variant, same discipline as
// every other core POD). The union keeps a slot to a small fixed size
// regardless of type (D33) -- set_type()/InsertChain re-activates the
// matching member whenever the type changes, the standard, compiler-
// supported "tagged union" idiom this codebase's own abi.hpp/pad_bank.hpp
// param packings already rely on at the wire level.
struct Insert {
  InsertType type = InsertType::kScaleLock;
  bool enabled = true;
  union Params {
    ScaleLockParams scale_lock;
    VelocityProcParams vel_proc;
    EchoParams echo;
    NoteRepeatParams note_repeat;
    GrooveInsertParams groove;
    ArpInsertParams arp;
    constexpr Params() noexcept : scale_lock{} {}
  } params{};

  // Processes ONE input note through THIS insert -> 0..N output notes into
  // `out` (bounded by `max_out`). Dispatches on `type`; each case is a free,
  // stateless function (fx_detail namespace below) so it stays unit-testable
  // in isolation, mirroring Phase-5 Item #9's perf::validate extraction.
  // kArp always returns 0 here (swallows the note) -- see ingest()/on_tick()
  // below for how an arp-typed slot actually participates in the chain.
  int process(const struct FxNote& in, struct FxNote* out, int max_out,
              const struct FxContext& ctx) const noexcept;

  // Phase-6 Theme 4 (Fork 1, decision 1): OPTIONAL capability, switch-
  // dispatched, genuine no-op for the 5 other types (and a disabled kArp
  // slot). Feeds ONE resolved note into the role's own ArpeggiatorEngine as
  // a held-chord source (kArp's note_on equivalent) -- called from
  // InsertChain::ingest() below, once per note of a role's freshly-resolved
  // step group, ALONGSIDE (not instead of) the ordinary process()/apply()
  // pass.
  void ingest(const struct FxNote& in, ArpeggiatorEngine& arp) const noexcept;

  // Phase-6 Theme 4 (Fork 1, decision 1): OPTIONAL capability, switch-
  // dispatched, genuine no-op for the 5 other types. Fires ONCE PER
  // TRANSPORT TICK, regardless of whether the style's own grid produced any
  // event this tick -- an arp's own rate grid is independent of the
  // arranger's step grid. Syncs the engine's wire-persisted config
  // (rate/direction/octaves/gate) from this slot's own params before
  // ticking it (latch/seed are left untouched -- session-only, never in the
  // union). Returns 0..N notes ready to schedule (kArp emits at most 1 per
  // call, ArpeggiatorEngine's own contract) into `out` (bounded by
  // `max_out`).
  int on_tick(Tick transport_tick, ArpeggiatorEngine& arp, FxNote* out, int max_out) const noexcept;
};
static_assert(sizeof(Insert) == 6, "Insert RAM budget pin (D33)");

// One resolved note flowing through a role's chain -- mirrors NoteReq's own
// field shapes (voicing.hpp) so seeding/reading back is a straight copy.
struct FxNote {
  int note = -1;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;
  TickOffset offset = 0;  // scheduling offset from the step boundary (gesture_delay-compatible)
  // Phase-6 Theme 4 (5210, groove-as-insert): the accumulated groove timing
  // push, written ONLY by a kGroove-typed slot's own process() -- a CARRY
  // field, distinct from the chain's own self-relative clock (`offset`
  // above), read by the FINAL schedule() call (Arranger::on_tick) alongside
  // `offset` and gesture_delay. The other 5 types copy `in` verbatim
  // (`out[0] = in` / `FxNote n = in`), so this free-rides through every
  // existing insert unchanged, exactly like `offset` did before it.
  TickOffset groove_offset = 0;
};

// The live musical context an Insert may read (ScaleLock reads `key`; kGroove
// reads `role`/`groove`/`step`/`tick`). Read-only per-call context, never
// mutated, never wire-persisted -- a short-lived stack value rebuilt once per
// role-step (and once per role-tick for the P5 arp pass).
struct FxContext {
  Key key{};
  ChordState chord{};
  std::uint16_t step = 0;
  Tick tick = 0;
  // Phase-6 Theme 4 (5210): the role index (Arranger::m_routes' own ordinal
  // space) -- needed by kGroove's per-role position hash (groove::apply);
  // the other 5 types never read it.
  std::uint8_t role = 0;
  // Phase-6 Theme 4 (5210): a BY-VALUE copy of the GLOBAL groove-panel feel
  // (Arranger::m_groove) -- FxContext is never wire-persisted, so a 12 B
  // copy per role-step call is cheap and carries no ABI cost (Fork 2/3:
  // "context is a short-lived per-call stack struct, never wire-persisted").
  // Only kGroove reads this; the other 5 types (including kArp) ignore it.
  GrooveParams groove{};
};

namespace fx_detail {

// Pure scale-snap helpers (no state, freestanding): is pitch class `pc` in
// `key`'s scale, and the nearest in-key MIDI note to `note` (ties broken
// downward -- deterministic, D16). A 7-of-12 scale always has a hit within 6
// semitones either side, so the bounded search below always terminates with
// a snapped result.
constexpr bool pc_in_key(std::uint8_t pc, const Key& key) noexcept {
  const theory::Scale scale = theory::scale_of(key.mode);
  const auto rel = static_cast<std::uint8_t>((pc + 12 - key.root_pc) % 12);
  for (std::uint8_t s : scale.steps) {
    if (s == rel) {
      return true;
    }
  }
  return false;
}

constexpr int snap_to_key(int note, const Key& key) noexcept {
  if (note < 0 || note > 127) {
    return note;  // out of MIDI range: leave it for the caller's own clamp
  }
  if (pc_in_key(static_cast<std::uint8_t>(note % 12), key)) {
    return note;
  }
  for (int d = 1; d <= 6; ++d) {
    const int down = note - d;
    if (down >= 0 && pc_in_key(static_cast<std::uint8_t>(down % 12), key)) {
      return down;
    }
    const int up = note + d;
    if (up <= 127 && pc_in_key(static_cast<std::uint8_t>(up % 12), key)) {
      return up;
    }
  }
  return note;  // unreachable: a diatonic 7-note scale always hits within +-6
}

inline int process_scale_lock(const ScaleLockParams& p, const FxNote& in, FxNote* out, int max_out,
                              const FxContext& ctx) noexcept {
  if (max_out < 1) {
    return 0;
  }
  out[0] = in;
  if (p.strength != 0) {
    out[0].note = snap_to_key(in.note, ctx.key);
  }
  return 1;
}

inline int process_velocity(const VelocityProcParams& p, const FxNote& in, FxNote* out,
                            int max_out) noexcept {
  if (max_out < 1) {
    return 0;
  }
  int vel = in.vel;
  switch (static_cast<VelocityProcMode>(p.mode)) {
    case VelocityProcMode::kScale:
      vel = (vel * p.amount) / 100;
      break;
    case VelocityProcMode::kFixed:
      vel = p.amount;
      break;
    case VelocityProcMode::kCompressStatic:
    default: {
      constexpr int kCenter = 64;
      vel = kCenter + ((vel - kCenter) * (100 - p.amount)) / 100;
      break;
    }
  }
  out[0] = in;
  out[0].vel = static_cast<std::uint8_t>(vel < 1 ? 1 : (vel > 127 ? 127 : vel));
  return 1;
}

inline int process_echo(const EchoParams& p, const FxNote& in, FxNote* out, int max_out) noexcept {
  if (max_out < 1) {
    return 0;
  }
  int produced = 0;
  out[produced++] = in;  // the seed, byte-identical (delta == 0 for it)
  int vel = in.vel;
  for (std::uint8_t i = 1; i <= p.repeats && produced < max_out; ++i) {
    vel = (vel * p.vel_decay) / 255;
    if (vel < 1) {
      vel = 1;  // a vel-0 NoteOn is a NoteOff by MIDI convention (D29's own precedent)
    }
    FxNote n = in;
    n.vel = static_cast<std::uint8_t>(vel);
    n.offset = static_cast<TickOffset>(in.offset + static_cast<TickOffset>(i) *
                                                       static_cast<TickOffset>(p.delay_ticks));
    out[produced++] = n;
  }
  return produced;
}

inline int process_note_repeat(const NoteRepeatParams& p, const FxNote& in, FxNote* out,
                               int max_out) noexcept {
  const std::uint8_t hits = p.count == 0 ? std::uint8_t{1} : p.count;
  const std::uint16_t gate = in.gate < p.rate_ticks ? in.gate : p.rate_ticks;
  int produced = 0;
  for (std::uint8_t i = 0; i < hits && produced < max_out; ++i) {
    FxNote n = in;
    n.gate = gate;
    n.offset = static_cast<TickOffset>(in.offset + static_cast<TickOffset>(i) *
                                                       static_cast<TickOffset>(p.rate_ticks));
    out[produced++] = n;
  }
  return produced;
}

// Phase-6 Theme 4 (5210): groove is computed at THIS note's OWN grid
// position -- ctx.step/ctx.tick are the role's step-fire instant, `in.offset`
// is however far this note has already been fanned from the seed by any
// UPSTREAM Echo/NoteRepeat slot -- never the seed's own position (the
// Corelli must-fix that 5100 already established, preserved unchanged here
// now that groove is a slot instead of a hardcoded call). `groove_offset`
// carries the timing push forward instead of folding it into `offset`
// itself, so a slot running AFTER kGroove (if the chain is reordered) still
// sees this note's own, unmodified chain-relative `offset`.
inline int process_groove(const FxNote& in, FxNote* out, int max_out,
                          const FxContext& ctx) noexcept {
  if (max_out < 1) {
    return 0;
  }
  const Tick tick_j = ctx.tick + static_cast<Tick>(in.offset);
  const auto step_j = static_cast<std::uint16_t>(
      ctx.step + static_cast<std::uint16_t>(in.offset / static_cast<TickOffset>(kTicksPerStep)));
  const GrooveOut g = groove::apply(ctx.groove, ctx.role, step_j, tick_j, in.vel);
  out[0] = in;
  out[0].vel = g.velocity;
  out[0].groove_offset = static_cast<TickOffset>(in.groove_offset + g.timing_offset);
  return 1;
}

}  // namespace fx_detail

inline int Insert::process(const FxNote& in, FxNote* out, int max_out,
                           const FxContext& ctx) const noexcept {
  switch (type) {
    case InsertType::kScaleLock:
      return fx_detail::process_scale_lock(params.scale_lock, in, out, max_out, ctx);
    case InsertType::kVelocityProc:
      return fx_detail::process_velocity(params.vel_proc, in, out, max_out);
    case InsertType::kEcho:
      return fx_detail::process_echo(params.echo, in, out, max_out);
    case InsertType::kGroove:
      return fx_detail::process_groove(in, out, max_out, ctx);
    case InsertType::kArp:
      // Never scheduled directly through the ordinary per-note apply() pass
      // -- an arp-typed slot's held-chord source is fed via ingest() and its
      // own emission happens on the separate on_tick() pass (Fork 1: "the
      // role's resolved notes feed the arp and are NOT scheduled directly").
      return 0;
    case InsertType::kNoteRepeat:
    default:
      return fx_detail::process_note_repeat(params.note_repeat, in, out, max_out);
  }
}

inline void Insert::ingest(const FxNote& in, ArpeggiatorEngine& arp) const noexcept {
  if (type != InsertType::kArp || !enabled) {
    return;  // no-op: the 5 other types (and a disabled arp slot) never touch arp state
  }
  if (in.note < 0 || in.note > 127) {
    return;
  }
  arp.note_on(static_cast<std::uint8_t>(in.note), in.vel);
}

inline int Insert::on_tick(Tick transport_tick, ArpeggiatorEngine& arp, FxNote* out,
                           int max_out) const noexcept {
  if (type != InsertType::kArp || !enabled || max_out < 1) {
    return 0;  // no-op for the 5 other types (and a disabled/unsized arp slot)
  }
  // Sync the engine's wire-persisted config from this slot's own params
  // before ticking it -- latch/seed are session-only and untouched here
  // (never in the union, see ArpInsertParams' own comment).
  ArpeggiatorParams p = arp.params();
  p.rate = static_cast<ArpRate>(params.arp.rate);
  p.direction = static_cast<ArpDirection>(params.arp.direction);
  p.octaves = params.arp.octaves;
  p.gate = params.arp.gate;
  arp.set_params(p);

  int produced = 0;
  arp.on_tick(transport_tick, [&](std::uint8_t note, std::uint8_t velocity, TickOffset gate) {
    if (produced < max_out) {
      out[produced++] =
          FxNote{.note = note, .vel = velocity, .gate = static_cast<std::uint16_t>(gate)};
    }
  });
  return produced;
}

// Per-role chain: kMaxInserts fixed slots, ALWAYS at full capacity (every
// slot is a live, direct-indexed Insert, never a sparse/grow-on-demand
// pool like ClipMatrix/PerformanceStore) -- `fx set <role> <slot> ...`
// addresses any slot 0..kMaxInserts-1 in any order, mirroring PadEngine's
// own flat-array precedent (pad_bank.hpp) for the same "randomly addressable
// slot" reason. A default-constructed slot is `{kScaleLock, enabled=true,
// strength=0}` -- an EXACT passthrough (see ScaleLockParams above) -- so a
// freshly-constructed InsertChain (nothing configured, the state of every
// role before this item existed) is PROVABLY, not just practically,
// byte-identical to "no chain": apply() below returns exactly {in} unchanged
// for every note. That is what "empty chain" means product-side; it is
// backed by a full StaticVector<Insert, kMaxInserts> rather than a
// zero-length one purely so every slot is directly addressable without a
// PerformanceStore-style grow-or-overwrite gap restriction.
class InsertChain {
 public:
  constexpr InsertChain() noexcept {
    for (std::uint16_t i = 0; i < kMaxInserts; ++i) {
      ARR_ASSERT(m_inserts.push_back(default_slot(i)));  // always succeeds: capacity == kMaxInserts
    }
  }

  bool set_type(std::size_t slot, InsertType type) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    Insert& ins = m_inserts[slot];
    ins.type = type;
    // Fresh defaults for the new type -- drop whatever bits the PREVIOUS
    // type left in the union so a stale field can never be misread as the
    // new type's own field.
    switch (type) {
      case InsertType::kScaleLock:
        ins.params.scale_lock = ScaleLockParams{};
        break;
      case InsertType::kVelocityProc:
        ins.params.vel_proc = VelocityProcParams{};
        break;
      case InsertType::kEcho:
        ins.params.echo = EchoParams{};
        break;
      case InsertType::kGroove:
        ins.params.groove = GrooveInsertParams{};
        break;
      case InsertType::kArp:
        ins.params.arp = ArpInsertParams{};
        break;
      case InsertType::kNoteRepeat:
      default:
        ins.params.note_repeat = NoteRepeatParams{};
        break;
    }
    return true;
  }

  // `param_id` addresses a field WITHIN the slot's CURRENT type (set_type
  // must run first): ScaleLock{0=strength}; VelocityProc{0=mode,1=amount};
  // Echo{0=repeats,1=vel_decay,2=delay_ticks}; NoteRepeat{0=count,
  // 1=rate_ticks}; Groove{no settable params}; Arp{0=rate,1=direction,
  // 2=octaves,3=gate}. `value` is clamped to the field's own width (u8 or
  // u16), negative values clamp to 0. Returns false for an out-of-range slot
  // or an unknown param_id for the slot's current type.
  //
  // Over the cognitive-complexity threshold now that kGroove/kArp (Phase-6
  // Theme 4) joined the original 4 types: a flat per-type dispatch table,
  // one `if` per field -- splitting it would scatter each type's own field
  // list across functions for no readability gain (mirrors Arranger::
  // on_tick's own NOLINT rationale).
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  bool set_param(std::size_t slot, std::uint8_t param_id, std::int32_t value) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    Insert& ins = m_inserts[slot];
    switch (ins.type) {
      case InsertType::kScaleLock:
        if (param_id != 0) {
          return false;
        }
        ins.params.scale_lock.strength = clamp_u8(value);
        return true;
      case InsertType::kVelocityProc:
        if (param_id == 0) {
          ins.params.vel_proc.mode = clamp_u8(value);
          return true;
        }
        if (param_id == 1) {
          ins.params.vel_proc.amount = clamp_u8(value);
          return true;
        }
        return false;
      case InsertType::kEcho:
        if (param_id == 0) {
          ins.params.echo.repeats = clamp_u8(value);
          return true;
        }
        if (param_id == 1) {
          ins.params.echo.vel_decay = clamp_u8(value);
          return true;
        }
        if (param_id == 2) {
          ins.params.echo.delay_ticks = clamp_u16(value);
          return true;
        }
        return false;
      case InsertType::kGroove:
        // Fork 2/3: no per-slot params -- groove is configured via the
        // global `groove` panel (Arranger::set_groove_field), never here.
        return false;
      case InsertType::kArp:
        if (param_id == 0) {
          ins.params.arp.rate = clamp_enum(value, kArpRateCount);
          return true;
        }
        if (param_id == 1) {
          ins.params.arp.direction = clamp_enum(value, kArpDirectionCount);
          return true;
        }
        if (param_id == 2) {
          ins.params.arp.octaves = clamp_range(value, 1, 4);
          return true;
        }
        if (param_id == 3) {
          ins.params.arp.gate = clamp_range(value, 0, 100);
          return true;
        }
        return false;
      case InsertType::kNoteRepeat:
      default:
        if (param_id == 0) {
          ins.params.note_repeat.count = clamp_u8(value);
          return true;
        }
        if (param_id == 1) {
          ins.params.note_repeat.rate_ticks = clamp_u16(value);
          return true;
        }
        return false;
    }
  }

  bool set_enabled(std::size_t slot, bool enabled) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    m_inserts[slot].enabled = enabled;
    return true;
  }

  // Resets ONE slot to the inert default IN PLACE (does not shift/renumber
  // the other slots -- a hardware-FX-rack "clear this slot" model). The
  // LAST slot's own "inert default" is kGroove, not kScaleLock -- see
  // default_slot() below.
  bool clear(std::size_t slot) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    m_inserts[slot] = default_slot(slot);
    return true;
  }
  void clear_all() noexcept {
    for (std::size_t i = 0; i < m_inserts.size(); ++i) {
      m_inserts[i] = default_slot(i);
    }
  }

  const Insert* get(std::size_t slot) const noexcept {
    return slot < m_inserts.size() ? &m_inserts[slot] : nullptr;
  }

  // Restores ONE slot to an EXACT Insert value (type + enabled + params),
  // bypassing set_type()/set_param()'s semantic per-field API -- the
  // companion to get() for Performance recall (Engine::apply_performance),
  // which must reconstruct a chain byte-for-byte from a snapshot rather than
  // replaying individual field edits one param_id at a time. A plain struct
  // copy: Insert is a flat POD with no internal padding
  // (static_assert(sizeof(Insert) == 6) above), exactly as safe as
  // `m_inserts[slot] = Insert{}` in clear() above.
  bool restore(std::size_t slot, const Insert& ins) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    m_inserts[slot] = ins;
    return true;
  }

  // Runs `in` through every ENABLED slot in order, fanning 1 note into up to
  // kMaxChainFan; a disabled or default-inert slot passes its input through
  // unchanged (1-in, 1-out identity). Returns the number of notes written to
  // `out` (<= max_out, and <= kMaxChainFan regardless of max_out). A chain
  // with nothing configured (or entirely disabled) returns exactly 1 = `in`
  // unchanged -- the Corelli bit-identity guarantee this item's whole graft
  // depends on.
  int apply(const FxNote& in, FxNote* out, int max_out, const FxContext& ctx) const noexcept {
    return apply_impl(0, in, out, max_out, ctx);
  }

  // Phase-6 Theme 4 (5220) P5: resumes the chain at slot `start_slot` instead
  // of slot 0 -- used to feed an arp-typed slot's OWN emitted note through
  // whatever the role configured AFTER it (typically a trailing kGroove),
  // without re-entering the arp slot itself (which would swallow the note
  // right back, see Insert::process's kArp case) or any EARLIER slot (which
  // already ran on the seed that fed the arp, not on its output).
  int apply_from(std::size_t start_slot, const FxNote& in, FxNote* out, int max_out,
                 const FxContext& ctx) const noexcept {
    return apply_impl(start_slot, in, out, max_out, ctx);
  }

  // Phase-6 Theme 4 (5220, Fork 1): whether the chain carries an ENABLED
  // kArp-typed slot -- gates Arranger's "replace the held chord on a fresh
  // resolved group" bookkeeping (decision 5). A chain with no arp slot never
  // touches the role's ArpeggiatorEngine.
  bool has_arp() const noexcept {
    for (const Insert& ins : m_inserts) {
      if (ins.enabled && ins.type == InsertType::kArp) {
        return true;
      }
    }
    return false;
  }

  // Phase-6 Theme 4 (5220, Fork 1): feeds ONE resolved note into every slot's
  // own ingest() -- a genuine no-op for the 5 other types, real work only for
  // an enabled kArp slot. Called once per note of a role's freshly-resolved
  // step group, ALONGSIDE apply() (never instead of it: apply()'s own kArp
  // case already swallows the note from the ordinary schedule path).
  void ingest(const FxNote& in, ArpeggiatorEngine& arp) const noexcept {
    for (const Insert& ins : m_inserts) {
      ins.ingest(in, arp);
    }
  }

  // Phase-6 Theme 4 (5220) P5: the ungated per-tick pass. Scans for an
  // enabled kArp slot; if its own rate grid fires this tick, the emitted
  // note is run through apply_from(slot+1, ...) so a trailing kGroove (or
  // any other slot configured after it) still applies -- the SIMPLEST
  // faithful model (Fork 1's own open question): an arp-emitted note is
  // just another note entering the chain at its own slot's position.
  // Returns the total notes written to `out` across every kArp slot found
  // (normally 0 or 1 -- the product model is one arp per role, but nothing
  // here enforces a single kArp slot per chain).
  int on_tick(Tick transport_tick, ArpeggiatorEngine& arp, const FxContext& ctx, FxNote* out,
              int max_out) const noexcept {
    int total = 0;
    for (std::size_t slot = 0; slot < m_inserts.size() && total < max_out; ++slot) {
      const Insert& ins = m_inserts[slot];
      if (!ins.enabled || ins.type != InsertType::kArp) {
        continue;
      }
      FxNote emitted[1];
      const int n = ins.on_tick(transport_tick, arp, emitted, 1);
      for (int i = 0; i < n && total < max_out; ++i) {
        FxNote fanned[kMaxChainFan];
        const int fan_count = apply_from(slot + 1, emitted[i], fanned, kMaxChainFan, ctx);
        for (int j = 0; j < fan_count && total < max_out; ++j) {
          out[total++] = fanned[j];
        }
      }
    }
    return total;
  }

 private:
  static constexpr std::uint8_t clamp_u8(std::int32_t v) noexcept {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
  }
  static constexpr std::uint16_t clamp_u16(std::int32_t v) noexcept {
    return static_cast<std::uint16_t>(v < 0 ? 0 : (v > 65535 ? 65535 : v));
  }
  static constexpr std::uint8_t clamp_enum(std::int32_t v, std::uint8_t count) noexcept {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v >= count ? count - 1 : v));
  }
  static constexpr std::uint8_t clamp_range(std::int32_t v, std::uint8_t lo,
                                            std::uint8_t hi) noexcept {
    return static_cast<std::uint8_t>(v < lo ? lo : (v > hi ? hi : v));
  }

  // Phase-6 Theme 4 (5210, Corelli's pinned-last resolution): the LAST
  // slot's own "inert default" is an ENABLED kGroove, not kScaleLock --
  // every role's chain carries the global groove feel out of the box,
  // reproducing the pre-5210 hardcoded groove::apply call's exact position
  // (after every other configured slot) with ZERO opt-in needed. kGroove
  // with the (also-default) identity GrooveParams is itself a provable
  // passthrough (test_groove_apply's own first assertion), so an
  // unconfigured chain stays byte-identical to "no chain" -- the SAME
  // guarantee the other 7 slots' kScaleLock default already provided. This
  // is a NAMED ASYMMETRY (Fork 2): groove is toggleable/configurable as a
  // slot like any other, but not truly free-orderable relative to a
  // fanning insert placed AFTER it (an Echo/NoteRepeat placed after
  // kGroove grooves the SEED only; its fanned copies inherit that single
  // push rather than re-grooving at their own, later grid position -- a
  // deliberate, documented footgun, not a bug). The other 7 slots are
  // unaffected: still directly addressable/reorderable, still
  // kScaleLock/strength==0 by default -- and a caller CAN still overwrite
  // this last slot explicitly (set_type/clear/clear_all all route through
  // this same helper, so "pinned" means "the default", never "enforced").
  static constexpr Insert default_slot(std::size_t slot) noexcept {
    Insert ins;
    if (slot == kMaxInserts - 1) {
      ins.type = InsertType::kGroove;
      ins.params.groove = GrooveInsertParams{};
    }
    return ins;
  }

  // Runs `in` through slots [start_slot, kMaxInserts) -- apply()'s and
  // apply_from()'s shared body (apply() == apply_impl(0, ...)).
  int apply_impl(std::size_t start_slot, const FxNote& in, FxNote* out, int max_out,
                 const FxContext& ctx) const noexcept {
    FxNote buf_a[kMaxChainFan];
    FxNote buf_b[kMaxChainFan];
    FxNote* cur = buf_a;
    FxNote* next = buf_b;
    cur[0] = in;
    int count = 1;
    for (std::size_t slot = start_slot; slot < m_inserts.size(); ++slot) {
      const Insert& ins = m_inserts[slot];
      if (!ins.enabled) {
        continue;
      }
      int next_count = 0;
      for (int i = 0; i < count; ++i) {
        const int room = kMaxChainFan - next_count;
        if (room <= 0) {
          // The fan cap is a REACHABLE, ordinary condition (chaining two
          // fan-out inserts, e.g. Echo -> Echo, on one role easily exceeds
          // kMaxChainFan) -- not a programming error, so this stage stops
          // accumulating and gracefully TRUNCATES the result instead of
          // trapping (matches this function's own documented contract:
          // "drop past kMaxChainFan, return the truncated result").
          break;
        }
        FxNote produced[kMaxChainFan];
        const int n = ins.process(cur[i], produced, room, ctx);
        for (int j = 0; j < n; ++j) {
          next[next_count++] = produced[j];
        }
      }
      ARR_ASSERT(next_count <=
                 kMaxChainFan);  // genuinely never false: a real net, not a trap-on-cap
      count = next_count;
      FxNote* tmp = cur;
      cur = next;
      next = tmp;
    }
    const int result = count < max_out ? count : max_out;
    for (int i = 0; i < result; ++i) {
      out[i] = cur[i];
    }
    return result;
  }

  StaticVector<Insert, kMaxInserts> m_inserts;
};

}  // namespace arrangrr

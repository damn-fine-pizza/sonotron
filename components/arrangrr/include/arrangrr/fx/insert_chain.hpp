#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"  // kMaxInserts (the committed RESERVED symbol, node 5100)
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"  // kMaxChainFan
#include "chorddet/theory.hpp"  // Key, ChordState, theory::scale_of
#include "common/assert.hpp"    // ARR_ASSERT
#include "common/time.hpp"      // Tick, TickOffset

// MIDI-FX insert chain (Phase-5 Item #10, node 5100/5200): four per-note
// inserts (ScaleLock/VelocityProc/Echo/NoteRepeat) grafted onto the
// Arranger's D40 resolution pipeline, per TrackRole (the SAME ordinal space
// as Arranger::m_routes -- Corelli's concrete-shape review, locked scope:
// per-ROLE addressing, NOT per-Timeline-Track). v1 is the chain CORE ONLY:
// groove-as-insert and arp-as-insert are DEFERRED, so an Insert has no
// on_tick capability -- it is a pure, stateless, one-note-in/N-notes-out
// function (D16 determinism: same input+params+context always yields the
// same output, no per-note history, no RNG).
//
// FX persistence: NOT in v1. Performance (arrangrr/perf/performance.hpp)
// does not carry the chain -- a future format_version would add it
// explicitly (Architectural Principle #8's own versioned-migration
// discipline). Live config only, exactly like the `groove`/`arp` panels
// before a Performance existed to snapshot them.

namespace arrangrr {

enum class InsertType : std::uint8_t {
  kScaleLock = 0,
  kVelocityProc = 1,
  kEcho = 2,
  kNoteRepeat = 3,
};
inline constexpr std::uint8_t kInsertTypeCount = 4;

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
    constexpr Params() noexcept : scale_lock{} {}
  } params{};

  // Processes ONE input note through THIS insert -> 0..N output notes into
  // `out` (bounded by `max_out`). Dispatches on `type`; each case is a free,
  // stateless function (fx_detail namespace below) so it stays unit-testable
  // in isolation, mirroring Phase-5 Item #9's perf::validate extraction.
  int process(const struct FxNote& in, struct FxNote* out, int max_out,
              const struct FxContext& ctx) const noexcept;
};
static_assert(sizeof(Insert) == 6, "Insert RAM budget pin (D33)");

// One resolved note flowing through a role's chain -- mirrors NoteReq's own
// field shapes (voicing.hpp) so seeding/reading back is a straight copy.
struct FxNote {
  int note = -1;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;
  TickOffset offset = 0;  // scheduling offset from the step boundary (gesture_delay-compatible)
};

// The live musical context an Insert may read (ScaleLock reads `key`; a
// future insert could read `chord`/`step`/`tick`) -- v1 has NO on_tick
// capability (deferred), so this is read-only per-call context, never
// mutated, never carrying scheduler/transport state beyond what one note's
// resolution already needed.
struct FxContext {
  Key key{};
  ChordState chord{};
  std::uint16_t step = 0;
  Tick tick = 0;
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
    case InsertType::kNoteRepeat:
    default:
      return fx_detail::process_note_repeat(params.note_repeat, in, out, max_out);
  }
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
      ARR_ASSERT(m_inserts.push_back(Insert{}));  // always succeeds: capacity == kMaxInserts
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
  // 1=rate_ticks}. `value` is clamped to the field's own width (u8 or u16),
  // negative values clamp to 0. Returns false for an out-of-range slot or an
  // unknown param_id for the slot's current type.
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
  // the other slots -- a hardware-FX-rack "clear this slot" model).
  bool clear(std::size_t slot) noexcept {
    if (slot >= kMaxInserts) {
      return false;
    }
    m_inserts[slot] = Insert{};
    return true;
  }
  void clear_all() noexcept {
    for (Insert& ins : m_inserts) {
      ins = Insert{};
    }
  }

  const Insert* get(std::size_t slot) const noexcept {
    return slot < m_inserts.size() ? &m_inserts[slot] : nullptr;
  }

  // Runs `in` through every ENABLED slot in order, fanning 1 note into up to
  // kMaxChainFan; a disabled or default-inert slot passes its input through
  // unchanged (1-in, 1-out identity). Returns the number of notes written to
  // `out` (<= max_out, and <= kMaxChainFan regardless of max_out). A chain
  // with nothing configured (or entirely disabled) returns exactly 1 = `in`
  // unchanged -- the Corelli bit-identity guarantee this item's whole graft
  // depends on.
  int apply(const FxNote& in, FxNote* out, int max_out, const FxContext& ctx) const noexcept {
    FxNote buf_a[kMaxChainFan];
    FxNote buf_b[kMaxChainFan];
    FxNote* cur = buf_a;
    FxNote* next = buf_b;
    cur[0] = in;
    int count = 1;
    for (const Insert& ins : m_inserts) {
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

 private:
  static constexpr std::uint8_t clamp_u8(std::int32_t v) noexcept {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
  }
  static constexpr std::uint16_t clamp_u16(std::int32_t v) noexcept {
    return static_cast<std::uint16_t>(v < 0 ? 0 : (v > 65535 ? 65535 : v));
  }

  StaticVector<Insert, kMaxInserts> m_inserts;
};

}  // namespace arrangrr

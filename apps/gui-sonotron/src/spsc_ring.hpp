#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

// Bounded single-producer/single-consumer lock-free ring buffer of trivially
// copyable PODs (Phase 2b, docs/design/sonotron-server-phase2-brief.md's
// "Thread-boundary mechanism", Corelli §15.2 correction #2: lock-free from
// the first landing, NOT mutex-first -- a mutex-guarded queue and the
// brief's own "wait-free" claim are mutually exclusive, since a GUI-thread
// producer preempted inside a locked critical section would stall the
// engine-thread consumer for an OS-scheduler-defined, unbounded duration).
//
// Contract (exactly one producer thread calling try_push, exactly one
// consumer thread calling try_pop -- enforced by the caller's usage, not by
// this type):
//   - One reserved physical slot disambiguates full vs. empty with only two
//     atomic indices (capacity N usable slots, N+1 physical slots) -- no CAS
//     loop needed, this is the classic SPSC shape, not the general
//     lock-free-queue problem.
//   - `m_head` (producer-owned) and `m_tail` (consumer-owned) each get their
//     own cache line so push/pop never ping-pong the same line between cores
//     (false-sharing guard).
//   - The producer publishes the slot's payload THEN releases the updated
//     head index (memory_order_release); the consumer acquires the head
//     index before reading the payload (memory_order_acquire) -- and
//     symmetrically for the tail index the consumer publishes back.
//
// Host-side glue only (this lives in the GUI app, never in the freestanding
// core): plain <atomic>, no new dependency.

namespace sonotron {

inline constexpr std::size_t kCacheLineSize = 64;

template <typename T, std::size_t Capacity>
class SpscRing {
  static_assert(std::is_trivially_copyable_v<T>, "SpscRing carries fixed-size PODs only");
  static_assert(Capacity >= 1, "SpscRing needs at least one usable slot");

 public:
  // Usable capacity (what the caller asked for); one extra physical slot
  // disambiguates full from empty -- see the header comment.
  static constexpr std::size_t kCapacity = Capacity;

  SpscRing() = default;
  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  // Producer side (the GUI thread for the Command ring, the engine thread
  // for the OutEvent ring): returns false when the ring is full. The overflow
  // POLICY (drop vs. never-drop-plus-warn) is asymmetric per direction and
  // lives in the caller, not here -- this type only ever reports full/empty
  // honestly; it never blocks and never allocates.
  bool try_push(const T& value) noexcept {
    const std::size_t head = m_head.load(std::memory_order_relaxed);
    const std::size_t next = advance(head);
    if (next == m_tail.load(std::memory_order_acquire)) {
      return false;  // full
    }
    m_slots[head] = value;
    m_head.store(next, std::memory_order_release);
    return true;
  }

  // Consumer side: returns false when the ring is empty. Wait-free -- never
  // blocks, never allocates -- so the engine-thread drain (Command ring) and
  // the GUI-thread poll (OutEvent ring) never stall behind each other.
  bool try_pop(T& out) noexcept {
    const std::size_t tail = m_tail.load(std::memory_order_relaxed);
    if (tail == m_head.load(std::memory_order_acquire)) {
      return false;  // empty
    }
    out = m_slots[tail];
    m_tail.store(advance(tail), std::memory_order_release);
    return true;
  }

  // Best-effort occupancy hint. The result can be stale the instant it is
  // read (the other thread keeps moving its own index concurrently) --
  // diagnostics/tests only, never control flow that needs an exact count.
  bool empty_hint() const noexcept {
    return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
  }

 private:
  static constexpr std::size_t kSlotCount = Capacity + 1;

  static constexpr std::size_t advance(std::size_t idx) noexcept {
    const std::size_t next = idx + 1;
    return next == kSlotCount ? 0 : next;
  }

  alignas(kCacheLineSize) std::atomic<std::size_t> m_head{0};
  alignas(kCacheLineSize) std::atomic<std::size_t> m_tail{0};
  std::array<T, kSlotCount> m_slots{};
};

}  // namespace sonotron

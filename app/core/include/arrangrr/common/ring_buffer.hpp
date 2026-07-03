#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace arrangrr {

// Lock-free single-producer/single-consumer ring buffer: the only structure
// allowed to cross the ISR/loop boundary (DESIGN.md §20). One slot is kept
// free to distinguish full from empty, so usable capacity is N - 1.
template <typename T, std::size_t N>
class SpscRingBuffer {
  static_assert(N >= 2 && (N & (N - 1)) == 0, "capacity must be a power of two");
  static_assert(std::is_trivially_copyable_v<T>,
                "only trivially copyable types may cross the ISR boundary");

 public:
  static constexpr std::size_t capacity() noexcept { return N - 1; }

  // Producer side only.
  [[nodiscard]] bool push(const T& value) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & kMask;
    if (next == tail_.load(std::memory_order_acquire)) return false;  // full
    buf_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer side only.
  [[nodiscard]] bool pop(T& out) noexcept {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false;  // empty
    out = buf_[tail];
    tail_.store((tail + 1) & kMask, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept {
    return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
  }

  // Approximate under concurrency; exact when quiescent.
  std::size_t size() const noexcept {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    return (head - tail) & kMask;
  }

 private:
  static constexpr std::size_t kMask = N - 1;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  T buf_[N]{};
};

}  // namespace arrangrr

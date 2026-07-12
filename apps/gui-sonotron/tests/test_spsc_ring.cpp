// Unit + threaded stress tests for SpscRing (Phase 2b, docs/design/
// sonotron-server-phase2-brief.md's "Thread-boundary mechanism"). The
// single-threaded tests check the full/empty disambiguation and wraparound
// explicitly rather than relying on it working "by luck" at a given
// capacity (Corelli §15.2's own warning); the threaded stress test is the
// one meant to be run under ThreadSanitizer (see the milestone report for
// the exact TSan build/run command -- this project has no dedicated TSan
// CMake preset yet, so that run is a manual, documented verification step
// alongside the normal ctest-registered run below).

#include "src/spsc_ring.hpp"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "test.hpp"

namespace {

struct Pod {
  std::uint32_t a = 0;
  std::uint32_t b = 0;
};

void test_empty_ring_reports_empty() {
  sonotron::SpscRing<Pod, 4> ring;
  Pod out;
  CHECK(ring.empty_hint());
  CHECK(!ring.try_pop(out));
}

void test_push_pop_round_trip() {
  sonotron::SpscRing<Pod, 4> ring;
  CHECK(ring.try_push(Pod{1, 2}));
  CHECK(!ring.empty_hint());
  Pod out;
  CHECK(ring.try_pop(out));
  CHECK(out.a == 1 && out.b == 2);
  CHECK(ring.empty_hint());
}

// Fills the ring to its usable capacity, confirms the NEXT push reports full
// (one reserved slot correctly disambiguates full from empty rather than
// silently overwriting or wrapping), then drains it and checks FIFO order.
void test_full_disambiguation_and_fifo_order() {
  sonotron::SpscRing<Pod, 4> ring;
  for (std::uint32_t i = 0; i < 4; ++i) {
    CHECK(ring.try_push(Pod{i, 0}));
  }
  Pod overflow;
  CHECK(!ring.try_push(Pod{99, 0}));  // full: must not silently overwrite

  for (std::uint32_t i = 0; i < 4; ++i) {
    Pod out;
    CHECK(ring.try_pop(out));
    CHECK(out.a == i);  // FIFO order preserved
  }
  CHECK(!ring.try_pop(overflow));  // now empty again
}

// Drives the ring past its physical slot count several times over so a bug
// in the modular index wraparound (off-by-one in kSlotCount, or reusing the
// reserved slot) would show up as a corrupted value or a wrong full/empty
// verdict.
void test_sustained_wraparound() {
  sonotron::SpscRing<Pod, 3> ring;
  std::uint32_t next_push = 0;
  std::uint32_t next_pop = 0;
  for (int round = 0; round < 100; ++round) {
    CHECK(ring.try_push(Pod{next_push, 0}));
    ++next_push;
    Pod out;
    CHECK(ring.try_pop(out));
    CHECK(out.a == next_pop);
    ++next_pop;
  }
}

// The threaded stress test: one producer thread pushes a long, strictly
// increasing sequence; one consumer thread drains it and checks the
// sequence arrives complete and in order. Under ThreadSanitizer this is the
// test that would flag a missing acquire/release pairing or a false-sharing
// -induced data race on the head/tail indices.
void test_threaded_producer_consumer_stress() {
  constexpr std::uint32_t kCount = 200'000;
  sonotron::SpscRing<std::uint32_t, 64> ring;
  std::atomic<bool> producer_done{false};

  std::thread producer([&] {
    for (std::uint32_t i = 0; i < kCount; ++i) {
      while (!ring.try_push(i)) {
        std::this_thread::yield();  // ring momentarily full: back off, don't drop
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::vector<std::uint32_t> received;
  received.reserve(kCount);
  std::uint32_t value = 0;
  while (received.size() < kCount) {
    if (ring.try_pop(value)) {
      received.push_back(value);
    } else {
      std::this_thread::yield();
    }
  }
  producer.join();
  CHECK(producer_done.load(std::memory_order_acquire));

  CHECK(received.size() == kCount);
  bool in_order = true;
  for (std::uint32_t i = 0; i < kCount; ++i) {
    if (received[i] != i) {
      in_order = false;
      break;
    }
  }
  CHECK(in_order);
}

}  // namespace

int main() {
  test_empty_ring_reports_empty();
  test_push_pop_round_trip();
  test_full_disambiguation_and_fifo_order();
  test_sustained_wraparound();
  test_threaded_producer_consumer_stress();
  return sonotron::test::failures();
}

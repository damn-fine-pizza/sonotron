#include "arrangrr/common/crc.hpp"
#include "arrangrr/common/result.hpp"
#include "arrangrr/common/ring_buffer.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/version.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

void test_static_vector() {
  StaticVector<int, 3> v;
  CHECK(v.empty());
  CHECK(v.push_back(10));
  CHECK(v.push_back(20));
  CHECK(v.push_back(30));
  CHECK(v.full());
  CHECK(!v.push_back(40));  // graceful refusal when full
  CHECK(v.size() == 3);
  v.erase(0);
  CHECK(v.size() == 2 && v[0] == 20 && v[1] == 30);
  v.pop_back();
  CHECK(v.back() == 20);
  v.clear();
  CHECK(v.empty());
}

void test_span() {
  int raw[4] = {1, 2, 3, 4};
  Span<int> s(raw);
  CHECK(s.size() == 4);
  CHECK(s[2] == 3);
  CHECK(s.first(2).size() == 2);
  CHECK(s.subspan(1, 2)[0] == 2);
  Span<const int> cs = s;  // const conversion
  CHECK(cs.size() == 4);
}

void test_ring_buffer() {
  SpscRingBuffer<int, 8> rb;  // usable capacity 7
  CHECK(rb.empty());
  for (int i = 0; i < 7; ++i) CHECK(rb.push(i));
  CHECK(!rb.push(99));  // full
  CHECK(rb.size() == 7);
  int out = -1;
  for (int i = 0; i < 7; ++i) {
    CHECK(rb.pop(out));
    CHECK(out == i);  // FIFO order
  }
  CHECK(!rb.pop(out));  // empty
  // Wraparound: interleave push/pop past the physical end.
  for (int round = 0; round < 5; ++round) {
    for (int i = 0; i < 5; ++i) CHECK(rb.push(round * 10 + i));
    for (int i = 0; i < 5; ++i) {
      CHECK(rb.pop(out));
      CHECK(out == round * 10 + i);
    }
  }
}

void test_result() {
  auto ok = Result<int, char>::ok(42);
  CHECK(ok.is_ok() && ok.value() == 42);
  auto err = Result<int, char>::err('e');
  CHECK(!err.is_ok() && err.error() == 'e');
  CHECK(err.value_or(-1) == -1);
  auto st = Status<char>::err('x');
  CHECK(!st.is_ok() && st.error() == 'x');
  CHECK(Status<char>::ok().is_ok());
}

void test_time() {
  // 1 s @120.00 BPM = exactly 1920 ticks at 960 PPQN.
  TickAccumulator acc;
  acc.set_bpm(12000);
  CHECK(acc.advance_us(1'000'000) == 1920);
  // Drift-free: 1000 x 1 ms must equal 1 x 1 s exactly.
  TickAccumulator ms;
  ms.set_bpm(12345);  // awkward tempo on purpose
  std::uint32_t total = 0;
  for (int i = 0; i < 1000; ++i) total += ms.advance_us(1000);
  TickAccumulator once;
  once.set_bpm(12345);
  const std::uint32_t whole = once.advance_us(1'000'000);
  CHECK(total == whole);
}

void test_crc() {
  constexpr std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(crc32(Span<const std::uint8_t>(check)) == 0xCBF43926u);
  // Incremental == one-shot when chained via seed.
  const std::uint8_t a[] = {'1', '2', '3', '4'};
  const std::uint8_t b[] = {'5', '6', '7', '8', '9'};
  const std::uint32_t partial = crc32(Span<const std::uint8_t>(a)) ^ 0xFFFFFFFFu;
  CHECK(crc32(Span<const std::uint8_t>(b), partial) == 0xCBF43926u);
}

}  // namespace

int main() {
  test_static_vector();
  test_span();
  test_ring_buffer();
  test_result();
  test_time();
  test_crc();
  if (arrangrr::test::failures() == 0) std::printf("test_common: all OK (%s)\n", arrangrr::version_string());
  return arrangrr::test::failures();
}

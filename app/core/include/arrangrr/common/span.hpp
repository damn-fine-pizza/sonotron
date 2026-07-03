#pragma once

#include <cstddef>
#include <type_traits>

#include "arrangrr/common/assert.hpp"

// Own non-owning view type instead of std::span: guaranteed available and
// identical on host and freestanding arm builds (DESIGN.md §20, D32).

namespace arrangrr {

template <typename T>
class Span {
 public:
  constexpr Span() noexcept = default;
  constexpr Span(T* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
  template <std::size_t N>
  constexpr Span(T (&arr)[N]) noexcept : m_data(arr), m_size(N) {}

  // A Span<const T> is constructible from a Span<T>.
  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_same_v<const U, T>)
  constexpr Span(Span<U> other) noexcept : m_data(other.data()), m_size(other.size()) {}

  constexpr T* data() const noexcept { return m_data; }
  constexpr std::size_t size() const noexcept { return m_size; }
  constexpr bool empty() const noexcept { return m_size == 0; }

  constexpr T& operator[](std::size_t i) const noexcept {
    ARR_ASSERT(i < m_size);
    return m_data[i];
  }

  constexpr T* begin() const noexcept { return m_data; }
  constexpr T* end() const noexcept { return m_data + m_size; }

  constexpr Span first(std::size_t n) const noexcept {
    ARR_ASSERT(n <= m_size);
    return Span(m_data, n);
  }
  constexpr Span subspan(std::size_t offset) const noexcept {
    ARR_ASSERT(offset <= m_size);
    return Span(m_data + offset, m_size - offset);
  }
  constexpr Span subspan(std::size_t offset, std::size_t n) const noexcept {
    ARR_ASSERT(offset + n <= m_size);
    return Span(m_data + offset, n);
  }

 private:
  T* m_data = nullptr;
  std::size_t m_size = 0;
};

}  // namespace arrangrr
